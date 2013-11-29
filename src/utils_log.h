#ifndef UTILS_LOG_H
#define UTILS_LOG_H

#include "stdio.h"
#include <stdarg.h>
#include "string.h"

#include "utils_time.h"

//#define _DEFINE_NONLOG
//#define _DEFINE_SHOW_GET_SLICE_LOG = 1;


inline void log(const char *format, ...)
{
#ifdef _DEFINE_NONLOG
    return;
#endif

    FILE *f = stderr;

    char *data = new char [512];
    memset(data, 0, 512);

    va_list ap;
    va_start(ap, format);
    vsnprintf(data, 512, format, ap);
    va_end(ap);

//    char *timestamp = time_stamp_char();
//    fprintf(f, "%s:\t%s\n", timestamp,  data);

    char *t2 = time_stamp_char_ms();
    fprintf(f, "%s:\t%s\n", t2, data);

    fflush(f);
//    delete [] timestamp;
    delete [] data;
    delete [] t2;
}



#endif // UTILS_LOG_H
