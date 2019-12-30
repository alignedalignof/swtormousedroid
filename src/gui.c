#include "gui.h"
#include "log.h"

#include "smd.h"

#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
	int l;
	int t;
	int r;
	int b;
	bool mouse;
} Bb;

typedef struct {
	const char* name;
	bool extended;
	uint8_t code;
	Bb bb;
	bool selected;
	const char* help;
} Bind;

typedef struct Opt {
	const char* name;
	const char* help;
	Bb bb;
	bool selected;
	bool hidden;
} Opt;

struct Gui {
	Bind binds[SMD_MOD_BIND_CNT];
	Bind toggler;
	Opt opts[SMD_OPT_CNT];
	Bb minx[SMD_GUI_SYS_CNT];
	struct {
		Bb bar;
		Bb bb;
		int y;
	} scroll;

	struct {
		int x;
		int y;
		int type;
		void* over;
		const char* tip;
		UINT_PTR timer;
	} m;

	Bind* focus;

	DWORD tidio;
	smd_cfg_t smd;

	struct {
		Bind* bind;
		HWND target;
		int x;
		int y;
	} test;

	struct {
		HRGN all;
		HRGN binds;
	} rgn;
	HWND main;
	HDC hdc;
};

static struct Gui gui;

////////////////////////////////////////////////////////////////////////////////

static void smd_gui_post_smd(UINT msg, WPARAM wPar, LPARAM lPar) {
	if (gui.smd.tid)
		PostThreadMessage(gui.smd.tid, msg, wPar, lPar);
}

static void smd_gui_post_io(UINT msg, WPARAM wPar, LPARAM lPar) {
	if (gui.tidio)
		PostThreadMessage(gui.tidio, msg, wPar, lPar);
}

static const char* smd_gui_title() {
	return gui.smd.elevated ? SMD_GUI_TXT_TITLE : SMD_GUI_TXT_TITLE_GUI;
}

////////////////////////////////////////////////////////////////////////////////

static CALLBACK void smd_gui_hover(HWND hwnd, UINT msg, UINT_PTR timer, DWORD millis) {
	const int OFS = 15;

	POINT p;
	if (timer) {
		KillTimer(0, timer);
		GetCursorPos(&p);
	}
	gui.m.timer = 0;
	smd_gui_cntx_tip(p.x + OFS, p.y + OFS, gui.m.tip);
}

static void smd_gui_request_tip(const char* tip) {
	if (gui.m.tip == tip)
		return;
	gui.m.tip = gui.opts[SMD_OPT_TIPS].selected ? tip : 0;
	if (gui.m.tip)
		gui.m.timer = SetTimer(0, gui.m.timer, 300, smd_gui_hover);
	else
		smd_gui_hover(0, 0, gui.m.timer, 0);
}

static void smd_gui_request_tip_bind(Bind* bind) {
	smd_gui_request_tip(bind->help);
}

static void smd_gui_request_tip_opt(Opt* opt) {
	if (!opt->hidden)
		smd_gui_request_tip(opt->help);
}

////////////////////////////////////////////////////////////////////////////////

static void smd_gui_col_bb(Bb* bb) {
	int x = gui.m.x;
	int y = gui.m.y;
	bb->mouse = (x >= bb->l) && (x < bb->r) && (y >= bb->t) && (y < bb->b);
}

static void smd_gui_col_bind(Bind* bind, Bb* mask) {
	smd_gui_col_bb(&bind->bb);
	if (mask)
		bind->bb.mouse = bind->bb.mouse && mask->mouse;
	if (bind->bb.mouse)
		gui.m.over = bind;
}

static void smd_gui_col_opt(Opt* opt) {
	smd_gui_col_bb(&opt->bb);
	if (opt->bb.mouse)
		gui.m.over = opt;
}

static void smd_gui_col_xmin(int xmin) {
	smd_gui_col_bb(&gui.minx[xmin]);
	if (gui.minx[xmin].mouse)
		gui.m.over = &gui.minx[xmin];
}

static void smd_gui_col() {
	if (gui.main == GetCapture())
		return;

	void* over = gui.m.over;
	gui.m.over = 0;

	smd_gui_col_xmin(SMD_GUI_MINX_MIN);
	smd_gui_col_xmin(SMD_GUI_MINX_X);
	smd_gui_col_bb(&gui.scroll.bb);
	for (int i = 0; i < SMD_OPT_CNT; ++i)
		smd_gui_col_opt(&gui.opts[i]);
	for (int i = 0; i < SMD_MOD_BIND_CNT; ++i)
		smd_gui_col_bind(&gui.binds[i], &gui.scroll.bb);
	smd_gui_col_bind(&gui.toggler, 0);
	if (gui.opts[SMD_OPT_EXT_BINDS].selected)
		smd_gui_col_bb(&gui.scroll.bar);
	else
		gui.scroll.bar.mouse = false;
	if (gui.scroll.bar.mouse)
		gui.m.over = &gui.scroll;

	if (over == gui.m.over)
		return;

	smd_gui_request_tip(0);
	over = gui.m.over;
	if (!over)
		smd_gui_request_tip(0);
	else if (over <= (void*)&gui.toggler)
		smd_gui_request_tip_bind((Bind*)over);
	else if (over <= (void*)&gui.opts[SMD_OPT_CNT])
		smd_gui_request_tip_opt((Opt*)over);
	else
		smd_gui_request_tip(0);
	InvalidateRect(gui.main, 0, FALSE);
}

////////////////////////////////////////////////////////////////////////////////

