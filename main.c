/*
 * Simplified clone of the Caffeine application for Windows Vista and newer
 *
 * Original idea and Mac implementation by Lighthead Software
 *     http://lightheadsw.com/caffeine/
 *
 * Copyright (C) 2013 Patrick Schlangen <patrick@schlangen.me>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */
#include <Windows.h>
#include <tchar.h>

#include "resource.h"

/*
 * Enable manifest generation for modern look&feel
 */
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")

/*
 * IDs for menu items and messagesw
 */
#define ID_TRAYICON			500
#define ID_MENU_EXIT		510
#define ID_MENU_TOGGLE		511
#define ID_MENU_PREFS		512
#define WM_SHELLNOTIFY		(WM_USER+1)

/*
 * Structure for configuration options
 */
struct CaffeinePrefs_t
{
	BOOL bDisplayOn;
	BOOL bNoStandby;
	BOOL bAutoStart;
	BOOL bAutoEnable;
};

/*
 * Global variables
 */
HINSTANCE g_hInstance;
WCHAR g_szClassName[] = _T("CaffeineTrayIcon");
HWND g_hTrayIcon;
HMENU g_hTrayMenu;
NOTIFYICONDATA g_TrayIcon;
BOOL g_bIsEnabled = FALSE;
struct CaffeinePrefs_t g_cPrefs;

/*
 * Core functionality (set thread execution state to prevent display-off etc)
 */
void CaffeineSet(BOOL enable)
{
	EXECUTION_STATE esFlags = ES_CONTINUOUS;

	if(enable)
	{
		if(g_cPrefs.bDisplayOn)
			esFlags |= ES_DISPLAY_REQUIRED;
		if(g_cPrefs.bNoStandby)
			esFlags |= ES_SYSTEM_REQUIRED;

		g_TrayIcon.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON_ENABLED));
		wcscpy_s(g_TrayIcon.szTip, sizeof(g_TrayIcon.szTip), _T("Caffeine (enabled)"));

		CheckMenuItem(g_hTrayMenu, ID_MENU_TOGGLE, MF_BYCOMMAND | MF_CHECKED);
	}
	else
	{
		g_TrayIcon.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON_DISABLED));
		wcscpy_s(g_TrayIcon.szTip, sizeof(g_TrayIcon.szTip), _T("Caffeine (disabled)"));
		
		CheckMenuItem(g_hTrayMenu, ID_MENU_TOGGLE, MF_BYCOMMAND | MF_UNCHECKED);
	}
	
	SetThreadExecutionState(esFlags);
	
	Shell_NotifyIcon(NIM_MODIFY, &g_TrayIcon);

	g_bIsEnabled = enable;
}

/*
 * Toggle Caffeine state
 */
void CaffeineToggle()
{
	CaffeineSet(!g_bIsEnabled);
}

/*
 * Save preferences from g_cPrefs to registry
 */
void PrefsSave()
{
	HKEY hk;
	WCHAR strFileName[MAX_PATH];

	if(RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\Caffeine"), 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hk, NULL) == ERROR_SUCCESS)
	{
		RegSetValueEx(hk, _T("DisplayOn"), 0, REG_DWORD,
			(BYTE *)&g_cPrefs.bDisplayOn, sizeof(g_cPrefs.bDisplayOn));
		RegSetValueEx(hk, _T("NoStandby"), 0, REG_DWORD,
			(BYTE *)&g_cPrefs.bNoStandby, sizeof(g_cPrefs.bNoStandby));
		RegSetValueEx(hk, _T("AutoEnable"), 0, REG_DWORD,
			(BYTE *)&g_cPrefs.bAutoEnable, sizeof(g_cPrefs.bAutoEnable));
		RegSetValueEx(hk, _T("AutoStart"), 0, REG_DWORD,
			(BYTE *)&g_cPrefs.bAutoEnable, sizeof(g_cPrefs.bAutoStart));

		RegCloseKey(hk);
	}

	if(RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_ALL_ACCESS, &hk) == ERROR_SUCCESS)
	{
		if(g_cPrefs.bAutoStart)
		{
			GetModuleFileName(GetModuleHandle(NULL), strFileName, sizeof(strFileName));
			RegSetValueEx(hk, _T("Caffeine"), 0, REG_SZ, (BYTE *)strFileName, wcslen(strFileName)*sizeof(WCHAR)+1);
		}
		else
		{
			RegDeleteValue(hk, _T("Caffeine"));
		}

		RegCloseKey(hk);
	}
}

