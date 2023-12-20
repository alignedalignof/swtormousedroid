#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>

#include "smd.h"
#include "gui.h"

using namespace Gdiplus;

struct {
	HWND hwnd;
	HDC hdc;
	int x;
	int y;
	TextBox* tip;
	RectGrad* frame;
} static cntx;

////////////////////////////////////////////////////////////////////////////////

static void smd_gui_cntx_request(HWND hwnd) {
	if (cntx.tip->Txt)
	{
		HDC hdc = GetDC(hwnd);
		Graphics gfx(cntx.hdc);
		gfx.SetSmoothingMode(SmoothingModeAntiAlias);
		gfx.SetTextRenderingHint(TextRenderingHintAntiAlias);
		gfx.Clear(Color(0, 0, 0, 0));
		Draw(&gfx, cntx.frame, cntx.frame->W(), cntx.frame->H());

		POINT dst = { cntx.x, cntx.y };
		POINT ptSrc = { 0, 0 };
		BLENDFUNCTION blend = { 0 };
		blend.BlendOp = AC_SRC_OVER;
		blend.BlendFlags = 0;
		blend.SourceConstantAlpha = 255;
		blend.AlphaFormat = AC_SRC_ALPHA;
		SIZE szWnd = { (int)cntx.frame->W() + 1, (int)cntx.frame->H() + 1 };
		UpdateLayeredWindow(hwnd, NULL, &dst, &szWnd, cntx.hdc, &ptSrc, 0, &blend, ULW_ALPHA);
		ShowWindow(hwnd, SW_SHOWNOACTIVATE);
	}
	else
	{
		ShowWindow(hwnd, SW_HIDE);
	}
}

////////////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK smd_gui_cntx_winproc(HWND hwnd, UINT msg, WPARAM wPar, LPARAM lPar) {
	switch (msg)
	{
	case SMD_MSG_CNTX:
		smd_gui_cntx_request(hwnd);
		return 0;
	case WM_NCCALCSIZE:
		return 0;
	default:
		break;
	}
	return DefWindowProc(hwnd, msg, wPar, lPar);
}

////////////////////////////////////////////////////////////////////////////////

void smd_gui_cntx_tip(int x, int y, const wchar_t* txt)
{
	cntx.x = x;
	cntx.y = y;
	cntx.tip->Txt = txt;
	PostMessage(cntx.hwnd, SMD_MSG_CNTX, 0, 0);
}

void smd_gui_cntx_spawn(HWND parent, void* family)
{
	WNDCLASS wclass;
	memset(&wclass, 0, sizeof(wclass));
	wclass.lpfnWndProc = smd_gui_cntx_winproc;
	wclass.hInstance = GetModuleHandle(0);
	wclass.lpszClassName = "cntx";
	wclass.hCursor = LoadCursor(0, IDC_ARROW);
	wclass.style = CS_OWNDC;
	RegisterClass(&wclass);

	cntx.hwnd = CreateWindowEx(WS_EX_NOACTIVATE | WS_EX_LAYERED, wclass.lpszClassName, "", WS_POPUP,
			CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
			parent, 0, wclass.hInstance, 0);

	HDC hdc = GetDC(cntx.hwnd);
	cntx.hdc = CreateCompatibleDC(hdc);
	HBITMAP bmp = CreateCompatibleBitmap(hdc, 1000, 1000);
	SelectObject(cntx.hdc, bmp);

	cntx.tip = new TextBox;
	cntx.tip->Box.X = 5;
	cntx.tip->Box.Y = 5;
	cntx.tip->Fam = (FontFamily*)family;
	cntx.tip->Size = 13;
	cntx.tip->Color = Color(244, 196, 66);
	cntx.tip->Format->SetAlignment(StringAlignmentNear);
	cntx.tip->Format->SetLineAlignment(StringAlignmentCenter);

	cntx.frame = new RectGrad;
	cntx.frame->Box.W = [](){ return cntx.tip->W() + 2*cntx.tip->X(); };
	cntx.frame->Box.H = [](){ return cntx.tip->H() + 2*cntx.tip->Y(); };
	cntx.frame->Colors = { Color{200, 0, 200, 255}, Color{240, 0, 100, 150}, Color{210, 0, 6, 9}};
	cntx.frame->Widths = { 1, 1.5, };
	cntx.frame->R = 5;

	cntx.frame->Add(cntx.tip);
}
