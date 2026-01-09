#ifndef CRONTAB_PARSER_H
#define CRONTAB_PARSER_H

#include <time.h>

typedef struct {
    int minutes[60];   // 0-59, 1 if allowed
    int hours[24];     // 0-23
    int days[31];      // 1-31
    int months[12];    // 0-11 (Jan=0)
    int daysofweek[7]; // 0-6 (Sun=0)
    char command[512]; // the command to run limit to 256 characters

    volatile int is_running; // 1 if the job is running, 0 otherwise
    time_t last_run;         // last time the job was run
} cron_job;

int parse_cron_line(const char *line, cron_job *job);

int time_matches(const cron_job *job, const struct tm *tm);

#endif // CRONTAB_PARSER_H
