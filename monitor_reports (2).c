#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

#define PID_FILE ".monitor_pid"


static void write_msg(const char *prefix, const char *text) {
    char buf[256];
    int len = snprintf(buf, sizeof(buf), "%s:%s\n", prefix, text);
    if (len > 0) write(STDOUT_FILENO, buf, len);
}

static void handle_sigusr1(int sig) {
    (void)sig;
    write_msg("REPORT", "new report added");
}

static void handle_sigint(int sig) {
    (void)sig;
    write_msg("END", "shutting down");
    unlink(PID_FILE);
    _exit(0);
}

static void handle_sigterm(int sig) {
    (void)sig;
    write_msg("END", "terminated");
    unlink(PID_FILE);
    _exit(0);
}

int main(void) {
    /* ---- Check if another monitor is already running ---- */
    int check_fd = open(PID_FILE, O_RDONLY);
    if (check_fd >= 0) {
        char existing[32];
        ssize_t n = read(check_fd, existing, sizeof(existing) - 1);
        close(check_fd);
        if (n > 0) {
            existing[n] = '\0';
            pid_t existing_pid = (pid_t)atoi(existing);
            /* Send signal 0 to check if the process is alive */
            if (existing_pid > 0 && kill(existing_pid, 0) == 0) {
                char errmsg[64];
                snprintf(errmsg, sizeof(errmsg), "monitor already running with PID %d", (int)existing_pid);
                write_msg("ERROR", errmsg);
                fsync(STDOUT_FILENO);
                return 1;
            }
        }
        unlink(PID_FILE);
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

    char startmsg[64];
    snprintf(startmsg, sizeof(startmsg), "started with PID %d", (int)getpid());
    write_msg("INFO", startmsg);
    fsync(STDOUT_FILENO);

    struct sigaction sa_usr1, sa_int, sa_term;

    memset(&sa_usr1, 0, sizeof(sa_usr1));
    sa_usr1.sa_handler = handle_sigusr1;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa_usr1, NULL) < 0) {
        perror("sigaction SIGUSR1");
        unlink(PID_FILE);
        return 1;
    }

    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    if (sigaction(SIGINT, &sa_int, NULL) < 0) {
        perror("sigaction SIGINT");
        unlink(PID_FILE);
        return 1;
    }

    memset(&sa_term, 0, sizeof(sa_term));
    sa_term.sa_handler = handle_sigterm;
    sigemptyset(&sa_term.sa_mask);
    sa_term.sa_flags = 0;
    sigaction(SIGTERM, &sa_term, NULL);

    while (1) {
        pause();
    }

    unlink(PID_FILE);
    return 0;
}
