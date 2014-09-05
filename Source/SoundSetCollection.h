#ifndef SoundSetCollection_H
#define SoundSetCollection_H

#include "Orchestra.h"
#include "Drum.h"
#include <map>

class SoundSetCollection {
    std::map<std::string, Synthesizer::SoundSet*> myMap;
public:
    typedef Synthesizer::SoundSet SoundSet;
    //! Add a SoundSet (and load it from the file) if it is not already present.
    SoundSet* addSoundSet(const std::string& name, const std::string& path);
    //! Find WaSet by name (or return nullptr) if it does not exist.
    SoundSet* find( const std::string& name ) const {
        auto i = myMap.find(name);
        return i!=myMap.end() ? i->second : nullptr;
    }
    // Apply f(name,w) for each SoundSet in the collection.
    template<typename F>
    void forEach( const F& f ) {
        for( const auto& item: myMap )
            f(item.first,*item.second);
    }
};

extern SoundSetCollection TheSoundSetCollection;

#endif /* SoundSetCollection_H */
