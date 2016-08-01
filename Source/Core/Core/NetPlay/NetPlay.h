// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <SFML/Network/Packet.hpp>
#include <map>
#include <vector>

#include "EventEmitter.h"
#include "Network.h"
#include "Peer.h"

namespace NetPlay
{
enum class MeshMessageType : u16
{
  // network messages
  None,
  Hello,
  Welcome,
  GetNextPeerId,
  Ping,
  Pong,
  Peer,

  // netplay messages
  ChatMessage,
};

enum class ConnectionStatus
{
  Success,
  FailureCannotConnect,
  FailureFailedNegotiation,
};

using MeshPacketCallback = std::function<void(Peer, sf::Packet)>;
using MeshPacketsCallback = std::function<void(std::map<Peer, sf::Packet>)>;
using MeshConnectionCallback = std::function<void(ConnectionStatus)>;

sf::Packet& operator>>(sf::Packet& packet, MeshMessageType& msg_type);
sf::Packet& operator<<(sf::Packet& packet, const MeshMessageType& msg_type);

class NetPlay
{
public:
  NetPlay(Network& network);

  void Start(u16 port, const std::string& player_name);
  void Join(const std::string& address, u16 port, u32 timeout, MeshConnectionCallback callback);
  void Disconnect(const Peer& peer);

  void Send(const Peer& peer, const sf::Packet& packet);
  void Send(const Peer& peer, const sf::Packet& packet, PacketCallback callback);
  void Send(const Peer& peer, const sf::Packet& packet, u32 callback_timeout,
            PacketCallback callback);
  void Broadcast(const sf::Packet& packet);
  void Broadcast(const sf::Packet& packet, MeshPacketsCallback callback);
  void Broadcast(const sf::Packet& packet, u32 callback_timeout, MeshPacketsCallback callback);

  // players
  void OnPeerListChanged(std::function<void(std::map<u16, Peer>)> callback);

  // chat
  void OnChatMessage(std::function<void(Peer, std::string)> callback);
  void SendChatMessage(const std::string& message);

  std::map<u16, Peer> GetPeers() const;

private:
  void HandleHello(ENetPeer* peer, sf::Packet& packet, PacketCallback reply);
  void HandleGetNextPeerId(PacketCallback reply);
  void HandlePeer(sf::Packet& packet);
  void HandlePing(PacketCallback reply);
  void HandleChatMessage(ENetPeer* peer, sf::Packet& packet);

  u16 GetNextPeerId();
  void AdvertisePeers();
  void FlushTimedOutPeers();
  Peer* PeerFromSocket(ENetPeer* peer);

  static const u32 GET_NEXT_PEER_ID_TIMEOUT = 2000;
  static const u32 CONNECT_TIMEOUT = 2000;
  static const u32 ADVERTISE_PEER_EVERY = 1000;
  static const u32 PEER_TIMEOUT = 3000;

  Network& m_network;
  u16 m_peer_id;
  std::map<u16, Peer> m_peers;
  u16 m_next_peer_id;
  u32 m_last_advertise_time;
  EventEmitter<Peer, sf::Packet> m_data_event;

  std::string m_player_name;

  EventEmitter<Peer, std::string> m_chat_event;
  EventEmitter<std::map<u16, Peer>> m_peers_event;
};
}