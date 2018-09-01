#include <stdio.h>
#include <stdint.h>

#include <windows.h>
#include <windowsx.h>
#include <Psapi.h>

#include "log.h"
#include "d3d.h"
#include "smd.h"

#define VK0			0x30
#define VK9			0x39

static bool smd_should_log_key(uint8_t vk) {
	if (vk >= VK0 && vk <= VK9)
		return true;
	if (vk >= VK_NUMPAD1 && vk <= VK_NUMPAD4)
		return true;
	if (vk == VK_OEM_MINUS || vk == VK_OEM_PLUS)
		return true;
	return false;
}
LRESULT CALLBACK GetMsgProc(int code, WPARAM wParam, LPARAM lParam) {
	MSG msg;
	memcpy(&msg, (void*)lParam, sizeof(msg));
	switch (msg.message) {
	case SMD_MSG_INIT:
		log_line("D3D init");
		PostThreadMessageA(msg.wParam, SMD_MSG_INIT, d3d_init(msg.wParam, (HANDLE)msg.lParam), 0);
		break;
	case SMD_MSG_LOG:
		log_init(&msg.lParam);
		log_designate_thread("swtor");
		break;
	case SMD_MSG_DEINIT:
		d3d_deinit();
		log_line("Deinit");
		log_deinit();
		break;
	case SMD_MSG_CROSS:
		d3d_cross((int16_t)msg.lParam, (int16_t)(msg.lParam >> 16));
		break;
	case SMD_MSG_NO_CROSS:
		d3d_nocross();
		break;
	case SMD_MSG_FLASH_LOOT:
		d3d_flash_loot();
		break;
	case WM_KEYUP:
	case WM_SYSCHAR:
		if (smd_should_log_key(msg.wParam))
			log_line("Got virtual key: 0x%x", msg.wParam);
		break;
	}
	return CallNextHookEx(0, code, wParam, lParam);
}

