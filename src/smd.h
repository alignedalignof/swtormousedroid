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
#define SMD_MSG_FLASH_LOOT		(WM_APP + 7)
#define SMD_MSG_CFG				(WM_APP + 8)
#define SMD_MSG_CNTX			(WM_APP + 9)
#define SMD_MSG_HANDLE			(WM_APP + 10)
#define SMD_MSG_TOR_CHK			(WM_APP + 11)

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
	SMD_BIND_LMB_DBL_PRS,
	SMD_BIND_RMB,
	SMD_BIND_RMB_DBL,
	SMD_BIND_RMB_PRS,
	SMD_BIND_RMB_DBL_PRS,
	SMD_BIND_CNT,
} SmdBind;

typedef enum {
	SMD_MOD_NON,
	SMD_MOD_LMB,
	SMD_MOD_RMB,
	SMD_MOD_CNT,
} SmdMod;

enum {
	SMD_MOD_BIND_CNT = SMD_BIND_CNT * SMD_MOD_CNT,
};

enum {
	SMD_OPT_EXT_BINDS,
	SMD_OPT_MOD_BINDS,
	SMD_OPT_TIPS,
	SMD_OPT_IDK,
	SMD_OPT_CONSOLE,
	SMD_OPT_NO_U,
	SMD_OPT_ACT_SMD,
	SMD_OPT_ACT_KEY,
	SMD_OPT_CNT,
};

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


int smd_run();
void smd_quit();
DWORD WINAPI smd_io_run(LPVOID arg);
DWORD WINAPI smd_gui_run(LPVOID arg);

void smd_gui_line(HDC hdc, int x0, int y0, int x1, int y1);
void smd_gui_gradient_h(HDC hdc, int x0, int y0, COLORREF c0, int x1, int y1, COLORREF c1);
void smd_gui_gradient_v(HDC hdc, int x0, int y0, COLORREF c0, int x1, int y1, COLORREF c1);
const char* smd_gui_bind_name(int b);
int smd_gui_bind_code(SmdBind b, SmdMod m);
void smd_gui_close();

void smd_gui_cntx_tip(int x, int y, const char* txt);
void smd_gui_cntx_menu(int x, int y, SmdBind bind, SmdMod mod);
void smd_gui_cntx_spawn(HWND parent);

#ifdef __cplusplus
}
#endif

#endif
