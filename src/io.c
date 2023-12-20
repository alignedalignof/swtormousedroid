#include <windows.h>
#include <windowsx.h>
#include <Psapi.h>

#include <stdint.h>

#include "smd.h"
#include "log.h"

static struct {
	struct {
		HANDLE exe;
		DWORD tid;
		HWND win;
		HHOOK msg;
		HANDLE log;
	} tor;
	struct {
		int x;
		int y;
	} ui;
	smd_cfg_t smd;
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
	if (msg->wParam & 1)
	{
		input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
		SendInput(1, &input, sizeof(input));
		input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
		SendInput(1, &input, sizeof(input));
	}
	if (msg->wParam & 2)
	{
		input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
		SendInput(1, &input, sizeof(input));
		input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
		SendInput(1, &input, sizeof(input));
	}
	Sleep(30);
	smd_rmb_center(msg);
}

static void smd_io_mod(MSG* msg)
{
	static WORD shift_scan;
	static WORD ctrl_scan;
	static WORD alt_scan;

	shift_scan = shift_scan ? shift_scan : MapVirtualKeyA(VK_SHIFT, MAPVK_VK_TO_VSC);
	ctrl_scan = ctrl_scan ? ctrl_scan : MapVirtualKeyA(VK_CONTROL, MAPVK_VK_TO_VSC);
	alt_scan = alt_scan ? alt_scan : MapVirtualKeyA(VK_MENU, MAPVK_VK_TO_VSC);

	bool shift = msg->wParam & 1;
	bool alt = msg->wParam & 2;
	bool ctrl = msg->wParam & 4;
	bool down = msg->wParam & 8;

	int count = 0;
	INPUT mods[3] =
	{
			{
		.type = INPUT_KEYBOARD,
		.ki = {
			.wVk = 0,
			.wScan = 0,
			.dwFlags =  (down ? 0 : KEYEVENTF_KEYUP) | KEYEVENTF_SCANCODE,
			.time = 0,
			.dwExtraInfo = 0,
		},
			}, {
		.type = INPUT_KEYBOARD,
		.ki = {
			.wVk = 0,
			.wScan = 0,
			.dwFlags =  (down ? 0 : KEYEVENTF_KEYUP) | KEYEVENTF_SCANCODE,
			.time = 0,
			.dwExtraInfo = 0,
		},
			}, {
		.type = INPUT_KEYBOARD,
		.ki = {
			.wVk = 0,
			.wScan = 0,
			.dwFlags =  (down ? 0 : KEYEVENTF_KEYUP) | KEYEVENTF_SCANCODE,
			.time = 0,
			.dwExtraInfo = 0,
		},
			}
	};
	if (shift) mods[count++].ki.wScan = shift_scan;
	if (ctrl) mods[count++].ki.wScan = ctrl_scan;
	if (alt) mods[count++].ki.wScan = alt_scan;
	if (count)
	{
		SendInput(count, mods, sizeof(INPUT));
		//Sleep(16);
	}

}

