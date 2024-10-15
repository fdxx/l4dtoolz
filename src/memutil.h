#pragma once

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <TlHelp32.h>
#include <memoryapi.h>
#define _PAGE_EXECUTE_READWRITE (PAGE_EXECUTE_READWRITE)
#else
#include <sys/mman.h>
#include <link.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#define _PAGE_EXECUTE_READWRITE (PROT_READ|PROT_WRITE|PROT_EXEC)
#endif

// memory addresses below 0x10000 are automatically considered invalid for dereferencing
#define VALID_MINIMUM_MEMORY_ADDRESS 0x10000


namespace memutil {

struct DynLibInfo
{
	std::string name;
	std::string pathname;	// Full name with path.
	void *baseAddr;				
	size_t size;			
};

// Get libInfo from DynLibInfo.name
bool GetLibInfo(DynLibInfo *libInfo);

// Decode the hexadecimal string into corresponding byte data.
std::vector<uint8_t> DecodeHexString(const char *hexstr);

// Find the address from the byte array.
// Or find the address from the symbol (Linux only).
void *FindAddrFromPattern(const DynLibInfo *libInfo, const std::vector<uint8_t> &pattern, size_t len);
void *FindAddrFromSymbol(const DynLibInfo *libInfo, const char *symbol);

// Supports '0x' prefix or 'h' suffix to represent hexadecimal.
int StrToInt(const char *str);

bool SetMemAccess(void *addr, size_t len, int prot);

template<typename T>
inline T AlignedBase(T addr, size_t alignment)
{
	return (T)(uintptr_t(addr) & ~(alignment - 1));
}

template<typename T>
inline T ReadAddress(void *addr, int offset)
{
	uintptr_t ptr = uintptr_t(addr) + offset;
	return *(T*)ptr;
}

template<typename T>
inline void WriteAddress(void *addr, int offset, T data, bool updateMemAccess = true)
{
	uintptr_t ptr = uintptr_t(addr) + offset;
	if (updateMemAccess)
		SetMemAccess((void*)ptr, sizeof(T), _PAGE_EXECUTE_READWRITE);
	*(T*)ptr = data;
}


} // namespace memutil


