#include <windows.h>
#include <windowsx.h>
#include <Psapi.h>

#include <stdint.h>

#include "torui/torui.h"
#include "smd.h"
#include "log.h"

static struct {
	struct {
		int x;
		int y;
		UINT_PTR ticker;
		torui_scan_t scans[TORUI_SCANS];
		uint8_t cnt;
		uint8_t mask;
	} ui;
	struct {
		HANDLE exe;
		DWORD tid;
		HWND win;
	} tor;

	smd_cfg_t smd;

	struct {
		HHOOK msg;
		HANDLE log;
	} scan;

	struct {
		UINT_PTR timer;
		DWORD flags;
	} uno;

	struct {
		HANDLE file;
		uint8_t* scans;
	} map;

	HWND gui;
} io;

////////////////////////////////////////////////////////////////////////////////

static void smd_io_post_smd(UINT msg, WPARAM wPar, LPARAM lPar) {
	if (io.smd.tid)
		PostThreadMessage(io.smd.tid, msg, wPar, lPar);
}

static void smd_io_post_tor(UINT msg, WPARAM wPar, LPARAM lPar) {
	if (io.tor.tid)
		PostThreadMessage(io.tor.tid, msg, wPar, lPar);
}

static void smd_io_post_gui(UINT msg, WPARAM wPar, LPARAM lPar) {
	if (io.gui)
		PostMessage(io.gui, msg, wPar, lPar);
}

static void smd_io_request_cap() {
	smd_io_post_tor(SMD_MSG_SCAN, smd_gui_get_opt(SMD_OPT_LOOT), 0);
}

////////////////////////////////////////////////////////////////////////////////

static CALLBACK void smd_io_start_cap(HWND hwnd, UINT msg, UINT_PTR timer, DWORD millis) {
	if (io.ui.ticker)
		KillTimer(0, io.ui.ticker);
	io.ui.ticker = 0;
	smd_io_request_cap();
}

static CALLBACK void smd_io_no_u(HWND hwnd, UINT msg, UINT_PTR timer, DWORD millis) {
	POINT p;
	GetCursorPos(&p);
	HWND win = WindowFromPoint(p);

	io.uno.flags = (io.uno.flags == MOUSEEVENTF_LEFTUP) ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;

	if (io.uno.flags == MOUSEEVENTF_LEFTUP || win == io.tor.win) {
		INPUT input;
		memset(&input, 0, sizeof(input));
		input.type = INPUT_MOUSE;
		input.mi.dwFlags = io.uno.flags;
		SendInput(1, &input, sizeof(input));
	}
}

static void smd_io_uno(bool set) {
	if (set)
		io.uno.timer = SetTimer(0, io.uno.timer, 500, smd_io_no_u);
	else
		KillTimer(0, io.uno.timer);
}

////////////////////////////////////////////////////////////////////////////////

static void smd_rmb_center(MSG* msg) {
	POINT p;
	p.x = io.ui.x;
	p.y = io.ui.y;
	ClientToScreen((HWND)msg->lParam, &p);
	SetCursorPos(p.x, p.y);

	INPUT input;
	memset(&input, 0, sizeof(input));
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
	SendInput(1, &input, sizeof(input));
}

static void smd_io_click(MSG* msg) {
	INPUT input;
	memset(&input, 0, sizeof(input));
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
	SendInput(1, &input, sizeof(input));
	Sleep(30);
	input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
	SendInput(1, &input, sizeof(input));
	input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
	SendInput(1, &input, sizeof(input));
	input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
	SendInput(1, &input, sizeof(input));
	input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
	SendInput(1, &input, sizeof(input));
	Sleep(30);
	smd_rmb_center(msg);
}

static void smd_io_update_ui(MSG* msg);

static void smd_io_click_tor(torui_e ui) {
	if ((io.ui.mask & (1 << ui)) == 0)
		return;

	INPUT input;
	memset(&input, 0, sizeof(input));
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_RIGHTUP;
	SendInput(1, &input, sizeof(input));
	Sleep(30);

	for (uint8_t i = 0; i < io.ui.cnt; ++i) {
		torui_scan_t* scan = io.ui.scans + i;
		if (scan->ui != ui)
			continue;

		POINT p;
		p.x = scan->x + scan->w/2;
		p.y = scan->y + scan->h/2;
		ClientToScreen(io.tor.win, &p);
		SetCursorPos(p.x, p.y);

		input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
		SendInput(1, &input, sizeof(input));
		Sleep(16);
		input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
		SendInput(1, &input, sizeof(input));
		Sleep(16);

	}
	Sleep(100);
	smd_io_start_cap(0, 0, 0, 0);
	if (smd_peek_messgae(SMD_MSG_SCAN, 300))
		smd_io_update_ui(0);
}

