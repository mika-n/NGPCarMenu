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
//----------------------------------------------------------------------------------------------------------------------------------------
//
// The information in this RBR-API interface is a result of painstaking investigations with the RBR game engine and the information published
// in other web pages (especially the following web pages).
//
// - https://web.archive.org/web/20181105131638/http://www.geocities.jp/v317mt/index.html
// - https://suxin.space/notes/rbr-play-replay/
// - http://www.tocaedit.com/2013/05/gran-turismo-guages-for-rbr-v12-update.html
// - http://www.tocaedit.com/2018/03/sources-online.html
// - https://vauhtimurot.blogspot.com/p/in-english.html  (in most part the web page is in Finnish)
// - http://rbr.onlineracing.cz/?setlng=eng
// - https://gitlab.com/TheIronWolfModding/rbrcrewchief
//
// Also, various forum posts by WorkerBee, Kegetys, black f./jharron, Racer_S, Mandosukai (and I'm sure many others) here and there have been useful.
// Thank you all for the good work with the RBR game engine (RBR still has the best rally racing physics engine and WorkeBeer's NGP plugin makes it even greater).
//

#ifndef __RBRAPI_H_INCLUDED
#define __RBRAPI_H_INCLUDED

// You need to define this before the includes, or use /D __SUPPORT_PLUGIN__=1 in the C/C++ / Commandline project configuration (some RBR default plugin header stuff)
#define __SUPPORT_PLUGIN__ 1

#include "IPlugin.h"
#include "IRBRGame.h"
#include "D3D9Font\D3DFont.h"		// D3DXVECTOR3


#define C_PROCESS_READ_WRITE_QUERY (PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION)

#define C_RANGE_REMAP(value, low1, high1, low2, high2) (low2 + (value - low1) * (high2 - low2) / (high1 - low1))

//------------------------------------------------------------------------------------------------
union BYTEBUFFER_FLOAT {
#pragma pack(push,1)
	float fValue;
	BYTE byteBuffer[sizeof(float)];
	DWORD dwordBuffer;
#pragma pack(pop)
};

union BYTEBUFFER_DWORD {
#pragma pack(push,1)
	DWORD dwValue;
	BYTE byteBuffer[sizeof(DWORD)];
#pragma pack(pop)
};

union BYTEBUFFER_INT32 {
#pragma pack(push,1)
	__int32 iValue;
	BYTE byteBuffer[sizeof(__int32)];
#pragma pack(pop)
};

union BYTEBUFFER_PTR {
#pragma pack(push,1)
	LPVOID ptrValue;
	BYTE byteBuffer[sizeof(LPVOID)];
#pragma pack(pop)
};

//--------------------------------------------------------------------------------------------------

extern LPDIRECT3DDEVICE9 g_pRBRIDirect3DDevice9;  // RBR D3D device

extern HWND	g_hRBRWnd;              // RBR D3D hWnd handle
extern RECT	g_rectRBRWnd;			// RBR wnd top,left,right,bottom coordinates (including windows borders in windowed mode)
extern RECT	g_rectRBRWndClient;		// RBR client area coordinates (resolutionX=right-left, resolutionY=bottom-top)
extern RECT g_rectRBRWndMapped;		// RBR client area re-mapped to screen points (normally client area top,left is always relative to physical wndRect coordinates)

//--------------------------------------------------------------------------------------------------

extern LPVOID GetModuleBaseAddr(const char* szModuleName);
extern LPVOID GetModuleOffsetAddr(const char* szModuleName, DWORD offset);

inline float DWordBufferToFloat(DWORD dwValue) { BYTEBUFFER_FLOAT byteFloat{}; byteFloat.dwordBuffer = dwValue; return byteFloat.fValue; }

extern BOOL WriteOpCodeHexString(const LPVOID writeAddr, LPCSTR sHexText);
extern BOOL WriteOpCodeBuffer(const LPVOID writeAddr, const BYTE* buffer, const int iBufLen);
extern BOOL WriteOpCodePtr(const LPVOID writeAddr, const LPVOID ptrValue);
extern BOOL WriteOpCodeInt32(const LPVOID writeAddr, const __int32 iValue);
extern BOOL WriteOpCodeNearCallCmd(const LPVOID writeAddr, const LPVOID callTargetAddr);
extern BOOL WriteOpCodeNearJmpCmd(const LPVOID writeAddr, const LPVOID jmpTargetAddr);

extern BOOL ReadOpCodePtr(const LPVOID readAddr, LPVOID* ptrValue);

extern BOOL RBRAPI_InitializeObjReferences();
extern BOOL RBRAPI_InitializeRaceTimeObjReferences();

extern int RBRAPI_MapCarIDToMenuIdx(int carID);     // 00..07 carID is not the same as order of car selection items
extern int RBRAPI_MapMenuIdxToCarID(int menuIdx);

extern BOOL RBRAPI_Replay(const std::string rbrAppFolder, LPCSTR szReplayFileName);

extern void RBRAPI_MapRBRPointToScreenPoint(const float srcX, const float srcY, int* trgX, int* trgY);
extern void RBRAPI_MapRBRPointToScreenPoint(const float srcX, const float srcY, float* trgX, float* trgY);
extern void RBRAPI_MapRBRPointToScreenPoint(const float srcX, const float srcY, POINT* trgPoint);

extern void RBRAPI_RefreshWndRect();
extern BOOL RBRAPI_MapRBRColorToRGBA(IRBRGame::EMenuColors colorType, int* outRed, int* outGreen, int* outBlue, int* outAlpha); // TRUE=Changed values, could not use cached values, FALSE=The same value, used cached values


