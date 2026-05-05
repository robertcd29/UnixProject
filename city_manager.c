#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX 256

typedef struct {
    int id;
    char inspector[32];
    float latitude;
    float longitude;
    char category[32];
    int severity;
    time_t timestamp;
    char description[64];
} Report;

typedef struct {
    char role[16];
    char user[64];
    char command[32];
    char district[32];
    int report_id;
} Options;

Options parse(int argc, char *argv[]) {
    Options opt = {0};

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0)
            strcpy(opt.role, argv[++i]);
        else if (strcmp(argv[i], "--user") == 0)
            strcpy(opt.user, argv[++i]);
            else if (strcmp(argv[i], "add") == 0 || strcmp(argv[i], "--add") == 0) {
                    strcpy(opt.command, "add");
                    strcpy(opt.district, argv[i + 1]);
                    }
                    else if (strcmp(argv[i], "remove") == 0 || strcmp(argv[i], "--remove") == 0) {
                    strcpy(opt.command, "remove");
                    strcpy(opt.district, argv[i + 1]);
                    }
                    else if (strcmp(argv[i], "list") == 0 || strcmp(argv[i], "--list") == 0) {
                        strcpy(opt.command, "list");
                        strcpy(opt.district, argv[i + 1]);
                        }
    }

    return opt;
}

void create_district(char *name) {
    mkdir(name, 0750);

    char path[MAX];

    snprintf(path, MAX, "%s/reports.dat", name);
    int fd = open(path, O_CREAT | O_RDWR, 0664);
    close(fd);
    chmod(path, 0664);

    snprintf(path, MAX, "%s/district.cfg", name);
    fd = open(path, O_CREAT | O_RDWR, 0640);
    write(fd, "threshold=1\n", 12);
    close(fd);
    chmod(path, 0640);

    snprintf(path, MAX, "%s/logged_district", name);
    fd = open(path, O_CREAT | O_RDWR, 0644);
    close(fd);
    chmod(path, 0644);

    char linkname[MAX];

    snprintf(path, MAX, "%s/reports.dat", name);
    symlink(path, linkname);
}

void remove_district(char *name) {
    char path[MAX];
    snprintf(path, MAX, "active_reports-%s", name);
    unlink(path);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        char *args[] = { "rm", "-rf", name, NULL };
        execvp("rm", args);
        perror("execvp");
        exit(1);
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            printf("District '%s' removed successfully\n", name);
        } else {
            printf("Failed to remove district '%s'\n", name);
        }
    }
}

void clean_newline(char *str) {
    str[strcspn(str, "\n")] = 0;
}

void log_action(char *district, char *user, char *action, char *role, int id)
{
    char path[MAX];
    snprintf(path, MAX, "%s/logged_district", district);
    int fd = open(path, O_WRONLY | O_APPEND);
    if (fd < 0) return;

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "ID %d: %s - %s - %s\n", id, user, role, action);
    write(fd, buffer, strlen(buffer));
    close(fd);
}

void notify_monitor(char *district, char *user, char *role, int report_id) {
    int fd = open(".monitor_pid", O_RDONLY);
    if (fd < 0) {
        log_action(district, user, "monitor could not be notified", role, report_id);
        return;
    }

    char buf[32];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n <= 0) {
        log_action(district, user, "monitor could not be notified", role, report_id);
        return;
    }
    buf[n] = '\0';

    pid_t monitor_pid = (pid_t)atoi(buf);
    if (monitor_pid <= 0) {
        log_action(district, user, "monitor could not be notified", role, report_id);
        return;
    }

    if (kill(monitor_pid, SIGUSR1) < 0) {
        log_action(district, user, "monitor could not be notified", role, report_id);
    } else {
        log_action(district, user, "monitor notified", role, report_id);
    }
}

void add_report(Options opt) {
    char path[MAX];
    snprintf(path, MAX, "%s/reports.dat", opt.district);

    int fd = open(path, O_RDWR | O_APPEND);
    if (fd < 0) {
        create_district(opt.district);
        fd = open(path, O_RDWR | O_APPEND);
    }

    Report r;
    memset(&r, 0, sizeof(Report));

    printf("Latitude: "); scanf("%f", &r.latitude);
    printf("Longitude: "); scanf("%f", &r.longitude);
    printf("Category (road/lighting/flooding/other): ");
    scanf("%s", r.category);

    printf("Severity level (1-3): ");
    scanf("%d", &r.severity);

    printf("Description: ");
    getchar();
    fgets(r.description, sizeof(r.description), stdin);
    clean_newline(r.description);

    r.id = lseek(fd, 0, SEEK_END) / sizeof(Report) + 1;
    strcpy(r.inspector, opt.user);
    r.timestamp = time(NULL);

    write(fd, &r, sizeof(Report));
    close(fd);

    log_action(opt.district, opt.user, "add", opt.role, r.id);
    notify_monitor(opt.district, opt.user, opt.role, r.id);
}

void list_reports(Options opt) {
    char path[MAX];
    snprintf(path, MAX, "%s/reports.dat", opt.district);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("No district\n");
        return;
    }

    Report r;

    while (read(fd, &r, sizeof(Report)) > 0) {
        printf("ID: %d | Inspector: %s | Category: %s | Severity: %d\n",
               r.id, r.inspector, r.category, r.severity);
    }

    close(fd);
}

int main(int argc, char **argv)
{
    Options opt = parse(argc, argv);

    if (strcmp(opt.command, "add") == 0)
        add_report(opt);
    else if (strcmp(opt.command, "list") == 0)
        list_reports(opt);
    else if (strcmp(opt.command, "remove") == 0) {
        if (strcmp(opt.role, "manager") != 0) {
            printf("Permission denied: only manager can remove districts.\n");
            return 1;
        }
        remove_district(opt.district);
    }
    else
        printf("Unknown command\n");

    return 0;
}
