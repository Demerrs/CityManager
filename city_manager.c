#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

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

void log_action(const char *district, const char *role, const char *user, const char *action) {
    char path[256];
    snprintf(path, sizeof(path), "%s/logged_district", district);
    
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) return;

    time_t now = time(NULL);
    char buffer[512];
    int len = snprintf(buffer, sizeof(buffer), "%ld\t%s\t%s\t%s\n", (long)now, user, role, action);
    
    write(fd, buffer, len);
    close(fd);
}

void add_report(const char *district_name, const char *role, const char *user) {
    char path[256];
    
    mkdir(district_name, 0750);

    snprintf(path, sizeof(path), "%s/reports.dat", district_name);
    int fd = open(path, O_CREAT | O_RDWR, 0664);
    if (fd != -1) {
        close(fd);
        chmod(path, 0664); 
    }

    snprintf(path, sizeof(path), "%s/district.cfg", district_name);

    int fd_cfg = open(path, O_CREAT | O_WRONLY | O_EXCL, 0640);
    if (fd_cfg != -1) {
        const char *default_cfg = "escalation_threshold=3\n";
        write(fd_cfg, default_cfg, strlen(default_cfg));
        close(fd_cfg);
        chmod(path, 0640);
    }

    int report_id = 1;
    snprintf(path, sizeof(path), "%s/reports.dat", district_name);
    fd = open(path, O_RDONLY);
    if (fd != -1) {
        off_t size = lseek(fd, 0, SEEK_END); 
        report_id = (size / sizeof(ReportRecord)) + 1;
        close(fd);
    }

    ReportRecord record;
    memset(&record, 0, sizeof(ReportRecord)); 

    record.report_id = report_id;
    strncpy(record.inspector_name, user, sizeof(record.inspector_name) - 1);
    record.timestamp = time(NULL);

    printf("X: "); scanf("%f", &record.x);
    printf("Y: "); scanf("%f", &record.y);
    printf("Category (road/lighting/flooding/other): "); scanf("%29s", record.issue_category);
    printf("Severity level (1/2/3):"); scanf("%d", &record.severity_level);
    
    int c; while ((c = getchar()) != '\n' && c != EOF);
    
    printf("Description:");
    fgets(record.description, sizeof(record.description), stdin);
    record.description[strcspn(record.description, "\n")] = 0;

    fd = open(path, O_WRONLY | O_APPEND);
    if (fd != -1) {
        write(fd, &record, sizeof(ReportRecord));
        close(fd);
        log_action(district_name, role, user, "add");
        printf("Raport was added successfully.\n");
    } else {
        perror("ERROR: Failed to open reports.dat for writing");
    }
}

void list_reports(const char *district_name, const char *role, const char *user) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district_name);
    
    struct stat st;
    if (stat(path, &st) == 0) {
        printf("--- File Info ---\nDimension: %ld bytes\nPermisions: %o\n\n", st.st_size, st.st_mode & 0777);
    }

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        printf("No raport founds in this district! %s.\n", district_name);
        return;
    }

    ReportRecord rec;
    printf("Raport List:\n");
    printf("%-5s | %-15s | %-10s | %-10s\n", "ID", "Inspector", "Category", "Severity");
    printf("----------------------------------------------------\n");
    
    while (read(fd, &rec, sizeof(ReportRecord)) == sizeof(ReportRecord)) {
        printf("%-5d | %-15s | %-10s | %-10d\n", rec.report_id, rec.inspector_name, rec.issue_category, rec.severity_level);
    }
    
    close(fd);
    log_action(district_name, role, user, "list");
}

void view_report(const char *district_name, int target_id, const char *role, const char *user) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district_name);
    
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        printf("ERROR: Failed to open raport file.\n");
        return;
    }

    ReportRecord rec;
    int found = 0;
    while (read(fd, &rec, sizeof(ReportRecord)) == sizeof(ReportRecord)) {
        if (rec.report_id == target_id) {
            found = 1;
            printf("--- Details raport ID %d ---\n", rec.report_id);
            printf("Inspector: %s\n", rec.inspector_name);
            printf("Location: X: %.2f, Y: %.2f\n", rec.x, rec.y);
            printf("Category: %s\n", rec.issue_category);
            printf("Severity: %d\n", rec.severity_level);
            printf("Description: %s\n", rec.description);
            printf("Timestamp: %s", ctime(&rec.timestamp));
            break;
        }
    }
    close(fd);
    
    if (!found) printf("Raport with ID %d not found.\n", target_id);
    else log_action(district_name, role, user, "view");
}

