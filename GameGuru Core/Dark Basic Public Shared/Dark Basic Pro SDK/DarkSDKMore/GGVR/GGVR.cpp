//
// GGVR - VR System Handling
//

// Common Includes
#include "globstruct.h"
#include "GGVR.h"
#include "stdafx.h"
#include <string>
#include "openvr.h"
#include "CCameraC.h"
#include "CImageC.h"
#include "CObjectsC.h"
#include "dxdiag.h"
#include "D3dx9math.h"
#include "BlitzTerrain.h"
#include "CGfxC.h"
#include "CSystemC.h"

#include <iostream>
#include <exception>
using namespace std;

// Externs
extern GlobStruct*				g_pGlob;
extern LPGGDEVICE				m_pD3D;
extern LPGGIMMEDIATECONTEXT		m_pImmediateContext;
extern LPGGSURFACE				g_pBackBuffer;
extern LPGGRENDERTARGETVIEW		m_pRenderTargetView;

// Constants
const float GGVR_SINGULARITYRADIUS = 0.0000001f;
const float GGVR_180DIVPI = 57.2957795130823f;
const float GGVR_PI = 3.14159265358979f;
const float GGVR_PIOVER2 = 0.5f * GGVR_PI;
const float GGVR_radian = 0.0174533f;
const int GGVR_LEye = 0;
const int GGVR_REye = 1; 
const int GGVR_LEye_Depth = 2;
const int GGVR_REye_Depth = 3;

// Reccomended Texture Sizes
uint32_t						GGVR_EyeW;
uint32_t						GGVR_EyeH;

// Pointers to Render Images
ID3D11Resource*					GGVR_LEyeImage_Res;
ID3D11Resource*					GGVR_REyeImage_Res;
ID3D11Resource*					GGVR_Skybox_Res[6];
ID3D11Texture2D*				GGVR_LEyeImage;
ID3D11Texture2D*				GGVR_REyeImage;
ID3D11Texture2D*				GGVR_Skybox[6];
int								GGVR_REyeImageID;
int								GGVR_LEyeImageID;
int								GGVR_REyeImageID_Depth = -1;
int								GGVR_LEyeImageID_Depth = -1;
int								GGVR_RCamID;
int								GGVR_LCamID;

// Raw Projection Matrix Values and camera clipping ranges
float							GGVR_LeyeProj_Top, GGVR_LeyeProj_Bot, GGVR_LeyeProj_Left, GGVR_LeyeProj_Right;
float							GGVR_ReyeProj_Top, GGVR_ReyeProj_Bot, GGVR_ReyeProj_Left, GGVR_ReyeProj_Right;
float							GGVR_NearClip = 1.0f;
float							GGVR_FarClip = 70000.0f;
float							ProjectionRaw[2][4];

// Index for Devices
uint32_t						GGVR_HMD = 0;
uint32_t						GGVR_RHandIndex = -1;
uint32_t						GGVR_LHandIndex = -1;
int								GGVR_AxisType[2][5];
int								GGVR_RTrigger = -1;
int								GGVR_LTrigger = -1;
int								GGVR_RJoystick = -1;
int								GGVR_LJoystick = -1;
int								GGVR_LSideButton = -1;
int								GGVR_RSideButton = -1;

// Controller Button Presses
bool							GGVR_bTouchPadIsRightHand = false;
bool							GGVR_bTouchPadTouched = false;
bool							GGVR_bTouchPadPressed = false;
bool							GGVR_bTouchPadNeedToRelease = false;
float							GGVR_fTouchPadX = 0.0f;
float							GGVR_fTouchPadY = 0.0f;
float							GGVR_fTelePortDestinationX = 0.0f;
float							GGVR_fTelePortDestinationY = 0.0f;
float							GGVR_fTelePortDestinationZ = 0.0f;
bool							GGVR_RCntrlBut_Press[64];
bool							GGVR_RCntrlBut_Touch[64];
bool							GGVR_LCntrlBut_Press[64];
bool							GGVR_LCntrlBut_Touch[64];

// Camera Reference
float							GGVR_WorldScale = 39.3700787f;
float							GGVR_ipdscale = 1.0f;
GGMATRIX						GGVR_LeftEyeProjection;
GGMATRIX						GGVR_RightEyeProjection;

class GGVR_PlayerData
{
public:
	int ObjBase;
	int ObjHead;
	int ObjOrigin;
	int ObjRightHand;
	int ObjLeftHand;
	int ObjTeleportStart;
	int ObjTeleportFinish;
	int ShaderID;
	int TextureID;
	int ControllerShaderID;
	int ControllerTextureID0;
	int ControllerTextureID1;
	int ControllerTextureID2;
	int ControllerTextureID3;
	int ControllerTextureID4;
	int ControllerTextureID5;
	int ControllerTextureID6;
	int Create ( LPSTR pRootDir );
	void Destroy();

	int GetBaseObjID();
	int GetHeadObjID();
	int GetOriginObjID();
	int GetRHandObjID();
	int GetLHandObjID();

	bool PitchLock;
	bool TurnLock;
	int LaserGuideActive;
};
GGVR_PlayerData					GGVR_Player;

// VR System
int								GGVR_EnabledMode = 0;
int								GGVR_EnabledState = 0;
int								GGVR_TrackingSpace = 0;

// WMR (Microsoft)
HMODULE hGGWMRDLL = NULL;
typedef int (*sGGWMR_CreateHolographicSpace1Fnc)(HWND,int); sGGWMR_CreateHolographicSpace1Fnc GGWMR_CreateHolographicSpace1 = NULL;
typedef void (*sGGWMR_ReconnectWithHolographicSpaceControllersFnc)(void); sGGWMR_ReconnectWithHolographicSpaceControllersFnc GGWMR_ReconnectWithHolographicSpaceControllers = NULL;
typedef int (*sGGWMR_CreateHolographicSpace2Fnc)(void*,void*); sGGWMR_CreateHolographicSpace2Fnc GGWMR_CreateHolographicSpace2 = NULL;
typedef void (*sGGWMR_GetUpdateFnc)(void); sGGWMR_GetUpdateFnc GGWMR_GetUpdate = NULL;
typedef void (*sGGWMR_GetHeadPosAndDirFnc)(float*,float*,float*,float*,float*,float*,float*,float*,float*); sGGWMR_GetHeadPosAndDirFnc GGWMR_GetHeadPosAndDir = NULL;
typedef void (*sGGWMR_GetProjectionMatrixFnc)(int,float*,float*,float*,float*,float*,float*,float*,float*,float*,float*,float*,float*,float*,float*,float*,float*); sGGWMR_GetProjectionMatrixFnc GGWMR_GetProjectionMatrix = NULL;
typedef void (*sGGWMR_GetThumbAndTriggerFnc)(float*,float*,float*,float*); sGGWMR_GetThumbAndTriggerFnc GGWMR_GetThumbAndTrigger = NULL;
typedef void (*sGGWMR_GetTouchPadDataFnc)(bool*,bool*,bool*,float*,float*); sGGWMR_GetTouchPadDataFnc GGWMR_GetTouchPadData = NULL;
typedef void (*sGGWMR_GetHandPosAndOrientationFnc)(int,float*,float*,float*,float*,float*,float*,float*); sGGWMR_GetHandPosAndOrientationFnc GGWMR_GetHandPosAndOrientation = NULL;
typedef void (*sGGWMR_GetRenderTargetAndDepthStencilViewFnc)(void**,void**,void**,DWORD*,DWORD*,void**); sGGWMR_GetRenderTargetAndDepthStencilViewFnc GGWMR_GetRenderTargetAndDepthStencilView = NULL;
typedef void (*sGGWMR_PresentFnc)(void); sGGWMR_PresentFnc GGWMR_Present = NULL;

// OpenVR (Steam)
#ifdef USINGOPENVR
bool							GGVR_ErrorMessageOn = true;
std::string						GGVR_HMD_TrackingSysName = "NULL";
std::string						GGVR_HMD_ModelNo = "NULL";
std::string						GGVR_HMD_SerialNo = "NULL";
std::string						GGVR_HMD_Manufacturer = "NULL";
std::string						GGVR_RCntrl_TrackingSysName = "NULL";
std::string						GGVR_RCntrl_ModelNo = "NULL";
std::string						GGVR_RCntrl_SerialNo = "NULL";
std::string						GGVR_RCntrl_Manufacturer = "NULL";
std::string						GGVR_LCntrl_TrackingSysName = "NULL";
std::string						GGVR_LCntrl_ModelNo = "NULL";
std::string						GGVR_LCntrl_SerialNo = "NULL";
std::string						GGVR_LCntrl_Manufacturer = "NULL";
vr::IVRSystem					*ivr_system;
vr::EVRInitError				GGVR_init_error;
vr::EVRApplicationType			GGVR_ApplicationType = vr::VRApplication_Scene;
vr::IVRCompositor				*ivr_compositor;
vr::EVRCompositorError			GGVR_CompositorError;
vr::IVRChaperone				*Chaperone;
vr::EVRSubmitFlags				GGVR_SubmitFlags = vr::Submit_Default;
vr::TrackedDevicePose_t			pRenderPoseArray[vr::k_unMaxTrackedDeviceCount];
vr::HmdVector3_t				pRenderPoseArray_YPR[vr::k_unMaxTrackedDeviceCount];
vr::HmdVector3_t				pRenderPoseArray_Pos[vr::k_unMaxTrackedDeviceCount];
vr::Texture_t					GGVR_tex_info[2];
vr::VRTextureBounds_t			GGVR_texture_bounds[2];
vr::Texture_t					GGVR_CustomSkybox[6];
#endif

// reuse these data structures
vr::TrackedDevicePose_t			pGamePoseArray[vr::k_unMaxTrackedDeviceCount];
vr::HmdVector3_t				pGamePoseArray_YPR[vr::k_unMaxTrackedDeviceCount];
vr::HmdVector3_t				pGamePoseArray_Pos[vr::k_unMaxTrackedDeviceCount];
vr::VRControllerState001_t		GGVR_ControllerState[2];

// DLL Entry (no DLL though!)
BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	return TRUE;
}

// Debugging

int g_iDebuggingActive = 0;
char* g_pGGVRDebugLog[10000];
char g_pDebugAbsPath[1024];
extern void timestampactivity ( int i, char* desc_s );

