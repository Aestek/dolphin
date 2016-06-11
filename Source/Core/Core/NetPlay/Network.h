// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <SFML/Network/Packet.hpp>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "Common/Timer.h"
#include "Peer.h"
#include "Router.h"

namespace NetPlay
{
using ConnectionCallback = std::function<void(ENetPeer*)>;
using PacketCallback = std::function<void(sf::Packet&)>;
using DataCallback = std::function<void(ENetPeer*, sf::Packet&, PacketCallback)>;
using TickFunction = std::function<void()>;

class PacketCallbackEntry
{
public:
  PacketCallbackEntry(const PacketCallback& callback, u32 timeout) : m_callback(callback)
  {
    m_timeouts_at = Common::Timer::GetTimeMs() + timeout;
  }

  bool HasTimedOut()
  {
    if (m_timeouts_at == 0)
      return false;

    return Common::Timer::GetTimeMs() >= m_timeouts_at;
  }

  void operator()(sf::Packet& packet) { m_callback(packet); }
private:
  u32 m_timeouts_at;
  PacketCallback m_callback;
};

class Network
{
public:
  Network(const u16 port);
  ~Network();

  ENetPeer* Connect(const std::string& address, u16 port, u32 timeout);
  void Disconnect(ENetPeer* peer);

  void Send(ENetPeer* socket, const sf::Packet& packet);
  void Send(ENetPeer* socket, const sf::Packet& packet, PacketCallback callback);
  void Send(ENetPeer* socket, const sf::Packet& packet, PacketCallback callback,
            u32 callback_timeout);

  void RunOnTick(TickFunction fn);

  void OnData(DataCallback callback);
  void OnConnect(ConnectionCallback callback);
  void OnDisconnect(ConnectionCallback callback);

  static const u32 CALLBACK_DEFAULT_TIMEOUT = 0;

private:
  void MainLoop();
  void HandleConnect(const ENetEvent& net_event);
  void HandleDisconnect(const ENetEvent& net_event);
  void HandleData(const ENetEvent& net_event);
  void Send(ENetPeer* socket, const sf::Packet& packet, u32 reply_seq);
  void FlushTimeoutCallbacks();

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
  u32 m_sequence_number = 0;

  std::map<u32, PacketCallbackEntry> m_packet_callbacks;
  std::vector<DataCallback> m_data_callbacks;
  std::vector<ConnectionCallback> m_connect_callbacks;
  std::vector<ConnectionCallback> m_disconnect_callbacks;
  std::vector<TickFunction> m_tick_functions;
};
}