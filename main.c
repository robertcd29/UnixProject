#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>

#define MAX 256
#define MAX_FILTERS 16

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
    char filters[MAX_FILTERS][64];
    int filter_count;
    int threshold_value;
} Options;

Options parse(int argc, char *argv[]) {
    Options opt = {0};

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc)
            strcpy(opt.role, argv[++i]);
        else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc)
            strcpy(opt.user, argv[++i]);
        else if (strcmp(argv[i], "--add") == 0 || strcmp(argv[i], "add") == 0) {
            strcpy(opt.command, "add");
            if (i + 1 < argc) { strcpy(opt.district, argv[++i]); }
        }
        else if (strcmp(argv[i], "--remove") == 0 || strcmp(argv[i], "remove") == 0) {
            strcpy(opt.command, "remove");
            if (i + 1 < argc) { strcpy(opt.district, argv[++i]); }
        }
        else if (strcmp(argv[i], "--list") == 0 || strcmp(argv[i], "list") == 0) {
            strcpy(opt.command, "list");
            if (i + 1 < argc) { strcpy(opt.district, argv[++i]); }
        }
        else if (strcmp(argv[i], "--view") == 0 || strcmp(argv[i], "view") == 0) {
            strcpy(opt.command, "view");
            if (i + 1 < argc) { strcpy(opt.district, argv[++i]); }
            if (i + 1 < argc) { opt.report_id = atoi(argv[++i]); }
        }
        else if (strcmp(argv[i], "--remove_report") == 0 || strcmp(argv[i], "remove_report") == 0) {
            strcpy(opt.command, "remove_report");
            if (i + 1 < argc) { strcpy(opt.district, argv[++i]); }
            if (i + 1 < argc) { opt.report_id = atoi(argv[++i]); }
        }
        else if (strcmp(argv[i], "--update_threshold") == 0 || strcmp(argv[i], "update_threshold") == 0) {
            strcpy(opt.command, "update_threshold");
            if (i + 1 < argc) { strcpy(opt.district, argv[++i]); }
            if (i + 1 < argc) { opt.threshold_value = atoi(argv[++i]); }
        }
        else if (strcmp(argv[i], "--filter") == 0 || strcmp(argv[i], "filter") == 0) {
            strcpy(opt.command, "filter");
            if (i + 1 < argc) { strcpy(opt.district, argv[++i]); }
            while (i + 1 < argc && opt.filter_count < MAX_FILTERS) {
                strcpy(opt.filters[opt.filter_count++], argv[++i]);
            }
        }
    }

    return opt;
}

void mode_to_str(mode_t mode, char *out) {
    const char bits[] = "rwx";
    for (int i = 0; i < 9; i++) {
        int bit = 8 - i;
        out[i] = (mode & (1 << bit)) ? bits[i % 3] : '-';
    }
    out[9] = '\0';
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

    char target[MAX];
    char linkname[MAX];
    snprintf(target, MAX, "%s/reports.dat", name);
    snprintf(linkname, MAX, "active_reports-%s", name);
    unlink(linkname);
    symlink(target, linkname);
}

void remove_district(char *name) {
    char path[MAX];

    snprintf(path, MAX, "active_reports-%s", name);
    unlink(path);

    snprintf(path, MAX, "%s/reports.dat", name);
    unlink(path);

    snprintf(path, MAX, "%s/district.cfg", name);
    unlink(path);

    snprintf(path, MAX, "%s/logged_district", name);
    unlink(path);

    rmdir(name);
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
}

