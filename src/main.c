#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

#include <windows.h>

#include "getopt/getopt.h"
#include "log.h"
#include "smd.h"

#define APP_DEFAULT_DELAY_MS				200
#define APP_DEFAULT_X_OFS					50
#define APP_DEFAULT_Y_OFS					41
#define APP_LOG_INI							0
#define APP_LOG_RUN							1
#define APP_LOG_DIE							2
#define APP_LOG_DED							3

struct {
	struct {
		struct {
			HANDLE rd;
			HANDLE wr;
		} pipe;
		HANDLE file;
		int append;
		atomic_int run;
	} log;
	smd_cfg_t smd;
} static app = {
	.smd = {
		.delay = APP_DEFAULT_DELAY_MS,
		.x = APP_DEFAULT_X_OFS,
		.y = APP_DEFAULT_Y_OFS,
	},
};

////////////////////////////////////////////////////////////////////////////////

static DWORD WINAPI app_logging_run(LPVOID arg)
{
	while (atomic_load(&app.log.run) == APP_LOG_INI)
		Sleep(0);
	log_designate_thread("log");

	app.log.file = CreateFileA(SMD_FOLDER"log.txt", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, app.log.append ? OPEN_EXISTING : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (INVALID_HANDLE_VALUE == app.log.file)
		log_line("Opening log file failed");
	else if (app.log.append)
		SetFilePointer(app.log.file, 0 , NULL, FILE_END);

	uint8_t str[1024];
	DWORD len;
	DWORD dum;
	while (atomic_load(&app.log.run) != APP_LOG_DIE) {
		if (!ReadFile(app.log.pipe.rd, str, sizeof(str), &len, 0))
			if (GetLastError() != ERROR_MORE_DATA)
				break;
		if (app.log.file != INVALID_HANDLE_VALUE)
			WriteFile(app.log.file, str, len, &dum, 0);
		for (int i = 0; i < len; ++i)
			putchar(str[i]);
	}
	if (app.log.file != INVALID_HANDLE_VALUE)
		WriteFile(app.log.file, ".\n", 2, &dum, 0);
	printf(".\n");
	atomic_store(&app.log.run, APP_LOG_DED);
	return 0;
}

static void app_logging_init()
{
	app.smd.epoch = GetTickCount();
	ShowWindow(GetConsoleWindow(), SW_HIDE);
	atomic_store(&app.log.run, APP_LOG_INI);

	app.log.pipe.rd = INVALID_HANDLE_VALUE;
	app.log.pipe.wr = INVALID_HANDLE_VALUE;
	if (CreatePipe(&app.log.pipe.rd, &app.log.pipe.wr, 0, 8192) &&
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)app_logging_run, 0, 0, 0)) {
		log_init(&app.log.pipe.wr, app.smd.epoch);
		atomic_store(&app.log.run, APP_LOG_RUN);
	}
	else {
		atomic_store(&app.log.run, APP_LOG_DED);
		ShowWindow(GetConsoleWindow(), SW_SHOW);
		printf("Log pump failed");
	}
	app.smd.log = app.log.pipe.wr;
}

static void app_logging_deinit_wait()
{
	for (int i = 50; i; --i) {
		if (atomic_load(&app.log.run) == APP_LOG_DED)
			return;
		Sleep(10);
	}
}

static void app_logging_deinit()
{
	Sleep(30);
	if (atomic_load(&app.log.run) == APP_LOG_RUN)
		atomic_store(&app.log.run, APP_LOG_DIE);
	log_line("");
	app_logging_deinit_wait();
	if (app.log.pipe.wr != INVALID_HANDLE_VALUE)
		CloseHandle(app.log.pipe.wr);
	if (app.log.pipe.rd != INVALID_HANDLE_VALUE)
		CloseHandle(app.log.pipe.rd);
	if (app.log.file != INVALID_HANDLE_VALUE)
		CloseHandle(app.log.file);
}

////////////////////////////////////////////////////////////////////////////////

