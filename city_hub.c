#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <signal.h>


#define PID_FILE ".monitor_pid"

pid_t mon_hub_pid = -1;

static void handle_sigint(int sig) {
    (void)sig;
    const char *msg = "shutting down\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    unlink(PID_FILE);
    _exit(0);
}


void start_monitor()
{
    if (mon_hub_pid > 0 && kill(mon_hub_pid, 0) == 0) {
        printf("Monitor is already running\n");
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(1);
    }

    if (pid == 0) {
        int pipefd[2];
        if (pipe(pipefd) < 0) {
            perror("pipe failed");
            exit(1);
        }

        pid_t monitor_pid = fork();
        if (monitor_pid < 0) {
            perror("fork failed");
            exit(1);
        }

        if (monitor_pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);

            execl("./monitor_report", "monitor_reports", NULL);
            perror("execl monitor_reports failed");
            exit(1);
        }

        close(pipefd[1]);
        char buf[512];
        int n;
        while ((n = read(pipefd[0], buf, sizeof(buf)-1)) > 0) {
            buf[n] = '\0';
            char *line = strtok(buf, "\n");
            while (line) {
                printf("Monitor: %s\n", line);
                line = strtok(NULL, "\n");
            }
            fflush(stdout);
        }

        close(pipefd[0]);
        waitpid(monitor_pid, NULL, 0);
        exit(0);
    }

    mon_hub_pid = pid;
    printf("mon hub started - PID %d\n", (int)pid);
    fflush(stdout);
}

void calculate_scores(char *districts[], int num_districts)
{
    for (int i = 0; i < num_districts; i++)
    {
        int pipefd[2];
        if (pipe(pipefd) < 0) {
            perror("pipe failed");
            exit(1);
        }

        pid_t scorer_pid = fork();
        if (scorer_pid < 0) {
            perror("fork failed");
            exit(1);
        }

        if (scorer_pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);

            execl("./scorer", "scorer", districts[i], NULL);
            exit(1);
        }

        close(pipefd[1]);
        char buf[256];
        int n;
        while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
            write(STDOUT_FILENO, buf, n);
        }
        close(pipefd[0]);

        int status;
        waitpid(scorer_pid, &status, 0);
    }
}

int main(int argc, char *argv[])
{
    if(strcmp(argv[1], "--calculate") == 0)
    {
        calculate_scores(argv + 2, argc - 2);
    }
    else if (strcmp(argv[1], "--start") == 0) {
        start_monitor();
        struct sigaction sa_int;
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
    }


    return 0;
}