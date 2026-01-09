#include "wcron/service.h"
#include "minwindef.h"
#include "wcron/parser.h"
#include "wcron/runner.h"
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

cron_job jobs[WCRON_MAX_JOBS];
int job_count = 0; // Number of jobs loaded from crontab.txt
int paused = 0;
SERVICE_STATUS service_status;
SERVICE_STATUS_HANDLE service_status_handle;
HANDLE stop_event;

// Template when creating a new crontab file
const char *DEFAULT_CRONTAB_TEMPLATE =
    "# wcrontab - Windows Cron Task Scheduler\n"
    "# Edit this file to introduce tasks to be run by wcron.\n"
    "#\n"
    "# Each line is a scheduled task with the following format:\n"
    "# +------------- minute (0 - 59)\n"
    "# | +------------- hour (0 - 23)\n"
    "# | | +------------- day of month (1 - 31)\n"
    "# | | | +------------- month (1 - 12)\n"
    "# | | | | +------------- day of week (0 - 7) (Sunday=0 or 7)\n"
    "# | | | | |\n"
    "# * * * * * command to execute\n"
    "#\n"
    "# Special characters:\n"
    "#   *     any value\n"
    "#   ,     value list separator (e.g., 1,3,5)\n"
    "#   -     range of values (e.g., 1-5)\n"
    "#   /     step values (e.g., */5 = every 5 units)\n"
    "#\n"
    "# Examples:\n"
    "# 0 2 * * * C:\\backup\\daily_backup.bat          # Run at 2:00 AM every day\n"
    "# */15 * * * * C:\\scripts\\check_status.exe      # Run every 15 minutes\n"
    "# 0 9 * * 1 C:\\reports\\weekly_report.bat        # Run at 9:00 AM every Monday\n"
    "# 30 14 1 * * C:\\tasks\\monthly_task.exe         # Run at 2:30 PM on first day of month\n"
    "#\n"
    "# NOTE: Use absolute paths for commands. Relative paths may not work.\n"
    "# NOTE: Lines starting with # are comments and will be ignored.\n"
    "\n";

void log_msg(const char *msg) {
    char log_path[MAX_PATH];
    char exe_dir[MAX_PATH];

    // Get executable directory
    if (__dirname(exe_dir, sizeof(exe_dir)) == 0) {
        return;
    }

    int result = snprintf(log_path, sizeof(log_path), "%s\\wcron.log", exe_dir);
    if (result >= (int)sizeof(log_path) || result < 0) {
        return;
    }

    FILE *f = fopen(log_path, "a");
    if (!f)
        return;

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
            tm->tm_min, tm->tm_sec, msg);
    fclose(f);
}

void show_logs() {
    char log_path[MAX_PATH];
    char exe_dir[MAX_PATH];

    // Get executable directory
    if (__dirname(exe_dir, sizeof(exe_dir)) == 0) {
        printf("Failed to get executable directory\n");
        return;
    }

    int result = snprintf(log_path, sizeof(log_path), "%s\\wcron.log", exe_dir);
    if (result >= (int)sizeof(log_path) || result < 0) {
        return;
    }

    FILE *f = fopen(log_path, "r");
    if (!f) {
        printf("No log file found at: %s\n", log_path);
        return;
    }

    printf("=== wCron Logs ===\n");
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        printf("%s", line);
    }
    fclose(f);
}

int __dirname(char *buffer, size_t length) {
    DWORD len = GetModuleFileName(NULL, buffer, (DWORD)length);
    if (len == 0) {
        return 0;
    }

    char *last_slash = strrchr(buffer, '\\');
    if (last_slash) {
        *last_slash = '\0';
    }

    return 1;
}

int __filename(char *buffer, size_t length) {
    DWORD len = GetModuleFileName(NULL, buffer, (DWORD)length);
    if (len == 0) {
        return 0;
    }

    // Find last backslash to get filename
    char *last_slash = strrchr(buffer, '\\');
    if (last_slash) {
        char *filename = last_slash + 1;
        memmove(buffer, filename, strlen(filename) + 1);
    }

    return 1;
}

