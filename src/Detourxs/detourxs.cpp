#include "detourxs.h"

// 

DetourXS::DetourXS()
{
	m_detourLen = 0;
	m_Created = FALSE;

	// Initialize to get rid of compiler warnings about uninitialized variables (although these will be set to proper values when Create method is called)
	m_lpFuncOrig = nullptr;
	m_lpFuncDetour = nullptr;
	m_lpbFuncOrig = nullptr;
	m_lpbFuncDetour = nullptr;
	m_OrigJmp = Absolute;
	m_TrampJmp = Absolute;
	m_hFirstHookEvent = nullptr;
	m_IgnoreDestroyRestoration = FALSE;
}

DetourXS::DetourXS(LPVOID lpFuncOrig, LPVOID lpFuncDetour, BOOL ignoreDestroyRestoration) : DetourXS()
{
	m_IgnoreDestroyRestoration = ignoreDestroyRestoration;
	Create(lpFuncOrig, lpFuncDetour);
}

DetourXS::~DetourXS()
{
	Destroy();
}

BOOL DetourXS::Create(const LPVOID lpFuncOrig, const LPVOID lpFuncDetour)
{
	DWORD dwProt;

	// Already created, need to Destroy() first
	if (m_Created == TRUE)
	{
		return FALSE;
	}

	// Init
	m_lpFuncOrig = RecurseJumps(lpFuncOrig);
	m_lpFuncDetour = lpFuncDetour;
	m_lpbFuncOrig = reinterpret_cast<LPBYTE>(m_lpFuncOrig);
	m_lpbFuncDetour = reinterpret_cast<LPBYTE>(m_lpFuncDetour);
	m_trampoline.resize(50, 0x00); // data() must not be relocated to determine jmp type

	// Determine which jump is necessary
	m_OrigJmp = GetJmpType(m_lpbFuncOrig, m_lpbFuncDetour);
	m_TrampJmp = GetJmpType(m_trampoline.data(), m_lpbFuncOrig);

	// Determine detour length
	if (m_detourLen == 0 && (m_detourLen = GetDetourLenAuto(m_lpFuncOrig, m_OrigJmp)) == 0)
	{
		return FALSE;
	}

	// Copy orig bytes to trampoline
	std::copy(m_lpbFuncOrig, m_lpbFuncOrig + m_detourLen, m_trampoline.begin());

	// Write a jump to the orig from the tramp
	WriteJump(m_trampoline.data() + m_detourLen, m_lpbFuncOrig + m_detourLen, m_TrampJmp);

	// Trim the tramp
	m_trampoline.resize(m_detourLen + m_TrampJmp);

	// Enable full access for when tramp is executed
	if (VirtualProtect(m_trampoline.data(), m_trampoline.size(), PAGE_EXECUTE_READWRITE, &dwProt) == FALSE) { return FALSE; }

	// Write jump from orig to detour
	if (VirtualProtect(m_lpFuncOrig, m_detourLen, PAGE_EXECUTE_READWRITE, &dwProt) == FALSE) { return FALSE; }
	memset(m_lpFuncOrig, 0x90, m_detourLen);
	WriteJump(m_lpbFuncOrig, m_lpbFuncDetour);
	VirtualProtect(m_lpFuncOrig, m_detourLen, dwProt, &dwProt);

	// Flush cache to make sure CPU doesn't execute old instructions
	FlushInstructionCache(GetCurrentProcess(), m_lpFuncOrig, m_detourLen);

	// Check if this was the first hook to this lpFuncOrig pointer (if the named event with the speficied func pointer already exists then this was NOT the first hook in the original func)
	if (!m_IgnoreDestroyRestoration)
	{
		char szBuffer[32];
		snprintf(szBuffer, sizeof(szBuffer), "_RBR_DetourXSFuncPtr_%08x", (DWORD)lpFuncOrig);
		HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, szBuffer);
		if (hEvent != nullptr && GetLastError() == ERROR_ALREADY_EXISTS)
			CloseHandle(hEvent);			// Not the first hook. Release the handle because the first instance keeps it open until that hook is destroyed
		else
			m_hFirstHookEvent = hEvent;		// The first hook. Keep the handle open to let other RBR plugins hooking the same original func that those were not the first one
	}

	m_Created = TRUE;
	return TRUE;
}

BOOL DetourXS::Destroy()
{
	DWORD dwProt;

	// If this was the first hook in the lpFuncOrig function then restore the original RBR function code. If there are other hooks on top of 
	// the first hook then those won't restore the JMP code because it would not restore the original RBR code (ie. trampoline points to the previous pluing doing a RBR detouring).
	if (m_hFirstHookEvent != nullptr)
	{
		VirtualProtect(m_lpFuncOrig, m_detourLen, PAGE_EXECUTE_READWRITE, &dwProt);
		memcpy(m_lpFuncOrig, m_trampoline.data(), m_detourLen);
		VirtualProtect(m_lpFuncOrig, m_detourLen, dwProt, &dwProt);

		CloseHandle(m_hFirstHookEvent);
		m_hFirstHookEvent = nullptr;
	}

	m_trampoline.clear();
	m_detourLen = 0;
	m_Created = FALSE;
	return TRUE;
}

