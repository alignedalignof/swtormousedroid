#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

#include <windows.h>
#include <Psapi.h>

#include "getopt/getopt.h"
#include "uiscan/uiscan.h"
#include "log.h"
#include "smd.h"

#define APP_DEFAULT_DELAY_MS				200
#define APP_DEFAULT_X_OFS					50
#define APP_DEFAULT_Y_OFS					45
#define APP_SEARCH_SWTOR					"Waiting for SWTOR..."
enum {
	APP_WAIT,
	APP_RUN,
};
struct {
	struct {
		struct {
			HANDLE app;
			HANDLE sink;
		} pipe;
		HANDLE file;
		int console;
		int append;
		atomic_bool run;
	} log;
	int elevated;
	smd_cfg_t smd;
	atomic_bool run;
	atomic_bool done;
} static app = {
	.smd = {
		.delay = APP_DEFAULT_DELAY_MS,
		.x = APP_DEFAULT_X_OFS,
		.y = APP_DEFAULT_Y_OFS,
		.scan = true,
	},
	.log = {
		.console = 1,
	},
};
////////////////////////////////////////////////////////////////////////////////
static DWORD WINAPI app_logging_run(LPVOID arg) {
	uint8_t str[1024];
	DWORD len;
	while (atomic_load(&app.log.run)) {
		if (!ReadFile(app.log.pipe.sink, str, sizeof(str), &len, 0))
			if (GetLastError() != ERROR_MORE_DATA)
				break;
		WriteFile(app.log.file, str, len, 0, 0);
		if (!app.log.console)
			continue;
		char* ofs = str;
		while (len) {
			putchar(*ofs++);
			--len;
		}
	}
	return 0;
}
static int app_logging_init() {
	app.log.file = CreateFileA("log.txt", GENERIC_WRITE, FILE_SHARE_READ, 0, app.log.append ? OPEN_EXISTING : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (!app.log.file)
		return -1;
	if (app.log.append)
		SetFilePointer(app.log.file, 0 , NULL, FILE_END);
	CreatePipe(&app.log.pipe.sink, &app.log.pipe.app, 0, 8192);
	atomic_store(&app.log.run, true);
	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)app_logging_run, 0, 0, 0);
	app.smd.log = app.log.pipe.app;
	return 0;
}
static void app_logging_deinit() {
	if (!app.log.file)
		return;
	Sleep(500);
	atomic_store(&app.log.run, false);
	log_line(".");
	if (app.log.pipe.app)
		CloseHandle(app.log.pipe.app);
	if (app.log.pipe.sink)
		CloseHandle(app.log.pipe.sink);
	if (app.log.file)
		CloseHandle(app.log.file);
	app.log.file = 0;
}
////////////////////////////////////////////////////////////////////////////////
static bool app_is_elevated() {
	HANDLE h = NULL;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &h))
		return FALSE;
	TOKEN_ELEVATION elevation;
	DWORD len = sizeof(elevation);
	if (!GetTokenInformation(h, TokenElevation, &elevation, sizeof( elevation ), &len))
		elevation.TokenIsElevated = FALSE;
	CloseHandle(h);
	return elevation.TokenIsElevated;
}
static void app_elevate(int argn, char* argv[]) {
	char cmd[2048];
	cmd[0] = '\0';
	const char* elevate_cmd = "--elevated --append-log-file";
	int l = strlen(elevate_cmd) + 1;
	for (int i = 1; i < argn; ++i)
		l += strlen(argv[i]) + 1;
	if (l >= sizeof(cmd))
		return;
	for (int i = 1; i < argn; ++i) {
		strcat(cmd, argv[i]);
		strcat(cmd, " ");
	}
	strcat(cmd, elevate_cmd);
	char exe[2048];
	GetModuleFileName(0, exe, sizeof(exe));
	SHELLEXECUTEINFO sei;
	sei.cbSize = sizeof(sei);
	sei.fMask = SEE_MASK_DEFAULT | SEE_MASK_NOASYNC;
	sei.hwnd = 0;
	sei.lpVerb = "runas";
	sei.lpFile = exe;
	sei.lpParameters = cmd;
	sei.lpDirectory = 0;
	sei.nShow = SW_NORMAL;
	sei.hInstApp = 0;
	ShellExecuteExA(&sei);
}
////////////////////////////////////////////////////////////////////////////////
static BOOL CALLBACK app_find_swtor(HWND win, LPARAM par) {
	if (!IsWindowVisible(win))
		return TRUE;
	DWORD pid;
	DWORD tid = GetWindowThreadProcessId(win, &pid);
	HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, pid);
	if (!h)
		return TRUE;
	char exe[2048] = "";
	GetModuleFileNameEx(h, 0, exe, sizeof(exe));
	CloseHandle(h);
	if (!strstr(exe, "swtor.exe"))
		return TRUE;
	app.smd.win = win;
	app.smd.thread = tid;
	app.smd.swtor = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE, 0, pid);
	return FALSE;
}
static void CALLBACK app_check_swtor(HWND Arg1, UINT Arg2, UINT_PTR Arg3, DWORD Arg4) {
	if (app.smd.swtor) {
		DWORD active = 0;
		GetExitCodeProcess(app.smd.swtor, &active);
		if (active != STILL_ACTIVE) {
			CloseHandle(app.smd.swtor);
			log_line(APP_SEARCH_SWTOR);
			smd_deinit();
			app.smd.swtor = 0;
		}
	}
	if (app.smd.swtor)
		return;
	EnumWindows(app_find_swtor, 0);
	if (!app.smd.swtor)
		return;
	log_line("SWTOR found");
	smd_init(&app.smd);
}
static void app_end() {
	atomic_store(&app.run, false);
	while (!atomic_load_explicit(&app.done, memory_order_relaxed))
		Sleep(0);
}
////////////////////////////////////////////////////////////////////////////////
static void main_signal(int signal) {
	exit(0);
}
int main(int argn, char* argv[]) {
	struct option lopt[] = {
		{ "elevated", no_argument, &app.elevated, 1 },
		{ "delay", required_argument, 0, 'd' },
		{ "x-offset", required_argument, 0, 'x' },
		{ "y-offset", required_argument, 0, 'y' },
		{ "append-log-file", no_argument, &app.log.append, 1 },
		{ "disable-console-log", no_argument, &app.log.console, 0 },
		{ "disable-ui-scan", no_argument, 0, 's' },
		{ 0, 0, 0, 0 }
	};
	int ix = 0;
	int c;
	while ((c = getopt_long(argn, argv, "", lopt, &ix)) != -1) {
		switch (c) {
		case 'd':
			app.smd.delay = atoi(optarg);
			break;
		case 'x':
			app.smd.x = atoi(optarg);
			break;
		case 'y':
			app.smd.y = atoi(optarg);
			break;
		case 's':
			app.smd.scan = false;
			break;
		}
	}
	atomic_store(&app.run, true);
	if (app_logging_init()) {
		printf("Can't open log file");
		return -1;
	}

	log_init(&app.log.pipe.app);
	log_designate_thread("smd");
	for (int i = 1; i < argn; ++i)
		log_line("arg %i: %s", i, argv[i]);

	if (!app_is_elevated()) {
		log_line("Not elevated");
		if (app.elevated) {
			log_line("Elevation attempt failed");
			app_logging_deinit();
		}
		else {
			app_logging_deinit();
			app_elevate(argn, argv);
		}
		return -1;
	}
	atexit(app_end);
	signal(SIGABRT, main_signal);
	signal(SIGINT, main_signal);
	signal(SIGTERM, main_signal);
	signal(SIGBREAK, main_signal);

	log_line(APP_SEARCH_SWTOR);
	MSG msg;
	UINT_PTR ticker = SetTimer(0, 0, 1000, app_check_swtor);
	while (GetMessageA(&msg, 0, 0, 0) != -1) {
		if (msg.message == WM_QUIT || !atomic_load_explicit(&app.run, memory_order_relaxed))
			break;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (app.smd.swtor)
			smd_process_msg(&msg);
	}
	log_line("Exit");
	smd_deinit();
	app_logging_deinit();
	atomic_store(&app.done, true);
	return 0;
}