int get_crontab_path(char *buffer, size_t size) {
    char dir[MAX_PATH];
    if (!__dirname(dir, MAX_PATH))
        return 0;
    int res = snprintf(buffer, size, "%s%s%s", dir, DIRECTORY_SEPARATOR, "crontab.txt");
    return (res > 0 && res < (int)size);
}

void load_jobs() {
    char crontab_path[MAX_PATH];
    if (!get_crontab_path(crontab_path, sizeof(crontab_path))) {
        perror("Failed to get crontab path");
        return;
    }

    FILE *fp = fopen(crontab_path, "r");
    if (!fp) {
        return;
    }

    char line[512];
    job_count = 0;

    while (fgets(line, sizeof(line), fp) && job_count < WCRON_MAX_JOBS) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }

        if (parse_cron_line(line, &jobs[job_count]) == 0) {
            job_count++;
        }
    }

    fclose(fp);
}

/**
 * Create a crontab file with the default template
 * @param path Path of the file to create
 * @return 1 if created successfully, 0 otherwise
 */
int create_default_crontab(const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        return 0;
    }

    fputs(DEFAULT_CRONTAB_TEMPLATE, fp);
    fclose(fp);
    return 1;
}

/**
 * Open the editor with safe path validation
 * @param crontab_path Path of the crontab file
 * @return 1 if opened successfully, 0 otherwise
 */
int open_editor_safely(const char *crontab_path) {
    // Review path for invalid characters
    if (strchr(crontab_path, '\"') != NULL || strchr(crontab_path, '&') != NULL) {
        fprintf(stderr, "Error: Invalid characters in crontab path\n");
        return 0;
    }

    // Use CreateProcess for greater security
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Build command safely
    char cmd[MAX_PATH * 2];
    snprintf(cmd, sizeof(cmd), "notepad.exe \"%s\"", crontab_path);

    // Create process
    if (!CreateProcess(NULL,  // No module name (use command line)
                       cmd,   // Command line
                       NULL,  // Process handle not inheritable
                       NULL,  // Thread handle not inheritable
                       FALSE, // Set handle inheritance to FALSE
                       0,     // No creation flags
                       NULL,  // Use parent's environment block
                       NULL,  // Use parent's starting directory
                       &si,   // Pointer to STARTUPINFO structure
                       &pi)   // Pointer to PROCESS_INFORMATION structure
    ) {
        fprintf(stderr, "Failed to open editor (error %lu)\n", GetLastError());
        return 0;
    }

    // Wait for the editor to close
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Close handles
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return 1;
}

// ============================ WIN32 API IMPLEMENTATION ============================
void WINAPI ServiceCtrlHandler(DWORD control) {
    switch (control) {
    case SERVICE_CONTROL_STOP:
        service_status.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(service_status_handle, &service_status);
        SetEvent(stop_event);
        break;
    case SERVICE_CONTROL_PAUSE:
        paused = 1;
        service_status.dwCurrentState = SERVICE_PAUSED;
        SetServiceStatus(service_status_handle, &service_status);
        break;
    case SERVICE_CONTROL_CONTINUE:
        paused = 0;
        service_status.dwCurrentState = SERVICE_RUNNING;
        SetServiceStatus(service_status_handle, &service_status);
        break;
    case 128: // custom reload
        load_jobs();
        break;
    default:
        break;
    }
}

void WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv) {
    (void)dwArgc;
    (void)lpszArgv;

    service_status_handle = RegisterServiceCtrlHandler(WCRON_SERVICE_NAME, ServiceCtrlHandler);
    if (!service_status_handle) {
        printf("Failed to register service control handler: %lu\n", GetLastError());
        return;
    }

    service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    service_status.dwCurrentState = SERVICE_START_PENDING;
    service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE;
    service_status.dwWin32ExitCode = 0;
    service_status.dwServiceSpecificExitCode = 0;
    service_status.dwCheckPoint = 0;
    service_status.dwWaitHint = 0;

    SetServiceStatus(service_status_handle, &service_status);

    load_jobs();
    init_job_system();

    stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    _beginthread(scheduler_thread, 0, NULL);

    service_status.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(service_status_handle, &service_status);

    log_msg("Cron service started successfully");
    WaitForSingleObject(stop_event, INFINITE);
    log_msg("Cron service stopping");
    shutdown_job_system();

    CloseHandle(stop_event);
}

