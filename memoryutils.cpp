#include <stdio.h>
#include <string.h>
#include "memoryutils.h"

#ifdef _WIN32
	static const char *platform = "windows";
	static const char *libSuffix = ".dll";
#else
	static const char *platform = "linux";
	static const char *libSuffix = "_srv.so";
	#define PAGESIZE 4096
	#define SH_LALIGN(x) (void*)((uintptr_t)(x) & ~(PAGESIZE-1))
	#define SH_LALDIF(x) ((uintptr_t)(x) % PAGESIZE)
#endif

static const char *game = "left4dead2";

//memory addresses below 0x10000 are automatically considered invalid for dereferencing
#define VALID_MINIMUM_MEMORY_ADDRESS 0x10000

bool GameConfig::LoadFile(IBaseFileSystem *filesystem, const char *file, const char *pathID)
{
	m_pkv = new KeyValues("");
	if (m_pkv->LoadFromFile(filesystem, file, pathID))
		return true;
	
	m_pkv->deleteThis();
	m_pkv = nullptr;
	return false;
}

GameConfig::~GameConfig()
{
	m_pkv->deleteThis();
}

int GameConfig::GetOffset(const char *key)
{
	if (!m_pkv) return -1;

	char buffer[512];
	snprintf(buffer, sizeof(buffer), "%s/Offsets/%s/%s", game, key, platform);
	KeyValues *pOffsetSubKey = m_pkv->FindKey(buffer);
	if (!pOffsetSubKey)
		return -1;

	const char *sOffset = pOffsetSubKey->GetString();
	if (!sOffset[0])
		return -1;

	return ResolveStrToInt(sOffset);
	//return kv->GetInt(buffer, -1);
}

void* GameConfig::GetAddress(const char *key)
{
	if (!m_pkv) return nullptr;

	char buffer[512];
	snprintf(buffer, sizeof(buffer), "%s/Addresses/%s/%s", game, key, platform);
	KeyValues *pAddrSubKey = m_pkv->FindKey(buffer);
	if (!pAddrSubKey)
		return nullptr;
	
	const char *signKey = pAddrSubKey->GetString("signature");
	if (!signKey[0])
		return nullptr;

	void *addr = GetMemSig(signKey);
	if (!addr)
		return nullptr;

	for (KeyValues *pSub = pAddrSubKey->GetFirstValue(); pSub; pSub = pSub->GetNextValue())
	{
		if (!strcmp(pSub->GetName(), "read"))
		{
			int offset = ResolveStrToInt(pSub->GetString());
			addr = reinterpret_cast<void*>(ReadAddress<uint32_t>(addr, offset));
		}
		else if (!strcmp(pSub->GetName(), "offset"))
		{
			int offset = ResolveStrToInt(pSub->GetString());
			addr = PointerAdd(addr, offset);
		}
	}

	if (reinterpret_cast<uint32_t>(addr) < VALID_MINIMUM_MEMORY_ADDRESS)
		return nullptr;

	return addr;
}

void* GameConfig::GetMemSig(const char *key)
{
	if (!m_pkv) return nullptr;

	char buffer[512];
	snprintf(buffer, sizeof(buffer), "%s/Signatures/%s", game, key);
	KeyValues *pSignSubKey = m_pkv->FindKey(buffer);
	if (!pSignSubKey)
		return nullptr;

	const char *library = pSignSubKey->GetString("library");
	const char *signature = pSignSubKey->GetString(platform);

	if (!library[0] || !signature[0])
		return nullptr;

	DynLibInfo libInfo;
	snprintf(libInfo.name, sizeof(libInfo.name), "%s%s", library, libSuffix);

	if (GetLibInfo(&libInfo))
	{
		if (signature[0] == '@')
			return ResolveSymbol(&libInfo, signature+1);

		size_t byteCount = DecodeHexString(buffer, sizeof(buffer), signature);
		if (byteCount > 0)
			return FindPattern(&libInfo, buffer, byteCount);
	}
	
	return nullptr;
}


// -----------------------------------------------------

