// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <enet/enet.h>
#include <utility>
#include <vector>

#include "PingBasedRouter.h"
#include "Router.h"

namespace NetPlay
{
const Peer PingBasedRouter::SELF = Peer(0, nullptr);

void PingBasedRouter::AddRoute(const Peer& from, const Peer& to, u32 ping)
{
  std::pair<Peer, Peer> peers = std::make_pair(from, to);
  m_routes[peers] = ping;
}

const Peer* PingBasedRouter::GetNextHop(const Peer& to)
{
  return &to;
}
}