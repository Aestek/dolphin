// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Peer.h"

namespace NetPlay
{
class Router
{
public:
  virtual void AddRoute(const Peer& from, const Peer& to, u32 ping) = 0;
  virtual const Peer* GetNextHop(const Peer& to) = 0;
};
}