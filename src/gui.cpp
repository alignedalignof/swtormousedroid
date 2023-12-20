#include "log.h"

#include "smd.h"

#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <chrono>

#include "gui.h"

using namespace Gdiplus;
using namespace std;
using namespace chrono;

#define TIP_BIND L"Click to bind a key\nClick again to unbind"
#define TIP_CLICK TIP_BIND
#define TIP_DCLICK TIP_BIND
#define TIP_HOLD_CLICK L"Hold mouse button down\nWill only fire on release if modifiers are configured\n" TIP_BIND
#define TIP_DHOLD_CLICK L"Double click-hold mouse button\n" TIP_BIND
#define TIP_NOTCH L"Scroll one notch\n" TIP_BIND
#define TIP_SCROLL TIP_BIND

struct {
	DWORD tidio;
	smd_cfg_t smd;
	HWND main;
	HDC hdc;
	HBITMAP bmp;
	RectGrad* frame;
	LRESULT area;
	PrivateFontCollection* fonts;
	FontFamily* fam;
	uint8_t codes[SMD_BIND_CNT];
	uint8_t mods[SMD_MB_CNT];
	int key_toggle;
	struct {
		HWND target;
		int x;
		int y;
	} test;
	struct
	{
		int rand = -1;
		Image* images[4];
	} toobig;
	struct
	{
		int last = -1;
		bool mod[SMD_MB_CNT];
	} binds[3];
	struct
	{
		UINT_PTR timer;
		const wchar_t* txt;
	} tip;
	struct
	{
		int left = CW_USEDEFAULT;
		int top = CW_USEDEFAULT;
		int width = 470;
		int height = 860;
		wchar_t title[50] = L"SWToR Mouse Droid";
	} win;
} static _gui;

///////////////////////////////////////////////////////////////////////////////

static CALLBACK void _gui_hover(HWND hwnd, UINT msg, UINT_PTR timer, DWORD millis)
{
	const int OFS = 15;

	POINT p;
	if (timer)
	{
		KillTimer(0, timer);
		GetCursorPos(&p);
	}
	_gui.tip.timer = 0;
	smd_gui_cntx_tip(p.x + OFS, p.y + OFS, _gui.tip.txt);
}

static void _gui_request_tip(const wchar_t* tip) {
	if (_gui.tip.txt == tip)
		return;
	_gui.tip.txt = tip;
	if (_gui.tip.txt)
		_gui.tip.timer = SetTimer(0, _gui.tip.timer, 300, _gui_hover);
	else
		_gui_hover(0, 0, _gui.tip.timer, 0);
}

static void _gui_redraw(bool resizing = false, int dx = 0, int dy = 0)
{
	Graphics gfx(_gui.hdc);
	gfx.SetSmoothingMode(SmoothingModeAntiAlias);
	gfx.SetTextRenderingHint(TextRenderingHintAntiAlias);
	gfx.Clear(Color(0, 0, 0, 0));
	if (dx || dy)
	{

	}
	else if (resizing && _gui.frame->W() > 1000)
	{
		_gui.toobig.rand = (_gui.toobig.rand < 0) ? rand() % 4: _gui.toobig.rand;
		gfx.DrawImage(_gui.toobig.images[_gui.toobig.rand], 0.f, 0.f, _gui.frame->W(), _gui.frame->H());
	}
	else
	{
		_gui.toobig.rand = -1;
		Draw(&gfx, _gui.frame, _gui.frame->W(), _gui.frame->H());
	}
	if (_gui.test.target == _gui.main)
	{
		SolidBrush white{Color{255, 255, 255}};
		gfx.FillRectangle(&white, _gui.test.x - 1, _gui.test.y - 1, 3, 3);
	}
	POINT ptSrc = { 0, 0 };
	BLENDFUNCTION blend = { 0 };
	blend.BlendOp = AC_SRC_OVER;
	blend.BlendFlags = 0;
	blend.SourceConstantAlpha = 255;
	blend.AlphaFormat = AC_SRC_ALPHA;
	if (dx || dy)
	{
		RECT r;
		GetWindowRect(_gui.main, &r);
		POINT dst = {  r.left + dx, r.top + dy };
		UpdateLayeredWindow(_gui.main, NULL, &dst, NULL, NULL, NULL, 0, &blend, ULW_ALPHA);
	}
	else
	{
		SIZE sz = { (int)_gui.frame->W(), (int)_gui.frame->H() };
		UpdateLayeredWindow(_gui.main, NULL, NULL, &sz, _gui.hdc, &ptSrc, 0, &blend, ULW_ALPHA);
	}
}

static void smd_gui_store_handle(MSG* msg) {
	switch (msg->wParam) {
	case SMD_HANDLE_IO_TID:
		_gui.tidio = (DWORD)msg->lParam;
		break;
	case SMD_HANDLE_TID:
		_gui.smd.tid = (DWORD)msg->lParam;
		if (!_gui.smd.tid)
			PostQuitMessage(0);
		break;
	}
}

static void _gui_post_smd(UINT msg, WPARAM wPar, LPARAM lPar)
{
	if (_gui.smd.tid) PostThreadMessage(_gui.smd.tid, msg, wPar, lPar);
}