int DebugGGVRlog ( const char* pReportLog )
{
	// dont use this for now - is it crashing istefl?
	return 0;

	// Log File
	if ( pReportLog == NULL )
	{
		// Reset at VERY start
		for ( int iFind = 0; iFind < 10000; iFind++ ) g_pGGVRDebugLog[iFind] = NULL; 
		strcpy ( g_pDebugAbsPath, "" );
		return 0;
	}
	if ( g_iDebuggingActive == 1 )
	{
		// the debug file
		const char* pFilename = "GGVRDebugLog.log";

		// First use of debug, get absolute path to log file
		if ( strcmp ( g_pDebugAbsPath, "" ) == NULL )
		{
			GetCurrentDirectoryA ( 1024, g_pDebugAbsPath );
		}

		// New entry 
		char pWithTime[2048];
		sprintf_s ( pWithTime, "%d : %s\r\n", timeGetTime(), pReportLog );
		DWORD dwNewEntry = strlen(pWithTime);

		// Find new slot
		for ( int iFind = 0; iFind < 10000; iFind++ )
		{
			if ( g_pGGVRDebugLog[iFind] == NULL ) 
			{
				g_pGGVRDebugLog[iFind] = new char[2048];
				memset ( g_pGGVRDebugLog[iFind], 0, 2048 );
				strcpy_s ( g_pGGVRDebugLog[iFind], 2048, pWithTime );
				break;
			}
		}

		// save new log file
		DWORD bytesdone = 0;
		char pAbsFilename[1024];
		strcpy ( pAbsFilename, g_pDebugAbsPath );
		strcat ( pAbsFilename, "\\" );
		strcat ( pAbsFilename, pFilename );
		HANDLE hFile = CreateFile(pAbsFilename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if ( hFile != INVALID_HANDLE_VALUE )
		{
			for ( int iFind = 0; iFind < 10000; iFind++ )
			{
				if ( g_pGGVRDebugLog[iFind] != NULL ) 
				{
					WriteFile(hFile, g_pGGVRDebugLog[iFind], strlen(g_pGGVRDebugLog[iFind]), &bytesdone, FALSE);
				}
			}
			CloseHandle ( hFile );
		}
		else
		{
			// error - cannot get write lock on this file!
			return 1;
		}
	}

	// success
	return 0;
}

// Support Functions

std::string GetTrackedDeviceString(vr::IVRSystem *pHmd, vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError *peError = NULL)
{
	uint32_t unRequiredBufferLen = pHmd->GetStringTrackedDeviceProperty(unDevice, prop, NULL, 0, peError);
	if (unRequiredBufferLen == 0)
		return "";

	char *pchBuffer = new char[unRequiredBufferLen];
	unRequiredBufferLen = pHmd->GetStringTrackedDeviceProperty(unDevice, prop, pchBuffer, unRequiredBufferLen, peError);
	std::string sResult = pchBuffer;
	delete[] pchBuffer;
	return sResult;
}

void GGVR_Mat34toYPR(vr::HmdMatrix34_t *Mat, vr::HmdVector3_t *YPR, vr::HmdVector3_t *Pos)
{
	vr::HmdVector3_t r;
	vr::HmdVector3_t f;
	vr::HmdVector3_t u;

	r.v[0] = Mat->m[0][0]; r.v[1] = -Mat->m[2][0]; r.v[2] = Mat->m[1][0];
	f.v[0] = -Mat->m[0][2]; f.v[1] = Mat->m[2][2]; f.v[2] = -Mat->m[1][2];
	u.v[0] = Mat->m[0][1]; u.v[1] = -Mat->m[2][1]; u.v[2] = Mat->m[1][1];

	if (f.v[2] < -1.0f + GGVR_SINGULARITYRADIUS) // down
	{
		YPR->v[1] = 0.0f;
		YPR->v[0] = -GGVR_PIOVER2;
		YPR->v[2] = atan2f(u.v[0], u.v[1]);
	}
	else if (f.v[2] > 1.0f - GGVR_SINGULARITYRADIUS) // up
	{
		YPR->v[1] = 0.0f;
		YPR->v[0] = GGVR_PIOVER2;
		YPR->v[2] = GGVR_PI - atan2f(u.v[0], u.v[1]);
	}
	else // usual case
	{
		YPR->v[1] = atan2f(-f.v[0], f.v[1]);
		YPR->v[0] = asinf(f.v[2]);
		YPR->v[2] = -atan2f(r.v[2], u.v[2]);
	}

	YPR->v[1] = -YPR->v[1] * GGVR_180DIVPI;
	YPR->v[0] = -YPR->v[0] * GGVR_180DIVPI;
	YPR->v[2] = -YPR->v[2] * GGVR_180DIVPI;
	Pos->v[0] = Mat->m[0][3];
	Pos->v[1] = Mat->m[1][3];
	Pos->v[2] = -Mat->m[2][3];
}

// Generic
int GGVR_ChooseVRSystem ( int iGGVREnabledMode, int iDebuggingActive, LPSTR pAbsPathToDLL )
{
	// Debug flag can only be zero or one
	if ( iDebuggingActive > 1 ) iDebuggingActive = 1;

	// Assign VR System Mode to Use
	g_iDebuggingActive = iDebuggingActive;
	//DebugGGVRlog ( NULL );
	if ( GGVR_EnabledMode == 0 )
	{
		// Set VR System Mode
		GGVR_EnabledMode = iGGVREnabledMode;

		// 1 = OpenVR (Steam)
		if ( GGVR_EnabledMode == 1 )
		{
		}

		// 2 = Microsoft WMR
		if ( GGVR_EnabledMode == 2 )
		{
			hGGWMRDLL = LoadLibrary ( pAbsPathToDLL );//g.fpscrootdir_s+"\\GGWMR.dll" ); // "F:\\GameGuruRepo\\GameGuru\\GGWMR.dll" ); idiot Lee!!
			if ( hGGWMRDLL )
			{
				// get proc calls
				GGWMR_CreateHolographicSpace1 = (sGGWMR_CreateHolographicSpace1Fnc) GetProcAddress ( hGGWMRDLL, "GGWMR_CreateHolographicSpace1" );
				GGWMR_ReconnectWithHolographicSpaceControllers = (sGGWMR_ReconnectWithHolographicSpaceControllersFnc) GetProcAddress ( hGGWMRDLL, "GGWMR_ReconnectWithHolographicSpaceControllers" );
				GGWMR_CreateHolographicSpace2 = (sGGWMR_CreateHolographicSpace2Fnc) GetProcAddress ( hGGWMRDLL, "GGWMR_CreateHolographicSpace2" );
				GGWMR_GetUpdate = (sGGWMR_GetUpdateFnc) GetProcAddress ( hGGWMRDLL, "GGWMR_GetUpdate" );
				GGWMR_GetHeadPosAndDir = (sGGWMR_GetHeadPosAndDirFnc) GetProcAddress ( hGGWMRDLL, "GGWMR_GetHeadPosAndDir" );
				GGWMR_GetProjectionMatrix = (sGGWMR_GetProjectionMatrixFnc) GetProcAddress ( hGGWMRDLL, "GGWMR_GetProjectionMatrix" );
				GGWMR_GetThumbAndTrigger = (sGGWMR_GetThumbAndTriggerFnc) GetProcAddress ( hGGWMRDLL, "GGWMR_GetThumbAndTrigger" );
				GGWMR_GetTouchPadData = (sGGWMR_GetTouchPadDataFnc) GetProcAddress ( hGGWMRDLL, "GGWMR_GetTouchPadData" );
				GGWMR_GetHandPosAndOrientation = (sGGWMR_GetHandPosAndOrientationFnc) GetProcAddress ( hGGWMRDLL, "GGWMR_GetHandPosAndOrientation" );
				GGWMR_GetRenderTargetAndDepthStencilView = (sGGWMR_GetRenderTargetAndDepthStencilViewFnc) GetProcAddress ( hGGWMRDLL, "GGWMR_GetRenderTargetAndDepthStencilView" );
				GGWMR_Present = (sGGWMR_PresentFnc) GetProcAddress ( hGGWMRDLL, "GGWMR_Present" );

				// confirm proc exists
				//if ( GGWMR_CreateHolographicSpace1 ) //DebugGGVRlog ( "CreateHolographicSpace1 Valid" );
			}
		}
	}

	// to prevent first appearance of WMR portal wiping out directsound device, do this early
	int iErrorCode = GGVR_PreSubmit(2+g_iDebuggingActive);
	if ( iErrorCode == 0 ) iErrorCode = GGVR_PreSubmit(2+g_iDebuggingActive);
	if ( iErrorCode > 0 ) return iErrorCode;

	// no error code
	return 0;
}

int	GGVR_IsHmdPresent()
{
	// Type:
	// 1 - OpenVR
	// 2 - WMR
	int result = 0;
	#ifdef USINGOPENVR
	if ( GGVR_EnabledMode == 1 ) result = vr::VR_IsHmdPresent();
	#endif
	if ( GGVR_EnabledMode == 2 && GGVR_EnabledState != 3 ) result = 2;
	return result;
}

void GGVR_SetTrackingSpace( int space )
{
	// 0 = seated
	// 1 = standing
	GGVR_TrackingSpace = space;
}

int GGVR_GetTrackingSpace ( void ) 
{
	return GGVR_TrackingSpace;
}

int	GGVR_Init ( LPSTR pRootDir, int RImageID, int LImageID, int RCamID, int LCamID, int ObjBase, int ObjHead, int ObjOrigin, int ObjRightHand, int ObjLeftHand, int iObjTeleportStart, int iObjTeleportFinish, int iShaderID, int iTextureID, int iCShaderID, int iCTextureID0, int iCTextureID1, int iCTextureID2, int iCTextureID3, int iCTextureID4, int iCTextureID5, int iCTextureID6)
{
	//Set Onject ID's for Player dummy objects
	GGVR_Player.ObjBase = ObjBase;
	GGVR_Player.ObjHead = ObjHead;
	GGVR_Player.ObjOrigin = ObjOrigin;
	GGVR_Player.ObjRightHand = ObjRightHand;
	GGVR_Player.ObjLeftHand = ObjLeftHand;
	GGVR_Player.ObjTeleportStart = iObjTeleportStart;
	GGVR_Player.ObjTeleportFinish = iObjTeleportFinish;

	GGVR_Player.ShaderID = iShaderID;//g.guishadereffectindex
	GGVR_Player.TextureID = iTextureID;//g.editorimagesoffset+14

	GGVR_Player.ControllerShaderID = iCShaderID;
	GGVR_Player.ControllerTextureID0 = iCTextureID0;
	GGVR_Player.ControllerTextureID1 = iCTextureID1;
	GGVR_Player.ControllerTextureID2 = iCTextureID2;
	GGVR_Player.ControllerTextureID3 = iCTextureID3;
	GGVR_Player.ControllerTextureID4 = iCTextureID4;
	GGVR_Player.ControllerTextureID5 = iCTextureID5;
	GGVR_Player.ControllerTextureID6 = iCTextureID6;

	//Create Cameras
	GGVR_RCamID = RCamID;
	GGVR_LCamID = LCamID;
	if (CameraExist(RCamID) == 0)
	{
		CreateCamera(RCamID);
	}
	SetCameraRange(RCamID, GGVR_NearClip, GGVR_FarClip);
	if (CameraExist(LCamID) == 0)
	{
		CreateCamera(LCamID);
	}
	SetCameraRange(LCamID, GGVR_NearClip, GGVR_FarClip);

	// Initialise VR System
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		GGVR_init_error = vr::VRInitError_None;
		//Initialize OpenVR
		ivr_system = vr::VR_Init(&GGVR_init_error, GGVR_ApplicationType);
		//Could add ability to change application type, but probably no reason.
		// Default is: vr::VRApplication_Scene
		//Other options:
		//	vr::VRApplication_Other
		//	vr::VRApplication_Overlay
		//	vr::VRApplication_Background
		//	vr::VRApplication_Utility
		if (GGVR_init_error != vr::VRInitError_None)
		{
			if (GGVR_ErrorMessageOn == true)
			{
				char buf[1024];
				sprintf_s(buf, sizeof(buf), "Unable to init VR runtime: %s", vr::VR_GetVRInitErrorAsEnglishDescription(GGVR_init_error));
				MessageBox(GetForegroundWindow(), buf, "VR Error", MB_OK | MB_ICONEXCLAMATION | MB_TOPMOST);
			}
			return 1;
		}
		//Initialize the Compositor
		ivr_compositor = vr::VRCompositor();
		if (ivr_compositor == NULL)
		{
			ivr_system = NULL;
			vr::VR_Shutdown();
			if (GGVR_ErrorMessageOn == true)
			{
				char buf[1024];
				sprintf_s(buf, sizeof(buf), "Compositor initialization failed.");
				MessageBox(GetForegroundWindow(), buf, "VR Error", MB_OK | MB_ICONEXCLAMATION | MB_TOPMOST);
			}
			return 2;
		}
		//Chaperone
		Chaperone = vr::VRChaperone();

		//Info Strings for the VR Hardware
		//-----------------------------------
		GGVR_HMD_TrackingSysName = GetTrackedDeviceString(ivr_system, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String, NULL);
		GGVR_HMD_ModelNo = GetTrackedDeviceString(ivr_system, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ModelNumber_String, NULL);
		GGVR_HMD_SerialNo = GetTrackedDeviceString(ivr_system, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String, NULL);
		GGVR_HMD_Manufacturer = GetTrackedDeviceString(ivr_system, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ManufacturerName_String, NULL);
		//Reccomended Texture Size
		ivr_system->GetRecommendedRenderTargetSize(&GGVR_EyeW, &GGVR_EyeH);
		//Set Submit Flags
		GGVR_SubmitFlags = vr::Submit_Default;
		//textureInfo
		GGVR_tex_info[GGVR_LEye].eType = vr::TextureType_DirectX;
		GGVR_tex_info[GGVR_REye].eType = vr::TextureType_DirectX;
		GGVR_tex_info[GGVR_LEye].eColorSpace = vr::ColorSpace_Auto;
		GGVR_tex_info[GGVR_REye].eColorSpace = vr::ColorSpace_Auto;
		//Texture Bounds
		GGVR_texture_bounds[GGVR_REye].uMin = 0;
		GGVR_texture_bounds[GGVR_REye].uMax = 1;
		GGVR_texture_bounds[GGVR_REye].vMin = 0;
		GGVR_texture_bounds[GGVR_REye].vMax = 1;
		GGVR_texture_bounds[GGVR_LEye].uMin = 0;
		GGVR_texture_bounds[GGVR_LEye].uMax = 1;
		GGVR_texture_bounds[GGVR_LEye].vMin = 0;
		GGVR_texture_bounds[GGVR_LEye].vMax = 1;
		#endif
	}
	if ( GGVR_EnabledMode == 2 )
	{
		// initial settings - small images, going to delete and supplant with direct WMR views eventually
		GGVR_EyeW = 32;
		GGVR_EyeH = 32;

		// force a reset of hand pos coordinates so controller can be hidden if not used in session
		float fRightHandX = 0.0f;
		float fRightHandY = 0.0f;
		float fRightHandZ = 0.0f;
		float fQuatW = 0.0f;
		float fQuatX = 0.0f;
		float fQuatY = 0.0f;
		float fQuatZ = 0.0f;
		GGWMR_GetHandPosAndOrientation ( -1, &fRightHandX, &fRightHandY, &fRightHandZ, &fQuatW, &fQuatX, &fQuatY, &fQuatZ );
	}

	//Create Render Images
	GGVR_REyeImageID = RImageID;
	GGVR_LEyeImageID = LImageID;
	SetCameraToImage(GGVR_RCamID, GGVR_REyeImageID, GGVR_EyeW, GGVR_EyeH);
	SetCameraToImage(GGVR_LCamID, GGVR_LEyeImageID, GGVR_EyeW, GGVR_EyeH);
	GGVR_REyeImage_Res = GetImagePointer(GGVR_REyeImageID);
	GGVR_LEyeImage_Res = GetImagePointer(GGVR_LEyeImageID);
	GGVR_REyeImage_Res->QueryInterface(IID_ID3D11Texture2D, (void **)&GGVR_REyeImage);
	GGVR_LEyeImage_Res->QueryInterface(IID_ID3D11Texture2D, (void **)&GGVR_LEyeImage);

	// Get the Projection Matrix values
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		GGVR_tex_info[GGVR_LEye].handle = (void**)GGVR_LEyeImage;
		GGVR_tex_info[GGVR_REye].handle = (void**)GGVR_REyeImage;
		vr::HmdMatrix44_t HMDMat_Left;
		vr::HmdMatrix44_t HMDMat_Right;
		HMDMat_Left = ivr_system->GetProjectionMatrix(vr::Eye_Left, GGVR_NearClip, GGVR_FarClip);
		HMDMat_Right = ivr_system->GetProjectionMatrix(vr::Eye_Right, GGVR_NearClip, GGVR_FarClip);
		GGVR_LeftEyeProjection._11 = HMDMat_Left.m[0][0]; GGVR_LeftEyeProjection._12 = HMDMat_Left.m[0][1]; GGVR_LeftEyeProjection._13 = HMDMat_Left.m[0][2]; GGVR_LeftEyeProjection._14 = HMDMat_Left.m[0][3];
		GGVR_LeftEyeProjection._21 = HMDMat_Left.m[1][0]; GGVR_LeftEyeProjection._22 = HMDMat_Left.m[1][1]; GGVR_LeftEyeProjection._23 = HMDMat_Left.m[1][2]; GGVR_LeftEyeProjection._24 = HMDMat_Left.m[1][3];
		GGVR_LeftEyeProjection._31 = HMDMat_Left.m[2][0]; GGVR_LeftEyeProjection._32 = HMDMat_Left.m[2][1]; GGVR_LeftEyeProjection._33 = HMDMat_Left.m[2][2]; GGVR_LeftEyeProjection._34 = HMDMat_Left.m[2][3];
		GGVR_LeftEyeProjection._41 = HMDMat_Left.m[3][0]; GGVR_LeftEyeProjection._42 = HMDMat_Left.m[3][1]; GGVR_LeftEyeProjection._43 = HMDMat_Left.m[3][2]; GGVR_LeftEyeProjection._44 = HMDMat_Left.m[3][3];
		GGVR_RightEyeProjection._11 = HMDMat_Right.m[0][0]; GGVR_RightEyeProjection._12 = HMDMat_Right.m[0][1]; GGVR_RightEyeProjection._13 = HMDMat_Right.m[0][2]; GGVR_RightEyeProjection._14 = HMDMat_Right.m[0][3];
		GGVR_RightEyeProjection._21 = HMDMat_Right.m[1][0]; GGVR_RightEyeProjection._22 = HMDMat_Right.m[1][1]; GGVR_RightEyeProjection._23 = HMDMat_Right.m[1][2]; GGVR_RightEyeProjection._24 = HMDMat_Right.m[1][3];
		GGVR_RightEyeProjection._31 = HMDMat_Right.m[2][0]; GGVR_RightEyeProjection._32 = HMDMat_Right.m[2][1]; GGVR_RightEyeProjection._33 = HMDMat_Right.m[2][2]; GGVR_RightEyeProjection._34 = HMDMat_Right.m[2][3];
		GGVR_RightEyeProjection._41 = HMDMat_Right.m[3][0]; GGVR_RightEyeProjection._42 = HMDMat_Right.m[3][1]; GGVR_RightEyeProjection._43 = HMDMat_Right.m[3][2]; GGVR_RightEyeProjection._44 = HMDMat_Right.m[3][3];
		#endif
	}
	if ( GGVR_EnabledMode == 2 )
	{
		// done in realtime
	}
	return GGVR_Player.Create(pRootDir);
}

void GGVR_Shutdown()
{
	// Free any playerdata object resources
	GGVR_Player.Destroy();

	// Free camera images associated with VR rendering
	SetCameraToImage(GGVR_RCamID, -1, 0, 0);
	SetCameraToImage(GGVR_LCamID, -1, 0, 0);

	// final shutdown of VR system
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		vr::VR_Shutdown();
		#endif
	}
	if ( GGVR_EnabledMode == 2 )
	{
		// keep WMR holographic space active throughout session (cleaner than shutting it down)
	}
}

GGMATRIX GGVR_GetRightEyeProjectionMatrix()
{
	return GGVR_RightEyeProjection;
}

GGMATRIX GGVR_GetLeftEyeProjectionMatrix()
{
	return GGVR_LeftEyeProjection;
}

void GGVR_SetWorldScale( float scale )
{
	GGVR_WorldScale = scale;
}

float GGVR_GetWorldScale(  )
{
	return GGVR_WorldScale;
}

int GGVR_GetPlayerOriginObjID( )
{
	return GGVR_Player.GetOriginObjID( );
}

int GGVR_GetPlayerHeadObjID( )
{
	return GGVR_Player.GetHeadObjID( );
}

int GGVR_GetPlayerBaseObjID( )
{
	return GGVR_Player.GetBaseObjID( );
}

int GGVR_GetPlayerRightHandObjID( )
{
	return GGVR_Player.GetRHandObjID( );
}

int GGVR_GetPlayerLeftHandObjID( )
{
	return GGVR_Player.GetLHandObjID( );
}

