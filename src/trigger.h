#ifndef TRIGGER_H
#define TRIGGER_H

#include <windows.h>

#include "log.h"

enum {
	TRIGGER_IDLE,
	TRIGGER_ONCE,
	TRIGGER_HOLD,
	TRIGGER_RELEASED,
	TRIGGER_TWICE,
	TRIGGER_DTAP,
	TRIGGER_DHOLD,
};

typedef struct {
	int tick;
	int state;
	struct {
		void (*tap)(void);
		void (*press)(void);
		void (*hold)(void);
		void (*dtap)(void);
		void (*dhold)(void);
	} cb;
	struct
	{
		int tap;
		int press;
		int dtap;
		int repeat;
	} window;
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
	case TRIGGER_RELEASED:
		trigger->state = TRIGGER_TWICE;
		break;
	}
}

static inline void trigger_release(trigger_t* trigger) {
	int dt = GetTickCount() - trigger->tick;
	switch (trigger->state) {
	case TRIGGER_ONCE:
		trigger->state = TRIGGER_IDLE;
		if (!trigger->cb.press || dt < trigger->window.tap)
		{
			if (trigger->cb.dtap || trigger->cb.dhold)
			{
				trigger->state = TRIGGER_RELEASED;
			}
			else if (trigger->cb.tap)
			{
				trigger->cb.tap();
			}
		}
		else if (trigger->cb.press)
		{
			trigger->cb.press();
		}
		break;
	case TRIGGER_TWICE:
		trigger->state = TRIGGER_DTAP;
		/* no break */
	case TRIGGER_DTAP:
		trigger->cb.dtap();
		break;
	case TRIGGER_HOLD:
		trigger->state = TRIGGER_IDLE;
		if (trigger->cb.hold)
			trigger->cb.hold();
		break;
	case TRIGGER_DHOLD:
		trigger->state = TRIGGER_IDLE;
		if (trigger->cb.dhold)
			trigger->cb.dhold();
		break;
	default:
		trigger->state = TRIGGER_IDLE;
		break;
	}
}

static inline void trigger_tick(trigger_t* trigger) {
	int dt = GetTickCount() - trigger->tick;
	if (trigger->tick)
	switch (trigger->state) {
	case TRIGGER_IDLE:
		trigger->tick = 0;
		break;
	case TRIGGER_ONCE:
		if (dt < trigger->window.tap + trigger->window.press)
			break;
		trigger->state = TRIGGER_HOLD;
		dt = trigger->window.repeat;
		/* no break */
	case TRIGGER_HOLD:
		if (dt < trigger->window.repeat)
			break;
		if (trigger->cb.hold)
			trigger->cb.hold();
		else if (trigger->cb.press)
			trigger->cb.press();
		trigger->tick += trigger->window.repeat ? trigger->window.repeat : -trigger->tick;
		break;
	case TRIGGER_RELEASED:
		if (dt < trigger->window.dtap)
			break;
		if (trigger->cb.tap)
			trigger->cb.tap();
		trigger->state = TRIGGER_IDLE;
		break;
	case TRIGGER_DTAP:
		if (dt < trigger->window.dtap)
			break;
		trigger->state = TRIGGER_IDLE;
		break;
	case TRIGGER_TWICE:
		if (dt < trigger->window.dtap)
			break;
		trigger->state = TRIGGER_DHOLD;
		dt = trigger->window.repeat;
		/* no break */
	case TRIGGER_DHOLD:
		if (dt < trigger->window.repeat)
			break;
		if (trigger->cb.dhold)
			trigger->cb.dhold();
		trigger->tick += trigger->window.repeat ? trigger->window.repeat : -trigger->tick;
		break;
	}
}

static inline bool trigger_is_held(trigger_t* trigger) {
	return trigger->state == TRIGGER_ONCE || trigger->state == TRIGGER_HOLD || trigger->state == TRIGGER_TWICE || trigger->state == TRIGGER_DHOLD;
}

#endif
