#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>

#define MAX        512
#define MAX_ARGS   64
#define PID_FILE   ".monitor_pid"
#define SCORER_BIN "./scorer"

/* -----------------------------------------------------------------------
 * Globals for the hub_mon child (manages the monitor sub-process)
 * --------------------------------------------------------------------- */
static pid_t g_hub_mon_pid = -1;  /* PID of the hub_mon child we spawned  */

/* -----------------------------------------------------------------------
 * Utility: split a line into tokens (modifies the string in place)
 * --------------------------------------------------------------------- */
static int split(char *line, char *argv[], int max_args) {
    int n = 0;
    char *tok = strtok(line, " \t\r\n");
    while (tok && n < max_args - 1) {
        argv[n++] = tok;
        tok = strtok(NULL, " \t\r\n");
    }
    argv[n] = NULL;
    return n;
}

/* -----------------------------------------------------------------------
 * hub_mon process
 *
 * This is the child forked by start_monitor.
 * It:
 *   1. Creates a pipe.
 *   2. Forks monitor_reports, wiring its stdout to the write-end of the pipe.
 *   3. Reads lines from the read-end and forwards them to the user (its own
 *      stdout, i.e. the hub's terminal).
 *   4. When the pipe is closed (monitor exited) prints a notice and exits.
 * --------------------------------------------------------------------- */
static void run_hub_mon(void) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("hub_mon: pipe");
        _exit(1);
    }

    pid_t monitor_pid = fork();
    if (monitor_pid < 0) {
        perror("hub_mon: fork monitor");
        _exit(1);
    }

    if (monitor_pid == 0) {
        /* ---- monitor_reports child ---- */
        close(pipefd[0]);                    /* close read end   */
        dup2(pipefd[1], STDOUT_FILENO);      /* stdout -> pipe   */
        close(pipefd[1]);

        execl("./monitor_reports", "monitor_reports", NULL);
        perror("hub_mon: execl monitor_reports");
        _exit(1);
    }

    /* ---- hub_mon (parent of monitor) continues here ---- */
    close(pipefd[1]);   /* close write end – only monitor writes  */

    /* Read and forward messages from the monitor */
    char buf[256];
    char partial[512] = {0};
    int  partial_len  = 0;

    while (1) {
        ssize_t n = read(pipefd[0], buf, sizeof(buf));
        if (n <= 0) {
            /* pipe closed – monitor exited */
            printf("[hub_mon] monitor has ended for some reason\n");
            fflush(stdout);
            break;
        }

        /* Accumulate into partial buffer and flush complete lines */
        for (ssize_t i = 0; i < n; i++) {
            if (partial_len < (int)sizeof(partial) - 1) {
                partial[partial_len++] = buf[i];
            }
            if (buf[i] == '\n') {
                partial[partial_len] = '\0';

                /* Parse type prefix: "TYPE:text\n" */
                char *colon = strchr(partial, ':');
                if (colon) {
                    *colon = '\0';
                    char *type = partial;
                    char *text = colon + 1;
                    /* strip trailing newline from text */
                    text[strcspn(text, "\n")] = '\0';

                    if (strcmp(type, "ERROR") == 0) {
                        printf("[monitor] ERROR: %s\n", text);
                    } else if (strcmp(type, "REPORT") == 0) {
                        printf("[monitor] New report: %s\n", text);
                    } else if (strcmp(type, "END") == 0) {
                        printf("[monitor] %s\n", text);
                    } else {
                        printf("[monitor] %s\n", text);
                    }
                } else {
                    /* no prefix – print raw */
                    printf("[monitor] %s", partial);
                }
                fflush(stdout);
                partial_len = 0;
                memset(partial, 0, sizeof(partial));
            }
        }
    }

    close(pipefd[0]);
    /* Wait for monitor to fully exit */
    waitpid(monitor_pid, NULL, 0);
    _exit(0);
}

/* -----------------------------------------------------------------------
 * start_monitor command
 * --------------------------------------------------------------------- */
