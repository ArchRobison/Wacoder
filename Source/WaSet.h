#ifndef WaSet_H
#define WaSet_H

#include "Synthesizer.h"
#include "MidiPlayer.h"
#include <string>
#include <map>

#define HAVE_WriteWaPlot 0

//! A vocal sound
struct Wa {
    Synthesizer::Waveform waveform;
    float freq;                     // Frequency of wa in Hz
    float duration;                 // Duration of wa in seconds
};

//! A collection of Was
class WaSet: NoCopy {
public:
    //! Construct WaSet from given .wav file of was.
    WaSet( const std::string& filename );

    //! Name used to construct this WaSet
    const std::string& name() const {return myFileName;}

    //! Find closest match Wa
    const Wa* lookup( float pitch, float duration ) const;

    //! Apply f to each Wa
    template<typename F>
    void forEach(F f) const {
        for( const Wa& w: myArray )
            f(w);
    }
#if HAVE_WriteWaPlot
    // For debugging
    void writeWaPlot( const char* filename ) const;
#endif
private:
    SimpleArray<Wa> myArray;
    std::string myFileName;
};

class WaInstrument: public Midi::Instrument {
    /*override*/ void noteOn(const Midi::Event& on, const  Midi::Event& off);
    /*override*/ void noteOff(const  Midi::Event& off);
    /*override*/ void stop();
    const WaSet& myWaSet;
public:
    WaInstrument(const WaSet& w) : myWaSet(w) {}
};

class WaSetCollection {
    std::map<std::string, WaSet*> myMap;
public:
    //! Add a WaSet (and load it from the file) if it is not already present.
    WaSet* addWaSet(const std::string& name, const std::string& path);
    //! Find WaSet by name (or return nullptr) if it does not exist.
    WaSet* find( const std::string& name ) const {
        auto i = myMap.find(name);
        return i!=myMap.end() ? i->second : nullptr;
    }
    // Apply f(name,w) for each WasSet in the collection.
    template<typename F>
    void forEach( const F& f ) {
        for( const auto& item: myMap )
            f(item.first,*item.second);
    }
};

extern WaSetCollection TheWaSetCollection;

#if HAVE_WriteWaPlot
void WriteWaPlot( const char* filename, const MidiTrack& track, double ticksPerSec );
#endif

#endif /* WaSet_H */