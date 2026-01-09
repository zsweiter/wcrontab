#include "wcron/runner.h"
#include "wcron/parser.h"
#include "wcron/service.h"
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

BOOL execute_command_safely(const char *command);
void __cdecl execute_job_worker(void *param);

static BOOL spawn_process(const char *cmdline, DWORD *exit_code);
BOOL should_execute_job(cron_job *job, struct tm *current_time, time_t now);

typedef struct {
    char command[512];
    int job_index;
    time_t scheduled_time;
} job_execution_data;

static BOOL spawn_process(const char *cmdline, DWORD *exit_code) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    BOOL ok = CreateProcessA(NULL, (LPSTR)cmdline, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);

    if (!ok) {
        return FALSE;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    if (exit_code) {
        GetExitCodeProcess(pi.hProcess, exit_code);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return TRUE;
}

CRITICAL_SECTION jobs_lock;

BOOL execute_command_safely(const char *command) {
    char cmd_line[2048];
    DWORD exit_code = 0;
    char msg[512];

    // !TODO: Prevent command injection
    int r = snprintf(cmd_line, sizeof(cmd_line), "%s", command);
    if (r > 0 && r < (int)sizeof(cmd_line)) {
        if (spawn_process(cmd_line, &exit_code)) {
            if (exit_code == 0) {
                snprintf(msg, sizeof(msg), "Job successfully runs: %s", command);
                log_msg(msg);
                return TRUE;
            } else {
                snprintf(msg, sizeof(msg), "Direct execution failed with exit code %lu", exit_code);
                log_msg(msg);
            }
        }
    }

    // Try with cmd.exe is insecure: only use if absolutely necessary, dev purposes
    r = snprintf(cmd_line, sizeof(cmd_line), "cmd.exe /C \"%s\"", command);
    if (r <= 0 || r >= (int)sizeof(cmd_line)) {
        log_msg("Command too long for cmd.exe");
        return FALSE;
    }

    if (!spawn_process(cmd_line, &exit_code)) {
        snprintf(msg, sizeof(msg), "cmd.exe execution failed (err=%lu)", GetLastError());
        log_msg(msg);
        return FALSE;
    }

    snprintf(msg, sizeof(msg), "Executed via cmd.exe with exit code %lu", exit_code);
    log_msg(msg);
    return exit_code == 0;
}

void __cdecl execute_job_worker(void *param) {
    job_execution_data *data = (job_execution_data *)param;

    char log_buffer[768];
    snprintf(log_buffer, sizeof(log_buffer), "Executing job #%d: %s", data->job_index, data->command);
    log_msg(log_buffer);

    DWORD start_time = GetTickCount();
    BOOL success = execute_command_safely(data->command);
    DWORD elapsed = GetTickCount() - start_time;

    if (success) {
        snprintf(log_buffer, sizeof(log_buffer), "Job #%d completed in %lu ms", data->job_index, elapsed);
    } else {
        snprintf(log_buffer, sizeof(log_buffer), "Job #%d failed after %lu ms", data->job_index, elapsed);
    }
    log_msg(log_buffer);

    // Mark job as not running
    EnterCriticalSection(&jobs_lock);
    if (data->job_index < job_count) {
        jobs[data->job_index].last_run = data->scheduled_time;
        jobs[data->job_index].is_running = 0;
    }
    LeaveCriticalSection(&jobs_lock);

    free(data);
}

BOOL should_execute_job(cron_job *job, struct tm *current_time, time_t now) {
    if (job->is_running) {
        return FALSE;
    }

    if (job->last_run > 0) {
        time_t time_diff = now - job->last_run;
        if (time_diff < 60) {
            return FALSE;
        }
    }

    return time_matches(job, current_time);
}

void __cdecl scheduler_thread(void *param) {
    (void)param;

    InitializeCriticalSection(&jobs_lock);

    log_msg("Scheduler thread started");

    HANDLE timer = CreateWaitableTimer(NULL, FALSE, NULL);
    if (!timer) {
        log_msg("Failed to create waitable timer");
        return;
    }

    LARGE_INTEGER due_time;
    due_time.QuadPart = 0;

    if (!SetWaitableTimer(timer, &due_time, 1000, NULL, NULL, FALSE)) {
        log_msg("Failed to set waitable timer");
        CloseHandle(timer);
        return;
    }

    int last_minute = -1;

    while (1) {
        HANDLE handles[2] = {stop_event, timer};
        DWORD wait_result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

        if (wait_result == WAIT_OBJECT_0) {
            log_msg("Scheduler received stop signal");
            break;
        }

        if (paused) {
            continue;
        }

        time_t now = time(NULL);
        struct tm *current_time = localtime(&now);

        if (!current_time) {
            continue;
        }

        int current_minute = current_time->tm_min;

        if (current_minute != last_minute) {
            last_minute = current_minute;

            EnterCriticalSection(&jobs_lock);

            for (int i = 0; i < job_count; i++) {
                cron_job *job = &jobs[i];

                if (should_execute_job(job, current_time, now)) {
                    job->is_running = 1;

                    job_execution_data *data = malloc(sizeof(job_execution_data));
                    if (data) {
                        strncpy(data->command, job->command, sizeof(data->command) - 1);
                        data->command[sizeof(data->command) - 1] = '\0';
                        data->job_index = i;
                        data->scheduled_time = now;

                        uintptr_t thread = _beginthread(execute_job_worker, 0, data);
                        if ((int)thread == -1) {
                            log_msg("Failed to create job execution thread");
                            job->is_running = 0;
                            free(data);
                        }
                    } else {
                        log_msg("Failed to allocate memory for job execution");
                        job->is_running = 0;
                    }
                }
            }

            LeaveCriticalSection(&jobs_lock);
        }
    }

    CancelWaitableTimer(timer);
    CloseHandle(timer);
    DeleteCriticalSection(&jobs_lock);

    log_msg("Scheduler thread stopped");
}

void init_job_system(void) {
    for (int i = 0; i < WCRON_MAX_JOBS; i++) {
        cron_job *job = &jobs[i];
        if (job) {
            job->is_running = 0;
            job->last_run = 0;
        }
    }
}

void shutdown_job_system(void) {
    log_msg("Shutting down job system");

    Sleep(5000);

    EnterCriticalSection(&jobs_lock);

    for (int i = 0; i < job_count; i++) {
        if (jobs[i].is_running) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Warning: Job #%d still running during shutdown", i);
            log_msg(msg);
        }
    }

    LeaveCriticalSection(&jobs_lock);
}