static bool app_is_elevated()
{
	HANDLE h = NULL;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &h))
		return FALSE;
	TOKEN_ELEVATION elevation = { .TokenIsElevated = FALSE };
	DWORD len = sizeof(elevation);
	GetTokenInformation(h, TokenElevation, &elevation, sizeof(elevation), &len);
	CloseHandle(h);
	return elevation.TokenIsElevated;
}

static int app_elevate(int argn, char* argv[])
{
	char cmd[2048];
	cmd[0] = '\0';
	const char* elevate_cmd = "--elevated --append-log-file";
	int l = strlen(elevate_cmd) + 1;
	for (int i = 1; i < argn; ++i)
		l += strlen(argv[i]) + 1;
	if (l >= sizeof(cmd))
		return -1;
	for (int i = 1; i < argn; ++i) {
		strcat(cmd, argv[i]);
		strcat(cmd, " ");
	}
	strcat(cmd, elevate_cmd);
	char exe[2048];
	GetModuleFileName(0, exe, sizeof(exe));
	SHELLEXECUTEINFO sei;
	sei.cbSize = sizeof(sei);
	sei.fMask = SEE_MASK_DEFAULT | SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS;
	sei.hwnd = 0;
	volatile char posdefender[6] = "sanur";
	volatile char c = posdefender[0];
	posdefender[0] = posdefender[4];
	posdefender[4] = c;
	c = posdefender[1];
	posdefender[1] = posdefender[3];
	posdefender[3] = c;
	sei.lpVerb = (char*)posdefender;
	sei.lpFile = exe;
	sei.lpParameters = cmd;
	sei.lpDirectory = 0;
	sei.nShow = SW_NORMAL;
	sei.hInstApp = 0;
	WINBOOL (*volatile poshaxor)(SHELLEXECUTEINFOA *pExecInfo) = ShellExecuteExA;
	poshaxor(&sei);
	return ((uintptr_t)sei.hInstApp > 32) ? 0 : -1;
}

static BOOL WINAPI app_signal_handler(DWORD ctrl)
{
	smd_quit();
	app_logging_deinit_wait();
	return FALSE;
}

static void app_load_dll()
{
	const char* loc = "smd5.dll";
	HRSRC tor = FindResource(0, MAKEINTRESOURCE(2), RT_RCDATA);
	void* bytes = LockResource(LoadResource(0, tor));
	DWORD len = SizeofResource(0, tor);
	FILE* f = fopen(loc, "wb");
	if (!f)
		return;
	fwrite(bytes, len, 1, f);
	fclose(f);
	app.smd.dll = LoadLibrary(loc);
	if (!app.smd.dll) log_line("%s not loaded: 0x%x", loc, GetLastError());
}

////////////////////////////////////////////////////////////////////////////////

int main(int argn, char* argv[])
{
	struct option lopt[] = {
		{ "elevated", no_argument, &app.smd.elevated, 1 },
		{ "delay", required_argument, 0, 'd' },
		{ "x-offset", required_argument, 0, 'x' },
		{ "y-offset", required_argument, 0, 'y' },
		{ "append-log-file", no_argument, &app.log.append, 1 },
		{ "secrets", no_argument, &app.smd.secrets, 1 },
		{ 0, 0, 0, 0 }
	};
	int ix = 0;
	int c;
	while ((c = getopt_long(argn, argv, "", lopt, &ix)) != -1)
	{
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
		}
	}

	app_logging_init();

	for (int i = 1; i < argn; ++i) log_line("arg %i: %s", i, argv[i]);

	if (!app_is_elevated())
	{
		log_line("Not elevated");
		if (app.smd.elevated) log_line("Elevation failed");
		else if (app_elevate(argn, argv)) log_line("Elevation denied");
		else return app_logging_deinit(), 0;
		app.smd.elevated = 0;
	}
	else
	{
		app.smd.elevated = 2;
	}

	app_load_dll();
	SetConsoleCtrlHandler(app_signal_handler, TRUE);
	int ret = smd_run(&app.smd);
	if (app.smd.dll) FreeLibrary(app.smd.dll);
	app_logging_deinit();
	return ret;
}
