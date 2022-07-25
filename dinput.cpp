/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

//-----------------------------------------------------------------------------
// 
// This file exists only on the windows build and contains code for the DI 
// interface.
// 
// This file is originally called dinput.c, however due to pch conflicts with
// dinput.h from the platform SDK I needed to change the name for local engine
// file.
// 
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// 
// Defines
// 
//-----------------------------------------------------------------------------

// Indexes to originalmouseparms[] & newmouseparms[]
#define DI_MSPM_THRESHOLD_X				0
#define DI_MSPM_THRESHOLD_Y				1
#define DI_MSPM_ACCELERATE				2
#define DI_MSPM_MAX						3

// Used when failed to get the original mouse parameters from the systen
#define DI_MOUSE_DEFAULT_THRESHOLD_X	7 // Default acceleration 1
#define DI_MOUSE_DEFAULT_THRESHOLD_X	0 // Default acceleration 2
#define DI_MOUSE_DEFAULT_ACCELERATION	1 // Default speed

// Used when trying to get the acceleration value from the system
#define DI_MOUSE_ACCELERATION_MIN		1
#define DI_MOUSE_ACCELERATION_MAX		20
#define DI_MOUSE_ACCELERATION_DEFAULT	10

// Keyboard repeat delay in milliseconds
#define DI_KEYBOARD_DELAY_ULTRA_FAST	250
#define DI_KEYBOARD_DELAY_FASTER		500
#define DI_KEYBOARD_DELAY_FAST			700
#define DI_KEYBOARD_DELAY_SLOW			1000 // Default value

// Keyboard repeat rate constants
#define DI_KEYBOARD_SPEED_MINVAL		0
#define DI_KEYBOARD_SPEED_MAXVAL		32
#define DI_KEYBOARD_SPEED_LIMIT_MS		400 // 400ms delay

#define DI_MSTATE_BUTTON0_BIT			(1 << 0) // Left mouse
#define DI_MSTATE_BUTTON1_BIT			(1 << 1) // Right mouse
#define DI_MSTATE_BUTTON2_BIT			(1 << 2) // Middle mouse
#define DI_MSTATE_BUTTON3_BIT			(1 << 3) // Mouse4
#define DI_MSTATE_BUTTON4_BIT			(1 << 4) // Mouse5

// Returns true if the button was pressed. Whenether the button was
// pressed or not indecates the most-significant bit inside the first
// byte of dwData. (X000 0000)
#define DI_BUTTON_PRESSED(dwData) ((dwData & 0x80) == 0 ? false : true)

// The amount of objecs inside data object
#define DI_NUM_OBJECTS(object) (sizeof(object) / sizeof(object[0]))

//-----------------------------------------------------------------------------
// 
// Globals
// 
//-----------------------------------------------------------------------------

// Holds true if DI code is initialized
qboolean    DInput_Initialized;

// Holds version of the DI interface
DWORD       DInput_Version;

// True if mouse or keyboard are enabled
qboolean    DInput_MouseEnabled, DInput_KeyboardEnabled;

int			originalmouseparms[DI_MSPM_MAX], newmouseparms[DI_MSPM_MAX] = { 0, 0, 1 };

// Keyboard repeat-delay setting
int			keyboard_delay;

// Keyboard system key repeat-rate
int			keyboard_speed;

// Active mouse positions
int			mouse_x, mouse_y;

// DInput mouse active state. see DI_MSTATE_* macros
int			mstate_di;

// Variables for key events
int			key_last, key_ticks_pressed, key_ticks_repeat;

// DI interface
static LPDIRECTINPUT		g_pdi;

// Mouse and keyboard devices
static LPDIRECTINPUTDEVICE	g_pMouse, g_pKeyboard;

//-----------------------------------------------------------------------------
// 
// DI data format
// 
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Old data structure for DI data format used by Quake Engine.
// 
// Note:	The old format supports up to 4 mouse buttons.
//-----------------------------------------------------------------------------
struct DI_DataFormatOld
{
	// Mouse axes
	LONG lX, lY, lZ;

	// Supported mouse buttons
	BYTE bButtonA, bButtonB, bButtonC, bButtonD;

}; // Size 16 bytes

