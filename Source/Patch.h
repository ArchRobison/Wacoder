#ifndef Patch_H
#define Patch_H

#include "Synthesizer.h"
#include "Utility.h"
#include "Orchestra.h"
#include <string>

class Patch;

class PatchSample: public Synthesizer::Waveform {
    // ~timeType(0) if not a looping/ping-pong patch.
    timeType myLoopStart;
    // ~timeType(0) if not a looping/ping-pong patch.
    timeType myLoopEnd;
    float myHighFreq;
    float myLowFreq;
    // Nominal pitch
    float myRootFreq;  
    float mySampleRate;  
    typedef Synthesizer::PatchSource::stateType stateType;
    stateType myInitialState;
    friend class Patch;
    friend class Synthesizer::PatchSource;
public:
    bool isLooping() const {
        return Synthesizer::PatchSource::isLooping(myInitialState); 
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
    std::string myInstrumentName;
    class parser;
    void readFromFile( const std::string& filename );
public:
    Patch( const std::string& filename );
    const PatchSample* begin() const {return mySamples.begin();}
    const PatchSample* end() const {return mySamples.end();} 
    size_t size() const {return mySamples.size();}
    bool empty() const {return size()==0;}
    const PatchSample& operator[]( size_t k ) const {return mySamples[k];}
    const PatchSample& find( float freq ) const;
    /*override*/ Midi::Instrument* makeInstrument() const;
};

namespace Synthesizer {
    class PatchSource;
};

//! Base class for ToneInstrument and DrumInstrument
class PatchInstrument: public Midi::Instrument {   
protected:
    typedef Midi::Event Event;
    PatchInstrument();
    ~PatchInstrument();
    void playSource( const PatchSample& ps, int note, float relFreq, float volume );
private:
    Synthesizer::PatchSource* keyArray[128]; 
    /*override*/ void noteOff( const Event& off);
    /*override*/ void stop();
    void release( int note );
};

#endif /* Patch_H */