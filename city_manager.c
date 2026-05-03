#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_CONDITIONS 10

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
    char field[32];
    char op[4];
    char value[100];
} Condition;


//Helper functions

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

void ensure_district_files_exist(const char *district_name) {
    char path[256];
    mkdir(district_name, 0750);
    
    snprintf(path, sizeof(path), "%s/reports.dat", district_name);
    int fd = open(path, O_CREAT | O_RDWR, 0664);
    if (fd != -1) { close(fd); chmod(path, 0664); }

    snprintf(path, sizeof(path), "%s/district.cfg", district_name);
    int fd_cfg = open(path, O_CREAT | O_WRONLY | O_EXCL, 0640);
    if (fd_cfg != -1) {
        write(fd_cfg, "escalation_threshold=3\n", 23);
        close(fd_cfg);
        chmod(path, 0640);
    }

    char linkpath[256];
    snprintf(linkpath, sizeof(linkpath), "active_reports-%s", district_name);

    symlink(path, linkpath);
}

int get_next_report_id(const char *district_name) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district_name);
    
    int fd = open(path, O_RDONLY);
    if (fd == -1) return 1;

    off_t size = lseek(fd, 0, SEEK_END);
    if (size == 0) {
        close(fd);
        return 1;
    }
    
    lseek(fd, -sizeof(ReportRecord), SEEK_END);
    
    ReportRecord last_record;
    if (read(fd, &last_record, sizeof(ReportRecord)) == sizeof(ReportRecord)) {
        close(fd);
        return last_record.report_id + 1; 
    }

    close(fd);
    return 1;
}

void prompt_report_details(ReportRecord *record) {
    printf("X: "); scanf("%f", &record->x);
    printf("Y: "); scanf("%f", &record->y);
    printf("Category (road/lighting/flooding/other): "); scanf("%29s", record->issue_category);
    printf("Severity level (1/2/3):"); scanf("%d", &record->severity_level);
    
    int c; while ((c = getchar()) != '\n' && c != EOF);
    
    printf("Description:");
    fgets(record->description, sizeof(record->description), stdin);
    record->description[strcspn(record->description, "\n")] = 0;
}


//FIlter logic (AI made) + refactoring

int parse_condition(const char *input, char *field, char *op, char *value) {
    if (sscanf(input, "%31[^:]:%3[^:]:%99[^\n]", field, op, value) == 3) {
        return 1;
    }
    return 0;
}

int match_condition(ReportRecord *r, const char *field, const char *op, const char *value) {
    if (strcmp(field, "severity") == 0 || strcmp(field, "timestamp") == 0) {
        long val_rec = (strcmp(field, "severity") == 0) ? r->severity_level : (long)r->timestamp;
        long val_cond = atol(value);

        if (strcmp(op, "==") == 0) return val_rec == val_cond;
        if (strcmp(op, "!=") == 0) return val_rec != val_cond;
        if (strcmp(op, "<") == 0)  return val_rec < val_cond;
        if (strcmp(op, "<=") == 0) return val_rec <= val_cond;
        if (strcmp(op, ">") == 0)  return val_rec > val_cond;
        if (strcmp(op, ">=") == 0) return val_rec >= val_cond;
    }

    else if (strcmp(field, "category") == 0 || strcmp(field, "inspector") == 0) {
        const char *str_rec = (strcmp(field, "category") == 0) ? r->issue_category : r->inspector_name;
        int cmp = strcmp(str_rec, value);

        if (strcmp(op, "==") == 0) return cmp == 0;
        if (strcmp(op, "!=") == 0) return cmp != 0;
    }
    return 0;
}

int match_all_conditions(ReportRecord *rec, Condition *conds, int num_conds) {
    for (int i = 0; i < num_conds; i++) {
        if (!match_condition(rec, conds[i].field, conds[i].op, conds[i].value)) return 0;
    }
    return 1;
}


//Main functions

void add_report(const char *district_name, const char *role, const char *user) {
    ensure_district_files_exist(district_name);

    ReportRecord record;
    memset(&record, 0, sizeof(ReportRecord)); 
    record.report_id = get_next_report_id(district_name);
    strncpy(record.inspector_name, user, sizeof(record.inspector_name) - 1);
    record.timestamp = time(NULL);

    prompt_report_details(&record);

    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district_name);
    int fd = open(path, O_WRONLY | O_APPEND);
    
    if (fd != -1) {
        write(fd, &record, sizeof(ReportRecord));
        close(fd);
        
        char log_msg[256];
        int pid_fd = open(".monitor_pid", O_RDONLY);
        
        if (pid_fd == -1) {
            snprintf(log_msg, sizeof(log_msg), "add (Warning: monitor no-notified, file .monitor_pid missing!)");
        } else {
            char pid_buf[32] = {0};
            read(pid_fd, pid_buf, sizeof(pid_buf) - 1);
            close(pid_fd);
            
            pid_t monitor_pid = atoi(pid_buf);
            
            if (kill(monitor_pid, SIGUSR1) == 0) {
                snprintf(log_msg, sizeof(log_msg), "add (Monitor notified successfully with SIGUSR1)");
            } else {
                snprintf(log_msg, sizeof(log_msg), "add (Warning: monitor no-notified, function kill() issue)");
            }
        }
        
        log_action(district_name, role, user, log_msg);
        printf("Raport added successfully!\n");
    }
}