void remove_report(const char *district_name, int target_id, const char *role, const char *user) {
    if (strcmp(role, "manager") != 0) {
        printf("ERROR: Only manager can remove raport files.\n");
        return;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district_name);
    
    int fd = open(path, O_RDWR);
    if (fd == -1) {
        perror("Error to open the file");
        return;
    }

    ReportRecord rec;
    off_t target_pos = -1;

    while (read(fd, &rec, sizeof(ReportRecord)) == sizeof(ReportRecord)) {
        if (rec.report_id == target_id) {
            target_pos = lseek(fd, 0, SEEK_CUR) - sizeof(ReportRecord);
            break;
        }
    }

    if (target_pos == -1) {
        printf("Raport with ID %d not found.\n", target_id);
        close(fd);
        return;
    }

    off_t read_pos = target_pos + sizeof(ReportRecord);
    off_t write_pos = target_pos;

    while (1) {
        lseek(fd, read_pos, SEEK_SET);
        int bytes_read = read(fd, &rec, sizeof(ReportRecord));
        if (bytes_read <= 0) break; 

        lseek(fd, write_pos, SEEK_SET);
        write(fd, &rec, sizeof(ReportRecord));
        
        read_pos += sizeof(ReportRecord);
        write_pos += sizeof(ReportRecord);
    }

    if (ftruncate(fd, write_pos) == 0) {
        printf("Raport %d successfully removed.\n", target_id);
        char action_str[50];
        snprintf(action_str, sizeof(action_str), "remove_report %d", target_id);
        log_action(district_name, role, user, action_str);
    }

    close(fd);
}

void update_threshold(const char *district_name, int noul_prag, const char *role, const char *user) {
    if (strcmp(role, "manager") != 0) {
        printf("ERROR: Only manager can set treeshold.\n");
        return;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/district.cfg", district_name);

    struct stat st;
    if (stat(path, &st) == -1) {
        perror("ERROR stat");
        return;
    }

    if ((st.st_mode & 0777) != 0640) {
        printf("Diagnostic: Permisions are %o, not 640! Modification declined.\n", st.st_mode & 0777);
        return;
    }

    int fd = open(path, O_WRONLY | O_TRUNC);
    if (fd != -1) {
        char buffer[128];
        int len = snprintf(buffer, sizeof(buffer), "escalation_threshold=%d\n", noul_prag);
        write(fd, buffer, len);
        close(fd);
        
        printf("Treeshold updated to %d.\n", noul_prag);
        char action_str[50];
        snprintf(action_str, sizeof(action_str), "update_threshold %d", noul_prag);
        log_action(district_name, role, user, action_str);
    }
}

int main(int argc, char *argv[]) {
    umask(0); 

    char *role = "unknown";
    char *user = "unknown";
    char *action = NULL;
    char *target_district = NULL;
    int target_id = -1;
    int target_value = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) role = argv[++i];
        else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) user = argv[++i];
        else if (strcmp(argv[i], "--add") == 0 && i + 1 < argc) { action = "add"; target_district = argv[++i]; }
        else if (strcmp(argv[i], "--list") == 0 && i + 1 < argc) { action = "list"; target_district = argv[++i]; }
        else if (strcmp(argv[i], "--view") == 0 && i + 2 < argc) { action = "view"; target_district = argv[++i]; target_id = atoi(argv[++i]); }
        else if (strcmp(argv[i], "--remove_report") == 0 && i + 2 < argc) { action = "remove_report"; target_district = argv[++i]; target_id = atoi(argv[++i]); }
        else if (strcmp(argv[i], "--update_threshold") == 0 && i + 2 < argc) { action = "update_threshold"; target_district = argv[++i]; target_value = atoi(argv[++i]); }
    }

    if (!action) return 1;

    if (strcmp(action, "add") == 0) add_report(target_district, role, user);
    else if (strcmp(action, "list") == 0) list_reports(target_district, role, user);
    else if (strcmp(action, "view") == 0) view_report(target_district, target_id, role, user);
    else if (strcmp(action, "remove_report") == 0) remove_report(target_district, target_id, role, user);
    else if (strcmp(action, "update_threshold") == 0) update_threshold(target_district, target_value, role, user);

    return 0;
}
