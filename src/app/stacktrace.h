// stacktrace.h (c) 2008, Timo Bingmann from http://idlebox.net/
// published under the WTFPL v2.0

#ifndef _STACKTRACE_H_
#define _STACKTRACE_H_

#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>
#include <cxxabi.h>

#include <vector>

/** Print a demangled stack backtrace of the caller function to FILE* out. */
static inline void print_stacktrace(FILE *out = stderr, unsigned int max_frames = 63)
{
    fprintf(out, "Stack trace:\n");

    // storage array for stack trace address data
    std::vector<void *> addrlist(max_frames + 1);

    // retrieve current stack addresses
    int addrlen = backtrace(addrlist.data(), addrlist.size());

    if (addrlen == 0)
    {
        fprintf(out, "  <empty, possibly corrupt>\n");
        return;
    }

    // resolve addresses into strings containing "filename(function+address)",
    // this array must be free()-ed
    char * *symbollist = backtrace_symbols(addrlist.data(), addrlen);

    // allocate string which will be filled with the demangled function name
    size_t funcnamesize = 256;
    char *funcname = static_cast<char *>(malloc(funcnamesize));

    int functionNamesFound = 0;
    // iterate over the returned symbol lines. skip the first, it is the
    // address of this function.
    for (int i = 2; i < addrlen; i++)
    {
        char *begin_name = 0, *begin_offset = 0, *end_offset = 0;

        // find parentheses and +address offset surrounding the mangled name:
        // ./module(function+0x15c) [0x8048a6d]
        // fprintf(out, "%s TT\n", symbollist[i]);
        for (char *p = symbollist[i]; *p; ++p)
        {
            if (*p == '(')
            {
                begin_name = p;
            }
            else if (*p == '+')
            {
                begin_offset = p;
            }
            else if ((*p == ')') && begin_offset)
            {
                end_offset = p;
                break;
            }
        }

        if (begin_name && begin_offset && end_offset
            && (begin_name < begin_offset))
            {
            *begin_name++ = '\0';
            *begin_offset++ = '\0';
            *end_offset = '\0';

            // mangled name is now in [begin_name, begin_offset) and caller
            // offset in [begin_offset, end_offset). now apply
            // __cxa_demangle():

            int status;
            char *ret = abi::__cxa_demangle(begin_name,
                                            funcname, &funcnamesize, &status);
            if (status == 0)
            {
                funcname = ret; // use possibly realloc()-ed string
                fprintf(out, "  %s : %s+%s %s\n",
                        symbollist[i], funcname, begin_offset, ++end_offset);
            }
            else
            {
                // demangling failed. Output function name as a C function with
                // no arguments.
                fprintf(out, "  %s : %s()+%s %s\n",
                        symbollist[i], begin_name, begin_offset, ++end_offset);
            }
            ++functionNamesFound;
        }
        else
        {
            // couldn't parse the line? print the whole line.
            fprintf(out, "  %s\n", symbollist[i]);
        }
    }

    if (!functionNamesFound)
    {
        fprintf(out, "There were no function names found in the stack trace\n."
            "Seems like debug symbols are not installed, and the stack trace is useless.\n");
    }
    if (functionNamesFound < addrlen - 2)
    {
        fprintf(out, "Consider installing debug symbols for packages containing files with empty"
            " function names (i.e. empty braces \"()\") to make your stack trace more useful\n");
    }
    free(funcname);
    free(symbollist);
}

#endif // _STACKTRACE_H_
