#ifndef D3D_H_
#define D3D_H_

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

int d3d_init(DWORD thread, HANDLE pipe);
void d3d_deinit();
void d3d_cross(int x, int y);
void d3d_nocross();
void d3d_flash_loot();

#ifdef __cplusplus
}
#endif


#endif /* D3D_H_ */