static void smd_gui_calc_mod_bind_bb(Bind* bind, SmdMod m, int y) {
	struct {
		int x;
		int y;
	} ANCHORS[] = {
		{ 70, 2*SMD_GUI_OFS_SPACE + SMD_GUI_H_BINDER },
		{ 70 + SMD_GUI_W/2, SMD_GUI_OFS_SPACE },
		{ 70 + SMD_GUI_W/2, 2*SMD_GUI_OFS_SPACE + SMD_GUI_H_BINDER },
	};

	Bind* mod = bind + m*SMD_BIND_CNT;
	if (!mod->help ||
		(bind->extended && !gui.opts[SMD_OPT_EXT_BINDS].selected) ||
		((m != SMD_MOD_NON) && !gui.opts[SMD_OPT_MOD_BINDS].selected)) {
		mod->bb.r = mod->bb.l;
		return;
	}
	mod->bb.l = ANCHORS[m].x;
	mod->bb.t = ANCHORS[m].y + y;
	mod->bb.r = mod->bb.l + SMD_GUI_W_BINDER;
	mod->bb.b = mod->bb.t + SMD_GUI_H_BINDER;
}

static void smd_gui_calc_key_bbs() {
	int y = SMD_GUI_H_TITLE - gui.scroll.y;
	for (int i = 0; i < SMD_BIND_CNT; ++i) {
		Bind* bind = &gui.binds[i];
		smd_gui_calc_mod_bind_bb(bind, SMD_MOD_NON, y);
		smd_gui_calc_mod_bind_bb(bind, SMD_MOD_LMB, y);
		smd_gui_calc_mod_bind_bb(bind, SMD_MOD_RMB, y);
		if (!bind->extended || gui.opts[SMD_OPT_EXT_BINDS].selected)
			y += SMD_GUI_H_BIND;
	}

	gui.scroll.bar.r = SMD_GUI_W - SMD_GUI_W_BORDER_L;
	gui.scroll.bar.l = gui.scroll.bar.r - SMD_GUI_W_TOGL;
	gui.scroll.bar.t = SMD_GUI_H_TITLE + gui.scroll.y/2;
	gui.scroll.bar.b = gui.scroll.bar.t + SMD_GUI_H_BINDS/2;

	Bb* opt = &gui.opts[SMD_OPT_ACT_KEY].bb;
	gui.toggler.bb.l = opt->r + 50;
	gui.toggler.bb.t = opt->t;
	gui.toggler.bb.r = gui.toggler.bb.l + (gui.opts[SMD_OPT_ACT_KEY].selected ? SMD_GUI_W_BINDER : 0);
	gui.toggler.bb.b = opt->b;

	smd_gui_col();
}

static void smd_gui_scroll(int dy) {
	if (!gui.opts[SMD_OPT_EXT_BINDS].selected)
		return;
	gui.scroll.y += dy;
	if (gui.scroll.y < 0)
		gui.scroll.y = 0;
	else if (gui.scroll.y >= SMD_GUI_H_BINDS)
		gui.scroll.y = SMD_GUI_H_BINDS;
	smd_gui_calc_key_bbs();
	InvalidateRect(gui.main, 0, FALSE);
}

////////////////////////////////////////////////////////////////////////////////

static void smd_gui_post_toggler() {
	LPARAM code = gui.opts[SMD_OPT_ACT_KEY].selected ? gui.toggler.code : 0;
	smd_gui_post_smd(SMD_MSG_CFG, SMD_CFG_MAKE_OPT(SMD_OPT_ACT_KEY), code);
	smd_gui_calc_key_bbs();
	InvalidateRect(gui.main, 0, FALSE);
}

static void smd_gui_set_focus(Bind* bind) {
	gui.focus = (gui.focus == bind) ? 0 : bind;
}

static void smd_gui_start_drag() {
	smd_gui_set_focus(0);
	SetCursor(LoadCursor(0, IDC_SIZEALL));
}

static void smd_gui_opt(Opt* opt) {
	opt->selected  = !opt->selected;
	int t = opt - gui.opts;
	switch (t) {
	case SMD_OPT_EXT_BINDS:
		if (gui.focus && gui.focus->extended)
			smd_gui_set_focus(0);
		gui.scroll.y = 0;
		smd_gui_calc_key_bbs();
		break;
	case SMD_OPT_MOD_BINDS:
		if (gui.focus >= &gui.binds[SMD_BIND_CNT])
			smd_gui_set_focus(0);
		smd_gui_calc_key_bbs();
		break;
	case SMD_OPT_CONSOLE:
		if (opt->selected)
			ShowWindow(GetConsoleWindow(), SW_SHOW);
		else
			ShowWindow(GetConsoleWindow(), SW_HIDE);
		break;
	case SMD_OPT_ACT_SMD:
		if (gui.focus == &gui.toggler)
			smd_gui_set_focus(0);
		opt->selected = true;
		gui.opts[SMD_OPT_ACT_KEY].selected = false;
		smd_gui_post_toggler();
		break;
	case SMD_OPT_ACT_KEY:
		opt->selected = true;
		gui.opts[SMD_OPT_ACT_SMD].selected = false;
		smd_gui_post_toggler();
		break;
	case SMD_OPT_IDK:
	case SMD_OPT_NO_U:
		smd_gui_post_io(SMD_MSG_CFG, SMD_CFG_MAKE_OPT(t), opt->selected);
		break;
	}
	InvalidateRect(gui.main, 0, FALSE);
}

static void smd_gui_mouse_move(WPARAM wPar, LPARAM lPar) {
	int x = GET_X_LPARAM(lPar);
	int y = GET_Y_LPARAM(lPar);

	if (gui.test.target == gui.main || (!gui.m.over && (wPar & MK_LBUTTON))) {
		if (x != gui.m.x || y != gui.m.y) {
			RECT r;
			GetWindowRect(gui.main, &r);
			SetWindowPos(gui.main, 0, r.left + x - gui.m.x, r.top + y - gui.m.y, 0, 0, SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOSIZE | SWP_NOSENDCHANGING | SWP_DEFERERASE);
		}
		return;
	}

	int dy = y - gui.m.y;
	gui.m.x = x;
	gui.m.y = y;
	if (gui.main == GetCapture() && gui.m.over == &gui.scroll)
		smd_gui_scroll(2*dy);
	smd_gui_col();

	TRACKMOUSEEVENT hover;
	hover.cbSize = sizeof(TRACKMOUSEEVENT);
	hover.dwFlags = TME_LEAVE;
	hover.hwndTrack = gui.main;
	hover.dwHoverTime = 0;
	TrackMouseEvent(&hover);
	InvalidateRect(gui.main, 0, FALSE);
}