// Overloaded RBR specific DX9 function types. These re-routed functions are used to draw custom graphics on top of RBR graphics. The custom DX9 function should call these "parent functions" to let RBR do it's own things also.
typedef HRESULT(__fastcall* tRBRDirectXBeginScene)(void* objPointer);
typedef HRESULT(__fastcall* tRBRDirectXEndScene)(void* objPointer);

// Overloaded RBR replay methods
typedef int(__thiscall* tRBRReplay)(void* objPointer, const char* szReplayFileName, __int32* pUnknown1, __int32* pUnknown2, size_t iReplayFileSize);
typedef int(__thiscall* tRBRSaveReplay)(void* objPointer, const char* szReplayFileName, __int32 mapID, __int32 carID, __int32 unknown1);

// Overloaded dinput controller axis (digital button/steering analog axis) handler method
typedef float(__thiscall* tRBRControllerAxisData)(void* objPointer, __int32 axisID);


//----------------------------------------------------------------------------------
// Note! The formula of PAD size = Start idx of the next item - Idx of the prev item - sizeof of the previous item

// CameraInfo
typedef struct {
#pragma pack(push,1)
	__int32 cameraType;					// 0x00. 0x01=ExternalCamBackNear, 0x02=ExternalCamBack, 0x03=BumperCam (allows customCams), 04=BonnetCam, 0x05=InternalCam, 0x06=InternalCamBackseat, 0x07=RoadSideReplayCam, 0x08=(unknown), 0x09=BirdsEyeCam, 0x0A=SpinAroundCam, 0x0B=ChaseCam, 
	BYTE pad1[0xCC - 0x00 - sizeof(__int32)];
	D3DMATRIX currentCamMapLocation;	// 0xCC (4x4 matrix of floats). These values are overwritten by the values in 0x318 location if cameraType is 0x03 (custom cam location)
	BYTE pad2[0x318 - 0xCC - sizeof(D3DMATRIX)];
	D3DXVECTOR3	camOrientation;			// 0x318
	D3DXVECTOR3 camPOV1;				// 0x324
	D3DXVECTOR3 camPOV2;				// 0x330
	D3DXVECTOR3 camPOS;					// 0x33C
	float		camFOV;					// 0x348
	float		camNear;				// 0x34C
#pragma pack(pop)
} RBRCameraInfo;
typedef RBRCameraInfo* PRBRCameraInfo;

// Pointer to the current cameraInfo (this struct is a member of RBRCarInfo)
typedef struct {
#pragma pack(push,1)
	BYTE pad1[0x10];
	PRBRCameraInfo pCameraInfo; // 0x10  Pointer to the actual cameraInfo data (see above)
#pragma pack(pop)
} RBRCamera1;
typedef RBRCamera1* PRBRCamera1;


// Offset 0x0165FC68 RBRCarInfo
typedef struct {
#pragma pack(push,1)
	__int32   hudPositionX;  // 0x00
	__int32   hudPositionY;  // 0x04
	__int32   raceStarted;   // 0x08 (1=Race started. Start countdown less than 5 secs, so false start possible, 0=Race not yet started or start countdown still more than 5 secs and gas pedal doesn't work yet)
	float speed;			 // 0x0C
	float rpm;				 // 0x10
	float temp;				 // 0x14 (water temp in celsius?)
	float turbo;			 // 0x18. (pressure, in Pascals?)
	__int32 unknown2;		 // 0x1C (always 0?)
	float distanceFromStartControl; // 0x20
	float distanceTravelled; // 0x24   ??? hmmmm.. maybe not distTravelled because this value is not very logical. What is it?
	float distanceToFinish;  // 0x28   >0 Meters left to finish line, <0 Crossed the finish line (meters after finish line)
							
	BYTE  pad1[0x13C - 0x28 - sizeof(float)];
	float stageProgress;	 // 0x13C  (meters, hundred meters, some map unit?. See RBRMapInfo.stageLength also)
	float raceTime;			 // 0x140  Total race time (includes time penalties)  (or if gameMode=8 then the time is taken from replay video)
	__int32 raceFinished;    // 0x144  (0=Racing after GO! command, 1=Racing completed/retired or not yet started
	__int32 unknown4;        // 0x148
	__int32 unknown5;        // 0x14C
	__int32 drivingDirection;// 0x150. 0=Correct direction, 1=Car driving to wrong direction
	float fadeWrongWayMsg;   // 0x154. 1 when "wrong way" msg is shown
							 //	TODO: 0x15C Some time attribute? Total race time? 
	BYTE  pad3[0x170 - 0x154 - sizeof(float)];
	__int32 gear;		     // 0x170. 0=Reverse,1=Neutral,2..6=Gear-1 (ie. value 3 means gear 2) (note! the current value only, gear is not set via this value)

	BYTE  pad4[0x244 - 0x170 - sizeof(__int32)];
	float stageStartCountdown; // 0x244 (7=Countdown not yet started, 6.999-0.1 Countdown running, 0=GO!, <0=Racing time since GO! command)
	__int32 falseStart;		   // 0x248 (0=No false start, 1=False start)

	BYTE pad5[0x254 - 0x248 - sizeof(__int32)];
	__int32 splitReachedNo;  // 0x254 0=Start line passed if race is on, 1=Split#1 passed, 2=Split#2 passed
	float split1Time;        // 0x258 Total elapsed time in secs up to split1
	float split2Time;        // 0x25C Total elapsed time in secs up to split2  (split2-split1 would be the time between split1 and split2)
	float unknown6;			 // 0x260

	BYTE pad6[0x2C4 - 0x260 - sizeof(float)];
	__int32 finishLinePassed;	 // 0x2C4 1=stageFinished,  0=Stage not started or not finished yet

	BYTE pad7[0x758 - 0x2C4 - sizeof(__int32)];
	PRBRCamera1	pCamera;	 // 0x758  Pointer to camera data

	BYTE pad8[0xEF8 - 0x758 - sizeof(PRBRCamera1)];

	D3DXVECTOR3 carPosition;    // 0xEF8..F00 (3 floats, car position X,Y,Z
#pragma pack(pop)
} RBRCarInfo;
typedef RBRCarInfo* PRBRCarInfo;


