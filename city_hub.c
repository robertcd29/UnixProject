#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>

#define PID_FILE ".monitor_pid"

static pid_t mon_hub_pid = -1;


void start_monitor()
{
    if (mon_hub_pid > 0 && kill(mon_hub_pid, 0) == 0) {
        printf("Monitor is already running\n");
        return;
    }
    pid_t pid = fork();
    if(pid < 0)
    {
        printf("eroare\n");
        exit(-1);
    }
    if(pid == 0)
    {
        int pipefd[2];

        if(pipe(pipefd) < 0)
        {
            printf("eroare\n");
            exit(-1);
        }

        pid_t monitor_pid = fork();
        if(monitor_pid < 0)
        {
            perror("eroare\n");
            exit(-1);
        }

        if(monitor_pid == 0)
        {
            close(pipefd[0]);                    
            dup2(pipefd[1], STDOUT_FILENO);      
            close(pipefd[1]);

            execl("./monitor_reports", "monitor_reports", NULL);
            exit(1);
        }

        close(pipefd[1]);
        char buf[512];
        char msg[512];
        int msg_len = 0;
        while(1)
        {
            int n = read(pipefd[0], buf, sizeof(buf));
            if(n <= 0)
            {
                printf("Pipe ended\n");
                break;
            }

            for(int i = 0; i < n; i++)
            {
                if(buf[i] == '\n') break;
                msg[msg_len++] = buf[i];
            }
            msg[msg_len] = '\0';
            printf("Monitor: %s\n", msg);
            fflush(stdout);
        }

        close(pipefd[0]);
        waitpid(monitor_pid, NULL, 0);
    }
    mon_hub_pid = pid;
    printf("mon hub started - PID %d\n", (int)pid);

}


int main(void)
{
    start_monitor();

    return 0;
}