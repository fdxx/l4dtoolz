#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "memutil.h"

namespace memutil {

bool GetLibInfo(DynLibInfo *libInfo)
{
#ifdef _WIN32
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
	if (hSnap == INVALID_HANDLE_VALUE)
		return false;

	MODULEENTRY32 module{};
	module.dwSize = sizeof(MODULEENTRY32);
	bool found = false;

	for (bool it = Module32First(hSnap, &module); it; it = Module32Next(hSnap, &module))
	{
		if (!strstr(module.szExePath, libInfo->name.c_str()) || strstr(module.szExePath, "metamod"))
			continue;
		
		libInfo->pathname = module.szExePath;
		libInfo->baseAddr = (void*)module.modBaseAddr;
		libInfo->size = module.modBaseSize;
		found = true;
		break;
	}

	CloseHandle(hSnap);
	return found;
#else
	int result = dl_iterate_phdr(
		[](dl_phdr_info *dl, size_t size, void *data) -> int {
			if (!dl->dlpi_name || !dl->dlpi_name[0])
				return 0;

			DynLibInfo *libInfo = (DynLibInfo*)data;
			if (!strstr(dl->dlpi_name, libInfo->name.c_str()) || strstr(dl->dlpi_name, "metamod"))
				return 0;

			for (ElfW(Half) i = 0; i < dl->dlpi_phnum; i++)
			{
				const ElfW(Phdr) &phdr = dl->dlpi_phdr[i];
				if (phdr.p_type != PT_LOAD || phdr.p_flags != (PF_X|PF_R))
					continue;

				libInfo->pathname = dl->dlpi_name;
				libInfo->baseAddr = (void*)dl->dlpi_addr;

				// phdr.p_vaddr is the offset of this segment relative to the base address (dl->dlpi_addr)
				// We calculate the size from the base address to the end of this segment.
				libInfo->size = phdr.p_vaddr + phdr.p_memsz;
				return 1;
			}
			return 0;
		},
		libInfo
	);
	return result > 0;

#endif
}

void *FindAddrFromPattern(const DynLibInfo *libInfo, const std::vector<uint8_t> &pattern, size_t len)
{
	bool found;
	uint8_t *ptr = (uint8_t*)libInfo->baseAddr;
	uint8_t *end = ptr + libInfo->size - len;

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

void *FindAddrFromSymbol(const DynLibInfo *libInfo, const char *symbol)
{
#ifdef _WIN32
	return nullptr;
#else

	uintptr_t result = 0;

	int fd = open(libInfo->pathname.c_str(), O_RDONLY);
	if (fd == -1)
		return nullptr;

	struct stat fileStat;
	if (fstat(fd, &fileStat) == -1)
	{
		close(fd);
		return nullptr;
	}

	ElfW(Ehdr) *pEhdr = (ElfW(Ehdr)*)mmap(nullptr, fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);

	if (pEhdr == MAP_FAILED)
		return nullptr;
	
	// Check if the ELF header has a section header table.
	if (pEhdr->e_shoff == 0)
	{
		munmap(pEhdr, fileStat.st_size);
		return nullptr;
	}

	// Get section headers.
	ElfW(Shdr) *pShdr = (ElfW(Shdr)*)(uintptr_t(pEhdr) + pEhdr->e_shoff);

	// Iterate section headers.
	for (ElfW(Half) i = 0, shCount = pEhdr->e_shnum; i < shCount; i++)
	{
		// SHT_SYMTAB should contain SHT_DYNSYM.
		if (pShdr[i].sh_type != SHT_SYMTAB)
			continue;
		
		// Get the symbol table and string table.
		ElfW(Sym) *pSymTab = (ElfW(Sym)*)(uintptr_t(pEhdr) + pShdr[i].sh_offset);
		ElfW(Word) strTabIndex = pShdr[i].sh_link;
		const char *pStrTab = (char*)pEhdr + pShdr[strTabIndex].sh_offset;

		// Iterate symbol tables.
		for (ElfW(Word) j = 0, symCount = (pShdr[i].sh_size / pShdr[i].sh_entsize); j < symCount; j++)
		{
			if (pSymTab[j].st_shndx == SHN_UNDEF)
				continue;

			unsigned char symType = ELF32_ST_TYPE(pSymTab[j].st_info);
			if (symType != STT_FUNC && symType != STT_OBJECT)
				continue;
			
			const char *sym = pStrTab + pSymTab[j].st_name;
			if (strcmp(symbol, sym) == 0)
			{
				result = uintptr_t(libInfo->baseAddr) + pSymTab[j].st_value;
				break;
			}
		}

		// Are there really multiple SHT_SYMTAB?
		if (result)
			break;
	}

	munmap(pEhdr, fileStat.st_size);
	return (void*)result;
#endif
}

std::vector<uint8_t> DecodeHexString(const char *hexstr)
{
	std::vector<uint8_t> result{};
	
	char *str = strdup(hexstr);
	char *token = strtok(str, "\\x");
	while (token)
	{
		uint8_t byte = (uint8_t)strtol(token, nullptr, 16);
		result.push_back(byte);
		token = strtok(nullptr, "\\x");
	}
	free(str);
	
	return result;
}

int StrToInt(const char *str)
{
	int base = 0;
	if (str[strlen(str)-1] == 'h')
		base = 16;
	return strtol(str, nullptr, base);
}


bool SetMemAccess(void *addr, size_t length, int prot)
{
#ifdef _WIN32
	DWORD tmp;
	return VirtualProtect(addr, length, prot, &tmp);
#else
	static long pageSize = 0;
	if (!pageSize)
		pageSize = sysconf(_SC_PAGESIZE);
	
	uintptr_t start = AlignedBase(uintptr_t(addr), pageSize);
	uintptr_t end = AlignedBase(uintptr_t(addr) + length, pageSize);
	size_t len = end - start + pageSize;

	return mprotect((void*)start, len, prot) == 0;
#endif
}

} // namespace memutil
