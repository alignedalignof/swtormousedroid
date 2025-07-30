#ifndef SMD_H
#define SMD_H

#include <windows.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMD_FOLDER				""
#define SMD_COUNT(array)		(sizeof(array)/sizeof(*array))
#define SMD_MSG_CROSS			(WM_APP + 0)
#define SMD_MSG_NO_CROSS		(WM_APP + 1)
#define SMD_MSG_CLICK			(WM_APP + 2)
#define SMD_MSG_PRESS			(WM_APP + 3)
#define SMD_MSG_TOGGLE			(WM_APP + 4)
#define SMD_MSG_SCAN			(WM_APP + 5)
#define SMD_MSG_CLICK_UI		(WM_APP + 6)
#define SMD_MSG_LOOT			(WM_APP + 7)
#define SMD_MSG_CFG				(WM_APP + 8)
#define SMD_MSG_CNTX			(WM_APP + 9)
#define SMD_MSG_HANDLE			(WM_APP + 10)
#define SMD_MSG_TOR_CHK			(WM_APP + 11)
#define SMD_MSG_MID				(WM_APP + 12)
#define SMD_MSG_MOD				(WM_APP + 13)
#define SMD_MSG_MX				(WM_APP + 14)
#define SMD_MSG_BIND			(WM_APP + 15)
#define SMD_MSG_SCROLL				(WM_APP + 16)

#define SMD_LPAR_TO_CODE(x)		(((x) >> 16) & 0xffU)
#define SMD_CODE_TO_LPAR(x)		((x) << 16)

enum {
	SMD_HANDLE_GUI_WIN,
	SMD_HANDLE_TOR_WIN,
	SMD_HANDLE_TOR_EXE,
	SMD_HANDLE_TOR_TID,
	SMD_HANDLE_IO_TID,
	SMD_HANDLE_TID,
	SMD_HANDLE_LOG_WR,
	SMD_HANDLE_LOG_RD,
	SMD_HANDLE_UI,
	SMD_HANDLE_EPOCH,
};

typedef enum {
	SMD_BIND_FWD,
	SMD_BIND_FWD_DBL,
	SMD_BIND_BWD,
	SMD_BIND_BWD_DBL,
	SMD_BIND_LMB,
	SMD_BIND_LMB_DBL,
	SMD_BIND_LMB_PRS,
	SMD_BIND_LMB_HLD,
	SMD_BIND_LMB_DBL_HLD,
	SMD_BIND_RMB,
	SMD_BIND_RMB_DBL,
	SMD_BIND_RMB_PRS,
	SMD_BIND_RMB_HLD,
	SMD_BIND_RMB_DBL_HLD,
	SMD_BIND_LFT,
	SMD_BIND_RGT,
	SMD_BIND_MID_HLD,
	SMD_BIND_MX1_HLD,
	SMD_BIND_MX2_HLD,
	SMD_BIND_CNT,
} SmdBind;

typedef enum
{
	SMD_LMB,
	SMD_MMB,
	SMD_RMB,
	SMD_MX1,
	SMD_MX2,
	SMD_MB_CNT,
} SmdMb;

typedef enum
{
	SMD_MOD_SHIFT = 1 << 0,
	SMD_MOD_ALT = 1 << 1,
	SMD_MOD_CTRL = 1 << 2,
} SmdMod;

enum {
	SMD_WIN_TOR,
	SMD_WIN_GUI,
	SMD_WIN_CNT,
};

enum {
	SMD_CFG_BIND = 1,
	SMD_CFG_OPT = 2,
};

#define SMD_BIND_IX(b, m)			((b) + SMD_BIND_CNT*(m))
#define SMD_CFG_MAKE_BIND(b)		((SMD_CFG_BIND << 16) | (b))
#define SMD_CFG_MAKE_OPT(o)			((SMD_CFG_OPT << 16) | (o))
#define SMD_CFG_TYPE(cfg)			((cfg) >> 16)
#define SMD_CFG_VAL(cfg)			((cfg) & 0xffffU)

typedef struct {
	int delay;
	int x;
	int y;
	int secrets;
	int epoch;
	int elevated;
	HANDLE log;
	DWORD tid;
	HMODULE dll;
} smd_cfg_t;


int smd_run(const smd_cfg_t* smd_cfg);
void smd_quit();
DWORD WINAPI smd_io_run(LPVOID arg);
DWORD WINAPI smd_gui_run(LPVOID arg);

void smd_gui_line(HDC hdc, int x0, int y0, int x1, int y1);
void smd_gui_gradient_h(HDC hdc, int x0, int y0, COLORREF c0, int x1, int y1, COLORREF c1);
void smd_gui_gradient_v(HDC hdc, int x0, int y0, COLORREF c0, int x1, int y1, COLORREF c1);
const char* smd_gui_bind_name(int b);
int smd_gui_bind_code(SmdBind b);
int smd_gui_get_mod(SmdMb mouse);
int smd_gui_is_hold_click_delayed(SmdMb mouse);
int smd_gui_get_key_toggle();
int smd_gui_get_opt(int opt);
void smd_gui_close();

void smd_gui_cntx_tip(int x, int y, const wchar_t* txt);
void smd_gui_cntx_menu(int x, int y, SmdBind bind);
void smd_gui_cntx_spawn(HWND parent, void* family);

static inline bool smd_peek_messgae(WPARAM id, int ms) {
	MSG msg;
	for (int i = ms/10 + 1; i; --i) {
		Sleep(10);
		if (PeekMessage(&msg, 0, id, id, PM_NOREMOVE))
			return true;
	}
	return false;
}
#ifdef __cplusplus
}
#endif

#endif
