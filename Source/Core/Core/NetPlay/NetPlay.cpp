// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <SFML/Network/Packet.hpp>
#include <algorithm>
#include <map>

#include "Common/Timer.h"
#include "NetPlay.h"
#include "Network.h"

namespace NetPlay
{
sf::Packet& operator>>(sf::Packet& packet, MeshMessageType& msg_type)
{
  u16 type;
  packet >> type;
  msg_type = static_cast<MeshMessageType>(type);
  return packet;
}

sf::Packet& operator<<(sf::Packet& packet, const MeshMessageType& msg_type)
{
  packet << static_cast<u16>(msg_type);
  return packet;
}

NetPlay::NetPlay(Network& network) : m_network(network)
{
  m_network.OnData([this](ENetPeer* peer, sf::Packet& packet, PacketCallback callback) {
    MeshMessageType msg_type;
    packet >> msg_type;

    switch (msg_type)
    {
    case MeshMessageType::Hello:
      HandleHello(peer, packet, callback);
      break;

    case MeshMessageType::GetNextPeerId:
      HandleGetNextPeerId(callback);
      break;

    case MeshMessageType::Peer:
      HandlePeer(packet);
      break;

    case MeshMessageType::Ping:
      HandlePing(callback);
      break;

    case MeshMessageType::ChatMessage:
      HandleChatMessage(peer, packet);
      break;

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

  m_network.RunOnTick(std::bind(&NetPlay::FlushTimedOutPeers, this));
}

void NetPlay::Start(u16 port, const std::string& player_name)
{
  m_network.Listen(port);
  m_player_name = player_name;
}

void NetPlay::Join(const std::string& address, u16 port, u32 timeout,
                   MeshConnectionCallback callback)
{
  m_network.Connect(address, port, timeout, [&](ENetPeer* peer) {
    if (!peer)
    {
      callback(ConnectionStatus::FailureCannotConnect);
      return;
    }

    sf::Packet packet;
    packet << MeshMessageType::Hello;
    packet << m_player_name;

    m_network.Send(peer, packet, [&](const sf::Packet& accept_packet) {
      if (accept_packet.endOfPacket())
      {
        callback(ConnectionStatus::FailureFailedNegotiation);
        return;
      }

      MeshMessageType msg_type;
      packet >> msg_type;
      packet >> m_peer_id;

      callback(ConnectionStatus::Success);
    });
  });
}

void NetPlay::Disconnect(const Peer& peer)
{
  m_network.Disconnect(peer.GetSocket());
  m_peers.erase(peer.GetId());
}

void NetPlay::Send(const Peer& peer, const sf::Packet& packet)
{
  m_network.Send(peer.GetSocket(), packet);
}

void NetPlay::Send(const Peer& peer, const sf::Packet& packet, PacketCallback callback)
{
  m_network.Send(peer.GetSocket(), packet, callback);
}

void NetPlay::Send(const Peer& peer, const sf::Packet& packet, u32 callback_timeout,
                   PacketCallback callback)
{
  m_network.Send(peer.GetSocket(), packet, callback, callback_timeout);
}

void NetPlay::Broadcast(const sf::Packet& packet)
{
  for (const auto& entry : m_peers)
  {
    Send(entry.second, packet);
  }
}

void NetPlay::Broadcast(const sf::Packet& packet, MeshPacketsCallback callback)
{
  Broadcast(packet, Network::CALLBACK_DEFAULT_TIMEOUT, callback);
}

void NetPlay::Broadcast(const sf::Packet& packet, u32 callback_timeout,
                        MeshPacketsCallback callback)
{
  std::map<Peer, sf::Packet> responses;
  std::size_t peer_count = m_peers.size();

  std::for_each(m_peers.begin(), m_peers.end(), [&](std::pair<u16, Peer> entry) {
    Send(entry.second, packet, callback_timeout, [&](const sf::Packet& new_packet) {
      responses.emplace(entry.second, new_packet);
      if (responses.size() >= peer_count)
        callback(responses);
    });
  });
}

std::map<u16, Peer> NetPlay::GetPeers() const
{
  return m_peers;
};

u16 NetPlay::GetNextPeerId()
{
  return ++m_next_peer_id;
}

void NetPlay::HandleGetNextPeerId(PacketCallback reply)
{
  sf::Packet packet;
  packet << MeshMessageType::None;
  packet << GetNextPeerId();
  reply(packet);
}

void NetPlay::HandleHello(ENetPeer* socket, sf::Packet& hello_packet, PacketCallback reply)
{
  std::string player_name;
  hello_packet >> player_name;

  sf::Packet packet;
  packet << MeshMessageType::GetNextPeerId;

  Broadcast(packet, GET_NEXT_PEER_ID_TIMEOUT, [&](std::map<Peer, sf::Packet> packets) {
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
    peer.SetName(player_name);
    m_peers.emplace(peer_id, peer);

    sf::Packet welcome_packet;
    welcome_packet << MeshMessageType::Welcome;
    welcome_packet << peer_id;

    reply(hello_packet);

    m_peers_event.Fire(m_peers);
  });
}

void NetPlay::AdvertisePeers()
{
  std::for_each(m_peers.begin(), m_peers.end(), [&](auto entry) {
    sf::Packet ping_packet;
    ping_packet << MeshMessageType::Hello;
    ping_packet << entry.first;

    u32 sent_at = Common::Timer::GetTimeMs();
    this->Send(entry.second, ping_packet, [&](sf::Packet& pong_packet) {
      MeshMessageType msg_type;
      pong_packet >> msg_type;

      u32 ping = Common::Timer::GetTimeMs() - sent_at;
      entry.second.SetPing(ping);
      entry.second.SetLastSeen(Common::Timer::GetTimeMs());

      sf::Packet peer_packet;
      peer_packet << MeshMessageType::Peer;
      peer_packet << entry.second.GetId();
      peer_packet << ping;
      peer_packet << entry.second.GetAddress() << entry.second.GetPort();

      this->Broadcast(peer_packet);
    });
  });
}

void NetPlay::HandlePeer(sf::Packet& packet)
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

    m_network.Connect(addr, port, CONNECT_TIMEOUT, [&](ENetPeer* socket) {
      if (socket == nullptr)
        return;

      socket->data = new u16(peer_id);
      Peer peer(peer_id, socket);
      peer.SetPing(ping);
      peer.SetLastSeen(now);
      m_peers.emplace(peer_id, peer);
      m_peers_event.Fire(m_peers);
    });
  }
}

void NetPlay::HandlePing(PacketCallback reply)
{
  sf::Packet packet;
  reply(packet);
}

void NetPlay::FlushTimedOutPeers()
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

Peer* NetPlay::PeerFromSocket(ENetPeer* socket)
{
  u16 peer_id = *static_cast<u16*>(socket->data);
  auto peer = m_peers.find(peer_id);

  if (peer == m_peers.end())
    return nullptr;

  return &peer->second;
}

// chat

void NetPlay::SendChatMessage(const std::string& message)
{
  sf::Packet packet;
  packet << MeshMessageType::ChatMessage;
  packet << message;

  Broadcast(packet);
}

void NetPlay::HandleChatMessage(ENetPeer* socket, sf::Packet& packet)
{
  auto peer = PeerFromSocket(socket);

  if (!peer)
    return;

  std::string message;
  packet >> message;

  m_chat_event.Fire(*peer, message);
}

// peers

void NetPlay::OnPeerListChanged(std::function<void(std::map<u16, Peer>)> callback)
{
  m_peers_event.On(callback);
}
}
