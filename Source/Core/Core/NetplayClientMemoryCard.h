// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "Common/Event.h"
#include "Common/Flag.h"
#include "Core/HW/GCMemcard.h"
#include "NetPlayClient.h"

class PointerWrap;

class NetplayClientMemoryCard : public MemoryCardBase
{
public:
	NetplayClientMemoryCard(NetPlayClient* netplay_client);
	~NetplayClientMemoryCard();
	void FlushThread();
	void MakeDirty();

	s32 Read(u32 address, s32 length, u8 *destaddress) override;
	s32 Write(u32 destaddress, s32 length, u8 *srcaddress) override;
	void ClearBlock(u32 address) override;
	void ClearAll() override;
	void DoState(PointerWrap &p) override;

private:
	NetPlayClient*  m_netplay_client;
};