//-----------------------------------------------------------------------------
// Purpose: Old data format used by old Quake Engine.
//-----------------------------------------------------------------------------
static DIOBJECTDATAFORMAT DInput_ObjectDataFormatOld[] =
{
	{ &GUID_XAxis,	FIELD_OFFSET(DI_DataFormatOld, lX),			DIDFT_AXIS | DIDFT_ANYINSTANCE,					NULL },
	{ &GUID_YAxis,	FIELD_OFFSET(DI_DataFormatOld, lY),			DIDFT_AXIS | DIDFT_ANYINSTANCE,					NULL },
	{ &GUID_ZAxis,	FIELD_OFFSET(DI_DataFormatOld, lZ),			0x80000000 | DIDFT_AXIS | DIDFT_ANYINSTANCE,	NULL },
	{ nullptr,		FIELD_OFFSET(DI_DataFormatOld, bButtonA),	DIDFT_BUTTON | DIDFT_ANYINSTANCE,				NULL },
	{ nullptr,		FIELD_OFFSET(DI_DataFormatOld, bButtonB),	DIDFT_BUTTON | DIDFT_ANYINSTANCE,				NULL },
	{ nullptr,		FIELD_OFFSET(DI_DataFormatOld, bButtonC),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE,	NULL },
	{ nullptr,		FIELD_OFFSET(DI_DataFormatOld, bButtonD),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE,	NULL },
};

//-----------------------------------------------------------------------------
// Purpose: Old data structure for DI data format used by Quake Engine.
// 
// Note:	The new format supports up to 8 mouse buttons.
//-----------------------------------------------------------------------------
struct DI_DataFormatNew
{
	// Mouse axes
	LONG lX, lY, lZ;

	// Supported mouse buttons
	BYTE bButtonA, bButtonB, bButtonC, bButtonD, bButtonE, bButtonF, bButtonG, bButtonH;

}; // Size 20 bytes

//-----------------------------------------------------------------------------
// Purpose: DI object data format for GoldSource engine.
//-----------------------------------------------------------------------------
static DIOBJECTDATAFORMAT DInput_ObjectDataFormatNew[] =
{
	{ &GUID_XAxis,	FIELD_OFFSET(DI_DataFormatNew, lX),			DIDFT_AXIS | DIDFT_ANYINSTANCE,					NULL },
	{ &GUID_YAxis,	FIELD_OFFSET(DI_DataFormatNew, lY),			DIDFT_AXIS | DIDFT_ANYINSTANCE,					NULL },
	{ &GUID_ZAxis,	FIELD_OFFSET(DI_DataFormatNew, lZ),			0x80000000 | DIDFT_AXIS | DIDFT_ANYINSTANCE,	NULL },
	{ nullptr,		FIELD_OFFSET(DI_DataFormatNew, bButtonA),	DIDFT_BUTTON | DIDFT_ANYINSTANCE,				NULL },
	{ nullptr,		FIELD_OFFSET(DI_DataFormatNew, bButtonB),	DIDFT_BUTTON | DIDFT_ANYINSTANCE,				NULL },
	{ nullptr,		FIELD_OFFSET(DI_DataFormatNew, bButtonC),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE,	NULL },
	{ nullptr,		FIELD_OFFSET(DI_DataFormatNew, bButtonD),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE,	NULL },
	{ nullptr,		FIELD_OFFSET(DI_DataFormatNew, bButtonE),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE,	NULL },
	{ nullptr,		FIELD_OFFSET(DI_DataFormatNew, bButtonF),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE,	NULL },
	{ nullptr,		FIELD_OFFSET(DI_DataFormatNew, bButtonG),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE,	NULL },
	{ nullptr,		FIELD_OFFSET(DI_DataFormatNew, bButtonH),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE,	NULL },
};

//-----------------------------------------------------------------------------
// Purpose: Old data format used by the Quake Engine. Here it is used when
//			only 0x500 DI version is available.
//-----------------------------------------------------------------------------
static DIDATAFORMAT DInput_DataFormatOld =
{
	sizeof(DIDATAFORMAT),						// This structure
	sizeof(DIOBJECTDATAFORMAT),					// Size of object data format
	DIDF_RELAXIS,								// Absolute axis coordinates
	sizeof(DI_DataFormatOld),					// Device data size
	DI_NUM_OBJECTS(DInput_ObjectDataFormatOld),	// Number of objects
	DInput_ObjectDataFormatOld,					// And here they are
};

//-----------------------------------------------------------------------------
// Purpose: New data format used by our GoldSource engine. It is used when 0x700
//			DI version is available.
//-----------------------------------------------------------------------------
static DIDATAFORMAT DInput_DataFormatNew =
{
	sizeof(DIDATAFORMAT),						// This structure
	sizeof(DIOBJECTDATAFORMAT),					// Size of object data format
	DIDF_RELAXIS,								// Absolute axis coordinates
	sizeof(DI_DataFormatNew),					// Device data size
	DI_NUM_OBJECTS(DInput_ObjectDataFormatNew),	// Number of objects
	DInput_ObjectDataFormatNew,					// And here they are
};