static LRESULT CALLBACK _gui_main(HWND hwnd, UINT msg, WPARAM wPar, LPARAM lPar)
{
	if (GetCapture() != _gui.main) gFocus = nullptr;

	switch (msg) {
	case WM_MOUSEMOVE:
	{
		int x = GET_X_LPARAM(lPar);
		int y = GET_Y_LPARAM(lPar);
		if (_gui.test.target == _gui.main)
		{
			if (x != _gui.test.x || y != _gui.test.y)
				_gui_redraw(false, x - _gui.test.x, y - _gui.test.y);
			return 0;
		}
		Node* top = MouseMove(_gui.frame, x, y);
		_gui_request_tip(top ? top->GetTip() : nullptr);
		TRACKMOUSEEVENT hover;
		hover.cbSize = sizeof(TRACKMOUSEEVENT);
		hover.dwFlags = TME_LEAVE;
		hover.hwndTrack = _gui.main;
		hover.dwHoverTime = 0;
		TrackMouseEvent(&hover);
		return 0;
	}
	case WM_NCMOUSEMOVE:
	{
		int x = GET_X_LPARAM(lPar);
		int y = GET_Y_LPARAM(lPar);
		RECT r;
		GetWindowRect(hwnd, &r);
		Node* top = MouseMove(_gui.frame, x - r.left, y - r.top);
		_gui_request_tip(top ? top->GetTip() : nullptr);
		break;
	}

	case WM_MOUSELEAVE:
		while (ShowCursor(TRUE) < 0) {}
		MouseMove(_gui.frame, -1,-1);
		_gui_request_tip(nullptr);
		break;
	case WM_LBUTTONDOWN:
		SetCapture(_gui.main);
		MouseClick(_gui.frame, GET_X_LPARAM(lPar), GET_Y_LPARAM(lPar));
		break;
	case WM_LBUTTONUP:
		ReleaseCapture();
		gFocus = nullptr;
		break;
	case WM_MOUSEWHEEL:
	case WM_MOUSEHWHEEL:
	{
		POINT p{GET_X_LPARAM(lPar), GET_Y_LPARAM(lPar)};
		ScreenToClient(_gui.main, &p);
		float v = GET_WHEEL_DELTA_WPARAM(wPar)/WHEEL_DELTA*(msg == WM_MOUSEWHEEL);
		float h = GET_WHEEL_DELTA_WPARAM(wPar)/WHEEL_DELTA*(msg == WM_MOUSEHWHEEL);
		MouseScroll(_gui.frame, p.x, p.y, v, h);
		return 0;
	}
	case WM_SYSKEYUP:
	case WM_KEYUP:
	{
		WORD vkCode = LOWORD(wPar);
		WORD keyFlags = HIWORD(lPar);
		WORD scanCode = LOBYTE(keyFlags);
		BOOL isExtendedKey = (keyFlags & KF_EXTENDED) == KF_EXTENDED;
		if (isExtendedKey) scanCode = MAKEWORD(scanCode, 0xE0);
		ButtonUp(_gui.frame, vkCode, scanCode);
	}
		break;
	case WM_NCHITTEST:
	{
		LRESULT hit = DefWindowProc(hwnd, msg, wPar, lPar);
		return (hit == HTCLIENT) ? _gui.area : hit;
	}
	case WM_GETMINMAXINFO:
	{
		LPMINMAXINFO lpMMI = (LPMINMAXINFO)lPar;
		lpMMI->ptMinTrackSize.x = 470;
		lpMMI->ptMinTrackSize.y = 340;
		return 0;
	}
	case WM_NCCALCSIZE:
		if (wPar == TRUE)
		{
			NCCALCSIZE_PARAMS* pars = (NCCALCSIZE_PARAMS*)lPar;
			_gui.frame->Box.W = pars->rgrc[0].right - pars->rgrc[0].left;
			_gui.frame->Box.H = pars->rgrc[0].bottom - pars->rgrc[0].top;

			BITMAP bmp;
			memset(&bmp, 0, sizeof(BITMAP));
			GetObject(GetCurrentObject(_gui.hdc, OBJ_BITMAP), sizeof(BITMAP), &bmp);
			int w =  (_gui.frame->Box.W > bmp.bmWidth) ? 1.3f*_gui.frame->Box.W : 1.0f*_gui.frame->Box.W;
			int h =  (_gui.frame->Box.H > bmp.bmHeight) ? 1.3f*_gui.frame->Box.H : 1.0f*_gui.frame->Box.H;
			if (w > bmp.bmWidth || h > bmp.bmHeight)
			{
				HDC hdc = GetDC(hwnd);
				HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
				SelectObject(_gui.hdc, bmp);
				DeleteObject(_gui.bmp);
				_gui.bmp = bmp;
			}
			_gui_redraw(true);
			return 0;
		}
		break;
	case WM_CLOSE:
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wPar, lPar);
}

struct TitledBox : virtual Node, Initee<TitledBox>
{
	TextBox title;
	RectGrad outline;
	Node content;

	Initer<TitledBox> initer = this;
	void init()
	{
		Add(&outline);
		Add(&title);
		Add(&content);
		outline.Box.Y = [this](){ return 0.63f*title.H(); };
		outline.Box.W = [this](){ return W(); };
		outline.Box.H = [this](){ return H() - outline.Y(); };
		content.Box.Y = [this](){ return title.H(); };
	}
};

struct KeyBox : virtual Node, Initee<KeyBox>
{
	constexpr static const wchar_t* const NOT_BOUND = L"Not Bound";
	constexpr static const wchar_t* const BINDING = L"...";

	TextBox Description;
	TextBox Key;
	RectGrad KeyBg;
	Prop<int> ScanCode;

	wchar_t _name[50];
	virtual const wchar_t* _key_name()
	{
		return GetKeyNameTextW(ScanCode << 16, _name, sizeof(_name)/sizeof(*_name)) ? _name : NOT_BOUND;
	}

	virtual void MouseLeftClick(float x, float y, Node* top)
	{
		if (top == &KeyBg || top == &Key)
		{
			if (Key.Txt == BINDING)
			{
				ScanCode = 0;
			}
			else
			{
				Key.Txt = BINDING;
			}
		}
		else
		{
			Key.Txt = _key_name();
		}
	}

	virtual void ButtonUp(int vk, int code)
	{
		if (Key.Txt == BINDING)
		{
			this->ScanCode = code;
		}
	}

	virtual void Draw(Graphics* gfx)
	{
		Key.Color = (Key.Txt == NOT_BOUND) ? Color(232, 17, 35) :
					(Key.Txt == BINDING) ? Color(244, 196, 66) :
					Color(172, 239, 252);
	}