// Offset 0x008EF660 RBRCarMovement
typedef struct {
#pragma pack(push,1)
	BYTE pad1[0x100];
	D3DXQUATERNION carQuat;			// 0x100..0x10C (4 floats). Car look direction x,y,z,w
	D3DMATRIX      carMapLocation;	// 0x110..0x14C (4x4 matrix of floats). _41.._44 is the current map position of the car

	BYTE pad2[0x190 - 0x110 - sizeof(D3DMATRIX)];
	D3DXVECTOR3 spin;				// 0x190  (Spin X,Y,Z)
	BYTE pad3[0x1C0 - 0x190 - sizeof(D3DXVECTOR3)];
	D3DXVECTOR3 speed;				// 0x1C0 (Speed/Velocity? X,Y,Z)
	BYTE pad4[0x85C - 0x1C0 - sizeof(D3DXVECTOR3)];
	float driveThrottle;	// 0x85C	0.0-1.0	 (0=No throttle, 1=full throttle)
	float driveBrake;		// 0x860	0.0-1.0  (0=No brake, 1=full brake)
	float driveHandbrake;	// 0x864	0.0-1.0  (0=No handbrake, 1=full handbrake)
	float driveSteering;	// 0x868	<0.0 left, 0.0 center >0.0 right
	float driveClutch;		// 0x86C	0.0-1.0  (0=clutch released, 1=clutch pedal pushed fully down, values >=0.85 as clutch on)

#pragma pack(pop)
} RRBRCarMovement;
typedef RRBRCarMovement* PRBRCarMovement;


typedef struct
{
#pragma pack(push,1)
	BYTE pad1[0x24];
	__int32 status;           // 0x24  <=1 axis disabled
	__int32 unknown1;         // 0x28
	__int32 dinputStatus;     // 0x2C  0=Dinput data received <>0=not yet received any dinput events (either controller is offline or pedals/wheels not moved yet)

	float   axisValue;		// 0x30 -1.0..1.0 the current axis value
	DWORD   axisRawValue;   // 0x34 0..FFFF the current raw axis value

	float   axisValue2;		// 0x38 Always the same as above? Or are these before and after output linearity transformations?
	DWORD   axisRawValue2;  // 0x3C 
#pragma pack(pop)
} RBRControllerAxisData;
typedef RBRControllerAxisData* PRBRControllerAxisData;

typedef struct
{
#pragma pack(push,1)
	char*  szAxisNameID;  // 0x00
	WCHAR* wszAxisName;   // 0x04
	__int32 unknown1;	  // 0x08
	PRBRControllerAxisData controllerAxisData;// 0x0C
	__int32 unknown2;	  // 0x10
#pragma pack(pop)
} RBRControllerAxis;
typedef RBRControllerAxis* PRBRControllerAxis;

typedef struct {
#pragma pack(push,1)
	BYTE pad1[0x24];
	RBRControllerAxis controllerAxis[21];	// 0x24
											// 0=Steering analog, 1=Left digital, 2=Right digital, 3=Throttle analog, 4=CombinedThrottleBrake analog
											// 5=Brake, 6=Handbrake, 7=GearUp, 8=GearDown, 9=ChangeCam, 10=Pause, 11=Clutch, 12=Ignition, 
											// 13=Reverse, 14=Neutral, 15..20=Gear1..6

	BYTE pad2[0x258 - 0x24 - (sizeof(RBRControllerAxis)*21)];
	__int32 throttleInverted; // 0x258  0=not inverted (input.ini file), 1=inverted
	__int32 brakeInverted;    // 0x25C 
	__int32 combinedThrottleBrakeInverted;  // 0x260
	__int32 handbrakeInverted;// 0x264
	__int32 clutchInverted;   // 0x268
	__int32 unknown1;         // 0x26c
#pragma pack(pop)
} RBRControllerObj;
typedef RBRControllerObj* PRBRControllerObj;

typedef struct {
#pragma pack(push,1)
	__int32 unknown1;					// 0x00
	PRBRControllerObj controllerObj;	// 0x04
#pragma pack(pop)
} RBRControllerBaseObj;
typedef RBRControllerBaseObj* PRBRControllerBaseObj;


// Offset 0x007EAC48. Game configurations (0x007EAC48 -> +0x5C= IRBRGame instance obj)
typedef struct {
#pragma pack(push,1)
	BYTE pad1[0x54];		// 0x00
	__int32 resolutionX;	// 0x54
	__int32 resolutionY;	// 0x58
	// TODO: What are the values following resolution?

	BYTE pad3[0x0CF8 - 0x58 - sizeof(__int32)];
	PRBRControllerBaseObj controllerBaseObj; // 0xCF8
#pragma pack(pop)
} RBRGameConfig;
typedef RBRGameConfig* PRBRGameConfig;


// Offset 0x007C3668
typedef struct {
#pragma pack(push,1)
	float menuBackground_r;	// 0x00		0.0 - 1.0 percent of 0..255 absolute values
	float menuBackground_g; // 0x04
	float menuBackground_b; // 0x08
	float menuBackground_a; // 0x0C
#pragma pack(pop)
} RBRColorTable;
typedef RBRColorTable* PRBRColorTable;


