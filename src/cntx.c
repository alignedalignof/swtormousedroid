#include "smd.h"
#include "gui.h"

#include <windows.h>
#include <windowsx.h>

struct {
	HWND hwnd;
	HDC hdc;
	int mx;
	int my;
	int x;
	int y;
	struct {
		const char* txt;
		RECT bb;
	} tip;
	struct {
		SmdBind bind;
		SmdMod mod;
		struct {
			const char* txt;
			RECT bb;
		} items[4];
		int w;
		int h;
	} menu;
} static cntx = {
	.menu = {
		.items = {
			[0] = { "Unbind" },
			[1] = { "Default" },
		},
	},
};

////////////////////////////////////////////////////////////////////////////////
static void smd_gui_cntx_paint(HWND hwnd) {
	RECT r;
	GetWindowRect(hwnd, &r);
	r.right -= r.left;
	r.bottom -= r.top;
	r.left = 5;
	r.top = 5;
	smd_gui_gradient_h(cntx.hdc, 0, 0, SMD_GUI_RGB_VOID, r.right, r.bottom, SMD_GUI_RGB_BK);
	if (cntx.tip.txt) {
		SetTextColor(cntx.hdc, SMD_GUI_RGB_HOT);
		DrawText(cntx.hdc, cntx.tip.txt, -1, &r, DT_LEFT | DT_TOP);
	}
	else if (cntx.menu.bind < SMD_BIND_CNT) {
		for (int i = 0; i < SMD_COUNT(cntx.menu.items); ++i) {
			RECT* bb = &cntx.menu.items[i].bb;
			if (SMD_GUI_IN_RECT(cntx.mx, cntx.my, bb))
				SetTextColor(cntx.hdc, SMD_GUI_RGB_HLINE);
			else
				SetTextColor(cntx.hdc, SMD_GUI_RGB_HOT);
			DrawText(cntx.hdc, cntx.menu.items[i].txt, -1, bb, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
			if (i) {
				int m = bb->right/2;
				smd_gui_gradient_h(cntx.hdc, 0, bb->top, SMD_GUI_RGB_BK, m, bb->top + 1, SMD_GUI_RGB_ULINE);
				smd_gui_gradient_h(cntx.hdc, m, bb->top, SMD_GUI_RGB_ULINE, bb->right, bb->top + 1, SMD_GUI_RGB_BK);
			}
		}
	}

	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	BitBlt(hdc, 0, 0, r.right, r.bottom, cntx.hdc, 0, 0, SRCCOPY);
	EndPaint(hwnd, &ps);
}

static void smd_gui_cntx_mousemove(HWND hwnd, WPARAM wPar, LPARAM lPar) {
	cntx.mx = GET_X_LPARAM(lPar);
	cntx.my = GET_Y_LPARAM(lPar);
	InvalidateRect(hwnd, 0, FALSE);
}

static void smd_gui_cntx_mouseleave(HWND hwnd) {
	cntx.mx = -1;
	cntx.my = -1;
	InvalidateRect(hwnd, 0, FALSE);
}

static void smd_gui_cntx_request(HWND hwnd) {
	if (cntx.tip.txt || cntx.menu.bind < SMD_BIND_CNT) {
		int x = cntx.x;
		int y = cntx.y;
		int cx = cntx.tip.txt ? cntx.tip.bb.right : cntx.menu.w;
		int cy = cntx.tip.txt ? cntx.tip.bb.bottom : cntx.menu.h;
		SetWindowPos(hwnd, HWND_TOP, x, y, cx, cy, SWP_SHOWWINDOW | (cntx.tip.txt ? SWP_NOACTIVATE : 0));
	}
	else {
		ShowWindow(hwnd, SW_HIDE);
	}
}

static void smd_gui_cntx_click(HWND hwnd, WPARAM wPar, LPARAM lPar) {
	if (cntx.menu.bind >= SMD_BIND_CNT)
		return;
	int x = GET_X_LPARAM(lPar);
	int y = GET_Y_LPARAM(lPar);
	for (int i = 0; i < SMD_COUNT(cntx.menu.items); ++i) {
		RECT* bb = &cntx.menu.items[i].bb;
		if (!SMD_GUI_IN_RECT(x, y, bb))
			continue;
		smd_gui_cntx_menu(0, 0, SMD_BIND_CNT, SMD_MOD_CNT);
		PostMessageA(GetParent(cntx.hwnd), WM_COMMAND, i, 0);
		return;
	}
}

////////////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK smd_gui_cntx_winproc(HWND hwnd, UINT msg, WPARAM wPar, LPARAM lPar) {
	switch (msg) {
	case WM_PAINT:
		smd_gui_cntx_paint(hwnd);
		return 0;
	case WM_ERASEBKGND:
		return 0;
	case WM_MOUSEMOVE:
		smd_gui_cntx_mousemove(hwnd, wPar, lPar);
		return 0;
	case WM_MOUSELEAVE:
		smd_gui_cntx_mouseleave(hwnd);
		return 0;
	case SMD_MSG_CNTX:
		smd_gui_cntx_request(hwnd);
		return 0;
	case WM_LBUTTONDOWN:
		smd_gui_cntx_click(hwnd, wPar, lPar);
		return 0;
	case WM_NCCALCSIZE:
		return 0;
	default:
		break;
	}
	return DefWindowProc(hwnd, msg, wPar, lPar);
}

