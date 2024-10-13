#include <cstdint>
#include "gamedata.h"
#include "memutil.h"


GameData::GameData(const char *game)
{
	m_Game = game;
	m_pkv = nullptr;

#ifdef _WIN32
	m_Platform = "windows";
	m_libSuffix = ".dll";
#else
	m_Platform = "linux";
	m_libSuffix = "_srv.so";
#endif
}

GameData::~GameData()
{
	if (m_pkv)
		m_pkv->deleteThis();
	m_pkv = nullptr;
}

bool GameData::LoadFile(IFileSystem *filesystem, const char *file, const char *pathID)
{
	m_pkv = new KeyValues("");
	if (m_pkv->LoadFromFile(filesystem, file, pathID))
		return true;
	
	m_pkv->deleteThis();
	m_pkv = nullptr;
	return false;
}

int GameData::GetOffset(const char *key)
{
	if (!m_pkv)
		return -1;

	char buffer[512];
	snprintf(buffer, sizeof(buffer), "%s/Offsets/%s/%s", m_Game, key, m_Platform);
	KeyValues *kv = m_pkv->FindKey(buffer);
	if (!kv)
		return -1;

	const char *offset = kv->GetString();
	if (!offset[0])
		return -1;

	return memutil::StrToInt(offset);
}

void *GameData::GetAddress(const char *key)
{
	if (!m_pkv)
		return nullptr;

	char buffer[512];
	snprintf(buffer, sizeof(buffer), "%s/Addresses/%s/%s", m_Game, key, m_Platform);
	KeyValues *kv = m_pkv->FindKey(buffer);
	if (!kv)
		return nullptr;
	
	const char *signKey = kv->GetString("signature");
	if (!signKey[0])
		return nullptr;

	uintptr_t addr = (uintptr_t)GetMemSig(signKey);
	if (!addr)
		return nullptr;

	for (KeyValues *pSub = kv->GetFirstValue(); pSub; pSub = pSub->GetNextValue())
	{
		if (!strcmp(pSub->GetName(), "read"))
		{
			int offset = memutil::StrToInt(pSub->GetString());
			addr = *(uintptr_t*)(addr + offset);
		}
		else if (!strcmp(pSub->GetName(), "offset"))
		{
			int offset = memutil::StrToInt(pSub->GetString());
			addr += offset;
		}
	}

	if (addr < VALID_MINIMUM_MEMORY_ADDRESS)
		return nullptr;

	return (void*)addr;
}


void *GameData::GetMemSig(const char *key)
{
	if (!m_pkv)
		return nullptr;

	char buffer[512];
	snprintf(buffer, sizeof(buffer), "%s/Signatures/%s", m_Game, key);
	KeyValues *kv = m_pkv->FindKey(buffer);
	if (!kv)
		return nullptr;

	const char *library = kv->GetString("library");
	const char *signature = kv->GetString(m_Platform);

	if (!library[0] || !signature[0])
		return nullptr;

	memutil::DynLibInfo libInfo{};
	libInfo.name = library;

	// If no suffix is ​​specified, m_libSuffix is ​​appended.
	if (!strstr(library, ".dll") && !strstr(library, ".so"))
		libInfo.name += m_libSuffix;

	if (!memutil::GetLibInfo(&libInfo))
		return nullptr;
	
	if (signature[0] == '@')
		return memutil::FindAddrFromSymbol(&libInfo, signature+1);
	
	std::vector<uint8_t> bytes = memutil::DecodeHexString(signature);
	if (bytes.size() > 0)
		return memutil::FindAddrFromPattern(&libInfo, bytes, bytes.size());

	return nullptr;
}
