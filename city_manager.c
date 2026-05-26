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
#define MAX_FILTERS 8

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
    char filters[MAX_FILTERS][128];
    int filter_count;
} Options;

Options parse(int argc, char *argv[]) {
    Options opt = {0};
 
    //populam structura options cu nume rol comanda si district

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
            else if (strcmp(argv[i], "view") == 0 || strcmp(argv[i], "--view") == 0) {
                        strcpy(opt.command, "view");
                        strcpy(opt.district, argv[i + 1]);
                        opt.report_id = atoi(argv[i + 2]);
                        }
            else if (strcmp(argv[i], "remove_report") == 0 || strcmp(argv[i], "--remove_report") == 0) {
                        strcpy(opt.command, "remove_report");
                        strcpy(opt.district, argv[i + 1]);
                        opt.report_id = atoi(argv[i + 2]);
                        }
            else if (strcmp(argv[i], "update_threshold") == 0 || strcmp(argv[i], "--update_threshold") == 0) {
                        strcpy(opt.command, "update_threshold");
                        strcpy(opt.district, argv[i + 1]);
                        opt.report_id = atoi(argv[i + 2]);
                        }
            else if (strcmp(argv[i], "filter") == 0 || strcmp(argv[i], "--filter") == 0) {
                        strcpy(opt.command, "filter");
                        strcpy(opt.district, argv[i + 1]);
                        /* colectam toate conditiile care urmeaza dupa numele districtului */
                        int j = i + 2;
                        while (j < argc && opt.filter_count < MAX_FILTERS) {
                            strcpy(opt.filters[opt.filter_count++], argv[j++]);
                        }
                        }
    }
    return opt;
}

void mode_to_string(mode_t mode, char *buf) {
    buf[0] = (mode & S_IRUSR) ? 'r' : '-';
    buf[1] = (mode & S_IWUSR) ? 'w' : '-';
    buf[2] = (mode & S_IXUSR) ? 'x' : '-';
    buf[3] = (mode & S_IRGRP) ? 'r' : '-';
    buf[4] = (mode & S_IWGRP) ? 'w' : '-';
    buf[5] = (mode & S_IXGRP) ? 'x' : '-';
    buf[6] = (mode & S_IROTH) ? 'r' : '-';
    buf[7] = (mode & S_IWOTH) ? 'w' : '-';
    buf[8] = (mode & S_IXOTH) ? 'x' : '-';
    buf[9] = '\0';
}

void create_district(char *name) {
    mkdir(name, 0750);

    char path[MAX];

    snprintf(path, MAX, "%s/reports.dat", name); //scriem in path, se inlocuieste %s cu numele districtului
    int fd = open(path, O_CREAT | O_RDWR, 0664); //O_CREAT daca nu exista il creeaza, O_RDWR il deschide atat pt write cat si pt read
    close(fd);
    chmod(path, 0664); //dam permisiuni

    snprintf(path, MAX, "%s/district.cfg", name);
    fd = open(path, O_CREAT | O_RDWR, 0640);
    write(fd, "threshold=1\n", 12); //scriem initial threshold = 1, 
    close(fd);
    chmod(path, 0640);

    snprintf(path, MAX, "%s/logged_district", name);
    fd = open(path, O_CREAT | O_RDWR, 0644);
    close(fd);
    chmod(path, 0644);

    char linkname[MAX];
    snprintf(linkname, MAX, "active_reports-%s", name);
    snprintf(path, MAX, "%s/reports.dat", name);
    symlink(path, linkname); //creaza legatura simbolica (shortcut)
}

void remove_district(char *name) {
    char path[MAX];
    snprintf(path, MAX, "active_reports-%s", name);
    unlink(path);

    pid_t pid = fork(); //creeaza noul proces
    if (pid < 0) { //fork() returneaza 0 daca este valid , -1 daca este o eroare
        perror("fork");
        return;
    }

    if (pid == 0) {
        char *args[] = { "rm", "-rf", name, NULL };
        execvp("rm", args); //executa comanda
        perror("execvp"); // daca execvp NU da fail, codul de aici nu se mai executa
        exit(1);
    } else { //asta se executa doar in programul parinte (acesta)
        int status;
        waitpid(pid, &status, 0); //asteptam ca procesul copil sa isi termine executia
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) { //verificam daca procesul copil s a terminat normal si daca s a terminat cu codul de succes 0
            printf("District '%s' removed successfully\n", name);
        } else {
            printf("Failed to remove district '%s'\n", name);
        }
    }
}

