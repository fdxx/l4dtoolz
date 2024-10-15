#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "gamedata.h"

class MemoryPatch
{
public:
	MemoryPatch(GameData *gamedata);
	~MemoryPatch();

	bool CreatePatch(const char *key);
	bool EnablePatch();
	bool VerifyPatch();
	void DisablePatch();
	bool IsEnabled();
	void *GetAddress();
	const char *GeKey();

private:
	GameData *m_GameData;
	void *m_pAddress;
	std::string m_key;
	std::vector<uint8_t> m_vecPatch, m_vecRestore, m_vecVerify, m_vecPreserve;
};