	Initer<KeyBox> initer = this;
	void init()
	{
		KeyBg.Box = { .X = [this](){ return Description.X() + Description.W() + 5; } };
		Key.Box = { .Y = 2, .W = [this](){ return KeyBg.W(); }, .H = [this](){ return KeyBg.H(); } };
		Description.Box.Y = Key.Box.Y;
		Key.Txt = _key_name();
		ScanCode.OnSet += [this](auto, auto val) { Key.Txt =_key_name(); };
		Add(&Description);
		Add(&KeyBg);
		KeyBg.Add(&Key);
	}
};

struct Bind : Initee<Bind, TitledBox>
{
	struct CppPos : KeyBox
	{
		SmdBind* binder = nullptr;
		constexpr static const wchar_t* const LMB = L"Left Click";
		constexpr static const wchar_t* const RMB = L"Right Click";
		constexpr static const wchar_t* const FWD = L"Scroll Forward";
		constexpr static const wchar_t* const BWD = L"Scroll Backward";
		constexpr static const wchar_t* const LFT = L"Scroll Left";
		constexpr static const wchar_t* const RGT = L"Scroll Right";
		virtual const wchar_t* _key_name()
		{
			if (!ScanCode && binder)
			{
				SmdBind b = *binder;
				if (b == SMD_BIND_FWD || b == SMD_BIND_FWD_DBL) return FWD;
				if (b == SMD_BIND_BWD || b == SMD_BIND_BWD_DBL) return BWD;
				if (b == SMD_BIND_LFT) return LFT;
				if (b == SMD_BIND_RGT) return RGT;
				if (b == SMD_BIND_LMB || b == SMD_BIND_LMB_DBL || b == SMD_BIND_LMB_HLD || b == SMD_BIND_LMB_DBL_HLD)
					return LMB;
				if (b == SMD_BIND_RMB || b == SMD_BIND_RMB_DBL || b == SMD_BIND_RMB_HLD || b == SMD_BIND_RMB_DBL_HLD)
					return RMB;
				return NOT_BOUND;
			}
			return GetKeyNameTextW(ScanCode << 16, _name, sizeof(_name)/sizeof(*_name)) ? _name : NOT_BOUND;
		}

	} key;
	SmdBind binder = SMD_BIND_BWD;
	int _ts = 0;
	int _n = 0;

	virtual void MouseMove(float x, float y, Node* top)
	{
		if (!key.Key.Contains(top))
		{
			key.KeyBg.Colors = {Color(0, 255, 255, 255), Color(255, 18, 100, 148), Color(255, 0, 6, 9)};
			key.KeyBg.Widths = {1, 1};
			key.KeyBg.Center = {0.5, 0.5};
			if (_ts)
			{
				_n += (GetTickCount() - _ts) > 500;
				_ts = 0;
			}
		}
		else
		{
			if (_ts <= 0) _ts = GetTickCount();
			key.KeyBg.Colors = {Color(0, 25, 85, 125), Color(255, 0, 255, 255), Color(192, 0, 160, 255), Color(255, 0, 6, 9)};
			key.KeyBg.Widths = { 0.66, 0.66, 5};
			key.KeyBg.Center = {0.6, 0.6};
		}
	}

	virtual void Draw(Graphics* gfx)
	{
		if (_gui.binds[0].last == binder)
		{
			outline.Colors = {Color(0, 25, 85, 125), Color(255, 50, 255, 128), Color(192, 50, 255, 128), Color(32, 50, 255, 128)};
		}
		else if (_gui.binds[1].last == binder)
		{
			outline.Colors = {Color(0, 25, 85, 125), Color(255, 25, 200, 100), Color(192, 25, 170, 80), Color(32, 25, 170, 80)};
		}
		else if (_gui.binds[2].last == binder)
		{
			outline.Colors = {Color(0, 25, 85, 125), Color(255, 0, 100, 40), Color(192, 0, 100, 40), Color(32, 0, 100, 40)};
		}
		else
		{
			outline.Colors = {Color(0, 25, 85, 125), Color(255, 25, 85, 125), Color(66, 0, 6, 9), Color(0, 0, 6, 9)};
		}
		TitledBox::Draw(gfx);
	}

	Initer<Bind> initer = this;
	void init()
	{
		title.Fam = _gui.fam;
		title.Color = Color(255, 169, 239, 245);
		title.Size = 14;
		title.Box.X = 10.f;
		outline.R = 3;
		outline.Colors = {Color(0, 25, 85, 125), Color(255, 25, 85, 125), Color(66, 0, 6, 9), Color(0, 0, 6, 9)};
		outline.Widths = { 0.66, 0.66, 5};
		outline.Center = {0.85, 0.85};

		key.Description.Fam = _gui.fam;
		key.Description.Size = 12;
		key.Description.Txt = L"KEY BIND:";
		key.Description.Color = Color(255, 243, 196, 66);
		key.Box.X = 15.f;
		key.Box.Y = 7.f;

		key.Key <<= [](auto& _)
		{
			_.Fam = _gui.fam;
			_.Color = Color(255, 169, 239, 245);
			_.Size = 12;
			_.Format->SetAlignment(StringAlignmentCenter);
			_.Format->SetLineAlignment(StringAlignmentCenter);
		};

		key.KeyBg <<= [this](auto& _)
		{
			_.R = 5;
			_.Colors = {Color(0, 255, 255, 255), Color(255, 18, 100, 148), Color(255, 0, 6, 9)};
			_.Widths = { 1, 1 }; _.Center = {0.5, 0.5};
			_.Box.X = key.Description.X() + key.Description.W();
			_.Box.W = 110; _.Box.H = 20;
		};

		content.Add(&key);
		key.binder = &binder;
		key.ScanCode.OnSet += [this](auto, auto) { _gui.codes[binder] = key.ScanCode; };
	}
};

struct Radion : virtual Node, Initee<Radion>
{
	Ellipze radio;
	Ellipze check;
	KeyBox Option;
	Prop<bool> Checked = false;

