//
// NGPCarMenu.cpp : Defines the entry point for the DLL application.
//
// Copyright 2020, MIKA-N. https://github.com/mika-n
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this softwareand associated
// documentation files(the "Software"), to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions :
// 
// - The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// - The derived work or derived parts are also "open sourced" and the source code or part of the work using components
//   from this project is made publicly available with modifications to this base work.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS
// OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// Note. RBR_RX/BTB plugin in RBR was created by black f./jharron. We all should thank him for making it possible in RBR.
//

#if 0

#include "stdafx.h"

#include <filesystem>			// fs::directory_iterator
#include <fstream>				// std::ifstream
#include <sstream>				// std::stringstream
#include <algorithm>			// std::clamp

#include <locale>				// UTF8 locales

#include "NGPCarMenu.h"

#include "SQLite/CppSQLite3U.h"

namespace fs = std::filesystem;


//-------------------------------------------------------------------------------------------------------------------
// Initialize race stat DB if it is missing
//
bool CNGPCarMenu::RaceStatDB_Initialize()
{
	// RaceStatDB feature disabled if the db filePath is empty
	if (m_raceStatDBFilePath.empty())
		return FALSE;

	// Is raceStatDB already initialized and connected?
	if (g_pRaceStatDB != nullptr)
		return TRUE;

	bool bResult = TRUE;

	try
	{
		g_pRaceStatDB = new CppSQLite3DB();
		LogPrint("RaceStatDB %s (SQLite %s)", m_raceStatDBFilePath.c_str(), g_pRaceStatDB->SQLiteVersion());

		if (!fs::exists(m_raceStatDBFilePath))
			LogPrint("InitializeRaceStatDB. Creating new race statistics database file");

		g_pRaceStatDB->open(m_raceStatDBFilePath.c_str());
		if (!g_pRaceStatDB->tableExists("StatInfo"))
		{
			LogPrint("InitializeRaceStatDB. Creating StatInfo");

			bResult = g_pRaceStatDB->execDML("CREATE TABLE StatInfo(AttribName char(16) PRIMARY KEY not null, AttribValue char(64) not null);") == SQLITE_OK;
			if (bResult) bResult = g_pRaceStatDB->execDML("INSERT INTO StatInfo VALUES ('Version', '1');") == SQLITE_OK;
		}

		// TODO. Read the Version attribute from StatInfo table and check if the table structure needs DDL changes
		// TODO. Create D_Car and D_Map dim tables

		if (bResult && !g_pRaceStatDB->tableExists("F_RallyResult"))
		{
			LogPrint("InitializeRaceStatDB. Creating F_RallyResult");

			bResult = g_pRaceStatDB->execDML("CREATE TABLE F_RallyResult("
				"RaceKey integer PRIMARY KEY not null,"	// Unique raceID "surrogate" key
				"RaceDate int not null,"			// YYYYMMDD
				"RaceDateTime int not null,"		// HHMISS (24h, local timestamp)
				"CarKey integer not null,"				// Key to D_Car tbl
				"MapKey integer not null,"				// Key to D_Map tbl
				"SplitTime1 real,"
				"SplitTime2 real,"					// Split1/Split2/Finish time in secs (if finish time is NULL then the race was retired)
				"FinishTime real,"
				"TotalDuration real not null,"		// Total time between "OnRaceStarted and OnRaceEnded" (including the time spent in pause state, this is NOT the same as finish time in racing)
				"FalseStart int not null,"			// 0=No false start, 1=Yes false start (false start penalty added to the race time)
				"CallForHelp int not null,"			// Number of "call for help" calls (always 0 at the moment, not yet implemented)
				"TyreType char(1) not null,"		// G=Gravel, T=Tarmac, S=Snow, U=Unknown
				"TyreSubType char(1) not null,"		// D=Dry, I=Intermediate, W=Wet (snow tracks have only D)
				"WeatherType char(1) not null,"		// G=Good, B=Bad, U=Unknown
				"ProfileName char(16) not null,"	// Name of the profile
				"PluginType char(4) not null,"		// Plugin or RBR game mode used in this race (RBR, TM, RX, RSF, UNK=Unknown)
				"PluginSubType char(4) not null,"	// RBR: QR, MP | TM: SHA, OFF, ONL | RX: SHA | RSF: SHA, OFF, ONL | UNK=Unknown
				"CarSlot int not null,"				// Car slot#
				"NGPVersion int not null,"			// NGP physics version (3xxx, 4xxx, 5xxx or 6xxx where x is the minor version(example NGP 6.3.758 -> 63758)
				"SetupFile char(64),"				// Name of the setup file (empty str at the moment, not yet implemented)
				"DamagesOnStart char(64),"			// Damages on the start time (not yet implemented)
				"DamagesOnFinish char(64)"			// Damages on the finish line (not yet implemented)
				");") == SQLITE_OK;
		}

		if (bResult && !g_pRaceStatDB->tableExists("D_Car"))
		{
			LogPrint("InitializeRaceStatDB. Creating D_Car");

			bResult = g_pRaceStatDB->execDML("CREATE TABLE D_Car("
				"CarKey integer PRIMARY KEY not null,"	// Unique carID "surrogate" key
				"ModelName char(128) not null,"
				"FIACategory char(128) not null,"
				"Physics char(128) not null,"
				"IniFile char(128) not null,"
				"Folder char(128) not null,"
				"NGPRevision char(128) not null,"
				"NGPVersion int,"
				"NGPCarID int"
				");") == SQLITE_OK;
		}

		if (bResult && !g_pRaceStatDB->tableExists("D_Map"))
		{
			LogPrint("InitializeRaceStatDB. Creating D_Map");

			bResult = g_pRaceStatDB->execDML("CREATE TABLE D_Map("
				"MapKey integer PRIMARY KEY not null,"	// Unique mapID "surrogate" key
				"StageName char(128) not null,"
				"Surface char(1) not null,"			// G=Gravel, T=Tarmac, S=Snow, U=Unknown
				"Length int not null,"				// Length in meters
				"Format char(3) not null,"			// RBR (=classic), BTB (=RBRRX/BTB)
				"PluginType char(4) not null,"		// Plugin type defining the map (TM, RSF, UNK=Unknown)
				"MapID int not null,"				// TM, RSF, RBR mapID (for BTB this is always -1) 
				"Folder char(128)"					// RBR=TrackName option from Tracks.ini, BTB=tracks\xxxx folder name
				");") == SQLITE_OK;
		}
	}
	catch (CppSQLite3Exception ex)
	{
		bResult = FALSE;
		LogPrint("ERROR. %s", ex.errorMessage());
	}
	catch (...)
	{
		bResult = FALSE;
	}

	if (bResult == FALSE)
	{
		LogPrint("ERROR. Error while opening the race stat db");
		m_raceStatDBFilePath.clear();
		SAFE_DELETE(g_pRaceStatDB);
	}

	return bResult;
}


