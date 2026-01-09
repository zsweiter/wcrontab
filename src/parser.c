#include "wcron/parser.h"
#include "wcron/service.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_range(int *arr, int start, int end, int step, int offset, int max) {
    if (step < 1)
        step = 1;

    for (int i = start; i <= end; i += step) {
        int idx = i - offset;
        if (idx >= 0 && idx < max) {
            arr[idx] = 1;
        }
    }
}

static int validate_value(int value, int min, int max, const char *field_name) {
    if (value < min || value > max) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Invalid %s value: %d (must be %d-%d)", field_name, value, min, max);
        log_msg(msg);
        return 0;
    }
    return 1;
}

/**
 * Parse one field of the cron expression
 *
 * @param field raw cron field string
 */
static int parse_field(const char *field, int *arr, int min, int max, int offset, int size) {
    if (!field || !arr) {
        log_msg("NULL pointer in parse_field");
        return -1;
    }

    memset(arr, 0, sizeof(int) * size);

    char *copy = strdup(field);
    if (!copy) {
        log_msg("Failed to allocate memory in parse_field");
        return -1;
    }

    char *saveptr;
    char *token = strtok_r(copy, ",", &saveptr);

    while (token) {
        while (isspace(*token))
            token++;
        char *end = token + strlen(token) - 1;
        while (end > token && isspace(*end))
            *end-- = '\0';

        if (strlen(token) == 0) {
            token = strtok_r(NULL, ",", &saveptr);
            continue;
        }

        if (token[0] == '*') {
            int step = 1;

            if (strlen(token) > 1 && token[1] == '/') {
                step = atoi(token + 2);
                if (step < 1) {
                    log_msg("Invalid step value in */N");
                    free(copy);
                    return -1;
                }
            }

            set_range(arr, min, max, step, offset, size);
        } else if (strchr(token, '-')) {
            int start, end, step = 1;
            char *slash = strchr(token, '/');

            if (slash) {
                *slash = '\0';
                step = atoi(slash + 1);
                if (step < 1) {
                    log_msg("Invalid step in range");
                    free(copy);
                    return -1;
                }
            }

            if (sscanf(token, "%d-%d", &start, &end) != 2) {
                log_msg("Invalid range format");
                free(copy);
                return -1;
            }

            if (!validate_value(start, min, max, "range start") || !validate_value(end, min, max, "range end")) {
                free(copy);
                return -1;
            }

            if (start > end) {
                log_msg("Range start > end");
                free(copy);
                return -1;
            }

            set_range(arr, start, end, step, offset, size);
        } else {
            int val = atoi(token);

            if (!validate_value(val, min, max, "value")) {
                free(copy);
                return -1;
            }

            int idx = val - offset;
            if (idx >= 0 && idx < size) {
                arr[idx] = 1;
            }
        }

        token = strtok_r(NULL, ",", &saveptr);
    }

    free(copy);
    return 0;
}

