#ifndef WaSet_H
#define WaSet_H

#include "Synthesizer.h"
#include "Orchestra.h"
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
class WaSet: public Synthesizer::SoundSet {
public:
    //! Construct WaSet from given .wav file of was.
    WaSet( const std::string& wavFilename );

    //! Find closest match Wa
    const Wa* lookup( float pitch, float duration ) const;

    //! Apply f to each Wa
    template<typename F>
    void forEach(F f) const {
        for( const Wa& w: myArray )
            f(w);
    }

    /*override*/ Midi::Instrument* makeInstrument() const;

#if HAVE_WriteWaPlot
    // For debugging
    void writeWaPlot( const char* filename ) const;
#endif
private:
    SimpleArray<Wa> myArray;
};

#if HAVE_WriteWaPlot
void WriteWaPlot( const char* filename, const MidiTrack& track, double ticksPerSec );
#endif

#endif /* WaSet_H */