static void smd_io_press(MSG* msg) {
	static WORD shift_scan;
	static WORD ctrl_scan;
	static WORD alt_scan;

	shift_scan = shift_scan ? shift_scan : MapVirtualKeyA(VK_SHIFT, MAPVK_VK_TO_VSC);
	ctrl_scan = ctrl_scan ? shift_scan : MapVirtualKeyA(VK_CONTROL, MAPVK_VK_TO_VSC);
	alt_scan = alt_scan ? alt_scan : MapVirtualKeyA(VK_MENU, MAPVK_VK_TO_VSC);

	int vk = msg->wParam & 0xff;
	int scan = msg->lParam;
	bool shift = msg->wParam & 0x100;
	bool alt = msg->wParam & 0x200;
	bool ctrl = msg->wParam & 0x400;
	bool down = msg->wParam & 0x800;
	if (!vk && !scan)
		return;

	int count = 0;
	INPUT keys[4] =
	{
			{
		.type = INPUT_KEYBOARD,
		.ki = {
			.wVk = 0,
			.wScan = 0,
			.dwFlags =  KEYEVENTF_SCANCODE,
			.time = 0,
			.dwExtraInfo = 0,
		},
			}, {
		.type = INPUT_KEYBOARD,
		.ki = {
			.wVk = 0,
			.wScan = 0,
			.dwFlags =  KEYEVENTF_SCANCODE,
			.time = 0,
			.dwExtraInfo = 0,
		},
			}, {
		.type = INPUT_KEYBOARD,
		.ki = {
			.wVk = 0,
			.wScan = 0,
			.dwFlags =  KEYEVENTF_SCANCODE,
			.time = 0,
			.dwExtraInfo = 0,
		},
			}
	};
	if (shift) keys[count++].ki.wScan = shift_scan;
	if (ctrl) keys[count++].ki.wScan = ctrl_scan;
	if (alt) keys[count++].ki.wScan = alt_scan;
	keys[count].ki.wVk = vk;
	keys[count].ki.wScan = scan;
	keys[count].ki.dwFlags = vk ? 0 : KEYEVENTF_SCANCODE;
	count++;

	SendInput(count, keys, sizeof(INPUT));
	Sleep(16);
	for (int i = 0; i < count; i++) keys[i].ki.dwFlags |= KEYEVENTF_KEYUP;
	SendInput(count, keys, sizeof(INPUT));
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
		break;
	}
}