void list_reports(const char *district_name, const char *role, const char *user) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district_name);
    
    int fd = open(path, O_RDONLY);
    if (fd == -1) { printf("No raports.\n"); return; }

    ReportRecord rec;
    printf("%-5s | %-15s | %-10s | %-10s\n", "ID", "Inspector", "Category", "Severity");
    printf("----------------------------------------------------\n");
    
    while (read(fd, &rec, sizeof(ReportRecord)) == sizeof(ReportRecord)) {
        printf("%-5d | %-15s | %-10s | %-10d\n", rec.report_id, rec.inspector_name, rec.issue_category, rec.severity_level);
    }
    close(fd);
    log_action(district_name, role, user, "list");
}

// AI-Made + refactoring
void filter_reports(const char *district_name, Condition *conds, int num_conds, const char *role, const char *user) {
    char path[256]; snprintf(path, sizeof(path), "%s/reports.dat", district_name);
    int fd = open(path, O_RDONLY);
    if (fd == -1) { printf("No raport exists.\n"); return; }

    ReportRecord rec; int found = 0;
    printf("Filter results:\n%-5s | %-15s | %-10s | %-10s\n", "ID", "Inspector", "Category", "Severity");
    printf("----------------------------------------------------\n");
    
    while (read(fd, &rec, sizeof(ReportRecord)) == sizeof(ReportRecord)) {
        if (match_all_conditions(&rec, conds, num_conds)) {
            printf("%-5d | %-15s | %-10s | %-10d\n", rec.report_id, rec.inspector_name, rec.issue_category, rec.severity_level);
            found = 1;
        }
    }
    close(fd);
    if (!found) printf("No raport which meet criteria.\n");
    log_action(district_name, role, user, "filter");
}

void view_report(const char *district_name, int target_id, const char *role, const char *user) {
    char path[256]; snprintf(path, sizeof(path), "%s/reports.dat", district_name);
    int fd = open(path, O_RDONLY);
    if (fd == -1) return;

    ReportRecord rec; int found = 0;
    while (read(fd, &rec, sizeof(ReportRecord)) == sizeof(ReportRecord)) {
        if (rec.report_id == target_id) {
            found = 1;
            printf("ID: %d\nInspector: %s\nLocation: %.2f, %.2f\nCategory: %s\nSeverity: %d\nDescription: %s\nDate: %s", 
                   rec.report_id, rec.inspector_name, rec.x, rec.y, rec.issue_category, rec.severity_level, rec.description, ctime(&rec.timestamp));
            break;
        }
    }
    close(fd);
    if (found) log_action(district_name, role, user, "view");
    else printf("Raport not found.\n");
}

void remove_report(const char *district_name, int target_id, const char *role, const char *user) {
    if (strcmp(role, "manager") != 0) { 
        printf("ERROR: Only manager can remove!\n"); 
        return; 
    }
    
    char path[256]; 
    snprintf(path, sizeof(path), "%s/reports.dat", district_name);
    
    int fd = open(path, O_RDWR);
    if (fd == -1) {
        perror("ERROR to open file for removing");
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
        close(fd); 
        printf("Rapor with ID %d not found.\n", target_id); 
        return; 
    }

    off_t read_pos = target_pos + sizeof(ReportRecord);
    off_t write_pos = target_pos;

    while (1) {
        lseek(fd, read_pos, SEEK_SET);
        if (read(fd, &rec, sizeof(ReportRecord)) != sizeof(ReportRecord)) break; 
        
        lseek(fd, write_pos, SEEK_SET);
        write(fd, &rec, sizeof(ReportRecord));
        
        read_pos += sizeof(ReportRecord); 
        write_pos += sizeof(ReportRecord);
    }
    
    if (ftruncate(fd, write_pos) == 0) {
        printf("Raport %d successfully removed!\n", target_id);
        log_action(district_name, role, user, "remove_report");
    } else {
        perror("ERROR at function ftruncate");
    }
    
    close(fd);
}

void remove_district(const char *district_name, const char *role, const char *user) {
    if (strcmp(role, "manager") != 0) {
        printf("Error: Only managers can remove districts!\n");
        return;
    }

    if (strchr(district_name, '/') != NULL || strcmp(district_name, ".") == 0 || strcmp(district_name, "..") == 0) {
        printf("Error: Invalid name of district.\n");
        return;
    }

    char linkpath[256];
    snprintf(linkpath, sizeof(linkpath), "active_reports-%s", district_name);
    unlink(linkpath);

    pid_t pid = fork();
    
    if (pid == -1) {
        perror("Critical error at fork()");
    } else if (pid == 0) {
        execlp("rm", "rm", "-rf", district_name, NULL);
        
        perror("Error: execlp issue");
        exit(1);
    } else {
        int status;
        waitpid(pid, &status, 0);
        printf("Command processed: Distric '%s' and all contents removed.\n", district_name);
    }
}

