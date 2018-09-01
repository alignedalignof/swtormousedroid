#include <stdatomic.h>
#include <stdbool.h>

#include "uiscan/uiscan.h"
#include "log.h"
#include "trigger.h"

#include "smd.h"

#define SMD_CNT_MAX							50

#define VK_0			0x30
#define VK_1			0x31
#define VK_2			0x32
#define VK_3			0x33
#define VK_4			0x34
#define VK_5			0x35
#define VK_6			0x36
#define VK_7			0x37
#define VK_8			0x38
#define VK_9			0x39
#define VK_g			0x47
#define VK_n			0x4e
#define VK_x			0x58

typedef enum {
	SMD_INACTIVE,
	SMD_ACTIVE,
	SMD_ACTIVATING,
	SMD_DEACTIVATING,
	SMD_CLICKING,
	SMD_CLICKING_UI,
} smd_state_e;

struct smd {
	HMODULE dll;
	UINT_PTR ticker;
	int x;
	int y;
	struct {
		HHOOK msg;
		HHOOK mouse;
		HHOOK key;
	} hook;
	struct {
		DWORD swtor;
		DWORD hook;
		DWORD input;
	} thread;
	smd_state_e state;
	int cnt;
	int delay;
	struct {
		UiElement elements[20];
		uint8_t count;
		atomic_bool lock;
		struct {
			int vk;
			UiControl control;
		} keys[3];
		struct {
			HANDLE sink;
			HANDLE source;
		} pipe;
	} ui;
	struct {
		struct {
			int vk;
			trigger_t trigger;
		} keys[VK_NUMPAD5 - VK_NUMPAD1];
		trigger_t lmb;
		trigger_t mid;
		trigger_t rmb;
		trigger_t fwd;
		trigger_t bwd;
	} trigger;
	smd_cfg_t cfg;
} static smd;
static void smd_press_btn(int btn) {
	PostThreadMessageA(smd.thread.input, SMD_MSG_PRESS, btn, 0);
	PostThreadMessageA(smd.thread.input, SMD_MSG_PRESS, btn, 0);
}
////////////////////////////////////////////////////////////////////////////////
static bool smd_ui_has_control(UiControl control) {
	bool locked = false;
	if (!atomic_compare_exchange_strong(&smd.ui.lock, &locked, true))
		return false;
	int i;
	for (i = 0; i < smd.ui.count; ++i)
		if (smd.ui.elements[i].control == control)
			break;
	bool ret = i < smd.ui.count;
	atomic_store(&smd.ui.lock, false);
	return ret;
}
static void smd_set_state(smd_state_e state) {
	log_line("State switch %i -> %i", smd.state, state);
	smd.cnt = 0;
	smd.state = state;
}
static void smd_toggle() {
	RECT r;
	GetClientRect(smd.cfg.win, &r);
	smd.x = (smd.cfg.x*r.right)/100;
	smd.y = (smd.cfg.y*r.bottom)/100;
	POINT p;
	p.x = smd.x;
	p.y = smd.y;
	ClientToScreen(smd.cfg.win, &p);
	SetCursorPos(p.x, p.y);
	DWORD flags = MOUSEEVENTF_RIGHTUP;
	switch (smd.state) {
	case SMD_INACTIVE:
	case SMD_ACTIVATING:
		if (smd_ui_has_control(UI_CONTROL_X)) {
			smd_set_state(SMD_CLICKING_UI);
			PostThreadMessageA(smd.thread.input, SMD_MSG_CLICK_UI, SMD_ACTIVATING, UI_CONTROL_X);
		}
		else {
			flags = MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_LEFTUP | MOUSEEVENTF_MIDDLEUP;
			smd_set_state(SMD_ACTIVATING);
			PostThreadMessageA(smd.thread.input, SMD_MSG_TOGGLE, flags, 0);
		}
		break;
	default:
		smd_set_state(SMD_DEACTIVATING);
		PostThreadMessageA(smd.thread.input, SMD_MSG_TOGGLE, flags, 0);
		break;
	}
}
////////////////////////////////////////////////////////////////////////////////
static void smd_trigger_tick() {
	trigger_tick(&smd.trigger.lmb);
	trigger_tick(&smd.trigger.rmb);
	trigger_tick(&smd.trigger.fwd);
	trigger_tick(&smd.trigger.bwd);
	for (int i = 0; i < sizeof(smd.trigger.keys)/sizeof(*smd.trigger.keys); ++i)
		trigger_tick(&smd.trigger.keys[i].trigger);
}
static void smd_trigger_reset() {
	trigger_reset(&smd.trigger.lmb);
	trigger_reset(&smd.trigger.rmb);
	trigger_reset(&smd.trigger.fwd);
	trigger_reset(&smd.trigger.bwd);
	for (int i = 0; i < sizeof(smd.trigger.keys)/sizeof(*smd.trigger.keys); ++i)
		trigger_reset(&smd.trigger.keys[i].trigger);
}
static void CALLBACK smd_machine(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
	static const int MAX = 50;
	trigger_tick(&smd.trigger.mid);
	CURSORINFO c;
	c.cbSize = sizeof(CURSORINFO);
	switch (smd.state) {
	case SMD_CLICKING:
	case SMD_ACTIVATING:
		++smd.cnt;
		if (smd.cnt < MAX)
			break;
		log_line("Transition timeout");
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
		smd_trigger_reset();
		smd_set_state(SMD_ACTIVE);
		PostThreadMessageA(smd.thread.swtor, SMD_MSG_CROSS, 0, smd.x | (smd.y << 16));
		break;
	}
	case SMD_ACTIVE:
		smd_trigger_tick();
		break;
	case SMD_DEACTIVATING:
		if (!GetCursorInfo(&c) || !(c.flags & CURSOR_SHOWING))
			break;
		smd_set_state(SMD_INACTIVE);
		PostThreadMessageA(smd.thread.swtor, SMD_MSG_NO_CROSS, 0, 0);
		break;
	default:
		break;
	}
}
////////////////////////////////////////////////////////////////////////////////
static bool smd_key_for_ui(int vk, UiControl* control) {
	for (int i = 0; i < sizeof(smd.ui.keys)/sizeof(*smd.ui.keys); ++i)
		if (vk == smd.ui.keys[i].vk && smd_ui_has_control(smd.ui.keys[i].control))
			return *control = smd.ui.keys[i].control, true;
	return false;
}
static LRESULT CALLBACK smd_key_hook(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode != HC_ACTION)
		return CallNextHookEx(0, nCode, wParam, lParam);
	KBDLLHOOKSTRUCT key;
	memcpy(&key, (void*)lParam, sizeof(key));
	if (key.flags & LLKHF_INJECTED)
		return CallNextHookEx(0, nCode, wParam, lParam);
	void (*kfp)(trigger_t* trigger) = wParam == WM_KEYDOWN ? trigger_press : trigger_release;
	UiControl control;
	switch (wParam) {
	case WM_KEYDOWN:
	case WM_KEYUP:
		if (smd.state != SMD_CLICKING_UI && smd_key_for_ui(key.vkCode, &control)) {
			PostThreadMessageA(smd.thread.input, SMD_MSG_CLICK_UI, smd.state, control);
			smd_set_state(SMD_CLICKING_UI);
			return !0;
			switch (control) {
			case UI_CONTROL_X:
				if (smd_ui_has_control(UI_CONTROL_GREED) || smd_ui_has_control(UI_CONTROL_NEED)) {
					log_line("Select loot before closing");
					PostThreadMessageA(smd.thread.swtor, SMD_MSG_FLASH_LOOT, 0, 0);
					break;
				}
				//ikr
			case UI_CONTROL_GREED:
			case UI_CONTROL_NEED:
				PostThreadMessageA(smd.thread.input, SMD_MSG_CLICK_UI, smd.state, control);
				smd_set_state(SMD_CLICKING_UI);
				return !0;
			}
		}
		for (int i = 0; i < sizeof(smd.trigger.keys)/sizeof(*smd.trigger.keys); ++i) {
			if (key.vkCode != smd.trigger.keys[i].vk)
				continue;
			kfp(&smd.trigger.keys[i].trigger);
			return !0;
		}
		break;
	}
	return CallNextHookEx(0, nCode, wParam, lParam);
}
static LRESULT CALLBACK smd_mouse_hook(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode != HC_ACTION)
		return CallNextHookEx(0, nCode, wParam, lParam);
	MSLLHOOKSTRUCT mouse;
	memcpy(&mouse, (void*)lParam, sizeof(mouse));
	HWND w = WindowFromPoint(mouse.pt);
	if (w != smd.cfg.win || (mouse.flags & LLMHF_INJECTED))
		return CallNextHookEx(0, nCode, wParam, lParam);
	switch (smd.state) {
	case SMD_ACTIVATING:
	case SMD_CLICKING:
	case SMD_CLICKING_UI:
		return !0;
	}
	switch (wParam) {
	case WM_MBUTTONUP:
		trigger_release(&smd.trigger.mid);
		return !0;
	case WM_MBUTTONDOWN:
		trigger_press(&smd.trigger.mid);
		return !0;
	}
	if (smd.state != SMD_ACTIVE)
		return CallNextHookEx(0, nCode, wParam, lParam);
	switch (wParam) {
	case WM_MOUSEMOVE:
		return CallNextHookEx(0, nCode, wParam, lParam);
	case WM_LBUTTONUP:
		trigger_release(&smd.trigger.lmb);
		break;
	case WM_LBUTTONDOWN:
		trigger_press(&smd.trigger.lmb);
		break;
	case WM_RBUTTONUP:
		trigger_release(&smd.trigger.rmb);
		break;
	case WM_RBUTTONDOWN:
		trigger_press(&smd.trigger.rmb);
		break;
	case WM_MOUSEWHEEL: {
		trigger_t* wheel = ((int16_t)(mouse.mouseData >> 16)) < 0 ? &smd.trigger.bwd : &smd.trigger.fwd;
		trigger_press(wheel);
		trigger_release(wheel);
		break;
	}
	}
	return !0;
}
////////////////////////////////////////////////////////////////////////////////
static void smd_rmb_center() {
	POINT p;
	p.x = smd.x;
	p.y = smd.y;
	ClientToScreen(smd.cfg.win, &p);
	SetCursorPos(p.x, p.y);

	INPUT input;
	memset(&input, 0, sizeof(input));
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
	SendInput(1, &input, sizeof(input));
}
static void smd_click() {
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
	smd_rmb_center();
	PostThreadMessageA(smd.thread.hook, SMD_MSG_CLICK, 0, 0);
}
static void smd_press(int vk) {
	log_line("Send virtual key: 0x%x", vk);

	INPUT key = {
		.type = INPUT_KEYBOARD,
		.ki = {
			.wVk = vk,
			.wScan = MapVirtualKey(vk, MAPVK_VK_TO_VSC),
			.dwFlags =  0,
			.time = 0,
			.dwExtraInfo = 0,
		},
	};
	SendInput(1, &key, sizeof(key));
	key.ki.dwFlags = KEYEVENTF_KEYUP;
	Sleep(16);
	SendInput(1, &key, sizeof(key));
}
static void smd_click_ui(UiControl control) {
	INPUT input;
	memset(&input, 0, sizeof(input));
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
	SendInput(1, &input, sizeof(input));
	Sleep(30);
	for (int i = 0; i < smd.ui.count; ++i) {
		if (smd.ui.elements[i].control != control)
			continue;
		POINT p;
		p.x = (smd.ui.elements[i].left + smd.ui.elements[i].right)/2;
		p.y = (smd.ui.elements[i].top + smd.ui.elements[i].bottom)/2;
		ClientToScreen(smd.cfg.win, &p);
		SetCursorPos(p.x, p.y);
		input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
		SendInput(1, &input, sizeof(input));
		Sleep(16);
		input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
		SendInput(1, &input, sizeof(input));
	}
}
static void smd_copy_ui(uint8_t count) {
	bool unlocked = false;
	if (!atomic_compare_exchange_strong(&smd.ui.lock, &unlocked, true))
		return;
	smd.ui.count = sizeof(smd.ui.elements)/sizeof(*smd.ui.elements);
	if (count < smd.ui.count)
		smd.ui.count = count;
	if (smd.ui.count && !ReadFile(smd.ui.pipe.sink, smd.ui.elements, smd.ui.count*sizeof(*smd.ui.elements), 0, 0))
		smd.ui.count = 0;
	atomic_store(&smd.ui.lock, false);
}
static DWORD WINAPI smd_input(LPVOID arg) {
	log_designate_thread("input");
	MSG msg;
	bool close_x = false;
	smd_state_e from;
	while (GetMessageA(&msg, 0, 0, 0) != -1) {
		if (msg.message == WM_QUIT)
			break;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		switch (msg.message) {
		case SMD_MSG_INIT:
			log_line("D3D init status %i", msg.wParam);
			if (msg.wParam)
				exit(-1);
			break;
		case SMD_MSG_CLICK:
			smd_click();
			break;
		case SMD_MSG_PRESS:
			smd_press(msg.wParam);
			break;
		case SMD_MSG_CLICK_UI:
			from = (smd_state_e)msg.wParam;
			if (from == SMD_ACTIVE)
				from = SMD_ACTIVATING;
			UiControl control = (UiControl)msg.lParam;
			if (control == UI_CONTROL_X) {
				if (smd_ui_has_control(UI_CONTROL_GREED) || smd_ui_has_control(UI_CONTROL_NEED)) {
					log_line("Select loot before closing");
					PostThreadMessageA(smd.thread.swtor, SMD_MSG_FLASH_LOOT, 0, 0);
				}
				else {
					smd_click_ui(control);
					close_x = true;
					break;
				}
			}
			else {
				smd_click_ui(control);
			}
			if (from == SMD_ACTIVATING)
				smd_rmb_center();
			PostThreadMessageA(smd.thread.hook, SMD_MSG_CLICK_UI, from, 0);
			break;
		case SMD_MSG_TOGGLE: {
			INPUT input;
			memset(&input, 0, sizeof(input));
			input.type = INPUT_MOUSE;
			input.mi.dwFlags = msg.wParam;
			SendInput(1, &input, sizeof(input));
			break;
		}
		case SMD_MSG_SCAN:
			smd_copy_ui(msg.wParam);
			log_line("UI element count %u", smd.ui.count);
			for (int i = 0; i < smd.ui.count; ++i)
				log_line("%u @%u, %u", smd.ui.elements[i].control, smd.ui.elements[i].top, smd.ui.elements[i].left);
			if (close_x) {
				if (smd_ui_has_control(UI_CONTROL_X)) {
					PostThreadMessageA(smd.thread.input, SMD_MSG_CLICK_UI, from, UI_CONTROL_X);
				}
				else {
					close_x = false;
					if (from == SMD_ACTIVATING)
						smd_rmb_center();
					PostThreadMessageA(smd.thread.hook, SMD_MSG_CLICK_UI, from, 0);
				}
			}
			break;
		}
	}
	return 0;
}
int smd_init(const smd_cfg_t* smd_cfg) {
	if (!smd_cfg)
		return -1;
	smd.cfg = *smd_cfg;

	trigger_reset(&smd.trigger.mid);
	smd.trigger.mid.timeout = smd.cfg.delay;
	smd.trigger.rmb.timeout = smd.cfg.delay;
	smd.trigger.lmb.timeout = smd.cfg.delay;
	smd.trigger.fwd.timeout = 3*smd.cfg.delay/4;
	smd.trigger.bwd.timeout = 3*smd.cfg.delay/4;

	smd.thread.hook =  GetCurrentThreadId();
	smd.thread.swtor = smd_cfg->thread;

	smd.dll = LoadLibraryA("smd.dll");
	if (!smd.dll) {
		log_line("Unable to load SMD library %i", GetLastError());
		return -1;
	}
	smd.hook.msg = SetWindowsHookEx(WH_GETMESSAGE, (HOOKPROC)GetProcAddress(smd.dll, "GetMsgProc@12"), smd.dll, smd.thread.swtor);
	if (!smd.hook.msg) {
		log_line("Unable to inject SMD library %i", GetLastError());
		return -1;
	}
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	smd.ticker = SetTimer(0, 0,  USER_TIMER_MINIMUM, smd_machine);
	smd.hook.mouse = SetWindowsHookEx(WH_MOUSE_LL, smd_mouse_hook, 0, 0);
	smd.hook.key = SetWindowsHookEx(WH_KEYBOARD_LL, smd_key_hook, 0, 0);
	if (!smd.hook.key || !smd.hook.mouse || !smd.ticker) {
		log_line("Unable to hook user input");
		return -1;
	}

	HANDLE input = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)smd_input, 0, CREATE_SUSPENDED, &smd.thread.input);
	SetThreadPriority(input, THREAD_PRIORITY_BELOW_NORMAL);
	ResumeThread(input);

	if (smd.cfg.scan) {
		CreatePipe(&smd.ui.pipe.sink, &smd.ui.pipe.source, 0, 8192);
		if (!DuplicateHandle(GetCurrentProcess(), smd.ui.pipe.source, smd.cfg.swtor, &smd.ui.pipe.source, 0, false, DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
			log_line("Unable to open UI stream %i", GetLastError());
			return -1;
		}
	}
	DuplicateHandle(GetCurrentProcess(), smd.cfg.log, smd.cfg.swtor, &smd.cfg.log, 0, false, DUPLICATE_SAME_ACCESS);
	PostThreadMessageA(smd.thread.swtor, SMD_MSG_LOG, 0, (LPARAM)smd.cfg.log);
	PostThreadMessageA(smd.thread.swtor, SMD_MSG_INIT, smd.thread.input, (LPARAM)smd.ui.pipe.source);
	return 0;
}
void smd_process_msg(MSG* msg) {
	switch (msg->message) {
	case SMD_MSG_CLICK:
		smd_set_state(SMD_ACTIVATING);
		break;
	case SMD_MSG_CLICK_UI:
		smd_set_state(msg->wParam);
		break;
	}
}
void smd_deinit() {
	if (!smd.dll)
		return;
	if (smd.ticker)
		KillTimer(0, smd.ticker);
	if (smd.hook.mouse)
		UnhookWindowsHookEx(smd.hook.mouse);
	if (smd.hook.key)
		UnhookWindowsHookEx(smd.hook.key);
	if (smd.thread.swtor)
		PostThreadMessageA(smd.thread.swtor, SMD_MSG_DEINIT, 0, 0);
	Sleep(1000);
	if (smd.ui.pipe.sink)
		CloseHandle(smd.ui.pipe.sink);
	if (smd.ui.pipe.source)
		DuplicateHandle(GetCurrentProcess(), smd.ui.pipe.source, 0, 0, 0, false, DUPLICATE_CLOSE_SOURCE);
	if (smd.cfg.log)
		DuplicateHandle(GetCurrentProcess(), smd.cfg.log, 0, 0, 0, false, DUPLICATE_CLOSE_SOURCE);
	if (smd.hook.msg)
		UnhookWindowsHookEx(smd.hook.msg);
	if (smd.dll)
		FreeLibrary(smd.dll);
	smd.dll = 0;
}
////////////////////////////////////////////////////////////////////////////////
static void smd_left_right_click() {
	if (smd.state != SMD_ACTIVE)
		return;
	trigger_release(&smd.trigger.mid);
	smd_set_state(SMD_CLICKING);
	PostThreadMessageA(smd.thread.input, SMD_MSG_CLICK, 0, 0);
}
static void smd_lmb_once() {
	if (trigger_is_held(&smd.trigger.rmb))
		smd_press_btn(VK_0);
	else
		smd_press_btn(VK_5);
}
static void smd_lmb_twice() {
	if (trigger_is_held(&smd.trigger.rmb))
		smd_press_btn(VK_OEM_MINUS);
	else
		smd_press_btn(VK_6);
}
static void smd_lmb_held() {
	if (trigger_is_held(&smd.trigger.rmb))
		smd_press_btn(VK_OEM_PLUS);
	else
		smd_press_btn(VK_7);
}
static void smd_rmb_once() {
	smd_press_btn(VK_8);
}
static void smd_rmb_twice() {
	smd_press_btn(VK_9);
}
static void smd_rmb_held() {

}
static void smd_fwd_once() {
	if (trigger_is_held(&smd.trigger.rmb))
		smd_press_btn(VK_NUMPAD1);
	else
		smd_press_btn(VK_1);
}
static void smd_fwd_twice() {
	if (trigger_is_held(&smd.trigger.rmb))
		smd_press_btn(VK_NUMPAD2);
	else
		smd_press_btn(VK_2);
}
static void smd_bwd_once() {
	if (trigger_is_held(&smd.trigger.rmb))
		smd_press_btn(VK_NUMPAD3);
	else
		smd_press_btn(VK_3);
}
static void smd_bwd_twice() {
	if (trigger_is_held(&smd.trigger.rmb))
		smd_press_btn(VK_NUMPAD4);
	else
		smd_press_btn(VK_4);
}
static void smd_press_num1() { smd_press_btn(VK_NUMPAD1); }
static void smd_press_num2() { smd_press_btn(VK_NUMPAD2); }
static void smd_press_num3() { smd_press_btn(VK_NUMPAD3); }
static void smd_press_num4() { smd_press_btn(VK_NUMPAD4); }
static void smd_press_num5() { smd_press_btn(VK_NUMPAD5); }
static void smd_press_num6() { smd_press_btn(VK_NUMPAD6); }
static void smd_press_num7() { smd_press_btn(VK_NUMPAD7); }
static void smd_press_num8() { smd_press_btn(VK_NUMPAD8); }