static void smd_gui_mouse_lmb_dn() {
	void* over = gui.m.over;
	SetCapture(gui.main);
	if (!over)
		smd_gui_start_drag();
	else if (over <= (void*)&gui.toggler)
		smd_gui_set_focus((Bind*)over);
	else if (over < (void*)&gui.opts[SMD_OPT_CNT])
		smd_gui_opt((Opt*)over);
	else if (over == (void*)&gui.minx[SMD_GUI_MINX_X])
		smd_gui_close();
	else if (over == (void*)&gui.minx[SMD_GUI_MINX_MIN])
		ShowWindow(gui.main, SW_MINIMIZE);
	else
		smd_gui_set_focus(0);
	InvalidateRect(gui.main, 0, FALSE);
}

static void smd_gui_mouse_rmb_dn() {
	smd_gui_cntx_tip(0, 0, 0);
	void* over = gui.m.over;
	if (!over || over > (void*)&gui.toggler)
		return;
	Bind* bind = (Bind*)over;
	bind->code = 0;
	gui.focus = 0;
	if (bind == &gui.toggler)
		smd_gui_post_toggler();
}

////////////////////////////////////////////////////////////////////////////////

static bool smd_gui_test_bind(Bind* bind, LPARAM lPar) {
	if (!bind->help || !bind->code)
		return false;
	if (bind->code != SMD_LPAR_TO_CODE(lPar))
		return false;
	gui.test.bind = bind;
	return true;
}

static void smd_gui_test(LPARAM lPar) {
	gui.test.bind = 0;
	for (int b = 0; b < SMD_MOD_BIND_CNT; ++b)
		if (smd_gui_test_bind(&gui.binds[b], lPar))
			return;
}

////////////////////////////////////////////////////////////////////////////////

