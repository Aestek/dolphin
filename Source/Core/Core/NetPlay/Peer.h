// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <enet/enet.h>
#include <string>

#include "Common/CommonTypes.h"

namespace NetPlay
{
class Peer
{
public:
  Peer(u16 id, ENetPeer* socket) : m_id(id), m_socket(socket) {}
  u16 GetId() const { return m_id; }
  ENetPeer* GetSocket() const { return m_socket; }
  u32 GetLastSeen() const { return m_last_seen; }
  void SetLastSeen(u32 last_seen) { m_last_seen = last_seen; }
  u32 GetPing() const { return m_ping; }
  void SetPing(u32 ping) { m_ping = ping; }
  std::string GetName() const { return m_name; }
  void SetName(const std::string& name) { m_name = name; }
  std::string GetAddress() const
  {
    std::array<char, 255> addr;
    enet_address_get_host_ip(&m_socket->address, &addr[0], 255);
    return std::string(&addr[0]);
  }
  u16 GetPort() const { return m_socket->address.port; }
  bool operator<(const Peer& other) const { return this->GetId() < other.GetId(); }
  bool operator==(const Peer& other) const { return this->GetId() == other.GetId(); }
  bool operator!=(const Peer& other) const { return this->GetId() != other.GetId(); }
private:
  u16 m_id;
  ENetPeer* m_socket;
  u32 m_last_seen;
  u32 m_ping;
  std::string m_name;
};
}