//-----------------------------------------------------------------------------
// 
// Scan codes
// 
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: This maps raw key scan codes to our internal key codes.
//-----------------------------------------------------------------------------
static BYTE mpBScanNKey[256] =
{
//  0				1				2				3					4				5					6				7 
//  8				9				A				B					C				D					E				F 
	NULL,			K_ESCAPE,		'1',			'2',				'3',			'4',				'5',			'6', 	// 0 
	'7',			'8',			'9',			'NULL',				'-',			'=',				K_BACKSPACE,	K_TAB,
	'q',			'w',			'e',			'r',				't',			'y',				'u',			'i',	// 1  
	'o',			'p',			'[',			']',				K_ENTER,		K_CTRL,				'a',			's',
	'd',			'f',			'g',			'h',				'j',			'k',				'l',			';',	// 2  
	'\'',			'`',			K_SHIFT,		'\\',				'z',			'x',				'c',			'v',
	'b',			'n',			'm',			',',				'.',			'/',				K_SHIFT,		'*', 	// 3 
	K_ALT,			' ',			K_CAPSLOCK,		K_F1,				K_F2,			K_F3,				K_F4,			K_F5,
	K_F6,			K_F7,			K_F8,			K_F9,				K_F10,			K_PAUSE,			NULL,			K_KP_HOME,// 4 
	K_KP_UPARROW,	K_KP_PGUP,		K_KP_MINUS,		K_KP_LEFTARROW,		K_KP_5,			K_KP_RIGHTARROW,	K_KP_PLUS,		K_KP_END,
	K_KP_DOWNARROW,	K_KP_PGDN,		K_KP_INS,		K_KP_DEL,			NULL,			NULL,				NULL,			K_F11, 	// 5
	K_F12,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,	// 6
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,	// 7
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,	// 8
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,	// 9
	NULL,			NULL,			NULL,			NULL,				K_KP_ENTER,		K_CTRL,				NULL,			NULL,
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,	// A
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,
	NULL,			NULL,			NULL,			NULL,				NULL,			K_KP_SLASH,			NULL,			NULL,	// B
	K_ALT,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			K_HOME,	// C
	K_UPARROW,		K_PGUP,			NULL,			K_LEFTARROW,		NULL,			K_RIGHTARROW,		NULL,			K_END,
	K_DOWNARROW,	K_PGDN,			K_INS,			K_DEL,				NULL,			NULL,				NULL,			NULL,	// D
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,	// E
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,	// F
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,
};

//-----------------------------------------------------------------------------
// Purpose: Scan code used inside DInput_SendKeyboardEvent()
//-----------------------------------------------------------------------------
static BYTE mpBScanNKey1[256] =
{
//  0				1				2				3					4				5					6				7 
//  8				9				A				B					C				D					E				F 
	NULL,			VK_ESCAPE,		'1',			'2',				'3',			'4',				'5',			'6', 	// 0 
	'7',			'8',			'9',			'NULL',				VK_OEM_PLUS,	VK_OEM_MINUS,		VK_BACK,		VK_TAB,
	'Q',			'W',			'E',			'R',				'T',			'Y',				'U',			'I',	// 1
	'O',			'P',			VK_OEM_4,		VK_OEM_6,			VK_RETURN,		VK_CONTROL,			'A',			'S',
	'D',			'F',			'G',			'H',				'J',			'K',				'L',			VK_OEM_1,// 2
	VK_IME_ON,		VK_OEM_3,		VK_SHIFT,		VK_OEM_5,			'Z',			'X',				'C',			'V',
	'B',			'N',			'M',			VK_OEM_COMMA,		VK_OEM_PERIOD,	VK_OEM_2,			VK_SHIFT,		'j',	// 3
	VK_MENU,		' ',			VK_CAPITAL,		'p',				'q',			'r',				's',			't',
	'u',			'v',			'w',			'x',				'y',			VK_PAUSE,			NULL,			'$',	// 4
	'&',			'!',			'm',			'%',				'e',			'\'',				'k',			'#',
	'(',			'"',			'`',			'n',				NULL,			NULL,				NULL,			'z',	// 5
	'{',			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,	// 6
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,	// 7
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,	// 8
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,	// 9
	NULL,			NULL,			NULL,			NULL,				VK_RETURN,		VK_CONTROL,			NULL,			NULL,
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,	// A
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,
	NULL,			NULL,			NULL,			NULL,				NULL,			'o',				NULL,			NULL,	// B
	VK_MENU,		NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			'$',	// C
	'&',			'!',			NULL,			'%',				NULL,			'\'',				NULL,			'#',
	'(',			'"',			'-',			'.',				NULL,			NULL,				NULL,			NULL,	// D
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,	// E
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,	// F
	NULL,			NULL,			NULL,			NULL,				NULL,			NULL,				NULL,			NULL,
};