static void smd_gui_paint_key(Bind* bind) {
	if (bind->bb.mouse || bind == gui.focus)
		SetDCPenColor(gui.hdc, SMD_GUI_RGB_HLINE);
	else
		SetDCPenColor(gui.hdc, SMD_GUI_RGB_OLINE);
	SetDCBrushColor(gui.hdc, SMD_GUI_RGB_VOID);
	RoundRect(gui.hdc, bind->bb.l, bind->bb.t, bind->bb.r, bind->bb.b, 8, 8);

	RECT r = { bind->bb.l, bind->bb.t, bind->bb.r, bind->bb.b, };
	if (bind == gui.focus) {
		SetTextColor(gui.hdc, SMD_GUI_RGB_HOT);
		DrawText(gui.hdc, "...", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}
	else if (bind->code) {
		SetTextColor(gui.hdc, SMD_GUI_RGB_HLINE);
		char name[50];
		if (GetKeyNameText(SMD_CODE_TO_LPAR(bind->code), name, sizeof(name)))
			DrawText(gui.hdc, name, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}
	else {
		SetTextColor(gui.hdc, SMD_GUI_RGB_X);
		DrawText(gui.hdc, "Not bound", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}
}

static void smd_gui_paint_bind(Bind* bind, int y) {
	if (!bind->help)
		return;

	RECT r = { SMD_GUI_OFS_ULINE, y + 10, SMD_GUI_H, SMD_GUI_W };
	if (bind < &gui.binds[SMD_BIND_CNT]) {
		SetTextColor(gui.hdc, SMD_GUI_RGB_HLINE);
		DrawText(gui.hdc, bind->name, -1, &r, DT_TOP | DT_LEFT);
	}

	const char* iLlGeTmYmAiN = &"Bind:\x00+LMB:\x00+RMB:\x00"[6*((bind - gui.binds)/SMD_BIND_CNT)];
	r.left =  0, r.top = bind->bb.t, r.right =  bind->bb.l - 10, r.bottom = bind->bb.b;
	SetTextColor(gui.hdc, SMD_GUI_RGB_HOT);
	DrawText(gui.hdc, iLlGeTmYmAiN, -1, &r, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
	smd_gui_paint_key(bind);
}

static void smd_gui_paint_bind_mods(Bind* bind, int y) {
	SetBkMode(gui.hdc, TRANSPARENT);
	smd_gui_paint_bind(bind, y);
	if (gui.opts[SMD_OPT_MOD_BINDS].selected) {
		smd_gui_paint_bind(bind + SMD_BIND_CNT, y);
		smd_gui_paint_bind(bind + 2*SMD_BIND_CNT, y);
	}
	y = y + SMD_GUI_H_BIND - 1;
	int x = SMD_GUI_W_FRAME - SMD_GUI_OFS_ULINE;
	SetDCPenColor(gui.hdc, SMD_GUI_RGB_ULINE);
	smd_gui_line(gui.hdc, SMD_GUI_OFS_ULINE, y, x, y);
	smd_gui_gradient_h(gui.hdc, x, y, SMD_GUI_RGB_ULINE, x + SMD_GUI_OFS_ULINE, y + 1, SMD_GUI_RGB_BK);
}

static void smd_gui_paint_binds() {
	SelectClipRgn(gui.hdc, gui.rgn.binds);
	SelectObject(gui.hdc, GetStockObject(DC_PEN));
	int y = SMD_GUI_H_TITLE - gui.scroll.y;
	for (int i = 0; i < SMD_BIND_CNT; ++i) {
		Bind* bind = &gui.binds[i];
		if (bind->extended && !gui.opts[SMD_OPT_EXT_BINDS].selected)
			continue;
		if (y > -SMD_GUI_H_BIND)
			smd_gui_paint_bind_mods(bind, y);
		y += SMD_GUI_H_BIND;
		if (y >= gui.scroll.bb.b)
			break;
	}
}

static void smd_gui_paint_scroll() {
	if (!gui.opts[SMD_OPT_EXT_BINDS].selected)
		return;

	COLORREF rgb = gui.scroll.bar.mouse ? SMD_GUI_RGB_HOT : SMD_GUI_RGB_TTGL;
	int l = gui.scroll.bar.mouse ? gui.scroll.bar.l : gui.scroll.bar.l + SMD_GUI_W_TOGL/2;
	if (gui.main == GetCapture() && gui.scroll.bar.mouse) {
		l = gui.scroll.bar.l;
		rgb = SMD_GUI_RGB_BTGL;
	}
	SetDCBrushColor(gui.hdc, rgb);
	SetDCPenColor(gui.hdc, rgb);
	Rectangle(gui.hdc, l, gui.scroll.bar.t, gui.scroll.bar.r, gui.scroll.bar.b);
}

////////////////////////////////////////////////////////////////////////////////

static void smd_gui_paint_toggle(Opt* o) {
	if (o->hidden)
		return;

	if (o->bb.mouse)
		SetDCPenColor(gui.hdc, SMD_GUI_RGB_HLINE);
	else
		SetDCPenColor(gui.hdc, SMD_GUI_RGB_OLINE);
	SetDCBrushColor(gui.hdc, SMD_GUI_RGB_VOID);
	if (o < &gui.opts[SMD_OPT_ACT_SMD])
		RoundRect(gui.hdc, o->bb.l, o->bb.t, o->bb.r, o->bb.b, 6, 6);
	else
		Ellipse(gui.hdc, o->bb.l, o->bb.t, o->bb.r, o->bb.b);

	RECT r = { o->bb.l + 1.5*SMD_GUI_W_TOGL, o->bb.t, SMD_GUI_W, o->bb.b };
	DrawText(gui.hdc, o->name, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

	if (o == &gui.opts[SMD_OPT_ACT_KEY] && o->selected)
		smd_gui_paint_key(&gui.toggler);

	if (!o->selected)
		return;
	int x = (o->bb.l + o->bb.r)/2;
	int y = (o->bb.t + o->bb.b)/2;
	r.left = x - 7, r.top = y - 7, 	r.right = x + 7, r.bottom = y + 7;
	if (o >= &gui.opts[SMD_OPT_ACT_SMD]) {
		HRGN radio = CreateEllipticRgn(r.left, r.top, r.right + 1, r.bottom + 1);
		SelectClipRgn(gui.hdc, radio);
		smd_gui_gradient_v(gui.hdc, r.left, r.top, SMD_GUI_RGB_TTGL, r.right, r.bottom, SMD_GUI_RGB_BTGL);
		SelectClipRgn(gui.hdc, gui.rgn.all);
		DeleteObject(radio);
	}
	else {
		smd_gui_gradient_v(gui.hdc, r.left, r.top, SMD_GUI_RGB_TTGL, r.right, r.bottom, SMD_GUI_RGB_BTGL);
	}
}

////////////////////////////////////////////////////////////////////////////////

static void smd_gui_paint_xmin(int xmin) {
	Bb* bb = &gui.minx[xmin];
	if (bb->mouse) {
		COLORREF rgb = (xmin == SMD_GUI_MINX_X) ? SMD_GUI_RGB_X : SMD_GUI_RGB_MIN;
		SetDCPenColor(gui.hdc, rgb);
		SetDCBrushColor(gui.hdc, rgb);
		Rectangle(gui.hdc, bb->l, bb->t, bb->r, bb->b);
	}
	SetDCPenColor(gui.hdc, SMD_GUI_RGB_HLINE);
	int mx = (bb->l + bb->r)/2;
	int my = (bb->t + bb->b)/2;
	if (xmin == SMD_GUI_MINX_X) {
		smd_gui_line(gui.hdc, mx - 5, my -5, mx + 6, my + 6);
		smd_gui_line(gui.hdc, mx - 5, my + 5, mx + 6, my - 6);
	}
	else if (xmin == SMD_GUI_MINX_MIN) {
		smd_gui_line(gui.hdc, mx - 5, my, mx + 5, my);
	}
}

static void smd_gui_paint_frame() {
	SelectClipRgn(gui.hdc, gui.rgn.all);
	smd_gui_gradient_h(gui.hdc, 0, 0, SMD_GUI_RGB_ULINE, SMD_GUI_W, SMD_GUI_H, SMD_GUI_RGB_OLINE);

	RECT r = { 10, 0, SMD_GUI_W, SMD_GUI_H_TITLE, };
	SetBkMode(gui.hdc, TRANSPARENT);
	SetTextColor(gui.hdc, SMD_GUI_RGB_HLINE);
	DrawText(gui.hdc, smd_gui_title(), -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

	r.top = SMD_GUI_H_TITLE + SMD_GUI_H_BINDS;
	r.bottom = r.top + SMD_GUI_H_SBTTL;
	if (gui.test.target == gui.main) {
		SetTextColor(gui.hdc, SMD_GUI_RGB_SMD);
		char title[128] = SMD_GUI_TXT_SMD_TST;
		if (gui.test.bind) {
			strcat(title, gui.test.bind->name);
			strcat(title, " -> ");
			int l = strlen(title);
			GetKeyNameText(SMD_CODE_TO_LPAR(gui.test.bind->code), title + l, sizeof(title) - l);
		}
		DrawText(gui.hdc, title, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	}
	else if (gui.test.target) {
		SetTextColor(gui.hdc, SMD_GUI_RGB_SMD);
		DrawText(gui.hdc, SMD_GUI_TXT_SMD_ACT, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	}
	else {
		SetTextColor(gui.hdc, SMD_GUI_RGB_HLINE);
		DrawText(gui.hdc, SMD_GUI_TXT_SMD, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	}

	SetTextColor(gui.hdc, SMD_GUI_RGB_HLINE);
	r.top = r.bottom + SMD_GUI_H_ACTV;
	r.bottom = r.top + SMD_GUI_H_SBTTL;
	DrawText(gui.hdc, "Options", -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

	smd_gui_paint_xmin(SMD_GUI_MINX_MIN);
	smd_gui_paint_xmin(SMD_GUI_MINX_X);

	SetDCBrushColor(gui.hdc, SMD_GUI_RGB_BK);
	SetDCPenColor(gui.hdc, SMD_GUI_RGB_BK);
	Rectangle(gui.hdc, gui.scroll.bb.l, gui.scroll.bb.t, gui.scroll.bb.r, gui.scroll.bb.b);
	int y = gui.scroll.bb.b + SMD_GUI_H_SBTTL;
	Rectangle(gui.hdc, gui.scroll.bb.l, y, gui.scroll.bb.r, y + SMD_GUI_H_ACTV);
	y += SMD_GUI_H_ACTV + SMD_GUI_H_SBTTL;
	Rectangle(gui.hdc, gui.scroll.bb.l, y, gui.scroll.bb.r, y + SMD_GUI_H_XTRA);

	SelectObject(gui.hdc, GetStockObject(DC_PEN));
}

static void smd_gui_paint_cross() {
	SetDCPenColor(gui.hdc, RGB(255, 255, 255));
	SetDCBrushColor(gui.hdc, RGB(255, 255, 255));
	int x = gui.test.x;
	int y = gui.test.y;
	Rectangle(gui.hdc, x - 1, y - 1, x + 2, y + 2);
}

////////////////////////////////////////////////////////////////////////////////

static void smd_gui_paint(HWND hwnd) {

	smd_gui_paint_frame();
	for (Opt* o = gui.opts; o < &gui.opts[SMD_OPT_CNT]; ++o)
		smd_gui_paint_toggle(o);

	smd_gui_paint_binds();
	smd_gui_paint_scroll();
	if (gui.test.target == gui.main)
		smd_gui_paint_cross();

	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	BitBlt(hdc, 0, 0, SMD_GUI_W, SMD_GUI_H, gui.hdc, 0, 0, SRCCOPY);
	EndPaint(hwnd, &ps);
}

static void smd_gui_gradient(HDC hdc, int x0, int y0, COLORREF c0, int x1, int y1, COLORREF c1, ULONG mode) {
	TRIVERTEX bb[] = {{}, {}};
	bb[0].x = x0, bb[0].y = y0;
	bb[0].Red = GetRValue(c0)*256, bb[0].Green = GetGValue(c0)*256, bb[0].Blue = GetBValue(c0)*256;
	bb[1].x = x1, bb[1].y = y1;
	bb[1].Red = GetRValue(c1)*256, bb[1].Green = GetGValue(c1)*256, bb[1].Blue = GetBValue(c1)*256;
	GRADIENT_RECT grad;
	grad.UpperLeft = 0;
	grad.LowerRight = 1;
	GradientFill(hdc, bb, 2, &grad, 1, mode);
}

////////////////////////////////////////////////////////////////////////////////

#define GUI_FILE SMD_FOLDER"gui.cfg"
#define GUI_MAGIC 0xdeadc0d4

typedef struct  __attribute__((packed)) { //same PC only
	uint32_t magic;
	uint32_t options;
	uint8_t codes[SMD_MOD_BIND_CNT];
	uint8_t key;
} GuiBin;

static void smd_gui_load_settings() {
	for (int b = 0; b < SMD_MOD_BIND_CNT; ++b)
		gui.binds[b].code = MapVirtualKeyA(gui.binds[b].code, MAPVK_VK_TO_VSC);

	HANDLE f = CreateFileA(GUI_FILE, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (f == INVALID_HANDLE_VALUE) {
		log_line("Error %x while opening user settings", GetLastError());
	}
	else {
		GuiBin bin = { .magic = 0 };
		DWORD len = sizeof(bin);
		if (ReadFile(f, &bin, len, &len, 0) && (len == sizeof(bin)) && (bin.magic == GUI_MAGIC)) {
			for (int i = 0; i < SMD_MOD_BIND_CNT; ++i)
				gui.binds[i].code = bin.codes[i];
			gui.toggler.code = bin.key;

			for (int i = 0; i < SMD_OPT_CNT; ++i)
				gui.opts[i].selected = (bin.options & (1U << i));
		}
		CloseHandle(f);
	}
}

static void smd_gui_save_settings() {
	GuiBin bin;
	memset(&bin, 0, sizeof(bin));
	bin.magic = GUI_MAGIC;

	for (int b = 0; b < SMD_MOD_BIND_CNT; ++b)
		bin.codes[b] = gui.binds[b].code;
	bin.key = gui.toggler.code;

	bin.options = 0;
	for (int o = 0; o < SMD_OPT_CNT; ++o)
		bin.options |= gui.opts[o].selected ? (1U << o) : 0;

	HANDLE f = CreateFileA(GUI_FILE, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (f != INVALID_HANDLE_VALUE) {
		DWORD len;
		WriteFile(f, &bin, sizeof(bin), &len, 0);
		CloseHandle(f);
	}
	else {
		log_line("Error %x while saving user settings", GetLastError());
	}
}

////////////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK gui_main(HWND hwnd, UINT msg, WPARAM wPar, LPARAM lPar) {
	switch (msg) {
	case WM_PAINT:
		smd_gui_paint(hwnd);
		return 0;
	case WM_ERASEBKGND:
		return 0;
	case WM_MOUSEMOVE:
		smd_gui_mouse_move(wPar, lPar);
		return 0;
	case WM_MOUSELEAVE:
		gui.m.x = -1;
		gui.m.y = -1;
		while (ShowCursor(TRUE) < 0)
			;
		smd_gui_col();
		break;
	case WM_LBUTTONDOWN:
		smd_gui_mouse_lmb_dn();
		break;
	case WM_RBUTTONDOWN:
		smd_gui_mouse_rmb_dn();
		break;
	case WM_LBUTTONUP:
		if (gui.test.target != gui.main)
			ReleaseCapture();
		break;
	case WM_CAPTURECHANGED:
		InvalidateRect(hwnd, 0, FALSE);
		break;
	case WM_MOUSEWHEEL:
		smd_gui_scroll(GET_WHEEL_DELTA_WPARAM(wPar)/-3);
		return 0;
	case WM_SYSKEYUP:
	case WM_KEYUP:
		if (gui.test.target == gui.main) {
			smd_gui_test(lPar);
			InvalidateRect(hwnd, 0, FALSE);
			break;
		}
		if (!gui.focus || wPar == VK_MENU || wPar == VK_SHIFT || wPar == VK_CONTROL)
			break;
		gui.focus->code = SMD_LPAR_TO_CODE(lPar);
		if (gui.focus == &gui.toggler)
			smd_gui_post_toggler();
		gui.focus = 0;
		break;
	case WM_NCCALCSIZE:
		return 0;
	case WM_DESTROY:
		smd_gui_save_settings();
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hwnd, msg, wPar, lPar);
}

////////////////////////////////////////////////////////////////////////////////

static void CALLBACK smd_gui_init(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
	KillTimer(0, idEvent);
	smd_gui_load_settings();

	WNDCLASS wclass;
	memset(&wclass, 0, sizeof(wclass));
	wclass.lpfnWndProc = gui_main;
	wclass.hInstance = GetModuleHandle(0);
	wclass.lpszClassName = "gui";
	wclass.hCursor = LoadCursor(0, IDC_ARROW);
	wclass.style = CS_DROPSHADOW | CS_OWNDC;
	RegisterClass(&wclass);

	gui.main = CreateWindowEx(0, wclass.lpszClassName, smd_gui_title(), WS_POPUP,
		CW_USEDEFAULT, CW_USEDEFAULT, SMD_GUI_W, SMD_GUI_H,
		0, 0, GetModuleHandle(0), 0);

	HDC hdc = GetDC(gui.main);
	gui.hdc = CreateCompatibleDC(hdc);
	SelectObject(gui.hdc, CreateCompatibleBitmap(hdc, SMD_GUI_W, SMD_GUI_H));
	ReleaseDC(gui.main, hdc);

	gui.rgn.all = CreateRectRgn(0, 0, SMD_GUI_W, SMD_GUI_H);
	gui.rgn.binds = CreateRectRgn(gui.scroll.bb.l, gui.scroll.bb.t, gui.scroll.bb.r, gui.scroll.bb.b);

	SelectObject(gui.hdc, GetStockObject(DC_PEN));
	SelectObject(gui.hdc, GetStockObject(DC_BRUSH));
	SetTextAlign(gui.hdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);

	int x = SMD_GUI_OFS_ULINE;
	int y = SMD_GUI_OFS_OPTS_V;
	for (int i = 0; i < SMD_OPT_CNT; ++i) {
		Opt* o = &gui.opts[i];
		o->hidden = o->hidden && !gui.smd.secrets;
		if (i < SMD_OPT_ACT_SMD) {
			o->bb.l = x + (i/3)*0.5*SMD_GUI_W;
			o->bb.t = y + (i%3)*1.5*SMD_GUI_W_TOGL;
		}
		else {
			o->bb.l = x + (i/7)*0.5*SMD_GUI_W;
			o->bb.t = SMD_GUI_OFS_ACTV + SMD_GUI_H_SBTTL + (SMD_GUI_H_ACTV - SMD_GUI_W_TOGL)/2;
		}
		o->bb.r = o->bb.l + SMD_GUI_W_TOGL;
		o->bb.b = o->bb.t + SMD_GUI_W_TOGL;
	}
	if (gui.smd.secrets && gui.opts[SMD_OPT_CONSOLE].selected)
		ShowWindow(GetConsoleWindow(), SW_SHOW);
	smd_gui_cntx_spawn(gui.main);
	smd_gui_calc_key_bbs();
	ShowWindow(gui.main, SW_SHOW);

	smd_gui_post_smd(SMD_MSG_HANDLE, SMD_HANDLE_GUI_WIN, (LPARAM)gui.main);
}

static void smd_gui_store_handle(MSG* msg) {
	switch (msg->wParam) {
	case SMD_HANDLE_IO_TID:
		gui.tidio = (DWORD)msg->lParam;
		smd_gui_post_toggler();
		break;
	case SMD_HANDLE_TID:
		gui.smd.tid = (DWORD)msg->lParam;
		if (!gui.smd.tid)
			smd_gui_close();
		else
			smd_gui_post_toggler();
		break;
	}
}

static void smd_gui_toggle(MSG* msg) {
	bool active = msg->wParam;
	gui.test.target = (HWND)msg->lParam;
	gui.test.bind = 0;
	if (!gui.test.target) {
		while (ShowCursor(TRUE) < 0)
			;
		return;
	}
	else {
		if (gui.test.target == gui.main) {
			if (active)
				while (ShowCursor(FALSE) >= 0)
					;
			else
				while (ShowCursor(TRUE) < 0)
					;
		}
		if (!active)
			gui.test.target = 0;
	}
	InvalidateRect(gui.main, 0, FALSE);
}

////////////////////////////////////////////////////////////////////////////////

void smd_gui_line(HDC hdc, int x0, int y0, int x1, int y1) {
	MoveToEx(hdc, x0, y0, 0);
	LineTo(hdc, x1, y1);
}

void smd_gui_gradient_h(HDC hdc, int x0, int y0, COLORREF c0, int x1, int y1, COLORREF c1) {
	smd_gui_gradient(hdc, x0, y0, c0, x1, y1, c1, GRADIENT_FILL_RECT_H);
}

void smd_gui_gradient_v(HDC hdc, int x0, int y0, COLORREF c0, int x1, int y1, COLORREF c1) {
	smd_gui_gradient(hdc, x0, y0, c0, x1, y1, c1, GRADIENT_FILL_RECT_V);
}

void smd_gui_close() {
	PostMessageA(gui.main, WM_CLOSE, 0, 0);
}

const char* smd_gui_bind_name(int b) {
	return (b < SMD_MOD_BIND_CNT) ? gui.binds[b].name : 0;
}

int smd_gui_bind_code(SmdBind b, SmdMod m) {
	Bind* bind = &gui.binds[SMD_BIND_IX(b, m)];
	Bind* fwd = &gui.binds[SMD_BIND_IX(SMD_BIND_FWD_DBL, m)];
	Bind* bwd = &gui.binds[SMD_BIND_IX(SMD_BIND_BWD_DBL, m)];
	bool ext = gui.opts[SMD_OPT_EXT_BINDS].selected;
	bool mod = gui.opts[SMD_OPT_MOD_BINDS].selected;

	if (!bind->help)
		return 0;
	if (b == SMD_BIND_FWD && !ext)
		return (m == SMD_MOD_NON || mod) ? fwd->code : 0;
	if (b == SMD_BIND_BWD && !ext)
		return (m == SMD_MOD_NON || mod) ? bwd->code : 0;
	if (bind->extended && !ext)
		return 0;
	if (bind >= &gui.binds[SMD_BIND_CNT] && !mod)
		return 0;
	return bind->code;
}

DWORD WINAPI smd_gui_run(LPVOID arg) {
	log_designate_thread("gui");
	log_line("Run...");

	memcpy(&gui.smd, arg, sizeof(gui.smd));
	MSG msg;
	if (SetTimer(0, 0,  USER_TIMER_MINIMUM, smd_gui_init)) {
		while (GetMessageA(&msg, 0, 0, 0) > 0) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			switch (msg.message) {
			case SMD_MSG_HANDLE:
				smd_gui_store_handle(&msg);
				break;
			case SMD_MSG_TOGGLE:
				smd_gui_toggle(&msg);
				break;
			case SMD_MSG_CROSS:
				gui.test.x = (int16_t)msg.lParam;
				gui.test.y = (int16_t)(msg.lParam >> 16);
				break;
			}
		}
	}
	log_line("Exit %i", msg.wParam);
	smd_gui_post_smd(SMD_MSG_HANDLE, SMD_HANDLE_GUI_WIN, 0);
	return msg.wParam;
}

////////////////////////////////////////////////////////////////////////////////

static struct Gui gui = {
	.binds = {
		//MMB
		[SMD_BIND_FWD] = {
			.name = "Notch Forward",
			.help = "Scroll mouse wheel forward one notch\n"SMD_GUI_TXT_TIP,
			.extended = true,
		},
		[SMD_BIND_FWD_DBL] = {
			.name = "Scroll Forward",
			.help = SMD_GUI_TXT_TIP,
			.code = '1',
		},
		[SMD_BIND_BWD] = {
			.name = "Notch Backward",
			.help = "Scroll mouse wheel backward one notch\n"SMD_GUI_TXT_TIP,
			.extended = true,
		},
		[SMD_BIND_BWD_DBL] = {
			.name = "Scroll Backward",
			.help = SMD_GUI_TXT_TIP,
			.code = '2',
		},
		//LMB
		[SMD_BIND_LMB] = {
			.name = "Left Click",
			.help = SMD_GUI_TXT_TIP,
			.code = '3',
		},
		[SMD_BIND_LMB_DBL] = {
			.name = "Double Left Click",
			.help = SMD_GUI_TXT_TIP,
			.code = '4',
		},
		[SMD_BIND_LMB_PRS] = {
			.name = "Press Left Click",
			.help = "Press and release left mouse button\n"SMD_GUI_TXT_TIP,
			.extended = true,
		},
		[SMD_BIND_LMB_DBL_PRS] = {
			.name = "Double Left Click Hold",
			.help = "Double click-hold left mouse button\n"SMD_GUI_TXT_TIP,
			.extended = true,
		},
		//RMB
		[SMD_BIND_RMB] = {
			.name = "Right Click",
			.help = SMD_GUI_TXT_TIP,
			.code = '5',
		},
		[SMD_BIND_RMB_DBL] = {
			.name = "Double Right Click",
			.help = SMD_GUI_TXT_TIP,
			.code = '6',
		},
		[SMD_BIND_RMB_PRS] = {
			.name = "Press Right Click",
			.help = "Press and release right mouse button\n"SMD_GUI_TXT_TIP,
			.extended = true,
		},
		[SMD_BIND_RMB_DBL_PRS] = {
			.name = "Double Right Click Hold",
			.help = "Double click-hold right mouse button\n"SMD_GUI_TXT_TIP,
			.extended = true,
		},

		//LMB + MMB
		[SMD_BIND_IX(SMD_BIND_FWD, SMD_MOD_LMB)] = {
			.name = "LMB + Notch Forward",
			.help = "Hold left mouse while scrolling forward one notch\n"SMD_GUI_TXT_TIP,
			.extended = true,
		},
		[SMD_BIND_IX(SMD_BIND_FWD_DBL, SMD_MOD_LMB)] = {
			.name = "LMB + Scroll Forward",
			.help = "Hold left mouse while scrolling forward\n"SMD_GUI_TXT_TIP,
		},
		[SMD_BIND_IX(SMD_BIND_BWD, SMD_MOD_LMB)] = {
			.name = "LMB + Notch Backward",
			.help = "Hold left mouse while scrolling backward one notch\n"SMD_GUI_TXT_TIP,
			.extended = true,
		},
		[SMD_BIND_IX(SMD_BIND_BWD_DBL, SMD_MOD_LMB)] = {
			.name = "LMB + Scroll Backward",
			.help = "Hold left mouse while scrolling backward\n"SMD_GUI_TXT_TIP,
		},
		//LMB + RMB
		[SMD_BIND_IX(SMD_BIND_RMB, SMD_MOD_LMB)] = {
			.name = "LMB + Right Click",
			.help = "Hold left mouse while clicking right mouse button\n"SMD_GUI_TXT_TIP,
		},
		[SMD_BIND_IX(SMD_BIND_RMB_DBL, SMD_MOD_LMB)] = {
			.name = "LMB + Double Right Click",
			.help = "Hold left mouse while double clicking right mouse button\n"SMD_GUI_TXT_TIP,
		},
		[SMD_BIND_IX(SMD_BIND_RMB_PRS, SMD_MOD_LMB)] = {
			.name = "LMB + Right Click Press",
			.help = "Hold left mouse while pressing right mouse button\n"SMD_GUI_TXT_TIP,
			.extended = true,
		},
		[SMD_BIND_IX(SMD_BIND_RMB_DBL_PRS, SMD_MOD_LMB)] = {
			.name = "LMB + Double Right Click Hold",
			.help = "idk mang\ni bound combat rezz to this\n"SMD_GUI_TXT_TIP,
			.extended = true,
		},

		//RMB + MMB
		[SMD_BIND_IX(SMD_BIND_FWD, SMD_MOD_RMB)] = {
			.name = "RMB + Notch Forward",
			.help = "Hold right mouse while scrolling forward one notch\n"SMD_GUI_TXT_TIP,
			.extended = true,
		},
		[SMD_BIND_IX(SMD_BIND_FWD_DBL, SMD_MOD_RMB)] = {
			.name = "RMB + Scroll Forward",
			.help = "Hold right mouse while scrolling forward\n"SMD_GUI_TXT_TIP,
		},
		[SMD_BIND_IX(SMD_BIND_BWD, SMD_MOD_RMB)] = {
			.name = "RMB + Notch Backward",
			.help = "Hold right mouse while scrolling backward one notch\n"SMD_GUI_TXT_TIP,
			.extended = true,
		},
		[SMD_BIND_IX(SMD_BIND_BWD_DBL, SMD_MOD_RMB)] = {
			.name = "RMB + Scroll Backward",
			.help = "Hold right mouse while scrolling backward\n"SMD_GUI_TXT_TIP,
		},
		//RMB + LMB
		[SMD_BIND_IX(SMD_BIND_LMB, SMD_MOD_RMB)] = {
			.name = "RMB + Left Click",
			.help = "Hold right mouse while clicking left mouse button\n"SMD_GUI_TXT_TIP,
		},
		[SMD_BIND_IX(SMD_BIND_LMB_DBL, SMD_MOD_RMB)] = {
			.name = "RMB + Double Left Click",
			.help = "Hold right mouse while double clicking left mouse button\n"SMD_GUI_TXT_TIP,
		},
		[SMD_BIND_IX(SMD_BIND_LMB_PRS, SMD_MOD_RMB)] = {
			.name = "RMB + Left Click Press",
			.help = "Hold right mouse while pressing left mouse button\n"SMD_GUI_TXT_TIP,
			.extended = true,
		},
		[SMD_BIND_IX(SMD_BIND_LMB_DBL_PRS, SMD_MOD_RMB)] = {
			.name = "RMB + Double Left Click Hold",
			.help = "this isnt even my final form\n"SMD_GUI_TXT_TIP,
			.extended = true,
		},
	},
	.opts = {
		[SMD_OPT_EXT_BINDS] = {
			.name = "Extended Binds",
			.help = "Enable binds that utilize\nmouse presses",
		},
		[SMD_OPT_MOD_BINDS] = {
			.name = "+LMB/RMB Modifier Binds",
			.help = "Enable binds that utilize\nLMB/RMB \"modifiers\"",
		},
		[SMD_OPT_TIPS] = {
			.name = "Enable tooltips",
			.help = "Show tooltips when hovering\nover elements",
			.selected = true,
		},
		[SMD_OPT_IDK] = {
			.name = "UI assist",
			.help = "maybe next decade",
			.hidden = true,
			.selected = true,
		},
		[SMD_OPT_CONSOLE] = {
			.name = "Console",
			.help = "Midnight Abyss Zone",
			.hidden = true,
			.selected = true,
		},
		[SMD_OPT_NO_U] = {
			.name = "no u",
			.help = "feed companion",
			.hidden = true,
		},
		[SMD_OPT_ACT_SMD] = {
			.name = "Middle click",
			.help = "Toggle mouselook mode with a middle click\nHold middle click to emit clicks\nUse within SWToR or current UI to test",
			.selected = true,
		},
		[SMD_OPT_ACT_KEY] = {
			.name = "Key",
			.help = "Toggle mouselook mode with a key\nUse within SWToR or SMD",
		},
	},
	.minx = {
		[SMD_GUI_MINX_MIN] = {
			.l = SMD_GUI_W - 2*SMD_GUI_W_XMIN_BB,
			.t = 0,
			.r = SMD_GUI_W - SMD_GUI_W_XMIN_BB,
			.b = SMD_GUI_H_TITLE,
		},
		[SMD_GUI_MINX_X] = {
			.l = SMD_GUI_W - SMD_GUI_W_XMIN_BB,
			.t = 0,
			.r = SMD_GUI_W,
			.b = SMD_GUI_H_TITLE,
		},
	},
	.scroll = {
		.bb = {
			.l = SMD_GUI_W_BORDER_L,
			.t = SMD_GUI_H_TITLE,
			.r = SMD_GUI_W_BORDER_L + SMD_GUI_W_FRAME,
			.b = SMD_GUI_H_TITLE + SMD_GUI_H_BINDS,
		},
	},
	.toggler = {
		.help = "Select key to toggle mouselook mode"
	},
};
