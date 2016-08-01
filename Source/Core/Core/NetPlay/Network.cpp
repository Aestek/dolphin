// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <Common/ENetUtil.h>
#include <SFML/Network/Packet.hpp>
#include <algorithm>
#include <thread>

#include "Network.h"

namespace NetPlay
{
Network::Network()
{
  enet_initialize();
  RunOnTick(std::bind(&Network::FlushTimedOutCallbacks, this));
  RunOnTick(std::bind(&Network::SendQueuedPackets, this));
}

bool Network::Listen(const u16 port)
{
  if (m_is_listening)
  {
    return false;
  }

  ENetAddress server_addr;
  server_addr.host = ENET_HOST_ANY;
  server_addr.port = port;
  m_host = enet_host_create(&server_addr, ENET_PEER_COUNT, ENET_CHANEL_COUNT, ENET_MAX_IN_BANDWIDTH,
                            ENET_MAX_OUT_BANDWIDTH);
  Start();
  m_is_listening = true;
  return true;
}

void Network::Connect(const std::string& address, u16 port, u32 timeout,
                      ConnectionCallback callback)
{
  ENetAddress addr;
  enet_address_set_host(&addr, address.c_str());
  addr.port = port;

  ENetPeer* peer = enet_host_connect(m_host, &addr, ENET_CHANEL_COUNT, ENET_CONNECT_DATA);

  if (!peer)
  {
    callback(nullptr);
    return;
  }

  m_pending_connections.emplace(peer, Ephemeral<ConnectionCallback>(callback, timeout));
  Start();
}

void Network::Stop()
{
  m_running = false;
}

void Network::Send(ENetPeer* socket, const sf::Packet& packet)
{
  Send(socket, packet, 0, 0);
}

void Network::Send(ENetPeer* socket, const sf::Packet& packet, PacketCallback callback)
{
  Send(socket, packet, callback, CALLBACK_DEFAULT_TIMEOUT);
}

void Network::Send(ENetPeer* socket, const sf::Packet& packet, PacketCallback callback,
                   u32 callback_timeout)
{
  u16 seq = GetNextSequenceNumber();
  Ephemeral<PacketCallback> entry(callback, callback_timeout);
  m_packet_callbacks.emplace(seq, entry);
  Send(socket, packet, seq, 0);
}

void Network::Send(ENetPeer* socket, const sf::Packet& packet, u16 send_seq, u16 reply_seq)
{
  sf::Packet sequenced_packet;
  sequenced_packet << send_seq;
  sequenced_packet << reply_seq;
  sequenced_packet.append(packet.getData(), packet.getDataSize());

  if (m_thread.get_id() == std::this_thread::get_id())
  {
    SendPacket(socket, sequenced_packet);
  }
  else
  {
    m_queued_packets.emplace(socket, sequenced_packet);
    ENetUtil::WakeupThread(m_host);
  }
}

void Network::SendPacket(ENetPeer* socket, const sf::Packet packet)
{
  ENetPacket* enet_packet =
      enet_packet_create(packet.getData(), packet.getDataSize(), ENET_PACKET_FLAG_RELIABLE);

  enet_peer_send(socket, ENET_CHANEL, enet_packet);
}

void Network::Disconnect(ENetPeer* peer)
{
  enet_peer_disconnect(peer, ENET_DISCONNECT_DATA);
}

void Network::RunOnTick(TickFunction fn)
{
  m_tick_functions.push_back(fn);
}

void Network::Start()
{
  if (m_running)
    return;

  m_thread = std::thread([this]() {
    m_running = true;
    while (m_running)
    {
      MainLoop();
    }
  });
}

void Network::MainLoop()
{
  ENetEvent net_event;
  int net = enet_host_service(m_host, &net_event, LOOP_TIMEOUT);

  for (TickFunction fn : m_tick_functions)
    fn();

  if (net <= 0)
    return;  // no event to process

  switch (net_event.type)
  {
  case ENET_EVENT_TYPE_CONNECT:
    HandleConnect(net_event);
    break;

  case ENET_EVENT_TYPE_RECEIVE:
    HandleData(net_event);
    break;

  case ENET_EVENT_TYPE_DISCONNECT:
    HandleDisconnect(net_event);
    break;

  default:
    break;
  }
}

void Network::OnData(DataCallback callback)
{
  m_data_callbacks.push_back(callback);
}

void Network::OnPeerDisconnect(ConnectionCallback callback)
{
  m_disconnect_callbacks.push_back(callback);
}

void Network::HandleConnect(const ENetEvent& net_event)
{
  ENetPeer* socket = net_event.peer;

  auto entry = m_pending_connections.find(socket);

  if (entry == m_pending_connections.end())
    return;

  entry->second.GetValue()(socket);
}

void Network::HandleData(const ENetEvent& net_event)
{
  void* data = net_event.packet->data;
  size_t data_length = net_event.packet->dataLength;

  sf::Packet packet;
  packet.append(data, data_length);

  u16 send_seq;
  packet >> send_seq;

  u16 reply_seq;
  packet >> reply_seq;

  auto callback_entry = m_packet_callbacks.find(reply_seq);

  if (callback_entry != m_packet_callbacks.end())
  {
    callback_entry->second.GetValue()(packet);
    m_packet_callbacks.erase(callback_entry);
  }

  for (DataCallback cb : m_data_callbacks)
  {
    cb(net_event.peer, packet,
       [&](const sf::Packet& new_packet) { Send(net_event.peer, new_packet, 0, send_seq); });
  }
}

void Network::HandleDisconnect(const ENetEvent& net_event)
{
  ENetPeer* socket = net_event.peer;

  for (ConnectionCallback cb : m_disconnect_callbacks)
    cb(socket);

  Disconnect(socket);
}

void Network::FlushTimedOutCallbacks()
{
  for (auto entry = m_packet_callbacks.begin(); entry != m_packet_callbacks.end();)
  {
    if (!entry->second.HasTimedOut())
    {
      ++entry;
    }
    else
    {
      sf::Packet packet;
      entry->second.GetValue()(packet);
      m_packet_callbacks.erase(entry);
    }
  }

  for (auto entry = m_pending_connections.begin(); entry != m_pending_connections.end();)
  {
    if (!entry->second.HasTimedOut())
    {
      ++entry;
    }
    else
    {
      entry->second.GetValue()(nullptr);
      m_pending_connections.erase(entry);
    }
  }
}

void Network::SendQueuedPackets()
{
  while (m_queued_packets.size() > 0)
  {
    auto entry = m_queued_packets.front();
    SendPacket(entry.first, entry.second);
    m_queued_packets.pop();
  }
}

u16 Network::GetNextSequenceNumber()
{
  return m_sequence_number = static_cast<u16>(++m_sequence_number % (1 << 16));
}

Network::~Network()
{
  enet_host_destroy(m_host);
}
}