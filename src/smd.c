#include <stdbool.h>
#include <stdint.h>

#include "log.h"
#include "trigger.h"
#include "torui/torui.h"
#include "smd.h"

#define SMD_CNT_MAX							50

typedef enum {
	SMD_INACTIVE,
	SMD_ACTIVE,
	SMD_ACTIVATING,
} smd_state_e;

struct smd {
	UINT_PTR ticker;

	int x;
	int y;
	struct {
		HHOOK mouse;
		HHOOK key;
	} hook;
	smd_state_e state;
	int delay;
	int timeout;

	struct {
		trigger_t lmb;
		trigger_t rmb;
		trigger_t fwd;
		trigger_t bwd;
		trigger_t mid;
		trigger_t key;
		trigger_t click;
		trigger_t loot;
	} trigger;
	bool mods[SMD_MOD_CNT];
	smd_cfg_t cfg;
	struct {
		DWORD itid;
		struct {
			HANDLE sink;
			HANDLE source;
		} ui;
		struct {
			DWORD tid;
			HANDLE exe;
			HWND win;
		} tor;
		HWND gui;
		HANDLE hio;
		HANDLE hui;
	} handle;

	HWND win;
	LPARAM toggle;
	bool ack;
	uint8_t ui;

	struct {
		UINT_PTR ticker;
		const char* desc[3];
		torui_e opts[3];
		uint8_t def;
		uint8_t foc;
	} loot;
} static smd;

////////////////////////////////////////////////////////////////////////////////

static void smd_post_io(UINT msg, WPARAM wPar, LPARAM lPar) {
	if (smd.handle.itid)
		PostThreadMessage(smd.handle.itid, msg, wPar, lPar);
}

static void smd_post_tor(UINT msg, WPARAM wPar, LPARAM lPar) {
	if (smd.handle.tor.win)
		PostMessage(smd.handle.tor.win, msg, wPar, lPar);
}

static void smd_post_gui(UINT msg, WPARAM wPar, LPARAM lPar) {
	if (smd.handle.gui)
		PostMessage(smd.handle.gui, msg, wPar, lPar);
}

static void smd_post_win(UINT msg, WPARAM wPar, LPARAM lPar) {
	if (smd.win == smd.handle.tor.win)
		smd_post_tor(msg, wPar, lPar);
	else if (smd.win == smd.handle.gui)
		smd_post_gui(msg, wPar, lPar);
}

static void smd_post_toggle(bool set) {
	smd_post_io(SMD_MSG_TOGGLE, set, (LPARAM)smd.win);
}

static void smd_post_win_check(HWND hwnd) {
	smd_post_io(SMD_MSG_TOR_CHK, 0, (LPARAM)hwnd);
}

static bool smd_ui_has(torui_e ui) {
	return smd.ui & (1 << ui);
}

////////////////////////////////////////////////////////////////////////////////

static void smd_trigger_tick() {
	for (trigger_t* t = (trigger_t*)&smd.trigger; t <= &smd.trigger.loot; ++t)
		trigger_tick(t);
}

static void smd_trigger_reset_binds() {
	for (trigger_t* t = (trigger_t*)&smd.trigger; t < &smd.trigger.mid; ++t)
		trigger_reset(t);
	smd.mods[SMD_MOD_NON] = false;
	smd.mods[SMD_MOD_LMB] = false;
	smd.mods[SMD_MOD_RMB] = false;
}

////////////////////////////////////////////////////////////////////////////////

static CALLBACK void smd_auto_loot(HWND hwnd, UINT msg, UINT_PTR timer, DWORD millis);

static void smd_set_state(smd_state_e state) {
	int timeouts[] = { 0, 500, 1000, };
	smd.timeout = GetTickCount() + timeouts[state];
	smd.state = state;
	if (state == SMD_ACTIVATING)
		smd.ack = false;
}

