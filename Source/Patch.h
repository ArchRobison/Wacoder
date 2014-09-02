#ifndef Patch_H
#define Patch_H

#include "Waveform.h"
#include "Utility.h"
#include "MidiPlayer.h"
#include <string>

class Patch;

class PatchSample: public Synthesizer::Waveform {
    // ~timeType(0) if not a looping patch.
    timeType myLoopStart;
    // ~timeType(0) if not a looping patch.
    timeType myLoopEnd;
    float myHighFreq;
    float myLowFreq;
    // Nominal pitch
    float myRootFreq;  
    float mySampleRate;
    friend class Patch;
public:
    bool isLooping() const {
        return myLoopStart<~timeType(0);
    }
    timeType loopStart() const {
        Assert(isLooping());
        return myLoopStart;
    }
    timeType loopEnd() const {
        Assert(isLooping());
        return myLoopEnd;
    }
    timeType soundEnd() const {return size()<<Waveform::timeShift;}
    float sampleRate() const {return mySampleRate;}
    float pitch() const {return myRootFreq;}
};

class Patch {
    SimpleArray<PatchSample> mySamples;
    std::string myReadStatus;
    std::string myInstrumentName;
    class parser;
public:
    const PatchSample* begin() const {return mySamples.begin();}
    const PatchSample* end() const {return mySamples.end();} 
    size_t size() const {return mySamples.size();}
    bool empty() const {return size()==0;}
    const PatchSample& operator[]( size_t k ) {return mySamples[k];}
    bool readFromFile( const std::string& filename );
    const std::string& readStatus() const {return myReadStatus;}
    const PatchSample& find( float freq ) const;
};

namespace Synthesizer {
    class PatchSource;
}

class PatchInstrument: public Midi::Instrument {
    Synthesizer::PatchSource* keyArray[128];
    const Patch& myPatch;
    typedef Midi::Event Event;
    /*override*/ void noteOn( const Event& on, const Event& off );
    /*override*/ void noteOff( const Event& off);
    /*override*/ void stop();
    void release( int note );
public:
    PatchInstrument( const Patch& patch );
    ~PatchInstrument();
};

#endif /* Patch_H */