// RBRTestPlugin.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "TestPlugin.h"

class IRBRGame;

CTestPlugin* g_pRBRPlugin = nullptr;

BOOL APIENTRY DllMain ( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved )
{
    return TRUE;
}


IPlugin* RBR_CreatePlugin ( IRBRGame* pGame )
{
    if (g_pRBRPlugin == nullptr) g_pRBRPlugin = new CTestPlugin(pGame);
	return g_pRBRPlugin;
}