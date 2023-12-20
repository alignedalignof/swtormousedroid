#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <atomic>

#include <windows.h>
#include <D3D9.h>
#include <D3dx9tex.h>

#include "minhook-master/include/MinHook.h"
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
	DWORD tid;
} static d3d;

static HRESULT APIENTRY (*tor_end_scene)(IDirect3DDevice9* device);
static HRESULT APIENTRY smd_end_scene(IDirect3DDevice9* device) {
	IDirect3DSurface9* ui = 0;
	device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &ui);
	if (!ui) return tor_end_scene(device);
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
	return tor_end_scene(device);
}

int d3d_init(DWORD thread, HANDLE pipe) {
	MH_Initialize();

	d3d.tid = thread;

	WNDCLASSEX cls;
	IDirect3D9* d3d9 = 0;
	IDirect3DDevice9* dev = 0;
	HWND win = 0;
	D3DDISPLAYMODE display_mode;
	D3DPRESENT_PARAMETERS present_parameters;
	uint64_t* dVtable;

	memset(&cls, 0, sizeof(cls));
	cls.cbSize = sizeof(WNDCLASSEX);
	cls.style = CS_HREDRAW | CS_VREDRAW;
	cls.lpfnWndProc = DefWindowProc;
	cls.lpszClassName = "mousedroog5";
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
	dVtable = (uint64_t*)dev;
	dVtable = (uint64_t*)dVtable[0];
	ret = -7;
	if (MH_CreateHook((void*)dVtable[42], (void*)&smd_end_scene, (void**)&tor_end_scene) != MH_OK)
		goto cleanup;
	ret = -8;
	if (MH_EnableHook((void*)dVtable[42]) != MH_OK)
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

void d3d_deinit()
{
	MH_Uninitialize();
}

void d3d_cross(int x, int y)
{
	d3d.cross.x = x;
	d3d.cross.y = y;
	d3d.cross.show = 1;
}

void d3d_nocross()
{
	d3d.cross.show = 0;
}

