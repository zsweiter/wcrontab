#ifndef WCRON_RUNNER_H
#define WCRON_RUNNER_H

#include <windows.h>

extern HANDLE stop_event;

void init_job_system(void);
void __cdecl scheduler_thread(void *param);
void shutdown_job_system(void);

#endif // WCRON_RUNNER_H