// Offset 0x007EAC48 + 0x728 The current game mode (state machine of RBR)
typedef struct {
#pragma pack(push,1)
	BYTE pad1[0x728];
	// gameMode
	//		00 = (not available)
	//		01 = driving (after 5secs or less left in start clock or already driving after GO! command)
	//		02 = pause (when a menu is shown while stage or replay is running, not the main menu)
	//		03 = main menu or plugin menu (stage not running)
	//		04 = ? (black out)
	//		05 = loading track (race or replay. When the track model is loaded the status goes briefly to 0x0D and when the countdown starts the status goes to 0x0A)
	//		06 = exiting to menu from a race or replay (after this the mode goes to 12 for a few secs and finally to 3 when the game is showing the main or plugin menu)
	//		07 = quit the application ?
	//		08 = replay
	//		09 = end lesson / finish race / retiring / end replay
	//      0A = Before starting a race or replay (camera is spinning around the car. At this point map and car model has been loaded and is ready to rock)
	//      0B = ? (black out)
	//      0C = Game is starting or racing ended and going back to main menu (loading the initial "Load Profile" screen or "RBR menu system". Status goes to 0x03 when the "Load Profile" menu is ready and shown)
	//      0D = (not available) (0x0D status after 0x05 map loading step is completed. After few secs the status goes to 0x0A and camera starts to spin around the car)
    //      0E = (not available) (status goes to 0x0F and then RBR crashes)
	//      0F = ? Doesnt work anymore. Goes to menu? Pause racing and replaying and hide all on-screen instruments and timers (supported only after the race or replay has started, ie 0x0A and 0x10 status has changed to 0x08 or 0x01)
	//		10-0xFF = ?
	__int32 gameMode; // 0x728
#pragma pack(pop)
} RBRGameMode;
typedef RBRGameMode* PRBRGameMode;


// 0x893634 gameModeExt (Additional details of the current game mode)
typedef struct {
#pragma pack(push,1)
	BYTE pad1[0x10];
	__int32 gameModeExt;		// 0x10. 0x00 = Racing active. Update car model movement (or if set during replay then freeze the car movement)
								//       0x01 = Loading replay. If racing active then don't react to controllers, but the car keeps on moving. During replay stops the car position updates.
								//       0x02 = Replay mode (update car movements).
								//       0x03 = Plugin menu open (if replaying then stop the car updates)
								//       0x04 = Pause replay (Pacenote plugin uses this value to pause replay)
	//BYTE pad2[0x14 - 0x10 - sizeof(__int32)];
	__int32 trackID;			// 0x14 RBR trackID (or if BTB/RBRRX then always value 41). trackID and carID is updated when gameMode goes to 01 (racing) or 08 (replaying)
	__int32 carID;				// 0x18 00..07 = The current racing or replay car model slot#
#pragma pack(pop)
} RBRGameModeExt;
typedef RBRGameModeExt* PRBRGameModeExt;


// Offset 0x007EAC48 + 0x738 + 0x5C RBRCarControls
typedef struct {
#pragma pack(push,1)
	BYTE pad1[0x738 + 0x5C];
	float steering;		// 0x5C (0.0 - 1.0 float value representing left 0%-49.9%, center 50.0%, right 50.1%-100%)
	float throttle;		// 0x60 (0.0 - 1.0 float value)
	float brake;		// 0x64 (0.0 - 1.0 float value)
	float handBrake;	// 0x68 (0.0 or 1.0)
	float clutch;		// 0x6C (0.0 or 1.0 float value >0.85 clutch on, <=0.85 clutch off)
#pragma pack(pop)
} RBRCarControls;
typedef RBRCarControls* PRBRCarControls;


// Offset 0x1659184 + 0x75310 RBRMapInfo (Note! valid only when replay or racing is on)
typedef struct {
#pragma pack(push,1)
	BYTE pad1[0x75310];
	__int32 stageLength;   // 0x75310  Length of current stage (to the time checking station few meters after the finish line. RBRCarInfo.stageProgress value is related to this total length) ?
#pragma pack(pop)
} RBRMapInfo;
typedef RBRMapInfo* PRBRMapInfo;


// Fixed Offset 0x1660800 RBRMapSettings. Configuration of the next stage (Note! Need to set these values before the stage begins)
typedef struct {
#pragma pack(push,1)
	__int32 unknown1;   // 0x00
	__int32 trackID;	// 0x04   (xx trackID)
	__int32 carID;		// 0x08   (0..7 carID)
	__int32 unknown2;   // 0x0C
	__int32 unknown3;   // 0x10
	__int32 transmissionType; // 0x14  (0=Manual, 1=Automatic)
	
	BYTE pad1[0x30 - 0x14 - sizeof(__int32)];
	__int32 racePaused; // 0x30 (0=Normal mode, 1=Racing in paused state)
	
	BYTE pad2[0x38 - 0x30 - sizeof(__int32)];
	__int32 tyreType;	// 0x38 (0=Dry tarmac, 1=Intermediate tarmac, 2=Wet tarmac, 3=Dry gravel, 4=Inter gravel, 5=Wet gravel, 6=Snow)

	BYTE pad3[0x48 - 0x38 - sizeof(__int32)];
	__int32 weatherType;	// 0x48   (0=Good, 1=Random, 2=Bad)
	__int32 unknown4;       // 0x4C
	__int32 damageType;		// 0x50   (0=No damage, 1=Safe, 2=Reduced, 3=Realistic)
	__int32 pacecarEnabled; // 0x54   (0=Pacecar disabled, 1=Pacecar enabled)
#pragma pack(pop)
} RBRMapSettings;
typedef RBRMapSettings* PRBRMapSettings;


