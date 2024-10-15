#include <cstdint>
#include <cstdio>
#include "memorypatch.h"
#include "memutil.h"

MemoryPatch::MemoryPatch(GameData *gamedata)
{
	m_GameData = gamedata;
	m_pAddress = nullptr;
	m_key = "";
	m_vecPatch = {};
	m_vecRestore = {};
	m_vecVerify = {};
	m_vecPreserve = {};
}

MemoryPatch::~MemoryPatch()
{
	DisablePatch();
}

bool MemoryPatch::CreatePatch(const char *key)
{
	if (!m_GameData || !m_GameData->m_pkv)
		return false;

	if (m_vecRestore.size() > 0)
		return false;

	char buffer[512];
	snprintf(buffer, sizeof(buffer), "%s/MemPatches/%s/%s", m_GameData->m_Game, key, m_GameData->m_Platform);
	KeyValues *kv = m_GameData->m_pkv->FindKey(buffer);
	if (!kv)
		return false;

	const char *sOffset = kv->GetString("offset");
	const char *signKey = kv->GetString("signature");
	const char *addrKey = kv->GetString("address");
	const char *verify = kv->GetString("verify");
	const char *patch = kv->GetString("patch");
	const char *preserve = kv->GetString("preserve");

	if (!patch[0])
		return false;

	if (!signKey[0] && !addrKey[0])
		return false;

	void *addr = signKey[0] ? m_GameData->GetMemSig(signKey) : m_GameData->GetAddress(addrKey);
	if (!addr)
		return false;

	int iOffset = 0;
	if (sOffset[0])
		iOffset = memutil::StrToInt(sOffset);

	m_pAddress = (void*)(uintptr_t(addr) + iOffset);
	m_vecPatch = memutil::DecodeHexString(patch);
	m_vecVerify = memutil::DecodeHexString(verify);
	m_vecPreserve = memutil::DecodeHexString(preserve);

	m_key = key;
	return m_vecPatch.size() > 0;
}


bool MemoryPatch::EnablePatch()
{
	if (IsEnabled())
		return false;
	
	if (!VerifyPatch())
		return false;
	
	uint8_t *addr = (uint8_t*)m_pAddress;

	m_vecRestore.assign(addr, addr + m_vecPatch.size());
	memutil::SetMemAccess(addr, m_vecPatch.size(), _PAGE_EXECUTE_READWRITE);
	std::copy(m_vecPatch.begin(), m_vecPatch.end(), addr);

	if (m_vecPreserve.size() == 0)
		return true;
		
	for (size_t i = 0; i < m_vecPatch.size(); i++)
	{
		uint8_t preserveBits = 0;
		if (i < m_vecPreserve.size())
			preserveBits = m_vecPreserve[i];
		*(addr + i) = (m_vecPatch[i] & ~preserveBits) | (m_vecRestore[i] & preserveBits);
	}

	return true;
}

bool MemoryPatch::VerifyPatch()
{
	if (!m_pAddress || m_vecPatch.size() == 0)
		return false;

	uint8_t *addr = (uint8_t*)m_pAddress;
	for (size_t i = 0; i < m_vecVerify.size(); i++)
	{
		if (m_vecVerify[i] != '*' && m_vecVerify[i] != addr[i])
			return false;
	}
	return true;
}

void MemoryPatch::DisablePatch()
{
	// no memory to restore, fug
	if (m_vecRestore.size() == 0)
		return;

	std::copy(m_vecRestore.begin(), m_vecRestore.end(), (uint8_t*)m_pAddress);
	m_vecRestore.clear();
}

bool MemoryPatch::IsEnabled()
{
	return m_vecRestore.size() > 0;
}

void *MemoryPatch::GetAddress()
{
	return m_pAddress;
}

const char *MemoryPatch::GeKey()
{
	return m_key.c_str();
}