void GGVR_UpdatePoses(void)
{
	// Debug Info
	char pInfo[1024];
	sprintf ( pInfo, "Info: EnabledMode:%d  EnabledState:%d", GGVR_EnabledMode, GGVR_EnabledState );
	timestampactivity ( 0, pInfo );
	//DebugGGVRlog ( pInfo );

	// Handles HMD orientation and Motion Controller handling
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		if (GGVR_init_error == vr::VRInitError_None && ivr_compositor != NULL)
		{
			//Update the current orientation of the HMD's and Controllers
			GGVR_CompositorError = ivr_compositor->WaitGetPoses(pGamePoseArray, vr::k_unMaxTrackedDeviceCount, pRenderPoseArray, vr::k_unMaxTrackedDeviceCount);
			for (int i = 0; i < vr::k_unMaxTrackedDeviceCount; i++)
			{
				if (pGamePoseArray[i].bPoseIsValid == true)
				{
					GGVR_Mat34toYPR(&pGamePoseArray[i].mDeviceToAbsoluteTracking, &pGamePoseArray_YPR[i], &pGamePoseArray_Pos[i]);
				}
				if (pRenderPoseArray[i].bPoseIsValid == true)
				{
					GGVR_Mat34toYPR(&pRenderPoseArray[i].mDeviceToAbsoluteTracking, &pRenderPoseArray_YPR[i], &pRenderPoseArray_Pos[i]);
				}
			}

			//Search for hand controllers if not already found, otherwise update the button presses
			//Right Hand
			if (GGVR_RHandIndex < 1 || GGVR_RHandIndex > vr::k_unMaxTrackedDeviceCount)
			{
				GGVR_RHandIndex = ivr_system->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
				vr::ETrackedPropertyError pError;
				if (GGVR_RHandIndex != 0) // Index for controller was found, so get info about controller
				{
					//Info Strings for the Right Controller - 2017-11-02: Added device info
					//--------------------------------------
					GGVR_RCntrl_TrackingSysName = GetTrackedDeviceString(ivr_system, GGVR_RHandIndex, vr::Prop_TrackingSystemName_String, NULL);
					GGVR_RCntrl_ModelNo = GetTrackedDeviceString(ivr_system, GGVR_RHandIndex, vr::Prop_ModelNumber_String, NULL);
					GGVR_RCntrl_SerialNo = GetTrackedDeviceString(ivr_system, GGVR_RHandIndex, vr::Prop_SerialNumber_String, NULL);
					GGVR_RCntrl_Manufacturer = GetTrackedDeviceString(ivr_system, GGVR_RHandIndex, vr::Prop_ManufacturerName_String, NULL);

					GGVR_AxisType[0][0] = ivr_system->GetInt32TrackedDeviceProperty(GGVR_RHandIndex, vr::Prop_Axis0Type_Int32, &pError);
					GGVR_AxisType[0][1] = ivr_system->GetInt32TrackedDeviceProperty(GGVR_RHandIndex, vr::Prop_Axis1Type_Int32, &pError);
					GGVR_AxisType[0][2] = ivr_system->GetInt32TrackedDeviceProperty(GGVR_RHandIndex, vr::Prop_Axis2Type_Int32, &pError);
					GGVR_AxisType[0][3] = ivr_system->GetInt32TrackedDeviceProperty(GGVR_RHandIndex, vr::Prop_Axis3Type_Int32, &pError);
					GGVR_AxisType[0][4] = ivr_system->GetInt32TrackedDeviceProperty(GGVR_RHandIndex, vr::Prop_Axis4Type_Int32, &pError);

					//Now find specific Axis info:
					for (int i = 0; i < 5; ++i)
					{
						//Look for trigger
						if (GGVR_AxisType[0][i] == vr::k_eControllerAxis_Trigger && GGVR_RTrigger == -1)
						{
							GGVR_RTrigger = i;
						}

						//Look for Joystick
						if (GGVR_AxisType[0][i] == vr::k_eControllerAxis_TrackPad)
						{
							GGVR_RJoystick = i;
						}
						if (GGVR_AxisType[0][i] == vr::k_eControllerAxis_Joystick)
						{
							GGVR_RJoystick = i;
						}
					}
				}
			}
			else
			{ 
				ivr_system->GetControllerState(GGVR_RHandIndex, &GGVR_ControllerState[0], sizeof(vr::VRControllerState001_t));
			}
	
			//Left Hand
			if (GGVR_LHandIndex < 1 || GGVR_LHandIndex > vr::k_unMaxTrackedDeviceCount)
			{
				GGVR_LHandIndex = ivr_system->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
				vr::ETrackedPropertyError pError;
				if (GGVR_LHandIndex != 0) // Index for controller was found, so get info about controller
				{
					//Info Strings for the Right Controller - 2017-11-02: Added device info
					//--------------------------------------
					GGVR_LCntrl_TrackingSysName = GetTrackedDeviceString(ivr_system, GGVR_LHandIndex, vr::Prop_TrackingSystemName_String, NULL);
					GGVR_LCntrl_ModelNo = GetTrackedDeviceString(ivr_system, GGVR_LHandIndex, vr::Prop_ModelNumber_String, NULL);
					GGVR_LCntrl_SerialNo = GetTrackedDeviceString(ivr_system, GGVR_LHandIndex, vr::Prop_SerialNumber_String, NULL);
					GGVR_LCntrl_Manufacturer = GetTrackedDeviceString(ivr_system, GGVR_LHandIndex, vr::Prop_ManufacturerName_String, NULL);

					GGVR_AxisType[1][0] = ivr_system->GetInt32TrackedDeviceProperty(GGVR_LHandIndex, vr::Prop_Axis0Type_Int32, &pError);
					GGVR_AxisType[1][1] = ivr_system->GetInt32TrackedDeviceProperty(GGVR_LHandIndex, vr::Prop_Axis1Type_Int32, &pError);
					GGVR_AxisType[1][2] = ivr_system->GetInt32TrackedDeviceProperty(GGVR_LHandIndex, vr::Prop_Axis2Type_Int32, &pError);
					GGVR_AxisType[1][3] = ivr_system->GetInt32TrackedDeviceProperty(GGVR_LHandIndex, vr::Prop_Axis3Type_Int32, &pError);
					GGVR_AxisType[1][4] = ivr_system->GetInt32TrackedDeviceProperty(GGVR_LHandIndex, vr::Prop_Axis4Type_Int32, &pError);

					//Now find specific Axis info:
					for (int i = 0; i < 5; ++i)
					{
						//Look for trigger
						if (GGVR_AxisType[1][i] == vr::k_eControllerAxis_Trigger && GGVR_LTrigger == -1)
						{
							GGVR_LTrigger = i;
						}

						//Look for Joystick
						if (GGVR_AxisType[1][i] == vr::k_eControllerAxis_TrackPad)
						{
							GGVR_LJoystick = i;
						}
						if (GGVR_AxisType[1][i] == vr::k_eControllerAxis_Joystick)
						{
							GGVR_LJoystick = i;
						}
					}
				}
			}
			else
			{
				ivr_system->GetControllerState(GGVR_LHandIndex, &GGVR_ControllerState[1], sizeof(vr::VRControllerState001_t));
			}
		}
		#endif
	}
	if ( GGVR_EnabledMode == 2 )
	{
		if ( GGVR_EnabledState == 2 )
		{
			// WMR Tracking
			timestampactivity ( 0, "TS: Calling GGWMR_GetUpdate" );
			//DebugGGVRlog ( "Calling GGWMR_GetUpdate" );
			///MessageBox ( NULL, "Hard Debug Trace", "0.A", MB_OK );
			GGWMR_GetUpdate();
			pGamePoseArray[0].bPoseIsValid = true;
			if ( pGamePoseArray[0].bPoseIsValid )
			{
				// update the current orientation of the HMD
				float fPosX, fPosY, fPosZ;
				float fUpX, fUpY, fUpZ;
				float fDirX, fDirY, fDirZ;
				timestampactivity ( 0, "TS: Calling GGWMR_GetHeadPosAndDir" );
				//DebugGGVRlog ( "Calling GGWMR_GetHeadPosAndDir" );
				GGWMR_GetHeadPosAndDir ( &fPosX, &fPosY, &fPosZ, &fUpX, &fUpY, &fUpZ, &fDirX, &fDirY, &fDirZ );
				GGMATRIX matTransform;
				KMaths::Vector3 up = KMaths::Vector3(fUpX,fUpY,fUpZ);
				KMaths::Vector3 eye = KMaths::Vector3(fDirX,fDirY,fDirZ);
				KMaths::Vector3 at = KMaths::Vector3(0,0,0);
				GGMatrixLookAtLH ( &matTransform, &eye, &at, &up );
				pGamePoseArray[0].mDeviceToAbsoluteTracking.m[0][0] = matTransform.m[0][0];
				pGamePoseArray[0].mDeviceToAbsoluteTracking.m[1][0] = matTransform.m[1][0];
				pGamePoseArray[0].mDeviceToAbsoluteTracking.m[2][0] = matTransform.m[2][0];
				pGamePoseArray[0].mDeviceToAbsoluteTracking.m[0][1] = matTransform.m[0][1];
				pGamePoseArray[0].mDeviceToAbsoluteTracking.m[1][1] = matTransform.m[1][1];
				pGamePoseArray[0].mDeviceToAbsoluteTracking.m[2][1] = matTransform.m[2][1];
				pGamePoseArray[0].mDeviceToAbsoluteTracking.m[0][2] = matTransform.m[0][2];
				pGamePoseArray[0].mDeviceToAbsoluteTracking.m[1][2] = matTransform.m[1][2];
				pGamePoseArray[0].mDeviceToAbsoluteTracking.m[2][2] = matTransform.m[2][2];
				pGamePoseArray[0].mDeviceToAbsoluteTracking.m[0][3] = fPosX;
				pGamePoseArray[0].mDeviceToAbsoluteTracking.m[1][3] = fPosY;
				pGamePoseArray[0].mDeviceToAbsoluteTracking.m[2][3] = fPosZ;
				GGVR_Mat34toYPR(&pGamePoseArray[0].mDeviceToAbsoluteTracking, &pGamePoseArray_YPR[0], &pGamePoseArray_Pos[0]);
			}

			// Left and Right Hand Controller
			GGVR_RHandIndex = 1;
			GGVR_LHandIndex = 2;
			pGamePoseArray[GGVR_RHandIndex].bPoseIsValid = true;
			pGamePoseArray[GGVR_LHandIndex].bPoseIsValid = true;
			if ( pGamePoseArray[GGVR_RHandIndex].bPoseIsValid )
			{
				// Get Controller data
				float fSideButtonValue = 0.0f;
				float fTriggerValue = 0.0f;
				float fThumbStickX = 0.0f;
				float fThumbStickY = 0.0f;
				timestampactivity ( 0, "TS: Calling GGWMR_GetThumbAndTrigger" );
				//DebugGGVRlog ( "Calling GGWMR_GetThumbAndTrigger" );
				GGWMR_GetThumbAndTrigger ( &fSideButtonValue, &fTriggerValue, &fThumbStickX, &fThumbStickY );

				// Update controller input : GGVR_AxisType[controllerID][typeofinput]
				GGVR_LTrigger = 0;
				GGVR_RTrigger = 0; 
				GGVR_LJoystick = 1;
				GGVR_RJoystick = 1;
				GGVR_LSideButton = 2;
				GGVR_RSideButton = 2;
				for ( int c = 0; c <= 1; c++ )
				{
					GGVR_AxisType[c][0] = 0;
					GGVR_AxisType[c][1] = 1;
					GGVR_AxisType[c][2] = -1;
					GGVR_AxisType[c][3] = -1;
					GGVR_AxisType[c][4] = -1;
					GGVR_ControllerState[c].rAxis[GGVR_LSideButton].x = fSideButtonValue;
					GGVR_ControllerState[c].rAxis[GGVR_LTrigger].x = fTriggerValue;
					GGVR_ControllerState[c].rAxis[GGVR_LJoystick].x = fThumbStickX;
					GGVR_ControllerState[c].rAxis[GGVR_LJoystick].y = fThumbStickY;
				}

				// also need touch pad for teleport action
				GGVR_bTouchPadIsRightHand = false;
				GGVR_bTouchPadTouched = false;
				GGVR_bTouchPadPressed = false;
				GGVR_fTouchPadX = 0.0f;
				GGVR_fTouchPadY = 0.0f;
				timestampactivity ( 0, "TS: Calling GGWMR_GetTouchPadData" );
				//DebugGGVRlog ( "Calling GGWMR_GetTouchPadData" );
				GGWMR_GetTouchPadData ( &GGVR_bTouchPadIsRightHand, &GGVR_bTouchPadTouched, &GGVR_bTouchPadPressed, &GGVR_fTouchPadX, &GGVR_fTouchPadY );

				// also need position/orientation of RIGHT controller
				float fRightHandX = 0.0f;
				float fRightHandY = 0.0f;
				float fRightHandZ = 0.0f;
				float fQuatW = 0.0f;
				float fQuatX = 0.0f;
				float fQuatY = 0.0f;
				float fQuatZ = 0.0f;
				timestampactivity ( 0, "TS: Calling GGWMR_GetHandPosAndOrientation right" );
				//DebugGGVRlog ( "Calling GGWMR_GetHandPosAndOrientation right" );
				GGWMR_GetHandPosAndOrientation ( 0, &fRightHandX, &fRightHandY, &fRightHandZ, &fQuatW, &fQuatX, &fQuatY, &fQuatZ );
				GGMATRIX matRot;
				GGQUATERNION QuatRot = GGQUATERNION ( -fQuatX, -fQuatY, -fQuatZ, fQuatW );
				GGMatrixRotationQuaternion ( &matRot, &QuatRot );
				pGamePoseArray[GGVR_RHandIndex].mDeviceToAbsoluteTracking.m[0][0] = matRot.m[0][0];
				pGamePoseArray[GGVR_RHandIndex].mDeviceToAbsoluteTracking.m[1][0] = matRot.m[1][0];
				pGamePoseArray[GGVR_RHandIndex].mDeviceToAbsoluteTracking.m[2][0] = matRot.m[2][0];
				pGamePoseArray[GGVR_RHandIndex].mDeviceToAbsoluteTracking.m[0][1] = matRot.m[0][1];
				pGamePoseArray[GGVR_RHandIndex].mDeviceToAbsoluteTracking.m[1][1] = matRot.m[1][1];
				pGamePoseArray[GGVR_RHandIndex].mDeviceToAbsoluteTracking.m[2][1] = matRot.m[2][1];
				pGamePoseArray[GGVR_RHandIndex].mDeviceToAbsoluteTracking.m[0][2] = matRot.m[0][2];
				pGamePoseArray[GGVR_RHandIndex].mDeviceToAbsoluteTracking.m[1][2] = matRot.m[1][2];
				pGamePoseArray[GGVR_RHandIndex].mDeviceToAbsoluteTracking.m[2][2] = matRot.m[2][2];
				pGamePoseArray[GGVR_RHandIndex].mDeviceToAbsoluteTracking.m[0][3] = fRightHandX;
				pGamePoseArray[GGVR_RHandIndex].mDeviceToAbsoluteTracking.m[1][3] = fRightHandY;
				pGamePoseArray[GGVR_RHandIndex].mDeviceToAbsoluteTracking.m[2][3] = fRightHandZ;
				GGVR_Mat34toYPR(&pGamePoseArray[GGVR_RHandIndex].mDeviceToAbsoluteTracking, &pGamePoseArray_YPR[GGVR_RHandIndex], &pGamePoseArray_Pos[GGVR_RHandIndex]);

				// also need position/orientation of LEFT controller
				timestampactivity ( 0, "TS: Calling GGWMR_GetHandPosAndOrientation left" );
				//DebugGGVRlog ( "Calling GGWMR_GetHandPosAndOrientation left" );
				float fLeftHandX = 0.0f;
				float fLeftHandY = 0.0f;
				float fLeftHandZ = 0.0f;
				fQuatW = 0.0f;
				fQuatX = 0.0f;
				fQuatY = 0.0f;
				fQuatZ = 0.0f;
				GGWMR_GetHandPosAndOrientation ( 1, &fLeftHandX, &fLeftHandY, &fLeftHandZ, &fQuatW, &fQuatX, &fQuatY, &fQuatZ );
				QuatRot = GGQUATERNION ( -fQuatX, -fQuatY, -fQuatZ, fQuatW );
				GGMatrixRotationQuaternion ( &matRot, &QuatRot );
				pGamePoseArray[GGVR_LHandIndex].mDeviceToAbsoluteTracking.m[0][0] = matRot.m[0][0];
				pGamePoseArray[GGVR_LHandIndex].mDeviceToAbsoluteTracking.m[1][0] = matRot.m[1][0];
				pGamePoseArray[GGVR_LHandIndex].mDeviceToAbsoluteTracking.m[2][0] = matRot.m[2][0];
				pGamePoseArray[GGVR_LHandIndex].mDeviceToAbsoluteTracking.m[0][1] = matRot.m[0][1];
				pGamePoseArray[GGVR_LHandIndex].mDeviceToAbsoluteTracking.m[1][1] = matRot.m[1][1];
				pGamePoseArray[GGVR_LHandIndex].mDeviceToAbsoluteTracking.m[2][1] = matRot.m[2][1];
				pGamePoseArray[GGVR_LHandIndex].mDeviceToAbsoluteTracking.m[0][2] = matRot.m[0][2];
				pGamePoseArray[GGVR_LHandIndex].mDeviceToAbsoluteTracking.m[1][2] = matRot.m[1][2];
				pGamePoseArray[GGVR_LHandIndex].mDeviceToAbsoluteTracking.m[2][2] = matRot.m[2][2];
				pGamePoseArray[GGVR_LHandIndex].mDeviceToAbsoluteTracking.m[0][3] = fLeftHandX;
				pGamePoseArray[GGVR_LHandIndex].mDeviceToAbsoluteTracking.m[1][3] = fLeftHandY;
				pGamePoseArray[GGVR_LHandIndex].mDeviceToAbsoluteTracking.m[2][3] = fLeftHandZ;
				GGVR_Mat34toYPR(&pGamePoseArray[GGVR_LHandIndex].mDeviceToAbsoluteTracking, &pGamePoseArray_YPR[GGVR_LHandIndex], &pGamePoseArray_Pos[GGVR_LHandIndex]);
			}
		}
	}
}