	virtual void MouseMove(float x, float y, Node* top)
	{
		if (!radio.Contains(top))
		{
			radio.Colors = {Color(0, 255, 255, 255), Color(255, 18, 100, 148), Color(255, 0, 6, 9)};
			radio.Widths = {1, 1};
		}
		else
		{
			radio.Colors = {Color(0, 25, 85, 125), Color(255, 0, 255, 255), Color(192, 0, 160, 255), Color(255, 0, 6, 9)};
			radio.Widths = { 0.66, 0.66, 5};
		}
	}

	virtual void MouseLeftClick(float x, float y, Node* top)
	{
		if (top == &radio)
		{
			Checked = true;
		}
	}

	Initer<Radion> initer = this;
	void init()
	{
		radio.Colors = {Color(0, 255, 255, 255), Color(255, 18, 100, 148), Color(255, 0, 6, 9)};
		radio.Widths = {1, 1};
		radio.Center = {0.5, 0.5};
		radio.Box = {.W = 22, .H = 22 };

		check.Colors = {Color(0, 177, 132, 22), Color(177, 132, 22), Color(227, 227, 186)};
		check.Widths = {1, 4};
		check.Center = {0.3, 0.3};
		check.Box = { .X = 3, .Y = 3, .W = [this](){ return Checked ? 16.f : 0.f; }, .H = 16 };

		Option.Box = { .X = 30, };
		Add(&radio);
		radio.Add(&check);
		Add(&Option);
	}
};

struct MouseLook : Initee<MouseLook, TitledBox>
{
	Radion mmb;
	Radion key;
	void Selected(auto prop)
	{
		if (prop == &key.Checked)
		{
			mmb.Checked = false;
		}
		else
		{
			key.Checked = false;
		}
		_gui.key_toggle = key.Checked ? +key.Option.ScanCode : -key.Option.ScanCode;
	}

	virtual void MouseMove(float x, float y, Node* top)
	{
		if (!key.Option.Key.Contains(top))
		{
			key.Option.KeyBg.Colors = {Color(0, 255, 255, 255), Color(255, 18, 100, 148), Color(255, 0, 6, 9)};
			key.Option.KeyBg.Widths = {1, 1};
			key.Option.KeyBg.Center = {0.5, 0.5};
		}
		else
		{
			key.Option.KeyBg.Colors = {Color(0, 25, 85, 125), Color(255, 0, 255, 255), Color(192, 0, 160, 255), Color(255, 0, 6, 9)};
			key.Option.KeyBg.Widths = { 0.66, 0.66, 5};
			key.Option.KeyBg.Center = {0.6, 0.6};
		}
	}

	Initer<MouseLook> initer = this;
	void init()
	{
		mmb.Box = { .X = 15, .Y = 5 };
		mmb.Option.Description.Fam = _gui.fam;
		mmb.Option.Description.Txt = L"Middle Click";
		mmb.Option.Description.Color = Color(255, 169, 239, 245);
		mmb.Option.Description.Size = 13;

		key.Box = { .X = [this](){ return W()/2; }, .Y = 5 };
		key.Option.Description = mmb.Option.Description;
		key.Option.Description.Txt = L"Key";
		key.Option.Key.Fam = _gui.fam;
		key.Option.Key.Size = 12;
		key.Option.Key.Color = mmb.Option.Description.Color;
		key.Option.Key.Format->SetAlignment(StringAlignmentCenter);
		key.Option.Key.Format->SetLineAlignment(StringAlignmentCenter);
		key.Option.KeyBg.R = 5;
		key.Option.KeyBg.Colors = {Color(0, 255, 255, 255), Color(255, 18, 100, 148), Color(255, 0, 6, 9)};
		key.Option.KeyBg.Widths = {1, 1};
		key.Option.KeyBg.Center = {0.5, 0.5};
		key.Option.KeyBg.Box.W = 100;
		key.Option.KeyBg.Box.H = 20;


		key.Option.ScanCode = abs(_gui.key_toggle);
		key.Checked = _gui.key_toggle > 0;
		mmb.Checked = _gui.key_toggle < 0;

		key.Option.ScanCode.OnSet += [this](auto, auto scan_code) { _gui.key_toggle = key.Checked ? scan_code : -scan_code; };
		mmb.Checked.OnSet += [this](auto prop, auto val){ if (val) Selected(prop); };
		key.Checked.OnSet += [this](auto prop, auto val){ if (val) Selected(prop); };
		content.Add(&mmb);
		content.Add(&key);

#define TIP_ML L"Press Q to left-right click in mouselook mode\nOnly works in SWToR and this application"
		mmb.radio.Tip = L"Toggle mouselook mode with a middle click\nHold middle click to middle click\n" TIP_ML;
		key.radio.Tip = L"Toggle mouselook mode with a key\n" TIP_ML;
		key.Option.KeyBg.Tip = L"Select key to toggle mouselook mode";
	}
};

struct Radio : virtual Node, Initee<Radio>
{
	RectGrad box;
	RectGrad check;
	TextBox Desc;
	bool Checked = false;

	virtual void MouseMove(float x, float y, Node* top)
	{
		if (!box.Contains(top))
		{
			box.Colors = {Color(0, 255, 255, 255), Color(255, 18, 100, 148), Color(255, 0, 6, 9)};
			box.Widths = {1, 1};
		}
		else
		{
			box.Colors = {Color(0, 25, 85, 125), Color(255, 0, 255, 255), Color(192, 0, 160, 255), Color(255, 0, 18, 12)};
			box.Widths = { 0.66, 0.66, 5};
		}
	}

	virtual void MouseLeftClick(float x, float y, Node* top)
	{
		if (top == &box || top == &check)
		{
			Checked ^= true;
		}
	}

	Initer<Radio> initer = this;
	void init()
	{
		box.Box = { .W = 22, .H = 22 };
		box.R = 5;
		box.Colors = {Color(0, 255, 255, 255), Color(255, 18, 100, 148), Color(255, 0, 6, 9)};
		box.Widths = {1, 1};
		box.Center = {0.5, 0.5};
		check.Box = { .X = 3, .Y = 3, .W = [this](){ return Checked ? 16.f : 0.f; }, .H = 16 };
		check.R = 3;
		check.Colors = {Color(0, 177, 132, 22), Color(177, 132, 22), Color(227, 227, 186)};
		check.Widths = {1, 4};
		check.Center = {0.3, 0.3};
		Desc.Box.X = 50;
		Add(&box);
		box.Add(&check);
		Add(&Desc);
	}
};

