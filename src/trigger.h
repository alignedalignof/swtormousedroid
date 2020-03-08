#ifndef TRIGGER_H
#define TRIGGER_H

#include <windows.h>

#include "log.h"

enum {
	TRIGGER_IDLE,
	TRIGGER_DOWN,
	TRIGGER_UP,

	TRIGGER_ONCE,
	TRIGGER_TWICE,
	TRIGGER_PRESSED,
	TRIGGER_HELD,
};

typedef struct {
	int tick;
	int timeout;
	int state;
	struct {
		void (*once)(void);
		void (*twice)(void);
		void (*pressed)(void);
		void (*held)(void);
	} cb;

} trigger_t;

static inline void trigger_reset(trigger_t* trigger) {
	trigger->state = TRIGGER_IDLE;
}

static inline void trigger_press(trigger_t* trigger) {
	trigger->tick = GetTickCount();
	switch (trigger->state) {
	case TRIGGER_IDLE:
		trigger->state = TRIGGER_ONCE;
		break;
	case TRIGGER_UP:
		trigger->state = TRIGGER_DOWN;
		if (trigger->cb.twice)
			break;
		trigger->state = TRIGGER_HELD;
		trigger->cb.held();
		break;
	}
}

static inline void trigger_release(trigger_t* trigger) {
	switch (trigger->state) {
	case TRIGGER_ONCE:
		trigger->state = TRIGGER_IDLE;
		if (trigger->cb.twice || trigger->cb.held)
			trigger->state = TRIGGER_UP;
		else if (trigger->cb.once)
			trigger->cb.once();
		break;
	case TRIGGER_PRESSED:
		trigger->state = TRIGGER_IDLE;
		trigger->cb.pressed();
		break;
	case TRIGGER_DOWN:
		trigger->state = TRIGGER_IDLE;
		if (!trigger->cb.twice)
			break;
		trigger->state = TRIGGER_TWICE;
		//ikr
	case TRIGGER_TWICE:
		trigger->cb.twice();
		break;
	default:
		trigger->state = TRIGGER_IDLE;
		break;
	}
}

static inline void trigger_tick(trigger_t* trigger) {
	int dt = GetTickCount() - trigger->tick;
	if (dt < trigger->timeout)
		return;
	switch (trigger->state) {
	case TRIGGER_UP:
		trigger->state = TRIGGER_IDLE;
		if (trigger->cb.once)
			trigger->cb.once();
		break;
	case TRIGGER_ONCE:
		if (trigger->cb.pressed)
			trigger->cb.pressed();
		trigger->tick += 3*trigger->timeout;
		trigger->state = TRIGGER_PRESSED;
		break;
	case TRIGGER_PRESSED:
		if (trigger->cb.pressed)
			trigger->cb.pressed();
		trigger->tick += 3*trigger->timeout;
		break;
	case TRIGGER_DOWN:
		trigger->state = TRIGGER_IDLE;
		if (!trigger->cb.held)
			break;
		trigger->state = TRIGGER_HELD;
	case TRIGGER_HELD:
		trigger->tick += 3*trigger->timeout;
		trigger->cb.held();
		break;
	case TRIGGER_TWICE:
		trigger->state = TRIGGER_IDLE;
		break;
	}
}

static inline bool trigger_is_held(trigger_t* trigger) {
	return trigger->state == TRIGGER_ONCE || trigger->state == TRIGGER_DOWN || trigger->state == TRIGGER_HELD || trigger->state == TRIGGER_PRESSED;
}

#endif