// Fixed offset 0x8938F8. Additional map settings 
typedef struct {
#pragma pack(push,1)
	__int32 unknown1;		// 0x00
	__int32 unknown2;		// 0x04
	__int32 trackID;		// 0x08
	__int32 unknown3;		// 0x0C
	__int32 skyCloudType;	// 0x10 (0=Clear, 1=PartCloud, 2=LightCloud, 3=HeavyCloud)
	__int32 surfaceWetness; // 0x14 (0=Dry, 1=Damp, 2=Wet)
	__int32 surfaceAge;     // 0x18 (0=New, 1=Normal, 2=Worn)

	BYTE pad1[0x38 - 0x18 - sizeof(__int32)];
	__int32 timeOfDay;		// 0x38 (0=Morning, 1=Noon, 2=Evening)
	__int32 skyType;		// 0x3C (0=Crisp, 1=Hazy, 2=NoRain, 3=LightRain, 4=HeavyRain, 5=NoSnow, 6=LightSnow, 7=HeavySnow, 8=LightFog, 9=HeavyFog)
#pragma pack(pop)
} RBRMapSettingsEx;
typedef RBRMapSettingsEx* PRBRMapSettingsEx;



// Offset 0x0x7EA678->+0x70->
// (yes, the GameModeExt2 object name is a stupid name, but could not come up with a better name)
// TODO. Hmm.. this pointer doesn't seem to be available until stage is loading and other than ghostCarID fields are not there?
typedef struct {
#pragma pack(push,1)
	BYTE pad1[0x10];		// 0x00
	__int32 loadingMode;	// 0x10 (0..7=loading menu/track/replay, 8=completed loading a track, replay or menu)
	__int32 racingPaused; 	// 0x14 (1=racing not active because RBR is in main menu, 0=racing or replying)
	__int32 ghostCarID;	    // 0x18 (-1=No ghost, 0..7 carid. 1=MG slot, 3=Subaru slot. This is the carID used in a pacecar "replay video")
	__int32 carID;			// 0x1C
	__int32 trackID;		// 0x20
#pragma pack(pop)
} RBRGameModeExt2;
typedef RBRGameModeExt2* PRBRGameModeExt2;


// Offset 0x893060 RBRGhostCarMovement
typedef struct {
#pragma pack(push,1)
	//D3DXVECTOR3    carVect;			// 0x00, 0x04, 0x08 (x,y,z,w)
	//float unknown1;					// 0x0C
	D3DXQUATERNION carMapLocation;  // 0x00, 0x04, 0x08, 0x0C (x,y,z,w) (old name carVect)
	D3DXQUATERNION carQuat;			// 0x10, 0x14, 0x18, 0x1C (x,y,z,w)	
#pragma pack(pop)
} RBRGhostCarMovement;
typedef RBRGhostCarMovement* PRBRGhostCarMovement;


// RBR memory locations of tyre model WCHAR text strings
#define PTR_TYREVALUE_WCHAR_FIRESTONE	0x743BA0   // "Firestone" text
#define PTR_TYREVALUE_WCHAR_BRIDGESTONE 0x743BB4
#define PTR_TYREVALUE_WCHAR_PIRELLI		0x743BCC
#define PTR_TYREVALUE_WCHAR_MICHELIN	0x743BDC


//----------------------------------------------------------------------------------------------------------

// RBRMenuItemPosition. Part of RBRMenuObj struct. Struct holding menu item X/Y position data.
// The position data is not exactly in pixels, but in some sort of char position + pixel fine tuning unit (it is not a float either. Maybe half-float?)
typedef struct {
#pragma pack(push,1)
	unsigned short x;  // TODO. Not really SHORT data type. But 0xFF__ and 0x__FF bytes are related to menu item position data
	unsigned short y;
#pragma pack(pop)
} RBRMenuItemPosition;
typedef RBRMenuItemPosition* PRBRMenuItemPosition;


// RBRMenuItemPositionExt. Some special menus don't have RBRMenuItemPosition struct in RBRMenuObj.pMenuItemPosition pointer.
// For example LoadProfile logon screen has a pointer to this struct with details about available profiles.
typedef struct {
#pragma pack(push,1)
	BYTE    pad1[0x2C];
	char*   szMenuTitleID;		// 0x2c  (LoadProfile logon screen has "SEL_PROF" string identifier and LoadReplay has "REPLAYS")
	WCHAR*  wszMenuTitleName;	// 0x30
	void*   unknown1;			// 0x34
	__int32 unknown2;			// 0x38	 
	__int32 unknown3;			// 0x3C  
	__int32 unknown4;			// 0x40  
	__int32 selectedItemIdx;	// 0x44  The current selected menu row (at least in LoadProfile logon menu)
	__int32 numOfItems;			// 0x48  The num of available profiles (0 if no profiles)
	__int32 selectedItemIdx2;	// 0x4C  Some menu screen may have column selection also (fex LoadReplay has Load/Delete/Back buttons). This is the idx to the focused column or button.
#pragma pack(pop)
} RBRMenuItemExt;
typedef RBRMenuItemExt* PRBRMenuItemExt;


