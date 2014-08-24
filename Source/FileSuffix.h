#ifndef FileSuffix_H
#define FileSuffix_H

#include <cstring>
#include <string>
#include "AssertLib.h"

//! File suffix after ".", converted to lower case.
/** Handles suffixes up to length 7.  For longer suffixes, behaves as if there were no suffix. */
class FileSuffix {
    char suffix[8];
    void initialize(const char* filename);
public:
    FileSuffix(const char* filename) { initialize(filename); }
    FileSuffix(const std::string& filename) { initialize(filename.c_str()); }
    // True if filename had a suffix
    operator bool() const { return suffix[0]!=0; }
    // True if suffix matches given extension.  Extension should not inclukde the ".".
    bool operator==(const char* ext) const {
        Assert(ext[0]!='.');
        return std::strcmp(suffix, ext)==0;
    }
    bool operator!=(const char* ext) const {
        return !(*this==ext);
    }
};

#endif /* FileSuffix_H */