#pragma once

#include <tier1/KeyValues.h>
#include <filesystem.h>

class MemoryPatch;

// This is basically the same as SourcePawn's GameData. 
// The "Addresses" etc. block needs to clearly distinguish between windows and linux data.
class GameData
{
public:
	friend class MemoryPatch;

	// Search from the specified game name key.
	GameData(const char *game);
	~GameData();

	bool LoadFile(IFileSystem *filesystem, const char *file, const char *pathID = nullptr);
	int GetOffset(const char *key);
	void *GetAddress(const char *key);
	void *GetMemSig(const char *key);

private:
	const char *m_Game;
	const char *m_Platform;
	const char *m_libSuffix;
	KeyValues *m_pkv;
};
