#include <stdbool.h>
#include <stdint.h>

#include "log.h"
#include "trigger.h"
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
		trigger_t mmb;
		trigger_t rmb;
		trigger_t fwd;
		trigger_t bwd;
		trigger_t rgt;
		trigger_t lft;
		trigger_t mx1;
		trigger_t mx2;
		trigger_t key;
		trigger_t clk;
	} trigger;
	WPARAM modder[SMD_MB_CNT];
	WPARAM modee[9];
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
	LPARAM key_toggle;
	bool ack;
	int last_scroll;
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

////////////////////////////////////////////////////////////////////////////////

static void smd_trigger_tick()
{
	for (trigger_t* t = (trigger_t*)&smd.trigger; t <= &smd.trigger.clk; ++t)
		trigger_tick(t);
}

static void smd_trigger(SmdBind b);
static void smd_lmb_pressed() { smd_trigger(SMD_BIND_LMB_PRS); }
static void smd_rmb_pressed() { smd_trigger(SMD_BIND_RMB_PRS); }

static void smd_trigger_reset_binds()
{
	for (trigger_t* t = (trigger_t*)&smd.trigger; t <= &smd.trigger.mx2; ++t)
	if (t != &smd.trigger.mmb)
		trigger_reset(t);
	smd_post_io(SMD_MSG_MOD, 7, 0);
	memset(&smd.modder, 0, sizeof(smd.modder));
	memset(&smd.modee, 0, sizeof(smd.modee));
	smd.trigger.lmb.cb.press = smd_gui_bind_code(SMD_BIND_LMB_PRS) ? smd_lmb_pressed : NULL;
	smd.trigger.rmb.cb.press = smd_gui_bind_code(SMD_BIND_RMB_PRS) ? smd_rmb_pressed : NULL;
}

////////////////////////////////////////////////////////////////////////////////

static void smd_set_state(smd_state_e state)
{
	int timeouts[] = { 0, 500, 1000, };
	smd.timeout = GetTickCount() + timeouts[state];
	smd.state = state;
	if (state == SMD_ACTIVATING) smd.ack = false;
}

