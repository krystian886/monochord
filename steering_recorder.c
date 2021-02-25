#define _XOPEN_SOURCE 500
#define _POSIX_C_SOURCE 199309
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>

int stringToIntValidation(const char* data)
{
    char* endPtr = NULL;
    errno=0;
    long number=strtol(data, &endPtr, 10);
    if((errno!=0) || (*endPtr!='\0') || (number>INT_MAX))
    {
        perror("stringToIntValidation\n");
        exit(EXIT_FAILURE);
    }
    return (int)number;
}


int main(int argc, char* argv[])
{
    int PID, sigNum, sigVal;
    union sigval valueToSend;
    int getOptVal, getOptError = 0;
    int P = 0, S = 0, V = 0;

    while ((getOptVal = getopt(argc, argv, "p:s:v:")) != -1)
    {
        if (getOptVal == 'p' && !P) {
            PID = stringToIntValidation(optarg);
            P = 1;
        }
        else if (getOptVal == 's' && !S) {
            sigNum = stringToIntValidation(optarg);
            if (sigNum < SIGRTMIN || sigNum > SIGRTMAX)
            {
                errno = 22;
                perror("-s: signal must have REAL-TIME value \n");
                exit(EXIT_FAILURE);
            }
            S = 1;
        }
        else if (getOptVal == 'v' && !V) {
            sigVal = stringToIntValidation(optarg);
            if (!(sigVal == 0 || sigVal == 1 || sigVal == 2 || sigVal == 3 || sigVal == 5 || sigVal == 9))
            {
                errno = 22;
                perror("-v: allowed values: 0, 1, 2, 3, 5, 9 \n");
                exit(EXIT_FAILURE);
            }
            V = 1;
        }
        else getOptError = 1;
    }
    if (argc - optind > 1 || !P || !S || !V) getOptError = 1;

    if (getOptError)
    {
        printf("Format: %s \n\
            -p <pid> where to send the signal\n\
            -s <signal> signal (only REAL-TIME type is valid)\n\
            -v <value> what to send to the process\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    valueToSend.sival_int = sigVal;
    errno = 0;
    if (sigqueue(PID, sigNum, valueToSend) == -1)
    {
        perror("sigqueue\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}