void GGVR_UpdatePlayer ( bool bPlayerDucking, int iTerrainID, int iLMObjStart, int iLMObjFinish, int iEntObjStart, int iEndObjEnd )
{
	// Update the HMD and controller feedbacks
	timestampactivity ( 0, "TS: Calling GGVR_UpdatePoses" );
	//DebugGGVRlog ( "Calling GGVR_UpdatePoses" );
	GGVR_UpdatePoses();

	//Position the Base Object offset from the origin
	//-------------------------------------------------------
	//Set the base to the orientation of the origin
	RotateObject(GGVR_Player.ObjBase, ObjectAngleX(GGVR_Player.ObjOrigin), ObjectAngleY(GGVR_Player.ObjOrigin), ObjectAngleZ(GGVR_Player.ObjOrigin));
	//Set the base to the position of the origin
	PositionObject(GGVR_Player.ObjBase, ObjectPositionX(GGVR_Player.ObjOrigin), ObjectPositionY(GGVR_Player.ObjOrigin), ObjectPositionZ(GGVR_Player.ObjOrigin));
	//Move the base on the local X and Z axis of the origin based on the X and Z HMD offset distances
	MoveObject(GGVR_Player.ObjBase, pGamePoseArray_Pos[GGVR_HMD].v[2] * GGVR_WorldScale);
	MoveObjectRight(GGVR_Player.ObjBase, pGamePoseArray_Pos[GGVR_HMD].v[0] * GGVR_WorldScale);
	//Rotate the base's Y axis to match the turn angle offset of the HMD 
	if (GGVR_Player.TurnLock == 1)
	{
		TurnObjectRight(GGVR_Player.ObjBase, pGamePoseArray_YPR[GGVR_HMD].v[1]);
	}
	//Rotate the base's X axis to match the pitch angle offset of the HMD 
	if (GGVR_Player.PitchLock == 1)
	{
		PitchObjectDown(GGVR_Player.ObjBase, pGamePoseArray_YPR[GGVR_HMD].v[0]);
	}

	//Position the Head Object based on the origin
	//-------------------------------------------------------
	// Head is somewhere above the feet of the player
	//float fsubtleeyeadjustment = 60.0;
	//if ( bPlayerDucking ) fsubtleeyeadjustment = 20.0f;
	//Set the head to the orientation of the origin
	RotateObject(GGVR_Player.ObjHead, ObjectAngleX(GGVR_Player.ObjOrigin), ObjectAngleY(GGVR_Player.ObjOrigin), ObjectAngleZ(GGVR_Player.ObjOrigin));
	//Set the head to the position of the origin
	//PositionObject(GGVR_Player.ObjHead, ObjectPositionX(GGVR_Player.ObjOrigin), fsubtleeyeadjustment + ObjectPositionY(GGVR_Player.ObjOrigin), ObjectPositionZ(GGVR_Player.ObjOrigin));
	PositionObject(GGVR_Player.ObjHead, ObjectPositionX(GGVR_Player.ObjOrigin), ObjectPositionY(GGVR_Player.ObjOrigin), ObjectPositionZ(GGVR_Player.ObjOrigin));
	//Move the head on the local X, Y and Z axis of the origin based on the X, Y and Z HMD offset distances
	MoveObjectUp(GGVR_Player.ObjHead, pGamePoseArray_Pos[GGVR_HMD].v[1] * GGVR_WorldScale);
	MoveObject(GGVR_Player.ObjHead, pGamePoseArray_Pos[GGVR_HMD].v[2] * GGVR_WorldScale);
	MoveObjectRight(GGVR_Player.ObjHead, pGamePoseArray_Pos[GGVR_HMD].v[0] * GGVR_WorldScale);
	//Rotate the head based on the Turn, Pitch and Roll offsest of the HMD
	TurnObjectRight(GGVR_Player.ObjHead, pGamePoseArray_YPR[GGVR_HMD].v[1]);
	PitchObjectDown(GGVR_Player.ObjHead, pGamePoseArray_YPR[GGVR_HMD].v[0]);
	RollObjectRight(GGVR_Player.ObjHead, -pGamePoseArray_YPR[GGVR_HMD].v[2]);

	//Set Cameras based on head orientation
	PositionCamera(GGVR_RCamID, ObjectPositionX(GGVR_Player.ObjHead), ObjectPositionY(GGVR_Player.ObjHead), ObjectPositionZ(GGVR_Player.ObjHead));
	PositionCamera(GGVR_LCamID, ObjectPositionX(GGVR_Player.ObjHead), ObjectPositionY(GGVR_Player.ObjHead), ObjectPositionZ(GGVR_Player.ObjHead));
	RotateCamera(GGVR_RCamID, ObjectAngleX(GGVR_Player.ObjHead), ObjectAngleY(GGVR_Player.ObjHead), ObjectAngleZ(GGVR_Player.ObjHead));
	RotateCamera(GGVR_LCamID, ObjectAngleX(GGVR_Player.ObjHead), ObjectAngleY(GGVR_Player.ObjHead), ObjectAngleZ(GGVR_Player.ObjHead));

	//Move Cameras for IPD Distance
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		MoveCameraRight(GGVR_RCamID, (ivr_system->GetFloatTrackedDeviceProperty(0, vr::Prop_UserIpdMeters_Float) / 2.0f)*GGVR_WorldScale*GGVR_ipdscale);
		MoveCameraRight(GGVR_LCamID, -(ivr_system->GetFloatTrackedDeviceProperty(0, vr::Prop_UserIpdMeters_Float) / 2.0f)*GGVR_WorldScale*GGVR_ipdscale);
		#endif
	}
	if ( GGVR_EnabledMode > 1 )
	{
		float fUserIpdMeters = 0.035f; // 0.1f hill looked like a small toy
		MoveCameraRight(GGVR_RCamID, (fUserIpdMeters/2.0f)*GGVR_WorldScale*GGVR_ipdscale);
		MoveCameraRight(GGVR_LCamID, -(fUserIpdMeters/2.0f)*GGVR_WorldScale*GGVR_ipdscale);
	}

	//Set Right Hand Position
	//-------------------------------------------------------
	//Set the Right Hand to the orientation of the origin
	RotateObject(GGVR_Player.ObjRightHand, ObjectAngleX(GGVR_Player.ObjOrigin), ObjectAngleY(GGVR_Player.ObjOrigin), ObjectAngleZ(GGVR_Player.ObjOrigin));
	//Set the Right Hand to the position of the origin
	PositionObject(GGVR_Player.ObjRightHand, ObjectPositionX(GGVR_Player.ObjOrigin), ObjectPositionY(GGVR_Player.ObjOrigin), ObjectPositionZ(GGVR_Player.ObjOrigin));
	if (GGVR_RHandIndex != 0)
	{
		//Move the Right Hand on the local X, Y and Z axis of the origin based on the X, Y and Z controller tracking feedback
		MoveObject(GGVR_Player.ObjRightHand, pGamePoseArray_Pos[GGVR_RHandIndex].v[2] * GGVR_WorldScale);
		MoveObjectRight(GGVR_Player.ObjRightHand, pGamePoseArray_Pos[GGVR_RHandIndex].v[0] * GGVR_WorldScale);
		MoveObjectUp(GGVR_Player.ObjRightHand, pGamePoseArray_Pos[GGVR_RHandIndex].v[1] * GGVR_WorldScale);

		//Rotate the Right Hand based on the Turn, Pitch and Roll offsest of the controller tracking feedback
		TurnObjectRight(GGVR_Player.ObjRightHand, pGamePoseArray_YPR[GGVR_RHandIndex].v[1]);
		PitchObjectDown(GGVR_Player.ObjRightHand, pGamePoseArray_YPR[GGVR_RHandIndex].v[0]);
		RollObjectRight(GGVR_Player.ObjRightHand, -pGamePoseArray_YPR[GGVR_RHandIndex].v[2]);
	}

	//Set Left Hand Position
	//-------------------------------------------------------
	//Set the Left Hand to the orientation of the origin
	RotateObject(GGVR_Player.ObjLeftHand, ObjectAngleX(GGVR_Player.ObjOrigin), ObjectAngleY(GGVR_Player.ObjOrigin), ObjectAngleZ(GGVR_Player.ObjOrigin));
	//Set the Left Hand to the position of the origin
	PositionObject(GGVR_Player.ObjLeftHand, ObjectPositionX(GGVR_Player.ObjOrigin), ObjectPositionY(GGVR_Player.ObjOrigin), ObjectPositionZ(GGVR_Player.ObjOrigin));
	if (GGVR_LHandIndex != 0)
	{
		//Move the Left Hand on the local X, Y and Z axis of the origin based on the X, Y and Z controller tracking feedback
		MoveObject(GGVR_Player.ObjLeftHand, pGamePoseArray_Pos[GGVR_LHandIndex].v[2] * GGVR_WorldScale);
		MoveObjectRight(GGVR_Player.ObjLeftHand, pGamePoseArray_Pos[GGVR_LHandIndex].v[0] * GGVR_WorldScale);
		MoveObjectUp(GGVR_Player.ObjLeftHand, pGamePoseArray_Pos[GGVR_LHandIndex].v[1] * GGVR_WorldScale);
		//Rotate the Left Hand based on the Turn, Pitch and Roll offsest of the controller tracking feedback
		TurnObjectRight(GGVR_Player.ObjLeftHand, pGamePoseArray_YPR[GGVR_LHandIndex].v[1]);
		PitchObjectDown(GGVR_Player.ObjLeftHand, pGamePoseArray_YPR[GGVR_LHandIndex].v[0]);
		RollObjectRight(GGVR_Player.ObjLeftHand, -pGamePoseArray_YPR[GGVR_LHandIndex].v[2]);
	}

	// show controller if data coming through
	if ( pGamePoseArray_Pos[GGVR_LHandIndex].v[0]+pGamePoseArray_Pos[GGVR_LHandIndex].v[1]+pGamePoseArray_Pos[GGVR_LHandIndex].v[2] != 0 )
		ShowObject ( GGVR_Player.ObjLeftHand );
	else
		HideObject ( GGVR_Player.ObjLeftHand );

	if ( pGamePoseArray_Pos[GGVR_RHandIndex].v[0]+pGamePoseArray_Pos[GGVR_RHandIndex].v[1]+pGamePoseArray_Pos[GGVR_RHandIndex].v[2] != 0 )
		ShowObject ( GGVR_Player.ObjRightHand );
	else
		HideObject ( GGVR_Player.ObjRightHand );

	// show pointer when teleport touched
	bool bShowControllerWand = false;
	bool bShowHandLaserBeam = false;
	if ( GGVR_bTouchPadTouched == true ) // touch anywhere && GGVR_fTouchPadY > 0.5f )
		bShowControllerWand = true;
	else
		bShowHandLaserBeam = true;

	// find out active controller and store info
	static int g_iLastControllerActive = 0;
	if ( GGVR_bTouchPadTouched )
	{
		if ( GGVR_bTouchPadIsRightHand == true )
		{
			if ( GetVisible ( GGVR_Player.ObjRightHand ) == 1 )
			{
				g_iLastControllerActive = 1; // right has gadgets
			}
		}
		else
		{
			if ( GetVisible ( GGVR_Player.ObjLeftHand ) == 1 )
			{
				g_iLastControllerActive = 2; // left has gadgets
			}
		}
	}

	// determine hand position and angle
	float x = ObjectPositionX ( GGVR_Player.ObjLeftHand );
	float y = ObjectPositionY ( GGVR_Player.ObjLeftHand );
	float z = ObjectPositionZ ( GGVR_Player.ObjLeftHand );
	MoveObject ( GGVR_Player.ObjLeftHand, 10.0f );
	float nx = ObjectPositionX ( GGVR_Player.ObjLeftHand ) - x;
	float ny = ObjectPositionY ( GGVR_Player.ObjLeftHand ) - y;
	float nz = ObjectPositionZ ( GGVR_Player.ObjLeftHand ) - z;
	MoveObject ( GGVR_Player.ObjLeftHand, -10.0f );
	if ( g_iLastControllerActive == 1 )
	{
		x = ObjectPositionX ( GGVR_Player.ObjRightHand );
		y = ObjectPositionY ( GGVR_Player.ObjRightHand );
		z = ObjectPositionZ ( GGVR_Player.ObjRightHand );
		MoveObject ( GGVR_Player.ObjRightHand, 10.0f );
		nx = ObjectPositionX ( GGVR_Player.ObjRightHand ) - x;
		ny = ObjectPositionY ( GGVR_Player.ObjRightHand ) - y;
		nz = ObjectPositionZ ( GGVR_Player.ObjRightHand ) - z;
		MoveObject ( GGVR_Player.ObjRightHand, -10.0f );
	}

	// create teleport arc control points or laser
	GGVR_Player.LaserGuideActive = 0;
	static int g_iControllerVisualMode = 0;
	bool bCannotLandHereCancelDestination = false;
	if ( bShowControllerWand == true && g_iLastControllerActive > 0 )
	{
		std::vector <GGVECTOR3> vecControlPoints;
		vecControlPoints.clear();
		bool bAboveGround = true;
		float fLastX = x;
		float fLastY = y;
		float fLastZ = z;
		while ( bAboveGround == true )
		{
			// shift normal to fall with gravity
			nx *= 0.99f;
			ny -= 0.1f; if ( ny < -10.0f ) ny = -10.0f;
			nz *= 0.99f;

			// move control point
			x += nx;
			y += ny;
			z += nz;
			GGVECTOR3 pnt = GGVECTOR3 ( x, y, z );
			vecControlPoints.push_back ( pnt );

			// when drop below ground
			float fActualTerrainGroundY = BT_GetGroundHeight ( iTerrainID, x, z );
			if ( y < fActualTerrainGroundY )
			{
				GGVR_fTelePortDestinationX = x;
				GGVR_fTelePortDestinationY = fActualTerrainGroundY;
				GGVR_fTelePortDestinationZ = z;
				bAboveGround = false;
			}

			// when cut through an entity in the scene
			IntersectAll(iLMObjStart,iLMObjFinish,0,0,0,0,0,0,-123);
			int tHitObj = IntersectAll(iEntObjStart,iEndObjEnd,x,y,z,fLastX,fLastY,fLastZ,0);
			if ( tHitObj > 0 )
			{
				GGVR_fTelePortDestinationX = ChecklistFValueA(6);
				GGVR_fTelePortDestinationY = ChecklistFValueB(6);
				GGVR_fTelePortDestinationZ = ChecklistFValueC(6);
				bAboveGround = false;
				if ( ny >= 0.0f ) bCannotLandHereCancelDestination = true;
			}
			fLastX = x; fLastY = y; fLastZ = z;
		}

		// work out spread from perfect arc data to available dots below
		int iControlPointCount = vecControlPoints.size();
		float fCountPointIndex = 0;
		int iMaxObjects = (GGVR_Player.ObjTeleportFinish-GGVR_Player.ObjTeleportStart)-1;
		float fControlPointStep = (float)iControlPointCount/(float)(iMaxObjects);

		// project arc to show teleport destination
		int iLastControlPoint = -1;
		if ( GGVR_bTouchPadIsRightHand == true )
		{
			x = ObjectPositionX ( GGVR_Player.ObjRightHand );
			y = ObjectPositionY ( GGVR_Player.ObjRightHand );
			z = ObjectPositionZ ( GGVR_Player.ObjRightHand );
		}
		else
		{
			x = ObjectPositionX ( GGVR_Player.ObjLeftHand );
			y = ObjectPositionY ( GGVR_Player.ObjLeftHand );
			z = ObjectPositionZ ( GGVR_Player.ObjLeftHand );
		}

		// before we settle on final teleport dest, ensure its not too close to a surface (so dont get trapped inside it)
		float fDirX = GGVR_fTelePortDestinationX - x;
		float fDirZ = GGVR_fTelePortDestinationZ - z;
		float fDirDist = sqrt ( fabs(fDirX*fDirX)+fabs(fDirZ*fDirZ) );
		fDirX /= fDirDist;
		fDirZ /= fDirDist;
		fDirX *= 25.0f;
		fDirZ *= 25.0f;
		float fPosNearerPlrX = GGVR_fTelePortDestinationX - fDirX;
		float fPosNearerPlrZ = GGVR_fTelePortDestinationZ - fDirZ;
		float fPosEndOfCircleX = GGVR_fTelePortDestinationX + fDirX;
		float fPosEndOfCircleZ = GGVR_fTelePortDestinationZ + fDirZ;
		float fYOffsetAboveDest = 5.0f;
		IntersectAll(iLMObjStart,iLMObjFinish,0,0,0,0,0,0,-123);
		int tDoesCircleIntersect = IntersectAll(iEntObjStart,iEndObjEnd,fPosNearerPlrX,GGVR_fTelePortDestinationY+fYOffsetAboveDest,fPosNearerPlrZ,fPosEndOfCircleX,GGVR_fTelePortDestinationY+fYOffsetAboveDest,fPosEndOfCircleZ,0);
		if ( tDoesCircleIntersect > 0 )
		{
			// the circle is intersecting something, cannot land here
			bCannotLandHereCancelDestination = true;
		}
		IntersectAll(iLMObjStart,iLMObjFinish,0,0,0,0,0,0,-123);
		tDoesCircleIntersect = IntersectAll(iEntObjStart,iEndObjEnd,GGVR_fTelePortDestinationX,GGVR_fTelePortDestinationY+fYOffsetAboveDest,GGVR_fTelePortDestinationZ,GGVR_fTelePortDestinationX,GGVR_fTelePortDestinationY+85.0f,GGVR_fTelePortDestinationZ,0);
		if ( tDoesCircleIntersect > 0 )
		{
			// the circle is intersecting above (so player cannot fit in), cannot land here
			bCannotLandHereCancelDestination = true;
		}

		// create objects for the arc
		if ( g_iControllerVisualMode != 1 && ObjectExist(GGVR_Player.ObjTeleportStart) == 1 ) DeleteObject ( GGVR_Player.ObjTeleportStart );
		for ( int o = GGVR_Player.ObjTeleportStart; o < GGVR_Player.ObjTeleportFinish; o++ )
		{
			int iThisUniqueControlPoint = (int)fCountPointIndex;
			if ( iThisUniqueControlPoint >= iControlPointCount ) iThisUniqueControlPoint = iControlPointCount - 1;
			if ( !ObjectExist ( o ) )
			{
				// create object slice
				MakeObjectBox ( o, 0.3f, 0.1f, 5.0f );	
				SetObjectEffect ( o, GGVR_Player.ShaderID );
				SetSphereRadius ( o, 0 );
				SetObjectMask ( o, (1<<6) + (1<<7) + 1 );
				// from editimage+14 (also have +16 for red and +17 for green)
				if ( bCannotLandHereCancelDestination == true )
					TextureObject ( o, 0, GGVR_Player.TextureID+2 );
				else
					TextureObject ( o, 0, GGVR_Player.TextureID+3 );
				SetObjectTransparency ( o, 2 );
				SetObjectLight ( o, 0 );
				SetAlphaMappingOn ( o, 50 );
				g_iControllerVisualMode = 1;
			}
			else
			{
				// from editimage+14 (also have +16 for red and +17 for green)
				if ( bCannotLandHereCancelDestination == true )
					TextureObject ( o, 0, GGVR_Player.TextureID+2 );
				else
					TextureObject ( o, 0, GGVR_Player.TextureID+3 );
			}
			HideObject ( o );
			if ( ObjectExist ( o ) && iLastControlPoint != iThisUniqueControlPoint )
			{
				// use this object
				ShowObject ( o );

				// position and rotate object
				float lx = x;
				float ly = y;
				float lz = z;
				x = vecControlPoints[iThisUniqueControlPoint].x;
				y = vecControlPoints[iThisUniqueControlPoint].y;
				z = vecControlPoints[iThisUniqueControlPoint].z;
				PositionObject ( o, x, y, z );

				// rotate to follow curve
				PointObject ( o, lx, ly, lz );
			}

			// next control point
			iLastControlPoint = (int)fCountPointIndex;
			fCountPointIndex += fControlPointStep;
		}

		// last object is a circle (squashed sphere)
		if ( !ObjectExist ( GGVR_Player.ObjTeleportFinish ) )
		{
			MakeObjectSphere ( GGVR_Player.ObjTeleportFinish, 50.0f, 6, 30 );
			ScaleObject ( GGVR_Player.ObjTeleportFinish, 100, 10, 100 );
			SetObjectEffect ( GGVR_Player.ObjTeleportFinish, GGVR_Player.ShaderID );
			SetSphereRadius ( GGVR_Player.ObjTeleportFinish, 0 );
			SetObjectMask ( GGVR_Player.ObjTeleportFinish, (1<<6) + (1<<7) + 1 );
			SetObjectTransparency ( GGVR_Player.ObjTeleportFinish, 2 );
			SetObjectLight ( GGVR_Player.ObjTeleportFinish, 0 );
			SetAlphaMappingOn ( GGVR_Player.ObjTeleportFinish, 50 );
		}
		if ( bCannotLandHereCancelDestination == true )
			TextureObject ( GGVR_Player.ObjTeleportFinish, 0, GGVR_Player.TextureID+2 );
		else
			TextureObject ( GGVR_Player.ObjTeleportFinish, 0, GGVR_Player.TextureID+3 );
		PositionObject ( GGVR_Player.ObjTeleportFinish, GGVR_fTelePortDestinationX, GGVR_fTelePortDestinationY+5.0f, GGVR_fTelePortDestinationZ );
		RotateObject ( GGVR_Player.ObjTeleportFinish, 0, 0, 0 );
		if ( bCannotLandHereCancelDestination == false )
			ShowObject ( GGVR_Player.ObjTeleportFinish );
		else
			HideObject ( GGVR_Player.ObjTeleportFinish );
	}
	else
	{
		if ( bShowHandLaserBeam == true && g_iLastControllerActive > 0 )
		{
			if ( g_iControllerVisualMode != 2 && ObjectExist(GGVR_Player.ObjTeleportStart) == 1 ) DeleteObject ( GGVR_Player.ObjTeleportStart );
			int o = GGVR_Player.ObjTeleportStart;
			if ( !ObjectExist ( o ) )
			{
				MakeObjectBox ( o, 0.1f, 0.1f, 200.0f );	
				SetObjectEffect ( o, GGVR_Player.ShaderID );
				SetSphereRadius ( o, 0 );
				SetObjectMask ( o, (1<<6) + (1<<7) + 1 );
				TextureObject ( o, 0, GGVR_Player.TextureID+3 );
				SetObjectTransparency ( o, 2 );
				SetObjectLight ( o, 0 );
				SetAlphaMappingOn ( o, 50 );
				g_iControllerVisualMode = 2;
			}
			if ( ObjectExist ( o ) )
			{
				// position and rotate object
				ShowObject ( o );
				PositionObject ( o, x, y, z );
				PointObject ( o, x+nx, y+ny, z+nz );
				MoveObject ( o, 100.0f );
				GGVR_Player.LaserGuideActive = o;
			}
			for ( int o = GGVR_Player.ObjTeleportStart+1; o <= GGVR_Player.ObjTeleportFinish; o++ )
				if ( ObjectExist(o) )
					HideObject ( o );
		}
		else
		{
			// hide teleport arc and laser objects
			for ( int o = GGVR_Player.ObjTeleportStart; o <= GGVR_Player.ObjTeleportFinish; o++ )
				if ( ObjectExist(o) )
					HideObject ( o );
		}
	}
}

