// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <SFML/Network/Packet.hpp>
#include <map>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "Ephemeral.h"
#include "Peer.h"
#include "Router.h"

namespace NetPlay
{
using ConnectionCallback = std::function<void(ENetPeer*)>;
using PacketCallback = std::function<void(sf::Packet&)>;
using DataCallback = std::function<void(ENetPeer*, sf::Packet&, PacketCallback)>;
using TickFunction = std::function<void()>;

class Network
{
public:
  Network();
  ~Network();

  bool Listen(u16 port);
  void Connect(const std::string& address, u16 port, u32 timeout, ConnectionCallback callback);
  void Disconnect(ENetPeer* peer);
  void Stop();

  void Send(ENetPeer* socket, const sf::Packet& packet);
  void Send(ENetPeer* socket, const sf::Packet& packet, PacketCallback callback);
  void Send(ENetPeer* socket, const sf::Packet& packet, PacketCallback callback,
            u32 callback_timeout);

  void RunOnTick(TickFunction fn);

  void OnData(DataCallback callback);
  void OnPeerDisconnect(ConnectionCallback callback);

  static const u32 CALLBACK_DEFAULT_TIMEOUT = 0;

private:
  void Start();
  void MainLoop();
  void HandleConnect(const ENetEvent& net_event);
  void HandleDisconnect(const ENetEvent& net_event);
  void HandleData(const ENetEvent& net_event);
  void Send(ENetPeer* socket, const sf::Packet& packet, u16 sed_seq, u16 reply_seq);
  void SendPacket(ENetPeer* peer, const sf::Packet packet);
  void FlushTimedOutCallbacks();
  void SendQueuedPackets();
  u16 GetNextSequenceNumber();

  static const int ENET_CHANEL = 0;
  static const int ENET_CHANEL_COUNT = 3;
  static const int ENET_PEER_COUNT = 10;
  static const int ENET_MAX_IN_BANDWIDTH = 0;
  static const int ENET_MAX_OUT_BANDWIDTH = 0;
  static const int ENET_CONNECT_DATA = 0;
  static const int ENET_DISCONNECT_DATA = 0;
  static const u32 LOOP_TIMEOUT = 1000;

  ENetHost* m_host = nullptr;
  std::thread m_thread;
  bool m_running = false;
  bool m_is_listening = false;
  u16 m_sequence_number = 0;

  std::map<u32, Ephemeral<PacketCallback>> m_packet_callbacks;
  std::vector<DataCallback> m_data_callbacks;
  std::vector<ConnectionCallback> m_disconnect_callbacks;
  std::vector<TickFunction> m_tick_functions;
  std::map<ENetPeer*, Ephemeral<ConnectionCallback>> m_pending_connections;
  std::queue<std::pair<ENetPeer*, sf::Packet>> m_queued_packets;
};
}