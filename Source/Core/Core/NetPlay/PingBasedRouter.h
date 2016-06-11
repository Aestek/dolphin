// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <utility>

#include "Peer.h"
#include "Router.h"

namespace NetPlay
{
class Route
{
public:
  Peer from;
  Peer to;
};

class PingBasedRouter : Router
{
public:
  void AddRoute(const Peer& from, const Peer& to, u32 ping) override;
  const Peer* GetNextHop(const Peer& to) override;

  static const Peer SELF;

private:
  static const u16 MAX_HOP = 1;
  std::map<std::pair<Peer, Peer>, u32> m_routes;
};
}