//-----------------------------------------------------------------------------
// 
// Direct Input
// 
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Initializes DInput code with either version 0x700 or 0x500, depending
//			on whenether the newer version failed to intitialize. Then setup
//			individual devices and handle mouse startup code.
//-----------------------------------------------------------------------------
qboolean DInput_Initialize()
{
	HMODULE hInstDI;
	HRESULT hr;

	DInput_Initialized = true;

	// Get current module handle
	hInstDI = GetModuleHandleA(NULL);

	// Newer version (0x700)
	DInput_Version = DIRECTINPUT_VERSION;

	// Register with DirectInput and get an IDirectInput to play with
	hr = DirectInputCreateA(hInstDI, DIRECTINPUT_VERSION, &g_pdi, nullptr);

	// If we fail to initialize newer version, try the older one
	if (FAILED(hr))
	{
		// Older version (0x500)
		DInput_Version = DIRECTINPUT_VERSION_OLD;

		// Try again with older version
		hr = DirectInputCreateA(hInstDI, DIRECTINPUT_VERSION_OLD, &g_pdi, nullptr);

		// Failed with older version, exit
		if (FAILED(hr))
			return false;
	}

	// Try to create mouse device
	if (!DInput_CreateMouseDevice())
		return false;

	// Try to create keyboad device
	if (!DInput_CreateKeyboardDevice())
		return false;

	// Check for startup parameters as well as system parameters
	// for this mouse device.
	DInput_StartupMouse();

	// Mark devices as enabled
	DInput_MouseEnabled = true;
	DInput_KeyboardEnabled = true;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Shutdowns the DInput code, release devices, reset data
//-----------------------------------------------------------------------------
void DInput_Shutdown()
{
	// Release mouse device
	DInput_DeactivateMouse();

	if (DInput_Initialized)
	{
		// Release interface device
		if (DInput_KeyboardEnabled)
		{
			IDirectInput_Release(g_pdi);
			g_pdi = nullptr;
		}

		// Release keyboard device
		if (DInput_KeyboardEnabled)
		{
			if (g_pKeyboard)
			{
				IDirectInputDevice_Unacquire(g_pKeyboard);
				IDirectInputDevice_Release(g_pKeyboard);
				g_pKeyboard = nullptr;
			}
		}
	}

	// Reset data
	DInput_Initialized = false;
	DInput_KeyboardEnabled = false;
	DInput_MouseEnabled = false;
}

//-----------------------------------------------------------------------------
// Purpose: Setups parameters of mouse device such as threshhold for X and Y 
//			position and mouse sped/acceleration. Also takes care of launch
//			parameters for mouse settings and gets the keyboard delay and repeat
//			rate.
//-----------------------------------------------------------------------------
void DInput_StartupMouse()
{
	BOOL	sysparam;

	INT		nT;

	sysparam = SystemParametersInfoA(SPI_GETMOUSE, NULL, originalmouseparms, NULL);

	// Get the original mouse parameters. If failed to, set the hardcoded default ones.
	if (!sysparam)
	{
		originalmouseparms[DI_MSPM_THRESHOLD_X] = DI_MOUSE_DEFAULT_THRESHOLD_X;
		originalmouseparms[DI_MSPM_THRESHOLD_Y] = DI_MOUSE_DEFAULT_THRESHOLD_X;
		originalmouseparms[DI_MSPM_ACCELERATE] = DI_MOUSE_DEFAULT_ACCELERATION;
	}

	// Check if force mouse speed is disabled
	if (COM_CheckParm("-noforcemspd"))
	{
		newmouseparms[DI_MSPM_ACCELERATE] = originalmouseparms[DI_MSPM_ACCELERATE];
	}

	// Check if force mouse threshold is disabled
	if (COM_CheckParm("-noforcemaccel"))
	{
		newmouseparms[DI_MSPM_THRESHOLD_X] = originalmouseparms[DI_MSPM_THRESHOLD_X];
		newmouseparms[DI_MSPM_THRESHOLD_Y] = originalmouseparms[DI_MSPM_THRESHOLD_Y];
	}

	// Check if all of the mouse parameters are disabled
	if (COM_CheckParm("-noforcemparms"))
	{
		newmouseparms[DI_MSPM_THRESHOLD_X] = originalmouseparms[DI_MSPM_THRESHOLD_X];
		newmouseparms[DI_MSPM_THRESHOLD_X] = originalmouseparms[DI_MSPM_THRESHOLD_Y];
		newmouseparms[DI_MSPM_ACCELERATE] = originalmouseparms[DI_MSPM_ACCELERATE];
	}

	// Try to get mouse speed
	sysparam = SystemParametersInfoA(SPI_GETMOUSESPEED, NULL, &newmouseparms[DI_MSPM_ACCELERATE], NULL);

	// If we've failed of the speed we got is bad, set it to default value
	if (!sysparam)
	{
		newmouseparms[DI_MSPM_ACCELERATE] = DI_MOUSE_ACCELERATION_DEFAULT;
	}

	// The value we've got is bad, cap it to default
	if (newmouseparms[DI_MSPM_ACCELERATE] < DI_MOUSE_ACCELERATION_MIN || // Check min value
		newmouseparms[DI_MSPM_ACCELERATE] > DI_MOUSE_ACCELERATION_MAX)	 // Check max value
	{
		newmouseparms[DI_MSPM_ACCELERATE] = DI_MOUSE_ACCELERATION_DEFAULT;
	}

	// Get the system key repeat delay. The docs specify that a value of 0 equals a 
	// 250ms delay and a value of 3 equals a 1 sec delay.
	sysparam = SystemParametersInfoA(SPI_GETKEYBOARDDELAY, NULL, &nT, NULL);

	if (sysparam)
	{
		switch (nT)
		{
			case 0: // Fast delay
			{
				keyboard_delay = DI_KEYBOARD_DELAY_ULTRA_FAST;
				break;
			}
			case 1: // Slower delay
			{
				keyboard_delay = DI_KEYBOARD_DELAY_FASTER;
				break;
			}
			case 2: // Slow delay
			{
				keyboard_delay = DI_KEYBOARD_DELAY_FAST;
				break;
			}
			case 3: // Very slow delay
			default:
			{
				keyboard_delay = DI_KEYBOARD_DELAY_SLOW;
				break;
			}
		}
	}
	else
	{
		keyboard_delay = DI_KEYBOARD_DELAY_SLOW;
	}

	// Get the system key repeat rate. The docs specify that a value of 0 equals a 
	// rate of 2.5 / sec (400 ms delay), and a value of 31 equals a rate of 30/sec
	// (33 ms delay).
	if (SystemParametersInfoA(SPI_GETKEYBOARDSPEED, NULL, &nT, NULL))
	{
		// Clamp the value between [0, 32)
		nT = std::clamp(nT, 0, DI_KEYBOARD_SPEED_MAXVAL - 1);

		// The bigger the nT, the smaller keyboard_speed
		// For nT == 0  = 400 ms
		// For nT == 10 = 282 ms
		// For nT == 20 = 164 ms
		// For nT == 31 = 0	  ms
		keyboard_speed = DI_KEYBOARD_SPEED_LIMIT_MS + (nT * ((DI_KEYBOARD_SPEED_MAXVAL + 1) - DI_KEYBOARD_SPEED_LIMIT_MS) / (DI_KEYBOARD_SPEED_MAXVAL - 1));
	}
	else
	{
		keyboard_speed = DI_KEYBOARD_SPEED_LIMIT_MS;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the DInput mouse device
//-----------------------------------------------------------------------------
qboolean DInput_CreateMouseDevice()
{
	DIPROPDWORD		dipdw;
	HRESULT			hr;

	// SDL window handle
	SDL_SysWMinfo	wmInfo;

	// Create the mouse
	hr = IDirectInput_CreateDevice(g_pdi, GUID_SysMouse, &g_pMouse, NULL);
	if (FAILED(hr))
		return false;

	// Set the data format and cooperative level for specific data set
	if (DInput_Version == DIRECTINPUT_VERSION)
		hr = IDirectInputDevice_SetDataFormat(g_pMouse, &DInput_DataFormatNew);
	else
		hr = IDirectInputDevice_SetDataFormat(g_pMouse, &DInput_DataFormatOld);

	if (FAILED(hr))
		return false;

	// Get the window handle from SDL
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(pmainwindow, &wmInfo);

	// Try to set cooperative level
	hr = IDirectInputDevice_SetCooperativeLevel(g_pMouse, wmInfo.info.win.window, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
	if (FAILED(hr))
		return false;

	// Use buffered input
	dipdw.diph.dwSize = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj = 0;
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.dwData = 32;

	// Set device property (DIPROPDWORD property information)
	hr = IDirectInputDevice_SetProperty(g_pMouse, DIPROP_BUFFERSIZE, &dipdw.diph);
	if (FAILED(hr))
		return false;

	// Start out by acquiring the mouse
	IDirectInputDevice_Acquire(g_pMouse);

	// Center out the mouse at startup
	mouse_x = window_center_x;
	mstate_di = NULL; // Set to zero for now
	mouse_x = window_center_y;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Unacquires and releases mouse when enabled and initialized.
//-----------------------------------------------------------------------------
void DInput_DeactivateMouse()
{
	// The DInput code haven't been initialized yet
	if (!DInput_Initialized)
		return;

	// keyboard and mouse weren't enabled
	if (!DInput_KeyboardEnabled || !DInput_MouseEnabled)
		return;

	// Null mouse device
	if (!g_pMouse)
		return;

	// Destroy that device
	IDirectInputDevice_Unacquire(g_pMouse);
	IDirectInputDevice_Release(g_pMouse);
	g_pMouse = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the DInput keyboard device
//-----------------------------------------------------------------------------
qboolean DInput_CreateKeyboardDevice()
{
	DIPROPDWORD		dipdw;
	HRESULT			hr;

	// SDL window handle
	SDL_SysWMinfo	wmInfo;

	// Create the keyboard
	hr = IDirectInput_CreateDevice(g_pdi, GUID_SysKeyboard, &g_pKeyboard, NULL);
	if (FAILED(hr))
		return false;

	// Set the data format and cooperative level
	// For keyboard we use the standard data model c_dfDIKeyboard
	hr = IDirectInputDevice_SetDataFormat(g_pKeyboard, &c_dfDIKeyboard);
	if (FAILED(hr))
		return false;

	// Get the window handle from SDL
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(pmainwindow, &wmInfo);

	// Try to set cooperative level
	hr = IDirectInputDevice_SetCooperativeLevel(g_pKeyboard, wmInfo.info.win.window, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
	if (FAILED(hr))
		return false;

	// Use buffered input
	dipdw.diph.dwSize = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj = 0;
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.dwData = 32;

	// Set device property (DIPROPDWORD property information)
	hr = IDirectInputDevice_SetProperty(g_pKeyboard, DIPROP_BUFFERSIZE, &dipdw.diph);
	if (FAILED(hr))
		return false;

	// Start out by acquiring the keyboard
	IDirectInputDevice_Acquire(g_pKeyboard);

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Dispatch mouse and keyboard events and inform client dll about the
//			mouse movement.
//-----------------------------------------------------------------------------
void DInput_HandleEvents()
{
	// Only dispatch if the DI code has been initialized
	if (!DInput_Initialized)
		return;

	DInput_ReadMouseEvents();

	// Inform client dll about mouse events
	ClientDLL_MouseEvent(mstate_di);

	DInput_ReadKeyboardEvents();
}

//-----------------------------------------------------------------------------
// Purpose: Process all mouse messages up to the current point and
//			update mouse_x, mouse_y, and mouse state. Note that we don't
//			send any mouse messages (call SendMouseEvents to do that).  This is
//			because we sometimes need to update the mouse position but don't
//			want to be firing the buttons right away.
//-----------------------------------------------------------------------------
void DInput_ReadMouseEvents()
{
	HRESULT				hr;
	DIDEVICEOBJECTDATA	od;
	DWORD				dwElements;

	// Loop through all the buffered mouse events
	while (true)
	{
		dwElements = 1;

		// Try to get information about this mouse device
		hr = IDirectInputDevice_GetDeviceData(g_pMouse, sizeof(DIDEVICEOBJECTDATA), &od, &dwElements, NULL);

		// Failed, we have to try again
		if (FAILED(hr))
		{
			// Acquire the mouse and try to get device data that way
			IDirectInputDevice_Acquire(g_pMouse);
			hr = IDirectInputDevice_GetDeviceData(g_pMouse, sizeof(DIDEVICEOBJECTDATA), &od, &dwElements, NULL);
		}

		// If we've failed once more or the amount of elements we've get
		// were 0, then we cannot continue
		if (FAILED(hr) || dwElements == NULL)
			return;

		// Look at the element to see what happened
		switch (od.dwOfs)
		{
			// Horizontal mouse movement
			case DIMOFS_X:
			{
				mouse_x += DInput_AccelerateMovement(od.dwData);

				break;
			}
			// Vertical mouse movement
			case DIMOFS_Y:
			{
				mouse_y += DInput_AccelerateMovement(od.dwData);

				break;
			}
			// Mouse wheel event
			case DIMOFS_Z:
			{
				// Wheeled upwards
				if (static_cast<int>(od.dwData) > 0)
				{
					Key_Event(K_MWHEELUP, TRUE);
					Key_Event(K_MWHEELUP, FALSE);
				}
				// Wheeled downwards
				else
				{
					Key_Event(K_MWHEELDOWN, TRUE);
					Key_Event(K_MWHEELDOWN, FALSE);
				}

				break;
			}
			// Left click event
			case DIMOFS_BUTTON0:
			{
				if (DI_BUTTON_PRESSED(od.dwData))
					mstate_di |= DI_MSTATE_BUTTON0_BIT;
				else
					mstate_di &= ~DI_MSTATE_BUTTON0_BIT;

				break;
			}
			// Right click event
			case DIMOFS_BUTTON1:
			{
				if (DI_BUTTON_PRESSED(od.dwData))
					mstate_di |= DI_MSTATE_BUTTON1_BIT;
				else
					mstate_di &= ~DI_MSTATE_BUTTON1_BIT;

				break;
			}
			// Middle button event
			case DIMOFS_BUTTON2:
			{
				if (DI_BUTTON_PRESSED(od.dwData))
					mstate_di |= DI_MSTATE_BUTTON2_BIT;
				else
					mstate_di &= ~DI_MSTATE_BUTTON2_BIT;

				break;
			}
			// Mouse4 event
			case DIMOFS_BUTTON3:
			{
				if (DI_BUTTON_PRESSED(od.dwData))
					mstate_di |= DI_MSTATE_BUTTON3_BIT;
				else
					mstate_di &= ~DI_MSTATE_BUTTON3_BIT;

				break;
			}
			// Mouse5 event
			case DIMOFS_BUTTON4:
			{
				if (DI_BUTTON_PRESSED(od.dwData))
					mstate_di |= DI_MSTATE_BUTTON4_BIT;
				else
					mstate_di &= ~DI_MSTATE_BUTTON4_BIT;

				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Scans the keyboard and emits key up / down messages as necessary
//-----------------------------------------------------------------------------
void DInput_ReadKeyboardEvents()
{
	HRESULT				hr;
	DIDEVICEOBJECTDATA	od;
	DWORD				dwElements;

	int					nTicksCur;
	
	// Current timestamp in ms
	nTicksCur = ::GetTickCount();

	// Loop through all the bufferedkeyboard events
	while (true)
	{
		dwElements = 1;

		// Try to get information about this keyboard device
		hr = IDirectInputDevice_GetDeviceData(g_pKeyboard, sizeof(DIDEVICEOBJECTDATA), &od, &dwElements, NULL);

		// Failed, we have to try again
		if (FAILED(hr))
		{
			// Acquire the keyboard and try to get device data that way
			IDirectInputDevice_Acquire(g_pKeyboard);
			hr = IDirectInputDevice_GetDeviceData(g_pKeyboard, sizeof(DIDEVICEOBJECTDATA), &od, &dwElements, NULL);
		}

		// If we've failed once more or the amount of elements we've get
		// were 0, then we cannot continue
		if (FAILED(hr) || dwElements == NULL)
			break;

		// If it's a key we care about, fire a key event
		if (mpBScanNKey[od.dwOfs] != NULL)
		{
			// Dispatch this keyboard event for us
			DInput_SendKeyboardEvent(od.dwOfs, mpBScanNKey[od.dwOfs], DI_BUTTON_PRESSED(od.dwData));

			if (DI_BUTTON_PRESSED(od.dwData))
			{
				key_last = od.dwOfs;
				key_ticks_pressed = nTicksCur;
				key_ticks_repeat = nTicksCur;
			}
			else if (od.dwOfs == key_last)
			{
				key_last = 0;
			}
		}
	}

	// Auto-repeat the last key pressed
	if (key_last != NULL)
	{
		// Issue the first repeat message?
		if (key_ticks_repeat == key_ticks_pressed)
		{
			if (nTicksCur >= key_ticks_pressed + keyboard_delay)
			{
				DInput_SendKeyboardEvent(key_last, mpBScanNKey[key_last], true);
				key_ticks_repeat = nTicksCur;
			}
		}
		// Check if we need to repeat again
		else
		{
			if (nTicksCur >= key_ticks_repeat + keyboard_speed)
			{
				DInput_SendKeyboardEvent(key_last, mpBScanNKey[key_last], true);
				key_ticks_repeat = nTicksCur;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sends the keyboard event to the engine dispatcher.
//-----------------------------------------------------------------------------
void DInput_SendKeyboardEvent(DWORD dwOfs, unsigned int nChar, bool fDown)
{
	// Trap key event
	eng->TrapMouse_Event(mpBScanNKey1[dwOfs], fDown != false);

	// Inform engine
	Key_Event(nChar, fDown);
}

//-----------------------------------------------------------------------------
// Purpose: Returns cursor position
//-----------------------------------------------------------------------------
void DInput_GetCursorPos(Point_t* lpPoint)
{
	RecEngGetMousePos(lpPoint);

	// If DInput isn't initialized, just ask windows
	if (!DInput_Initialized)
	{
		::GetCursorPos(reinterpret_cast<POINT*>(lpPoint));
		return;
	}

	// Receive mouse events
	DInput_ReadMouseEvents();

	// Receive position
	lpPoint->x = mouse_x;
	lpPoint->y = mouse_y;
}

//-----------------------------------------------------------------------------
// Purpose: Sets new cursor position at [X, Y]
//-----------------------------------------------------------------------------
void DInput_SetCursorPos(int X, int Y)
{
	RecEngSetMousePos(X, Y);

	// If DInput isn't initialized, just ask windows
	if (!DInput_Initialized)
	{
		::SetCursorPos(X, Y);
		return;
	}

	// Receive mouse events
	DInput_ReadMouseEvents();

	// Update position
	mouse_x = X;
	mouse_y = Y;
}

//-----------------------------------------------------------------------------
// Purpose: Applies acceleration to either given X or Y value.
//-----------------------------------------------------------------------------
int DInput_AccelerateMovement(DWORD dwData)
{
	DWORD	dwResult;
	INT		iInverted;

	dwResult = dwData;
	iInverted = dwData;

	if (!(int)dwData)
		iInverted = -dwData;

	if (newmouseparms[DI_MSPM_ACCELERATE] > 0 && iInverted > newmouseparms[DI_MSPM_THRESHOLD_X])
		dwResult = 2 * dwData;

	if (newmouseparms[DI_MSPM_ACCELERATE] > 1 && iInverted > newmouseparms[DI_MSPM_THRESHOLD_Y])
		dwResult *= 2;

#if 0
	// 1717986919 (66666667h) as float:
	// 
	// 0 | 110 0110 | 0110 0110 0110 0110 0110 0111
	// ^ sign     ^                               ^
	//            | exponent                      |
	//                                       base |
	//
	// sign:     0 (positive)
	// exponent: 110 0110 (102)
	// base:     0110 0110 0110 0110 0110 0111 (6,710,887)

	{
		constexpr int64_t a = 1717986919i64;
		constexpr float b = static_cast<float>(a);
	}
	{
		constexpr int64_t a = 0x66666666;
		constexpr float b = static_cast<float>(a);
	}

	int32_t a = dwResult * newmouseparms[DI_MSPM_ACCELERATE];
	uint64_t b = 1717986919i64 * a;
	int32_t c = b >> 32;
	int32_t d = c >> 2;
	int32_t e = (d >> 31) + d;

	return e;
#else
	return dwResult * newmouseparms[DI_MSPM_ACCELERATE];
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Depending on passed fEnable, this funciton enables/disabled mouse
//			movement.
//-----------------------------------------------------------------------------
void SetMouseEnable(qboolean fEnable)
{
	RecEngSetMouseEnable(fEnable);

	// Already enabled
	if (DInput_MouseEnabled == fEnable)
		return;

	// DI code wasn't initialized
	if (!DInput_Initialized)
		return;

	// Keyboard device must be enabled
	if (!DInput_KeyboardEnabled)
		return;

	if (fEnable)
	{
		// Create and acquire new mouse device
		if (!DInput_CreateMouseDevice())
		{
			DInput_KeyboardEnabled = false;
			DInput_MouseEnabled = fEnable;

			return;
		}
	}
	else
	{
		// Deactive mouse movement
		DInput_DeactivateMouse();
	}

	// Update changes
	DInput_MouseEnabled = fEnable;
}