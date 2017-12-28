#include <windows.h>
#include <D3D9.h>

#include "minhook-master/include/MinHook.h"

#include "d3d.h"

struct {
	int x;
	int y;
	int show;
} static cross;

static HRESULT APIENTRY (*OrigEndScene)(IDirect3DDevice9* pDevice);
static HRESULT APIENTRY MyEndScene(IDirect3DDevice9* pDevice) {
	if (!cross.show)
		return OrigEndScene(pDevice);
	IDirect3DSurface9* target = 0;
	pDevice->GetRenderTarget(0, & target);
	RECT r;
	r.left = cross.x;
	r.top = cross.y;
	r.right = cross.x + 3;
	r.bottom = cross.y + 3;
	pDevice->ColorFill(target, &r, D3DCOLOR_RGBA(255, 255, 255, 255));
	if (target)
		target->Release();
	return OrigEndScene(pDevice);
}

void d3d_hook() {
	struct D3D {
		WNDCLASSEX cls;
		HWND win;
		IDirect3D9* d3d;
		IDirect3DDevice9* dev;
		D3D() :
			win(),
			d3d(),
			dev()
		{
			memset(&cls, 0, sizeof(cls));
			cls.cbSize = sizeof(WNDCLASSEX);
			cls.style = CS_HREDRAW | CS_VREDRAW;
			cls.lpfnWndProc = DefWindowProc;
			cls.lpszClassName = "mousedroog";
			cls.hInstance = GetModuleHandle(0);
			if (RegisterClassEx(&cls)) {
				win =  CreateWindow(cls.lpszClassName, "-3-", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, cls.hInstance, NULL);
				if (win) {
					d3d = Direct3DCreate9(D3D_SDK_VERSION);
					if (d3d) {
						D3DDISPLAYMODE display_mode;
						d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &display_mode);
						D3DPRESENT_PARAMETERS present_parameters;
						ZeroMemory(&present_parameters, sizeof(D3DPRESENT_PARAMETERS));
						present_parameters.Windowed = TRUE;
						present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
						present_parameters.BackBufferFormat = display_mode.Format;
						if (d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, win,
							D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT, &present_parameters, &dev) == D3D_OK) {
							DWORD* dVtable = (DWORD*)dev;
							dVtable = (DWORD*)dVtable[0];
							if (MH_CreateHook((DWORD_PTR*)dVtable[42], (void*)&MyEndScene, (void**)&OrigEndScene) == MH_OK)
								MH_EnableHook((DWORD_PTR*)dVtable[42]);
						}
					}
				}
			}
		}
		~D3D() {
			if (dev)
				dev->Release();
			if (d3d)
				d3d->Release();
			DestroyWindow(win);
			UnregisterClass(cls.lpszClassName, cls.hInstance);
		}
	} d3d;
}

void d3d_cross(int x, int y) {
	cross.x = x;
	cross.y = y;
	cross.show = 1;
}
void d3d_nocross() {
	cross.show = 0;
}
