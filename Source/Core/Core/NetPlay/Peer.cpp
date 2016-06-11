// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <enet/enet.h>
#include <string>

#include "Peer.h"

namespace NetPlay
{
Peer::Peer(u16 id, ENetPeer* socket) : m_id(id), m_socket(socket)
{
}

Peer::~Peer()
{
  enet_peer_disconnect(m_socket, 0);
}

u16 Peer::GetId() const
{
  return m_id;
}

ENetPeer* Peer::GetSocket() const
{
  return m_socket;
}

u32 Peer::GetLastSeen() const
{
  return m_last_seen;
}

void Peer::SetLastSeen(u32 last_seen)
{
  m_last_seen = last_seen;
}

u32 Peer::GetPing() const
{
  return m_ping;
}

void Peer::SetPing(u32 ping)
{
  m_ping = ping;
}

bool Peer::operator<(const Peer& other) const
{
  return this->GetId() < other.GetId();
}

bool Peer::operator==(const Peer& other) const
{
  return this->GetId() == other.GetId();
}

bool Peer::operator!=(const Peer& other) const
{
  return this->GetId() != other.GetId();
}
}