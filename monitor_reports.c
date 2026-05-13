#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

#define PID_FILE ".monitor_pid"

static void handle_sigusr1(int sig) {
    (void)sig;
    const char *msg = "new report added\n";
    write(STDOUT_FILENO, msg, strlen(msg));
}

static void handle_sigint(int sig) {
    (void)sig;
    const char *msg = "shutting down\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    unlink(PID_FILE);
    _exit(0);
}

int main(void) {
    int check_fd = open(PID_FILE, O_RDONLY);
    if(check_fd >= 0)
    {
        char exist[64];
        ssize_t n = read(check_fd, exist, sizeof(exist) - 1);
        close(check_fd);
        if(n > 0)
        {
            exist[n] = '\0';
            pid_t existing_pid = (pid_t)atoi(exist);
            if(existing_pid > 0 && kill(existing_pid, 0) == 0)
            {
                printf("Existing pid %d\n", existing_pid);
                return 0;
            }
        }
    }

    int fd = open(PID_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open .monitor_pid");
        return 1;
    }

    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d\n", (int)getpid());
    write(fd, pid_str, strlen(pid_str));
    close(fd);

    printf("monitor_reports: started (PID %d)\n", (int)getpid());
    fflush(stdout);

    struct sigaction sa_usr1, sa_int;

    memset(&sa_usr1, 0, sizeof(sa_usr1));
    sa_usr1.sa_handler = handle_sigusr1;
    if (sigaction(SIGUSR1, &sa_usr1, NULL) < 0) {
        perror("sigusr");
        unlink(PID_FILE);
        return 1;
    }

    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    if (sigaction(SIGINT, &sa_int, NULL) < 0) {
        perror("sigint");
        unlink(PID_FILE);
        return 1;
    }

    while (1) {
        pause();
    }

    unlink(PID_FILE);
    return 0;
}