/*
 * Load preferences from registry to g_cPrefs
 */
void PrefsLoad()
{
	HKEY hk;
	DWORD dwMaxLen;
	
	g_cPrefs.bDisplayOn		= TRUE;
	g_cPrefs.bNoStandby		= TRUE;
	g_cPrefs.bAutoEnable	= FALSE;
	g_cPrefs.bAutoStart		= FALSE;

	if(RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Caffeine"), 0, KEY_QUERY_VALUE, &hk) == ERROR_SUCCESS)
	{
		dwMaxLen = sizeof(g_cPrefs.bDisplayOn);
		RegQueryValueEx(hk, _T("DisplayOn"), NULL, NULL, (BYTE *)&g_cPrefs.bDisplayOn, &dwMaxLen);
		
		dwMaxLen = sizeof(g_cPrefs.bNoStandby);
		RegQueryValueEx(hk, _T("NoStandby"), NULL, NULL, (BYTE *)&g_cPrefs.bNoStandby, &dwMaxLen);

		dwMaxLen = sizeof(g_cPrefs.bAutoEnable);
		RegQueryValueEx(hk, _T("AutoEnable"), NULL, NULL, (BYTE *)&g_cPrefs.bAutoEnable, &dwMaxLen);
		
		dwMaxLen = sizeof(g_cPrefs.bAutoStart);
		RegQueryValueEx(hk, _T("AutoStart"), NULL, NULL, (BYTE *)&g_cPrefs.bAutoStart, &dwMaxLen);
		
		RegCloseKey(hk);
	}
}

/*
 * Preferences dialog callback
 */
INT_PTR CALLBACK PrefsDlgProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		{
			SendMessage(hWndDlg, WM_SETICON, ICON_SMALL,
				(LPARAM)LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON_ENABLED)));
			SendMessage(hWndDlg, WM_SETICON, ICON_BIG,
				(LPARAM)LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON_ENABLED)));
			
			SendDlgItemMessage(hWndDlg,
				IDC_CHECK_DISPLAY_ON,
				BM_SETCHECK,
				g_cPrefs.bDisplayOn ? BST_CHECKED : BST_UNCHECKED,
				0);
			SendDlgItemMessage(hWndDlg,
				IDC_CHECK_NO_STANDBY,
				BM_SETCHECK,
				g_cPrefs.bNoStandby ? BST_CHECKED : BST_UNCHECKED,
				0);
			SendDlgItemMessage(hWndDlg,
				IDC_CHECK_AUTOSTART,
				BM_SETCHECK,
				g_cPrefs.bAutoStart ? BST_CHECKED : BST_UNCHECKED,
				0);
			SendDlgItemMessage(hWndDlg,
				IDC_CHECK_AUTOENABLE,
				BM_SETCHECK,
				g_cPrefs.bAutoEnable ? BST_CHECKED : BST_UNCHECKED,
				0);
		}
		break;

	case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
			case IDOK:
				g_cPrefs.bDisplayOn = SendDlgItemMessage(hWndDlg, IDC_CHECK_DISPLAY_ON,
					BM_GETCHECK, 0, 0) == BST_CHECKED;
				g_cPrefs.bNoStandby = SendDlgItemMessage(hWndDlg, IDC_CHECK_NO_STANDBY,
					BM_GETCHECK, 0, 0) == BST_CHECKED;
				g_cPrefs.bAutoStart = SendDlgItemMessage(hWndDlg, IDC_CHECK_AUTOSTART,
					BM_GETCHECK, 0, 0) == BST_CHECKED;
				g_cPrefs.bAutoEnable = SendDlgItemMessage(hWndDlg, IDC_CHECK_AUTOENABLE,
					BM_GETCHECK, 0, 0) == BST_CHECKED;
				
				// save prefs to registry
				PrefsSave();

				// if currently enabled, commit configuration changes by disabling and re-enabling
				if(g_bIsEnabled)
				{
					CaffeineSet(FALSE);
					CaffeineSet(TRUE);
				}

				// close dialog
				EndDialog(hWndDlg, IDOK);
				break;

			case IDCANCEL:
				EndDialog(hWndDlg, IDCANCEL);
				break;
			};
		}
		break;

	default:
		return(FALSE);
	};

	return(TRUE);
}

/*
 * Show preferences dialog
 */
