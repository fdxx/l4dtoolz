#pragma once

#ifdef _WIN32
	#include <windows.h>
	#include <TlHelp32.h>
	#include <memoryapi.h>
	#define _PAGE_EXECUTE_READWRITE (PAGE_EXECUTE_READWRITE)
#else
	#include <link.h>
	#include <sys/mman.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#define _PAGE_EXECUTE_READWRITE (PROT_READ|PROT_WRITE|PROT_EXEC)
#endif

#include <vector>

#include "KeyValues.h"
#include "filesystem.h"

using ByteVector = std::vector<uint8_t>;

// Use KeyValues to parse data.
class GameConfig
{
public:
	~GameConfig();

	bool LoadFile(IBaseFileSystem *filesystem, const char *file, const char *pathID = nullptr);
	int GetOffset(const char *key);
	void* GetAddress(const char *key);
	void* GetMemSig(const char *key);
	KeyValues* GetConfigKv() { return m_pkv; }

private:
	KeyValues *m_pkv = nullptr;
};


class MemoryPatch
{
public:
	~MemoryPatch();

	bool CreateFromConf(GameConfig *gameconf, const char *key);
	bool EnablePatch();
	bool VerifyPatch();
	void DisablePatch();
	bool IsEnabled() { return m_vecRestore.size() > 0; }
	void* GetAddress() { return m_pAddress; }

private:
	void *m_pAddress = nullptr;
	ByteVector m_vecPatch, m_vecRestore, m_vecVerify, m_vecPreserve;
};

struct DynLibInfo
{
	char name[128];
	const char *fullname;
	void *baseAddr;
	size_t size;
};


size_t DecodeHexString(char *buffer, size_t maxlength, const char *hexstr);
void* ResolveSymbol(const DynLibInfo *libInfo, const char *symbol);
void* FindPattern(const DynLibInfo *libInfo, const char *pattern, size_t len);
bool GetLibInfo(DynLibInfo *libInfo);
int ResolveStrToInt(const char *str);
ByteVector ByteVectorFromString(const char *s);
void ByteVectorRead(ByteVector &vec, uint8_t *mem, size_t count);
void ByteVectorWrite(ByteVector &vec, uint8_t *mem);
bool SetMemAccess(void *addr, size_t len, int prot);

static inline void* PointerAdd(void *ptr, int offset)
{
	return reinterpret_cast<char*>(ptr) + offset;
}

template<typename T>
static T ReadAddress(void *addr, int offset)
{
	void *ptr = PointerAdd(addr, offset);
	return *reinterpret_cast<T*>(ptr);
}

template<typename T>
static void WriteAddress(void *addr, int offset, T data, bool updateMemAccess = true)
{
	void *ptr = PointerAdd(addr, offset);
	if (updateMemAccess)
		SetMemAccess(ptr, sizeof(T), _PAGE_EXECUTE_READWRITE);
	*reinterpret_cast<T*>(ptr) = data;
}
