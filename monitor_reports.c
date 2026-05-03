#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>

volatile sig_atomic_t keep_running = 1;

void handle_sigint(int sig) {
    keep_running = 0;
}

void handle_sigusr1(int sig) {
    char msg[] = "\n[Monitor] Alert: New report added to system!\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
}

int main() {
    struct sigaction sa_int, sa_usr1;

    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("Error occured in configuration of SIGINT");
        return 1;
    }

    sa_usr1.sa_handler = handle_sigusr1;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa_usr1, NULL) == -1) {
        perror("Error occured in configuration of SIGUSR1");
        return 1;
    }

    int fd = open(".monitor_pid", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Error in creating .monitor_pid");
        return 1;
    }

    char pid_str[32];
    int len = snprintf(pid_str, sizeof(pid_str), "%d\n", getpid());
    write(fd, pid_str, len);
    close(fd);

    printf("[Monitor] Started succesfully! PID: %d. Waiting for semnals...\n", getpid());

    while (keep_running) {
        pause();
    }

    printf("\n[Monitor] Signal SIGINT found. Closing application and removing .monitor_pid...\n");
    
    if (unlink(".monitor_pid") == -1) {
        perror("Errr to removing .monitor_pid");
    }

    return 0;
}
