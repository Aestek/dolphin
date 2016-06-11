// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <SFML/Network/Packet.hpp>
#include <algorithm>
#include <map>

#include "Common/Timer.h"
#include "MeshNetwork.h"
#include "Network.h"

namespace NetPlay
{
sf::Packet& operator>>(sf::Packet& packet, MeshMessageType& msg_type)
{
  u16 type;
  packet >> type;
  msg_type = static_cast<NetPlay::MeshMessageType>(type);
  return packet;
}

sf::Packet& operator<<(sf::Packet& packet, const NetPlay::MeshMessageType& msg_type)
{
  packet << static_cast<u16>(msg_type);
  return packet;
}

MeshNetwork::MeshNetwork(Network& network) : m_network(network)
{
  m_network.OnData([this](ENetPeer* peer, sf::Packet& packet, PacketCallback callback) {
    MeshMessageType msg_type;
    packet >> msg_type;

    switch (msg_type)
    {
    case MeshMessageType::Hello:
      HandleHello(peer, callback);
      break;

    case MeshMessageType::GetNextPeerId:
      HandleGetNextPeerId(callback);
      break;

    case MeshMessageType::Peer:
      HandlePeer(packet);
      break;

    case MeshMessageType::Ping:
      HandlePing(callback);

    default:
      break;
    }

  });

  m_network.RunOnTick([&]() {
    u32 now = Common::Timer::GetTimeMs();
    if (now - m_last_advertise_time < ADVERTISE_PEER_EVERY)
      return;

    AdvertisePeers();
    m_last_advertise_time = now;
  });
}

void MeshNetwork::Join(const std::string& address, u16 port, u32 timeout)
{
  ENetPeer* peer = m_network.Connect(address, port, timeout);
  sf::Packet packet;
  packet << MeshMessageType::Hello;

  m_network.Send(peer, packet, [&](const sf::Packet& accept_packet) {
    MeshMessageType msg_type;
    packet >> msg_type;
    packet >> m_peer_id;
  });
}

void MeshNetwork::Disconnect(const Peer& peer)
{
  m_network.Disconnect(peer.GetSocket());
  m_peers.erase(peer.GetId());
}

void MeshNetwork::Send(const Peer& peer, const sf::Packet& packet)
{
  m_network.Send(peer.GetSocket(), packet);
}

void MeshNetwork::Send(const Peer& peer, const sf::Packet& packet, PacketCallback callback)
{
  m_network.Send(peer.GetSocket(), packet, callback);
}

void MeshNetwork::Send(const Peer& peer, const sf::Packet& packet, PacketCallback callback,
                       u32 callback_timeout)
{
  m_network.Send(peer.GetSocket(), packet, callback, callback_timeout);
}

void MeshNetwork::Broadcast(const sf::Packet& packet)
{
  for (const auto& entry : m_peers)
  {
    Send(entry.second, packet);
  }
}

void MeshNetwork::Broadcast(const sf::Packet& packet, MeshPacketsCallback callback)
{
  Broadcast(packet, callback, Network::CALLBACK_DEFAULT_TIMEOUT);
}

void MeshNetwork::Broadcast(const sf::Packet& packet, MeshPacketsCallback callback,
                            u32 callback_timeout)
{
  std::map<Peer, sf::Packet> responses;
  std::size_t peer_count = m_peers.size();

  std::for_each(m_peers.begin(), m_peers.end(), [&](std::pair<u16, Peer> entry) {
    Send(entry.second, packet,
         [&](const sf::Packet& new_packet) {
           responses.emplace(entry.second, new_packet);
           if (responses.size() >= peer_count)
             callback(responses);
         },
         callback_timeout);
  });
}

u16 MeshNetwork::GetNextPeerId()
{
  return ++m_next_peer_id;
}

void MeshNetwork::HandleGetNextPeerId(PacketCallback reply)
{
  sf::Packet packet;
  packet << MeshMessageType::None;
  packet << GetNextPeerId();
  reply(packet);
}

void MeshNetwork::HandleHello(ENetPeer* socket, PacketCallback reply)
{
  sf::Packet packet;
  packet << MeshMessageType::GetNextPeerId;

  Broadcast(packet,
            [&](std::map<Peer, sf::Packet> packets) {
              u16 peer_id = 0;

              for (auto entry : packets)
              {
                u16 remote_peer_id;
                entry.second >> remote_peer_id;

                if (remote_peer_id > peer_id)
                  peer_id = remote_peer_id;
              }

              peer_id = std::max(peer_id, GetNextPeerId());

              Peer peer(peer_id, socket);
              peer.SetLastSeen(Common::Timer::GetTimeMs());
              m_peers.emplace(peer_id, peer);

              sf::Packet hello_packet;
              hello_packet << MeshMessageType::Welcome;
              hello_packet << peer_id;

              reply(hello_packet);

            },
            GET_NEXT_PEER_ID_TIMEOUT);
}

void MeshNetwork::AdvertisePeers()
{
  std::for_each(m_peers.begin(), m_peers.end(), [this](auto entry) {
    sf::Packet ping_packet;
    ping_packet << MeshMessageType::Hello;
    ping_packet << entry.first;

    u32 sent_at = Common::Timer::GetTimeMs();
    this->Send(entry.second, ping_packet, [&, this](sf::Packet& pong_packet) {
      MeshMessageType msg_type;
      pong_packet >> msg_type;

      u32 ping = Common::Timer::GetTimeMs() - sent_at;
      entry.second.SetPing(ping);
      entry.second.SetLastSeen(Common::Timer::GetTimeMs());

      std::array<char, 255> addr;
      enet_address_get_host_ip(&entry.second.GetSocket()->address, &addr[0], 255);
      u16 port = entry.second.GetSocket()->address.port;

      sf::Packet peer_packet;
      peer_packet << MeshMessageType::Peer;
      peer_packet << entry.second.GetId();
      peer_packet << ping;
      peer_packet << &addr[0] << port;

      this->Broadcast(peer_packet);
    });
  });
}

void MeshNetwork::HandlePeer(sf::Packet& packet)
{
  u16 peer_id;
  u32 ping;

  packet >> peer_id >> ping;

  if (peer_id == m_peer_id)
    return;

  u32 now = Common::Timer::GetTimeMs();

  auto peer_entry = m_peers.find(peer_id);

  if (peer_entry != m_peers.end())
  {
    // we know this peer, update it
    peer_entry->second.SetPing(ping);
    peer_entry->second.SetLastSeen(now);
  }
  else
  {
    // this peer is in the network, but we don't know it, connect to it
    std::string addr;
    u16 port;

    packet >> addr >> port;

    ENetPeer* socket = m_network.Connect(addr, port, CONNECT_TIMEOUT);
    if (socket == nullptr)
      return;

    Peer peer(peer_id, socket);
    peer.SetPing(ping);
    peer.SetLastSeen(now);
    m_peers.emplace(peer_id, peer);
  }
}

void MeshNetwork::HandlePing(PacketCallback reply)
{
  sf::Packet packet;
  reply(packet);
}

void MeshNetwork::FlushTimedOutPeers()
{
  u32 now = Common::Timer::GetTimeMs();
  for (auto entry = m_peers.cbegin(); entry != m_peers.cend();)
  {
    if (now - entry->second.GetLastSeen() > PEER_TIMEOUT)
      Disconnect(entry->second);
    else
      ++entry;
  }
}
}
