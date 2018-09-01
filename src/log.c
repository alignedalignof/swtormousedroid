#include <stdio.h>
#include <stdint.h>

#include <windows.h>

#include "log.h"

struct{
	HANDLE mutex;
	HANDLE pipe;
	uint32_t tick;
	struct {
		DWORD tid;
		char str[10];
	} tid2str[2];
} static log;

static void log_pipe(const char* stamp, const char* thread, const char* str, va_list args) {
	char buf[8192];
	char* line_end = buf + sizeof(buf);
	char* line = buf;
	line += sprintf(line, "[%-10s]%s: ", thread, stamp);
	if (line < buf)
		return;
	int ret = vsnprintf(line, line_end - line - 2, str, args);
	if (ret > 0)
		line += ret;
	*line++ = '\r';
	*line++ = '\n';
	WriteFile(log.pipe, buf, line - buf, 0, 0);
}
static void log_thread(char* thread) {
	DWORD tid = GetCurrentThreadId();
	for (int i = 0; i < sizeof(log.tid2str)/sizeof(*log.tid2str); ++i)
		if (tid == log.tid2str[i].tid)
			return (void)strcpy(thread, log.tid2str[i].str);
	sprintf(thread, "%x", tid);
}
static void log_timestamp(char* stamp) {
	uint32_t ticks = GetTickCount() - log.tick;
	uint32_t ms = ticks%1000;
	uint32_t s = ticks/1000%60;
	uint32_t m = ticks/1000/60%60;
	uint32_t h = ticks/1000/60/60;
	sprintf(stamp,"%u:%02u:%02u.%03i", h, m, s, ms);
}
void log_init(void* pipe) {
	memcpy(&log.pipe, pipe, sizeof(log.pipe));
	log.tick = GetTickCount();
	log.mutex = CreateMutexA(0, false, "SwtorMouseDroidLogMutex");
}
void log_designate_thread(const char* name) {
	if (WaitForSingleObjectEx(log.mutex, 200, false) != WAIT_OBJECT_0)
		return;
	for (int i = 0; i < sizeof(log.tid2str)/sizeof(*log.tid2str); ++i) {
		if (log.tid2str[i].str[0])
			continue;
		log.tid2str[i].tid = GetCurrentThreadId();
		const uint8_t len = sizeof(log.tid2str[i].str);
		strncpy(log.tid2str[i].str, name, len);
		log.tid2str[i].str[len - 1] = 0;
		break;
	}
	ReleaseMutex(log.mutex);
}
void log_line(const char* str, ...) {
	if (WaitForSingleObjectEx(log.mutex, 200, false) != WAIT_OBJECT_0)
		return;
	char stamp[128];
	log_timestamp(stamp);
	char thread[20];
	log_thread(thread);
	va_list args;
	va_start(args, str);
	log_pipe(stamp, thread, str, args);
	va_end(args);
	ReleaseMutex(log.mutex);
}
void log_deinit() {
	CloseHandle(log.mutex);
}
