#ifndef Patch_H
#define Patch_H

#include "Synthesizer.h"
#include "Utility.h"
#include "Orchestra.h"
#include <string>

class Patch;
class PatchSample;
class PatchSource;

//! Module for finite-state machine used to implement looping and ping-ponging patches.
class PatchStateOps {
protected:
    enum class stateType : uint8_t {
        forwardFinal=0,       // Go forwards until tableEnd is reached, then finished
        forwardBounce=1,      // Go forwards until loopEnd is reached, then switch to reverseBounce
        reverseFinal=2,       // Go backwards until loopStart is reached, then switch to forwarFinal 
        reverseBounce=3,      // Go backwards until loopStart is reached, then switch to forwardBounce
        finished=4,           // Done
        forwardLoop=5,        // Go forwards until loopEnd is reached, then jump to loopStart
    };
    //! True for states that walk backwards through wave table.
    static bool isReverse(stateType s) {
        Assert(unsigned(s)<=5);
        return (unsigned(s) & 2)!=0;
    }
    //! True for states that are part of loop.
    static bool isLooping(stateType s) {
        Assert(unsigned(s)<=5);
        return (unsigned(s)&1) != 0;
    }
    //! Return successor state for bouncing from current state.
    /** Invalid for non-bouncing states. */
    static stateType bounce(stateType s) {
        Assert(s==stateType::forwardBounce||s==stateType::reverseFinal||s==stateType::reverseBounce);
        return stateType(uint8_t(s)^2);
    }
    //! Return successor state for getting out of loop.
    /** Invalid for non-looping states. */
    static stateType release(stateType s) {
        Assert(isLooping(s));
        return stateType(unsigned(s)&~5u);
    }
};

class PatchSample: public Synthesizer::Waveform, PatchStateOps {
    // ~timeType(0) if not a looping/ping-pong patch.
    timeType myLoopStart;
    // ~timeType(0) if not a looping/ping-pong patch.
    timeType myLoopEnd;
    float myHighFreq;
    float myLowFreq;
    // Nominal pitch
    float myRootFreq;  
    float mySampleRate;  
    stateType myInitialState;
    friend class Patch;
    friend class PatchSource;
public:
    bool isLooping() const {
        return PatchStateOps::isLooping(myInitialState); 
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

//! Base class for ToneInstrument and DrumInstrument
class PatchInstrument: public Midi::Instrument {   
protected:
    typedef Midi::Event Event;
    PatchInstrument();
    ~PatchInstrument();
    void playSource( const PatchSample& ps, int note, float relFreq, float volume );
private:
    PatchSource* keyArray[128]; 
    /*override*/ void noteOff( const Event& off);
    /*override*/ void stop();
    void release( int note );
};

#endif /* Patch_H */