void InstallService() {
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!scm) {
        printf("Failed to open service manager: %lu\n", GetLastError());
        return;
    }

    char exe_path[MAX_PATH];
    DWORD len = GetModuleFileName(NULL, exe_path, MAX_PATH);
    if (len == 0) {
        printf("Failed to get module path: %lu\n", GetLastError());
        CloseServiceHandle(scm);
        return;
    }

    char exec_path[WCRON_PATH_MAX_SIZE];
    int result = snprintf(exec_path, sizeof(exec_path), "\"%s\" service", exe_path);
    if (result >= (int)sizeof(exec_path) || result < 0) {
        printf("Failed to construct executable path: buffer too small\n");
        CloseServiceHandle(scm);
        return;
    }

    SC_HANDLE service =
        CreateService(scm, WCRON_SERVICE_NAME, "Cron Service", SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                      SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, exec_path, NULL, NULL, NULL, NULL, NULL);
    if (!service) {
        printf("Failed to install service: %ld\n", GetLastError());
    } else {
        printf("Service installed successfully.\n");
    }

    CloseServiceHandle(scm);
}

void UninstallService() {
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) {
        printf("Failed to open service manager: %lu\n", GetLastError());
        return;
    }

    SC_HANDLE service = OpenService(scm, WCRON_SERVICE_NAME, DELETE);
    if (service) {
        DeleteService(service);
        CloseServiceHandle(service);
    }
    CloseServiceHandle(scm);
}

void StartCronService() {
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) {
        printf("Failed to open service manager: %lu\n", GetLastError());
        return;
    }

    SC_HANDLE service = OpenService(scm, WCRON_SERVICE_NAME, SERVICE_START);
    if (service) {
        if (!StartService(service, 0, NULL)) {
            printf("Failed to start service: %lu\n", GetLastError());
        } else {
            printf("Service started successfully.\n");
        }
        CloseHandle(service);
    } else {
        printf("Failed to open service: %lu\n", GetLastError());
    }
    CloseServiceHandle(scm);
}

void StopCronService() {
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) {
        printf("Failed to open service manager: %lu\n", GetLastError());
        return;
    }

    SC_HANDLE service = OpenService(scm, WCRON_SERVICE_NAME, SERVICE_STOP);
    if (service) {
        SERVICE_STATUS status;
        ControlService(service, SERVICE_CONTROL_STOP, &status);
        CloseServiceHandle(service);
    }
    CloseServiceHandle(scm);
}

void PauseCronService() {
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) {
        printf("Failed to open service manager: %lu\n", GetLastError());
        return;
    }

    SC_HANDLE service = OpenService(scm, WCRON_SERVICE_NAME, SERVICE_PAUSE_CONTINUE);
    if (service) {
        SERVICE_STATUS status;
        ControlService(service, SERVICE_CONTROL_PAUSE, &status);
        CloseServiceHandle(service);
    }
    CloseServiceHandle(scm);
}

void ResumeCronService() {
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) {
        printf("Failed to open service manager: %lu\n", GetLastError());
        return;
    }

    SC_HANDLE service = OpenService(scm, WCRON_SERVICE_NAME, SERVICE_PAUSE_CONTINUE);
    if (service) {
        SERVICE_STATUS status;
        ControlService(service, SERVICE_CONTROL_CONTINUE, &status);
        CloseServiceHandle(service);
    }

    CloseServiceHandle(scm);
}

void ReloadCronService() {
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) {
        printf("Failed to open service manager: %lu\n", GetLastError());
        return;
    }

    SC_HANDLE service = OpenService(scm, WCRON_SERVICE_NAME, SERVICE_USER_DEFINED_CONTROL);
    if (service) {
        SERVICE_STATUS status;
        ControlService(service, 128, &status); // custom code
        CloseServiceHandle(service);
    }
    CloseServiceHandle(scm);
}