struct smd static smd = {
	.state = SMD_INACTIVE,
	.trigger = {
		.keys = {
			{ .vk = VK_NUMPAD1, .trigger = { .once = smd_press_num1, .held = smd_press_num5, } },
			{ .vk = VK_NUMPAD2, .trigger = { .once = smd_press_num2, .held = smd_press_num6, } },
			{ .vk = VK_NUMPAD3, .trigger = { .once = smd_press_num3, .held = smd_press_num7, } },
			{ .vk = VK_NUMPAD4, .trigger = { .once = smd_press_num4, .held = smd_press_num8, } },
		},
		.lmb = {
			.once = smd_lmb_once,
			.twice = smd_lmb_twice,
			.held = smd_lmb_held,
		},
		.mid = {
			.once = smd_toggle,
			.held = smd_left_right_click,
		},
		.rmb = {
			.once = smd_rmb_once,
			.twice = smd_rmb_twice,
			.held = smd_rmb_held,
		},
		.fwd = {
			.once = smd_fwd_once,
			.twice = smd_fwd_twice,
		},
		.bwd = {
			.once = smd_bwd_once,
			.twice = smd_bwd_twice,
		},
	},
	.ui = {
		.keys = {
			{ .vk = VK_g, .control = UI_CONTROL_GREED, },
			{ .vk = VK_n, .control = UI_CONTROL_NEED, },
			{ .vk = VK_x, .control = UI_CONTROL_X, },
		},
	},
};