static void smd_toggle() {
	switch (smd.state) {
	case SMD_INACTIVE:
		smd_set_state(SMD_ACTIVATING);
		smd_trigger_reset_binds();
		if (smd_ui_has(TORUI_X) && !smd_ui_has(TORUI_GREED) && !smd_ui_has(TORUI_NEED)) {
			smd_post_io(SMD_MSG_CLICK_UI, TORUI_X, 0);
			smd_post_io(SMD_MSG_CLICK_UI, TORUI_X, 0);
			smd_post_io(SMD_MSG_CLICK_UI, TORUI_X, 0);
		}
		smd_post_toggle(true);
		break;
	default:
		smd_set_state(SMD_INACTIVE);
		smd_trigger_reset_binds();
		smd_auto_loot(0, 0, 0, 0);
		smd_post_toggle(false);
		break;
	}
}

static void smd_active(CURSORINFO* c) {
	if (c->flags & CURSOR_SHOWING) {
		smd_set_state(SMD_ACTIVATING);
		smd.ack = true;
		smd.timeout = GetTickCount() + 100;
	}
}

static void CALLBACK smd_machine(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
	smd_trigger_tick();
	if (smd.state == SMD_INACTIVE)
		return;

	CURSORINFO c;
	c.cbSize = sizeof(CURSORINFO);
	c.flags = 0;
	GetCursorInfo(&c);
	if (smd.state == SMD_ACTIVE)
		smd_active(&c);
	else if (smd.ack && !(c.flags & CURSOR_SHOWING))
		smd_set_state(SMD_ACTIVE);
	else if (dwTime > smd.timeout)
		smd_toggle();
}

////////////////////////////////////////////////////////////////////////////////

static CALLBACK void smd_auto_loot(HWND hwnd, UINT msg, UINT_PTR timer, DWORD millis) {
	torui_e loot = smd.loot.opts[smd.loot.foc];
	smd.loot.foc = smd.loot.def;
	if (smd.loot.ticker)
		KillTimer(0, smd.loot.ticker);
	smd.loot.ticker = 0;
	if (smd.state == SMD_ACTIVE) {
		smd_set_state(SMD_ACTIVATING);
		smd_post_io(SMD_MSG_CLICK_UI, loot, 0);
		smd_post_toggle(true);
	}
}

static void smd_request_loot() {
	if (smd_ui_has(smd.loot.opts[smd.loot.foc])) {
		smd_post_tor(SMD_MSG_LOOT, smd.loot.opts[smd.loot.foc], 0);
		smd.loot.ticker = SetTimer(0, smd.loot.ticker, 2000, smd_auto_loot);
	}
}

static void smd_select_loot() {
	uint8_t* cnt = 0;
	if ((smd.state == SMD_INACTIVE) && (smd.trigger.loot.state == TRIGGER_ONCE))
		cnt = &smd.loot.def;
	else if (smd.state == SMD_ACTIVE)
		cnt = &smd.loot.foc;
	if (!cnt)
		return;
	*cnt += 1;
	if (*cnt >= sizeof(smd.loot.opts)/sizeof(*smd.loot.opts))
		*cnt = 0;
	if (cnt == &smd.loot.foc)
		smd_request_loot();
	else
		log_line("Default loot options set to %s", smd.loot.desc[smd.loot.def]);
}

static void smd_update_ui(MSG* msg) {
	smd.ui = msg->wParam;
	if (smd.state != SMD_ACTIVE || smd.loot.ticker)
		return;
	smd_request_loot();
}

////////////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK smd_key_hook(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode != HC_ACTION)
		return CallNextHookEx(0, nCode, wParam, lParam);
	KBDLLHOOKSTRUCT key;
	memcpy(&key, (void*)lParam, sizeof(key));
	if (key.flags & LLKHF_INJECTED)
		return CallNextHookEx(0, nCode, wParam, lParam);

	trigger_t* t = 0;
	if (smd.toggle == key.scanCode)
		t = &smd.trigger.key;
	else if (key.vkCode == 'Q')
		t = &smd.trigger.click;
	else if (key.vkCode == 'E')
		t = &smd.trigger.loot;
	if (t) {
		if (wParam == WM_KEYUP)
			trigger_release(t);
		else if (!trigger_is_held(t))
			trigger_press(t);
	}
	return CallNextHookEx(0, nCode, wParam, lParam);
}

static bool smd_check_app_window(HWND w) {
	if (w != smd.win)
		smd_post_win_check(w);
	smd.win = w;
	HWND tor = smd.handle.tor.win;
	HWND gui = smd.handle.gui;
	if (!smd.win)
		return false;
	return smd.win == tor || smd.win == gui;
}