bool GGVR_HandlePlayerTeleport ( float* pNewPosX, float* pNewPosY, float* pNewPosZ, float* pNewAngleY )
{
	if ( GGVR_bTouchPadPressed==true && GGVR_bTouchPadNeedToRelease==false && GGVR_fTelePortDestinationX != 0.0f )
	{
		GGVR_bTouchPadNeedToRelease = true;
		if ( GetVisible ( GGVR_Player.ObjTeleportFinish ) == 1 )
		{
			*pNewPosX = GGVR_fTelePortDestinationX;
			*pNewPosY = GGVR_fTelePortDestinationY+30;
			*pNewPosZ = GGVR_fTelePortDestinationZ;
			*pNewAngleY = CameraAngleY(0);
			return true;
		}
		else
			return false;
	}
	if ( GGVR_bTouchPadPressed==false )
	{
		GGVR_bTouchPadNeedToRelease = false;
	}
	return false;
}

void GGVR_SetHMDPosition(float X, float Y, float Z)
{
	//Set the player base at the specified location
	PositionObject(GGVR_Player.ObjHead, X, Y, Z);

	//Move the base into the proper location based on it's X and Z offsets so the new location is not lost on the next player update
	PositionObject(GGVR_Player.ObjBase, X, Y, Z);
	MoveObjectUp(GGVR_Player.ObjBase, -pGamePoseArray_Pos[GGVR_HMD].v[1] * GGVR_WorldScale);

	//Move the origin into the proper location based on it's X and Z offsets so the new location is not lost on the next player update
	PositionObject(GGVR_Player.ObjOrigin, X, Y, Z);
	MoveObject(GGVR_Player.ObjOrigin, -pGamePoseArray_Pos[GGVR_HMD].v[2] * GGVR_WorldScale);
	MoveObjectRight(GGVR_Player.ObjOrigin, -pGamePoseArray_Pos[GGVR_HMD].v[0] * GGVR_WorldScale);
}

void GGVR_SetHMDRotation(float X, float Y, float Z)
{
	//Set the player base at the specified location
	RotateObject(GGVR_Player.ObjOrigin, X, Y, Z);
	TurnObjectRight(GGVR_Player.ObjOrigin, -pGamePoseArray_YPR[GGVR_HMD].v[1]);
	PitchObjectDown(GGVR_Player.ObjOrigin, -pGamePoseArray_YPR[GGVR_HMD].v[0]);
	RollObjectRight(GGVR_Player.ObjOrigin, -pGamePoseArray_YPR[GGVR_HMD].v[2]);
}

void GGVR_SetPlayerPosition( float X, float Y, float Z )
{	
	//Set the player base at the specified location
	PositionObject(GGVR_Player.ObjBase, X, Y, Z);

	//Move the origin into the proper location based on it's X and Z offsets so the new location is not lost on the next player update
	PositionObject(GGVR_Player.ObjOrigin, X, Y, Z);
	MoveObject(GGVR_Player.ObjOrigin, -pGamePoseArray_Pos[GGVR_HMD].v[2] * GGVR_WorldScale);
	MoveObjectRight(GGVR_Player.ObjOrigin, -pGamePoseArray_Pos[GGVR_HMD].v[0] * GGVR_WorldScale);
}

void GGVR_SetPlayerRotation( float X, float Y, float Z )
{
	RotateObject(GGVR_Player.ObjOrigin, X, Y, Z);
}

void GGVR_SetOriginPosition(float X, float Y, float Z)
{
	//Set the player base at the specified location
	PositionObject(GGVR_Player.ObjOrigin, X, Y, Z);
}

void GGVR_SetOriginRotation(float X, float Y, float Z)
{
	//Set the player base at the specified location
	RotateObject(GGVR_Player.ObjOrigin, X, Y, Z);
}

void GGVR_MovePlayerLocalX( float valx )
{
	if (valx != 0)
	{
		//Move the base object based on it's Z direction
		MoveObjectRight(GGVR_Player.ObjBase, valx);
		//Reposition the origin object
		PositionObject(GGVR_Player.ObjOrigin, ObjectPositionX(GGVR_Player.ObjBase), ObjectPositionY(GGVR_Player.ObjBase), ObjectPositionZ(GGVR_Player.ObjBase));
		MoveObject(GGVR_Player.ObjOrigin, -pGamePoseArray_Pos[GGVR_HMD].v[2] * GGVR_WorldScale);
		MoveObjectRight(GGVR_Player.ObjOrigin, -pGamePoseArray_Pos[GGVR_HMD].v[0] * GGVR_WorldScale);
	}
}

void GGVR_MovePlayerLocalY( float valy )
{
	if (valy != 0)
	{
		//Move the base object based on it's Z direction
		MoveObjectUp(GGVR_Player.ObjBase, valy);
		//Reposition the origin object
		PositionObject(GGVR_Player.ObjOrigin, ObjectPositionX(GGVR_Player.ObjBase), ObjectPositionY(GGVR_Player.ObjBase), ObjectPositionZ(GGVR_Player.ObjBase));
		MoveObject(GGVR_Player.ObjOrigin, -pGamePoseArray_Pos[GGVR_HMD].v[2] * GGVR_WorldScale);
		MoveObjectRight(GGVR_Player.ObjOrigin, -pGamePoseArray_Pos[GGVR_HMD].v[0] * GGVR_WorldScale);
	}
}

void GGVR_MovePlayerLocalZ( float valz )
{
	if (valz != 0)
	{
		//Move the base object based on it's Z direction
		MoveObject(GGVR_Player.ObjBase, valz);
		//Reposition the origin object
		PositionObject(GGVR_Player.ObjOrigin, ObjectPositionX(GGVR_Player.ObjBase), ObjectPositionY(GGVR_Player.ObjBase), ObjectPositionZ(GGVR_Player.ObjBase));
		MoveObject(GGVR_Player.ObjOrigin, -pGamePoseArray_Pos[GGVR_HMD].v[2] * GGVR_WorldScale);
		MoveObjectRight(GGVR_Player.ObjOrigin, -pGamePoseArray_Pos[GGVR_HMD].v[0] * GGVR_WorldScale);
	}
}

void GGVR_RotatePlayerLocalX( float valx )
{
	if (valx != 0)
	{
		//First Rotate the orgin object
		PitchObjectDown(GGVR_Player.ObjOrigin, valx);
		//Position the origin object at the base location, then push it to it's proper location
		PositionObject(GGVR_Player.ObjOrigin, ObjectPositionX(GGVR_Player.ObjBase), ObjectPositionY(GGVR_Player.ObjBase), ObjectPositionZ(GGVR_Player.ObjBase));
		MoveObject(GGVR_Player.ObjOrigin, -pGamePoseArray_Pos[GGVR_HMD].v[2] * GGVR_WorldScale);
		MoveObjectRight(GGVR_Player.ObjOrigin, -pGamePoseArray_Pos[GGVR_HMD].v[0] * GGVR_WorldScale);
	}
}

