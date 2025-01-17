﻿/*
 *    sfall
 *    Copyright (C) 2008 - 2021 Timeslip and sfall team
 *
 */

#include "FalloutEngine\Fallout2.h"
#include "main.h"
#include "InputFuncs.h"
#include "version.h"

#include "WinProc.h"

namespace sfall
{

static HWND window;
static Rectangle win;
static POINT client;

static long moveWindowKey[2];
static long windowData;

static long reqGameQuit;
static bool cCursorShow = true;
static bool bkgndErased = false;

void __stdcall WinProc::MessageWindow() {
	MSG msg;
	while (PeekMessageA(&msg, 0, 0, 0, 0)) {
		if (GetMessageA(&msg, 0, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
	}
}

void __stdcall WinProc::WaitMessageWindow() {
	MsgWaitForMultipleObjectsEx(0, 0, 1, 0xFF, 0);
	MessageWindow();
}

static int __stdcall WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	RECT rect;
	//POINT point;

	switch (msg) {
	//case WM_CREATE:
	//	break;

	case WM_DESTROY:
		__asm xor  eax, eax;
		__asm call fo::funcoffs::exit_;
		return 1;

	case WM_MOVE:
		client.x = LOWORD(lParam);
		client.y = HIWORD(lParam);
		break;

	case WM_WINDOWPOSCHANGED:
		win.x = ((WINDOWPOS*)lParam)->x;
		win.y = ((WINDOWPOS*)lParam)->y;
		break;

	case WM_ERASEBKGND:
		if (bkgndErased || !window) return 1;
		bkgndErased = true;
		break;

	case WM_PAINT:
		if (window && GetUpdateRect(hWnd, &rect, 0) != 0) { // it is not clear under what conditions the redrawing is required
			rect.right -= 1;
			rect.bottom -= 1;
			__asm {
				lea  eax, rect;
				call fo::funcoffs::win_refresh_all_;
			}
		}
		break;

	case WM_ACTIVATE:
		if (!cCursorShow && wParam == WA_INACTIVE) {
			cCursorShow = true;
			ShowCursor(1);
		}
		break;

	case WM_SETCURSOR:
	{
		short type = LOWORD(lParam);
		/*if (type == HTCAPTION || type == HTBORDER || type == HTMINBUTTON || type == HTCLOSE || type == HTMAXBUTTON) {
			if (!cCursorShow) {
				cCursorShow = true;
				ShowCursor(1);
			}
		}
		else*/ if (type == HTCLIENT && fo::var::getInt(FO_VAR_GNW95_isActive)) {
			if (cCursorShow) {
				cCursorShow = false;
				ShowCursor(0);
			}
		}
		return 1;
		//break;
	}
	case WM_SYSCOMMAND:
		if ((wParam & 0xFFF0) == SC_SCREENSAVE || (wParam & 0xFFF0) == SC_MONITORPOWER) return 0;
		break;

	case WM_ACTIVATEAPP:
		fo::var::setInt(FO_VAR_GNW95_isActive) = wParam;
		if (wParam) { // active
			/*point.x = 0;
			point.y = 0;
			ClientToScreen(hWnd, &point);
			GetClientRect(hWnd, &rect);
			rect.left += point.x;
			rect.right += point.x;
			rect.top += point.y;
			rect.bottom += point.y;
			ClipCursor(&rect);*/
			__asm {
				mov  eax, 1;
				call fo::funcoffs::GNW95_hook_input_;
				mov  eax, FO_VAR_scr_size;
				call fo::funcoffs::win_refresh_all_;
			}
		} else{
			// ClipCursor(0);
			__asm xor  eax, eax;
			__asm call fo::funcoffs::GNW95_hook_input_;
		}
		return 0;

	case WM_CLOSE:
		__asm {
			call fo::funcoffs::main_menu_is_shown_;
			test eax, eax;
			jnz  skip;
			call fo::funcoffs::game_quit_with_confirm_;
		skip:
			mov  reqGameQuit, eax;
		}
		return 0;
	}
	return DefWindowProcA(hWnd, msg, wParam, lParam);
}

static long __stdcall main_menu_loop_hook() {
	return (!reqGameQuit) ? fo::func::get_input() : 27; // ESC code
}

void WinProc::SetWindowProc() {
	SetWindowLongA((HWND)fo::var::getInt(FO_VAR_GNW95_hwnd), GWL_WNDPROC, (LONG)WindowProc);
}

void WinProc::SetHWND(HWND _window) {
	window = _window;
}

void WinProc::SetSize(long w, long h, char scaleX2) {
	if (scaleX2) {
		w *= 2;
		h *= 2;
	}
	win.width = w;
	win.height = h;
}

// Sets the window style and its position/size
void WinProc::SetStyle(long windowStyle) {
	SetWindowLongA(window, GWL_STYLE, windowStyle);
	RECT r;
	r.left = 0;
	r.right = win.width;
	r.top = 0;
	r.bottom = win.height;
	AdjustWindowRect(&r, windowStyle, false);
	r.right -= r.left;
	r.bottom -= r.top;
	SetWindowPos(window, HWND_NOTOPMOST, win.x, win.y, r.right, r.bottom, SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOCOPYBITS);
}

void WinProc::SetTitle(long wWidth, long wHeight, long gMode) {
	char windowTitle[128];
	char mode[4] = "DX9";
	if (gMode < 4) mode[2] = '7';

	if (HRP::Setting::ScreenWidth() != wWidth || HRP::Setting::ScreenHeight() != wHeight) {
		std::sprintf(windowTitle, "%s   @sfall-e v" VERSION_STRING "  %ix%i >> %ix%i [%s]",
			(const char*)0x50AF08, HRP::Setting::ScreenWidth(), HRP::Setting::ScreenHeight(), wWidth, wHeight, mode);
	} else {
		std::sprintf(windowTitle, "%s   @sfall-e v" VERSION_STRING "  [%s]", (const char*)0x50AF08, mode);
	}
	SetWindowTextA(window, windowTitle);
}

void WinProc::SetMoveKeys() {
	moveWindowKey[0] = IniReader::GetConfigInt("Input", "WindowScrollKey", 0);
	if (moveWindowKey[0] < 0) {
		switch (moveWindowKey[0]) {
		case -1:
			moveWindowKey[0] = DIK_LCONTROL;
			moveWindowKey[1] = DIK_RCONTROL;
			break;
		case -2:
			moveWindowKey[0] = DIK_LMENU;
			moveWindowKey[1] = DIK_RMENU;
			break;
		case -3:
			moveWindowKey[0] = DIK_LSHIFT;
			moveWindowKey[1] = DIK_RSHIFT;
			break;
		default:
			moveWindowKey[0] = 0;
		}
	} else {
		moveWindowKey[0] &= 0xFF;
	}
}

void WinProc::Moving() {
	if (moveWindowKey[0] != 0 && (KeyDown(moveWindowKey[0]) || (moveWindowKey[1] != 0 && KeyDown(moveWindowKey[1])))) {
		int mx, my;
		GetMouse(&mx, &my);
		win.x += mx;
		win.y += my;

		RECT toRect, curRect;
		toRect.left = win.x;
		toRect.right = win.x + win.width;
		toRect.top = win.y;
		toRect.bottom = win.y + win.height;
		AdjustWindowRect(&toRect, WS_CAPTION, false);

		toRect.right -= (toRect.left - win.x);
		toRect.left = win.x;
		toRect.bottom -= (toRect.top - win.y);
		toRect.top = win.y;

		if (GetWindowRect(GetShellWindow(), &curRect)) {
			if (toRect.right > curRect.right) {
				DWORD move = toRect.right - curRect.right;
				win.x -= move;
				toRect.right -= move;
			}
			else if (toRect.left < curRect.left) {
				DWORD move = curRect.left - toRect.left;
				win.x += move;
				toRect.right += move;
			}
			if (toRect.bottom > curRect.bottom) {
				DWORD move = toRect.bottom - curRect.bottom;
				win.y -= move;
				toRect.bottom -= move;
			}
			else if (toRect.top < curRect.top) {
				DWORD move = curRect.top - toRect.top;
				win.y += move;
				toRect.bottom += move;
			}
		}
		MoveWindow(window, win.x, win.y, toRect.right - win.x, toRect.bottom - win.y, true);
	}
}

void WinProc::SetToCenter(long wWidth, long wHeight, long* outX, long* outY) {
	RECT desktop;
	GetWindowRect(GetDesktopWindow(), &desktop);

	*outX = (desktop.right / 2) - (wWidth  / 2);
	*outY = (desktop.bottom / 2) - (wHeight / 2);
}

void WinProc::LoadPosition() {
	windowData = IniReader::GetConfigInt("Graphics", "WindowData", -1);
	if (windowData > 0) {
		win.x = windowData >> 16;
		win.y = windowData & 0xFFFF;
	} else if (windowData == -1) {
		WinProc::SetToCenter(win.width, win.height, &win.x, &win.y);
	}
}

void WinProc::SavePosition(long mode) {
	RECT rect;
	if ((mode == 2 || mode == 5) && GetWindowRect(window, &rect)) {
		int data = rect.top | (rect.left << 16);
		if (data >= 0 && data != windowData) IniReader::SetConfigInt("Graphics", "WindowData", data);
	}
}

const POINT* WinProc::GetClientPos() {
	return &client;
}

void WinProc::init() {
	// Replace the engine WindowProc_ with its own
	MakeJump(0x4DE9FC, WindowProc); // WindowProc_

	HookCall(0x481B2A, main_menu_loop_hook);
}

}