// RBRMenuItemCarSelectionCarSpecTexts. Properties of the "Select Car-Car specs" details
typedef struct {
#pragma pack(push,1)
	BYTE pad1[0x1C];
	WCHAR* wszTechSpecValue;		// 0x1C
	BYTE pad2[0x28 - 0x1C - sizeof(WCHAR*)];
	WCHAR* wszModelTitle;			// 0x28
	BYTE pad3[0x34 - 0x28 - sizeof(WCHAR*)];
	WCHAR* wszHorsepowerTitle;		// 0x34
	BYTE pad4[0x40 - 0x34 - sizeof(WCHAR*)];
	WCHAR* wszHorsepowerValue;		// 0x40
	BYTE pad5[0x4C - 0x40 - sizeof(WCHAR*)];
	WCHAR* wszTorqueTitle;			// 0x4C
	BYTE pad6[0x58 - 0x4C - sizeof(WCHAR*)];
	WCHAR* wszTorqueValue;			// 0x58
	BYTE pad7[0x64 - 0x58 - sizeof(WCHAR*)];
	WCHAR* wszEngineTitle;			// 0x64
	BYTE pad8[0x70 - 0x64 - sizeof(WCHAR*)];
	WCHAR* wszEngineValue;			// 0x70
	BYTE pad9[0x7C - 0x70 - sizeof(WCHAR*)];
	WCHAR* wszTyresTitle;			// 0x7C
	BYTE pad10[0x88 - 0x7C - sizeof(WCHAR*)];
	WCHAR* wszWeightTitle;			// 0x88
	BYTE pad11[0x94 - 0x88 - sizeof(WCHAR*)];
	WCHAR* wszWeightValue;			// 0x94
	BYTE pad12[0xA0 - 0x94 - sizeof(WCHAR*)];
	WCHAR* wszTransmissionTitle;	// 0xA0
	BYTE pad13[0xAC - 0xA0 - sizeof(WCHAR*)];
	WCHAR* wszTransmissionValue;	// 0xAC
#pragma pack(pop)
} RBRMenuItemCarSelectionCarSpecTexts;
typedef RBRMenuItemCarSelectionCarSpecTexts* PRBRMenuItemCarSelectionCarSpecTexts;


// RBRMenuObj->pItemObj[3] obj (menu title)
typedef struct {
#pragma pack(push,1)
	BYTE pad1[0x18];				// 0x00
	union
	{
		LPCSTR szMenuTitleID;		// 0x18		(Title identifier, not localized. Example: Options="OPT_MAIN" Plugins=L"Plugins")
		LPCWSTR wszMenuTitleID;
	};
	union
	{
		LPCWSTR wszMenuTitleName;	// 0x1C		(localised menuTitle. Example: Options=L"Main Options"  Plugins=dword 0x00000010)
		DWORD  dwTitleAttribute;
	};

	LPCSTR szItemName;				// 0x20		(Name of the plugin in Plugins menuObj)
#pragma pack(pop)
} RBRPluginMenuItemObj3;
typedef RBRPluginMenuItemObj3* PRBRPluginMenuItemObj3;


// MenuObj->pItemObj pointer (fex main menu options)
typedef struct {
#pragma pack(push,1)
	BYTE pad1[0x24];          // 0x00
	LPCSTR  szMenuTitleID;    // 0x24
	LPCWSTR wszMenuTitleName; // 0x28
#pragma pack(pop)
} RBRPluginMenuItemObj2;
typedef RBRPluginMenuItemObj2* PRBRPluginMenuItemObj2;


// Menu object (RBRMenuPoint has references to these objects)
struct RBRMenuObj;
typedef struct RBRMenuObj* PRBRMenuObj;
typedef struct RBRMenuObj RBRMenuObj;
struct RBRMenuObj {
#pragma pack(push,1)
	BYTE pad1[0x04];
	PRBRMenuObj rootMenuObj;			// 0x04 (always points to PRBRMenuPoint->rootMenuObj?)
	PRBRMenuObj prevMenuObj;			// 0x08 (ESC menu navigation key returns to this menu)
	LPVOID* pItemObj;                   // 0x0C (pointer to a array of menu item objects. Array of "numOfItems" items)
	union {
		PRBRMenuItemPosition pItemPosition;	// 0x10 (pointer to a array of menu item display properties like x/y pos and width/height)
		PRBRMenuItemExt      pExtMenuObj;   // 0x10 (pointer to struct in some special menus like when RBRMENUIDX_STARTUP LoadProfile logon menu is the currentMenuObj)
	};
	__int32 numOfItems;					// 0x14 (numOfItems - firstSelectableItemIdx = total num of items. Sometimes some items may be hidden, so total num doesn't always match with the visible items)
	__int32 selectedItemIdx;			// 0x18 (Index of the currently selected menu item if the firstItemIdx >=0, relative to firstItemIdx)
	__int32 firstSelectableItemIdx;		// 0x1C (Index of the first selectable menu item or <0 if undefined. selectedItemIdx-firstItemIdx is the actual selected menu line in 0..N range)

	//BYTE pad3[0x124 - 0x1C - sizeof(__int32)];
	//__int32 selectedItem124; // 0x124 (1=Selected, 0=Not selected, but only with menuObj at index pos 82)
#pragma pack(pop)
};


// itemObjArray in "02 - SelectCar" menu
//    0X00 unknown object (background image maybe?)
//    0x04 redSelectionBar object pointer
//    0x08 title object
//    0x0C verticalRedBar object
//    0x10 car0 menu line object
//     ... (car1..car6)
//    0x2c car7 menu line object
//    0x30 carTechSpec obect
//
//  pItemPosition in "02" car selection menu (X,Y position)
//   (unsigned short x, unsigned short y)
//   0x00 unknown1 background image pos maybe? If the value is ACC006E0 then this position value is ignored
//   0x04 redSelectionBar pos;  Doesn't seem to have any direct effect. Menu system takes care of the positioning of the selection line
//   0x08 title line pos.
//   0x0C verticalRedBar pos. x/y pos (default value 0x062019C0 0620 y-pos, 19C0 x-pos)
//   0x10 car0 menuItem pos. Pos of the first car menu line (value 0x08C00120 moves the line to left to make more room for longer car names)
//   ... car1-car6 menuItem pos
//   0x2C car7 menuItem pos. Pos of the last car menu line
//   0x30 techSpec data pos
//   
//
// Note. "SelectCar" (02) menu has NumOfItems=0x0D and the firstSelectableItemIdx=0x04. Still there are 8 selectable items because the last menu line item is actually the car spec detail view column.
//   Setting numOfItems attribute of "SelectCar" menuObj to value 0x0C would hide these car details.
//