void GGVR_SetPlayerAngleY( float valy )
{
	//First Rotate the orgin object
	YRotateObject ( GGVR_Player.ObjOrigin, valy);

	//Position the origin object at the base location, then push it to it's proper location
	PositionObject(GGVR_Player.ObjOrigin, ObjectPositionX(GGVR_Player.ObjBase), ObjectPositionY(GGVR_Player.ObjBase), ObjectPositionZ(GGVR_Player.ObjBase));
	MoveObject(GGVR_Player.ObjOrigin, -pGamePoseArray_Pos[GGVR_HMD].v[2] * GGVR_WorldScale);
	MoveObjectRight(GGVR_Player.ObjOrigin, -pGamePoseArray_Pos[GGVR_HMD].v[0] * GGVR_WorldScale);
}

void GGVR_RotatePlayerLocalY( float valy )
{
	if (valy != 0)
	{
		//First Rotate the orgin object
		TurnObjectRight(GGVR_Player.ObjOrigin, valy);
		//Position the origin object at the base location, then push it to it's proper location
		PositionObject(GGVR_Player.ObjOrigin, ObjectPositionX(GGVR_Player.ObjBase), ObjectPositionY(GGVR_Player.ObjBase), ObjectPositionZ(GGVR_Player.ObjBase));
		MoveObject(GGVR_Player.ObjOrigin, -pGamePoseArray_Pos[GGVR_HMD].v[2] * GGVR_WorldScale);
		MoveObjectRight(GGVR_Player.ObjOrigin, -pGamePoseArray_Pos[GGVR_HMD].v[0] * GGVR_WorldScale);
	}
}

void GGVR_RotatePlayerLocalZ( float valz )
{
	if (valz != 0)
	{
		//First Rotate the orgin object
		RollObjectRight(GGVR_Player.ObjOrigin, valz);
		//Position the origin object at the base location, then push it to it's proper location
		PositionObject(GGVR_Player.ObjOrigin, ObjectPositionX(GGVR_Player.ObjBase), ObjectPositionY(GGVR_Player.ObjBase), ObjectPositionZ(GGVR_Player.ObjBase));
		MoveObject(GGVR_Player.ObjOrigin, -pGamePoseArray_Pos[GGVR_HMD].v[2] * GGVR_WorldScale);
		MoveObjectRight(GGVR_Player.ObjOrigin, -pGamePoseArray_Pos[GGVR_HMD].v[0] * GGVR_WorldScale);
	}
}

void GGVR_LockPlayerTurn( int Lock )
{
	if (Lock == 1)
	{ GGVR_Player.TurnLock = 1;}
	else { GGVR_Player.TurnLock = 0;}
}

void GGVR_LockPlayerPitch( int Lock )
{
	if (Lock == 1)
	{ GGVR_Player.PitchLock = 1;}
	else { GGVR_Player.PitchLock = 0;}
}

float GGVR_GetPlayerX( )
{
	if (ObjectExist(GGVR_Player.ObjBase) == 1)
	{ 
		return ObjectPositionX(GGVR_Player.ObjBase);
	}
	else 
	{ return 0; }
}

float GGVR_GetPlayerY( )
{
	if (ObjectExist(GGVR_Player.ObjBase) == 1)
	{
		return ObjectPositionY(GGVR_Player.ObjBase);
	}
	else { return 0; }
}

float GGVR_GetPlayerZ( )
{
	if (ObjectExist(GGVR_Player.ObjBase) == 1)
	{
		return ObjectPositionZ(GGVR_Player.ObjBase);
	}
	else { return 0; }
}

float GGVR_GetPlayerAngleX( )
{
	if (ObjectExist(GGVR_Player.ObjBase) == 1)
	{
		return ObjectAngleX(GGVR_Player.ObjBase);
	}
	else { return 0; }
}

float GGVR_GetPlayerAngleY( )
{
	if (ObjectExist(GGVR_Player.ObjBase) == 1)
	{
		return ObjectAngleY(GGVR_Player.ObjBase);
	}
	else { return 0; }
}

float GGVR_GetPlayerAngleZ( )
{
	if (ObjectExist(GGVR_Player.ObjBase) == 1)
	{
		return ObjectAngleZ(GGVR_Player.ObjBase);
	}
	else { return 0; }
}

float GGVR_GetOriginX()
{
	if ( GGVR_Player.ObjOrigin > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjOrigin) == 1)
		{
			return ObjectPositionX(GGVR_Player.ObjOrigin);
		}
		else
		{
			return 0;
		}
	}
	else { return 0; }
}

float GGVR_GetOriginY()
{
	if ( GGVR_Player.ObjOrigin > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjOrigin) == 1)
		{
			return ObjectPositionY(GGVR_Player.ObjOrigin);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetOriginZ()
{
	if ( GGVR_Player.ObjOrigin > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjOrigin) == 1)
		{
			return ObjectPositionZ(GGVR_Player.ObjOrigin);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetHMDX( )
{
	if ( GGVR_Player.ObjHead > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjHead) == 1)
		{
			return ObjectPositionX(GGVR_Player.ObjHead);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetHMDY( )
{
	if ( GGVR_Player.ObjHead > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjHead) == 1)
		{
			return ObjectPositionY(GGVR_Player.ObjHead);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetHMDZ( )
{
	if ( GGVR_Player.ObjHead > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjHead) == 1)
		{
			return ObjectPositionZ(GGVR_Player.ObjHead);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetHMDYaw()
{
	if ( GGVR_Player.ObjHead > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjHead) == 1)
		{
			return pGamePoseArray_YPR[GGVR_HMD].v[1];
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetHMDPitch()
{
	if ( GGVR_Player.ObjHead > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjHead) == 1)
		{
			return pGamePoseArray_YPR[GGVR_HMD].v[0];
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetHMDRoll()
{
	if ( GGVR_Player.ObjHead > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjHead) == 1)
		{
			return pGamePoseArray_YPR[GGVR_HMD].v[2];
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetHMDOffsetX()
{
	if ( GGVR_Player.ObjHead > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjHead) == 1)
		{
			return pGamePoseArray_Pos[GGVR_HMD].v[0] * GGVR_WorldScale;
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetHMDOffsetY()
{
	if ( GGVR_Player.ObjHead > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjHead) == 1)
		{
			return pGamePoseArray_Pos[GGVR_HMD].v[1] * GGVR_WorldScale;
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetHMDOffsetZ()
{
	if ( GGVR_Player.ObjHead > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjHead) == 1)
		{
			return pGamePoseArray_Pos[GGVR_HMD].v[2] * GGVR_WorldScale;
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetHMDAngleX( )
{
	if ( GGVR_Player.ObjHead > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjHead) == 1)
		{
			return ObjectAngleX(GGVR_Player.ObjHead);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetHMDAngleY( )
{
	if ( GGVR_Player.ObjHead > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjHead) == 1)
		{
			return ObjectAngleY(GGVR_Player.ObjHead);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetHMDAngleZ( )
{
	if ( GGVR_Player.ObjHead > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjHead) == 1)
		{
			return ObjectAngleZ(GGVR_Player.ObjHead);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetRightHandX( )
{
	if ( GGVR_Player.ObjRightHand > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjRightHand) == 1)
		{
			return ObjectPositionX(GGVR_Player.ObjRightHand);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetRightHandY( )
{
	if ( GGVR_Player.ObjRightHand > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjRightHand) == 1)
		{
			return ObjectPositionY(GGVR_Player.ObjRightHand);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetRightHandZ( )
{
	if ( GGVR_Player.ObjRightHand > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjRightHand) == 1)
		{
			return ObjectPositionZ(GGVR_Player.ObjRightHand);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetRightHandAngleX( )
{
	if ( GGVR_Player.ObjRightHand > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjRightHand) == 1)
		{
			return ObjectAngleX(GGVR_Player.ObjRightHand);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetRightHandAngleY( )
{
	if ( GGVR_Player.ObjRightHand > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjRightHand) == 1)
		{
			return ObjectAngleY(GGVR_Player.ObjRightHand);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetRightHandAngleZ( )
{
	if ( GGVR_Player.ObjRightHand > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjRightHand) == 1)
		{
			return ObjectAngleZ(GGVR_Player.ObjRightHand);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetLeftHandX( )
{
	if ( GGVR_Player.ObjLeftHand > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjLeftHand) == 1)
		{
			return ObjectPositionX(GGVR_Player.ObjLeftHand);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetLeftHandY( )
{
	if ( GGVR_Player.ObjLeftHand > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjLeftHand) == 1)
		{
			return ObjectPositionY(GGVR_Player.ObjLeftHand);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetLeftHandZ( )
{
	if ( GGVR_Player.ObjLeftHand > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjLeftHand) == 1)
		{
			return ObjectPositionZ(GGVR_Player.ObjLeftHand);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetLeftHandAngleX( )
{
	if ( GGVR_Player.ObjLeftHand > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjLeftHand) == 1)
		{
			return ObjectAngleX(GGVR_Player.ObjLeftHand);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetLeftHandAngleY( )
{
	if ( GGVR_Player.ObjLeftHand > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjLeftHand) == 1)
		{
			return ObjectAngleY(GGVR_Player.ObjLeftHand);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetLeftHandAngleZ( )
{
	if ( GGVR_Player.ObjLeftHand > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjLeftHand) == 1)
		{
			return ObjectAngleZ(GGVR_Player.ObjLeftHand);
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetBestHandX()
{
	if ( GGVR_Player.ObjTeleportStart > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjTeleportStart) == 1)
		{
			float fValue = ObjectPositionX ( GGVR_Player.ObjTeleportStart );
			return fValue;
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetBestHandY()
{
	//float fValue = GGVR_GetLeftHandY();
	//if ( fabs(GGVR_GetRightHandX()) > 0 ) fValue = GGVR_GetRightHandY();
	if ( GGVR_Player.ObjTeleportStart > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjTeleportStart) == 1)
		{
			float fValue = ObjectPositionY ( GGVR_Player.ObjTeleportStart );
			return fValue;
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetBestHandZ()
{
	//float fValue = GGVR_GetLeftHandZ();
	//if ( fabs(GGVR_GetRightHandX()) > 0 ) fValue = GGVR_GetRightHandZ();
	if ( GGVR_Player.ObjTeleportStart > 0 )
	{
		if (ObjectExist(GGVR_Player.ObjTeleportStart) == 1)
		{
			float fValue = ObjectPositionZ ( GGVR_Player.ObjTeleportStart );
			return fValue;
		}
		else { return 0; }
	}
	else { return 0; }
}

float GGVR_GetBestHandAngleX()
{
	float fValue = GGVR_GetLeftHandAngleX();
	if ( fabs(GGVR_GetRightHandX()) > 0 ) fValue = GGVR_GetRightHandAngleX();
	return fValue;
}

float GGVR_GetBestHandAngleY()
{
	float fValue = GGVR_GetLeftHandAngleY();
	if ( fabs(GGVR_GetRightHandX()) > 0 ) fValue = GGVR_GetRightHandAngleY();
	return fValue;
}

float GGVR_GetBestHandAngleZ()
{
	float fValue = GGVR_GetLeftHandAngleZ();
	if ( fabs(GGVR_GetRightHandX()) > 0 ) fValue = GGVR_GetRightHandAngleZ();
	return fValue;
}

int GGVR_GetLaserGuidedEntityObj ( int entityviewstartobj, int entityviewendobj )
{
	int iEntObjectNumber = 0;
	if ( GGVR_Player.LaserGuideActive > 0 )
	{
		int o = GGVR_Player.LaserGuideActive;
		if ( ObjectExist ( o ) == 1 )
		{
			// work out ray cast from laser object
			MoveObject ( o, -100.0f );
			float fX = ObjectPositionX ( o );
			float fY = ObjectPositionY ( o );
			float fZ = ObjectPositionZ ( o );
			MoveObject ( o, 200.0f );
			float fNewX = ObjectPositionX ( o );
			float fNewY = ObjectPositionY ( o );
			float fNewZ = ObjectPositionZ ( o );
			MoveObject ( o, -100.0f );

			// use this laser object to trace line and detect any entity object in its path
			int iIgnoreObjNo = GGVR_Player.LaserGuideActive;
			int ttt = IntersectAll(0,0,0,0,0,0,0,0,-123);
			int tthitvalue = IntersectAll(entityviewstartobj,entityviewendobj,fX, fY, fZ, fNewX, fNewY, fNewZ, iIgnoreObjNo);
			if ( tthitvalue > 0 )
			{
				// this entity was found when tracing the laser line
				iEntObjectNumber = tthitvalue;
			}
		}
	}
	return iEntObjectNumber;
}

// Generic Controller

int GGVR_RightControllerFound( )
{
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		if (GGVR_RHandIndex < 1 || GGVR_RHandIndex > vr::k_unMaxTrackedDeviceCount)
		{
		}
		else
		{
			return 1;
		}
		#endif
	}
	if ( GGVR_EnabledMode == 2 && GGVR_RHandIndex != -1 ) return 1;
	return 0;
}

int GGVR_LeftControllerFound( )
{
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		if (GGVR_LHandIndex < 1 || GGVR_LHandIndex > vr::k_unMaxTrackedDeviceCount)
		{
		}
		else
		{
			return 1;
		}
		#endif
	}
	if ( GGVR_EnabledMode == 2 && GGVR_LHandIndex != -1 ) return 1;
	return 0;
}

float GGVR_RightController_JoyX( void )
{
	return GGVR_ControllerState[0].rAxis[GGVR_RJoystick].x;
}

float GGVR_RightController_JoyY( void )
{
	return GGVR_ControllerState[0].rAxis[GGVR_RJoystick].y;
}

float GGVR_LeftController_JoyX( void )
{
	return GGVR_ControllerState[1].rAxis[GGVR_LJoystick].x;
}

float GGVR_LeftController_JoyY( void )
{
	return GGVR_ControllerState[1].rAxis[GGVR_LJoystick].y;
}

float GGVR_RightController_Trigger( void )
{
	return GGVR_ControllerState[0].rAxis[GGVR_RTrigger].x;
}

float GGVR_LeftController_Trigger( void )
{
	return GGVR_ControllerState[1].rAxis[GGVR_LTrigger].x;
}

int GGVR_RightController_Grip(void)
{
	if ( GGVR_ControllerState[0].rAxis[GGVR_RSideButton].x > 0.9f )
		return 1;
	else
		return 0;
	//if ( GGVR_EnabledMode == 1 )
	//{
	//	#ifdef USINGOPENVR
	//	if (GGVR_ControllerState[0].ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Grip))
	//	{
	//		return 1;
	//	}
	//	#endif
	//}
	//return 0;
}

int GGVR_LeftController_Grip(void)
{
	if ( GGVR_ControllerState[1].rAxis[GGVR_RSideButton].x > 0.9f )
		return 1;
	else
		return 0;
	//if ( GGVR_EnabledMode == 1 )
	//{
	//	#ifdef USINGOPENVR
	//	if (GGVR_ControllerState[1].ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Grip))
	//	{
	//		return 1;
	//	}
	//	#endif
	//}
	//return 0;
}

int GGVR_RightController_Button1(void)
{
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		if (GGVR_ControllerState[0].ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu))
		{
			return 1;
		}
		#endif
	}
	return 0;
}

int GGVR_LeftController_Button1(void)
{
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		if (GGVR_ControllerState[1].ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu))
		{
			return 1;
		}
		#endif
	}
	return 0;
}

int GGVR_RightController_Button2(void)
{
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		if (GGVR_ControllerState[0].ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Axis0) || GGVR_ControllerState[0].ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_A))
		{
			return 1;
		}
		#endif
	}
	return 0;
}

int GGVR_LeftController_Button2(void)
{
	#ifdef USINGOPENVR
	if (GGVR_ControllerState[1].ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Axis0) || GGVR_ControllerState[1].ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_A))
	{
		return 1;
	}
	#endif
	return 0;
}

int GGVR_RightController_ButtonPressed(int Button)
{
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		if (Button < vr::k_EButton_Max)
		{
			if (GGVR_ControllerState[0].ulButtonPressed & vr::ButtonMaskFromId((vr::EVRButtonId)Button))
			{
				return 1;
			}
		}
		#endif
	}
	return 0;
}

int GGVR_LeftController_ButtonPressed(int Button)
{
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		if (Button < vr::k_EButton_Max)
		{
			if (GGVR_ControllerState[1].ulButtonPressed & vr::ButtonMaskFromId((vr::EVRButtonId)Button))
			{
				return 1;
			}
		}
		#endif
	}
	return 0;
}

void GGVR_LeftController_TriggerPulse( int axis, int duration_ms)
{
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		if (GGVR_init_error == vr::VRInitError_None && ivr_compositor != NULL)
		{
			ivr_system->TriggerHapticPulse(GGVR_LHandIndex, axis, duration_ms);
		}
		#endif
	}
}

void GGVR_RightController_TriggerPulse( int axis, int duration_ms)
{
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		if (GGVR_init_error == vr::VRInitError_None && ivr_compositor != NULL)
		{
			ivr_system->TriggerHapticPulse(GGVR_RHandIndex, axis, duration_ms);
		}
		#endif
	}
}

int GGVR_RightController_GetButtonPressed( int button )
{
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		if (GGVR_ControllerState[0].ulButtonPressed & vr::ButtonMaskFromId((vr::EVRButtonId)button))
		{
			return 1;
		}
		#endif
	}
	return 0;
}

int GGVR_LeftController_GetButtonPressed( int button )
{
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		if (GGVR_ControllerState[1].ulButtonPressed & vr::ButtonMaskFromId((vr::EVRButtonId)button))
		{
			return 1;
		}
		#endif
	}
	return 0;
}

int GGVR_RightController_GetButtonTouched( int button )
{
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		if (GGVR_ControllerState[0].ulButtonTouched & vr::ButtonMaskFromId((vr::EVRButtonId)button))
		{
			return 1;
		}
		#endif
	}
	return 0;
}

int GGVR_LeftController_GetButtonTouched( int button )
{
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		if (GGVR_ControllerState[1].ulButtonTouched & vr::ButtonMaskFromId((vr::EVRButtonId)button))
		{
			return 1;
		}
		#endif
	}
	return 0;
}

int GGVR_RightController_GetFingerPointed()
{
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		if (GGVR_ControllerState[0].ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_Axis1))
		{
			return 0;
		}
		#endif
	}
	return 1;
}

int GGVR_LeftController_GetFingerPointed()
{
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		if (GGVR_ControllerState[1].ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_Axis1))
		{
			return 0;
		}
		#endif
	}
	return 1;
}

int GGVR_RightController_GetThumbUp()
{
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		if (GGVR_ControllerState[0].ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_Axis0) || GGVR_ControllerState[0].ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_A) || GGVR_ControllerState[0].ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu))
		{
			return 0;
		}
		#endif
	}
	return 1;
}

int GGVR_LeftController_GetThumbUp()
{
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		if (GGVR_ControllerState[1].ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_Axis0) || GGVR_ControllerState[1].ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_A) || GGVR_ControllerState[1].ulButtonTouched & vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu))
		{
			return 0;
		}
		#endif
	}
	return 1;
}

