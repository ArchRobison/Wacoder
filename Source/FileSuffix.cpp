#include "FileSuffix.h"
#include <ctype.h>

void FileSuffix::initialize(const char* filename) {
    const char* s = nullptr;
    const char* t = filename;
    for(; *t; ++t)
        if(*t=='.')
            s = t+1;
    if(s && t-s<=7) {
        // Found a suffix, and it's short enough to be interesting.
        char* t = suffix;
        while(*s)
            *t++ = tolower(*s++);
        *t++ = '\0';
    } else {
        suffix[0] = 0;
    }
}