static void smd_io_press(MSG* msg) {
	WORD type = SMD_CFG_TYPE(msg->wParam);
	int vk = msg->wParam;
	int scan = msg->lParam;
	if (!vk && !scan)
		return;

	INPUT key = {
		.type = INPUT_KEYBOARD,
		.ki = {
			.wVk = vk,
			.wScan = scan,
			.dwFlags =  vk ? 0 : KEYEVENTF_SCANCODE,
			.time = 0,
			.dwExtraInfo = 0,
		},
	};
	SendInput(1, &key, sizeof(key));
	key.ki.dwFlags |= KEYEVENTF_KEYUP;
	Sleep(16);
	SendInput(1, &key, sizeof(key));
}

static void smd_io_store_handle(MSG* msg) {
	switch (msg->wParam) {
	case SMD_HANDLE_TID:
		io.smd.tid = (DWORD)msg->lParam;
		if (!io.smd.tid)
			PostQuitMessage(0);
		break;
	case SMD_HANDLE_GUI_WIN:
		io.gui = (HWND)msg->lParam;
		break;
	case SMD_HANDLE_UI:
		log_line("D3D init %i", msg->lParam);
		if (!msg->lParam)
			smd_io_request_cap();
		break;
	}
}

static void smd_io_toggle(MSG* msg) {
	HWND w = (HWND)msg->lParam;
	if (msg->wParam) {
		RECT r;
		GetClientRect(w, &r);
		io.ui.x = (io.smd.x*r.right)/100;
		io.ui.y = (io.smd.y*r.bottom)/100;
		POINT p = { io.ui.x, io.ui.y, };
		ClientToScreen(w, &p);
		SetCursorPos(p.x, p.y);
	}

	INPUT input;
	memset(&input, 0, sizeof(input));
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_MIDDLEUP | (msg->wParam ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
	SendInput(1, &input, sizeof(input));
	smd_io_post_gui(SMD_MSG_TOGGLE, msg->wParam, msg->lParam);
	if (msg->wParam)
		PostMessage(w, SMD_MSG_CROSS, 0, (io.ui.y << 16) | io.ui.x);
	else
		PostMessage(w, SMD_MSG_NO_CROSS, 0, 0);
}

////////////////////////////////////////////////////////////////////////////////

static void smd_io_init_scan() {
	if (!io.smd.dll)
		return;

	HOOKPROC proc = (HOOKPROC)GetProcAddress(io.smd.dll, "GetMsgProc@12");
	io.scan.msg = SetWindowsHookEx(WH_GETMESSAGE, proc, io.smd.dll, io.tor.tid);
	if (!io.scan.msg)
		return log_line("TOR messaging failed %x", GetLastError());

	if (!DuplicateHandle(GetCurrentProcess(), io.smd.log, io.tor.exe, &io.scan.log, 0, false, DUPLICATE_SAME_ACCESS))
		return log_line("TOR log failed %x", GetLastError());

	LPARAM lPar;
	memcpy(&lPar, &io.scan.log, sizeof(io.scan.log));
	smd_io_post_tor(SMD_MSG_HANDLE, SMD_HANDLE_EPOCH, io.smd.epoch);
	smd_io_post_tor(SMD_MSG_HANDLE, SMD_HANDLE_LOG_WR, lPar);
	smd_io_post_tor(SMD_MSG_HANDLE, SMD_HANDLE_IO_TID, (LPARAM)GetCurrentThreadId());
}

static void smd_io_deinit_scan() {
	if (io.ui.ticker)
		KillTimer(0, io.ui.ticker);
	else
		smd_peek_messgae(SMD_MSG_SCAN, 300);

	smd_io_post_tor(SMD_MSG_HANDLE, SMD_HANDLE_IO_TID, 0);
	smd_io_post_tor(SMD_MSG_HANDLE, SMD_HANDLE_LOG_WR, 0);
	if (io.tor.tid)
		smd_peek_messgae(SMD_MSG_HANDLE, 200);

	if (io.map.scans)
		UnmapViewOfFile(io.map.scans);
	if (io.map.file)
		CloseHandle(io.map.file);
	if (io.tor.exe)
		CloseHandle(io.tor.exe);
	if (io.scan.msg)
		UnhookWindowsHookEx(io.scan.msg);
	if (io.scan.log)
		DuplicateHandle(GetCurrentProcess(), io.scan.log, 0, 0, 0, false, DUPLICATE_CLOSE_SOURCE);
	io.tor.tid = 0;
	io.tor.exe = 0;
	io.tor.win = 0;
	io.scan.log = 0;
	io.scan.msg = 0;
	io.map.file = 0;
	io.map.scans = 0;
	io.ui.ticker = 0;
}

////////////////////////////////////////////////////////////////////////////////

static void smd_io_update_ui(MSG* msg) {
	io.ui.cnt = 0;
	if (!io.map.file)
		io.map.file = OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, "SmdToRUi");
	if (io.map.file && !io.map.scans)
		io.map.scans = (uint8_t*)MapViewOfFile(io.map.file, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (io.map.scans) {
		io.ui.cnt = io.map.scans[0];
		memcpy(io.ui.scans, io.map.scans + 1, io.ui.cnt*sizeof(torui_scan_t));
	}
	io.ui.mask = 0;
	for (uint8_t i = 0; i < io.ui.cnt; ++i)
		io.ui.mask |= 1 << io.ui.scans[i].ui;
	smd_io_post_smd(SMD_MSG_SCAN, io.ui.mask, 0);
	io.ui.ticker = SetTimer(0, io.ui.ticker, 250, smd_io_start_cap);
}

static void smd_io_mid() {
	INPUT input;
	memset(&input, 0, sizeof(input));
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
	SendInput(1, &input, sizeof(input));
	Sleep(16);
	input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
	SendInput(1, &input, sizeof(input));
}

static void smd_io_chk_tor(MSG* msg) {
	HWND hwnd = (HWND)msg->lParam;
	if (!io.tor.exe) {
		if (!hwnd || hwnd != GetAncestor(hwnd, GA_ROOT) || !IsWindowVisible(hwnd))
			return;
		char name[2048] = "";
		if (!GetWindowTextA(hwnd, name, sizeof(name)))
			return;

		if (!strstr(name, "The Old Republic"))
			return;

		DWORD pid;
		DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
		HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, pid);
		if (!h)
			return;

		char exe[2048] = "";
		GetModuleFileNameEx(h, 0, exe, sizeof(exe));
		CloseHandle(h);
		if (!strstr(exe, "swtor.exe"))
			return;

		io.tor.exe = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE, 0, pid);
		if (!io.tor.exe)
			return;
		io.tor.win = hwnd;
		io.tor.tid = tid;

		smd_io_post_smd(SMD_MSG_HANDLE, SMD_HANDLE_TOR_WIN, (LPARAM)io.tor.win);
		smd_io_init_scan();
	}
	else {
		DWORD active = 0;
		GetExitCodeProcess(io.tor.exe, &active);
		if (active == STILL_ACTIVE)
			return;
		smd_io_post_smd(SMD_MSG_HANDLE, SMD_HANDLE_TOR_WIN, 0);
		io.tor.tid = 0;
		smd_io_deinit_scan();
	}
}

