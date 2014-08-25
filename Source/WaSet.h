#ifndef WaSet_H
#define WaSet_H

#include "Synthesizer.h"
#include "Midi.h"
#include <string>

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

class WaInstrument: public MidiInstrument {
    /*override*/ void processEvent();
    /*override*/ void stop();
    const double myTickPerSec;
    const WaSet& myWaCoder;
public:
    WaInstrument( double tickPerSec, const WaSet& wc ) : myTickPerSec(tickPerSec), myWaCoder(wc) {}
};

#if HAVE_WriteWaPlot
void WriteWaPlot( const char* filename, const MidiTrack& track, double ticksPerSec );
#endif

#endif /* WaSet_H */