void log_action(char *district, char *user, char *action, char *role, int id)
{
    char path[MAX];
    snprintf(path, MAX, "%s/logged_district", district);
    int fd = open(path, O_WRONLY | O_APPEND); //write only, seteaza cursorul la sf fisierului
    if (fd < 0) return; //daca esueaza deschiderea, iesim

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "ID %d: %s - %s - %s\n", id, user, role, action); //scriem in fisier
    write(fd, buffer, strlen(buffer));
    close(fd);
}

void update_threshold(Options opt) {
    if (strcmp(opt.role, "manager") != 0) {
        printf("Permission denied: only manager can update threshold.\n");
        return;
    }
 
    char path[MAX];
    snprintf(path, MAX, "%s/district.cfg", opt.district);
 
    struct stat st; //citim cu functia stat care ne da dimensiunea, ownerul si permisiunile respective
    if (stat(path, &st) < 0) {
        perror("stat district.cfg");
        return;
    }
 
    mode_t perms = st.st_mode & 0777; // verificam cu o masca (000 111 111 111) care este codul permisiunilor
    if (perms != 0640) { //verificam daca e 0640
        char perm[10];
        mode_to_string(st.st_mode, perm);
        printf("Permission check failed on district.cfg: expected rw-r-----, got %s. Aborting.\n", perm);
        return;
    }
 
    int fd = open(path, O_WRONLY | O_TRUNC); //trunc sterge tot ce era inainte in fisier
    if (fd < 0) {
        perror("open district.cfg");
        return;
    }
 
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "threshold=%d\n", opt.report_id); //scriem noul threshold
    write(fd, buf, len);
    close(fd);
 
    log_action(opt.district, opt.user, "update_threshold", opt.role, opt.report_id);
    printf("Threshold updated to %d\n", opt.report_id);
}

void clean_newline(char *str) {
    str[strcspn(str, "\n")] = 0;
} //cand citim cu fgets ramane in buffer tasta enter

void notify_monitor(char *district, char *user, char *role, int report_id) {
    int fd = open(".monitor_pid", O_RDONLY);
    if (fd < 0) {
        log_action(district, user, "monitor could not be notified", role, report_id);
        return;
    }

    char buf[32];
    ssize_t n = read(fd, buf, sizeof(buf) - 1); //citim ce e in fisier - ssize_t = int cu semn folosit pt dimensiuni
    close(fd);

    if (n <= 0) { 
        log_action(district, user, "monitor could not be notified", role, report_id);
        return;
    }
    buf[n] = '\0';

    pid_t monitor_pid = (pid_t)atoi(buf); //transformam textul in PID
    if (monitor_pid <= 0) {
        log_action(district, user, "monitor could not be notified", role, report_id);
        return;
    }

    if (kill(monitor_pid, SIGUSR1) < 0) { //trimitem semnal catre procesul cu pid ul respectiv - SIGUSR1 stie ca trebuie ca programul sa faca ceva
        log_action(district, user, "monitor could not be notified", role, report_id);
    } else {
        log_action(district, user, "monitor notified", role, report_id);
    }
}

