#define _XOPEN_SOURCE 500
#define _POSIX_C_SOURCE 199309
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <getopt.h>
#include <fcntl.h>

#define MAXLINE 256

struct binaryToSave
{
    struct timespec ts;
    float data;
    pid_t source;
};

struct dataComingFromMonochord
{
    pid_t monochordPID;
    int data;
    int dataAlreadySeen;
}dataComingFromMonochord = {.dataAlreadySeen = 1};

struct dataComingFromInfo
{
    int data;
    int dataAlreadySeen;
    int sendResponse;
    int info_rejestratorPID;
}dataComingFromInfo = {.data = 0, .dataAlreadySeen = 1, .sendResponse = 0};

void writeToBinary(struct timespec ts, float data, pid_t source, int fd)
{
    struct binaryToSave binaryToSave = { .ts = ts, .data = data, .source = source };

    errno = 0;
    if(write(fd, &binaryToSave, sizeof(struct binaryToSave)) == -1)
    {
        perror("writeToBinary: write\n");
        exit(EXIT_FAILURE);
    }
}

void writeToText(int timeMarker, struct timespec ts, float data, pid_t source, int fd)
{
    char stringData[MAXLINE+1];
    int HH = (int)ts.tv_sec/3600;
    ts.tv_sec = ts.tv_sec - (HH * 3600);

    if(timeMarker == 0) if(HH > 23) HH = HH % 24;

    int MM = (int)ts.tv_sec/60;
    ts.tv_sec = ts.tv_sec - (MM * 60);
    if( MM > 59) MM = MM % 60;
    int SS = ts.tv_sec;
    int MS = ((int)(ts.tv_nsec / 1000000)) % 1000;

    int ret;
    if(source != 0)
        ret = snprintf(stringData, sizeof(stringData),
                       "%02d:%02d:%02d.%03d %f %d\n",
                       HH, MM, SS, MS, data, source);
    else ret = snprintf(stringData, sizeof(stringData),
                        "%02d:%02d:%02d.%03d %f\n",
                        HH, MM, SS, MS, data);

    if(ret<0)
        exit(EXIT_FAILURE);

    errno = 0;
    if(write(fd, stringData, strlen(stringData)) == -1)
    {
        perror("writeToText: write\n");
        exit(EXIT_FAILURE);
    }
}

float intToFloatValidation(const int data)
{
    double tmp = (double)data;
    tmp /= 1000000;
    float result = (float)tmp;

    return result;
}

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

struct timespec measureTime(struct timespec current, struct timespec ref)
{
    clock_gettime(CLOCK_MONOTONIC, &current);
    struct timespec retVal;
    retVal.tv_sec = current.tv_sec - ref.tv_sec;
    if(current.tv_nsec < ref.tv_nsec)
    {
        retVal.tv_sec -= 1;
        retVal.tv_nsec = (current.tv_nsec + 1000000000) - ref.tv_nsec;
    }
    else retVal.tv_nsec = current.tv_nsec - ref.tv_nsec;

    return retVal;
}

