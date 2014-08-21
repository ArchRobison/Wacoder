#ifndef Wack_H
#define Wack_H

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
class Wack: NoCopy {
public:
    Wack( const std::string& filename );
    //! Name used to construct this Wack
    const std::string& name() const {return myFileName;}
    //! Find closest match Wa
    const Wa* lookup( float pitch, float duration ) const;
    //! Number of Was
    size_t size() const {return myArray.size();}
    //! Return kth Wa
    const Wa& operator[](size_t k) const {return myArray[k];}
#if HAVE_WriteWaPlot
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
    const Wack& myWaCoder;
public:
    WaInstrument( double tickPerSec, const Wack& wc ) : myTickPerSec(tickPerSec), myWaCoder(wc) {}
};

#if HAVE_WriteWaPlot
void WriteWaPlot( const char* filename, const MidiTrack& track, double ticksPerSec );
#endif

#endif /* Wack_H */