int parse_cron_line(const char *line, cron_job *job) {
    if (!line || !job) {
        log_msg("NULL pointer in parse_cron_line");
        return -1;
    }

    memset(job, 0, sizeof(cron_job));

    char buf[512];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *newline = strchr(buf, '\n');
    if (newline)
        *newline = '\0';
    newline = strchr(buf, '\r');
    if (newline)
        *newline = '\0';

    char *fields[5];
    int field_count = 0;
    char *saveptr;
    char *token = strtok_r(buf, " \t", &saveptr);

    while (token && field_count < 5) {
        if (strlen(token) > 0) {
            fields[field_count++] = token;
        }
        token = strtok_r(NULL, " \t", &saveptr);
    }

    if (field_count < 5) {
        log_msg("Not enough fields in cron line (need 5 time fields + command)");
        return -1;
    }

    if (!token) {
        log_msg("No command specified in cron line");
        return -1;
    }

    size_t cmd_len = 0;
    while (token) {
        if (cmd_len > 0 && cmd_len < sizeof(job->command) - 1) {
            job->command[cmd_len++] = ' ';
        }

        size_t token_len = strlen(token);
        if (cmd_len + token_len < sizeof(job->command) - 1) {
            strcpy(job->command + cmd_len, token);
            cmd_len += token_len;
        } else {
            log_msg("Command too long");
            return -1;
        }

        token = strtok_r(NULL, " \t", &saveptr);
    }

    if (cmd_len == 0) {
        log_msg("Empty command in cron line");
        return -1;
    }

    if (parse_field(fields[0], job->minutes, 0, 59, 0, 60) != 0) {
        log_msg("Failed to parse minute field");
        return -1;
    }

    if (parse_field(fields[1], job->hours, 0, 23, 0, 24) != 0) {
        log_msg("Failed to parse hour field");
        return -1;
    }

    if (parse_field(fields[2], job->days, 1, 31, 1, 31) != 0) {
        log_msg("Failed to parse day field");
        return -1;
    }

    if (parse_field(fields[3], job->months, 1, 12, 1, 12) != 0) {
        log_msg("Failed to parse month field");
        return -1;
    }

    if (parse_field(fields[4], job->daysofweek, 0, 7, 0, 7) != 0) {
        log_msg("Failed to parse weekday field");
        return -1;
    }

    if (job->daysofweek[6]) {
        job->daysofweek[0] = 1;
        job->daysofweek[6] = 0;
    }

    return 0;
}

int time_matches(const cron_job *job, const struct tm *tm) {
    if (!job || !tm) {
        return 0;
    }

    // Verificar minuto (0-59)
    if (!job->minutes[tm->tm_min]) {
        return 0;
    }

    // Verificar hora (0-23)
    if (!job->hours[tm->tm_hour]) {
        return 0;
    }

    // Verificar mes (tm_mon: 0-11, array: 0-11 indexado por 1-12)
    if (!job->months[tm->tm_mon]) {
        return 0;
    }

    // Verificar día del mes vs día de la semana
    // Si ambos están especificados (no todos marcados), usar OR
    int day_match = job->days[tm->tm_mday - 1];       // tm_mday: 1-31, array: 0-30
    int weekday_match = job->daysofweek[tm->tm_wday]; // tm_wday: 0-6

    // Verificar si day está como * (todos marcados)
    int day_is_wildcard = 1;
    for (int i = 0; i < 31; i++) {
        if (!job->days[i]) {
            day_is_wildcard = 0;
            break;
        }
    }

    // Verificar si weekday está como * (todos marcados)
    int weekday_is_wildcard = 1;
    for (int i = 0; i < 7; i++) {
        if (!job->daysofweek[i]) {
            weekday_is_wildcard = 0;
            break;
        }
    }

    // Lógica según estándar cron:
    // - Si ambos son *, coinciden
    // - Si uno es * y otro no, verificar el específico
    // - Si ambos son específicos, usar OR (cualquiera coincide)
    if (day_is_wildcard && weekday_is_wildcard) {
        return 1; // Ambos son *, siempre coincide
    } else if (day_is_wildcard) {
        return weekday_match; // Solo verificar weekday
    } else if (weekday_is_wildcard) {
        return day_match; // Solo verificar day
    } else {
        return day_match || weekday_match; // Verificar ambos con OR
    }
}

/**
 * Función de utilidad para debugging: imprime un job parseado
 */
void print_cron_job(const cron_job *job) {
    if (!job)
        return;

    printf("Command: %s\n", job->command);

    printf("Minutes: ");
    for (int i = 0; i < 60; i++) {
        if (job->minutes[i])
            printf("%d ", i);
    }
    printf("\n");

    printf("Hours: ");
    for (int i = 0; i < 24; i++) {
        if (job->hours[i])
            printf("%d ", i);
    }
    printf("\n");

    printf("Days: ");
    for (int i = 0; i < 31; i++) {
        if (job->days[i])
            printf("%d ", i + 1);
    }
    printf("\n");

    printf("Months: ");
    for (int i = 0; i < 12; i++) {
        if (job->months[i])
            printf("%d ", i + 1);
    }
    printf("\n");

    printf("Weekdays: ");
    for (int i = 0; i < 7; i++) {
        if (job->daysofweek[i])
            printf("%d ", i);
    }
    printf("\n");
}