struct Modifiers : Initee<Modifiers, TitledBox>
{
	Radio shift;
	Radio alt;
	Radio ctrl;
	SmdMb mouse = SMD_LMB;

	virtual void MouseLeftClick(float x, float y, Node* top)
	{
		_gui.mods[mouse] = shift.Checked ? SMD_MOD_SHIFT : 0;
		_gui.mods[mouse] |= alt.Checked ? SMD_MOD_ALT : 0;
		_gui.mods[mouse] |= ctrl.Checked ? SMD_MOD_CTRL : 0;
	}

	virtual void Draw(Graphics* gfx)
	{
		if (_gui.binds[0].mod[mouse])
		{
			outline.Colors = {Color(0, 25, 85, 125), Color(255, 50, 255, 128), Color(192, 50, 255, 128), Color(32, 50, 255, 128)};
		}
		else
		{
			outline.Colors = {Color(0, 25, 85, 125), Color(255, 25, 85, 125), Color(66, 0, 6, 9), Color(0, 0, 6, 9)};
		}
		TitledBox::Draw(gfx);
	}

	Initer<Modifiers> initer = this;
	void init()
	{
		shift.Desc = ctrl.Desc = alt.Desc = title;
		shift.Desc.Box.X = 40;
		ctrl.Desc.Box.X = 40;
		alt.Desc.Box.X = 40;
		shift.Desc.Txt = L"Shift";
		shift.Box = { .X = 15, .Y = 5 };
		alt.Desc.Txt = L"Alt";
		alt.Box = { .X = 15, .Y = 35 };
		ctrl.Desc.Txt = L"Ctrl";
		ctrl.Box = { .X = 15, .Y = 65 };
		shift.Checked = _gui.mods[mouse] & SMD_MOD_SHIFT;
		alt.Checked = _gui.mods[mouse] & SMD_MOD_ALT;
		ctrl.Checked = _gui.mods[mouse] & SMD_MOD_CTRL;
		content.Add(&shift);
		content.Add(&alt);
		content.Add(&ctrl);

		const wchar_t* tip = L"Modifier applied while this mouse button is down\n"\
								L"Modifier only applies to binds, middle click, Mouse 4 and 5\n"\
								L"If any modifier is configured for this button, Hold binds are only activated on release";
		shift.box.Tip = tip;
		alt.box.Tip = tip;
		ctrl.box.Tip = tip;
	}
};