static void smd_mod_press(trigger_t* t) {
	trigger_t* lmb = &smd.trigger.lmb;
	trigger_t* rmb = &smd.trigger.rmb;
	if ((t != lmb) && trigger_is_held(lmb) && !smd.mods[SMD_MOD_RMB])
		smd.mods[SMD_MOD_LMB] = true;
	if ((t != rmb) && trigger_is_held(rmb) && !smd.mods[SMD_MOD_LMB])
		smd.mods[SMD_MOD_RMB] = true;
	trigger_press(t);
}

static LRESULT CALLBACK smd_mouse_hook(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode != HC_ACTION)
		return CallNextHookEx(0, nCode, wParam, lParam);

	MSLLHOOKSTRUCT mouse;
	memcpy(&mouse, (void*)lParam, sizeof(mouse));
	HWND w = WindowFromPoint(mouse.pt);
	bool is_app = smd_check_app_window(w);
	if (!is_app && (smd.state == SMD_ACTIVE))
		smd_toggle();
	if (!is_app || (wParam > WM_MOUSEWHEEL) || (mouse.flags & LLMHF_INJECTED))
		return CallNextHookEx(0, nCode, wParam, lParam);

	if (smd.state == SMD_ACTIVATING)
		return !0;

	switch (wParam) {
		case WM_MBUTTONUP:
			trigger_release(&smd.trigger.mid);
			return !smd.toggle ? !0 : CallNextHookEx(0, nCode, wParam, lParam);
		case WM_MBUTTONDOWN:
			if (!trigger_is_held(&smd.trigger.mid))
				trigger_press(&smd.trigger.mid);
			return !smd.toggle ? !0 : CallNextHookEx(0, nCode, wParam, lParam);
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
		smd_mod_press(&smd.trigger.lmb);
		break;
	case WM_RBUTTONUP:
		trigger_release(&smd.trigger.rmb);
		break;
	case WM_RBUTTONDOWN:
		smd_mod_press(&smd.trigger.rmb);
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

static void smd_post_handles() {
	if (!smd.handle.itid || !smd.handle.gui)
		return;

	smd_post_io(SMD_MSG_HANDLE, SMD_HANDLE_GUI_WIN, (LPARAM)smd.handle.gui);
	smd_post_gui(SMD_MSG_HANDLE, SMD_HANDLE_IO_TID, (LPARAM)smd.handle.itid);
}

static void smd_store_handle(MSG* msg) {
	switch (msg->wParam) {
	case SMD_HANDLE_TOR_WIN:
		smd.handle.tor.win = (HWND)msg->lParam;
		break;
	case SMD_HANDLE_TOR_EXE:
		smd.handle.tor.exe = (HANDLE)msg->lParam;
		break;
	case SMD_HANDLE_TOR_TID:
		smd.handle.tor.tid = (DWORD)msg->lParam;
		break;
	case SMD_HANDLE_GUI_WIN:
		smd.handle.gui = (HWND)msg->lParam;
		if (smd.handle.gui)
			smd_post_handles();
		else
			smd_quit();
		break;
	case SMD_HANDLE_IO_TID:
		smd.handle.itid = (DWORD)msg->lParam;
		if (smd.handle.itid)
			smd_post_handles();
		else
			smd_quit();
		break;
	}
}

static void smd_rebind_toggle(MSG* msg) {
	if (msg->wParam != SMD_CFG_MAKE_OPT(SMD_OPT_ACT_KEY))
		return;
	trigger_reset(&smd.trigger.mid);
	trigger_reset(&smd.trigger.key);
	smd.toggle = msg->lParam;
	log_line("%s toggle", smd.toggle ? "Key" : "MMB");
}

////////////////////////////////////////////////////////////////////////////////

static void CALLBACK smd_init(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
	KillTimer(0, idEvent);

	trigger_reset(&smd.trigger.mid);
	smd.trigger.mid.timeout = smd.cfg.delay;
	smd.trigger.rmb.timeout = smd.cfg.delay;
	smd.trigger.lmb.timeout = smd.cfg.delay;
	smd.trigger.fwd.timeout = 0.75*smd.cfg.delay;
	smd.trigger.bwd.timeout = 0.75*smd.cfg.delay;
	smd.trigger.key.timeout = smd.cfg.delay;
	smd.trigger.click.timeout = smd.cfg.delay;
	smd.trigger.loot.timeout = smd.cfg.delay;

	smd.handle.hio = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)smd_io_run, &smd.cfg, 0, 0);
	smd.handle.hui = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)smd_gui_run, &smd.cfg, 0, 0);

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	smd.hook.mouse = SetWindowsHookEx(WH_MOUSE_LL, smd_mouse_hook, 0, 0);
	smd.hook.key = SetWindowsHookEx(WH_KEYBOARD_LL, smd_key_hook, 0, 0);
	smd.ticker = SetTimer(0, 0,  USER_TIMER_MINIMUM, smd_machine);
}