float GGVR_RightController_AxisTriggerX(int axis)
{
	#ifdef USINGOPENVR
	if (axis >= 0 && axis < vr::k_unControllerStateAxisCount)
	{
		return GGVR_ControllerState[0].rAxis[axis].x;
	}
	#endif
	return 0.0f;
}

float GGVR_RightController_AxisTriggerY(int axis)
{
	#ifdef USINGOPENVR
	if (axis >= 0 && axis < vr::k_unControllerStateAxisCount)
	{
		return GGVR_ControllerState[0].rAxis[axis].y;
	}
	#endif
	return 0.0f;
}

float GGVR_LeftController_AxisTriggerX(int axis)
{
	#ifdef USINGOPENVR
	if (axis >= 0 && axis < vr::k_unControllerStateAxisCount)
	{
		return GGVR_ControllerState[1].rAxis[axis].x;
	}
	#endif
	return 0.0f;
}

float GGVR_LeftController_AxisTriggerY(int axis)
{
	#ifdef USINGOPENVR
	if (axis >= 0 && axis < vr::k_unControllerStateAxisCount)
	{
		return GGVR_ControllerState[1].rAxis[axis].y;
	}
	#endif
	return 0.0f;
}

int GGVR_PlayerData::Create( LPSTR pRootDir )
{
	// Clear teleport work objects
	for ( int o = GGVR_Player.ObjTeleportStart; o <= GGVR_Player.ObjTeleportFinish; o++ )
	{
		if ( ObjectExist ( o ) ) DeleteObject ( o );
	}

	//ObjBase
	MakeObjectCone(GGVR_Player.ObjBase, 0.1f*GGVR_WorldScale);
	RotateObject(GGVR_Player.ObjBase, 90.0, 0.0, 0.0);
	FixObjectPivot(GGVR_Player.ObjBase);
	SetObjectCollisionOff(GGVR_Player.ObjBase);

	//ObjHead
	MakeObjectCone(GGVR_Player.ObjHead, 0.1f*GGVR_WorldScale);
	RotateObject(GGVR_Player.ObjHead, 90.0, 0.0, 0.0);
	FixObjectPivot(GGVR_Player.ObjHead);
	SetObjectCollisionOff(GGVR_Player.ObjHead);

	//ObjOrigin
	MakeObjectCone(GGVR_Player.ObjOrigin, 0.1f*GGVR_WorldScale);
	RotateObject(GGVR_Player.ObjOrigin, 90.0, 0.0, 0.0);
	FixObjectPivot(GGVR_Player.ObjOrigin);
	SetObjectCollisionOff(GGVR_Player.ObjOrigin);

	//ObjRightHand
	//MakeObjectBox(GGVR_Player.ObjRightHand, 0.3f, 0.3f, 8.0f);//0.1f*GGVR_WorldScale);
	char pControllerModel[1024];
	strcpy ( pControllerModel, pRootDir );
	strcat ( pControllerModel, "\\gamecore\\vrcontroller\\vrcontrollerright.x" );
	LoadObject ( pControllerModel, GGVR_Player.ObjRightHand );
	SetObjectCollisionOff(GGVR_Player.ObjRightHand);

	//ObjLeftHand
	//MakeObjectCone(GGVR_Player.ObjLeftHand, 0.1f*GGVR_WorldScale);
	strcpy ( pControllerModel, pRootDir );
	strcat ( pControllerModel, "\\gamecore\\vrcontroller\\vrcontrollerleft.x" );
	LoadObject ( pControllerModel, GGVR_Player.ObjLeftHand );
	//ScaleObject ( GGVR_Player.ObjLeftHand, -100.0f, 100.0f, 100.0f );
	//SetObjectCull ( GGVR_Player.ObjLeftHand, 1 );
	//RotateObject(GGVR_Player.ObjLeftHand, 90.0, 0.0, 0.0);
	//FixObjectPivot(GGVR_Player.ObjLeftHand);
	SetObjectCollisionOff(GGVR_Player.ObjLeftHand);

	// make controller objects renderable
	for ( int iM = 0; iM < 2; iM++ )
	{
		int iMObj = 0;
		if ( iM == 0 ) iMObj = GGVR_Player.ObjRightHand;
		if ( iM == 1 ) iMObj = GGVR_Player.ObjLeftHand;
		//DisableObjectZDepth ( iMObj );
		//DisableObjectZRead ( iMObj );
		SetSphereRadius ( iMObj, 0 );
		SetObjectMask ( iMObj, (1<<6) + (1<<7) + 1 );
		TextureObject ( iMObj, 7, GGVR_Player.ControllerTextureID1 );
		TextureObject ( iMObj, 0, GGVR_Player.ControllerTextureID0 );
		TextureObject ( iMObj, 1, GGVR_Player.ControllerTextureID1 );
		TextureObject ( iMObj, 2, GGVR_Player.ControllerTextureID2 );
		TextureObject ( iMObj, 3, GGVR_Player.ControllerTextureID3 );
		TextureObject ( iMObj, 4, GGVR_Player.ControllerTextureID4 );
		TextureObject ( iMObj, 5, GGVR_Player.ControllerTextureID5 );
		TextureObject ( iMObj, 6, GGVR_Player.ControllerTextureID6 );
		SetObjectEffect ( iMObj, GGVR_Player.ControllerShaderID );
		DisableObjectZDepth(iMObj);
	}

	//PlayerVars
	PitchLock = 0;
	TurnLock = 1;

	// success
	return 0;
}

void GGVR_PlayerData::Destroy( )
{
	// Delete playerdata object resources
	if ( ObjectExist(GGVR_Player.ObjBase)==1 ) DeleteObject ( GGVR_Player.ObjBase );
	if ( ObjectExist(GGVR_Player.ObjHead)==1 ) DeleteObject ( GGVR_Player.ObjHead );
	if ( ObjectExist(GGVR_Player.ObjOrigin)==1 ) DeleteObject ( GGVR_Player.ObjOrigin );
	if ( ObjectExist(GGVR_Player.ObjRightHand)==1 ) DeleteObject ( GGVR_Player.ObjRightHand );
	if ( ObjectExist(GGVR_Player.ObjLeftHand)==1 ) DeleteObject ( GGVR_Player.ObjLeftHand );

	// Delete any teleport/laser objects
	for ( int o = GGVR_Player.ObjTeleportStart; o <= GGVR_Player.ObjTeleportFinish; o++ )
		if ( ObjectExist(o) ) 
			DeleteObject ( o );
}

int GGVR_PlayerData::GetOriginObjID()
{
	return ObjOrigin;
}

int GGVR_PlayerData::GetHeadObjID()
{
	return ObjHead;
}

int GGVR_PlayerData::GetBaseObjID()
{
	return ObjBase;
}

int GGVR_PlayerData::GetRHandObjID()
{
	return ObjRightHand;
}

int GGVR_PlayerData::GetLHandObjID()
{
	return ObjLeftHand;
}

// WMR

int GGVR_CreateHolographicSpace1 ( HWND hWnd, LPSTR pRootPath, int iDebugMode )
{
	//DebugGGVRlog ( "Calling GGWMR_CreateHolographicSpace1" );
	try
	{
		int iAdapterOrdinal = 0;
		char pAdapterName[2048];
		int iFeatureLevel = 0;
		GetD3DExtraInfo ( &iAdapterOrdinal, pAdapterName, &iFeatureLevel );
		char pDebugStr[2048];
		sprintf ( pDebugStr, "Graphics Adapter Ordinal : %d", iAdapterOrdinal ); //DebugGGVRlog ( pDebugStr );
		sprintf ( pDebugStr, "Graphics Adapter Name : %s", pAdapterName ); //DebugGGVRlog ( pDebugStr );
		sprintf ( pDebugStr, "Graphics Feature Level : %d", iFeatureLevel ); //DebugGGVRlog ( pDebugStr );
		return GGWMR_CreateHolographicSpace1 ( hWnd, iDebugMode );
	}
	catch (exception& e)
	{
		const char* pWhatStr = e.what();
		///MessageBox ( NULL, pWhatStr, "GGWMR_CreateHolographicSpace1 Exception", MB_OK );   
	}
	return 987;
}

void GGVR_ReconnectWithHolographicSpaceControllers ( void )
{
	if ( GGWMR_ReconnectWithHolographicSpaceControllers )
		GGWMR_ReconnectWithHolographicSpaceControllers();
}

int GGVR_CreateHolographicSpace2 ( void* pDevice, void* pContext )
{
	return GGWMR_CreateHolographicSpace2 ( pDevice, pContext );
}

int GGVR_PreSubmit ( int iDebugMode )
{
	// WMR prepares the views to be rendered to (not taking renders after they have done as with GGVR_Submit)
	if ( GGVR_EnabledMode == 2 )
	{
		if ( GGVR_EnabledState == 1 )
		{
			// true debug mode
			int iTrueDebugMode = iDebugMode;
			if ( iTrueDebugMode >=2 ) iTrueDebugMode -= 2;

			// initialise VR hardware
			//DebugGGVRlog ( "Calling GGVR_CreateHolographicSpace1" );
			int iErrorCode = GGVR_CreateHolographicSpace1 ( g_pGlob->hOriginalhWnd, "", iTrueDebugMode );
			char pErrStr[1024];
			sprintf ( pErrStr, "Error Value From GGVR_CreateHolographicSpace1 : %d", iErrorCode );
			//DebugGGVRlog ( pErrStr );
			if ( iErrorCode > 0 ) { GGVR_EnabledState=99; return iErrorCode; }
			iErrorCode = GGVR_CreateHolographicSpace2 ( m_pD3D, m_pImmediateContext );
			if ( iErrorCode > 0 ) { GGVR_EnabledState=99; return iErrorCode; }
			ShowWindow(g_pGlob->hOriginalhWnd, SW_SHOW);
			UpdateWindow(g_pGlob->hOriginalhWnd);

			// small delay to allow WMR portal to show properly
			Sleep ( 2000 );

			// if Windows Mixed Reality Portal pushed its way to the front, reassert desktop window to the front
			SetForegroundWindow(g_pGlob->hWnd);
			SetWindowPos(g_pGlob->hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

			// state to start rendering
			GGVR_EnabledState = 2;
		}
	}
	if ( GGVR_EnabledMode == 2 )
	{
		if ( GGVR_EnabledState == 0 )
		{
			GGVR_EnabledState = 1;
		}
	}
	if ( GGVR_EnabledMode == 2 && iDebugMode < 2 )
	{
		if ( GGVR_EnabledState == 2 )
		{
			// Get render target and depth stencil views from HolographicSpace, so can override
			// left and right camera ptrs so they render DIRECT to the HS backbuffer/depth targets
			ID3D11RenderTargetView* pRenderTargetLeftView = NULL;
			ID3D11RenderTargetView* pRenderTargetRightView = NULL;
			ID3D11DepthStencilView* pDepthStencilView = NULL;
			ID3D11ShaderResourceView* pLeftShaderResourceView = NULL;
			DWORD dwWidth, dwHeight;
			GGWMR_GetRenderTargetAndDepthStencilView ( (void**)&pRenderTargetLeftView, (void**)&pRenderTargetRightView, (void**)&pDepthStencilView, &dwWidth, &dwHeight, (void**)&pLeftShaderResourceView );
			if ( pRenderTargetLeftView == NULL )
			{
				// tried to run VR but no headset to render to
				GGVR_EnabledState = 3;
			}
			else
			{
				// additionally, copy left view shader resource view (so can paste to camera zero for quick preview)
				SetCameraToView ( 6, pRenderTargetLeftView, pDepthStencilView, dwWidth, dwHeight, pLeftShaderResourceView );
				SetCameraToView ( 7, pRenderTargetRightView, pDepthStencilView, dwWidth, dwHeight, NULL );

				// get projection matrix for left and right eyes
				float fM00, fM10, fM20, fM30;
				float fM01, fM11, fM21, fM31;
				float fM02, fM12, fM22, fM32;
				float fM03, fM13, fM23, fM33;
				GGWMR_GetProjectionMatrix ( 0, &fM00, &fM10, &fM20, &fM30, &fM01, &fM11, &fM21, &fM31, &fM02, &fM12, &fM22, &fM32, &fM03, &fM13, &fM23, &fM33 );
				GGVR_LeftEyeProjection = GGMATRIX ( fM00, fM10, fM20, fM30, fM01, fM11, fM21, fM31, fM02, fM12, fM22, fM32, fM03, fM13, fM23, fM33 );
				GGWMR_GetProjectionMatrix ( 1, &fM00, &fM10, &fM20, &fM30, &fM01, &fM11, &fM21, &fM31, &fM02, &fM12, &fM22, &fM32, &fM03, &fM13, &fM23, &fM33 );
				GGVR_RightEyeProjection = GGMATRIX ( fM00, fM10, fM20, fM30, fM01, fM11, fM21, fM31, fM02, fM12, fM22, fM32, fM03, fM13, fM23, fM33 );
			}
		}
		if ( GGVR_EnabledState == 3 )
		{
			// headset missing - prompt user of game to restart software (to init with live headset)
			return -123;
		}
	}
	return 0;
}

void GGVR_SetCameraRange( float Near, float Far )
{
	GGVR_NearClip = Near;
	GGVR_FarClip = Far;

	SetCameraRange(GGVR_RCamID, GGVR_NearClip, GGVR_FarClip);
	SetCameraRange(GGVR_LCamID, GGVR_NearClip, GGVR_FarClip);

	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		vr::HmdMatrix44_t HMDMat_Left;
		vr::HmdMatrix44_t HMDMat_Right;
		HMDMat_Left = ivr_system->GetProjectionMatrix(vr::Eye_Left, GGVR_NearClip, GGVR_FarClip);
		HMDMat_Right = ivr_system->GetProjectionMatrix(vr::Eye_Right, GGVR_NearClip, GGVR_FarClip);
		GGVR_LeftEyeProjection._11 = HMDMat_Left.m[0][0]; GGVR_LeftEyeProjection._12 = HMDMat_Left.m[0][1]; GGVR_LeftEyeProjection._13 = HMDMat_Left.m[0][2]; GGVR_LeftEyeProjection._14 = HMDMat_Left.m[0][3];
		GGVR_LeftEyeProjection._21 = HMDMat_Left.m[1][0]; GGVR_LeftEyeProjection._22 = HMDMat_Left.m[1][1]; GGVR_LeftEyeProjection._23 = HMDMat_Left.m[1][2]; GGVR_LeftEyeProjection._24 = HMDMat_Left.m[1][3];
		GGVR_LeftEyeProjection._31 = HMDMat_Left.m[2][0]; GGVR_LeftEyeProjection._32 = HMDMat_Left.m[2][1]; GGVR_LeftEyeProjection._33 = HMDMat_Left.m[2][2]; GGVR_LeftEyeProjection._34 = HMDMat_Left.m[2][3];
		GGVR_LeftEyeProjection._41 = HMDMat_Left.m[3][0]; GGVR_LeftEyeProjection._42 = HMDMat_Left.m[3][1]; GGVR_LeftEyeProjection._43 = HMDMat_Left.m[3][2]; GGVR_LeftEyeProjection._44 = HMDMat_Left.m[3][3];
		GGVR_RightEyeProjection._11 = HMDMat_Right.m[0][0]; GGVR_RightEyeProjection._12 = HMDMat_Right.m[0][1]; GGVR_RightEyeProjection._13 = HMDMat_Right.m[0][2]; GGVR_RightEyeProjection._14 = HMDMat_Right.m[0][3];
		GGVR_RightEyeProjection._21 = HMDMat_Right.m[1][0]; GGVR_RightEyeProjection._22 = HMDMat_Right.m[1][1]; GGVR_RightEyeProjection._23 = HMDMat_Right.m[1][2]; GGVR_RightEyeProjection._24 = HMDMat_Right.m[1][3];
		GGVR_RightEyeProjection._31 = HMDMat_Right.m[2][0]; GGVR_RightEyeProjection._32 = HMDMat_Right.m[2][1]; GGVR_RightEyeProjection._33 = HMDMat_Right.m[2][2]; GGVR_RightEyeProjection._34 = HMDMat_Right.m[2][3];
		GGVR_RightEyeProjection._41 = HMDMat_Right.m[3][0]; GGVR_RightEyeProjection._42 = HMDMat_Right.m[3][1]; GGVR_RightEyeProjection._43 = HMDMat_Right.m[3][2]; GGVR_RightEyeProjection._44 = HMDMat_Right.m[3][3];
		#endif
	}
	if ( GGVR_EnabledMode == 2 )
	{
		// done in realtime
	}
}

