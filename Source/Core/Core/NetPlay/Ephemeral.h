// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Timer.h"

template <typename T>
class Ephemeral
{
public:
  Ephemeral(T value, u32 timeout) : m_value(value)
  {
    m_timeouts_at = Common::Timer::GetTimeMs() + timeout;
  }

  bool HasTimedOut() const
  {
    if (m_timeouts_at == 0)
      return false;

    return Common::Timer::GetTimeMs() >= m_timeouts_at;
  }

  T GetValue() const { return m_value; }
private:
  u32 m_timeouts_at;
  T m_value;
};