#include <stdio.h>
#include <stdint.h>

#include <windows.h>
#include <windowsx.h>
#include <Psapi.h>

#include "minhook-master/include/MinHook.h"

#include "d3d.h"
#include "smd_msg.h"

static int no_debug(const char* format, ...) { return 0; }
static int (*DBG)(const char* format, ...) = no_debug;

LRESULT CALLBACK GetMsgProc(int code, WPARAM wParam, LPARAM lParam) {
	MSG msg;
	memcpy(&msg, (void*)lParam, sizeof(msg));
	if (msg.message == WM_USER) {
		switch (msg.wParam) {
		case SMD_MSG_INIT:
			MH_Initialize();
			d3d_hook();
			PostThreadMessageA(msg.lParam, WM_USER, SMD_MSG_INIT, 0);
			break;
		case SMD_MSG_DBG:
			AllocConsole();
			freopen("CONOUT$", "w", stdout);
			DBG = printf;
			break;
		case SMD_MSG_DEINIT:
			MH_Uninitialize();
			if (DBG == no_debug)
				break;
			fclose(stdout);
			FreeConsole();
			break;
		case SMD_MSG_CROSS:
			d3d_cross((int16_t)msg.lParam, (int16_t)(msg.lParam >> 16));
			break;
		case SMD_MSG_NO_CROSS:
			d3d_nocross();
			break;
		}
	}
	if (msg.message == WM_KEYDOWN)
		DBG("received virtual key code: 0x%x\n", msg.wParam);
	return CallNextHookEx(0, code, wParam, lParam);
}

