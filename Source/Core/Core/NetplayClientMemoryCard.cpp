// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <chrono>
#include <cstring>
#include <memory>
#include <string>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Thread.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/GCMemcard.h"
#include "NetplayClientMemoryCard.h"

NetplayClientMemoryCard::NetplayClientMemoryCard(NetPlayClient* netplay_client)
	: MemoryCardBase(0, MemCard2043Mb)
	, m_netplay_client(netplay_client)
{
}

NetplayClientMemoryCard::~NetplayClientMemoryCard()
{
	// noop
}

void NetplayClientMemoryCard::FlushThread()
{
	// noop
}

void NetplayClientMemoryCard::MakeDirty()
{
	// noop
}

s32 NetplayClientMemoryCard::Read(u32 src_address, s32 length, u8* dest_address)
{
	printf("NetplayClientMemoryCard: read\n");
	u8* data = m_netplay_client->ReadRemoteMemcard(src_address, length);
	printf("NetplayClientMemoryCard: got res\n");
	memcpy(dest_address, data, length);
	free(data);
	return length;
}

s32 NetplayClientMemoryCard::Write(u32 destaddress, s32 length, u8 *srcaddress)
{
	// noop
	return length;
}

void NetplayClientMemoryCard::ClearBlock(u32 address)
{
	// noop
}

void NetplayClientMemoryCard::ClearAll()
{
	// noop
}

void NetplayClientMemoryCard::DoState(PointerWrap &p)
{
	printf("Do state\n");
	// noop
}