LPVOID DetourXS::RecurseJumps(LPVOID lpAddr)
{
	LPBYTE lpbAddr = reinterpret_cast<LPBYTE>(lpAddr);

	// Absolute
	if(*lpbAddr == 0xFF && *(lpbAddr + 1) == 0x25)
	{
		LPVOID lpDest = nullptr;

		#ifdef _M_IX86
			lpDest = **reinterpret_cast<LPVOID**>( lpbAddr + 2 );
		#else
		if(*reinterpret_cast<DWORD*>(lpbAddr + 2) != 0)
		{
			lpDest = *reinterpret_cast<LPVOID**>( lpbAddr + *reinterpret_cast<PDWORD>(lpbAddr + 2) + 6 );
		}
		else
		{
			lpDest = *reinterpret_cast<LPVOID**>( lpbAddr + 6 );
		}
		#endif

		return RecurseJumps(lpDest);
	}

	// Relative Near
	else if(*lpbAddr == 0xE9)
	{
		LPVOID lpDest = nullptr;

	#ifdef _M_IX86
		lpDest = reinterpret_cast<LPVOID>( *reinterpret_cast<PDWORD>(lpbAddr + 1) + reinterpret_cast<DWORD>(lpbAddr) + relativeJmpSize );
	#else
		lpDest = reinterpret_cast<LPVOID>( *reinterpret_cast<PDWORD>(lpbAddr + 1) + reinterpret_cast<DWORD>(lpbAddr) + relativeJmpSize 
			+ (reinterpret_cast<DWORD_PTR>(GetModuleHandle(NULL)) & 0xFFFFFFFF00000000) );
	#endif

		return RecurseJumps(lpDest);
	}

	// Relative Short
	else if(*lpbAddr == 0xEB)
	{
		BYTE offset = *(lpbAddr + 1);

		// Jmp forwards
		if(offset > 0x00 && offset <= 0x7F)
		{
			LPVOID lpDest = reinterpret_cast<LPVOID>( lpbAddr + 2 + offset );
			return RecurseJumps(lpDest);
		}

		// Jmp backwards
		else if(offset > 0x80 && offset <= 0xFF) // tbh none should be > FD
		{
			offset = -abs(offset);
			LPVOID lpDest = reinterpret_cast<LPVOID>( lpbAddr + 2 - offset );
			return RecurseJumps(lpDest);
		}
	}

	return lpAddr;
}

size_t DetourXS::GetDetourLenAuto(const LPVOID lpStart, JmpType jmpType)
{
	size_t totalLen = 0;
	LPBYTE lpbDataPos = reinterpret_cast<LPBYTE>(lpStart);

	while(totalLen < jmpType)
	{
		size_t len = LDE(reinterpret_cast<LPVOID>(lpbDataPos), 0);
		lpbDataPos += len;
		totalLen += len;
	}

	return totalLen;
}

void DetourXS::WriteJump(const LPBYTE lpbFrom, const LPBYTE lpbTo)
{
	JmpType jmpType = GetJmpType(lpbFrom, lpbTo);
	WriteJump(lpbFrom, lpbTo, jmpType);
}

void DetourXS::WriteJump(const LPBYTE lpbFrom, const LPBYTE lpbTo, JmpType jmpType)
{
	if(jmpType == Absolute)
	{
		lpbFrom[0] = 0xFF;
		lpbFrom[1] = 0x25;

	#ifdef _M_IX86
		// FF 25 [ptr_to_jmp(4bytes)][jmp(4bytes)]
		*reinterpret_cast<PDWORD>(lpbFrom + 2) = reinterpret_cast<DWORD>(lpbFrom) + 6;
        *reinterpret_cast<PDWORD>(lpbFrom + 6) = reinterpret_cast<DWORD>(lpbTo);
	#else
		// FF 25 [ptr_to_jmp(4bytes)][jmp(8bytes)]
		*reinterpret_cast<PDWORD>(lpbFrom + 2) = 0;
		*reinterpret_cast<PDWORD_PTR>(lpbFrom + 6) = reinterpret_cast<DWORD_PTR>(lpbTo);
	#endif
	}

	else if(jmpType == Relative)
	{
		// E9 [to - from - jmp_size]
		lpbFrom[0] = 0xE9;
		DWORD offset = reinterpret_cast<DWORD>(lpbTo) - reinterpret_cast<DWORD>(lpbFrom) - relativeJmpSize;
		*reinterpret_cast<PDWORD>(lpbFrom + 1) = static_cast<DWORD>(offset); 
	}
}

DetourXS::JmpType DetourXS::GetJmpType(const LPBYTE lpbFrom, const LPBYTE lpbTo)
{
	const DWORD_PTR upper = reinterpret_cast<DWORD_PTR>((std::max)(lpbFrom, lpbTo));
    const DWORD_PTR lower = reinterpret_cast<DWORD_PTR>((std::min)(lpbFrom, lpbTo));

    return (upper - lower > 0x7FFFFFFF) ? Absolute : Relative;
}