void GGVR_Submit ( void )
{
	if ( GGVR_EnabledMode == 1 )
	{
		#ifdef USINGOPENVR
		if (GGVR_init_error == vr::VRInitError_None && ivr_compositor != NULL)
		{
			GGVR_CompositorError = ivr_compositor->Submit(vr::Eye_Left, &GGVR_tex_info[GGVR_LEye], &GGVR_texture_bounds[GGVR_LEye], GGVR_SubmitFlags);
			GGVR_CompositorError = ivr_compositor->Submit(vr::Eye_Right, &GGVR_tex_info[GGVR_REye], &GGVR_texture_bounds[GGVR_REye], GGVR_SubmitFlags);
		}
		#endif
	}
	if ( GGVR_EnabledMode == 2 )
	{
		if ( GGVR_EnabledState == 2 )
		{
			GGWMR_Present();
		}
	}
}

// OpenVR

#ifdef USINGOPENVR
int	GGVR_IsRuntimeInstalled()
{
	bool result = 0;
	if ( GGVR_EnabledMode == 1 ) result = vr::VR_IsRuntimeInstalled();
	return (int)result;
}

void GGVR_SuspendRendering(int flag)
{
	if ( GGVR_EnabledMode == 1 )
	{
		if (flag == 1)
		{
			ivr_compositor->SuspendRendering(true);
		}
		else
		{
			ivr_compositor->SuspendRendering(false);
		}
	}
}

void GGVR_ErrorMessagesOn( int state )
{
	if (state == 0)
	{
		GGVR_ErrorMessageOn = false;
	}
	else { GGVR_ErrorMessageOn = true; }
}

void GGVR_ResetSeatedZeroPose()
{
	if ( GGVR_EnabledMode == 1 )
	{
		if (GGVR_init_error == vr::VRInitError_None && ivr_compositor != NULL)
		{
			ivr_system->ResetSeatedZeroPose();
		}
	}
}

void GGVR_Submit_RightEye()
{
	if ( GGVR_EnabledMode == 1 )
	{
		if (GGVR_init_error == vr::VRInitError_None && ivr_compositor != NULL)
		{		
			GGVR_CompositorError = ivr_compositor->Submit(vr::Eye_Right, &GGVR_tex_info[GGVR_REye], &GGVR_texture_bounds[GGVR_REye], GGVR_SubmitFlags);
		}
	}
}

void GGVR_Submit_LeftEye()
{
	if ( GGVR_EnabledMode == 1 )
	{
		if (GGVR_init_error == vr::VRInitError_None && ivr_compositor != NULL)
		{
			GGVR_CompositorError = ivr_compositor->Submit(vr::Eye_Left, &GGVR_tex_info[GGVR_LEye], &GGVR_texture_bounds[GGVR_LEye], GGVR_SubmitFlags);
		}
	}
}

void GGVR_CreateSkyboxOveride(int Front, int Back, int Left, int Right, int Top, int Bottom)
{
	GGVR_Skybox_Res[0] = GetImagePointer(Front);
	GGVR_Skybox_Res[1] = GetImagePointer(Back);
	GGVR_Skybox_Res[2] = GetImagePointer(Left);
	GGVR_Skybox_Res[3] = GetImagePointer(Right);
	GGVR_Skybox_Res[4] = GetImagePointer(Top);
	GGVR_Skybox_Res[5] = GetImagePointer(Bottom);
	if ( GGVR_EnabledMode == 1 )
	{
		for (int i = 0; i < 6; i++)
		{
			GGVR_CustomSkybox[i].eType = vr::TextureType_DirectX;
			GGVR_CustomSkybox[i].eColorSpace = vr::ColorSpace_Auto;
		}
	}
	GGVR_CustomSkybox[0].handle = GetImagePointer(Front);
	GGVR_CustomSkybox[1].handle = GetImagePointer(Back);
	GGVR_CustomSkybox[2].handle = GetImagePointer(Left);
	GGVR_CustomSkybox[3].handle = GetImagePointer(Right);
	GGVR_CustomSkybox[4].handle = GetImagePointer(Top);
	GGVR_CustomSkybox[5].handle = GetImagePointer(Bottom);
	if ( GGVR_EnabledMode == 1 )
	{
		ivr_compositor->SetSkyboxOverride(GGVR_CustomSkybox, 6);
	}
}

void GGVR_ClearSkyboxOveride()
{
	if ( GGVR_EnabledMode == 1 )
	{
		ivr_compositor->ClearSkyboxOverride();
	}
}

float GGVR_Chaperone_GetPlayAreaSizeX()
{
	float SizeX;
	float SizeZ;
	if ( GGVR_EnabledMode == 1 ) Chaperone->GetPlayAreaSize(&SizeX, &SizeZ);
	return SizeX * GGVR_WorldScale;
}

float GGVR_Chaperone_GetPlayAreaSizeZ()
{
	float SizeX;
	float SizeZ;
	if ( GGVR_EnabledMode == 1 ) Chaperone->GetPlayAreaSize(&SizeX, &SizeZ);
	return SizeZ * GGVR_WorldScale;
}

int GGVR_Chaperone_AreBoundsVisible()
{
	if ( GGVR_EnabledMode == 1 ) 
		return Chaperone->AreBoundsVisible();
	else
		return 0;
}

void GGVR_Chaperone_ForceBoundsVisible(int flag)
{
	if ( GGVR_EnabledMode == 1 ) 
	{
		if (flag == 0)
		{
			Chaperone->ForceBoundsVisible(false);
		}
		else
		{
			Chaperone->ForceBoundsVisible(true);
		}
	}
}

float GGVR_Chaperone_GetPlayAreaPoint1X()
{
	if ( GGVR_EnabledMode == 1 ) 
	{
		vr::HmdQuad_t Rect;
		Chaperone->GetPlayAreaRect(&Rect);
		return Rect.vCorners[0].v[0];
	}
	return 0.0f;
}

float GGVR_Chaperone_GetPlayAreaPoint1Y()
{
	if ( GGVR_EnabledMode == 1 ) 
	{
		vr::HmdQuad_t Rect;
		Chaperone->GetPlayAreaRect(&Rect);
		return Rect.vCorners[0].v[1];
	}
	return 0.0f;
}

float GGVR_Chaperone_GetPlayAreaPoint1Z()
{
	if ( GGVR_EnabledMode == 1 ) 
	{
		vr::HmdQuad_t Rect;
		Chaperone->GetPlayAreaRect(&Rect);
		return Rect.vCorners[0].v[2];
	}
	return 0.0f;
}

float GGVR_Chaperone_GetPlayAreaPoint2X()
{
	if ( GGVR_EnabledMode == 1 ) 
	{
		vr::HmdQuad_t Rect;
		Chaperone->GetPlayAreaRect(&Rect);
		return Rect.vCorners[1].v[0];
	}
	return 0.0f;
}

float GGVR_Chaperone_GetPlayAreaPoint2Y()
{
	if ( GGVR_EnabledMode == 1 ) 
	{
		vr::HmdQuad_t Rect;
		Chaperone->GetPlayAreaRect(&Rect);
		return Rect.vCorners[1].v[1];
	}
	return 0.0f;
}

float GGVR_Chaperone_GetPlayAreaPoint2Z()
{
	if ( GGVR_EnabledMode == 1 ) 
	{
		vr::HmdQuad_t Rect;
		Chaperone->GetPlayAreaRect(&Rect);
		return Rect.vCorners[1].v[2];
	}
	return 0.0f;
}

float GGVR_Chaperone_GetPlayAreaPoint3X()
{
	if ( GGVR_EnabledMode == 1 ) 
	{
		vr::HmdQuad_t Rect;
		Chaperone->GetPlayAreaRect(&Rect);
		return Rect.vCorners[2].v[0];
	}
	return 0.0f;
}

float GGVR_Chaperone_GetPlayAreaPoint3Y()
{
	if ( GGVR_EnabledMode == 1 ) 
	{
		vr::HmdQuad_t Rect;
		Chaperone->GetPlayAreaRect(&Rect);
		return Rect.vCorners[2].v[1];
	}
	return 0.0f;
}

float GGVR_Chaperone_GetPlayAreaPoint3Z()
{
	if ( GGVR_EnabledMode == 1 ) 
	{
		vr::HmdQuad_t Rect;
		Chaperone->GetPlayAreaRect(&Rect);
		return Rect.vCorners[2].v[2];
	}
	return 0.0f;
}

float GGVR_Chaperone_GetPlayAreaPoint4X()
{
	if ( GGVR_EnabledMode == 1 ) 
	{
		vr::HmdQuad_t Rect;
		Chaperone->GetPlayAreaRect(&Rect);
		return Rect.vCorners[3].v[0];
	}
	return 0.0f;
}

float GGVR_Chaperone_GetPlayAreaPoint4Y()
{
	if ( GGVR_EnabledMode == 1 ) 
	{
		vr::HmdQuad_t Rect;
		Chaperone->GetPlayAreaRect(&Rect);
		return Rect.vCorners[3].v[1];
	}
	return 0.0f;
}

float GGVR_Chaperone_GetPlayAreaPoint4Z()
{
	if ( GGVR_EnabledMode == 1 ) 
	{
		vr::HmdQuad_t Rect;
		Chaperone->GetPlayAreaRect(&Rect);
		return Rect.vCorners[3].v[2];
	}
	return 0.0f;
}

char* GGVR_HMD_GetSerialNumber()
{
	char *cstr = new char[GGVR_HMD_SerialNo.length() + 1];
	strcpy(cstr, GGVR_HMD_SerialNo.c_str());
	return cstr;
}

char* GGVR_HMD_GetTrackingSysName()
{
	char *cstr = new char[GGVR_HMD_TrackingSysName.length() + 1];
	strcpy(cstr, GGVR_HMD_TrackingSysName.c_str());
	return cstr;
}

char* GGVR_HMD_GetModelNumber()
{
	char *cstr = new char[GGVR_HMD_ModelNo.length() + 1];
	strcpy(cstr, GGVR_HMD_ModelNo.c_str());
	return cstr;
}

char* GGVR_HMD_GetManufacturer()
{
	char *cstr = new char[GGVR_HMD_Manufacturer.length() + 1];
	strcpy(cstr, GGVR_HMD_Manufacturer.c_str());
	return cstr;
}

char* GGVR_RCntrl_GetSerialNumber()
{
	char *cstr = new char[GGVR_RCntrl_SerialNo.length() + 1];
	strcpy(cstr, GGVR_RCntrl_SerialNo.c_str());
	return cstr;
}

char* GGVR_RCntrl_GetTrackingSysName()
{
	char *cstr = new char[GGVR_RCntrl_TrackingSysName.length() + 1];
	strcpy(cstr, GGVR_RCntrl_TrackingSysName.c_str());
	return cstr;
}

char* GGVR_RCntrl_GetModelNumber()
{
	char *cstr = new char[GGVR_RCntrl_ModelNo.length() + 1];
	strcpy(cstr, GGVR_RCntrl_ModelNo.c_str());
	return cstr;
}

char* GGVR_RCntrl_GetManufacturer()
{
	char *cstr = new char[GGVR_RCntrl_Manufacturer.length() + 1];
	strcpy(cstr, GGVR_RCntrl_Manufacturer.c_str());
	return cstr;
}

char* GGVR_LCntrl_GetSerialNumber()
{
	char *cstr = new char[GGVR_LCntrl_SerialNo.length() + 1];
	strcpy(cstr, GGVR_LCntrl_SerialNo.c_str());
	return cstr;
}

char* GGVR_LCntrl_GetTrackingSysName()
{
	char *cstr = new char[GGVR_LCntrl_TrackingSysName.length() + 1];
	strcpy(cstr, GGVR_LCntrl_TrackingSysName.c_str());
	return cstr;
}

char* GGVR_LCntrl_GetModelNumber()
{
	char *cstr = new char[GGVR_LCntrl_ModelNo.length() + 1];
	strcpy(cstr, GGVR_LCntrl_ModelNo.c_str());
	return cstr;
}

char* GGVR_LCntrl_GetManufacturer()
{
	char *cstr = new char[GGVR_LCntrl_Manufacturer.length() + 1];
	strcpy(cstr, GGVR_LCntrl_Manufacturer.c_str());
	return cstr;
}
#endif
