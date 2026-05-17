#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct {
    int report_id;
    char inspector_name[50];
    float x;
    float y;
    char issue_category[30];
    int severity_level;
    time_t timestamp;
    char description[255];
} ReportRecord;

typedef struct {
    char name[50];
    int score;
} InspectorScore;

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", argv[1]);
    
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        char msg[256];
        int len = snprintf(msg, sizeof(msg), "District %s: Without reports (or not exists).\n", argv[1]);
        write(STDOUT_FILENO, msg, len);
        return 0;
    }

    InspectorScore inspectors[100];
    int num_inspectors = 0;
    ReportRecord rec;

    while (read(fd, &rec, sizeof(ReportRecord)) == sizeof(ReportRecord)) {
        int found = 0;
        for (int i = 0; i < num_inspectors; i++) {
            if (strcmp(inspectors[i].name, rec.inspector_name) == 0) {
                inspectors[i].score += rec.severity_level;
                found = 1; break;
            }
        }
        if (!found && num_inspectors < 100) {
            strcpy(inspectors[num_inspectors].name, rec.inspector_name);
            inspectors[num_inspectors].score = rec.severity_level;
            num_inspectors++;
        }
    }
    close(fd);

    char buffer[1024];
    int len = snprintf(buffer, sizeof(buffer), "--- Workload District: %s ---\n", argv[1]);
    write(STDOUT_FILENO, buffer, len);

    for (int i = 0; i < num_inspectors; i++) {
        len = snprintf(buffer, sizeof(buffer), "  Inspector: %-15s | Total Severity: %d\n", 
                       inspectors[i].name, inspectors[i].score);
        write(STDOUT_FILENO, buffer, len);
    }
    write(STDOUT_FILENO, "\n", 1);

    return 0;
}