void PrefsShow()
{
	SetForegroundWindow(g_hTrayIcon);
	DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG_PREFS), g_hTrayIcon, PrefsDlgProc);
}

/*
 * Tray icon callback
 */
LRESULT CALLBACK TrayIconWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	POINT cursorPos;

	switch(uMsg)
	{
	case WM_CREATE:
		{
			g_hTrayMenu = CreatePopupMenu();
			AppendMenu(g_hTrayMenu, MF_STRING | MF_UNCHECKED, ID_MENU_TOGGLE, _T("&Enable"));
			AppendMenu(g_hTrayMenu, MF_SEPARATOR, 0, NULL);
			AppendMenu(g_hTrayMenu, MF_STRING, ID_MENU_PREFS, _T("&Preferences"));
			AppendMenu(g_hTrayMenu, MF_SEPARATOR, 0, NULL);
			AppendMenu(g_hTrayMenu, MF_STRING, ID_MENU_EXIT, _T("E&xit"));

			g_TrayIcon.cbSize			= sizeof(g_TrayIcon);
			g_TrayIcon.hWnd				= hWnd;
			g_TrayIcon.uID				= ID_TRAYICON;
			g_TrayIcon.uFlags			= NIF_ICON|NIF_MESSAGE|NIF_TIP;
			g_TrayIcon.uCallbackMessage	= WM_SHELLNOTIFY;
			g_TrayIcon.hIcon			= LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON_DISABLED));
			wcscpy_s(g_TrayIcon.szTip, sizeof(g_TrayIcon.szTip), _T("Caffeine (disabled)"));

			Shell_NotifyIcon(NIM_ADD, &g_TrayIcon);
			CaffeineSet(g_cPrefs.bAutoEnable);

			return(0);
		}
		break;

	case WM_CLOSE:
		{
			DestroyWindow(hWnd);
		}
		break;

	case WM_DESTROY:
		{
			Shell_NotifyIcon(NIM_DELETE, &g_TrayIcon);
			PostQuitMessage(0);
		}
		break;

	case WM_SHELLNOTIFY:
		{
			if(wParam == ID_TRAYICON)
			{
				switch(lParam)
				{
				case WM_LBUTTONUP:
					CaffeineToggle();
					return(0);

				case WM_RBUTTONUP:
					GetCursorPos(&cursorPos);
					SetForegroundWindow(hWnd);
					TrackPopupMenu(g_hTrayMenu,
						TPM_RIGHTALIGN,
						cursorPos.x, cursorPos.y,
						0, hWnd, NULL);
					return(0);
				};
			}
		}
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case ID_MENU_EXIT:
			DestroyWindow(hWnd);
			return(0);

		case ID_MENU_TOGGLE:
			CaffeineToggle();
			return(0);

		case ID_MENU_PREFS:
			PrefsShow();
			return(0);
		};
	};
	
	return(DefWindowProc(hWnd, uMsg, wParam, lParam));
}

/*
 * Create tray icon
 */
BOOL TrayIconSetup()
{
	WNDCLASSEX wc;

	wc.cbSize			= sizeof(wc);
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= 0;
	wc.lpfnWndProc		= TrayIconWndProc;
	wc.style			= 0;
	wc.hInstance		= g_hInstance;
	wc.hIcon			= LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON_ENABLED));
	wc.hIconSm			= LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON_ENABLED));
	wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName	= g_szClassName;
	wc.hbrBackground	= (HBRUSH)COLOR_WINDOW;
	wc.lpszMenuName		= NULL;

	if(!RegisterClassEx(&wc))
	{
		MessageBox(NULL, _T("Failed to register tray icon window class."), _T("Error"),
			MB_ICONERROR | MB_OK);
		return(FALSE);
	}
	
	g_hTrayIcon = CreateWindowEx(0,
		g_szClassName,
		_T("Caffeine"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		240, 120,
		NULL, NULL,
		g_hInstance,
		NULL);
	if(g_hTrayIcon == NULL)
	{
		MessageBox(NULL, _T("Failed to create tray icon window."), _T("Error"),
			MB_ICONERROR | MB_OK);
		return(FALSE);
	}

	return(TRUE);
}

/*
 * Entry point
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	MSG Msg;

	g_hInstance = hInstance;

	PrefsLoad();

	if(!TrayIconSetup())
		return(1);

	while(GetMessage(&Msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}

	return((int)Msg.wParam);
}