void add_report(Options opt) {
    char path[MAX];
    snprintf(path, MAX, "%s/reports.dat", opt.district);

    int fd = open(path, O_RDWR | O_APPEND);
    if (fd < 0) { //daca n avem districtul, il creeam
        create_district(opt.district);
        fd = open(path, O_RDWR | O_APPEND);
    }

    // verificam permisiunile pe reports.dat inainte de scriere
    struct stat st;
    if (stat(fd, &st) < 0) {
        perror("stat reports.dat");
        close(fd);
        return;
    }
    mode_t perms = st.st_mode & 0777;
    if (perms != 0664) {
        char perm[10];
        mode_to_string(st.st_mode, perm);
        printf("Permission check failed on reports.dat: expected rw-rw-r--, got %s. Aborting.\n", perm);
        close(fd);
        return;
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

    r.id = lseek(fd, 0, SEEK_END) / sizeof(Report) + 1; // mutam cursorul la finalul fisierului si aflam dimensiunea totala a fisierului si impartim la dimensiunea unui raport sa aflam al catalea raport este si adaugam 1 pentru noul id
    strcpy(r.inspector, opt.user);
    r.timestamp = time(NULL);

    write(fd, &r, sizeof(Report)); //scriem in fisier raportul
    close(fd);

    log_action(opt.district, opt.user, "add", opt.role, r.id);
    notify_monitor(opt.district, opt.user, opt.role, r.id);
}

void view_report(Options opt) {
    char path[MAX];
    snprintf(path, MAX, "%s/reports.dat", opt.district);
 
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("No district\n");
        return;
    }
 
    off_t offset = (off_t)(opt.report_id - 1) * sizeof(Report); //calculam astfel incat sa sarim primele id-1 rapoarte si sa citim urmatorul raport
    if (lseek(fd, offset, SEEK_SET) < 0) {
        printf("Report not found\n");
        close(fd);
        return;
    }
 
    Report r;
    if (read(fd, &r, sizeof(Report)) != sizeof(Report)) {
        printf("Report not found\n");
        close(fd);
        return;
    }
    close(fd);
 
    printf("ID:          %d\n", r.id);
    printf("Inspector:   %s\n", r.inspector);
    printf("Latitude:    %.6f\n", r.latitude);
    printf("Longitude:   %.6f\n", r.longitude);
    printf("Category:    %s\n", r.category);
    printf("Severity:    %d\n", r.severity);
    printf("Description: %s\n", r.description);
}

void list_reports(Options opt) {
    char path[MAX];
    snprintf(path, MAX, "%s/reports.dat", opt.district);

    //folosim stat() pentru a afisa permisiunile si dimensiunea fisierului
    struct stat st;
    if (stat(path, &st) < 0) {
        printf("No district\n");
        return;
    }

    char perm[10];
    mode_to_string(st.st_mode, perm);
    printf("File: %s | Permissions: %s | Size: %ld bytes | Last modified: %s",
           path, perm, (long)st.st_size, ctime(&st.st_mtime));

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

void remove_report(Options opt) {
    if (strcmp(opt.role, "manager") != 0) {
        printf("Permission denied: only manager can remove reports.\n");
        return;
    }

    char path[MAX];
    snprintf(path, MAX, "%s/reports.dat", opt.district);

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        printf("No district\n");
        return;
    }

    // aflam numarul total de rapoarte din fisier
    struct stat st;
    if (stat(fd, &st) < 0) {
        perror("stat");
        close(fd);
        return;
    }
    int total = st.st_size / sizeof(Report);

    if (opt.report_id < 1 || opt.report_id > total) {
        printf("Report not found\n");
        close(fd);
        return;
    }

    //mutam toate rapoartele de dupa cel sters cu o pozitie in spate
    for (int i = opt.report_id; i < total; i++) {
        Report r;
        off_t src = (off_t)i * sizeof(Report);
        lseek(fd, src, SEEK_SET);
        read(fd, &r, sizeof(Report));
        r.id = i; /* actualizam id-ul */
        off_t dst = (off_t)(i - 1) * sizeof(Report);
        lseek(fd, dst, SEEK_SET);
        write(fd, &r, sizeof(Report));
    }

    // trunchiem fisierul cu un raport mai putin
    ftruncate(fd, (off_t)(total - 1) * sizeof(Report)); //lungimea noua
    close(fd);

    log_action(opt.district, opt.user, "remove_report", opt.role, opt.report_id);
    printf("Report %d removed successfully\n", opt.report_id);
}

