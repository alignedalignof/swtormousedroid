#ifndef TRIGGER_H
#define TRIGGER_H

#include <windows.h>

#include "log.h"

typedef enum {
	TRIGGER_IDLE,
	TRIGGER_HELD,
	TRIGGER_RELEASED,
	TRIGGER_FIRED,
	TRIGGER_HELD_FIRED,
} trigger_state_e;

typedef struct {
	int tick;
	int timeout;
	trigger_state_e state;
	void (*once)(void);
	void (*twice)(void);
	void (*held)(void);
} trigger_t;

static inline void trigger_reset(trigger_t* trigger) {
	trigger->state = TRIGGER_IDLE;
	trigger->tick = GetTickCount();
}
static inline void trigger_press(trigger_t* trigger) {
	trigger->tick = GetTickCount();
	switch (trigger->state) {
	case TRIGGER_IDLE:
		trigger->state = TRIGGER_HELD;
		break;
	case TRIGGER_RELEASED:
		trigger->state = TRIGGER_FIRED;
	case TRIGGER_FIRED:
		if (trigger->twice)
			trigger->twice();
		break;
	}
}
static inline void trigger_release(trigger_t* trigger) {
	switch (trigger->state) {
	case TRIGGER_HELD:
		if (trigger->twice) {
			trigger->state = TRIGGER_RELEASED;
		}
		else {
			trigger->state = TRIGGER_IDLE;
			if (trigger->once)
				trigger->once();
		}
		break;
	case TRIGGER_HELD_FIRED:
		trigger->state = TRIGGER_IDLE;
		break;
	}

}
static inline void trigger_tick(trigger_t* trigger) {
	if (GetTickCount() - trigger->tick < trigger->timeout)
		return;
	switch (trigger->state) {
	case TRIGGER_RELEASED:
		trigger->state = TRIGGER_IDLE;
		if (trigger->once)
			trigger->once();
		break;
	case TRIGGER_HELD:
		trigger->state = TRIGGER_HELD_FIRED;
		//ikr
	case TRIGGER_HELD_FIRED:
		trigger->tick = GetTickCount() - trigger->timeout/2;
		if (trigger->held)
			trigger->held();
		break;
	case TRIGGER_FIRED:
		trigger->state = TRIGGER_IDLE;
		break;
	}
}
static inline bool trigger_is_held(trigger_t* trigger) {
	return trigger->state == TRIGGER_HELD || trigger->state == TRIGGER_HELD_FIRED;
}
#endif
