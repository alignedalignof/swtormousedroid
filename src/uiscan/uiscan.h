#ifndef UISCAN_H
#define UISCAN_H

#include <stdint.h>

#include <D3D9.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum UiScan {
	UI_SCAN_BUSY,
	UI_SCAN_DONE,
	UI_SCAN_ERROR,
} UiScan;

typedef enum UiControl {
	UI_CONTROL_X,
	UI_CONTROL_NEED,
	UI_CONTROL_GREED,
} UiControl;

typedef struct UiElement {
	UiControl control;
	uint16_t top;
	uint16_t bottom;
	uint16_t left;
	uint16_t right;
} UiElement;

int uiscan_init();
UiScan uiscan_run(IDirect3DDevice9* device, UiElement* elements, uint16_t* count);
bool uiscan_same(const UiElement* a, const UiElement* b, uint16_t a_count, uint16_t b_count);
void uiscan_deinit();

#ifdef __cplusplus
}
#endif

#endif