static void smd_toggle()
{
	switch (smd.state)
	{
	case SMD_INACTIVE:
		smd_set_state(SMD_ACTIVATING);
		smd_trigger_reset_binds();
		smd_post_toggle(true);
		break;
	default:
		smd_set_state(SMD_INACTIVE);
		smd_trigger_reset_binds();
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
	{
		smd.key_toggle = smd_gui_get_key_toggle();
		return;
	}

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

static LRESULT CALLBACK smd_key_hook(int nCode, WPARAM wParam, LPARAM lParam)
{
	bool is_app = smd.win == smd.handle.tor.win || smd.win == smd.handle.gui;
	if (nCode != HC_ACTION || !is_app) return CallNextHookEx(0, nCode, wParam, lParam);

	KBDLLHOOKSTRUCT key;
	memcpy(&key, (void*)lParam, sizeof(key));

	if (smd.key_toggle)
	if (smd.key_toggle != key.scanCode)
	if (wParam == WM_KEYUP)
	if (key.vkCode != 'W' && key.vkCode != 'A' && key.vkCode != 'S' && key.vkCode != 'D' && key.vkCode != ' ')
	if (smd.trigger.key.state != TRIGGER_IDLE)
	{
		smd.trigger.key.state = TRIGGER_DHOLD;
	}

	if (key.flags & LLKHF_INJECTED) return CallNextHookEx(0, nCode, wParam, lParam);

	trigger_t* t = 0;
	if (smd.key_toggle == key.scanCode) t = &smd.trigger.key;
	else if (key.vkCode == 'Q') t = &smd.trigger.clk;

	if (t)
	{
		if (wParam == WM_KEYUP)
			trigger_release(t);
		else if (!trigger_is_held(t))
			trigger_press(t);
	}
	return (smd.state != SMD_INACTIVE && key.vkCode == 'Q') ? !0 : CallNextHookEx(0, nCode, wParam, lParam);
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

static void smd_trigger_press(trigger_t* t)
{
	int mb = t - &smd.trigger.lmb;
	trigger_press(t);
	for (int i = 0; i < 3; i++)
	{
		if (i != mb)
		if (trigger_is_held(&smd.trigger.lmb + i))
		{
			smd.modder[i] = smd_gui_get_mod((SmdMb)i);
			smd.modee[mb] |= smd.modder[i] << (i*8);
		}
	}
}

static void smd_trigger_release(trigger_t* t)
{
	int mb = t - &smd.trigger.lmb;
	trigger_release(t);
	if (mb < 3 && smd.modder[mb])
	{
		smd.modder[mb] = 0;
		trigger_reset(t);
	}
}

static LRESULT CALLBACK smd_mouse_hook(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode != HC_ACTION)
		return CallNextHookEx(0, nCode, wParam, lParam);

	MSLLHOOKSTRUCT mouse;
	memcpy(&mouse, (void*)lParam, sizeof(mouse));
	HWND w = WindowFromPoint(mouse.pt);
	bool is_app = smd_check_app_window(w);
	if (!is_app)
	{
		if (smd.state == SMD_ACTIVE) smd_toggle();
		return CallNextHookEx(0, nCode, wParam, lParam);
	}

	if (smd.state == SMD_INACTIVE)
	if (smd.key_toggle)
	if (wParam != WM_MOUSEMOVE)
	if (smd.trigger.key.state != TRIGGER_IDLE) smd.trigger.key.state = TRIGGER_DHOLD;

	if (mouse.flags & LLMHF_INJECTED)
		return CallNextHookEx(0, nCode, wParam, lParam);
	if (wParam == WM_MOUSEMOVE && smd.state == SMD_ACTIVATING)
		return !0;

	switch (wParam) {
		case WM_MBUTTONUP:
			smd_trigger_release(&smd.trigger.mmb);
			return !0;
		case WM_MBUTTONDOWN:
			if (!trigger_is_held(&smd.trigger.mmb))
				smd_trigger_press(&smd.trigger.mmb);
			return !0;
	}

	if (smd.state == SMD_INACTIVE) return CallNextHookEx(0, nCode, wParam, lParam);

	switch (wParam) {
	case WM_LBUTTONUP:
		smd_trigger_release(&smd.trigger.lmb);
		return !0;
	case WM_LBUTTONDOWN:
		smd_trigger_press(&smd.trigger.lmb);
		return !0;
	case WM_RBUTTONUP:
		smd_trigger_release(&smd.trigger.rmb);
		return !0;
	case WM_RBUTTONDOWN:
		smd_trigger_press(&smd.trigger.rmb);
		return !0;
	case WM_MOUSEWHEEL:
	{
		smd.last_scroll = GET_WHEEL_DELTA_WPARAM(mouse.mouseData);
		trigger_t* wheel = (smd.last_scroll < 0) ? &smd.trigger.bwd : &smd.trigger.fwd;
		smd_trigger_press(wheel);
		smd_trigger_release(wheel);
		return !0;
	}
	case WM_MOUSEHWHEEL:
	{
		smd.last_scroll = GET_WHEEL_DELTA_WPARAM(mouse.mouseData);
		trigger_t* wheel = (smd.last_scroll > 0) ? &smd.trigger.rgt : &smd.trigger.lft;
		smd_trigger_press(wheel);
		smd_trigger_release(wheel);
		return !0;
	}
	case WM_XBUTTONDOWN:
	{
		int btn = GET_XBUTTON_WPARAM(mouse.mouseData);
		trigger_t* mx = (btn == XBUTTON1) ? &smd.trigger.mx1 : (btn == XBUTTON2) ? &smd.trigger.mx2 : NULL;
		if (!mx) break;

		smd_trigger_press(mx);
		smd_trigger_release(mx);
		return !0;
	}
	}
	return CallNextHookEx(0, nCode, wParam, lParam);
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

////////////////////////////////////////////////////////////////////////////////

static void CALLBACK smd_init(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
	KillTimer(0, idEvent);

	trigger_reset(&smd.trigger.mmb);
	smd.trigger.mmb.window.tap = smd.cfg.delay;
	smd.trigger.fwd.window.dtap = 0.75*smd.cfg.delay;
	smd.trigger.bwd.window.dtap = 0.75*smd.cfg.delay;
	smd.trigger.lft.window.dtap = 0.75*smd.cfg.delay;
	smd.trigger.rgt.window.dtap = 0.75*smd.cfg.delay;

	smd.trigger.lmb.window.tap = (133*smd.cfg.delay)/200;
	smd.trigger.lmb.window.press = (116*smd.cfg.delay)/200;
	smd.trigger.lmb.window.dtap = smd.cfg.delay;
	smd.trigger.lmb.window.repeat = 2*smd.cfg.delay;

	smd.trigger.rmb.window.tap = (133*smd.cfg.delay)/200;
	smd.trigger.rmb.window.press = (116*smd.cfg.delay)/200;
	smd.trigger.rmb.window.dtap = smd.cfg.delay;
	smd.trigger.rmb.window.repeat = 2*smd.cfg.delay;

	smd.trigger.key.window.tap = smd.cfg.delay;
	smd.trigger.clk.window.tap = smd.cfg.delay;

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
			case SMD_MSG_TOGGLE:
			case SMD_MSG_CLICK:
				smd.ack = true;
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

static void smd_click(int btns)
{
	if (smd.state == SMD_ACTIVE)
	{
		smd_set_state(SMD_ACTIVATING);
		smd_post_io(SMD_MSG_CLICK, btns, (LPARAM)smd.win);
	}
}

static void smd_press_btn(SmdBind b, bool mid, int mx, WPARAM allmods)
{
	WPARAM mods = ((allmods >> 16) & 0xff)  | ((allmods >> 8) & 0xff) | (allmods & 0xff);
	allmods <<= 8;
	if (smd.cfg.secrets) log_line("FIRE b %i %u %i %x", b, mid, mx, mods);
	if (mid)
	{
		smd_post_io(SMD_MSG_MOD, 8 | mods, 0);
		smd_post_io(SMD_MSG_MID, 0, 0);
		smd_post_io(SMD_MSG_MID, 0, 0);
		smd_post_io(SMD_MSG_MOD, mods, 0);
		smd_post_gui(SMD_MSG_BIND, allmods | SMD_BIND_CNT, 0);
	}
	else if (mx)
	{
		smd_post_io(SMD_MSG_MOD, 8 | mods, 0);
		smd_post_io(SMD_MSG_MX, mx, 0);
		smd_post_io(SMD_MSG_MX, mx, 0);
		smd_post_io(SMD_MSG_MOD, mods, 0);
		smd_post_gui(SMD_MSG_BIND, allmods | SMD_BIND_CNT, 0);
	}
	else
	{
		int code = smd_gui_bind_code(b);
		if (!code)
		{
			smd_post_gui(SMD_MSG_BIND, b, 0);
			if (b >= SMD_BIND_FWD && b <= SMD_BIND_BWD_DBL) smd_post_io(SMD_MSG_SCROLL, 0, smd.last_scroll);
			if (b == SMD_BIND_LFT || b == SMD_BIND_RGT) smd_post_io(SMD_MSG_SCROLL, 1, smd.last_scroll);
			if (b == SMD_BIND_LMB || b == SMD_BIND_LMB_DBL || b == SMD_BIND_LMB_HLD || b == SMD_BIND_LMB_DBL_HLD) smd_click(1);
			if (b == SMD_BIND_RMB || b == SMD_BIND_RMB_DBL || b == SMD_BIND_RMB_HLD || b == SMD_BIND_RMB_DBL_HLD) smd_click(2);
			return;
		}
		smd_post_io(SMD_MSG_MOD, 8 | mods, 0);
		smd_post_io(SMD_MSG_PRESS, 0, code);
		smd_post_io(SMD_MSG_PRESS, 0, code);
		smd_post_io(SMD_MSG_MOD, mods, 0);
		smd_post_gui(SMD_MSG_BIND, allmods | b, 0);
	}
}

static void smd_trigger_ex(SmdBind b, bool mid, int mx)
{
	trigger_t* modee = NULL;
	if (b >= SMD_BIND_LMB && b <= SMD_BIND_LMB_DBL_HLD) modee = &smd.trigger.lmb;
	if (mid) modee = &smd.trigger.mmb;
	if (b >= SMD_BIND_RMB && b <= SMD_BIND_RMB_DBL_HLD) modee = &smd.trigger.rmb;
	if (b == SMD_BIND_FWD || b == SMD_BIND_FWD_DBL) modee = &smd.trigger.fwd;
	if (b == SMD_BIND_BWD || b == SMD_BIND_BWD_DBL) modee = &smd.trigger.bwd;
	if (b == SMD_BIND_LFT) modee = &smd.trigger.lft;
	if (b == SMD_BIND_RGT) modee = &smd.trigger.rgt;
	if (mx == XBUTTON1) modee = &smd.trigger.mx1;
	if (mx == XBUTTON2) modee = &smd.trigger.mx2;
	if (!modee) return; //how pathetic

	int i = modee - &smd.trigger.lmb;
	if (i < SMD_MB_CNT)
	if (smd.modder[i] || (modee->state == TRIGGER_HOLD && smd_gui_is_hold_click_delayed((SmdMb)i)))
	{
		return;
	}
	smd_press_btn(b, mid, mx, smd.modee[i]);
	smd.modee[i] = 0;
}

static void smd_trigger(SmdBind b) { smd_trigger_ex(b, false, 0); }

static void smd_q_click()
{
	if (smd.trigger.clk.state == TRIGGER_HOLD)
	{
		smd.trigger.clk.state = TRIGGER_DHOLD;
		smd_click(3);
	}
}

static void smd_q_q()
{
	if (smd.state == SMD_INACTIVE) return;
	static WORD q_scan;
	q_scan = q_scan ? q_scan : MapVirtualKeyA('Q', MAPVK_VK_TO_VSC);
	smd_post_io(SMD_MSG_PRESS, 0, q_scan);
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

static void smd_right_scroll_once() {
	smd_trigger(SMD_BIND_RGT);
}

static void smd_left_scroll_once() {
	smd_trigger(SMD_BIND_LFT);
}

static void smd_mx1_once() {
	smd_trigger_ex(SMD_BIND_CNT, false, XBUTTON1);
}

static void smd_mx2_once() {
	smd_trigger_ex(SMD_BIND_CNT, false, XBUTTON2);
}

static void smd_lmb_once() {
	smd_trigger(SMD_BIND_LMB);
}

static void smd_lmb_twice() {
	smd_trigger(SMD_BIND_LMB_DBL);
}

static void smd_lmb_held() {
	smd_trigger(SMD_BIND_LMB_HLD);
}

static void smd_lmb_dhold() {
	smd_trigger(SMD_BIND_LMB_DBL_HLD);
}

static void smd_rmb_once() {
	smd_trigger(SMD_BIND_RMB);
}

static void smd_rmb_twice() {
	smd_trigger(SMD_BIND_RMB_DBL);
}

static void smd_rmb_held() {
	smd_trigger(SMD_BIND_RMB_HLD);
}

static void smd_rmb_dhold() {
	smd_trigger(SMD_BIND_RMB_DBL_HLD);
}

static void smd_mid_click() {
	smd_trigger_ex(SMD_BIND_CNT, true, 0);
}

static void smd_mid_tap()
{
	if (smd.key_toggle)
		smd_mid_click();
	else
		smd_toggle();
}

static void smd_mid_hold()
{
	smd_mid_click();
}

static void smd_key_toggle()
{
	if (smd.key_toggle)
	if (smd.trigger.key.state != TRIGGER_HOLD)
		smd_toggle();
}

////////////////////////////////////////////////////////////////////////////////

struct smd static smd = {
	.state = SMD_INACTIVE,
	.trigger = {
		.lmb = {
			.cb = {
				.tap = smd_lmb_once,
				.dtap = smd_lmb_twice,
				.hold = smd_lmb_held,
				.dhold = smd_lmb_dhold,
			},
		},
		.mmb = {
			.cb = {
				.tap = smd_mid_tap,
				.hold = smd_mid_hold,
			},
		},
		.rmb = {
			.cb = {
				.tap = smd_rmb_once,
				.dtap = smd_rmb_twice,
				.hold = smd_rmb_held,
				.dhold = smd_rmb_dhold,
			},
		},
		.fwd = {
			.cb = {
				.tap = smd_fwd_once,
				.dtap = smd_fwd_twice,
			},
		},
		.bwd = {
			.cb = {
				.tap = smd_bwd_once,
				.dtap = smd_bwd_twice,
			},
		},
		.rgt = {
			.cb = {
				.tap = smd_right_scroll_once,
			},
		},
		.lft = {
			.cb = {
				.tap = smd_left_scroll_once,
			},
		},
		.mx1 = {
			.cb = {
				.tap = smd_mx1_once,
			},
		},
		.mx2 = {
			.cb = {
				.tap = smd_mx2_once,
			},
		},
		.key = {
			.cb = {
				.tap = smd_key_toggle,
				.hold = smd_key_toggle,
			},
		},
		.clk = {
			.cb = {
				.tap = smd_q_q,
				.hold = smd_q_click,
			},
		},
	},
};
