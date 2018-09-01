#ifndef SMD_H
#define SMD_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMD_MSG_INIT			(WM_APP + 0)
#define SMD_MSG_DEINIT			(WM_APP + 1)
#define SMD_MSG_CROSS			(WM_APP + 2)
#define SMD_MSG_NO_CROSS		(WM_APP + 3)
#define SMD_MSG_CLICK			(WM_APP + 4)
#define SMD_MSG_PRESS			(WM_APP + 5)
#define SMD_MSG_LOG				(WM_APP + 6)
#define SMD_MSG_TOGGLE			(WM_APP + 7)
#define SMD_MSG_SCAN			(WM_APP + 8)
#define SMD_MSG_CLICK_UI		(WM_APP + 9)
#define SMD_MSG_FLASH_LOOT		(WM_APP + 10)

typedef struct {
	int delay;
	int x;
	int y;
	HANDLE swtor;
	HANDLE log;
	HWND win;
	DWORD thread;
	bool scan;
} smd_cfg_t;

int smd_init(const smd_cfg_t* smd_cfg);
void smd_process_msg(MSG* msg);
void smd_deinit();

#ifdef __cplusplus
}
#endif

#endif