static void _gui_graph(void)
{
	auto title = new TextBox;
	*title <<= [&_=*title](auto)
	{
		_.Box = { .X = 10, .Y = 10 };
		_.Size = 15; _.Txt = (const wchar_t*)_gui.win.title; _.Color = Color(255, 192, 249, 252);
		_.Fam = _gui.fam;
	};

	struct Close : RectGrad
	{
		bool _bg = false;
		virtual void MouseMove(float x, float y, Node* top) { _bg = this == top; }
		virtual void MouseLeftClick(float x, float y, Node* top) { if (top == this) PostQuitMessage(0); }
		virtual void Draw(Graphics* gfx)
		{
			if (_bg) RectGrad::Draw(gfx);
			Pen pen(Color(255, 172, 239, 252));
			int mx = W()/2;
			int my = H()/2;
			GraphicsState state = gfx->Save();
			gfx->SetSmoothingMode(SmoothingModeNone);
			gfx->DrawLine(&pen, mx - 5, my - 5, mx + 5, my + 5);
			gfx->DrawLine(&pen, mx - 5, my + 5, mx + 5, my - 5);
			gfx->Restore(state);
		}
	} *x = new Close;
	x->Box = { .X = [x](){ return _gui.frame->W() - x->W() - 3; }, .Y = 3, .W = 43, .H = 30 };
	x->R = 5;
	x->Colors = { Color(0, 232, 17, 35), Color(255, 232, 17, 35), };
	x->Widths = { 3 };

	struct Min : RectGrad
	{
		bool _bg = false;
		virtual void MouseMove(float x, float y, Node* top) { _bg = this == top; }
		virtual void MouseLeftClick(float x, float y, Node* top) { if (top == this) ShowWindow(_gui.main, SW_MINIMIZE); }
		virtual void Draw(Graphics* gfx)
		{
			if (_bg) RectGrad::Draw(gfx);
			Pen pen(Color(255, 172, 239, 252));
			int mx = W()/2;
			int my = H()/2;
			gfx->DrawLine(&pen, mx - 5, my, mx + 5, my);
		}
	} *min = new Min;
	min->Box = { .X = [x, min](){ return x->X() - min->W(); }, .Y = 3, .W = 43, .H = 30 };
	min->R = 5;
	min->Colors = { Color(0, 0x1a, 0x75, 0x91), Color(255, 0x1a, 0x75, 0x91), };
	min->Widths = { 3 };

	struct Frame : RectGrad
	{
		Node* _title;
		virtual void MouseMove(float x, float y, Node* top)
		{
			if (x < 0 || y < 0) return;
			_gui.area = HTCLIENT;
			if (top == this || top == _title)
			{
				_gui.area = HTCAPTION;
				if (x < R)
				{
					_gui.area = (y < R) ? HTTOPLEFT : (y > H() - R) ? HTBOTTOMLEFT : HTLEFT;
				}
				else if (x > W() - R)
				{
					_gui.area = (y < R) ? HTTOPRIGHT : (y > H() - R) ? HTBOTTOMRIGHT : HTRIGHT;
				}
				else
				{
					_gui.area = (y < R) ? HTTOP : (y > H() - R) ? HTBOTTOM : _gui.area;
				}
			}
		}
	} *frame = new Frame;
	_gui.area = HTCLIENT;
	frame->_title = title;
	frame->R = 10;
	frame->Colors = {Color(0, 2, 128, 255), Color(220, 2, 160, 170), Color(220, 8, 100, 160), Color(200, 8, 100, 110), Color(220, 8, 80, 112), Color(240, 8, 70, 102)};
	frame->Widths = {1, 2, 6, 8, 40};
	frame->Center = {0.5, 0.9};
	frame->Box = { .W = _gui.win.width, .H = _gui.win.height};

	_gui.frame = frame;
	_gui.frame->Add(x);
	_gui.frame->Add(min);

	struct Binds: Initee<Binds, RectGrad, ScrollPane>
	{
		virtual float X() { return 5; }
		virtual float W() { return _gui.frame->W() - 2*X(); }
		virtual void Draw(Gdiplus::Graphics* gdi)
		{
			RectGrad::Draw(gdi);
			Clip = Path;
		}
	} *binds = new Binds();
	*binds <<= [x](auto& _)
	{
		_.Box.Y = [x](){ return x->Y() + x->H();};
		_.Colors = {Color(0, 255, 255, 255), Color(255, 5, 16, 20), Color(255, 10, 32, 49)};
		_.Widths = {3, 4};
		_.Center = {0.6, 0.6};
		_.R = 5;
		auto scrollers = [](RectGrad& _)
		{
			_.R = 1;
			_.Colors = {Color(128, 227, 227, 186), Color(255, 227, 227, 186)};
			_.Widths = {1};
			_.Center = {0.5, 0.5};
		};
		_.vscroll <<= scrollers;
		_.hscroll <<= scrollers;
	};

	RectGrad* mods = new RectGrad(*binds);
	mods->Box = { .X = [binds](){ return binds->X(); }, .Y = [binds, mods](){ return _gui.frame->H() - mods->H() - binds->X(); }, .W = [binds](){ return binds->W(); }, .H = 125 };

	RectGrad* opts = new RectGrad(*binds);
	opts->Box = { .X = [binds](){ return binds->X(); }, .Y = [binds, opts, mods](){ return mods->Y() - opts->H() - 15; }, .W = [binds](){ return binds->W(); }, .H = 70 };

	binds->Box.H = [binds, opts](){ return opts->Y() - binds->Y() - 15; };
	_gui.frame->Add(binds);
	_gui.frame->Add(opts);
	_gui.frame->Add(mods);
	_gui.frame->Add(title);

	struct
	{
		const wchar_t* title;
		const wchar_t* tip;
		SmdBind bind;
	} layout[4][5] =
	{
		{{L"Left Click", TIP_CLICK, SMD_BIND_LMB}, {L"Left Double Click", TIP_DCLICK, SMD_BIND_LMB_DBL}, {L"Left Long Click", L"1337", SMD_BIND_LMB_PRS}, {L"Left Hold", TIP_HOLD_CLICK, SMD_BIND_LMB_HLD}, {L"Left Double Click Hold", TIP_DHOLD_CLICK, SMD_BIND_LMB_DBL_HLD}},
		{{L"Right Click", TIP_CLICK, SMD_BIND_RMB}, {L"Right Double Click", TIP_DCLICK, SMD_BIND_RMB_DBL}, {L"Right Long Click", L"i think rickert is a pretty cool guy, eh slaps and doesnt afraid of anything", SMD_BIND_RMB_PRS}, {L"Right Hold", TIP_HOLD_CLICK, SMD_BIND_RMB_HLD}, {L"Right Double Click Hold", TIP_DHOLD_CLICK, SMD_BIND_RMB_DBL_HLD}},
		{{L"Scroll Forward", TIP_SCROLL, SMD_BIND_FWD_DBL}, {L"Scroll Backward", TIP_SCROLL, SMD_BIND_BWD_DBL}, {L"Scroll Left", TIP_SCROLL, SMD_BIND_LFT}},
		{{L"Notch Forward", TIP_NOTCH, SMD_BIND_FWD}, {L"Notch Backward", TIP_NOTCH, SMD_BIND_BWD}, {L"Scroll Right", TIP_SCROLL, SMD_BIND_RGT}},
	};

	for (int mouse = 0; mouse < 4; mouse++)
	for (int action = 0; action < 5; action++)
	{
		if (!layout[mouse][action].title) break;
		Bind* bind = new Bind;
		bind->title.Txt = layout[mouse][action].title;
		bind->binder = layout[mouse][action].bind;
		bind->key.ScanCode = _gui.codes[bind->binder];
		if (layout[mouse][action].bind == SMD_BIND_LMB_PRS)
		{
			bind->key.KeyBg.Tip = [bind]()
			{
				const wchar_t* _[] =
				{
					L"advanced",
					L"danger",
					L"do not touch",
					L"send credits instead",
					L"133.7-250 ms click",
				};
				return _[(bind->_n > 3) ? 4 : bind->_n];
			};
		}
		else if (layout[mouse][action].bind == SMD_BIND_RMB_PRS)
		{
			bind->key.KeyBg.Tip = [bind]()
			{
				const wchar_t* _[] =
				{
					L"hello",
					L"i wish i was a little bit taller",
					L"i wish i was a baller",
					L"i wish i had a girl who looked good, i would call her",
					L"i wish i had a rabbit in a hat with a bat and a six four impala",
					L"i wish"
				};
				return _[(bind->_n > 5) ? 5 : bind->_n];
			};
		}
		else
		{
			bind->key.KeyBg.Tip = layout[mouse][action].tip;
		}

		auto X = [binds, bind, mouse]()
		{
			int col = (mouse >= 2 && binds->W() < 2.5*bind->W()) ? mouse - 2 : mouse;
			return 10 + col*(bind->W() + 25);
		};
		auto Y = [binds, bind, mouse, action]()
		{
			float ofs = (mouse >= 2 && binds->W() < 2.5*bind->W()) ? 375 : 0;
			return 5 + ofs + action*70.f;
		};
		bind->Box = { .X = X, .Y = Y, .W = 210, .H = 65 };
		binds->Add(bind);
	}

	MouseLook* ml = new MouseLook;
	ml->title.Fam = _gui.fam;
	ml->title.Txt = L"Mouse look";
	ml->title.Color = Color(255, 169, 239, 245);
	ml->title.Size = 14;
	ml->title.Box.X = 10.f;
	ml->outline.R = 3;
	ml->outline.Colors = {Color(0, 25, 85, 125), Color(255, 25, 85, 125), Color(66, 0, 6, 9), Color(0, 0, 6, 9)};
	ml->outline.Widths = { 0.66, 0.66, 5};
	ml->outline.Center = {0.85, 0.85};
	ml->Box = { .X = 10, .Y = 5, .W = [opts](){ return opts->W() - 20;}, .H = [opts](){ return opts->H() - 10;} };

	auto fyi = new TextBox;
	*fyi <<= [ml](auto& _)
	{
		_.Box.X = ml->title.W();
		_.Size = 14;  _.Color = Color(255, 243, 226, 66);
		_.Txt = L"(needs admin rights to work with swtor)";
		_.Fam = _gui.fam;
	};
	if (_gui.smd.elevated != 2) ml->title.Add(fyi);

	opts->Add(ml);

	Modifiers lmb, mmb, rmb;

	lmb.mouse = SMD_LMB;
	lmb.title = ml->title;
	lmb.outline = ml->outline;
	lmb.title.Txt = L"LMB modifiers";
	lmb.Box = { .X = [](){ return 10; }, .Y = 5, .W = 130, .H = [mods](){ return mods->H() - 5; } };
	mods->Add(new auto(lmb));

	mmb = lmb;
	mmb.mouse = SMD_MMB;
	mmb.title.Txt = L"MMB modifiers";
	mmb.Box.X = [mods](){ return 0.33f*mods->W(); };
	mods->Add(new auto(mmb));

	rmb = lmb;
	rmb.mouse = SMD_RMB;
	rmb.title.Txt = L"RMB modifiers";
	rmb.Box.X = [mods](){ return 0.66f*mods->W(); };
	mods->Add(new auto(rmb));
}

