// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <SFML/Network/Packet.hpp>
#include <map>
#include <vector>

#include "Network.h"
#include "Peer.h"

namespace NetPlay
{
using MeshPacketsCallback = std::function<void(std::map<Peer, sf::Packet>)>;

enum class MeshMessageType
{
  None,
  Hello,
  Welcome,
  GetNextPeerId,
  Ping,
  Pong,
  Peer
};

sf::Packet& operator>>(sf::Packet& packet, NetPlay::MeshMessageType& msg_type);
sf::Packet& operator<<(sf::Packet& packet, const NetPlay::MeshMessageType& msg_type);

class MeshNetwork
{
public:
  MeshNetwork(Network& network);

  void Join(const std::string& address, u16 port, u32 timeout);
  void Disconnect(const Peer& peer);

  void Send(const Peer& peer, const sf::Packet& packet);
  void Send(const Peer& peer, const sf::Packet& packet, PacketCallback callback);
  void Send(const Peer& peer, const sf::Packet& packet, PacketCallback callback,
            u32 callback_timeout);
  void Broadcast(const sf::Packet& packet);
  void Broadcast(const sf::Packet& packet, MeshPacketsCallback callback);
  void Broadcast(const sf::Packet& packet, MeshPacketsCallback callback, u32 callback_timeout);

private:
  void HandleHello(ENetPeer* peer, PacketCallback reply);
  void HandleGetNextPeerId(PacketCallback reply);
  void HandlePeer(sf::Packet& packet);
  void HandlePing(PacketCallback reply);

  u16 GetNextPeerId();
  void AdvertisePeers();
  void FlushTimedOutPeers();

  static const u32 GET_NEXT_PEER_ID_TIMEOUT = 2000;
  static const u32 CONNECT_TIMEOUT = 2000;
  static const u32 ADVERTISE_PEER_EVERY = 1000;
  static const u32 PEER_TIMEOUT = 3000;

  Network& m_network;
  u16 m_peer_id;
  std::map<u16, Peer> m_peers;
  u16 m_next_peer_id;
  u32 m_last_advertise_time;
};
}