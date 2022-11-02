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
// Purpose: sage of the macro-based offsetof pattern in constant expressions is 
//			non-standard; use offsetof defined in the C++ standard library instead
//-----------------------------------------------------------------------------
#pragma warning(disable : 4644)

//-----------------------------------------------------------------------------
// Purpose: unary minus operator applied to unsigned type, result still unsigned
//-----------------------------------------------------------------------------
#pragma warning(disable : 4146)

#ifdef WIN32

//-----------------------------------------------------------------------------
// 
// Defines
// 
//-----------------------------------------------------------------------------

// Returns TRUE if the button was pressed. Whenether the button was
// pressed or not indecates the most-significant bit inside the first
// byte of dwData. (X000 0000)
#define DI_BUTTON_PRESSED(dwData) ((dwData & 0x80) == 0 ? FALSE : TRUE)

// The amount of objecs inside data object
#define DI_NUM_OBJECTS(object) (sizeof(object) / sizeof(object[0]))

//-----------------------------------------------------------------------------
// 
// Globals
// 
//-----------------------------------------------------------------------------

// Holds TRUE if DI code is initialized
qboolean	DInput_Initialized;

// Holds version of the DI interface
DWORD		DInput_Version;

// True if mouse or keyboard are enabled
qboolean	DInput_MouseEnabled, DInput_KeyboardEnabled;

int			newmouseparms[DI_MSPM_MAX] = { 0, 0, 1 };
int			newmouseparams_forcespeed;

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
	{ NULL,		FIELD_OFFSET(DI_DataFormatOld, bButtonA),	DIDFT_BUTTON | DIDFT_ANYINSTANCE,				NULL },
	{ NULL,		FIELD_OFFSET(DI_DataFormatOld, bButtonB),	DIDFT_BUTTON | DIDFT_ANYINSTANCE,				NULL },
	{ NULL,		FIELD_OFFSET(DI_DataFormatOld, bButtonC),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE,	NULL },
	{ NULL,		FIELD_OFFSET(DI_DataFormatOld, bButtonD),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE,	NULL },
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
	{ NULL,		FIELD_OFFSET(DI_DataFormatNew, bButtonA),	DIDFT_BUTTON | DIDFT_ANYINSTANCE,				NULL },
	{ NULL,		FIELD_OFFSET(DI_DataFormatNew, bButtonB),	DIDFT_BUTTON | DIDFT_ANYINSTANCE,				NULL },
	{ NULL,		FIELD_OFFSET(DI_DataFormatNew, bButtonC),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE,	NULL },
	{ NULL,		FIELD_OFFSET(DI_DataFormatNew, bButtonD),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE,	NULL },
	{ NULL,		FIELD_OFFSET(DI_DataFormatNew, bButtonE),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE,	NULL },
	{ NULL,		FIELD_OFFSET(DI_DataFormatNew, bButtonF),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE,	NULL },
	{ NULL,		FIELD_OFFSET(DI_DataFormatNew, bButtonG),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE,	NULL },
	{ NULL,		FIELD_OFFSET(DI_DataFormatNew, bButtonH),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE,	NULL },
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
//  0			1			2			3			4			5			6			7 
//  8			9			A			B			C			D			E			F 
	0,			27,			'1',		'2',		'3',		'4',		'5',		'6', 	// 0 
	'7',		'8',		'9',		'0',		'-',		'=',		K_BACKSPACE,9,
	'q',		'w',		'e',		'r',		't',		'y',		'u',		'i',	// 1  
	'o',		'p',		'[',		']',		13 ,		K_CTRL,		'a',		's',
	'd',		'f',		'g',		'h',		'j',		'k',		'l',		';',	// 2  
	'\'',		'`',		K_SHIFT,	'\\',		'z',		'x',		'c',		'v',
	'b',		'n',		'm',		',',		'.',		'/',		K_SHIFT,	'*', 	// 3 
	K_ALT,		' ',		K_CAPSLOCK,	K_F1,		K_F2,		K_F3,		K_F4,		K_F5,
	K_F6,		K_F7,		K_F8,		K_F9,		K_F10,		K_PAUSE,	0,			K_KP_HOME, // 4 
	K_KP_UPARROW,K_KP_PGUP,	K_KP_MINUS,	K_KP_LEFTARROW,K_KP_5,	K_KP_RIGHTARROW,K_KP_PLUS,K_KP_END,
	K_KP_DOWNARROW,K_KP_PGDN,K_KP_INS,	K_KP_DEL,	0,			0,			0,			K_F11, 	// 5
	K_F12,		0,			0,			0,			0,			0,			0,			0,
	0,			0,			0,			0,			0,			0,			0,			0,		// 6
	0,			0,			0,			0,			0,			0,			0,			0,
	0,			0,			0,			0,			0,			0,			0,			0,		// 7
	0,			0,			0,			0,			0,			0,			0,			0,
	0,			0,			0,			0,			0,			0,			0,			0,		// 8
	0,			0,			0,			0,			0,			0,			0,			0,
	0,			0,			0,			0,			0,			0,			0,			0,		// 9
	0,			0,			0,			0,			K_KP_ENTER,	K_CTRL,		0,			0,
	0,			0,			0,			0,			0,			0,			0,			0,		// A
	0,			0,			0,			0,			0,			0,			0,			0,
	0,			0,			0,			0,			0,			K_KP_SLASH,	0,			0,		// B
	K_ALT,		0,			0,			0,			0,			0,			0,			0,
	0,			0,			0,			0,			0,			0,			0,			K_HOME,	// C
	K_UPARROW,	K_PGUP,		0,			K_LEFTARROW,0,			K_RIGHTARROW,0,			K_END,
	K_DOWNARROW,K_PGDN,		K_INS,		K_DEL,		0,			0,			0,			0,		// D
	0,			0,			0,			0,			0,			0,			0,			0,
	0,			0,			0,			0,			0,			0,			0,			0,		// E
	0,			0,			0,			0,			0,			0,			0,			0,
	0,			0,			0,			0,			0,			0,			0,			0,		// F
	0,			0,			0,			0,			0,			0,			0,			0,
};

