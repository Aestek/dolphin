// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <functional>
#include <vector>

namespace NetPlay
{
template <typename... Args>
class EventEmitter
{
public:
  void On(std::function<void(Args...)> callback) { m_callbacks.push_back(callback); }
  void Off(std::function<void(Args...)> callback)
  {
    m_callbacks.erase(std::remove(m_callbacks.begin(), m_callbacks.end(), callback));
  }

  void Fire(Args... args) const
  {
    for (const auto& cb : m_callbacks)
    {
      cb(args...);
    }
  }

private:
  std::vector<std::function<void(Args...)>> m_callbacks;
};
}