void update_threshold(const char *district_name, int noul_prag, const char *role, const char *user) {
    if (strcmp(role, "manager") != 0) { printf("ERROR: Only for managers!\n"); return; }
    
    char path[256]; snprintf(path, sizeof(path), "%s/district.cfg", district_name);
    struct stat st;
    
    if (stat(path, &st) == -1 || (st.st_mode & 0777) != 0640) {
        printf("ERROR: No permissions to modify or file not exists!\n");
        return;
    }

    int fd = open(path, O_WRONLY | O_TRUNC);
    if (fd != -1) {
        char buffer[128];
        int len = snprintf(buffer, sizeof(buffer), "escalation_threshold=%d\n", noul_prag);
        write(fd, buffer, len);
        close(fd);
        printf("Treeshold updated.\n");
        log_action(district_name, role, user, "update_threshold");
    }
}

void check_links() {
    DIR *dir = opendir(".");
    if (!dir) {
        perror("Error! Can't open current directory!'");
        return;
    }

    struct dirent *entry;
    printf("Scanning symlinks in current directory...\n");
    printf("----------------------------------------------------\n");

    while ((entry = readdir(dir)) != NULL) {
        
        if (strncmp(entry->d_name, "active_reports-", 15) == 0) {
            struct stat lst;
            
            if (lstat(entry->d_name, &lst) == 0) {
                
                if (S_ISLNK(lst.st_mode)) {
                    
                    struct stat st;
                    if (stat(entry->d_name, &st) == -1) {
                        printf("[WARNING] Dangling link detected: '%s'. No destination!\n", entry->d_name);
                    } else {
                        printf("[OK] Valid link: '%s' pointing to existing directory.\n", entry->d_name);
                    }
                }
            }
        }
    }
    closedir(dir);
    printf("Scan completed.\n");
}

int main(int argc, char *argv[]) {
    umask(0);

    char *role = "unknown", *user = "unknown", *action = NULL, *target_district = NULL;
    int target_id = -1, target_value = -1;
    
    Condition conditions[MAX_CONDITIONS];
    int num_conditions = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) role = argv[++i];
        else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) user = argv[++i];
        else if (strcmp(argv[i], "--add") == 0 && i + 1 < argc) { action = "add"; target_district = argv[++i]; }
        else if (strcmp(argv[i], "--list") == 0 && i + 1 < argc) { action = "list"; target_district = argv[++i]; }
        else if (strcmp(argv[i], "--view") == 0 && i + 2 < argc) { action = "view"; target_district = argv[++i]; target_id = atoi(argv[++i]); }
        else if (strcmp(argv[i], "--remove_report") == 0 && i + 2 < argc) { action = "remove_report"; target_district = argv[++i]; target_id = atoi(argv[++i]); }
        else if (strcmp(argv[i], "--update_threshold") == 0 && i + 2 < argc) { action = "update_threshold"; target_district = argv[++i]; target_value = atoi(argv[++i]); }
        else if (strcmp(argv[i], "--filter") == 0 && i + 1 < argc) { 
            action = "filter"; 
            target_district = argv[++i];
            
            while (i + 1 < argc && strncmp(argv[i+1], "--", 2) != 0 && num_conditions < MAX_CONDITIONS) {
                if (parse_condition(argv[++i], conditions[num_conditions].field, conditions[num_conditions].op, conditions[num_conditions].value)) {
                    num_conditions++;
                }
            }
        }
        else if (strcmp(argv[i], "--check_links") == 0) { 
            action = "check_links"; 
        }
        else if (strcmp(argv[i], "--remove_district") == 0 && i + 1 < argc) { 
            action = "remove_district"; 
            target_district = argv[++i]; 
        }
    }

    if (!action) {
        printf("Specify a valid command.\n");
        return 1;
    }

    if (strcmp(action, "add") == 0) add_report(target_district, role, user);
    else if (strcmp(action, "list") == 0) list_reports(target_district, role, user);
    else if (strcmp(action, "view") == 0) view_report(target_district, target_id, role, user);
    else if (strcmp(action, "remove_report") == 0) remove_report(target_district, target_id, role, user);
    else if (strcmp(action, "update_threshold") == 0) update_threshold(target_district, target_value, role, user);
    else if (strcmp(action, "filter") == 0) filter_reports(target_district, conditions, num_conditions, role, user);
    else if (strcmp(action, "check_links") == 0) check_links();
    else if (strcmp(action, "remove_district") == 0) remove_district(target_district, role, user);

    return 0;
}