static void smd_deinit() {
	smd_post_io(SMD_MSG_HANDLE, SMD_HANDLE_TID, 0);
	smd_post_gui(SMD_MSG_HANDLE, SMD_HANDLE_TID, 0);
	for (int i = 0; i < 200; ++i) {
		Sleep(10);
		DWORD ret = 0;
		if (smd.handle.hio)
			if (GetExitCodeThread(smd.handle.hio, &ret))
				if (ret == STILL_ACTIVE)
					continue;
		ret = 0;
		if (smd.handle.hui)
			if (GetExitCodeThread(smd.handle.hui, &ret))
				if (ret == STILL_ACTIVE)
					continue;
		break;
	}
	if (smd.hook.mouse)
		UnhookWindowsHookEx(smd.hook.mouse);
	if (smd.hook.key)
		UnhookWindowsHookEx(smd.hook.key);
	if (smd.ticker)
		KillTimer(0, smd.ticker);
}

////////////////////////////////////////////////////////////////////////////////

void smd_quit() {
	PostThreadMessage(smd.cfg.tid, WM_CLOSE, 0, 0);
}

int smd_run(const smd_cfg_t* smd_cfg) {
	log_designate_thread("smd");
	log_line("Run...");

	smd.cfg = *smd_cfg;
	smd.cfg.tid =  GetCurrentThreadId();
	MSG msg;
	msg.wParam = 0;
	if (SetTimer(0, 0,  USER_TIMER_MINIMUM, smd_init)) {
		while (GetMessageA(&msg, 0, 0, 0) > 0) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			switch (msg.message) {
			case SMD_MSG_HANDLE:
				smd_store_handle(&msg);
				break;
			case SMD_MSG_CFG:
				smd_rebind_toggle(&msg);
				break;
			case SMD_MSG_TOGGLE:
			case SMD_MSG_CLICK:
				smd.ack = true;
				break;
			case SMD_MSG_SCAN:
				smd_update_ui(&msg);
				break;
			case WM_CLOSE:
				PostQuitMessage(0);
				break;
			}
		}
	}
	smd_deinit();
	log_line("Exit %i", msg.wParam);
	return msg.wParam;
}

////////////////////////////////////////////////////////////////////////////////

static void smd_left_right_click(trigger_t* t) {
	if (smd.state != SMD_ACTIVE || t->state != TRIGGER_ONCE)
		return;
	smd_set_state(SMD_ACTIVATING);
	smd_post_io(SMD_MSG_CLICK, 0, (LPARAM)smd.win);
}

static void smd_mid_click() {
	if (smd.state == SMD_ACTIVE) {
		smd_post_io(SMD_MSG_MID, 0, 0);
		smd_post_io(SMD_MSG_MID, 0, 0);
	}
	else if (smd.trigger.mid.state == TRIGGER_ONCE) {
		smd_post_io(SMD_MSG_MID, 0, 0);
		smd_post_io(SMD_MSG_MID, 0, 0);
	}
}

static void smd_q_click() {
	smd_left_right_click(&smd.trigger.click);
}

static void smd_press_btn(SmdBind b, SmdMod m) {
	smd.mods[m] = true;
	uint8_t code = smd_gui_bind_code(b, m);
	if (!code)
		return;
	smd_post_io(SMD_MSG_PRESS, 0, code);
	smd_post_io(SMD_MSG_PRESS, 0, code);
}

