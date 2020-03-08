#ifndef TORUI_H
#define TORUI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <D3D9.h>

#define TORUI_WIDTH 1920
#define TORUI_HEIGHT 1017
#define TORUI_SCANS 20

typedef enum {
	TORUI_X,
	TORUI_NEED,
	TORUI_GREED,
	TORUI_PASS,
	TORUI_COUNT,
} torui_e;

typedef struct {
	torui_e ui;
	uint16_t x;
	uint16_t y;
	uint16_t w;
	uint16_t h;
} torui_scan_t;

void torui_init();
void torui_scan(void (*done)(void), int set);
void torui_run(IDirect3DDevice9* dev, IDirect3DSurface9* back);
void torui_loot(torui_e ui);
void torui_reset();
void torui_deinit();


#ifdef __cplusplus
}
#endif

#endif