bool MemoryPatch::CreateFromConf(GameConfig *gameconf, const char *key)
{
	if (!gameconf || !gameconf->GetConfigKv())
		return false;

	char buffer[512];
	snprintf(buffer, sizeof(buffer), "%s/MemPatches/%s/%s", game, key, platform);
	KeyValues *pMemPatchSubKey = gameconf->GetConfigKv()->FindKey(buffer);
	if (!pMemPatchSubKey)
		return false;

	const char *sOffset = pMemPatchSubKey->GetString("offset");
	const char *signKey = pMemPatchSubKey->GetString("signature");
	const char *addrKey = pMemPatchSubKey->GetString("address");
	const char *verify = pMemPatchSubKey->GetString("verify");
	const char *patch = pMemPatchSubKey->GetString("patch");
	const char *preserve = pMemPatchSubKey->GetString("preserve");

	if (!patch[0])
		return false;

	if (!signKey[0] && !addrKey[0])
		return false;

	void *addr;
	if (signKey[0])
		addr = gameconf->GetMemSig(signKey);
	else
		addr = gameconf->GetAddress(addrKey);

	if (!addr)
		return false;

	int iOffset = 0;
	if (sOffset[0])
		iOffset = ResolveStrToInt(sOffset);

	m_pAddress = PointerAdd(addr, iOffset);
	m_vecPatch = ByteVectorFromString(patch);
	m_vecVerify = ByteVectorFromString(verify);
	m_vecPreserve = ByteVectorFromString(preserve);

	return m_vecPatch.size() > 0;
}

bool MemoryPatch::EnablePatch()
{
	// already patched, disregard
	if (m_vecRestore.size() > 0)
		return false;
	
	if (!VerifyPatch())
		return false;
	
	ByteVectorRead(m_vecRestore, (uint8_t*)m_pAddress, m_vecPatch.size());
	SetMemAccess(m_pAddress, m_vecPatch.size(), _PAGE_EXECUTE_READWRITE);
	ByteVectorWrite(m_vecPatch, (uint8_t*)m_pAddress);

	if (m_vecPreserve.size() == 0)
		return true;
		
	for (size_t i = 0; i < m_vecPatch.size(); i++)
	{
		uint8_t preserveBits = 0;
		if (i < m_vecPreserve.size())
			preserveBits = m_vecPreserve[i];
		*((uint8_t*) m_pAddress + i) = (m_vecPatch[i] & ~preserveBits) | (m_vecRestore[i] & preserveBits);
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
	ByteVectorWrite(m_vecRestore, (uint8_t*)m_pAddress);
	m_vecRestore.clear();
}

MemoryPatch::~MemoryPatch()
{
	DisablePatch();
}

// ---------------------------------------------
#ifndef _WIN32
int LibIterCallback(dl_phdr_info *iter, size_t size, void *vdata)
{
	if (!iter->dlpi_name || !iter->dlpi_name[0])
		return 0;

	DynLibInfo *libInfo = (DynLibInfo*)vdata;
	if (strstr(iter->dlpi_name, libInfo->name) && !strstr(iter->dlpi_name, "metamod"))
	{
		libInfo->fullname = iter->dlpi_name;
		libInfo->baseAddr = (void*)iter->dlpi_addr;
		libInfo->size = iter->dlpi_phnum > 0 ? iter->dlpi_phdr[0].p_filesz : 0; // Is the first program header enough?
		return 1;
	}
	return 0;
}
#endif


bool GetLibInfo(DynLibInfo *libInfo)
{
#ifdef _WIN32
	HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);
	if (hModuleSnap == INVALID_HANDLE_VALUE)
		return false;

	MODULEENTRY32 modent;
	modent.dwSize = sizeof(MODULEENTRY32);
	bool bFound = false;

	while (Module32Next(hModuleSnap, &modent))
	{
		if (strstr(modent.szExePath, libInfo->name) && !strstr(modent.szExePath, "metamod"))
		{
			//libInfo->fullname = modent.szExePath;
			libInfo->baseAddr = (void*)modent.modBaseAddr;
			libInfo->size = modent.modBaseSize;
			bFound = true;
			break;
		}
	}

	CloseHandle(hModuleSnap);
	return bFound;
#else
	return dl_iterate_phdr(LibIterCallback, libInfo) > 0;
#endif
	
}

size_t DecodeHexString(char *buffer, size_t maxlength, const char *hexstr)
{
	size_t written = 0;
	size_t length = strlen(hexstr);

	for (size_t i = 0; i < length; i++)
	{
		if (written >= maxlength)
			break;
		buffer[written++] = hexstr[i];
		if (hexstr[i] == '\\' && hexstr[i + 1] == 'x')
		{
			if (i + 3 >= length)
				continue;
			/* Get the hex part. */
			char s_byte[3];
			int r_byte;
			s_byte[0] = hexstr[i + 2];
			s_byte[1] = hexstr[i + 3];
			s_byte[2] = '\0';
			/* Read it as an integer */
			sscanf(s_byte, "%x", &r_byte);
			/* Save the value */
			buffer[written - 1] = r_byte;
			/* Adjust index */
			i += 3;
		}
	}

	return written;
}

