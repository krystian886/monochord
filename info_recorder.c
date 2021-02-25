#define _XOPEN_SOURCE 500
#define _POSIX_C_SOURCE 199309
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <time.h>

struct sigHandlerData
{
    int retVal;
    int dataReceived;
}sigHandlerData = {.retVal = 0, .dataReceived = 0};

void printFormatAndExit(const char* progName)
{
    printf("Format: %s \n\
    -c <int> signal number\n\
    <int> rejestrator PID number\n", progName);
    exit(EXIT_FAILURE);
}

int stringToIntValidation(const char* data, const char* progName)
{
    char* endPtr = NULL;
    errno=0;
    long number=strtol(data, &endPtr, 10);
    if((errno!=0) || (*endPtr!='\0') || (number>INT_MAX))
	printFormatAndExit(progName);
    return (int)number;
}

double measureTime(struct timespec current, struct timespec ref)
{
    clock_gettime(CLOCK_MONOTONIC, &current);
    double retVal = (double)(current.tv_sec - ref.tv_sec);
    if(current.tv_nsec < ref.tv_nsec)
    {
        retVal -= 1;
        retVal += (double) (((int)(((current.tv_nsec + 1000000000) - ref.tv_nsec) / 1000)) % 1000000) / 1000000;
    }
    else retVal += (double) (((int)((current.tv_nsec - ref.tv_nsec) / 1000)) % 1000000) / 1000000;

    return retVal;
}

void printResponse()
{
    int bitArr[4];
    for(int i = 0; i < 4; i++)
    {
        int mask =  1 << i;
        int masked = sigHandlerData.retVal & mask;
        int bit = masked >> i;
        bitArr[i] = bit;
    }

    if(bitArr[0] == 1)
        printf("recorder works\n");
    if(bitArr[1] == 1)
        printf("reference point is being used\n");
    if(bitArr[2] == 1)
        printf("source identification is being used (PID)\n");
    if(bitArr[3] == 1)
        printf("binary form is being used\n");

    exit(EXIT_SUCCESS);
}

void sigHandler(int signo, siginfo_t* info, void* extra) {
    sigHandlerData.retVal = info->si_value.sival_int;
    sigHandlerData.dataReceived = 1;
}


int main(int argc, char* argv[])
{
    int sigNum, PID;
    union sigval sigValue;
    struct sigaction returningSignal;
    struct timespec waitingTime, referenceTime,
            sleepingTime = {.tv_sec = 0, .tv_nsec = 0};
    
    if(argc != 4 && argc != 3 )
	printFormatAndExit(argv[0]);

    if(argv[1][0] == '-' && argv[1][1] == 'c' && argc == 4)
    {
        sigNum = stringToIntValidation(argv[2], argv[0]);
        PID = stringToIntValidation(argv[3], argv[0]);
    }
    else if(argv[1][0] == '-' && argv[1][1] == 'c' && argc == 3)
    {
        char* tmp = (char*)malloc((strlen(argv[1])) * sizeof(char));
        strcpy(tmp, argv[1]);
        tmp[0] = tmp[1] = '0';
        sigNum = stringToIntValidation(tmp, argv[0]);
        PID = stringToIntValidation(argv[2], argv[0]);
        free(tmp);
    }
    else printFormatAndExit(argv[0]);

    returningSignal.sa_flags = SA_SIGINFO;
    returningSignal.sa_sigaction = &sigHandler;

    errno = 0;
    if (sigaction(sigNum, &returningSignal, NULL) == -1)
    {
        perror("Sigaction: returningSignal\n");
        return -1;
    }

    sigValue.sival_int = 255;
    errno = 0;
    if (sigqueue(PID, sigNum, sigValue) == -1)
    {
        perror("sigqueue\n");
        exit(EXIT_FAILURE);
    }

    clock_gettime(CLOCK_MONOTONIC, &referenceTime);

    while(1)
    {
        if(measureTime(waitingTime, referenceTime) > 2.0)
            exit(EXIT_FAILURE);

        if(sigHandlerData.dataReceived == 1)
            printResponse();

        nanosleep(&sleepingTime, NULL);
    }
}
