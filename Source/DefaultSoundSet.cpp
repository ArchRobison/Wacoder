#include "DefaultSoundSet.h"
#include "ReadError.h"
#include "Patch.h"
#include "Drum.h"
#include "ToneSet.h"
#include <cstdio>

static void SkipSpace( char*& p ) {
    while( isspace(*p) )
        ++p;
}

static bool SkipTok( char*& p, const char* s ) {
    char* q = p;
    for(;;++q,++s) {
        if( !*s ) {
            p = q;
            return true;
        }
        if( *q!=*s )
            return false;
    }
}

static bool ReadLine( char buf[], size_t bufsize, FILE* f ) {
    if( fgets(buf,bufsize,f) ) {
        if(!strchr(buf, '\n')) {
            // Line was too long.  Just ignore the extra characters.
            int c;
            do {
                c = getc(f);
            } while(c!='\n' && c!=EOF);
        }
        return true;
    } else {
        return false;
    }
}

enum class BankKind {
    toneset,
    drumset,
    unknown
};

static std::string FreePatDir;

struct PatchInfo {
    std::string subpath;
    Synthesizer::SoundSet* soundSet;
    PatchInfo() : soundSet(nullptr) {}
};

static PatchInfo FreePatCache[256];

Synthesizer::SoundSet* GetDefaultSoundSet( unsigned midiNumber ) {
    Assert(midiNumber<256);
    auto& rec = FreePatCache[midiNumber];
    if( !rec.soundSet && !rec.subpath.empty() ) {
        // Not loaded yet.  Try loading it.
        Patch* patch = new Patch(FreePatDir+rec.subpath); 
        if( midiNumber>=128 )
            rec.soundSet = new Drum(patch);
        else
            rec.soundSet = new ToneSet(patch);
    }
    return rec.soundSet;
}

void ReadFreePatConfig( const std::string& path ) {
    FILE* f = std::fopen(path.c_str(),"r");
    if(!f) {
        // FIXME - report error
        throw ReadError(path + strerror(errno));
    }
    FreePatDir = path.substr(0,path.find_last_of("\\:")+1);
    char buf[1024];
    BankKind bank = BankKind::unknown;
    while(ReadLine(buf,1024,f)) {
        char* p = buf;
        SkipSpace(p);
        if( !*p || *p=='#' )
            // Blank line or comment
            continue;
        if( SkipTok(p,"drumset") ) {
            bank = BankKind::drumset;
            continue;   // Ignore following number - we handle only one drumset
        }
        if( SkipTok(p,"bank") ) {
            bank = BankKind::toneset;
            continue;   // Ignore following number - we handle only one drumset
        }
        if( isdigit(*p) ) {
            if( bank==BankKind::unknown )
                continue;
            char* q;
            long midiNumber = strtol(p,&q,10);
            p = q;
            if( midiNumber>=128 ) 
                continue;
            SkipSpace(p);
            q = p;
            while(*q && !isspace(*q)) ++q;
            auto& rec = FreePatCache[bank==BankKind::drumset ? midiNumber+128 : midiNumber];
            rec.subpath = std::string(p,q);
            rec.soundSet = nullptr;         // FIXME - should free previous SoundSet.  Or use unique_ptr for it.
        }
    }
    std::fclose(f);
}