// parse_condition: desparte un string de forma "field:op:value" in cele 3 parti
int parse_condition(const char *input, char *field, char *op, char *value) {
    // copiem input-ul ca sa nu il modificam
    char tmp[128];
    strncpy(tmp, input, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    // cautam primul ':' pentru a separa field de rest
    char *colon1 = strchr(tmp, ':');
    if (!colon1) return 0;
    *colon1 = '\0';
    strcpy(field, tmp);

    // cautam al doilea ':' pentru a separa op de value
    char *colon2 = strchr(colon1 + 1, ':');
    if (!colon2) return 0;
    *colon2 = '\0';
    strcpy(op, colon1 + 1);
    strcpy(value, colon2 + 1);

    return 1;
}

// match_condition: verifica daca un raport satisface o conditie field:op:value
int match_condition(Report *r, const char *field, const char *op, const char *value) {
    if (strcmp(field, "severity") == 0) {
        int val = atoi(value);
        if (strcmp(op, "==") == 0) return r->severity == val;
        if (strcmp(op, "!=") == 0) return r->severity != val;
        if (strcmp(op, ">=") == 0) return r->severity >= val;
        if (strcmp(op, "<=") == 0) return r->severity <= val;
        if (strcmp(op, ">")  == 0) return r->severity >  val;
        if (strcmp(op, "<")  == 0) return r->severity <  val;
    } else if (strcmp(field, "category") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->category, value) == 0;
        if (strcmp(op, "!=") == 0) return strcmp(r->category, value) != 0;
    } else if (strcmp(field, "inspector") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->inspector, value) == 0;
        if (strcmp(op, "!=") == 0) return strcmp(r->inspector, value) != 0;
    } else if (strcmp(field, "timestamp") == 0) {
        time_t val = (time_t)atol(value);
        if (strcmp(op, "==") == 0) return r->timestamp == val;
        if (strcmp(op, ">=") == 0) return r->timestamp >= val;
        if (strcmp(op, "<=") == 0) return r->timestamp <= val;
        if (strcmp(op, ">")  == 0) return r->timestamp >  val;
        if (strcmp(op, "<")  == 0) return r->timestamp <  val;
    }
    return 0;
}

void filter_reports(Options opt) {
    char path[MAX];
    snprintf(path, MAX, "%s/reports.dat", opt.district);

    char linkname[MAX];
    snprintf(linkname, MAX, "active_reports-%s", opt.district);
    struct stat lst;
    if (lstat(linkname, &lst) == 0)  { //lstat verifica daca legatura symlink exista pe disc
        struct stat tst;
        if (stat(linkname, &tst) < 0) //verificam daca exista fisierul symlink
            fprintf(stderr, "Warning: dangling symlink %s\n", linkname);
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("No district\n");
        return;
    }

    char fields[MAX_FILTERS][32];
    char ops[MAX_FILTERS][8];
    char values[MAX_FILTERS][64];
    int valid = 1;

    for (int i = 0; i < opt.filter_count; i++) {
        if (!parse_condition(opt.filters[i], fields[i], ops[i], values[i])) {
            fprintf(stderr, "Invalid condition: %s\n", opt.filters[i]);
            valid = 0;
        }
    }

    if (!valid) { close(fd); return; }

    Report r;
    int found = 0;
    //verificam daca exista rapoarte care sa indeplineasca toate conditiile
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        int all_match = 1; 
        for (int i = 0; i < opt.filter_count; i++) {
            if (!match_condition(&r, fields[i], ops[i], values[i])) {
                all_match = 0;
                break;
            }
        }
        if (all_match) {
            printf("ID: %d | Inspector: %s | Category: %s | Severity: %d\n",
                   r.id, r.inspector, r.category, r.severity);
            found = 1;
        }
    }

    if (!found) printf("No matching reports.\n");
    close(fd);
    log_action(opt.district, opt.user, "filter", opt.role, 0);
}

int main(int argc, char **argv)
{
    Options opt = parse(argc, argv);

    if (strcmp(opt.command, "add") == 0)
        add_report(opt);
    else if (strcmp(opt.command, "list") == 0)
        list_reports(opt);
    else if (strcmp(opt.command, "view") == 0)
        view_report(opt);
    else if (strcmp(opt.command, "remove_report") == 0)
        remove_report(opt);
    else if (strcmp(opt.command, "update_threshold") == 0)
        update_threshold(opt);
    else if (strcmp(opt.command, "filter") == 0)
        filter_reports(opt);
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