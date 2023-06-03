#pragma once

#ifdef _WIN32
	#include <windows.h>
	#include <TlHelp32.h>
	#include <memoryapi.h>
	#define _PAGE_EXECUTE_READWRITE (PAGE_EXECUTE_READWRITE)
#else
	#include <link.h> // dl_iterate_phdr
	#include <sys/mman.h> // mprotect
	#define _PAGE_EXECUTE_READWRITE (PROT_READ|PROT_WRITE|PROT_EXEC)
#endif

#include <vector>

#include "KeyValues.h"
#include "filesystem.h"

using ByteVector = std::vector<uint8_t>;

// Use KeyValues to parse data, which is different from SM's gamedata format.
class GameConfig
{
public:
	~GameConfig();

	bool LoadFile(IBaseFileSystem *filesystem, const char *file, const char *pathID = NULL);
	int GetOffset(const char *key);
	void* GetAddress(const char *key);
	void* GetMemSig(const char *key);

	KeyValues *kv = NULL;
};


// Copy from https://github.com/nosoop/SMExt-SourceScramble
class MemoryPatch
{
public:
	~MemoryPatch();

	bool CreateFromConf(GameConfig *gameconf, const char *key);
	bool EnablePatch();
	bool VerifyPatch();
	void DisablePatch();
	bool IsEnabled() {
		return vecRestore.size() > 0;
	}
	
	void *pAddress = NULL;
	ByteVector vecPatch, vecRestore, vecVerify, vecPreserve;
};

struct DynLibInfo
{
	char name[128];
	//const char *fullname;
	void *baseAddr;
	size_t size;
};


size_t DecodeHexString(char *buffer, size_t maxlength, const char *hexstr);
void* FindPattern(DynLibInfo *libInfo, const char *pattern, size_t len);
bool GetLibInfo(DynLibInfo *libInfo);
int ResolveStrToInt(const char *str);
ByteVector ByteVectorFromString(const char *s);
void ByteVectorRead(ByteVector &vec, uint8_t *mem, size_t count);
void ByteVectorWrite(ByteVector &vec, uint8_t *mem);
bool SetMemAccess(void *addr, size_t len, int prot);

static inline void* PointerAdd(void *ptr, int offset)
{
	return reinterpret_cast<void*>(reinterpret_cast<char*>(ptr) + offset);
}

template<typename T>
static T ReadAddress(void *addr, int offset)
{
	void *ptr = PointerAdd(addr, offset);
	return *reinterpret_cast<T*>(ptr);
}

template <typename T>
static void WriteAddress(void *addr, int offset, T data, bool updateMemAccess = true)
{
	void *ptr = PointerAdd(addr, offset);
	if (updateMemAccess)
		SetMemAccess(ptr, sizeof(T), _PAGE_EXECUTE_READWRITE);
	*reinterpret_cast<T*>(ptr) = data;
}