static void _gui_load_win()
{
	FILE* f = fopen("win.txt", "r");
	if (f)
	{
		char win[256];
		fgets(win, sizeof(win), f);
		sscanf(win, "%i,%i,%i,%i", &_gui.win.left, &_gui.win.top, &_gui.win.width, &_gui.win.height);
		fgets(win, sizeof(win), f);
		int i = 0;
		for (; win[i] && win[i] != '\n' && i < sizeof(_gui.win.title)/sizeof(_gui.win.title[0]) - 1; i++)
			_gui.win.title[i] = win[i];
		_gui.win.title[i] = 0;
		fclose(f);
	}
}

#define GUI_FILE "smd5.cfg"
#define GUI_MAGIC 0xdaedc1d3U

typedef struct  __attribute__((packed)) { //same PC only
	uint32_t magic;
	uint8_t codes[SMD_BIND_CNT];
	uint8_t mods[SMD_MB_CNT];
	int key_toggle;
} GuiBin;

static void _gui_load_binds()
{
	_gui.codes[SMD_BIND_FWD] = '1';
	_gui.codes[SMD_BIND_BWD] = '2';

	_gui.codes[SMD_BIND_LMB] = '3';
	_gui.codes[SMD_BIND_LMB_DBL] = '4';
	_gui.codes[SMD_BIND_LMB_DBL_HLD] = '5';

	_gui.codes[SMD_BIND_RMB] = '6';
	_gui.codes[SMD_BIND_RMB_DBL] = '7';
	_gui.codes[SMD_BIND_RMB_DBL_HLD] = '8';

	_gui.codes[SMD_BIND_LFT] = 'X';
	_gui.codes[SMD_BIND_RGT] = 'D';

	_gui.mods[SMD_RMB] = SMD_MOD_SHIFT;

	_gui.key_toggle =  MapVirtualKeyA(VK_SHIFT, MAPVK_VK_TO_VSC);
	for (int bind = 0; bind < SMD_BIND_CNT; bind++)
	if (_gui.codes[bind])
	{
		_gui.codes[bind] = MapVirtualKeyA(_gui.codes[bind], MAPVK_VK_TO_VSC);
	}

	FILE* f = fopen(GUI_FILE, "rb");
	if (f)
	{
		GuiBin bin = { .magic = 0 };
		int len = fread(&bin, 1, sizeof(bin), f);
		if (bin.magic == GUI_MAGIC)
		{
			if (len >= (uint8_t*)&bin.mods - (uint8_t*)&bin) memcpy(_gui.codes, bin.codes, sizeof(_gui.codes));
			if (len >= (uint8_t*)&bin.key_toggle - (uint8_t*)&bin) memcpy(_gui.mods, bin.mods, sizeof(_gui.mods));
			if (len >= sizeof(bin)) _gui.key_toggle = bin.key_toggle;
		}
		fclose(f);
	}
	else
	{
		log_line("Error %x while opening user binds", GetLastError());
	}
}

static void _gui_save_binds() {
	GuiBin bin;
	memset(&bin, 0, sizeof(bin));
	bin.magic = GUI_MAGIC;
	memcpy(bin.codes, _gui.codes, sizeof(bin.codes));
	memcpy(bin.mods, _gui.mods, sizeof(bin.mods));
	bin.key_toggle = _gui.key_toggle;

	FILE* f = fopen(GUI_FILE, "wb");
	if (f)
	{
		fwrite(&bin, sizeof(bin), 1, f);
		fclose(f);
	}
	else
	{
		log_line("Error %x while saving user binds", GetLastError());
	}
}

static void _gui_save_win()
{
	FILE* f = fopen("win.txt", "w");
	if (f)
	{
		RECT r;
		GetWindowRect(_gui.main, &r);
		char win[256];
		sprintf(win, "%i,%i,%i,%i\n", r.left, r.top, r.right - r.left, r.bottom - r.top);
		fwrite(win, strlen(win), 1, f);
		for (int i = 0; _gui.win.title[i] && i < sizeof(_gui.win.title)/sizeof(_gui.win.title[0]) - 1; i++)
			fputc(_gui.win.title[i], f);
		fputc('\n', f);
		const char* fyi = "Delete this file if the window is not showing";
		fwrite(fyi, strlen(fyi), 1, f);
		fclose(f);
	}
}