static void smd_io_cfg(MSG* msg) {
	WORD type = SMD_CFG_TYPE(msg->wParam);
	WORD val = SMD_CFG_VAL(msg->wParam);

	switch (type) {
	case SMD_CFG_OPT:
		if (val == SMD_OPT_NO_U)
			smd_io_uno(msg->lParam);
		break;
	}
}

static void CALLBACK smd_io_init(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
	KillTimer(0, idEvent);
	smd_io_post_smd(SMD_MSG_HANDLE, SMD_HANDLE_IO_TID, (LPARAM)GetCurrentThreadId());
}

static void smd_io_deinit() {
	smd_io_deinit_scan();
}

////////////////////////////////////////////////////////////////////////////////

DWORD WINAPI smd_io_run(LPVOID arg) {
	log_designate_thread("io");
	log_line("Run...");

	MSG msg;
	msg.wParam = 0;
	memcpy(&io.smd, arg, sizeof(io.smd));

	if (SetTimer(0, 0,  USER_TIMER_MINIMUM, smd_io_init)) {
		while (GetMessageA(&msg, 0, 0, 0) > 0) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			switch (msg.message) {
			case SMD_MSG_HANDLE:
				smd_io_store_handle(&msg);
				break;
			case SMD_MSG_CFG:
				smd_io_cfg(&msg);
				break;
			case SMD_MSG_CLICK:
				smd_io_click(&msg);
				smd_io_post_smd(SMD_MSG_CLICK, 0, 0);
				break;
			case SMD_MSG_CLICK_UI:
				smd_io_click_tor((torui_e)msg.wParam);
				smd_io_post_smd(SMD_MSG_CLICK_UI, 0, 0);
				break;
			case SMD_MSG_PRESS:
				smd_io_press(&msg);
				break;
			case SMD_MSG_TOGGLE:
				smd_io_toggle(&msg);
				smd_io_post_smd(SMD_MSG_TOGGLE, 0, 0);
				break;
			case SMD_MSG_TOR_CHK:
				smd_io_chk_tor(&msg);
				break;
			case SMD_MSG_SCAN:
				smd_io_update_ui(&msg);
				break;
			case SMD_MSG_MID:
				smd_io_mid();
				break;
			}
		}
	}
	smd_io_deinit();
	log_line("Exit %i", msg.wParam);
	smd_io_post_smd(SMD_MSG_HANDLE, SMD_HANDLE_IO_TID, 0);
	return msg.wParam;
}
