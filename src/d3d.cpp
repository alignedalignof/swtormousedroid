#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <atomic>

#include <windows.h>
#include <D3D9.h>
#include <D3dx9tex.h>

#include "minhook-master/include/MinHook.h"

#include "uiscan/uiscan.h"
#include "log.h"
#include "smd.h"

#include "d3d.h"

using namespace std;

struct {
	struct {
		int x;
		int y;
		int show;
	} cross;
	struct {
		DWORD thread;
		HANDLE pipe;
	} swtor;
	struct {
		UiElement elements[20];
		int count;
		uint8_t flash;
		int flash_tick;
		uint32_t scans;
		int report;
	} ui;
} static d3d;

static void d3d_highlight_x(IDirect3DDevice9* device, IDirect3DSurface9* ui, UiElement* element) {
	RECT line;
	line.top = element->top - 2;
	line.bottom = element->top;
	line.left = element->left - 2;
	line.right = element->left;
	device->ColorFill(ui, &line, D3DCOLOR_RGBA(255, 255, 255, 255));
}
static void d3d_highlight_loot(IDirect3DDevice9* device, IDirect3DSurface9* ui, UiElement* element, D3DCOLOR color)
{
	if (d3d.ui.flash & 1)
		color = D3DCOLOR_RGBA(255, 0, 0, 255);
	RECT thickLine;
	thickLine.top = element->top - 1;
	thickLine.bottom = element->top;
	thickLine.left = element->left - 1;
	thickLine.right = element->right + 1;
	device->ColorFill(ui, &thickLine, color);

	thickLine.top = element->bottom;
	thickLine.bottom = element->bottom + 1;
	device->ColorFill(ui, &thickLine, color);

	thickLine.top = element->top;
	thickLine.bottom = element->bottom;
	thickLine.left = element->left - 1;
	thickLine.right = element->left;
	device->ColorFill(ui, &thickLine, color);

	thickLine.left = element->right;
	thickLine.right = element->right + 1;
	device->ColorFill(ui, &thickLine, color);
}
static void d3d_highlight_elements(IDirect3DDevice9* device, IDirect3DSurface9* ui, UiElement* element)
{

	switch (element->control) {
	case UI_CONTROL_X:
		d3d_highlight_x(device, ui, element);
		break;
	case UI_CONTROL_GREED:
		d3d_highlight_loot(device, ui, element, D3DCOLOR_RGBA(0, 255, 255, 255));
		break;
	case UI_CONTROL_NEED:
		d3d_highlight_loot(device, ui, element, D3DCOLOR_RGBA(255, 255, 0, 255));
		break;
	}
}
static HRESULT APIENTRY (*OrigEndScene)(IDirect3DDevice9* device);
static HRESULT APIENTRY MyEndScene(IDirect3DDevice9* device) {
	IDirect3DSurface9* ui = 0;
	device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &ui);
	if (!ui)
		return OrigEndScene(device);
	UiElement elements[20];
	uint16_t count = sizeof(elements)/sizeof(*elements);
	if (uiscan_run(device, elements, &count) == UI_SCAN_DONE) {
		++d3d.ui.scans;
		if (!uiscan_same(elements, d3d.ui.elements, count, d3d.ui.count)) {
			if (!count || WriteFile(d3d.swtor.pipe, elements, count*sizeof(*elements), 0, 0))
				PostThreadMessageA(d3d.swtor.thread, SMD_MSG_SCAN, count, 0);
			d3d.ui.count = count;
			for (int i = 0; i < count; ++i)
				d3d.ui.elements[i] = elements[i];
		}
	}
	if (GetTickCount() - d3d.ui.report > 30*1000) {
		d3d.ui.report = GetTickCount();
		log_line("UI scans: %u", d3d.ui.scans);
	}
	int dt = GetTickCount() - d3d.ui.flash_tick;
	if (d3d.ui.flash && dt > 500) {
		++d3d.ui.flash;
		d3d.ui.flash_tick = GetTickCount();
		if (d3d.ui.flash == 6)
			d3d.ui.flash = 0;
	}
	for (int i = 0; i < d3d.ui.count; ++i)
		d3d_highlight_elements(device, ui, &d3d.ui.elements[i]);
	if (d3d.cross.show)
	{
		RECT r;
		r.left = d3d.cross.x;
		r.top = d3d.cross.y;
		r.right = d3d.cross.x + 3 ;
		r.bottom = d3d.cross.y + 3;
		device->ColorFill(ui, &r, D3DCOLOR_RGBA(255, 255, 255, 255));
	}
	ui->Release();
	return OrigEndScene(device);
}
int d3d_init(DWORD thread, HANDLE pipe) {
	d3d.swtor.pipe = pipe;
	d3d.swtor.thread = thread;

	if (d3d.swtor.pipe)
		if (uiscan_init())
			return -1;

	MH_Initialize();

	WNDCLASSEX cls;
	IDirect3D9* d3d9 = 0;
	IDirect3DDevice9* dev = 0;
	HWND win = 0;
	D3DDISPLAYMODE display_mode;
	D3DPRESENT_PARAMETERS present_parameters;
	DWORD* dVtable;

	memset(&cls, 0, sizeof(cls));
	cls.cbSize = sizeof(WNDCLASSEX);
	cls.style = CS_HREDRAW | CS_VREDRAW;
	cls.lpfnWndProc = DefWindowProc;
	cls.lpszClassName = "mousedroog";
	cls.hInstance = GetModuleHandle(0);
	int ret = -2;
	if (!RegisterClassEx(&cls))
		goto cleanup;
	ret = -3;
	win =  CreateWindow(cls.lpszClassName, "-3-", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, cls.hInstance, NULL);
	if (!win)
		goto cleanup;
	ret = -4;
	d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
	if (!d3d9)
		goto cleanup;
	ret = -5;
	if (d3d9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &display_mode) != D3D_OK)
		goto cleanup;

	ZeroMemory(&present_parameters, sizeof(D3DPRESENT_PARAMETERS));
	present_parameters.Windowed = TRUE;
	present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	present_parameters.BackBufferFormat = display_mode.Format;
	ret = -6;
	if (d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, win,
		D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT, &present_parameters, &dev) != D3D_OK)
		goto cleanup;
	dVtable = (DWORD*)dev;
	dVtable = (DWORD*)dVtable[0];
	ret = -7;
	if (MH_CreateHook((DWORD_PTR*)dVtable[42], (void*)&MyEndScene, (void**)&OrigEndScene) != MH_OK)
		goto cleanup;
	ret = -8;
	if (MH_EnableHook((DWORD_PTR*)dVtable[42]) != MH_OK)
		goto cleanup;
	ret = 0;
cleanup:
	if (dev)
		dev->Release();
	if (d3d9)
		d3d9->Release();
	if (win)
		DestroyWindow(win);
	UnregisterClass(cls.lpszClassName, cls.hInstance);
	return ret;
}
void d3d_deinit() {
	MH_Uninitialize();
	d3d.ui.report = GetTickCount();
	if (d3d.swtor.pipe)
		uiscan_deinit();
}
void d3d_cross(int x, int y) {
	d3d.cross.x = x;
	d3d.cross.y = y;
	d3d.cross.show = 1;
}
void d3d_nocross() {
	d3d.cross.show = 0;
}
void d3d_flash_loot() {
	d3d.ui.flash = 1;
	d3d.ui.flash_tick = GetTickCount();
}