//-----------------------------------------------------------------------------
// Purpose: Scan code used inside DInput_SendKeyboardEvent()
//-----------------------------------------------------------------------------
static BYTE mpBVirtualScanNKey[256] =
{
//  0			1			2			3			4			5			6			7 
//  8			9			A			B			C			D			E			F 
	0,			27,			'1',		'2',		'3',		'4',		'5',		'6', 	// 0 
	'7',		'8',		'9',		'0',		VK_OEM_MINUS,VK_OEM_PLUS,VK_BACK,	VK_TAB,
	'Q',		'W',		'E',		'R',		'T',		'Y',		'U',		'I',	// 1  
	'O',		'P',		VK_OEM_4,	VK_OEM_6,	VK_RETURN,	VK_CONTROL,	'A',		'S',
	'D',		'F',		'G',		'H',		'J',		'K',		'L',		VK_OEM_1,// 2  
	VK_OEM_7,	VK_OEM_3,	VK_SHIFT,	VK_OEM_5,	'Z',		'X',		'C',		'V',
	'B',		'N',		'M',		VK_OEM_COMMA,VK_OEM_PERIOD,	VK_OEM_2,VK_SHIFT,	'j',	// 3
	VK_MENU,	' ',		VK_CAPITAL,	'p',		'q',		'r',		's',		't',
	'u',		'v',		'w',		'x',		'y',		VK_PAUSE,	0,			'$',	// 4
	'&',		'!',		'm',		'%',		'e',		'\'',		'k',		'#',
	'(',		'"',		'`',		'n',		0,			0,			0,			'z',	// 5
	'{',		0,			0,			0,			0,			0,			0,			0,
	0,			0,			0,			0,			0,			0,			0,			0,		// 6
	0,			0,			0,			0,			0,			0,			0,			0,
	0,			0,			0,			0,			0,			0,			0,			0,		// 7
	0,			0,			0,			0,			0,			0,			0,			0,
	0,			0,			0,			0,			0,			0,			0,			0,		// 8
	0,			0,			0,			0,			0,			0,			0,			0,
	0,			0,			0,			0,			0,			0,			0,			0,		// 9
	0,			0,			0,			0,			VK_RETURN,	VK_CONTROL,	0,			0,
	0,			0,			0,			0,			0,			0,			0,			0,		// A
	0,			0,			0,			0,			0,			0,			0,			0,
	0,			0,			0,			0,			0,			'o',		0,			0,		// B
	VK_MENU,	0,			0,			0,			0,			0,			0,			0,
	0,			0,			0,			0,			0,			0,			0,			'$',	// C
	'&',		'!',		0,			'%',		0,			'\'',		0,			'#',
	'(',		'"',		'-',		'.',		0,			0,			0,			0,		// D
	0,			0,			0,			0,			0,			0,			0,			0,
	0,			0,			0,			0,			0,			0,			0,			0,		// E
	0,			0,			0,			0,			0,			0,			0,			0,
	0,			0,			0,			0,			0,			0,			0,			0,		// F
	0,			0,			0,			0,			0,			0,			0,			0,
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

	// Get current module handle
	hInstDI = ::GetModuleHandleA(NULL);

	// Newer version (0x700)
	DInput_Version = DIRECTINPUT_VERSION;

	// Register with DirectInput and get an IDirectInput to play with
	hr = ::DirectInputCreateA(hInstDI, DIRECTINPUT_VERSION, &g_pdi, NULL);

	// If we fail to initialize newer version, try the older one
	if (FAILED(hr))
	{
		// Older version (0x500)
		DInput_Version = DIRECTINPUT_VERSION_OLD;

		// Try again with older version
		hr = ::DirectInputCreateA(hInstDI, DIRECTINPUT_VERSION_OLD, &g_pdi, NULL);

		// Failed with older version, exit
		if (FAILED(hr))
			return FALSE;
	}

	// Try to create mouse device
	if (!DInput_CreateMouseDevice())
		return FALSE;

	// Try to create keyboad device
	if (!DInput_CreateKeyboardDevice())
		return FALSE;

	// Check for startup parameters as well as system parameters
	// for this mouse device.
	DInput_StartupMouse();

	// Mark devices as enabled
	DInput_MouseEnabled = TRUE;
	DInput_KeyboardEnabled = TRUE;
	DInput_Initialized = TRUE;

	return TRUE;
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
			g_pdi = NULL;
		}

		// Release keyboard device
		if (DInput_KeyboardEnabled)
		{
			if (g_pKeyboard)
			{
				IDirectInputDevice_Unacquire(g_pKeyboard);
				IDirectInputDevice_Release(g_pKeyboard);
				g_pKeyboard = NULL;
			}
		}
	}

	// Reset data
	DInput_Initialized = FALSE;
	DInput_KeyboardEnabled = FALSE;
	DInput_MouseEnabled = FALSE;
}