//-------------------------------------------------------------------------------------------------------------------
// Prepare a query, bind parameters and return the qry dataset
//
CppSQLite3Query CNGPCarMenu::RaceStatDB_ExecQuery(LPCSTR szQuery, LPCSTR param1, LPCSTR param2, LPCSTR param3, LPCSTR param4)
{
	CppSQLite3Query qryData;

	CppSQLite3Statement qryStmt = g_pRaceStatDB->compileStatement(szQuery);
	if (param1) qryStmt.bind(1, param1);
	if (param2) qryStmt.bind(2, param2);
	if (param3) qryStmt.bind(3, param3);
	if (param4) qryStmt.bind(4, param4);
	qryData = qryStmt.execQuery();
	return qryData;
}


//-------------------------------------------------------------------------------------------------------------------
// Return the D_Map MapKey or add a new record to D_Map table if the map is missing
//
int CNGPCarMenu::RaceStatDB_GetMapKey(int mapID, const std::string& mapName)
{
	int iResult = -1;
	std::string sQuery;

	if (g_pRaceStatDB == nullptr || mapName.empty()) 
		return -1;

	try
	{
		CppSQLite3Query qryData = RaceStatDB_ExecQuery("SELECT MapKey FROM D_Map where Format=? AND PluginType=? AND StageName=?", (mapID < 0 ? "BTB" : "RBR"), GetRBRInstallType().c_str(), mapName.c_str());
		if(!qryData.eof() && qryData.numFields() >= 1)
			iResult = atoi(qryData.fieldValue(0));
		else
		{
			DebugPrint("RaceStatDB_GetMapKey. Not found %d %s", mapID, mapName.c_str());
			// TODO. Add new map record. Where to get all attributes?
			// Format mapID <0 BTB, otherwise RBR
			// PluginType GetRBRInstallType TM or RSF or UNK
			// Surface?  RBR=From Tracks.ini.Surface   BTB=tracks\xxx\track.ini.Physics
			// Length? RBR=From Tracks.ini.Length      BTB=tracks\xxx\track.ini.Length
			// MapID = mapID
			// Folder?  BTB folder, RBR Tracks.ini.TrackName
		}			
	}
	catch (CppSQLite3Exception ex)
	{
		iResult = -1;
		LogPrint("ERROR. RaceStatDB_GetMapKey. %s", ex.errorMessage());
	}
	catch (...)
	{
		iResult = -1;
		LogPrint("ERROR. RaceStatDB_GetMapKey");
	}

	return iResult;
}

#endif
