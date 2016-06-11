// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <SFML/Network/Packet.hpp>
#include <algorithm>
#include <string>
#include <thread>

#include "Network.h"
#include "Peer.h"
#include "Router.h"

namespace NetPlay
{
Network::Network(const u16 port)
{
  ENetAddress server_addr;
  server_addr.host = ENET_HOST_ANY;
  server_addr.port = port;
  m_host = enet_host_create(&server_addr, ENET_PEER_COUNT, ENET_CHANEL_COUNT, ENET_MAX_IN_BANDWIDTH,
                            ENET_MAX_OUT_BANDWIDTH);

  m_thread = std::thread([this]() {
    m_running = true;
    while (m_running)
    {
      MainLoop();
    }
  });

  RunOnTick(std::bind(&Network::FlushTimeoutCallbacks, this));
};

void Network::Send(ENetPeer* socket, const sf::Packet& packet)
{
  Send(socket, packet, ++m_sequence_number);
}

void Network::Send(ENetPeer* socket, const sf::Packet& packet, PacketCallback callback)
{
  Send(socket, packet, callback, CALLBACK_DEFAULT_TIMEOUT);
}

void Network::Send(ENetPeer* socket, const sf::Packet& packet, PacketCallback callback,
                   u32 callback_timeout)
{
  u32 seq = ++m_sequence_number;
  PacketCallbackEntry entry(callback, callback_timeout);
  m_packet_callbacks.emplace(seq, entry);
  Send(socket, packet, seq);
}

void Network::Send(ENetPeer* socket, const sf::Packet& packet, u32 reply_seq)
{
  sf::Packet sequenced_packet;
  sequenced_packet << reply_seq;
  sequenced_packet.append(packet.getData(), packet.getDataSize());

  ENetPacket* enet_packet = enet_packet_create(
      sequenced_packet.getData(), sequenced_packet.getDataSize(), ENET_PACKET_FLAG_RELIABLE);

  enet_peer_send(socket, ENET_CHANEL, enet_packet);
}

ENetPeer* Network::Connect(const std::string& address, u16 port, u32 timeout)
{
  ENetAddress addr;
  enet_address_set_host(&addr, address.c_str());
  addr.port = port;

  ENetPeer* peer = enet_host_connect(m_host, &addr, ENET_CHANEL_COUNT, ENET_CONNECT_DATA);

  if (!peer)
    return nullptr;

  ENetEvent net_event;
  int net = enet_host_service(m_host, &net_event, timeout);
  if (net <= 0 || net_event.type != ENET_EVENT_TYPE_CONNECT)
    return nullptr;

  return peer;
}

void Network::Disconnect(ENetPeer* peer)
{
  enet_peer_disconnect(peer, ENET_DISCONNECT_DATA);
}

void Network::RunOnTick(TickFunction fn)
{
  m_tick_functions.push_back(fn);
}

void Network::MainLoop()
{
  ENetEvent net_event;
  int net = enet_host_service(m_host, &net_event, LOOP_TIMEOUT);

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

  for (TickFunction fn : m_tick_functions)
    fn();
}

void Network::OnData(DataCallback callback)
{
  m_data_callbacks.push_back(callback);
}

void Network::OnConnect(ConnectionCallback callback)
{
  m_connect_callbacks.push_back(callback);
}

void Network::OnDisconnect(ConnectionCallback callback)
{
  m_disconnect_callbacks.push_back(callback);
}

void Network::HandleConnect(const ENetEvent& net_event)
{
  ENetPeer* socket = net_event.peer;

  for (ConnectionCallback cb : m_connect_callbacks)
    cb(socket);
}

void Network::HandleData(const ENetEvent& net_event)
{
  void* data = net_event.packet->data;
  size_t data_length = net_event.packet->dataLength;

  sf::Packet packet;
  packet.append(data, data_length);

  u32 reply_seq;
  packet >> reply_seq;
  auto callback_entry = m_packet_callbacks.find(reply_seq);

  if (callback_entry != m_packet_callbacks.end())
  {
    callback_entry->second(packet);
    m_packet_callbacks.erase(callback_entry);
  }

  for (DataCallback cb : m_data_callbacks)
  {
    cb(net_event.peer, packet,
       [&](const sf::Packet& new_packet) { Send(net_event.peer, new_packet, reply_seq); });
  }
}

void Network::HandleDisconnect(const ENetEvent& net_event)
{
  ENetPeer* socket = net_event.peer;

  for (ConnectionCallback cb : m_disconnect_callbacks)
    cb(socket);

  Disconnect(socket);
}

void Network::FlushTimeoutCallbacks()
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
      entry->second(packet);
      m_packet_callbacks.erase(entry);
    }
  }
}

Network::~Network()
{
  enet_host_destroy(m_host);
}
}