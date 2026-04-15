#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>


typedef struct {
    char role[16];
    char user[64];
    char command[64];
    char districtId[64];
    int paramCount;
    char *params[10];
} Options;

void parseCommand(int argc, char **argv, Options *opt)
{
    opt->role[0] = '\0';
    opt->user[0] = '\0';
    opt->command[0] = '\0';
    opt->districtId[0] = '\0';
    opt->paramCount = 0;

    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--role") == 0)
        {
            if(i + 1 >= argc)
            {
                printf("Error - argument role missing\n");
                return;
            }

            strncpy(opt->role, argv[++i], sizeof(opt->role) - 1);
            opt->role[sizeof(opt->role) - 1] = '\0';

            if(strcmp(opt->role, "inspector") != 0 && strcmp(opt->role, "manager") != 0)
            {
                printf("Error - wrong role\n");
                return;
            }
        }
        else if(strcmp(argv[i], "--user") == 0)
        {
            if(i + 1 >= argc)
            {
                printf("Error - argument user missing\n");
                return;
            }

            strncpy(opt->user, argv[++i], sizeof(opt->user) - 1);
            opt->user[sizeof(opt->user) - 1] = '\0';
        }
        else
        {
            strncpy(opt->command, argv[i], sizeof(opt->command) - 1);
            opt->command[sizeof(opt->command) - 1] = '\0';

            if(i + 1 < argc)
            {
                strncpy(opt->districtId, argv[++i], sizeof(opt->districtId) - 1);
                opt->districtId[sizeof(opt->districtId) - 1] = '\0';
            }

            opt->paramCount = 0;
            i++;
            while(i < argc && opt->paramCount < 10)
            {
                opt->params[opt->paramCount++] = argv[i++];
            }
            break;
        }
    }
}

void createDistrict(char *districtName)
{
    mkdir(districtName, 0777);
}

int main(int argc, char **argv)
{
    Options opt;
    FILE *f = fopen("reports.dat", "w");

    parseCommand(argc, argv, &opt);

    printf("Role: %s\n", opt.role);
    printf("User: %s\n", opt.user);
    printf("Command: %s\n", opt.command);
    printf("District: %s\n", opt.districtId);
    printf("Parametrii (%d):\n", opt.paramCount);
    for(int j = 0; j < opt.paramCount; j++)
        printf("%s\n", opt.params[j]);
    createDistrict(opt.districtId);

    return 0;
}