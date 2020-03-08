#include <stdio.h>
#include <stdint.h>

#include <windows.h>

#include "log.h"
#include "d3d.h"
#include "smd.h"

static struct {
	DWORD io;
	int epoch;
} dll;

static void smd_dll_handle(MSG* msg) {
	switch (msg->wParam) {
	case SMD_HANDLE_EPOCH:
		dll.epoch = (int)msg->lParam;
		break;
	case SMD_HANDLE_LOG_WR:
		if (msg->lParam) {
			log_init(&msg->lParam, dll.epoch);
			log_designate_thread("tor");
			log_line("Run...");
		}
		else {
			log_line("Exit");
			log_deinit();
			PostThreadMessageA(dll.io, SMD_MSG_HANDLE, -1, 0);
		}
		break;
	case SMD_HANDLE_IO_TID:
		if (msg->lParam)
			dll.io = (DWORD)msg->lParam;
		if (msg->lParam)
			PostThreadMessageA(dll.io, SMD_MSG_HANDLE, SMD_HANDLE_UI, d3d_init(dll.io, 0));
		else
			d3d_deinit();
		break;
	}
}

LRESULT CALLBACK GetMsgProc(int code, WPARAM wParam, LPARAM lParam) {
	MSG msg;
	memcpy(&msg, (void*)lParam, sizeof(msg));
	switch (msg.message) {
	case SMD_MSG_HANDLE:
		smd_dll_handle(&msg);
		break;
	case SMD_MSG_CROSS:
		d3d_cross((int16_t)msg.lParam, (int16_t)(msg.lParam >> 16));
		break;
	case SMD_MSG_NO_CROSS:
		d3d_nocross();
		break;
	case SMD_MSG_SCAN:
		d3d_scan(msg.wParam);
		break;
	case SMD_MSG_LOOT:
		log_line("greedings");
		d3d_loot(msg.wParam);
		break;
	}
	return CallNextHookEx(0, code, wParam, lParam);
}