////////////////////////////////////////////////////////////////////////////////

void smd_gui_cntx_tip(int x, int y, const char* txt) {
	cntx.x = x;
	cntx.y = y;
	cntx.tip.txt = txt;
	cntx.menu.bind = SMD_BIND_CNT;
	memset(&cntx.tip.bb, 0, sizeof(cntx.tip.bb));
	if (txt)
		cntx.tip.bb.bottom = DrawText(cntx.hdc, txt, -1, &cntx.tip.bb, DT_CALCRECT);
	cntx.tip.bb.right += 10;
	cntx.tip.bb.bottom += 10;
	PostMessage(cntx.hwnd, SMD_MSG_CNTX, 0, 0);
}

void smd_gui_cntx_menu(int x, int y, SmdBind bind, SmdMod mod) {
	cntx.x = x;
	cntx.y = y;
	cntx.menu.bind = bind;
	cntx.menu.mod = mod;
	cntx.tip.txt = 0;
	PostMessage(cntx.hwnd, SMD_MSG_CNTX, 0, 0);
}

void smd_gui_cntx_spawn(HWND parent) {
	WNDCLASS wclass;
	memset(&wclass, 0, sizeof(wclass));
	wclass.lpfnWndProc = smd_gui_cntx_winproc;
	wclass.hInstance = GetModuleHandle(0);
	wclass.lpszClassName = "cntx";
	wclass.hCursor = LoadCursor(0, IDC_ARROW);
	wclass.style = CS_DROPSHADOW | CS_OWNDC;
	RegisterClass(&wclass);

	cntx.hwnd = CreateWindowEx(WS_EX_NOACTIVATE, wclass.lpszClassName, "", WS_POPUP,
			CW_USEDEFAULT, CW_USEDEFAULT, 300, 300,
			parent, 0, GetModuleHandle(0), 0);

	HDC hdc = GetDC(cntx.hwnd);
	cntx.hdc = CreateCompatibleDC(hdc);
	HBITMAP bmp = CreateCompatibleBitmap(hdc, SMD_GUI_W, SMD_GUI_H);
	ReleaseDC(cntx.hwnd, hdc);

	SelectObject(cntx.hdc, bmp);
	SelectObject(cntx.hdc, GetStockObject(DC_PEN));
	SelectObject(cntx.hdc, GetStockObject(DC_BRUSH));
	SetTextColor(cntx.hdc, SMD_GUI_RGB_HLINE);
	SetTextAlign(cntx.hdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);
	SetBkMode(cntx.hdc, TRANSPARENT);

	for (int i = 0; i < SMD_COUNT(cntx.menu.items); ++i) {
		RECT bb;
		memset(&bb, 0, sizeof(bb));
		int h = DrawText(cntx.hdc, cntx.menu.items[i].txt, -1, &bb, DT_CALCRECT);
		if (bb.right > cntx.menu.w)
			cntx.menu.w = bb.right;
		if (h > cntx.menu.h)
			cntx.menu.h = h;
	}
	cntx.menu.w += 2*SMD_GUI_OFS_ULINE;
	cntx.menu.h *= 1.5;
	for (int i = 0; i < SMD_COUNT(cntx.menu.items); ++i) {
		cntx.menu.items[i].bb.left = SMD_GUI_OFS_ULINE;
		cntx.menu.items[i].bb.right = cntx.menu.w;
		cntx.menu.items[i].bb.top = i*cntx.menu.h;
		cntx.menu.items[i].bb.bottom = (i + 1)*cntx.menu.h;
	}
	cntx.menu.h *= SMD_COUNT(cntx.menu.items);
}