static void smd_trigger(SmdBind b) {
	if (b < SMD_BIND_LMB) {
		if (trigger_is_held(&smd.trigger.lmb))
			smd_press_btn(b, SMD_MOD_LMB);
		else if (trigger_is_held(&smd.trigger.rmb))
			smd_press_btn(b, SMD_MOD_RMB);
		else
			smd_press_btn(b, SMD_MOD_NON);
	}
	else if (b < SMD_BIND_RMB) {
		if (!smd.mods[SMD_MOD_LMB]) {
			if (!smd.mods[SMD_MOD_RMB]) {
				if (smd.trigger.lmb.state != TRIGGER_PRESSED && smd.trigger.lmb.state != TRIGGER_ONCE)
					smd_press_btn(b, SMD_MOD_NON);
			}
			else {
				smd_press_btn(b, SMD_MOD_RMB);
			}
		}
	}
	else {
		if (!smd.mods[SMD_MOD_RMB]) {
			if (!smd.mods[SMD_MOD_LMB]) {
				if (smd.trigger.rmb.state != TRIGGER_PRESSED && smd.trigger.rmb.state != TRIGGER_ONCE)
					smd_press_btn(b, SMD_MOD_NON);
			}
			else {
				smd_press_btn(b, SMD_MOD_LMB);
			}
		}
	}
	if ((smd.trigger.lmb.state == TRIGGER_IDLE) && (smd.trigger.rmb.state == TRIGGER_IDLE)) {
		smd.mods[SMD_MOD_LMB] = false;
		smd.mods[SMD_MOD_RMB] = false;
	}
}

static void smd_fwd_once() {
	smd_trigger(SMD_BIND_FWD);
}

static void smd_fwd_twice() {
	smd_trigger(SMD_BIND_FWD_DBL);
}

static void smd_bwd_once() {
	smd_trigger(SMD_BIND_BWD);
}

static void smd_bwd_twice() {
	smd_trigger(SMD_BIND_BWD_DBL);
}

static void smd_lmb_once() {
	smd_trigger(SMD_BIND_LMB);
}

static void smd_lmb_twice() {
	smd_trigger(SMD_BIND_LMB_DBL);
}

static void smd_lmb_pressed() {
	smd_trigger(SMD_BIND_LMB_PRS);
}

static void smd_lmb_held() {
	smd_trigger(SMD_BIND_LMB_DBL_PRS);
}

static void smd_rmb_once() {
	smd_trigger(SMD_BIND_RMB);
}

static void smd_rmb_twice() {
	smd_trigger(SMD_BIND_RMB_DBL);
}

static void smd_rmb_pressed() {
	smd_trigger(SMD_BIND_RMB_PRS);
}

static void smd_rmb_held() {
	smd_trigger(SMD_BIND_RMB_DBL_PRS);
}

static void smd_mid_once() {
	if (!smd.toggle)
		smd_toggle();
}

static void smd_key_toggle() {
	if (!smd.toggle || smd.trigger.key.state != TRIGGER_ONCE)
		return;
	smd_toggle();
}

////////////////////////////////////////////////////////////////////////////////

struct smd static smd = {
	.state = SMD_INACTIVE,
	.trigger = {
		.lmb = {
			.cb = {
				.once = smd_lmb_once,
				.twice = smd_lmb_twice,
				.pressed = smd_lmb_pressed,
				.held = smd_lmb_held,
			},
		},
		.mid = {
			.cb = {
				.once = smd_mid_once,
				.pressed = smd_mid_click,
			},
		},
		.rmb = {
			.cb = {
				.once = smd_rmb_once,
				.twice = smd_rmb_twice,
				.pressed = smd_rmb_pressed,
				.held = smd_rmb_held,
			},
		},
		.fwd = {
			.cb = {
				.once = smd_fwd_once,
				.twice = smd_fwd_twice,
			},
		},
		.bwd = {
			.cb = {
				.once = smd_bwd_once,
				.twice = smd_bwd_twice,
			},
		},
		.key = {
			.cb = {
				.pressed = smd_key_toggle,
			},
		},
		.click = {
			.cb = {
				.pressed = smd_q_click,
			},
		},
		.loot = {
			.cb = {
				.pressed = smd_select_loot,
			},
		},
	},
	.loot = {
		.desc = { "Greed", "Need", "Pass" },
		.opts = { TORUI_GREED, TORUI_NEED, TORUI_PASS },
	},
};