static void smd_io_toggle(MSG* msg) {
	HWND w = (HWND)msg->lParam;
	DWORD active = msg->wParam;
	if (active)
	{
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
	input.mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_MIDDLEUP | (active ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
	SendInput(1, &input, sizeof(input));
	smd_io_post_gui(SMD_MSG_TOGGLE, active, (LPARAM)w);
	if (active)
		PostMessage(w, SMD_MSG_CROSS, 0, (io.ui.y << 16) | io.ui.x);
	else
		PostMessage(w, SMD_MSG_NO_CROSS, 0, 0);
}

////////////////////////////////////////////////////////////////////////////////

static void smd_io_init_dll()
{
	if (!io.smd.dll) return;

	HOOKPROC proc = (HOOKPROC)GetProcAddress(io.smd.dll, "GetMsgProc");
	io.tor.msg = SetWindowsHookEx(WH_GETMESSAGE, proc, io.smd.dll, io.tor.tid);

	if (!io.tor.msg) return log_line("TOR messaging failed %x", GetLastError());
	if (!DuplicateHandle(GetCurrentProcess(), io.smd.log, io.tor.exe, &io.tor.log, 0, false, DUPLICATE_SAME_ACCESS))
		return log_line("TOR log failed %x", GetLastError());

	LPARAM lPar;
	memcpy(&lPar, &io.tor.log, sizeof(io.tor.log));
	smd_io_post_tor(SMD_MSG_HANDLE, SMD_HANDLE_EPOCH, io.smd.epoch);
	smd_io_post_tor(SMD_MSG_HANDLE, SMD_HANDLE_LOG_WR, lPar);
	smd_io_post_tor(SMD_MSG_HANDLE, SMD_HANDLE_IO_TID, (LPARAM)GetCurrentThreadId());
}

static void smd_io_deinit_dll() {
	smd_io_post_tor(SMD_MSG_HANDLE, SMD_HANDLE_IO_TID, 0);
	smd_io_post_tor(SMD_MSG_HANDLE, SMD_HANDLE_LOG_WR, 0);
	if (io.tor.tid)
		smd_peek_messgae(SMD_MSG_HANDLE, 200);

	if (io.tor.exe)
		CloseHandle(io.tor.exe);
	if (io.tor.msg)
		UnhookWindowsHookEx(io.tor.msg);
	if (io.tor.log)
		DuplicateHandle(GetCurrentProcess(), io.tor.log, 0, 0, 0, false, DUPLICATE_CLOSE_SOURCE);
	io.tor.tid = 0;
	io.tor.exe = 0;
	io.tor.win = 0;
}

////////////////////////////////////////////////////////////////////////////////

static void smd_io_mid()
{
	INPUT input;
	memset(&input, 0, sizeof(input));
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
	SendInput(1, &input, sizeof(input));
	Sleep(16);
	input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
	SendInput(1, &input, sizeof(input));
}

static void smd_io_mousex(MSG* msg)
{
	INPUT input;
	memset(&input, 0, sizeof(input));
	input.type = INPUT_MOUSE;
	input.mi.mouseData = msg->wParam;
	input.mi.dwFlags = MOUSEEVENTF_XDOWN;
	SendInput(1, &input, sizeof(input));
	Sleep(16);
	input.mi.dwFlags = MOUSEEVENTF_XUP;
	SendInput(1, &input, sizeof(input));
}

static void smd_io_mouse_scroll(MSG* msg)
{
	INPUT input;
	memset(&input, 0, sizeof(input));
	input.type = INPUT_MOUSE;
	input.mi.mouseData = msg->lParam;
	input.mi.dwFlags = msg->wParam ? MOUSEEVENTF_HWHEEL : MOUSEEVENTF_WHEEL;
	SendInput(1, &input, sizeof(input));
	Sleep(16);
}

static void smd_io_chk_tor(MSG* msg)
{
	HWND hwnd = (HWND)msg->lParam;
	if (!io.tor.exe)
	{
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
		smd_io_init_dll();
	}
	else
	{
		DWORD active = 0;
		GetExitCodeProcess(io.tor.exe, &active);
		if (active == STILL_ACTIVE)
			return;
		smd_io_post_smd(SMD_MSG_HANDLE, SMD_HANDLE_TOR_WIN, 0);
		smd_io_deinit_dll();
	}
}

static void CALLBACK smd_io_init(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	KillTimer(0, idEvent);
	smd_io_post_smd(SMD_MSG_HANDLE, SMD_HANDLE_IO_TID, (LPARAM)GetCurrentThreadId());
}

static void smd_io_deinit()
{
	smd_io_deinit_dll();
}

////////////////////////////////////////////////////////////////////////////////

DWORD WINAPI smd_io_run(LPVOID arg)
{
	log_designate_thread("io");
	log_line("Run...");

	MSG msg;
	msg.wParam = 0;
	memcpy(&io.smd, arg, sizeof(io.smd));

	if (SetTimer(0, 0,  USER_TIMER_MINIMUM, smd_io_init))
	while (GetMessageA(&msg, 0, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		switch (msg.message) {
		case SMD_MSG_HANDLE:
			smd_io_store_handle(&msg);
			break;
		case SMD_MSG_CLICK:
			smd_io_click(&msg);
			smd_io_post_smd(SMD_MSG_CLICK, 0, 0);
			break;
		case SMD_MSG_PRESS:
			smd_io_press(&msg);
			break;
		case SMD_MSG_MOD:
			smd_io_mod(&msg);
			break;
		case SMD_MSG_TOGGLE:
			smd_io_toggle(&msg);
			smd_io_post_smd(SMD_MSG_TOGGLE, 0, 0);
			break;
		case SMD_MSG_TOR_CHK:
			smd_io_chk_tor(&msg);
			break;
		case SMD_MSG_MID:
			smd_io_mid();
			break;
		case SMD_MSG_MX:
			smd_io_mousex(&msg);
			break;
		case SMD_MSG_SCROLL:
			smd_io_mouse_scroll(&msg);
			break;
		}
	}
	smd_io_deinit();
	log_line("Exit %i", msg.wParam);
	smd_io_post_smd(SMD_MSG_HANDLE, SMD_HANDLE_IO_TID, 0);
	return msg.wParam;
}
