#include "wcron/service.h"
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

int main(int argc, char *argv[]) {
    // Run as service if "service" is passed
    if (argc == 2 && strcmp(argv[1], "service") == 0) {
        SERVICE_TABLE_ENTRY st[] = {{WCRON_SERVICE_NAME, ServiceMain}, {NULL, NULL}};
        if (!StartServiceCtrlDispatcher(st)) {
            fprintf(stderr, "Failed to start service (error %lu)\n", GetLastError());
            return 1;
        }

        return 0;
    }

    // Check for version command first
    if (argc == 2 && (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)) {
        wprintf(L"wCron version %hs\n", WCRON_VERSION);
        return 0;
    }

    // user commands when no arguments are provided or help is requested
    if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        wprintf(L"wCron version %hs\n", WCRON_VERSION);
        wprintf(L"Usage: wcrontab -l|-e|-r|install|uninstall|start|stop|pause|resume|reload|logs|version\n");
        wprintf(L"\nOptions:\n");
        wprintf(L"  -l, --list    List current crontab\n");
        wprintf(L"  -e, --edit    Edit crontab\n");
        wprintf(L"  -r, --remove  Remove crontab\n");
        wprintf(L"  -v, --version Show version information\n");
        wprintf(L"  -h, --help    Show this help message\n");
        wprintf(L"\nService commands:\n");
        wprintf(L"  install     Install wcron service\n");
        wprintf(L"  uninstall   Uninstall wcron service\n");
        wprintf(L"  start       Start wcron service\n");
        wprintf(L"  stop        Stop wcron service\n");
        wprintf(L"  pause       Pause wcron service\n");
        wprintf(L"  resume      Resume wcron service\n");
        wprintf(L"  reload      Reload crontab configuration\n");
        wprintf(L"  logs        Show wcron log file\n");
        return 0;
    }

    char *cmd = argv[1];
    char crontab_path[MAX_PATH];

    if (!get_crontab_path(crontab_path, sizeof(crontab_path))) {
        fprintf(stderr, "Error: Failed to get crontab path\n");
        return 1;
    }

    // List current crontab (-l)
    if (strcmp(cmd, "-l") == 0) {
        FILE *fp = fopen(crontab_path, "r");
        if (!fp) {
            // File does not exist → create with template
            if (!create_default_crontab(crontab_path)) {
                fprintf(stderr, "Error: Failed to create crontab file\n");
                return 1;
            }
            wprintf(L"No crontab found. Created new crontab with template.\n");
            wprintf(L"Use 'wcrontab -e' to edit it.\n");
            return 0;
        }

        // Show content
        char buf[1024];
        int has_content = 0;
        while (fgets(buf, sizeof(buf), fp) != NULL) {
            wprintf(L"%hs", buf);
            has_content = 1;
        }
        fclose(fp);

        if (!has_content) {
            wprintf(L"(empty crontab)\n");
        }

        // Open editor safely
    } else if (strcmp(cmd, "-e") == 0) {
        // verify if file exists
        FILE *fp = fopen(crontab_path, "r");
        if (!fp) {
            // Create with template
            if (!create_default_crontab(crontab_path)) {
                fprintf(stderr, "Error: Failed to create crontab file\n");
                return 1;
            }
            wprintf(L"Created new crontab with template.\n");
        } else {
            fclose(fp);
        }

        // Open editor safely
        if (!open_editor_safely(crontab_path)) {
            return 1;
        }

        printf("Crontab updated. Use 'wcrontab reload' to apply changes.\n");

        // delete crontab (-r)
    } else if (strcmp(cmd, "-r") == 0) {
        // Get confirmation for removal
        printf("Are you sure you want to remove your crontab? (y/n): ");
        char confirm = getchar();
        if (confirm != 'y' && confirm != 'Y') {
            printf("Cancelled.\n");
            return 0;
        }

        if (remove(crontab_path) != 0) {
            fprintf(stderr, "Error: Failed to remove crontab\n");
            return 1;
        }
        printf("Crontab removed successfully.\n");

        // WIN32 SERVICE MANAGEMENT
    } else if (strcmp(cmd, "install") == 0) {
        printf("Installing wcron service...\n");
        InstallService();

    } else if (strcmp(cmd, "uninstall") == 0) {
        printf("Uninstalling wcron service...\n");
        UninstallService();

    } else if (strcmp(cmd, "start") == 0) {
        printf("Starting wcron service...\n");
        StartCronService();

    } else if (strcmp(cmd, "stop") == 0) {
        printf("Stopping wcron service...\n");
        StopCronService();

    } else if (strcmp(cmd, "pause") == 0) {
        printf("Pausing wcron service...\n");
        PauseCronService();

    } else if (strcmp(cmd, "resume") == 0) {
        printf("Resuming wcron service...\n");
        ResumeCronService();

    } else if (strcmp(cmd, "reload") == 0) {
        printf("Reloading crontab configuration...\n");
        ReloadCronService();

    } else if (strcmp(cmd, "logs") == 0) {
        show_logs();

    } else if (strcmp(cmd, "version") == 0) {
        wprintf(L"wCron version %hs\n", WCRON_VERSION);

    } else {
        fprintf(stderr, "Error: Unknown command '%s'\n", cmd);
        printf("Use 'wcrontab' without arguments to see usage.\n");
        return 1;
    }

    return 0;
}