static void CALLBACK _gui_init(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
	KillTimer(0, idEvent);

	_gui_load_win();
	_gui_load_binds();
	_gui.fonts = new PrivateFontCollection();

	HRSRC reg = FindResource(0, MAKEINTRESOURCE(6), RT_RCDATA);
	void* bytes = LockResource(LoadResource(0, reg));
	DWORD len = SizeofResource(0, reg);
	_gui.fonts->AddMemoryFont(bytes, len);

	FontFamily fam;
	int found;
	_gui.fonts->GetFamilies(1, &fam, &found);
	_gui.fam = fam.Clone();

	WNDCLASS wclass;
	memset(&wclass, 0, sizeof(wclass));
	wclass.lpfnWndProc = _gui_main;
	wclass.hInstance = GetModuleHandle(0);
	wclass.lpszClassName = "gui";
	wclass.hCursor = LoadCursor(0, IDC_ARROW);
	wclass.style = CS_OWNDC;
	RegisterClass(&wclass);

	_gui.main = CreateWindowEx(WS_EX_LAYERED, wclass.lpszClassName, "SWToR Mouse Droid 5", WS_POPUP | WS_VISIBLE,
		_gui.win.left, _gui.win.top, _gui.win.width,_gui.win.width,
		0, 0, wclass.hInstance, 0);

	HDC hdc = GetDC(_gui.main);
	_gui.hdc = CreateCompatibleDC(hdc);
	_gui.bmp = CreateCompatibleBitmap(hdc, _gui.win.width, _gui.win.height);
	SelectObject(_gui.hdc, _gui.bmp);
	_gui_graph();

	int ids[] = {3, 4, 5, 8};
	for (int i = 0; i < 4; i++)
	{
		HRSRC tor = FindResource(0, MAKEINTRESOURCE(ids[i]), RT_RCDATA);
		bytes = LockResource(LoadResource(0, tor));
		len = SizeofResource(0, tor);

		IStream *pStream = SHCreateMemStream((BYTE *) bytes, len);
		Gdiplus::Image img(pStream);
		_gui.toobig.images[i] = img.Clone();
		pStream->Release();
	}

	if (_gui.smd.secrets) ShowWindow(GetConsoleWindow(), SW_SHOW);
	ShowWindow(_gui.main, SW_SHOW);
	smd_gui_cntx_spawn(_gui.main, _gui.fam);
	_gui_post_smd(SMD_MSG_HANDLE, SMD_HANDLE_GUI_WIN, (LPARAM)_gui.main);
}

int smd_gui_bind_code(SmdBind b)
{
	return _gui.codes[b];
}

int smd_gui_get_mod(SmdMb mouse)
{
	return _gui.mods[mouse] & 0x7;
}

int smd_gui_is_hold_click_delayed(SmdMb mouse)
{
	return (_gui.mods[mouse] & (SMD_MOD_SHIFT | SMD_MOD_ALT | SMD_MOD_CTRL)) != 0;
}

int smd_gui_get_key_toggle()
{
	return (_gui.key_toggle < 0) ? 0 : _gui.key_toggle;
}

int smd_gui_get_opt(int opt)
{
	return -1;
}

static void _gui_toggle(MSG* msg)
{
	bool active = msg->wParam;
	_gui.test.target = (HWND)msg->lParam;
	if (!_gui.test.target)
	{
		while (ShowCursor(TRUE) < 0) {}
	}
	else
	{
		if (_gui.test.target == _gui.main)
		{
			if (active) while (ShowCursor(FALSE) >= 0) {}
			else while (ShowCursor(TRUE) < 0) {}
		}
		if (!active) _gui.test.target = 0;
	}
}

DWORD WINAPI smd_gui_run(LPVOID arg) {
	log_designate_thread("gui");
	log_line("Run...");

	GdiplusStartupInput gdipi;
	ULONG_PTR gdipt;
	GpStatus status = GdiplusStartup(&gdipt, &gdipi, NULL);

	memcpy(&_gui.smd, arg, sizeof(_gui.smd));
	MSG msg;
	if (SetTimer(0, 0,  USER_TIMER_MINIMUM, _gui_init))
	while (GetMessageA(&msg, 0, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		switch (msg.message)
		{
		case SMD_MSG_HANDLE:
			smd_gui_store_handle(&msg);
			break;
		case SMD_MSG_TOGGLE:
			_gui_toggle(&msg);
			break;
		case SMD_MSG_BIND:
			_gui.binds[2] = _gui.binds[1];
			_gui.binds[1] = _gui.binds[0];
			_gui.binds[0].last = msg.wParam & 0xff;
			_gui.binds[0].mod[0] = (msg.wParam >> 8) & 0xff;
			_gui.binds[0].mod[1] = (msg.wParam >> 16) & 0xff;
			_gui.binds[0].mod[2] = (msg.wParam >> 24) & 0xff;
			break;
		case SMD_MSG_CROSS:
			_gui.test.x = (int16_t)msg.lParam;
			_gui.test.y = (int16_t)(msg.lParam >> 16);
			MouseMove(_gui.frame, _gui.test.x, _gui.test.y);
			_gui_request_tip(nullptr);
			break;
		}
		bool app =  msg.message >= SMD_MSG_BIND || msg.message == SMD_MSG_CROSS;
		bool scroll = msg.message == WM_MOUSEWHEEL || msg.message == WM_MOUSEHWHEEL;
		if (_gui.test.target != _gui.main || app || scroll)
		{
			_gui_redraw();
		}

	}
	_gui_save_binds();
	_gui_save_win();
	log_line("Exit %i", msg.wParam);
	_gui_post_smd(SMD_MSG_HANDLE, SMD_HANDLE_GUI_WIN, 0);
	GdiplusShutdown(gdipt);
	return msg.wParam;
}
