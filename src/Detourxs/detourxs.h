/*
	DetourXS v2 by Dominic Tobias
	Revision 1 (29/03/2011): Fixed potential bug with relative near recursion in 64bit
	Revision 2 (30/03/2011): Couple of semantic changes, added FlushInstructionCache
	dominictobias.com

	Revision 3 (06/2020) by mika-n: Modifications to support multiple hooks on the same function and restoring only the original hook (Richard Burns Rally plugin specific support)
*/

#ifndef __DETOURXS_H
#define __DETOURXS_H

#include <Windows.h>
#include <vector>
#include <algorithm>

#if !defined(_M_AMD64) && !defined(_M_IX86)
    #error "_M_AMD64 or _M_IX86 must be defined"
#endif

#ifdef _M_IX86
	#pragma comment(lib, "DetourXS/LDE64")
	extern "C" size_t __stdcall LDE(const LPVOID lpData, unsigned int size);
#else
	#pragma comment(lib, "DetourXS/LDE64x64")
	extern "C" size_t __fastcall LDE(const LPVOID lpData, unsigned int size);
#endif

class DetourXS
{
public:
	static const size_t relativeJmpSize = 1 + sizeof(DWORD);
	static const size_t absoluteJmpSize = 2 + sizeof(DWORD) + sizeof(DWORD_PTR);

	enum JmpType : size_t
	{
		Relative = relativeJmpSize,
		Absolute = absoluteJmpSize
	};

public:
	DetourXS();
	DetourXS(LPVOID lpFuncOrig, LPVOID lpFuncDetour, BOOL ignoreDestroyRestoration = FALSE);
	~DetourXS();

	BOOL Create(const LPVOID lpFuncOrig, const LPVOID lpFuncDetour);
	BOOL Destroy();

private:
	LPVOID RecurseJumps(LPVOID lpAddr);
	size_t GetDetourLenAuto(const LPVOID lpStart, JmpType jmpType);
	void WriteJump(const LPBYTE lpbFrom, const LPBYTE lpbTo);
	void WriteJump(const LPBYTE lpbFrom, const LPBYTE lpbTo, JmpType jmpType);
	JmpType GetJmpType(const LPBYTE lpbFrom, const LPBYTE lpbTo);
	LPVOID ChainedTo(LPVOID lpFunc);

public:
	LPVOID GetTrampoline()
	{
		return m_trampoline.empty() ? nullptr : m_trampoline.data();
	}

	size_t GetDetourLen()
	{
		return m_detourLen;
	}

	BOOL Created()
	{
		return m_Created;
	}

	// Use only if you know what you're doing. You are not guaranteed the function address is
	// where the detour will actually take place if you are chaining detours or the function 
	// starts with a jump. Use empty constructor and call before Create.
	void SetDetourLen(size_t len)
	{
		m_detourLen = len;
	}

	BOOL IsFirstHook()
	{
		return m_hFirstHookEvent != nullptr;
	}

private:
	LPVOID m_lpFuncOrig;
	LPVOID m_lpFuncDetour;
	LPBYTE m_lpbFuncOrig;
	LPBYTE m_lpbFuncDetour;
	JmpType m_OrigJmp;
	JmpType m_TrampJmp;
	std::vector<BYTE> m_trampoline;
	size_t m_detourLen;
	BOOL m_Created;

	BOOL   m_IgnoreDestroyRestoration;
	HANDLE m_hFirstHookEvent;  // Event handle to keep track of if the lpFuncOrig was already hooked by another object in any of the other processes/DLL plugins. NULL = Not the first hook. NULL != The first hook.
};

#endif // __DETOURXS_H