void* ResolveSymbol(const DynLibInfo *libInfo, const char *symbol)
{
#ifdef _WIN32
	return nullptr;
#else

	void *result = nullptr;

	struct stat fileStat;
	int fd = open(libInfo->fullname, O_RDONLY);
	if (fd == -1 || fstat(fd, &fileStat) == -1)
	{
		close(fd);
		return nullptr;
	}

	Elf32_Ehdr *pFileHeader = (Elf32_Ehdr*)mmap(nullptr, fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);

	if (pFileHeader == MAP_FAILED)
		return nullptr;

	if (pFileHeader->e_shoff == 0)
	{
		munmap(pFileHeader, fileStat.st_size);
		return nullptr;
	}

	Elf32_Shdr *pSectionHeader = (Elf32_Shdr*)((char*)pFileHeader + pFileHeader->e_shoff);
	for (Elf32_Half i = 0, sectionCount = pFileHeader->e_shnum; i < sectionCount; i++)
	{
		if (pSectionHeader[i].sh_type != SHT_SYMTAB)
			continue;

		Elf32_Sym *pSymTab = (Elf32_Sym*)((char*)pFileHeader + pSectionHeader[i].sh_offset);
		Elf32_Word symCount = pSectionHeader[i].sh_size / sizeof(Elf32_Sym);
		Elf32_Word strTabIndex = pSectionHeader[i].sh_link;	// Always link to SHT_STRTAB?
		const char *pStrTab = (char*)pFileHeader + pSectionHeader[strTabIndex].sh_offset;
		unsigned char symType;
		Elf32_Word symNameIndex;

		for (Elf32_Word j = 0; j < symCount; j++)
		{
			symType = ELF32_ST_TYPE(pSymTab[j].st_info);
			if (pSymTab[j].st_shndx == SHN_UNDEF || (symType != STT_FUNC && symType != STT_OBJECT))
				continue;

			symNameIndex = pSymTab[j].st_name;
			//if (strcmp(symbol, &pStrTab[symNameIndex]) == 0)
			if (strcmp(symbol, pStrTab + symNameIndex) == 0)
			{
				result = (char*)(libInfo->baseAddr) + pSymTab[j].st_value;
				break;
			}
		}

		break;
	}

	munmap(pFileHeader, fileStat.st_size);
	return result;
#endif
}

void* FindPattern(const DynLibInfo *libInfo, const char *pattern, size_t len)
{
	bool found;
	char *ptr, *end;

	ptr = reinterpret_cast<char *>(libInfo->baseAddr);
	end = ptr + libInfo->size;

	while (ptr < end)
	{
		found = true;
		for (size_t i = 0; i < len; i++)
		{
			if (pattern[i] != '\x2A' && pattern[i] != ptr[i])
			{
				found = false;
				break;
			}
		}

		if (found)
			return ptr;

		ptr++;
	}

	return nullptr;
}

int ResolveStrToInt(const char *str)
{
	if (str[strlen(str)-1] == 'h')
		return strtol(str, nullptr, 16);
	return atoi(str);
}

ByteVector ByteVectorFromString(const char *s)
{
	ByteVector payload;
	
	char* s1 = strdup(s);
	char* p = strtok(s1, "\\x ");
	while (p) {
		uint8_t byte = strtol(p, nullptr, 16);
		payload.push_back(byte);
		p = strtok(nullptr, "\\x ");
	}
	free(s1);
	
	return payload;
}

void ByteVectorRead(ByteVector &vec, uint8_t *mem, size_t count)
{
	vec.clear();
	for (size_t i = 0; i < count; i++)
		vec.push_back(mem[i]);
}

void ByteVectorWrite(ByteVector &vec, uint8_t *mem)
{
	for (size_t i = 0; i < vec.size(); i++)
		mem[i] = vec[i];
}


bool SetMemAccess(void *addr, size_t len, int prot)
{
#ifdef _WIN32
	DWORD tmp;
	return VirtualProtect(addr, len, prot, &tmp);
#else
	return mprotect(SH_LALIGN(addr), len + SH_LALDIF(addr), prot) == 0;
#endif
}