//-----------------------------------------------------------------------------
// Purpose: Setup parameters of mouse device such as threshhold for X and Y 
//			position and mouse sped/acceleration. Also takes care of launch
//			parameters for mouse settings and gets the keyboard delay and repeat
//			rate.
//-----------------------------------------------------------------------------
void DInput_StartupMouse()
{
	int nT, originalmouseparms[DI_MSPM_MAX];

	// Get the original mouse parameters. If failed to, set the hardcoded default ones.
	if (!::SystemParametersInfoA(SPI_GETMOUSE, NULL, originalmouseparms, NULL))
	{
		originalmouseparms[DI_MSPM_THRESHOLD_X] = DI_MOUSE_DEFAULT_THRESHOLD_X;
		originalmouseparms[DI_MSPM_THRESHOLD_Y] = DI_MOUSE_DEFAULT_THRESHOLD_Y;
		originalmouseparms[DI_MSPM_ACCELERATE] = DI_MOUSE_DEFAULT_ACCELERATION;
	}

	// Check if force mouse speed is disabled
	if (COM_CheckParm("-noforcemmparms") || COM_CheckParm("-noforcemspd"))
		newmouseparams_forcespeed = originalmouseparms[DI_MSPM_ACCELERATE];
	else
		newmouseparams_forcespeed = TRUE;

	// Check if force mouse threshold is disabled
	if (COM_CheckParm("-noforcemparms") || COM_CheckParm("-noforcemaccel"))
	{
		newmouseparms[DI_MSPM_THRESHOLD_X] = originalmouseparms[DI_MSPM_THRESHOLD_X];
		newmouseparms[DI_MSPM_THRESHOLD_Y] = originalmouseparms[DI_MSPM_THRESHOLD_Y];
	}
	else
	{
		newmouseparms[DI_MSPM_THRESHOLD_X] = 0;
		newmouseparms[DI_MSPM_THRESHOLD_Y] = 0;
	}

	// If we failed of the speed we got is bad, set it to default value
	if (!SystemParametersInfoA(SPI_GETMOUSESPEED, NULL, &newmouseparms[DI_MSPM_ACCELERATE], NULL) ||
		// The value wec got is bad, cap it to default
		newmouseparms[DI_MSPM_ACCELERATE] < DI_MOUSE_ACCELERATION_MIN || // Check min value
		newmouseparms[DI_MSPM_ACCELERATE] > DI_MOUSE_ACCELERATION_MAX)
	{
		newmouseparms[DI_MSPM_ACCELERATE] = DI_MOUSE_ACCELERATION_DEFAULT;
	}

	// Get the system key repeat delay. The docs specify that a value of 0 equals a 
	// 250ms delay and a value of 3 equals a 1 sec delay.
	if (::SystemParametersInfoA(SPI_GETKEYBOARDDELAY, NULL, &nT, NULL))
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
		return FALSE;

	// Set the data format and cooperative level for specific data set
	if (DInput_Version == DIRECTINPUT_VERSION)
		hr = IDirectInputDevice_SetDataFormat(g_pMouse, &DInput_DataFormatNew);
	else
		hr = IDirectInputDevice_SetDataFormat(g_pMouse, &DInput_DataFormatOld);

	if (FAILED(hr))
		return FALSE;

	// Get the window handle from SDL
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo((SDL_Window*)pmainwindow, &wmInfo);

	// Try to set cooperative level
	hr = IDirectInputDevice_SetCooperativeLevel(g_pMouse, wmInfo.info.win.window, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
	if (FAILED(hr))
		return FALSE;

	// Use buffered input
	dipdw.diph.dwSize = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj = 0;
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.dwData = 32;

	// Set device property (DIPROPDWORD property information)
	hr = IDirectInputDevice_SetProperty(g_pMouse, DIPROP_BUFFERSIZE, &dipdw.diph);
	if (FAILED(hr))
		return FALSE;

	// Start out by acquiring the mouse
	IDirectInputDevice_Acquire(g_pMouse);

	// Center out the mouse at startup
	mouse_x = window_center_x;
	mstate_di = NULL; // Set to zero for now
	mouse_y = window_center_y;

	return TRUE;
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
	g_pMouse = NULL;
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
		return FALSE;

	// Set the data format and cooperative level
	// For keyboard we use the standard data model c_dfDIKeyboard
	hr = IDirectInputDevice_SetDataFormat(g_pKeyboard, &c_dfDIKeyboard);
	if (FAILED(hr))
		return FALSE;

	// Get the window handle from SDL
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo((SDL_Window*)pmainwindow, &wmInfo);

	// Try to set cooperative level
	hr = IDirectInputDevice_SetCooperativeLevel(g_pKeyboard, wmInfo.info.win.window, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
	if (FAILED(hr))
		return FALSE;

	// Use buffered input
	dipdw.diph.dwSize = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj = 0;
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.dwData = 32;

	// Set device property (DIPROPDWORD property information)
	hr = IDirectInputDevice_SetProperty(g_pKeyboard, DIPROP_BUFFERSIZE, &dipdw.diph);
	if (FAILED(hr))
		return FALSE;

	// Start out by acquiring the keyboard
	IDirectInputDevice_Acquire(g_pKeyboard);

	return TRUE;
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

	// The DInput code haven't been initialized yet
	if (!DInput_Initialized)
		return;

	// keyboard and mouse weren't enabled
	if (!DInput_KeyboardEnabled || !DInput_MouseEnabled)
		return;

	// Loop through all the buffered mouse events
	while (TRUE)
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
				mouse_x += DInput_AccelerateMovement((int)od.dwData);

				break;
			}
			// Vertical mouse movement
			case DIMOFS_Y:
			{
				mouse_y += DInput_AccelerateMovement((int)od.dwData);

				break;
			}
			// Mouse wheel event
			case DIMOFS_Z:
			{
				// Wheeled upwards
				if ((int)(od.dwData) > 0)
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

	// The DInput code haven't been initialized yet
	if (!DInput_Initialized)
		return;

	// keyboard wasn't enabled
	if (!DInput_KeyboardEnabled)
		return;

	// Loop through all the bufferedkeyboard events
	while (TRUE)
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

		// If we failed once more or the amount of elements we've get
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
				DInput_SendKeyboardEvent(key_last, mpBScanNKey[key_last], TRUE);
				key_ticks_repeat = nTicksCur;
			}
		}
		// Check if we need to repeat again
		else
		{
			if (nTicksCur >= key_ticks_repeat + keyboard_speed)
			{
				DInput_SendKeyboardEvent(key_last, mpBScanNKey[key_last], TRUE);
				key_ticks_repeat = nTicksCur;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sends the keyboard event to the engine dispatcher.
//-----------------------------------------------------------------------------
void DInput_SendKeyboardEvent(DWORD dwOfs, unsigned int nChar, qboolean fDown)
{
	// Trap key event
	eng->TrapMouse_Event(mpBVirtualScanNKey[dwOfs], fDown != FALSE);

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
		::GetCursorPos((POINT*)lpPoint);
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
int DInput_AccelerateMovement(int pos)
{
	if (pos < 0)
		pos *= -1;

	if (newmouseparams_forcespeed >= 1 && pos > newmouseparms[DI_MSPM_THRESHOLD_X])
		pos *= 2;

	if (newmouseparams_forcespeed >= 2 && pos > newmouseparms[DI_MSPM_THRESHOLD_Y])
		pos *= 2;

	return pos * newmouseparms[DI_MSPM_ACCELERATE] / 10;
}

#endif // Following funciton exists on windows/linux.

//-----------------------------------------------------------------------------
// Purpose: Depending on passed fEnable, this funciton enables/disabled mouse
//			movement.
//-----------------------------------------------------------------------------
void SetMouseEnable(qboolean fEnable)
{
#ifdef WIN32
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
			DInput_KeyboardEnabled = FALSE;
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
#endif
}