// RBR menu system
#define RBRMENUSYSTEM_NUM_OF_MENUS 89

// TODO: Where is Options menu? It is not part of the menu array table. Weird. 
//       And what and where is developer menu? RBR has some references to this hidden menu. There could be some interesting things.

#define RBRMENUIDX_STARTUP			     82  // LoadProfile on startup screen
#define RBRMENUIDX_STARTUP_CREATEPROFILE 23  // Register a driver name (editbox to type in the driver name = profile name)

#define RBRMENUIDX_MAIN					 00

#define RBRMENUIDX_RALLYSCHOOL			    18  // Excercise
#define RBRMENUIDX_RALLYSCHOOL_BASICDRIVING 19
#define RBRMENUIDX_RALLYSCHOOL_ADVANCEDTECH 20
#define RBRMENUIDX_RALLYSCHOOL_TRANSMISSION 21

#define RBRMENUIDX_QUICKRALLY			   03  // Country
#define RBRMENUIDX_QUICKRALLY_STAGE_GB     05
#define RBRMENUIDX_QUICKRALLY_STAGE_JAP    08
#define RBRMENUIDX_QUICKRALLY_STAGE_FIN    04
#define RBRMENUIDX_QUICKRALLY_STAGE_USA    09
#define RBRMENUIDX_QUICKRALLY_STAGE_FRA    07
#define RBRMENUIDX_QUICKRALLY_STAGE_AUS    06
#define RBRMENUIDX_QUICKRALLY_WEATHER	   10
#define RBRMENUIDX_QUICKRALLY_CARS		   02	// car selection
#define RBRMENUIDX_QUICKRALLY_TRANSMISSION 11
#define RBRMENUIDX_QUICKRALLY_SETUP		   12

#define RBRMENUIDX_RALLYSEASON				36	// SkillLevel
#define RBRMENUIDX_RALLYSEASON_CARDAMAGE	37
#define RBRMENUIDX_RALLYSEASON_TEAMOFFERS	24
#define RBRMENUIDX_RALLYSEASON_TRANSMISSION	43
#define RBRMENUIDX_RALLYSEASON_TRIAL		25
#define RBRMENUIDX_RALLYSEASON_WELCOME		27
#define RBRMENUIDX_RALLYSEASON_SEASONCONTINUE 26
#define RBRMENUIDX_RALLYSEASON_SEASONDATA	49

#define RBRMENUIDX_MULTIPLAYER				57  // Two/Three/Four Players
#define RBRMENUIDX_MULTIPLAYER_SEATTYPE		58
#define RBRMENUIDX_MULTIPLAYER_PACECARS		77
#define RBRMENUIDX_MULTIPLAYER_SKILLLEVEL	64
#define RBRMENUIDX_MULTIPLAYER_COUNTRY		65
#define RBRMENUIDX_MULTIPLAYER_STAGE		68
#define RBRMENUIDX_MULTIPLAYER_WEATHER		66
#define RBRMENUIDX_MULTIPLAYER_CARS_P1		59	// car selection
#define RBRMENUIDX_MULTIPLAYER_CARS_P2		60  // car selection
#define RBRMENUIDX_MULTIPLAYER_CARS_P3		61  // car selection
#define RBRMENUIDX_MULTIPLAYER_CARS_P4		62  // car selection
#define RBRMENUIDX_MULTIPLAYER_TRANSMISSION	78
#define RBRMENUIDX_MULTIPLAYER_PLAYERFINISHED 79
#define RBRMENUIDX_MULTIPLAYER_REGISTERPLAYER 63

#define RBRMENUIDX_RBRCHALLENGE_WELCOME		 13
#define RBRMENUIDX_RBRCHALLENGE_STAGE		 15
#define RBRMENUIDX_RBRCHALLENGE_CARS		 14 // car selection
#define RBRMENUIDX_RBRCHALLENGE_TRANSMISSION 16
#define RBRMENUIDX_RBRCHALLENGE_SETUP		 17

#define RBRMENUIDX_DRIVERPROFILE	  	    81
#define RBRMENUIDX_DRIVERPROFILE_LOADREPLAY 85

#define RBRMENUIDX_CREDITS 22


// Offset 0x165FA48  Master menuSystem object
typedef struct {
#pragma pack(push,1)
	LPVOID unknown1;						   // 0x00 (Pointer to unknown RBR object)
	PRBRMenuObj rootMenuObj;				   // 0x04 (Pointer to root menu obj?)
	PRBRMenuObj currentMenuObj;				   // 0x08 (Poiter to the current menuObj or 0 if no menu open)
	PRBRMenuObj currentMenuObj2;			   // 0x0C (always points to the same address as currentMenuObj?  RBR startup LoadProfile screen points to LoadProfile menu in this field)
	BYTE pad1[0x48 - 0x0C - sizeof(PRBRMenuObj)];
	float menuImagePosX;				   // 0x48 (X,Y,Width,Height of the current background menu image. Usually some animated scene image shown behind the menu layout)
	float menuImagePosY;				   // 0x4C
	float menuImageWidth;				   // 0x50
	float menuImageHeight;				   // 0x54
	BYTE pad2[0x70 - 0x54 - sizeof(__int32)];
	__int32 menuVisible;				   // 0x70 (1=Show menus, 0=Do not show any menu, show just default RBR background image)
	__int32 unknown2;					   // 0x74
	PRBRMenuObj menuObj[RBRMENUSYSTEM_NUM_OF_MENUS]; // 0x78 (array of all menuObj pointers. pCurrentMenuObj has one of these pointer values, except Options menu is not in the array for some reason). RBRMENUIDX_xxx defines some known menu array indexes.
#pragma pack(pop)
} RBRMenuSystem;
typedef RBRMenuSystem* PRBRMenuSystem;


