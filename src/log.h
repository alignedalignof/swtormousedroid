#ifndef SRC_LOG_H_
#define SRC_LOG_H_

#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void log_init(void* pipe, int epoch);
void log_designate_thread(const char* name);
void log_line(const char* str, ...);
void log_deinit();

#ifdef __cplusplus
}
#endif

#endif /* SRC_LOG_H_ */
