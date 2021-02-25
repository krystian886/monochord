#define _XOPEN_SOURCE 500
#define _POSIX_C_SOURCE 199309
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <signal.h>
#include <math.h>
#include <time.h>

#define MAXLINE 512
#define MAXINFO 32
#define PI 3.14159

enum parameterToChange {
        amp = 0,
        freq,
        probe,
        period,
        pid,
        rt
};

struct steeringValues
{
    float amp;
    float freq;
    float probe;
    float period;
    short pid;
    short rt;
    int destExists;
    int restartTrigger;
    int timeHasPassed;
};

struct socketData
{
    int sockfd;
    unsigned int len;
    struct sockaddr_in cliaddr;
};

float stringToFloatValidation(const char* data)
{
    char* endPtr = NULL;
    errno=0;
    float number=strtof(data, &endPtr);
    if((errno!=0) || (*endPtr!='\0') || (number==FLT_MAX))
    {
        perror("stringToFloatValidation\n");
        exit(EXIT_FAILURE);
    }
    return number;
}

short stringToShortValidation(const char* data)
{
    char* endPtr = NULL;
    errno=0;
    long number=strtol(data, &endPtr, 10);
    if((errno!=0) || (*endPtr!='\0') || (number>SHRT_MAX))
    {
        perror("stringToShortValidation\n");
        exit(EXIT_FAILURE);
    }
    return (short)number;
}

unsigned short stringToUnsignedShortValidation(const char* data)
{
    char* endPtr = NULL;
    errno=0;
    long number=strtol(data, &endPtr, 10);
    if((errno!=0) || (*endPtr!='\0') || (number>SHRT_MAX) || (number < 0))
    {
        perror("stringToShortValidation\n");
        exit(EXIT_FAILURE);
    }
    return (short)number;
}

