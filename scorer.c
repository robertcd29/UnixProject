#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX 256

typedef struct {
    int id;
    char inspector[32];
    float latitude;
    float longitude;
    char category[32];
    int severity;
    long timestamp;
    char description[64];
} Report;

typedef struct {
    char inspector[32];
    int total_severity;
    int report_count;
} InspectorScore;

#define MAX_INSPECTORS 64

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: scorer <district>\n");
        return 1;
    }

    char *district = argv[1];

    char path[MAX];
    snprintf(path, MAX, "%s/reports.dat", district);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("District '%s': no reports found\n", district);
        return 0;
    }

    InspectorScore scores[MAX_INSPECTORS];
    int num_inspectors = 0;

    Report r;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        int found = -1;
        for (int i = 0; i < num_inspectors; i++) {
            if (strcmp(scores[i].inspector, r.inspector) == 0) {
                found = i;
                break;
            }
        }
        if (found == -1) {
            if (num_inspectors < MAX_INSPECTORS) {
                strncpy(scores[num_inspectors].inspector, r.inspector,
                        sizeof(scores[num_inspectors].inspector) - 1);
                scores[num_inspectors].inspector[sizeof(scores[num_inspectors].inspector)-1] = '\0';
                scores[num_inspectors].total_severity = 0;
                scores[num_inspectors].report_count   = 0;
                found = num_inspectors++;
            } else {
                continue;
            }
        }
        scores[found].total_severity += r.severity;
        scores[found].report_count++;
    }
    close(fd);

    printf("District: %s\n", district);
    if (num_inspectors == 0) {
        printf("  (no reports)\n");
    } else {
        for (int i = 0; i < num_inspectors; i++) {
            printf("  Inspector: %-20s  Reports: %2d  Workload Score: %d\n", //-20s ocupa 20 de caractere si se aliniaza la stanga
                   scores[i].inspector,
                   scores[i].report_count,
                   scores[i].total_severity);
        }
    }

    fflush(stdout);
    return 0;
}