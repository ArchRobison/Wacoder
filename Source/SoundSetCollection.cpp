#include "SoundSetCollection.h"
#include "WaSet.h"
#include "FileSuffix.h"
#include "ReadError.h"

Synthesizer::SoundSet* SoundSetCollection::addSoundSet(const std::string& name, const std::string& path) {
    auto i = myMap.find(name); 
    if( i==myMap.end() ){
        FileSuffix fs(path);
        SoundSet * s = nullptr;
        try {
#if 0
            if(fs=="pat") {
                Patch* p = new Patch(path);
                if(0&&p->size()==1) {
                   // myDrumSet[?] = 
                    // s = new DrumSet(p);
                } else
                    s = new ToneSet(p);
            }
#endif
            if(fs=="wav")
                s = new WaSet(path);
        } catch( const ReadError& ) {
            // FIXME report message to user
        }
        if(s) 
            myMap.insert(std::make_pair(name, s));
        return s;
    } else {
        return i->second;
    }
}

SoundSetCollection TheSoundSetCollection;