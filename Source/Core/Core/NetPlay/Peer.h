// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <enet/enet.h>
#include <string>

namespace NetPlay
{
class Peer
{
public:
  Peer(u16 id, ENetPeer* socket);
  ~Peer();
  u16 GetId() const;
  ENetPeer* GetSocket() const;
  u32 GetLastSeen() const;
  void SetLastSeen(u32 last_seen);
  u32 GetPing() const;
  void SetPing(u32 ping);

  bool operator<(const Peer& other) const;
  bool operator==(const Peer& other) const;
  bool operator!=(const Peer& other) const;

private:
  u16 m_id;
  ENetPeer* m_socket;
  u32 m_last_seen;
  u32 m_ping;
};
}