// Offset 0x007D2554. The name of the current driver profile (valid after a profile is loaded in RBR startup screen, ie RBR is at the main menu for the first time)
typedef struct {
#pragma pack(push,1)
	LPVOID unknown1;			// 0x00
	LPVOID unknown2;			// 0x04
	char   szProfileName[16];	// 0x08 The current profile name str (15 chars + null terminator. If the profile is forced to have more than 15 chars then RBR behaves a bit strange when the profile is saved, works but the profile filename has some garbage chars)
#pragma pack(pop)
} RBRProfile;
typedef RBRProfile* PRBRProfile; 


// Offset 0x007D1D50. RBR staus text labels ("Loading %s" or "Loading Replay" and so on)
typedef struct {
#pragma pack(push,1)
	BYTE     pad1[0x8c];
	char*    szLoadDestTitleID;	     // 0x8c
	wchar_t* wszLoadDestTitleName;   // 0x90
	__int32  unknown1;				 // 0x94
	char*    szLoadReplayTitleID;    // 0x98
	wchar_t* wszLoadReplayTitleName; // 0x9C
	__int32  unknown2;			     // 0xA0
#pragma pack(pop)
} RBRStatusText;
typedef RBRStatusText* PRBRStatusText;


// Offset (0x007EABA8) + 0x10.  Pacenotes. (Contributed by TheIronWolf)
typedef struct
{
#pragma pack(push,1)
  __int32 type;		// 0x00	(left or right and how tight corner. 0=veeeery sharp left (hairspin), 1..5=left turn where 5 is veeeery easy. 6=veeery easy right turn, 7..11=right turn, 12=veeery shart right (hairspin), 16=over crest. And lots of other note types)
  __int32 flags;	// 0x04
  float distance;	// 0x08
#pragma pack(pop)
} RBRPacenote;
typedef RBRPacenote* PRBRPacenote;

typedef struct
{
#pragma pack(push,1)
  BYTE pad1[0x20];			// 0x00
  __int32 numPacenotes;		// 0x20
  PRBRPacenote pPacenotes;	// 0x24. Pointer to array of RBRPacenote structs (numPacenotes)
#pragma pack(pop)
} RBRPacenotes;
typedef RBRPacenotes* PRBRPacenotes;



// Custom helper struct for plugins menu structure
typedef struct {
	PRBRMenuObj pluginsMenuObj;			// Plugins menuObj
	PRBRMenuObj customPluginMenuObj;    // Custom menuObj managed by a plugin (shared by all plugins). This menu has pluginsMenuObj as a prevMenuObj value.
	PRBRMenuObj optionsMenuObj;			// Options menuObj (pressing "ESC" in a plugin takes RBR back to this menu instead of Plugins menu even when custom plugin menu has Plugins obj as a parent menu. Weird)
} RBRPluginMenuSystem;
typedef RBRPluginMenuSystem* PRBRPluginMenuSystem;


//--------------------------------------------------------------------------------------------
extern int RBRAPI_MapRBRMenuObjToID(PRBRMenuObj pMenuObj);


//--------------------------------------------------------------------------------------------
// Global RBR object pointers
//
extern PRBRGameConfig		g_pRBRGameConfig;

extern PRBRGameMode			g_pRBRGameMode;
extern PRBRGameModeExt		g_pRBRGameModeExt;
extern PRBRGameModeExt2		g_pRBRGameModeExt2;

extern PRBRCameraInfo		g_pRBRCameraInfo;

extern PRBRCarInfo			g_pRBRCarInfo;
extern PRBRCarControls		g_pRBRCarControls;
extern PRBRMapSettings		g_pRBRMapSettings;
extern PRBRMapSettingsEx    g_pRBRMapSettingsEx;

extern __int32*             g_pRBRGhostCarReplayMode; // 0=No ghost, 1=Saving ghost car movements (during racing), 2=Replying ghost car movements (0x892EEC)
extern PRBRGhostCarMovement g_pRBRGhostCarMovement;
extern PRBRCarMovement		g_pRBRCarMovement;	// Valid only when racing or replay is on
extern PRBRMapInfo			g_pRBRMapInfo;		// Valid only when racing or replay is on

extern PRBRMenuSystem		g_pRBRMenuSystem;

extern PRBRPacenotes		g_pRBRPacenotes;	// Array of pacenotes. Valid only at racetime.

extern wchar_t*				g_pRBRMapLocationName; // Offset 0x007D1D64. The name of the current stage (map, WCHAR string). Valid only at racetime.
extern PRBRProfile			g_pRBRProfile;		   // Offset 0x007D2554. The name of the current driver profile

extern PRBRColorTable		g_pRBRColorTable;	   // Offset 0x007C3668

extern PRBRStatusText		g_pRBRStatusText;	   // Offset 0x007d1d50

//
// TODO:
//
//  0x008EF660 
//    +0x1100 long. Current gear 0..7? There is already gear value in above shown structures. What is this one?
//   
//  0x165F10F long. zero vs non-zero value. Clutch or gear related? Or engine started? What is this?
//   
//  How to handle mouse or keyboard events (mouse/kdb msg re-routing) during gameplay or replay?
//  

#endif
