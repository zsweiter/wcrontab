#ifndef WCRON_SERVICE_H
#define WCRON_SERVICE_H

#include "parser.h"
#include <windows.h>

#define WCRON_SERVICE_NAME "CronService"
#define WCRON_MAX_JOBS 100
#define WCRON_PATH_MAX_SIZE 1024
#define WCRON_VERSION "0.0.2"

#ifdef _WIN32
#define DIRECTORY_SEPARATOR "\\"
#else
#define DIRECTORY_SEPARATOR "/"
#endif // DIRECTORY_SEPARATOR

#define WCRON_SERVICE_SUFFIX " service"
#define WCRON_SERVICE_SUFFIX_LEN (sizeof(WCRON_SERVICE_SUFFIX))

// Template when creating a new crontab file
extern const char *DEFAULT_CRONTAB_TEMPLATE;

void log_msg(const char *msg);
void show_logs();
int __dirname(char *buffer, size_t length);
int __filename(char *buffer, size_t length);
int get_crontab_path(char *buffer, size_t size);

// For edit cron expression in crontab file (secure)
int open_editor_safely(const char *crontab_path);
int create_default_crontab(const char *path);

extern cron_job jobs[WCRON_MAX_JOBS];
extern int job_count;
extern int paused;

void InstallService();
void UninstallService();
void StartCronService();
void StopCronService();
void PauseCronService();
void ResumeCronService();
void ReloadCronService();

// The ServiceMain function is responsible for initializing the service and handling service control requests.
void WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv);

#endif // WCRON_SERVICE_H