void list_reports(Options opt) {
    char path[MAX];
    snprintf(path, MAX, "%s/reports.dat", opt.district);

    char linkname[MAX];
    snprintf(linkname, MAX, "active_reports-%s", opt.district);
    struct stat lst;
    if (lstat(linkname, &lst) == 0) {
        struct stat tst;
        if (stat(linkname, &tst) < 0) {
            fprintf(stderr, "Warning: dangling symlink %s\n", linkname);
        }
    }

    struct stat st;
    if (stat(path, &st) < 0) {
        printf("No district\n");
        return;
    }

    mode_t m = st.st_mode;
    if (strcmp(opt.role, "manager") == 0 && !(m & S_IRUSR)) {
        fprintf(stderr, "Permission denied: owner-read not set on %s\n", path);
        return;
    }
    if (strcmp(opt.role, "inspector") == 0 && !(m & S_IRGRP)) {
        fprintf(stderr, "Permission denied: group-read not set on %s\n", path);
        return;
    }

    char sym[10];
    mode_to_str(m, sym);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M", localtime(&st.st_mtime));
    printf("File: %s | Permissions: %s | Size: %lld bytes | Last modified: %s\n",
           path, sym, (long long)st.st_size, timebuf);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("No district\n");
        return;
    }

    Report r;
    int found = 0;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        printf("ID: %d | Inspector: %s | Category: %s | Severity: %d\n",
               r.id, r.inspector, r.category, r.severity);
        found = 1;
    }
    if (!found) printf("No reports in district %s\n", opt.district);

    close(fd);
    log_action(opt.district, opt.user, "list", opt.role, 0);
}

void view_report(Options opt) {
    char path[MAX];
    snprintf(path, MAX, "%s/reports.dat", opt.district);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("No district\n");
        return;
    }

    Report r;
    int found = 0;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        if (r.id == opt.report_id) {
            char timebuf[32];
            strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&r.timestamp));
            printf("=== Report ID: %d ===\n", r.id);
            printf("Inspector  : %s\n", r.inspector);
            printf("Latitude   : %.6f\n", r.latitude);
            printf("Longitude  : %.6f\n", r.longitude);
            printf("Category   : %s\n", r.category);
            printf("Severity   : %d\n", r.severity);
            printf("Timestamp  : %s\n", timebuf);
            printf("Description: %s\n", r.description);
            found = 1;
            break;
        }
    }
    close(fd);

    if (!found) printf("Report %d not found in district %s\n", opt.report_id, opt.district);
    else log_action(opt.district, opt.user, "view", opt.role, opt.report_id);
}

void remove_report(Options opt) {
    if (strcmp(opt.role, "manager") != 0) {
        fprintf(stderr, "Permission denied: only manager can remove reports\n");
        return;
    }

    char path[MAX];
    snprintf(path, MAX, "%s/reports.dat", opt.district);

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        printf("No district\n");
        return;
    }

    off_t size = lseek(fd, 0, SEEK_END);
    int total = size / sizeof(Report);
    lseek(fd, 0, SEEK_SET);

    int found_idx = -1;
    Report r;
    for (int i = 0; i < total; i++) {
        read(fd, &r, sizeof(Report));
        if (r.id == opt.report_id) { found_idx = i; break; }
    }

    if (found_idx < 0) {
        printf("Report %d not found\n", opt.report_id);
        close(fd);
        return;
    }

    for (int i = found_idx + 1; i < total; i++) {
        lseek(fd, (off_t)i * sizeof(Report), SEEK_SET);
        read(fd, &r, sizeof(Report));
        lseek(fd, (off_t)(i - 1) * sizeof(Report), SEEK_SET);
        write(fd, &r, sizeof(Report));
    }

    ftruncate(fd, (off_t)(total - 1) * sizeof(Report));
    close(fd);

    printf("Report %d removed from district %s\n", opt.report_id, opt.district);
    log_action(opt.district, opt.user, "remove_report", opt.role, opt.report_id);
}

