#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <conio.h>
#include <windows.h>
#include <Psapi.h>

#include "getopt/getopt.h"
#include "smd_msg.h"

#define SMD_DEFAULT_DELAY_MS				200
#define SMD_DEFAULT_X_OFS					50
#define SMD_DEFAULT_Y_OFS					45
#define SMD_CNT_MAX							50

#define VK0			0x30
#define VK1			0x31
#define VK2			0x32
#define VK3			0x33
#define VK4			0x34
#define VK5			0x35
#define VK6			0x36
#define VK7			0x37
#define VK8			0x38
#define VK9			0x39

static int no_debug(const char* format, ...) { return 0; }
static int (*DBG)(const char* format, ...) = no_debug;

typedef enum {
	TRG_IDLE,
	TRG_HELD,
	TRG_RELEASED,
	TRG_FIRED,
} trg_state_e;
typedef enum {
	SMD_INACTIVE,
	SMD_ACTIVE,
	SMD_ACTIVATING,
	SMD_DEACTIVATING,
	SMD_CLICKING,
} smd_state_e;
typedef struct {
	int ts;
	trg_state_e state;
	struct {
		int once;
		int twice;
		int held;
	} btn;
} trigger_t;
struct {
	trigger_t left;
	trigger_t right;
	trigger_t fwd;
	trigger_t bwd;
	trigger_t mid;
	HWND win;
	HMODULE dll;
	UINT_PTR ticker;
	int x;
	int y;
	struct {
		int x;
		int y;
	} ofs;
	struct {
		HHOOK msg;
		HHOOK mouse;
		HHOOK key;
	} hook;
	struct {
		DWORD swtor;
		DWORD hook;
		DWORD emu;
	} thread;
	smd_state_e state;
	int cnt;
	int delay;
} static smd = {
	.left = { .btn = { .once = VK5, .twice = VK6, .held = VK7, }},
	.right = { .btn = { .once = VK8, .twice = VK9, .held = VK0, }},
	.fwd = { .btn = { .once = VK1, .twice = VK2, }},
	.bwd = { .btn = { .once = VK3, .twice = VK4 }},
	.delay = SMD_DEFAULT_DELAY_MS,
	.ofs.x = SMD_DEFAULT_X_OFS,
	.ofs.y = SMD_DEFAULT_Y_OFS,
};
////////////////////////////////////////////////////////////////////////////////
static void smd_set_state(smd_state_e state) {
	DBG("State switch %i -> %i\n", smd.state, state);
	smd.cnt = 0;
	smd.state = state;
}
static void smd_toggle() {
	RECT r;
	GetClientRect(smd.win, &r);
	smd.x = (smd.ofs.x*r.right)/100;
	smd.y = (smd.ofs.y*r.bottom)/100;
	POINT p;
	p.x = smd.x;
	p.y = smd.y;
	ClientToScreen(smd.win, &p);
	SetCursorPos(p.x, p.y);
	DWORD flags = MOUSEEVENTF_RIGHTUP;
	switch (smd.state) {
	case SMD_INACTIVE:
	case SMD_ACTIVATING:
		smd_set_state(SMD_ACTIVATING);
		flags = MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_LEFTUP | MOUSEEVENTF_MIDDLEUP;
		break;
	default:
		smd_set_state(SMD_DEACTIVATING);
		break;
	}
	INPUT mouse = {
		.type = INPUT_MOUSE,
		.mi = {
			.dx = 0,
			.dy = 0,
			.mouseData = 0,
			.dwFlags = flags,
			.time = 0,
			.dwExtraInfo = 0,
		},
	};
	SendInput(1, &mouse, sizeof(mouse));
}
static void smd_press_btn(int btn) {
	PostThreadMessageA(smd.thread.emu, WM_USER, SMD_MSG_PRESS, btn);
}
static void smd_click() {
	INPUT m = {
		.type = INPUT_MOUSE,
		.mi = {
			.dx = 0,
			.dy = 0,
			.mouseData = 0,
			.dwFlags = MOUSEEVENTF_RIGHTUP,
			.time = 0,
			.dwExtraInfo = 0,
		},
	};
	SendInput(1, &m, sizeof(m));
	Sleep(30);
	m.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
	SendInput(1, &m, sizeof(m));
	m.mi.dwFlags = MOUSEEVENTF_LEFTUP;
	SendInput(1, &m, sizeof(m));
	m.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
	SendInput(1, &m, sizeof(m));
	m.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
	SendInput(1, &m, sizeof(m));
	POINT p;
	p.x = smd.x;
	p.y = smd.y;
	ClientToScreen(smd.win, &p);
	SetCursorPos(p.x, p.y);
	m.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
	SendInput(1, &m, sizeof(m));
	Sleep(30);
	PostThreadMessageA(smd.thread.hook, WM_USER, SMD_MSG_CLICK, 0);
}
////////////////////////////////////////////////////////////////////////////////
static void smd_mid_prs() {
	if (smd.mid.state != TRG_IDLE)
		return;
	smd.mid.state = TRG_HELD;
	smd.mid.ts = GetTickCount();
}
static void smd_mid_rls() {
	if (smd.mid.state != TRG_HELD)
		return;
	smd_toggle();
	smd.mid.state = TRG_IDLE;
}
static void smd_mid_machine() {
	if (smd.mid.state != TRG_HELD || GetTickCount() - smd.mid.ts < smd.delay)
		return;
	if (smd.state == SMD_ACTIVE) {
		smd_set_state(SMD_CLICKING);
		PostThreadMessageA(smd.thread.emu, WM_USER, SMD_MSG_CLICK, 0);
	}
	smd.mid.state = TRG_IDLE;
}
////////////////////////////////////////////////////////////////////////////////
static void smd_click_prs(trigger_t* c) {
	switch (c->state) {
	case TRG_IDLE:
		c->ts = GetTickCount();
		c->state = TRG_HELD;
		break;
	case TRG_RELEASED:
		smd_press_btn(c->btn.twice);
		c->state = TRG_IDLE;
		break;
	}
}
static void smd_click_rls(trigger_t* c, trigger_t* o) {
	switch (c->state) {
	case TRG_HELD:
		if (o->state == TRG_HELD || o->state == TRG_RELEASED) {
			c->state = TRG_IDLE;
			o->state = TRG_IDLE;
			smd_press_btn(VK_OEM_MINUS);
			smd_press_btn(VK_OEM_MINUS);
		}
		else {
			c->state = TRG_RELEASED;
		}
		break;
	case TRG_FIRED:
		c->state = TRG_IDLE;
		break;
	}
}
static void smd_click_machine(trigger_t* c, trigger_t* o) {
	if (GetTickCount() - c->ts < smd.delay)
		return;
	switch (c->state) {
	case TRG_HELD:
		c->state = TRG_IDLE;
		if (o->state == TRG_HELD) {
			smd_press_btn(VK_OEM_PLUS);
			smd_press_btn(VK_OEM_PLUS);
			o->state = TRG_IDLE;
			break;
		}
		smd_press_btn(c->btn.held);
		break;
	case TRG_RELEASED:
		c->state = TRG_IDLE;
		smd_press_btn(c->btn.once);
		break;
	}
}
////////////////////////////////////////////////////////////////////////////////
static void smd_scrolled(trigger_t* tr) {
	tr->ts = GetTickCount();
	switch (tr->state) {
	case TRG_IDLE:
		tr->state = TRG_HELD;
		break;
	case TRG_HELD:
		tr->state = TRG_FIRED;
		//ikr
	case TRG_FIRED:
		smd_press_btn(tr->btn.twice);
		break;
	}
}
static void smd_scroll_machine(trigger_t* tr) {
	switch (tr->state) {
	case TRG_HELD:
		if (GetTickCount() - tr->ts < (3*smd.delay/4))
			break;
		smd_press_btn(tr->btn.once);
		tr->state = TRG_IDLE;
		break;
	case TRG_FIRED:
		if (GetTickCount() - tr->ts < (smd.delay/2))
			break;
		tr->state = TRG_IDLE;
		break;
	}
}
////////////////////////////////////////////////////////////////////////////////
static void smd_mouse_machine() {
	smd_click_machine(&smd.left, &smd.right);
	smd_click_machine(&smd.right, &smd.left);
	smd_scroll_machine(&smd.fwd);
	smd_scroll_machine(&smd.bwd);
}
static LRESULT CALLBACK smd_mouse_hook(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode != HC_ACTION)
		return CallNextHookEx(0, nCode, wParam, lParam);
	MSLLHOOKSTRUCT m;
	memcpy(&m, (void*)lParam, sizeof(m));
	HWND w = WindowFromPoint(m.pt);
	if (w != smd.win || (m.flags & LLMHF_INJECTED))
		return CallNextHookEx(0, nCode, wParam, lParam);
	switch (smd.state) {
	case SMD_ACTIVATING:
	case SMD_CLICKING:
		return !0;
	}
	switch (wParam) {
	case WM_MBUTTONUP:
		smd_mid_rls();
		return !0;
	case WM_MBUTTONDOWN:
		smd_mid_prs();
		return !0;
	}
	if (smd.state != SMD_ACTIVE)
		return CallNextHookEx(0, nCode, wParam, lParam);
	switch (wParam) {
	case WM_MOUSEMOVE:
		return CallNextHookEx(0, nCode, wParam, lParam);
	case WM_LBUTTONUP:
		smd_click_rls(&smd.left, &smd.right);
		break;
	case WM_LBUTTONDOWN:
		smd_click_prs(&smd.left);
		break;
	case WM_RBUTTONUP:
		smd_click_rls(&smd.right, &smd.left);
		break;
	case WM_RBUTTONDOWN:
		smd_click_prs(&smd.right);
		break;
	case WM_MOUSEWHEEL:
		smd_scrolled(((int16_t)(m.mouseData >> 16)) < 0 ? &smd.bwd : &smd.fwd);
		break;
	}
	return !0;
}
////////////////////////////////////////////////////////////////////////////////
struct {
	int vk;
	trigger_t tr;
} static keys[] = {
	{ .vk = VK_NUMPAD1, .tr = { .btn = { .once = VK_NUMPAD1, .held = VK_NUMPAD5 }, }, },
	{ .vk = VK_NUMPAD2, .tr = { .btn = { .once = VK_NUMPAD2, .held = VK_NUMPAD6 }, }, },
	{ .vk = VK_NUMPAD3, .tr = { .btn = { .once = VK_NUMPAD3, .held = VK_NUMPAD7 }, }, },
	{ .vk = VK_NUMPAD4, .tr = { .btn = { .once = VK_NUMPAD4, .held = VK_NUMPAD8 }, }, },
};
static void key_prs(trigger_t* tr) {
	if (tr->state != TRG_IDLE)
		return;
	tr->ts = GetTickCount();
	tr->state = TRG_HELD;
}
static void key_rls(trigger_t* tr) {
	if (tr->state == TRG_FIRED)
		tr->state = TRG_IDLE;
	if (tr->state != TRG_HELD)
		return;
	smd_press_btn(tr->btn.once);
	tr->state = TRG_IDLE;
}
static void key_machine(trigger_t* tr) {
	if (GetTickCount() - tr->ts < smd.delay)
		return;
	switch (tr->state) {
	case TRG_HELD:
		smd_press_btn(tr->btn.held);
		tr->state = TRG_FIRED;
		break;
	}
}
static void smd_key_machine() {
	for (int i = 0; i < sizeof(keys)/sizeof(*keys); ++i)
		key_machine(&keys[i].tr);
}
static LRESULT CALLBACK smd_key_hook(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode != HC_ACTION)
		return CallNextHookEx(0, nCode, wParam, lParam);
	KBDLLHOOKSTRUCT key;
	memcpy(&key, (void*)lParam, sizeof(key));
	if (key.flags & LLKHF_INJECTED)
		return CallNextHookEx(0, nCode, wParam, lParam);
	void (*kfp)(trigger_t* tr) = wParam == WM_KEYDOWN ? key_prs : key_rls;
	switch (wParam) {
	case WM_KEYDOWN:
	case WM_KEYUP:
		if (smd.state == SMD_ACTIVE)
			for (int i = 0; i < sizeof(keys)/sizeof(*keys); ++i)
				if (key.vkCode == keys[i].vk) {
					kfp(&keys[i].tr);
					return !0;
				}
		break;
	}
	return CallNextHookEx(0, nCode, wParam, lParam);
}
static void CALLBACK smd_machine(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
	static const int MAX = 50;
	smd_mid_machine();
	CURSORINFO c;
	c.cbSize = sizeof(CURSORINFO);
	switch (smd.state) {
	case SMD_CLICKING:
	case SMD_ACTIVATING:
		++smd.cnt;
		if (smd.cnt < MAX)
			break;
		DBG("Transition timeout\n");
		smd_set_state(SMD_DEACTIVATING);
		smd_toggle();
		break;
	case SMD_ACTIVE:
		smd.cnt += GetCursorInfo(&c) && (c.flags & CURSOR_SHOWING) ? 1 : -smd.cnt;
		if (smd.cnt < MAX)
			break;
		smd_set_state(SMD_DEACTIVATING);
		smd_toggle();
		break;
	default:
		break;
	}
	switch (smd.state) {
	case SMD_ACTIVATING: {
		if (!GetCursorInfo(&c) || (c.flags & CURSOR_SHOWING))
			break;
		smd_set_state(SMD_ACTIVE);
		PostThreadMessageA(smd.thread.swtor, WM_USER, SMD_MSG_CROSS, smd.x | (smd.y << 16));
		break;
	}
	case SMD_ACTIVE:
		smd_mouse_machine();
		smd_key_machine();
		break;
	case SMD_DEACTIVATING:
		if (!GetCursorInfo(&c) || !(c.flags & CURSOR_SHOWING))
			break;
		smd_set_state(SMD_INACTIVE);
		PostThreadMessageA(smd.thread.swtor, WM_USER, SMD_MSG_NO_CROSS, 0);
		break;
	default:
		break;
	}
}
////////////////////////////////////////////////////////////////////////////////
static BOOL CALLBACK smd_find_swtor(HWND win, LPARAM par) {
	if (!IsWindowVisible(win))
		return TRUE;
	DWORD pid;
	DWORD tid = GetWindowThreadProcessId(win, &pid);
	HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, pid);
	if (!h)
		return TRUE;
	char exe[2048] = "";
	GetModuleFileNameEx(h, 0, exe, sizeof(exe));
	CloseHandle(h);
	DBG("%s\n", exe);
	if (!strstr(exe, "swtor.exe"))
		return TRUE;
	smd.win = win;
	smd.thread.swtor = tid;
	return FALSE;
}
static void smd_emu() {
	MSG msg;
	while (GetMessageA(&msg, 0, 0, 0) != -1) {
		if (msg.message == WM_QUIT)
			break;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.message != WM_USER)
			continue;
		switch (msg.wParam) {
		case SMD_MSG_CLICK:
			smd_click();
			break;
		case SMD_MSG_PRESS: {
			INPUT key = {
				.type = INPUT_KEYBOARD,
				.ki = {
					.wVk = msg.lParam,
					.wScan = MapVirtualKey(msg.lParam, MAPVK_VK_TO_VSC),
					.dwFlags =  0,
					.time = 0,
					.dwExtraInfo = 0,
				},
			};
			DBG("press virtual key: 0x%x\n", msg.lParam);
			SendInput(1, &key, sizeof(key));
			key.ki.dwFlags |= KEYEVENTF_KEYUP;
			Sleep(16);
			SendInput(1, &key, sizeof(key));
			break;
		}
		case SMD_MSG_INIT:
			PlaySound("gen2.wav", NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
			break;
		}
	}
}
static BOOL smd_has_admin() {
	HANDLE h = NULL;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &h))
		return FALSE;
	TOKEN_ELEVATION elevation;
	DWORD len = sizeof(elevation);
	if (!GetTokenInformation(h, TokenElevation, &elevation, sizeof( elevation ), &len))
		elevation.TokenIsElevated = FALSE;
	CloseHandle(h);
	return elevation.TokenIsElevated;
}
static void smd_exit() {
	DBG("Exiting\n");
	KillTimer(0, smd.ticker);
	UnhookWindowsHookEx(smd.hook.mouse);
	UnhookWindowsHookEx(smd.hook.key);
	if (smd.thread.swtor)
		PostThreadMessageA(smd.thread.swtor, WM_USER, SMD_MSG_DEINIT, 0);
	Sleep(1000);
	UnhookWindowsHookEx(smd.hook.msg);
	FreeLibrary(smd.dll);
}
static void smd_catch(int signal) {
	exit(signal);
}
////////////////////////////////////////////////////////////////////////////////
int main(int argn, char* argv[]) {

	int elevated = 0;
	int keyhook = 1;
	struct option lopt[] = {
		{ "elevated", no_argument, &elevated, 1 },
		{ "no-key-hook", no_argument, &keyhook, 0 },
		{ "delay", required_argument, 0, 'd' },
		{ "x-offset", required_argument, 0, 'x' },
		{ "y-offset", required_argument, 0, 'y' },
		{ "debug", no_argument, 0, 'D' },
		{ 0, 0, 0, 0 }
	};
	int ix = 0;
	int c;
	while ((c = getopt_long(argn, argv, "", lopt, &ix)) != -1) {
		switch (c) {
		case 'd':
			smd.delay = atoi(optarg);
			break;
		case 'x':
			smd.ofs.x = atoi(optarg);
			break;
		case 'y':
			smd.ofs.y = atoi(optarg);
			break;
		case 'D':
			DBG = printf;
			break;
		}
	}
	if (!smd_has_admin()) {
		if (elevated) {
			DBG("Elevation failed\n");
			return 0;
		}
		char cmd[2048];
		cmd[0] = 0;
		int l = 0;
		for (int i = 0; i < argn; ++i)
			l += strlen(argv[i]) + 1;
		if (l >= sizeof(cmd))
			return 0;
		for (int i = 1; i < argn; ++i) {
			strcat(cmd, argv[i]);
			strcat(cmd, " ");
		}
		strcat(cmd, " --elevated");
		char exe[2048];
		GetModuleFileName(0, exe, sizeof(exe));
		SHELLEXECUTEINFO sei;
		sei.cbSize = sizeof(sei);
		sei.fMask = SEE_MASK_DEFAULT | SEE_MASK_NOASYNC;
		sei.hwnd = 0;
		sei.lpVerb = "runas";
		sei.lpFile = exe;
		sei.lpParameters = cmd;
		sei.lpDirectory = 0;
		sei.nShow = SW_NORMAL;
		sei.hInstApp = 0;
		ShellExecuteExA(&sei);
		return 0;
	}
	FILE* readme = fopen("README.MD", "r");
	if (readme) {
		for (char c = fgetc(readme); c != EOF; c = fgetc(readme))
			printf("%c", c);
		printf("\n\n");
		fclose(readme);
	}
	smd.thread.hook =  GetCurrentThreadId();
	EnumWindows(smd_find_swtor, 0);
	if (!smd.win) {
		printf("SWTOR not found\n");
		getch();
		return 0;
	}
	atexit(smd_exit);
	signal(SIGABRT, smd_catch);
	signal(SIGINT, smd_catch);
	signal(SIGTERM, smd_catch);
	signal(SIGBREAK, smd_catch);
	smd.dll = LoadLibraryA("smd.dll");
	smd.hook.msg = SetWindowsHookEx(WH_GETMESSAGE, (HOOKPROC)GetProcAddress(smd.dll, "GetMsgProc@12"), smd.dll, smd.thread.swtor);
	if (!smd.hook.msg) {
		printf("Unable to hook messages\n");
		getch();
		return 0;
	}
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
	smd.thread.emu = GetThreadId(CreateThread(0, 0, (LPTHREAD_START_ROUTINE)smd_emu, 0, 0, 0));
	smd.ticker = SetTimer(0, 0,  USER_TIMER_MINIMUM, smd_machine);
	smd.hook.mouse = SetWindowsHookEx(WH_MOUSE_LL, smd_mouse_hook, 0, 0);
	if (keyhook)
		smd.hook.key = SetWindowsHookEx(WH_KEYBOARD_LL, smd_key_hook, 0, 0);
	PostThreadMessageA(smd.thread.swtor, WM_USER, SMD_MSG_INIT, smd.thread.emu);
	if (DBG != no_debug)
		PostThreadMessageA(smd.thread.swtor, WM_USER, SMD_MSG_DBG, 0);
	MSG msg;
	while (GetMessageA(&msg, 0, 0, 0) != -1) {
		if (msg.message == WM_QUIT)
			break;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.message != WM_USER)
			continue;
		switch (msg.wParam) {
		case SMD_MSG_CLICK:
			smd_set_state(SMD_ACTIVATING);
			break;
		}
	}
	return 0;
}
