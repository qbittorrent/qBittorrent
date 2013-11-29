#ifndef UTILS_TIME_H
#define UTILS_TIME_H

#include "sys/time.h"
#include "string.h"

inline char *time_stamp_char()
{
//    char *timestamp = (char *)malloc(sizeof(char) * 21);
    char *timestamp = new char [sizeof(char) * 21];
    time_t ltime;
    ltime=time(NULL);
    struct tm *tm;
    tm=localtime(&ltime);
    sprintf(timestamp,"%04d-%02d-%02d %02d:%02d:%02d", tm->tm_year+1900, tm->tm_mon+1,
        tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    return timestamp;
}

inline char *time_stamp_char_ms()
{
    int size = 256;
    char *timestamp = new char [size];
    memset(timestamp, 0, size);

    time_t ltime;
    ltime=time(NULL);

    struct timeval tv;
    struct tm *tm;
    gettimeofday(&tv, NULL);
    tm=localtime(&ltime);
//    tm = localtime(&tv.tv_sec);
    sprintf(timestamp,"%04d-%02d-%02d %02d:%02d:%02d.%03ld", tm->tm_year+1900, tm->tm_mon, tm->tm_mday, tm->tm_hour,
                                                             tm->tm_min, tm->tm_sec, tv.tv_usec/1000);
    return timestamp;
}

inline char *time_stamp_char(tm *tm)
{
    char *timestamp = (char *)malloc(sizeof(char) * 21);
    sprintf(timestamp,"%04d-%02d-%02d %02d:%02d:%02d", tm->tm_year+1900, tm->tm_mon,
        tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    return timestamp;
}

inline tm time_stamp_tm()
{
    time_t ltime;
    ltime=time(NULL);
    struct tm tt;
    tt = *localtime(&ltime);
    return tt;
}

inline int msleep(unsigned long milisec)
{
    struct timespec req;
    memset(&req,0,sizeof(req));
    time_t sec=(int)(milisec/1000);
    milisec=milisec-(sec*1000);
    req.tv_sec=sec;
    req.tv_nsec=milisec*1000000L;
    while(nanosleep(&req,&req)==-1)
        continue;
    return 1;
}

inline int slp(unsigned long milisec)
{
    struct timespec req;
    memset(&req,0,sizeof(req));
    time_t sec=(int)(milisec/1000);
    milisec=milisec-(sec*1000);
    req.tv_sec=sec;
    req.tv_nsec=milisec*1000000L;
    while(nanosleep(&req,&req)==-1)
        continue;
    return 1;
}

#endif // UTILS_TIME_H