void update_threshold(Options opt) {
    if (strcmp(opt.role, "manager") != 0) {
        fprintf(stderr, "Permission denied: only manager can update threshold\n");
        return;
    }

    char path[MAX];
    snprintf(path, MAX, "%s/district.cfg", opt.district);

    struct stat st;
    if (stat(path, &st) < 0) {
        fprintf(stderr, "Cannot stat %s\n", path);
        return;
    }
    mode_t expected __attribute__((unused)) = S_IRUSR | S_IWUSR | S_IRGRP;
    if ((st.st_mode & 0777) != 0640) {
        char sym[10];
        mode_to_str(st.st_mode, sym);
        fprintf(stderr, "Permission mismatch on %s: expected 640, got %s\n", path, sym);
        return;
    }

    int fd = open(path, O_WRONLY | O_TRUNC);
    if (fd < 0) {
        fprintf(stderr, "Cannot open %s for writing\n", path);
        return;
    }

    char buf[32];
    int len = snprintf(buf, sizeof(buf), "threshold=%d\n", opt.threshold_value);
    write(fd, buf, len);
    close(fd);

    printf("Threshold updated to %d in district %s\n", opt.threshold_value, opt.district);
    log_action(opt.district, opt.user, "update_threshold", opt.role, 0);
}

int parse_condition(const char *input, char *field, char *op, char *value) {
    const char *first_colon = strchr(input, ':');
    if (!first_colon) return 0;

    size_t field_len = first_colon - input;
    if (field_len == 0 || field_len >= 32) return 0;
    strncpy(field, input, field_len);
    field[field_len] = '\0';

    const char *second_colon = strchr(first_colon + 1, ':');
    if (!second_colon) return 0;

    size_t op_len = second_colon - (first_colon + 1);
    if (op_len == 0 || op_len >= 8) return 0;
    strncpy(op, first_colon + 1, op_len);
    op[op_len] = '\0';

    const char *val_start = second_colon + 1;
    if (strlen(val_start) == 0) return 0;
    strncpy(value, val_start, 63);
    value[63] = '\0';

    return 1;
}

int match_condition(Report *r, const char *field, const char *op, const char *value) {
    if (strcmp(field, "severity") == 0) {
        int rval = r->severity;
        int cval = atoi(value);
        if (strcmp(op, "==") == 0) return rval == cval;
        if (strcmp(op, "!=") == 0) return rval != cval;
        if (strcmp(op, "<")  == 0) return rval <  cval;
        if (strcmp(op, "<=") == 0) return rval <= cval;
        if (strcmp(op, ">")  == 0) return rval >  cval;
        if (strcmp(op, ">=") == 0) return rval >= cval;
    } else if (strcmp(field, "category") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->category, value) == 0;
        if (strcmp(op, "!=") == 0) return strcmp(r->category, value) != 0;
    } else if (strcmp(field, "inspector") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->inspector, value) == 0;
        if (strcmp(op, "!=") == 0) return strcmp(r->inspector, value) != 0;
    } else if (strcmp(field, "timestamp") == 0) {
        time_t rval = r->timestamp;
        time_t cval = (time_t)atol(value);
        if (strcmp(op, "==") == 0) return rval == cval;
        if (strcmp(op, "!=") == 0) return rval != cval;
        if (strcmp(op, "<")  == 0) return rval <  cval;
        if (strcmp(op, "<=") == 0) return rval <= cval;
        if (strcmp(op, ">")  == 0) return rval >  cval;
        if (strcmp(op, ">=") == 0) return rval >= cval;
    }
    return 0;
}

void filter_reports(Options opt) {
    char path[MAX];
    snprintf(path, MAX, "%s/reports.dat", opt.district);

    char linkname[MAX];
    snprintf(linkname, MAX, "active_reports-%s", opt.district);
    struct stat lst;
    if (lstat(linkname, &lst) == 0) {
        struct stat tst;
        if (stat(linkname, &tst) < 0)
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
    else if (strcmp(opt.command, "remove") == 0)
        remove_district(opt.district);
    else if (strcmp(opt.command, "update_threshold") == 0)
        update_threshold(opt);
    else if (strcmp(opt.command, "filter") == 0)
        filter_reports(opt);
    else
        printf("Unknown command");

    return 0;
}