static void cmd_start_monitor(void) {
    /* If we already have a hub_mon, check if it's still alive */
    if (g_hub_mon_pid > 0 && kill(g_hub_mon_pid, 0) == 0) {
        printf("Monitor is already running (hub_mon PID %d).\n", (int)g_hub_mon_pid);
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("start_monitor: fork");
        return;
    }
    if (pid == 0) {
        /* child becomes hub_mon */
        run_hub_mon();
        _exit(0);   /* never reached */
    }

    /* parent (hub) records hub_mon pid */
    g_hub_mon_pid = pid;
    printf("hub_mon started (PID %d)\n", (int)pid);
}

/* -----------------------------------------------------------------------
 * calculate_scores command
 *
 * For each district in the argument list, forks a scorer process,
 * redirects its stdout through a pipe, collects the output, prints it.
 * --------------------------------------------------------------------- */
static void cmd_calculate_scores(char *districts[], int n_districts) {
    if (n_districts == 0) {
        printf("Usage: calculate_scores <district1> [district2 ...]\n");
        return;
    }

    printf("===== Workload Report =====\n");

    for (int i = 0; i < n_districts; i++) {
        int pipefd[2];
        if (pipe(pipefd) < 0) {
            perror("calculate_scores: pipe");
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("calculate_scores: fork");
            close(pipefd[0]);
            close(pipefd[1]);
            continue;
        }

        if (pid == 0) {
            /* ---- scorer child ---- */
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);   /* stdout -> pipe */
            close(pipefd[1]);

            execl(SCORER_BIN, "scorer", districts[i], NULL);
            perror("calculate_scores: execl scorer");
            _exit(1);
        }

        /* ---- hub (parent) reads scorer output ---- */
        close(pipefd[1]);

        char buf[256];
        ssize_t n;
        while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
            write(STDOUT_FILENO, buf, n);
        }
        close(pipefd[0]);

        int status;
        waitpid(pid, &status, 0);
    }

    printf("===========================\n");
    fflush(stdout);
}

/* -----------------------------------------------------------------------
 * SIGCHLD handler – reap any finished background children
 * --------------------------------------------------------------------- */
static void handle_sigchld(int sig) {
    (void)sig;
    int saved_errno = errno;
    pid_t pid;
    int   status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (pid == g_hub_mon_pid) {
            /* hub_mon exited on its own */
            g_hub_mon_pid = -1;
        }
    }
    errno = saved_errno;
}

/* -----------------------------------------------------------------------
 * main – interactive REPL
 * --------------------------------------------------------------------- */
int main(void) {
    /* Install SIGCHLD handler to reap children without blocking */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    printf("city_hub ready. Commands: start_monitor | calculate_scores <d1> [d2...] | exit\n");

    char line[MAX];
    while (1) {
        printf("hub> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            /* EOF */
            printf("\n");
            break;
        }

        /* Trim */
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0') continue;

        char copy[MAX];
        strncpy(copy, line, MAX - 1);
        copy[MAX - 1] = '\0';

        char *args[MAX_ARGS];
        int n = split(copy, args, MAX_ARGS);
        if (n == 0) continue;

        if (strcmp(args[0], "exit") == 0 || strcmp(args[0], "quit") == 0) {
            break;
        } else if (strcmp(args[0], "start_monitor") == 0) {
            cmd_start_monitor();
        } else if (strcmp(args[0], "calculate_scores") == 0) {
            /* remaining args are district names */
            cmd_calculate_scores(args + 1, n - 1);
        } else {
            printf("Unknown command: %s\n", args[0]);
            printf("Commands: start_monitor | calculate_scores <d1> [d2...] | exit\n");
        }
    }

    /* Clean shutdown: kill hub_mon if still running */
    if (g_hub_mon_pid > 0 && kill(g_hub_mon_pid, 0) == 0) {
        kill(g_hub_mon_pid, SIGTERM);
        waitpid(g_hub_mon_pid, NULL, 0);
    }

    printf("city_hub exiting.\n");
    return 0;
}