void openFiles(int B, int T, char* binaryPath, char* textPath, int* binaryFileDesc, int* textFileDesc)
{
    if( B == 1 && binaryPath[0] != '-')
    {
        errno = 0;
        *binaryFileDesc = open(binaryPath, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if(*binaryFileDesc == -1)
        {
            perror("binaryFileDesc: open");
            exit(EXIT_FAILURE);
        }
    }
    if( T == 1 )
    {
        errno = 0;
        *textFileDesc = open(textPath, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if(*textFileDesc == -1)
        {
            perror("textFileDesc: open");
            exit(EXIT_FAILURE);
        }
    }
}

void sigCommands(int signo, siginfo_t* info, void* extra)
{
    if(info->si_value.sival_int == 255)
        dataComingFromInfo.sendResponse = 1;
    else
    {
        dataComingFromInfo.data = info->si_value.sival_int;
        dataComingFromInfo.dataAlreadySeen = 0;
    }
    dataComingFromInfo.info_rejestratorPID = info->si_pid;
}

void sigValues(int signo, siginfo_t* info, void* extra)
{
    dataComingFromMonochord.monochordPID = info->si_pid;
    dataComingFromMonochord.data = info->si_value.sival_int;
    dataComingFromMonochord.dataAlreadySeen = 0;
}


int main(int argc, char* argv[])
{
    int getOptVal, getOptError = 0;
    int dataSignal, commandSignal;
    int B = 0, T = 0, D = 0, C = 0;
    int binaryFileDesc = 1,  textFileDesc = 1;
    int workPossible = 0, timeMarker, usePID = 0, valuePID;
    int referenceTimeExists = 0, previousReferenceTimeExists = 0;
    char *binaryPath = NULL, *textPath = NULL;
    struct sigaction signalData, signalCommands;
    sigset_t clearingSet;
    struct timespec referenceTime = {.tv_sec = 0, .tv_nsec = 0},
            previousReferenceTime = {.tv_sec = 0, .tv_nsec = 0}, currentTime;
    union sigval sigValue;

    while ((getOptVal = getopt(argc, argv, "b:t:d:c:")) != -1)
    {
        if (getOptVal == 'b' && !B) {
            binaryPath = optarg;
            B = 1;
        }
        else if (getOptVal == 't' && !T) {
            textPath = optarg;
            T = 1;
        }
        else if (getOptVal == 'd' && !D)
        {
            dataSignal = stringToIntValidation(optarg);
            if (dataSignal < SIGRTMIN || dataSignal > SIGRTMAX)
            {
                errno = 22;
                perror("-d: signal must have REAL-TIME value \n");
                exit(EXIT_FAILURE);
            }
            if (C==1 && dataSignal == commandSignal)
            {
                errno = 22;
                perror("-d && -c: signals must have different values \n");
                exit(EXIT_FAILURE);
            }
            D = 1;
        }
        else if (getOptVal == 'c' && !C)
        {
            commandSignal = stringToIntValidation(optarg);
            if (commandSignal < SIGRTMIN || commandSignal > SIGRTMAX)
            {
                errno = 22;
                perror("-c: signal must have REAL-TIME value\n");
                exit(EXIT_FAILURE);
            }
            if (D==1 && dataSignal == commandSignal)
            {
                errno = 22;
                perror("-d && -c: signals must have different values \n");
                exit(EXIT_FAILURE);
            }
            C = 1;
        }
        else getOptError = 1;
    }
    if (argc - optind > 1 || !D || !C) getOptError = 1;

    if (getOptError)
    {
        printf("Format: %s \n\
            -b (optional) <path> path to the binary file\n\
            -t (optional) <path> path to the text file\n\
            -d <signal> data signal\n\
            -c <signal> commanding signal\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    openFiles(B, T, binaryPath, textPath, &binaryFileDesc, &textFileDesc);

    printf("PID: %d\n", getpid());

    // serving signals
    sigemptyset(&signalData.sa_mask);
    sigemptyset(&signalCommands.sa_mask);
    sigemptyset(&clearingSet);
    sigaddset(&signalData.sa_mask, commandSignal);
    sigaddset(&signalCommands.sa_mask, dataSignal);

    signalCommands.sa_flags = signalData.sa_flags = SA_SIGINFO;
    signalCommands.sa_sigaction = &sigCommands;
    signalData.sa_sigaction = &sigValues;

    errno = 0;
    if (sigaction(dataSignal, &signalData, NULL) == -1)
    {
        perror("Sigaction: signalData\n");
        return 0;
    }
    errno = 0;
    if (sigaction(commandSignal, &signalCommands, NULL) == -1)
    {
        perror("Sigaction: signalCommands\n");
        return 0;
    }

    while(1) {
        if(dataComingFromInfo.dataAlreadySeen == 0)
            sigprocmask(SIG_SETMASK, &signalCommands.sa_mask, NULL);
        else if(dataComingFromMonochord.dataAlreadySeen == 0)
            sigprocmask(SIG_SETMASK, &signalData.sa_mask, NULL);

        // serving steering signals
        if(dataComingFromInfo.dataAlreadySeen == 0 && dataComingFromInfo.data != 0)
        {
            if(dataComingFromInfo.data == 1)
            {
                dataComingFromInfo.dataAlreadySeen = 1;
                timeMarker = 0;
                workPossible = 1;
            }
            else if(dataComingFromInfo.data == 2)
            {
                if(referenceTimeExists == 0 )
                {
                    clock_gettime(CLOCK_MONOTONIC, &referenceTime);
                    referenceTimeExists = 1;
                    previousReferenceTimeExists = 0;
                }
                else
                {
                    previousReferenceTime.tv_sec = referenceTime.tv_sec;
                    previousReferenceTime.tv_nsec = referenceTime.tv_nsec;
                    clock_gettime(CLOCK_MONOTONIC, &referenceTime);
                    referenceTimeExists = 1;
                    previousReferenceTimeExists = 1;
                }
                timeMarker = 1;
                workPossible = 1;
                dataComingFromInfo.dataAlreadySeen = 1;
            }
            else if(dataComingFromInfo.data == 3)
            {
                if(referenceTimeExists == 0 && previousReferenceTimeExists == 0)
                {
                    clock_gettime(CLOCK_MONOTONIC, &referenceTime);
                    referenceTimeExists = 1;
                }
                else if(previousReferenceTimeExists == 1)
                {
                    referenceTime.tv_sec = previousReferenceTime.tv_sec;
                    referenceTime.tv_nsec = previousReferenceTime.tv_nsec;
                    previousReferenceTimeExists = 0;
                }

                timeMarker = 1;
                workPossible = 1;
                dataComingFromInfo.dataAlreadySeen = 1;
            }
            else if(dataComingFromInfo.data == 5)
            {
                usePID = 1;
                workPossible = 1;
                dataComingFromInfo.dataAlreadySeen = 1;
            }
            else if(dataComingFromInfo.data == 9)
            {
                if(binaryFileDesc != 1) close(binaryFileDesc);
                if(textFileDesc != 1) close(textFileDesc);
                openFiles(B, T, binaryPath, textPath, &binaryFileDesc, &textFileDesc);
                workPossible = 1;
                dataComingFromInfo.dataAlreadySeen = 1;
            }
        }
        else if(dataComingFromInfo.data == 0)
        {
            workPossible = 0;
            referenceTime.tv_sec = 0, referenceTime.tv_nsec = 0;
            previousReferenceTime.tv_sec = 0, previousReferenceTime.tv_nsec = 0;
            referenceTimeExists = previousReferenceTimeExists = 0;
            timeMarker = 0;
            usePID = 0;
            dataComingFromInfo.dataAlreadySeen = 1;
        }

        // writing data
        if(dataComingFromMonochord.dataAlreadySeen == 0 && workPossible == 1)
        {
            if(timeMarker == 1)
            {
                clock_gettime(CLOCK_MONOTONIC, &currentTime);
                currentTime = measureTime(currentTime, referenceTime);
            }
            else clock_gettime(CLOCK_REALTIME, &currentTime);

            if(usePID == 1) valuePID = dataComingFromMonochord.monochordPID;
            else valuePID = 0;
            writeToText(timeMarker , currentTime, intToFloatValidation(dataComingFromMonochord.data),
                       valuePID, textFileDesc);

            if(B == 1)
                writeToBinary(currentTime, intToFloatValidation(dataComingFromMonochord.data),
                              dataComingFromMonochord.monochordPID, binaryFileDesc);

            dataComingFromMonochord.dataAlreadySeen = 1;
        }

        // sending response to info_rejestrator
        if(dataComingFromInfo.sendResponse == 1)
        {
            int responseValue = 0;
            if(workPossible == 1) responseValue += 1;
            if(timeMarker == 1) responseValue += 2;
            if(usePID != 0) responseValue += 4;
            if(B == 1) responseValue += 8;

            sigValue.sival_int = responseValue;
            errno = 0;
            if (sigqueue(dataComingFromInfo.info_rejestratorPID, commandSignal, sigValue) == -1)
            {
                perror("sigqueue\n");
                exit(EXIT_FAILURE);
            }
            dataComingFromInfo.sendResponse = 0;
        }

        sigprocmask(SIG_SETMASK, &clearingSet, NULL);
        pause();
    }
}