int floatToIntValidation(const float data)
{
    double tmp = fmod(data * 1000000, INT_MAX);
    int result = (int)tmp;

    return result;
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

int validatePID(struct steeringValues* values)
{
    errno = 0;
    kill(values->pid, 0);
    if(errno == EPERM || errno == ESRCH)
    {
        values->destExists = 0;
        return 0;
    }
    else
    {
        values->destExists = 1;
        return 1;
    }
}

void sendReport(struct steeringValues values, struct socketData socketValues)
{
    char reportMsg[MAXLINE+1] = {'\0'};
    char periodInfo[MAXINFO+1] = {'\0'};
    char pidInfo[MAXINFO+1] = {'\0'};
    char rtInfo[MAXINFO+1] = {'\0'};

    if(values.period == 0) snprintf(periodInfo, sizeof(periodInfo), "non-stop");
    else if(values.period < 0) snprintf(periodInfo, sizeof(periodInfo), "stopped");
    else if(values.period > 0 && values.timeHasPassed == 1)
        snprintf(periodInfo, sizeof(periodInfo), "suspended");

    if(values.destExists == 1) snprintf(pidInfo, sizeof(pidInfo), "receiver exists");
    else snprintf(pidInfo, sizeof(pidInfo), "receiver does not exist");

    if(values.rt >= SIGRTMIN && values.rt <= SIGRTMAX)
        snprintf(rtInfo, sizeof(rtInfo), "value accepted");
    else snprintf(rtInfo, sizeof(rtInfo), "value not accepted");

    periodInfo[MAXINFO] = '\0';
    pidInfo[MAXINFO] = '\0';
    rtInfo[MAXINFO] = '\0';

    int ret = snprintf(reportMsg, sizeof(reportMsg),"parameters:\n"
                                                    "amp     %f\n"
                                                    "freq    %f\n"
                                                    "probe   %f\n"
                                                    "period  %f   %s\n"
                                                    "pid     %hi  %s\n"
                                                    "rt      %hi  %s\n",
                                                    values.amp, values.freq,
                                                    values.probe, values.period, periodInfo,
                                                    values.pid, pidInfo, values.rt, rtInfo);
    if(ret<0)
        exit(EXIT_FAILURE);

    errno = 0;
    if(sendto(socketValues.sockfd, (const char *)reportMsg, strlen(reportMsg),
           MSG_CONFIRM, (const struct sockaddr *) &socketValues.cliaddr,
           socketValues.len) == -1)
    {
        perror("sendReport: sendto\n");
        exit(EXIT_FAILURE);
    }
}

void socketDataAnalysis(const char* data, struct steeringValues* values, struct socketData socketValues)
{
    int sign = 0;
    enum parameterToChange arg = -1;
    int spaceSign = -1;
    int newLine = 0;
    int report=0;
    while(data[sign] != '\0')
    {
        if(data[sign] == ':')
        {
            sign++;
            continue;
        }
        if(data[sign] == ' ' || data[sign] == '\t')
        {
            spaceSign=sign;
            if(data[newLine]=='a' && data[newLine + 1]=='m' && data[newLine + 2]=='p')
                arg = 0;
            else if(data[newLine]=='f' && data[newLine+1]=='r'
                && data[newLine+2]=='e' && data[newLine+3]=='q')
                arg = 1;
            else if(data[newLine+0]=='p' && data[newLine+1]=='r' && data[newLine+2]=='o'
                    && data[newLine+3]=='b' && data[newLine+4]=='e')
                arg = 2;
            else if(data[newLine] =='p' && data[newLine+1]=='e' && data[newLine+2]=='r'
                    && data[newLine+3]=='i' && data[newLine+4]=='o' && data[newLine+5]=='d')
                arg = 3;
            else if(data[newLine]=='p' && data[newLine+1]=='i' && data[newLine+2]=='d')
                arg = 4;
            else if(data[newLine]=='r' && data[newLine+1]=='t')
                arg = 5;
        }
        else if((data[sign] == '\n') && (arg >= 0 && arg <=5))
        {
            char* number = (char*)malloc((sign - spaceSign+1) * sizeof(char));
            for(int i =0; i < (sign-spaceSign); i++)
                number[i] = data[spaceSign+i+1];

            number[sign-spaceSign-1] = '\0';

            switch(arg) {
                case 0:
                    values->amp =  stringToFloatValidation(number);
                    values->restartTrigger = 1;
                    break;
                case 1:
                    values->freq =  stringToFloatValidation(number);
                    values->restartTrigger = 1;
                    break;
                case 2:
                    values->probe =  stringToFloatValidation(number);
                    break;
                case 3:
                    values->period =  stringToFloatValidation(number);
                    values->restartTrigger = 1;
                    break;
                case 4:
                    values->pid =  stringToShortValidation(number);
                    validatePID(values);
                    values->restartTrigger = 1;
                    break;
                case 5:
                    values->rt = stringToShortValidation(number);
                    if(values->rt >= SIGRTMIN && values->rt <= SIGRTMAX)
                        values->restartTrigger = 1;

                    break;
                default: break;
            }
            arg = -1;
            free(number);
        }
        else if(data[sign] == '\n' && data[sign-6] == 'r' && data[sign-5] == 'e' && data[sign-4] == 'p'
            && data[sign-3] == 'o' && data[sign-2] == 'r' && data[sign-1] == 't')
            report = 1;

        if(data[sign] == '\n')
            newLine = sign + 1;

        sign++;
    }
    if(report == 1) sendReport(*values, socketValues);
}

int main(int argc, char* argv[])
{
    if(argc!=2)
    {
        printf("Format: %s \n\
            <unsigned short> UDP port number\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    unsigned short socketPort = stringToUnsignedShortValidation(argv[1]);
    int sockfd, n;
    char buffer[MAXLINE];
    struct sockaddr_in servaddr, cliaddr;
    unsigned int len;
    double x;
    float sinValue;
    struct timespec referenceTime, currentTime, probeTime, sleep = {.tv_sec = 0, .tv_nsec = 0};
    union sigval sigValue;
    struct steeringValues* values = &(struct steeringValues)
        {
            .amp = (float)1.0, .freq = (float)0.25, .probe = (float)1., .period=(float)-1.,
            .pid = 1, .rt = 0, .destExists = 0, .restartTrigger = 0, .timeHasPassed = 0
        };

    // socket config
    errno = 0;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd == -1)
    {
        perror("socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(socketPort);

    errno = 0;
    if(bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) == -1 )
    {
        perror("bind failed\n");
        exit(EXIT_FAILURE);
    }

    len = sizeof(cliaddr);
    struct socketData socketValues = { .sockfd = sockfd, .len = len, .cliaddr = cliaddr };
    clock_gettime(CLOCK_MONOTONIC, &referenceTime);

    while(1)
    {
        // UDP socket: listening and analysing data
        errno = 0;
        n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_DONTWAIT, ( struct sockaddr *) &cliaddr, &len);
        if(n == -1 && errno != 11)
        {
            perror("recvfrom\n");
            exit(EXIT_FAILURE);
        }
        buffer[n] = '\0';

        socketValues.len = len;
        socketValues.cliaddr = cliaddr;
        socketDataAnalysis(buffer, values, socketValues);
        buffer[0] = '\0';

        if(values->restartTrigger == 1)
        {
            clock_gettime(CLOCK_MONOTONIC, &referenceTime);
            values->restartTrigger = 0;
            values->timeHasPassed = 0;
            probeTime.tv_sec = 0; probeTime.tv_nsec = 0;
        }

        if(values->period > 0 && values->timeHasPassed == 0)
        {
            x = measureTime(currentTime, referenceTime);
            if(values->period < x) values->timeHasPassed = 1;
        }

        if(validatePID(values) == 1 && (values->rt >= SIGRTMIN && values->rt <= SIGRTMAX) &&
        (values->period == 0 || (values->timeHasPassed == 0 && values->period > 0)))
        {
            x = measureTime(currentTime, probeTime);
            if(values->probe <= x)
            {
                clock_gettime(CLOCK_MONOTONIC, &probeTime);
                // generating sine wave
                clock_gettime(CLOCK_MONOTONIC, &currentTime);
                x = measureTime(currentTime, referenceTime);
                sinValue = (float) (values->amp * sin(values->freq * x * 2 * (double) PI));

                // sending sine data
                sigValue.sival_int = floatToIntValidation(sinValue);
                errno = 0;
                if (sigqueue(values->pid, values->rt, sigValue) == -1)
                {
                    perror("sigqueue\n");
                    exit(EXIT_FAILURE);
                }
            }
        }
        nanosleep(&sleep, NULL);
    }
} 
