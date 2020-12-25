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

#include "stdafx.h"

#include <filesystem>			// fs::directory_iterator
#include <fstream>				// std::ifstream
#include <sstream>				// std::stringstream
#include <algorithm>			// std::clamp
#include <locale>				// UTF8 locales

#include "NGPCarMenu.h"
#include "PluginHelpers.h"

#include "SQLite/CppSQLite3U.h"

#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"

namespace fs = std::filesystem;

#define C_RACESTATDB_SCHEMA_VERSION 2

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
			bResult = g_pRaceStatDB->execDML("CREATE TABLE StatInfo(AttribName char(16) PRIMARY KEY not null, AttribValue char(64) not null)") == SQLITE_OK;
			if (bResult) bResult = g_pRaceStatDB->execDML((std::string("INSERT INTO StatInfo VALUES ('Version', '") + std::to_string(C_RACESTATDB_SCHEMA_VERSION) + "')").c_str()) == SQLITE_OK;
		}

		// Insert statDB instance identifier if the row is missing
		g_pRaceStatDB->execDML((std::string("INSERT INTO StatInfo (AttribName, AttribValue) SELECT 'Instance', '") + GetGUIDAsString() + "' WHERE NOT EXISTS (SELECT AttribName FROM StatInfo WHERE AttribName = 'Instance')").c_str());

		RaceStatDB_UpgradeSchema();

		if (bResult && !g_pRaceStatDB->tableExists("ValueText"))
		{
			LogPrint("InitializeRaceStatDB. Creating ValueText");
			bResult = g_pRaceStatDB->execDML("CREATE TABLE ValueText(Category char(24) not null, Lang char(2) not null, ValueCode char(8) not null, ValueText char(32));") == SQLITE_OK;
		}

		if (bResult && !g_pRaceStatDB->tableExists("F_RallyResult"))
		{
			LogPrint("InitializeRaceStatDB. Creating F_RallyResult");

			bResult = g_pRaceStatDB->execDML("CREATE TABLE F_RallyResult("
				"RaceKey integer PRIMARY KEY not null,"	// Unique raceID "surrogate" key
				"RaceDate integer not null,"			// YYYYMMDD
				"RaceDateTime char(6) not null,"		// HHMISS (24h, local timestamp)
				"CarKey integer not null,"				// Key to D_Car tbl
				"MapKey integer not null,"				// Key to D_Map tbl

				"Split1Time real,"
				"Split2Time real,"					// Split1/Split2/Finish time in secs (if finish time is NULL then the race was retired)
				"FinishTime real,"					

				"FalseStartPenaltyTime real not null," // FalseStart penalty time (m_latestFalseStartPenaltyTime)
				"OtherPenaltyTime real not null,"	// Total penalty time from false start, callForHelps and cut penalties (this time is part of the finishTime already) (m_latestOtherPenaltyTime)

				"FalseStart integer not null,"		// 0=No false start, 1=Yes false start (PRBRCarInfo.falseStart)
				"CallForHelp integer not null,"		// Number of "call for help" calls (always 0 at the moment, not yet implemented. CallForHelp time penalties are included in the FinishTime and OtherPenaltyTime values)

				"TransmissionType char(1) not null,"// M=Manual, A=Automatic, U=Unknown (RBRMapSettings.transmissionType)
				"TyreType char(1) not null,"		// G=Gravel, T=Tarmac, S=Snow, U=Unknown (RBRMapSettings.tyreType)
				"TyreSubType char(1) not null,"		// D=Dry, I=Intermediate, W=Wet, S=Snow, U=Unknown (RBRMapSettings.tyreType. Snow tyre has only S subtype)
				"DamageType integer not null,"		// 0=No damage, 1=Safe, 2=Reduced, 3=Realistic (RBRMapSettings.damageType)

				"TimeOfDay integer not null,"		// 0=Morning, 1=Noon, 2=Evening (RBRMapSettingsEx.timeOfDay)
				"WeatherType char(1) not null,"		// G=Good, B=Bad, U=Unknown (RBRMapSettings.weatherType)
				"SkyCloudType integer not null,"    // 0=Clear, 1=PartCloud, 2=LightCloud, 3=HeavyCloud (RBRMapSettingsEx.skyCloudType)
				"SkyType integer not null,"			// 0=Crisp, 1=Hazy, 2=NoRain, 3=LightRain, 4=HeavyRain, 5=NoSnow, 6=LightSnow, 7=HeavySnow, 8=LightFog, 9=HeavyFog (RBRMapSettingsEx.skyType)
				"SurfaceWetness integer not null,"  // 0=Dry, 1=Damp, 2=Wet (RBRMapSettingsEx.surfaceWetness)
				"SurfaceAge integer not null,"		// 0=New, 1=Normal, 2=Worn (RBRMapSettingsEx.surfaceAge)

				"ProfileName char(20) not null,"	// Name of the profile
				"PluginType char(4) not null,"		// Plugin or RBR game mode used in this race (RBR, TM, RX, RSF, UNK=Unknown)
				"PluginSubType char(4) not null,"	// RBR: QR, MP | TM: SHA, OFF, ONL | RX: SHA | RSF: SHA, OFF, ONL | UNK=Unknown
				"CarSlot integer not null"			// Car slot# 0..7
				//"SetupFile char(64),"				// Name of the setup file (empty str at the moment, not yet implemented)
				//"DamagesOnStart char(64),"			// Damages on the start line (not yet implemented)
				//"DamagesOnFinish char(64)"			// Damages on the finish line (not yet implemented)
				");") == SQLITE_OK;

			g_pRaceStatDB->execDML("CREATE INDEX idx_F_RallyResult_CarMap ON F_RallyResult(CarKey, MapKey)");
		}

		if (bResult && !g_pRaceStatDB->tableExists("D_Car"))
		{
			LogPrint("InitializeRaceStatDB. Creating D_Car");

			bResult = g_pRaceStatDB->execDML("CREATE TABLE D_Car("
				"CarKey integer PRIMARY KEY not null,"	// Unique carID "surrogate" key
				"ModelName char(128) not null,"
				"FIACategory char(128) not null,"		// FIA category or "UNK=Unknown"
				"Physics char(128) not null,"
				"Folder char(128) not null,"
				"Revision char(128) not null,"			// Revision of the car physics or "UNK=unknown"
				"NGPVersion integer not null"			// NGP version or 0 if vanilla physics/NGP not used
				");") == SQLITE_OK;
		}

		if (bResult && !g_pRaceStatDB->tableExists("D_Map"))
		{
			LogPrint("InitializeRaceStatDB. Creating D_Map");

			bResult = g_pRaceStatDB->execDML("CREATE TABLE D_Map("
				"MapKey integer PRIMARY KEY not null,"	// Unique mapID "surrogate" key
				"MapID integer not null,"				// TM, RSF, RBR mapID (for BTB this is always -1) 
				"StageName char(128) not null,"
				"Surface char(1) not null,"			// G=Gravel, T=Tarmac, S=Snow, U=Unknown
				"Length integer not null,"			// Length in meters
				"Format char(3) not null,"			// RBR (=classic), BTB (=RBRRX/BTB)
				"RBRInstallType char(4) not null"	// The type of RBR installation (TM, RSF, RBR)
				");") == SQLITE_OK;
		}

		if (bResult && !g_pRaceStatDB->tableExists("V_RallyResultSummary"))
		{
			LogPrint("InitializeRaceStatDB. Creating V_RallyResultSummary view");

			bResult = g_pRaceStatDB->execDML(
				"CREATE VIEW IF NOT EXISTS V_RallyResultSummary (RaceDate, RaceDateTime, StageName, StageFormat, StageLength, CarModel, FIACat, TotalPenaltyTime, FinishTime, StageRecord, StageDelta, CarStageRecord, CarStageDelta, FIACatStageRecord, FIACatStageDelta)"
				" AS SELECT FRR.RaceDate, FRR.RaceDateTime, M.StageName, M.Format as StageFormat, M.Length as StageLength, C.ModelName as CarModel, C.FIACategory as FIACat"
				",(FRR.FalseStartPenaltyTime + FRR.OtherPenaltyTime) as TotalPenaltyTime"
				",FRR.FinishTime"
				",MapBest.FinishTime as StageRecord"
				",round(FRR.FinishTime - MapBest.FinishTime, 1) as StageDelta"
				",CarBest.FinishTime as CarStageRecord"
				",round(FRR.FinishTime - CarBest.FinishTime, 1) as CarStageDelta"
				",FIACatBest.FinishTime as FIACatStageRecord"
				",round(FRR.FinishTime - FIACatBest.FinishTime, 1) as FIACatStageDelta"
				" FROM F_RallyResult FRR"
				",D_Map M"
				",D_Car C"
				",(SELECT MapKey, MIN(FinishTime) as FinishTime FROM F_RallyResult WHERE FinishTime IS NOT NULL GROUP BY MapKey) MapBest"
				",(SELECT MapKey, CarKey, MIN(FinishTime) as FinishTime FROM F_RallyResult WHERE FinishTime IS NOT NULL GROUP BY MapKey, CarKey) CarBest"
				",(SELECT F.MapKey, C.FIACategory, MIN(F.FinishTime) as FinishTime FROM F_RallyResult F, D_Car C WHERE F.FinishTime IS NOT NULL AND F.CarKey = C.CarKey GROUP BY F.MapKey, C.FIACategory) FIACatBest"
				" WHERE FRR.CarKey = C.CarKey"
				" AND FRR.MapKey = M.MapKey"
				" AND FRR.FinishTime IS NOT NULL"
				" AND FRR.MapKey = MapBest.MapKey"
				" AND FRR.MapKey = CarBest.MapKey AND FRR.CarKey = CarBest.CarKey"
				" AND FRR.MapKey = FIACatBest.MapKey AND C.FIACategory = FIACatBest.FIACategory"
				" ORDER BY FRR.RaceKey DESC") == SQLITE_OK;
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

int CNGPCarMenu::RaceStatDB_GetVersion()
{
	CppSQLite3Statement qryStmt = g_pRaceStatDB->compileStatement("SELECT AttribValue FROM StatInfo WHERE AttribName='Version'");
	CppSQLite3Query qryData = qryStmt.execQuery();
	if (!qryData.eof() && qryData.numFields() >= 1)
		return std::stoi(qryData.getStringField(0, "0"));
	else
		return 0;

}

bool CNGPCarMenu::RaceStatDB_UpgradeSchema()
{
	int currentDBVersion = RaceStatDB_GetVersion();
	if (currentDBVersion < C_RACESTATDB_SCHEMA_VERSION)
	{
		LogPrint("RaceStatDB_UpgradeSchema. OldVersion=%d  NewVersion=%d", currentDBVersion, C_RACESTATDB_SCHEMA_VERSION);

		//Schema updates:
		// Version 1 -> 2: F_RallyResultSummary view updated. Drop the old view to force it to be re-created
		if(currentDBVersion <= 1)
			g_pRaceStatDB->execDML("drop view if exists V_RallyResultSummary");

		// Update version tag
		g_pRaceStatDB->execDML((std::string("UPDATE StatInfo SET AttribValue='") + std::to_string(C_RACESTATDB_SCHEMA_VERSION) + "' WHERE AttribName='Version'").c_str());
		return true;
	}

	// No need to upgrade
	return false;
}

//-------------------------------------------------------------------------------------------------------------------
// Prepare a query, bind parameters and return the qry dataset
//
int queryGetIntValue(LPCSTR szQuery, LPCSTR param1 = nullptr, LPCSTR param2 = nullptr, LPCSTR param3 = nullptr, LPCSTR param4 = nullptr)
{	
	CppSQLite3Statement qryStmt = g_pRaceStatDB->compileStatement(szQuery);
	if (param1) qryStmt.bind(1, param1);
	if (param2) qryStmt.bind(2, param2);
	if (param3) qryStmt.bind(3, param3);
	if (param4) qryStmt.bind(4, param4);
	CppSQLite3Query qryData = qryStmt.execQuery();

	if (!qryData.eof() && qryData.numFields() >= 1)
		return qryData.getIntField(0, -1);
	else
		return -1;
}

int queryGetIntValue(LPCSTR szQuery, LPCSTR param1 = nullptr, int param2 = -9999)
{	
	CppSQLite3Statement qryStmt = g_pRaceStatDB->compileStatement(szQuery);
	if (param1) qryStmt.bind(1, param1);
	if (param2 != -9999) qryStmt.bind(2, param2);
	CppSQLite3Query qryData = qryStmt.execQuery();

	if (!qryData.eof() && qryData.numFields() >= 1)
		return qryData.getIntField(0, -1);
	else
		return -1;
}

int queryGetIntValue(LPCSTR szQuery, LPCSTR param1 = nullptr, LPCSTR param2 = nullptr, int param3 = -9999)
{
	CppSQLite3Statement qryStmt = g_pRaceStatDB->compileStatement(szQuery);
	if (param1) qryStmt.bind(1, param1);
	if (param2) qryStmt.bind(2, param2);
	if (param3 != -9999) qryStmt.bind(3, param3);
	CppSQLite3Query qryData = qryStmt.execQuery();

	if (!qryData.eof() && qryData.numFields() >= 1)
		return qryData.getIntField(0, -1);
	else
		return -1;
}

int RaceStatDB_ExecDML(LPCSTR szQuery, LPCSTR param1, LPCSTR param2, LPCSTR param3, LPCSTR param4)
{
	CppSQLite3Statement qryStmt = g_pRaceStatDB->compileStatement(szQuery);
	if (param1) qryStmt.bind(1, param1);
	if (param2) qryStmt.bind(2, param2);
	if (param3) qryStmt.bind(3, param3);
	if (param4) qryStmt.bind(4, param4);
	return qryStmt.execDML();
}



//-------------------------------------------------------------------------------------------------------------------
// Return the D_Map MapKey or add a new record to D_Map table if the map is missing
//
int CNGPCarMenu::RaceStatDB_GetMapKey(int mapID, const std::string& mapName, int racingType)
{
	int iResult = -1;

	if (g_pRaceStatDB == nullptr || mapName.empty()) 
		return -1;

	try
	{
		CppSQLite3Query qryData;

		// If the map is #41 (Cote) but this is RBRRX/BTB racing then use -1 mapID because RBRRX doesn't have real BTB track specific ID values
		if (racingType == 2 && mapID == 41)
			mapID = -1;

		if(mapID < 0)
			iResult = queryGetIntValue("SELECT MapKey FROM D_Map WHERE Format='BTB' AND RBRInstallType=? AND StageName=?", GetRBRInstallType().c_str(), mapName.c_str(), nullptr, nullptr);
		else if (GetRBRInstallType() == "RSF" && racingType == 2)
			iResult = queryGetIntValue("SELECT MapKey FROM D_Map WHERE Format='BTB' AND RBRInstallType=? AND MapID=?", GetRBRInstallType().c_str(), mapID);
		else
			iResult = queryGetIntValue("SELECT MapKey FROM D_Map WHERE Format='RBR' AND RBRInstallType=? AND MapID=?", GetRBRInstallType().c_str(), mapID);

		if (iResult < 0) 
			iResult = RaceStatDB_AddMap(mapID, mapName, racingType);
	}
	catch (CppSQLite3Exception ex)
	{
		iResult = -1;
		LogPrint("ERROR. RaceStatDB_GetMapKey. %s", ex.errorMessage());
	}
	catch (...)
	{
		iResult = -1;
		LogPrint("ERROR. RaceStatDB_GetMapKey exception");
	}

	return iResult;
}


//-------------------------------------------------------------------------------------------------------------------
// Add the current map to D_Map table
//
int CNGPCarMenu::RaceStatDB_AddMap(int mapID, const std::string& mapName, int racingType)
{
	static char surfaceArrayRBR[3] = { 'T', 'G', 'S' }; // RBR/TM (Tracks.ini): 0=Tarmac, 1=Gravel, 2=Snow | RSF (stage_data.json): 1=Tarmac, 2=Gravel, 3=Snow

	std::string sQuery;

	std::string mapSurface = "U";
	int  mapLength = 0;

	if (g_pRaceStatDB == nullptr || mapName.empty())
		return -1;

	//
	// mapName
	// Surface: RSF=stages_data.json.Surface, RBR=Tracks.ini.Surface, BTB=tracks\xxx\track.ini.Physics
	// Length:  RSF=stages_data.json.Length, RBR=Tracks.ini.Length, BTB=tracks\xxx\track.ini.Length
	// Format:  BTB or RBR track format (racingType==2 is BTB)
	// PluginType: GetRBRInstallType() (TM, RSF or UNK)
	// mapID
	//
	if (mapID >= 0)
	{
		// The map is in RBR format or BTB track managed by RSF (RX racing type and mapid is not 41)
		int  mapSurfaceID = -1;

		if (GetRBRInstallType() == "RSF")
		{
			// RSF rbr installation
			std::ifstream jsonFile(m_sRBRRootDir + "\\rsfdata\\cache\\stages_data.json");
			rapidjson::Document mapJsonData;
			rapidjson::IStreamWrapper isw(jsonFile);
			mapJsonData.ParseStream(isw);

			if (mapJsonData.HasParseError() || !mapJsonData.IsArray())
			{
				LogPrint("ERROR. RaceStatDB_AddMap. Failed to parse RSF stages_data.json JSON file for mapID %d", mapID);
				return -1;
			}

			for (rapidjson::Value::ConstValueIterator itr = mapJsonData.Begin(); itr != mapJsonData.End(); ++itr)
			{
				if ((*itr).HasMember("id") && atoi((*itr)["id"].GetString()) == mapID)
				{
					if((*itr).HasMember("length"))     mapLength = atoi((*itr)["length"].GetString());
					if((*itr).HasMember("surface_id")) mapSurfaceID = atoi((*itr)["surface_id"].GetString()) - 1;
					break;
				}
			}
		}
		else
		{
			// TM or vanilla rbr installation
			WCHAR wszMapINISection[16];
			swprintf_s(wszMapINISection, COUNT_OF_ITEMS(wszMapINISection), L"Map%02d", mapID);
			mapLength = static_cast<int>(m_pTracksIniFile->GetValueExFloat(wszMapINISection, L"", L"Length", 0.0f) * 1000.0);
			mapSurfaceID = NPlugin::GetStageSurface(mapID);
			if (mapSurfaceID < 0) mapSurfaceID = m_pTracksIniFile->GetValueEx(wszMapINISection, L"", L"Surface", -1);
		}

		if (mapSurfaceID >= 0 && mapSurfaceID < COUNT_OF_ITEMS(surfaceArrayRBR))
			mapSurface = surfaceArrayRBR[mapSurfaceID];
	}		
	else
	{
		// The map is in BTB format and not managed by RSF
		std::string rbrFolderName = RBRRX_FindFolderNameByMapName(mapName);
		if (!rbrFolderName.empty())
		{
			CSimpleIniEx btbTrackINIFile;
			btbTrackINIFile.LoadFile((m_sRBRRootDir + "\\RX_Content\\" + rbrFolderName + "\\track.ini").c_str());
			mapSurface = ::toupper(btbTrackINIFile.GetValueEx("INFO", "", "physics", "U")[0]);
			mapLength = static_cast<int>(btbTrackINIFile.GetValueExFloat("INFO", "", "length", 0.0) * 1000.0);
		}
	}

	DebugPrint("RaceStatDB_AddMap. %s mapID=%d surface=%s length=%d racingType=%d", mapName.c_str(), mapID, mapSurface.c_str(), mapLength, racingType);

	CppSQLite3Statement qryStmt = g_pRaceStatDB->compileStatement("INSERT INTO D_Map (MapID, StageName, Surface, Length, Format, RBRInstallType) VALUES (?, ?, ?, ?, ?, ?)");
	qryStmt.bind(1, mapID);
	qryStmt.bind(2, mapName.c_str());
	qryStmt.bind(3, mapSurface.c_str());
	qryStmt.bind(4, mapLength);
	qryStmt.bind(5, (racingType == 2 ? "BTB" : "RBR"));
	qryStmt.bind(6, GetRBRInstallType().c_str());

	if (qryStmt.execDML() > 0) return static_cast<int>(g_pRaceStatDB->lastRowId());
	else return -1;
}


//-------------------------------------------------------------------------------------------------------------------
// Return the D_Car MapKey or add a new record to D_Car table if the car is missing
//
int CNGPCarMenu::RaceStatDB_GetCarKey(int carSlotID)
{
	int iResult = -1;

	if (g_pRaceStatDB == nullptr || carSlotID < 0 || carSlotID > 7)
		return -1;

	int carMenuIdx = RBRAPI_MapCarIDToMenuIdx(carSlotID);
	int iNGPVersion = std::stoi(std::to_string(m_iPhysicsNGMajorVer) + std::to_string(m_iPhysicsNGMinorVer) + std::to_string(m_iPhysicsNGPatchVer) + std::to_string(m_iPhysicsNGBuildVer));

	std::string sRevision = _ToString(g_RBRCarSelectionMenuEntry[carMenuIdx].wszCarPhysicsRevision);
	if (sRevision.empty())
		sRevision = "UNK";

	try
	{
		CppSQLite3Query qryData;


		iResult = queryGetIntValue("SELECT CarKey FROM D_Car WHERE ModelName = ? AND Revision = ? AND NGPVersion = ?", 
			_ToString(g_RBRCarSelectionMenuEntry[carMenuIdx].wszCarModel).c_str(),
			sRevision.c_str(),
			iNGPVersion
		);

		if (iResult < 0)
			iResult = RaceStatDB_AddCar(carSlotID);
	}
	catch (CppSQLite3Exception ex)
	{
		iResult = -1;
		LogPrint("ERROR. RaceStatDB_GetCarKey. %s", ex.errorMessage());
	}
	catch (...)
	{
		iResult = -1;
		LogPrint("ERROR. RaceStatDB_GetCarKey exception");
	}

	return iResult;
}


//-------------------------------------------------------------------------------------------------------------------
// Add the current map to D_Map table
//
int CNGPCarMenu::RaceStatDB_AddCar(int carSlotID)
{
	std::string sQuery;

	if (g_pRaceStatDB == nullptr || carSlotID < 0 || carSlotID > 7)
		return -1;

	int carMenuIdx = RBRAPI_MapCarIDToMenuIdx(carSlotID);
	int iNGPVersion = std::stoi(std::to_string(m_iPhysicsNGMajorVer) + std::to_string(m_iPhysicsNGMinorVer) + std::to_string(m_iPhysicsNGPatchVer) + std::to_string(m_iPhysicsNGBuildVer));

	std::string sCarModel = _ToString(g_RBRCarSelectionMenuEntry[carMenuIdx].wszCarModel);
	if (sCarModel.empty()) sCarModel = "UNK";

	std::string sCarCategory = g_RBRCarSelectionMenuEntry[carMenuIdx].szCarCategory;
	if (sCarCategory.empty()) sCarCategory = "UNK";

	std::string sCarPhysics = _ToString(g_RBRCarSelectionMenuEntry[carMenuIdx].wszCarPhysics);
	if (sCarPhysics.empty()) sCarPhysics = "UNK";

	std::string sCarModelFolder = _ToString(g_RBRCarSelectionMenuEntry[carMenuIdx].wszCarModelFolder);
	if (sCarModelFolder.empty()) sCarModelFolder = "UNK";

	std::string sRevision = _ToString(g_RBRCarSelectionMenuEntry[carMenuIdx].wszCarPhysicsRevision);
	if (sRevision.empty()) sRevision = "UNK";

	DebugPrint("RaceStatDB_AddCar. %s (%s) %s %s revision=%s NGP=%d", 
		sCarModel.c_str(),
		sCarCategory.c_str(),
		sCarPhysics.c_str(),
		sCarModelFolder.c_str(),
		sRevision.c_str(),
		iNGPVersion
	);

	CppSQLite3Statement qryStmt = g_pRaceStatDB->compileStatement("INSERT INTO D_Car (ModelName, FIACategory, Physics, Folder, Revision, NGPVersion) VALUES (?, ?, ?, ?, ?, ?)");
	qryStmt.bind(1, sCarModel.c_str());
	qryStmt.bind(2, sCarCategory.c_str());
	qryStmt.bind(3, sCarPhysics.c_str());
	qryStmt.bind(4, sCarModelFolder.c_str());
	qryStmt.bind(5, sRevision.c_str());
	qryStmt.bind(6, iNGPVersion);

	if (qryStmt.execDML() > 0) return static_cast<int>(g_pRaceStatDB->lastRowId());
	else return -1;
}


//-------------------------------------------------------------------------------------------------------------------
// Add the current race result to statDB
//
int CNGPCarMenu::RaceStatDB_AddCurrentRallyResult(int mapKey, int mapID, int carKey, int carSlotID, const std::string& mapName)
{
	static char* tyreTypeArray[]    = { "T", "T", "T", "G", "G", "G", "S" }; // 0=Dry tarmac, 1=Intermediate tarmac, 2=Wet tarmac, 3=Dry gravel, 4=Inter gravel, 5=Wet gravel, 6=Snow
	static char* tyreSubTypeArray[] = { "D", "I", "W", "D", "I", "W", "S" }; // D=Dry, I=Intermediate, W=Wet, S=Snow
	static char* weatherTypeArray[] = { "G", "R", "B" }; // G=Good, R=Random, B=Bad

	std::string sActivePluginName;
	std::string sQuery;
	int iResult = -1;

	if (g_pRaceStatDB == nullptr || mapKey < 0 || carKey < 0)
		return -1;

	try
	{
		int raceDate = 0;
		std::string sTextValue;

		sActivePluginName = GetActivePluginName();

		// If activePlugin is "RX", mapID#41 and the weather is bad, skyCloudy and it's raining then BTB loading failed and the car ended up to rainy Cote. 
		if (GetActiveRacingType() == 2 /*&& mapID == 41*/ && g_pRBRMapSettings->weatherType == 2 && g_pRBRMapSettingsEx->skyCloudType == 3 && g_pRBRMapSettingsEx->skyType == 3 && g_pRBRMapSettingsEx->surfaceWetness == 2)
		{
			LogPrint("RaceStatDB_AddCurrentRallyResult. BTB racing, but the %s track loading failed. Ignoring the race result", mapName.c_str());
			return -1;
		}

		sQuery = "INSERT INTO F_RallyResult(RaceDate, RaceDateTime, CarKey, MapKey, "
			"Split1Time, Split2Time, FinishTime, FalseStartPenaltyTime, OtherPenaltyTime, "
			"FalseStart, CallForHelp, TransmissionType, TyreType, TyreSubType, DamageType, "
			"TimeOfDay, WeatherType, SkyCloudType, SkyType, SurfaceWetness, SurfaceAge, "
			"ProfileName, PluginType, PluginSubType, CarSlot) VALUES ("
			"?, ?, ?, ?, "
			"?, ?, ?, ?, ?, "
			"?, ?, ?, ?, ?, ?, "
			"?, ?, ?, ?, ?, ?, "
			"?, ?, ?, ?)";

		// If the race didn't reach the first split then don't add the result because the race was retired "too early"
		if (g_pRBRCarInfo->splitReachedNo == 0 && g_pRBRCarInfo->split1Time == 0.0f && g_pRBRCarInfo->finishLinePassed == 0 && g_pRBRCarInfo->distanceTravelled < 30.0f)
		{
			LogPrint("WARNING. Did not add a new race result to raceStatDB because the race was retired too early (the car didn't reach even the split1 or finish line)");
			DebugPrint("Split1=%f (%d) Finish=%f (%d) DistTravelled=%f", g_pRBRCarInfo->split1Time, g_pRBRCarInfo->splitReachedNo, g_pRBRCarInfo->raceTime, g_pRBRCarInfo->finishLinePassed, g_pRBRCarInfo->distanceTravelled);
			return 0;
		}

		CppSQLite3Statement qryStmt = g_pRaceStatDB->compileStatement(sQuery.c_str());

		GetCurrentDateAndTimeAsYYYYMMDD_HHMISS(&raceDate, &sTextValue);
		qryStmt.bind(1, raceDate);				// date YYYYMMDD
		qryStmt.bind(2, sTextValue.c_str());	// time HHMISS

		qryStmt.bind(3, carKey);
		qryStmt.bind(4, mapKey);
		
		if(g_pRBRCarInfo->split1Time != 0 && g_pRBRCarInfo->splitReachedNo >= 1) qryStmt.bind(5, RoundFloatToDouble(g_pRBRCarInfo->split1Time, 3));
		else qryStmt.bindNull(5);
		if (g_pRBRCarInfo->split2Time != 0 && g_pRBRCarInfo->splitReachedNo >= 2) qryStmt.bind(6, RoundFloatToDouble(g_pRBRCarInfo->split2Time, 3));
		else qryStmt.bindNull(6);		
		if (g_pRBRCarInfo->finishLinePassed > 0 || (g_pRBRCarInfo->raceTime != 0.0f && g_pRBRCarInfo->distanceToFinish <= 0.0f) ) qryStmt.bind(7, RoundFloatToDouble(g_pRBRCarInfo->raceTime, 3));
		else qryStmt.bindNull(7);

		qryStmt.bind(8, FloorFloatToDouble(m_latestFalseStartPenaltyTime, 1));
		qryStmt.bind(9, FloorFloatToDouble(m_latestOtherPenaltyTime, 1));

		qryStmt.bind(10, g_pRBRCarInfo->falseStart);
		qryStmt.bind(11, 0); // TODO. The num of CallForHelp calls not yet implemented (the value is always zero at the moment)

		qryStmt.bind(12, g_pRBRMapSettings->transmissionType == 0 ? "M": g_pRBRMapSettings->transmissionType == 1 ? "A" : "U" );

		// If the stage was driven using RBRRX plugin then the tyreType is always TarmacDry because RBRRX handles the setup of the stage physics and tyres on its own (weird).
		// Set the tyreType value based on the BTB stage physics (tarmac, gravel, snow)
		if (sActivePluginName == "RX")
		{
			char btbPhysicsType;
			std::string rbrFolderName = RBRRX_FindFolderNameByMapName(mapName);
			if (!rbrFolderName.empty())
			{
				CSimpleIniEx btbTrackINIFile;
				btbTrackINIFile.LoadFile((m_sRBRRootDir + "\\RX_Content\\" + rbrFolderName + "\\track.ini").c_str());
				btbPhysicsType = ::toupper(btbTrackINIFile.GetValueEx("INFO", "", "physics", "U")[0]);
				if (btbPhysicsType == 'S') g_pRBRMapSettings->tyreType = 6;
				else if (btbPhysicsType == 'G') g_pRBRMapSettings->tyreType = 3;
				else if (btbPhysicsType == 'T') g_pRBRMapSettings->tyreType = 0;

				//DebugPrint("DEBUG. RaceStatDB_AddCurrentRallyResult. RX is the active plugin. Set tyreType=%d", g_pRBRMapSettings->tyreType);
			}
		}

		qryStmt.bind(13, (g_pRBRMapSettings->tyreType >= 0 && g_pRBRMapSettings->tyreType < COUNT_OF_ITEMS(tyreTypeArray) ? tyreTypeArray[g_pRBRMapSettings->tyreType] : "U") );
		qryStmt.bind(14, (g_pRBRMapSettings->tyreType >= 0 && g_pRBRMapSettings->tyreType < COUNT_OF_ITEMS(tyreSubTypeArray) ? tyreSubTypeArray[g_pRBRMapSettings->tyreType] : "U"));

		qryStmt.bind(15, g_pRBRMapSettings->damageType);

		qryStmt.bind(16, g_pRBRMapSettingsEx->timeOfDay);
		qryStmt.bind(17, (g_pRBRMapSettings->weatherType >= 0 && g_pRBRMapSettings->weatherType < COUNT_OF_ITEMS(weatherTypeArray) ? weatherTypeArray[g_pRBRMapSettings->weatherType] : "U"));
		qryStmt.bind(18, g_pRBRMapSettingsEx->skyCloudType);
		qryStmt.bind(19, g_pRBRMapSettingsEx->skyType);
		qryStmt.bind(20, g_pRBRMapSettingsEx->surfaceWetness);
		qryStmt.bind(21, g_pRBRMapSettingsEx->surfaceAge);

		qryStmt.bind(22, g_pRBRProfile->szProfileName);

		if (_iEqual(sActivePluginName, "rallysimfans.hu", true)) sActivePluginName = "RSF";
		qryStmt.bind(23, sActivePluginName.substr(0, 4).c_str());
		
		if (sActivePluginName == "RSF") sTextValue = "SD"; // TODO. Not yet implemented "RSF race type" identification
		else if (sActivePluginName == "TM" && m_iRBRTMCarSelectionType == 1) sTextValue = "ONL";
		else sTextValue = "SD";
		qryStmt.bind(24, sTextValue.c_str());

		qryStmt.bind(25, carSlotID);

		if (qryStmt.execDML() > 0) 
			iResult = static_cast<int>(g_pRaceStatDB->lastRowId());
	}
	catch (CppSQLite3Exception ex)
	{
		iResult = -1;
		LogPrint("ERROR. RaceStatDB_AddCurrentRallyResult. %s", ex.errorMessage());
	}
	catch (...)
	{
		iResult = -1;
		LogPrint("ERROR. RaceStatDB_AddCurrentRallyResult exception");
	}

	return iResult;
}


//------------------------------------------------------------------------------------------------------------------------------------
// Query the last 5 rally results and stage records for a specific map
//
int CNGPCarMenu::RaceStatDB_QueryLastestStageResults(int /*mapID*/, const std::string& mapName, int racingType, std::vector<RaceStatDBStageResult>& latestStageResults)
{
	int numOfRows = 0;
	
	m_latestMapRBRRX.latestStageResults.clear();

	if (g_pRaceStatDB == nullptr)
		return 0;

	try
	{
		CppSQLite3Statement qryStmt;
		
		if (!mapName.empty())
		{
			// Results for a specific map
			qryStmt = g_pRaceStatDB->compileStatement("SELECT CarModel, FIACat, TotalPenaltyTime, FinishTime, CarStageRecord, FIACatStageRecord, StageRecord, StageLength FROM V_RallyResultSummary WHERE StageName = ? LIMIT 5");
			qryStmt.bind(1, mapName.c_str());
		}
		else if(racingType > 0)
		{
			// All recent results from stages in a specific map format (BTB or RBR)
			qryStmt = g_pRaceStatDB->compileStatement("SELECT CarModel, FIACat, TotalPenaltyTime, FinishTime, CarStageRecord, FIACatStageRecord, StageRecord, StageLength, StageName FROM V_RallyResultSummary WHERE StageFormat = ? LIMIT 5");
			qryStmt.bind(1, (racingType == 2 ? "BTB" : "RBR"));
		}
		else
		{
			// All recent results (all maps and map formats)
			qryStmt = g_pRaceStatDB->compileStatement("SELECT CarModel, FIACat, TotalPenaltyTime, FinishTime, CarStageRecord, FIACatStageRecord, StageRecord, StageLength, StageName FROM V_RallyResultSummary LIMIT 5");
		}

		CppSQLite3Query qryData = qryStmt.execQuery();

		if (qryData.numFields() >= 8)
		{
			latestStageResults.reserve(5);

			while (!qryData.eof())
			{
				numOfRows++;
				latestStageResults.push_back( {
					(mapName.empty() ? qryData.getStringField(8, "") : ""), // MapName set only when the data was not for a specific map
					qryData.getStringField(0, ""),                       // carModel
					qryData.getStringField(1, ""),
					static_cast<float>(qryData.getFloatField(2, 0.0)),
					static_cast<float>(qryData.getFloatField(3, 0.0)),
					static_cast<float>(qryData.getFloatField(4, 0.0)),
					static_cast<float>(qryData.getFloatField(5, 0.0)),
					static_cast<float>(qryData.getFloatField(6, 0.0)),	// stageRecord
					static_cast<float>(qryData.getFloatField(7, 0.0))   // stageLength (in meters)
					}
				);

				qryData.nextRow();
			}
		}
	}
	catch (CppSQLite3Exception ex)
	{
		numOfRows = 0;
		LogPrint("ERROR. RaceStatDB_QueryLastestStageResults. %s", ex.errorMessage());
	}
	catch (...)
	{
		numOfRows = 0;
		LogPrint("ERROR. RaceStatDB_QueryLastestStageResults exception");
	}

	return numOfRows;
}

