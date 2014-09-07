#ifndef Synthesizer_H
#define Synthesizer_H

#include "Utility.h"
#include "Waveform.h"
#include "NonblockingQueue.h"
#include <new>
#include <cstdint>

class PatchSample;

namespace Synthesizer {

class Envelope: public SampledSignalBase<float,20> {
public:
    Envelope() : myIsSustain(2) {}
    ~Envelope() {}
    bool isSustain() const {
        Assert(isCompleted());
        return myIsSustain!=0;
    }
    void complete( bool sustain ) {
        sampleType* e = end();
        e[0] = sustain ? e[-1] : 0;
        myIsSustain = sustain;
    }
    bool isCompleted() const {return myIsSustain<2;}
private:
    /** 0-> release channel when reaching end of envelope.
        1-> sustain channel when reaching end of envelope.
        2-> method complete has not been called yet */
    char myIsSustain;
};

enum class PlayerMessageKind: char {
    Start,
    ChangeVolume,                   // Used by DynamicSource
    ChangeEnvelope,                 // Used by AsrSource
    Release                         // Used by PatchSource
};

class Player;

class PlayerMessage {   // FIXME - find a better name
public:
    PlayerMessageKind kind;             // Really a PlayerMessageKind
    Player* player;
    union {
        struct {                        // kind==WMK_ChangeEnvelope
            const Envelope* envelope;
            Envelope::timeType envDelta;                     
        } midi;
        struct {                        // kind==WMK_ChangeVolume
            float newVolume;  
            unsigned deadline:31;
            unsigned release:1;
        } dynamic; 
    };
};

extern NonblockingQueue<PlayerMessage> PlayerMessageQueue;

class Player;
class PlayerMessage;

//! Sound source that can be played
class Source: NoCopy {
protected:
    Player* player;
    friend void Play( Source* src, float volume, float x, float y );
    friend void OutputInterruptHandler( Waveform::sampleType* left, Waveform::sampleType* right, unsigned n );
    friend class Player;
    //! Set acc[0:n] to next n samples (or fewer if src has reached its end).  Returns nmber of samples created
    virtual size_t update( float* acc, unsigned n ) = 0;
    virtual void destroy() = 0;
    //! Asynchronously called when a message is received by the interrupt handler.
    virtual void receive( const PlayerMessage& m ) = 0;
public:
};

//! Sound source that plays back pre-recorded waveform, without elaborate modifications.
class SimpleSource: public Source {
    const Waveform* waveform;
    unsigned short waveLowIndex;    // Low 16 bits, i.e. bits after the fixed-point
    unsigned int waveHighIndex;     // Upper 32 bits, i.e. integer part of index
    Waveform::timeType waveDelta; 
    /*override*/ unsigned update( float* acc, unsigned n ); 
    /*override*/ void destroy();  
    /*override*/ void receive( const PlayerMessage& m );
public:
    //! Construct source from given waveform, to be played at relative frequency freq.  Default is to play at original frequency.
    static SimpleSource* allocate( const Waveform& w, float freq=1.0f );
};

//! Source whose volume can set on the fly in a linear piece-wise fashion.
class DynamicSource: public Source {
private:
    const Waveform* waveform;
    Waveform::timeType waveIndex;
    Waveform::timeType waveDelta;
    float currentVolume;
    float targetVolume;
    unsigned deadline:31;
    unsigned release:1;
    /*override*/ unsigned update( float* acc, unsigned n );
    /*override*/ void destroy();
    /*override*/ void receive( const PlayerMessage& m );
public:
    static DynamicSource* allocate( const Waveform& w, float freq=1.0f );
    //! Cause volume to change smoothly to given value by given deadline.
    /** Deadline measured in sec. */
    void changeVolume( float newVolume, float deadline, bool release=false );
};

//! Deprecated.  Use PatchSource instead.
/** Source whose envelope can be set on the fly. 
    Useful for Attack-sustain-release synthesis. */
class AsrSource: public Source {
    const Waveform* waveform;
    Waveform::timeType waveIndex;
    Waveform::timeType waveDelta;
    const Envelope* envelope;
    Envelope::timeType envIndex;
    Envelope::timeType envDelta;
    const Envelope* relEnvelope;
    Envelope::timeType relEnvDelta;
    /*override*/ unsigned update( float* acc, unsigned n );
    /*override*/ void destroy();
    /*override*/ void receive( const PlayerMessage& m );
public:
    static AsrSource* allocate( const Waveform& w, float freq, const Envelope& attack, float speed=1.0f );
    void changeEnvelope(Envelope& e, float speed=1.0f );
};

//! Intialize synthesizer global structures.
void Initialize();

//! Fill left and right with next n samples
void OutputInterruptHandler( Waveform::sampleType* left, Waveform::sampleType* right, unsigned n );

void InputInterruptHandler( const Waveform::sampleType* wave, size_t n );

//! Start playing src.  Method src->destroy() will be invoked after src->update() returns.
/** No-op if src is NULL.  Doing so allows clients to SimplesSource to not have to check
    whether SimpleSource::allocate returns NULL. */
void Play( Source* src, float volume=1.0f, float x=0, float y=1.0f );

} // namespace Synthesizer

#define INJECT_SOUND 0

class SoundInjector {
public:
    void inject( float* p, size_t n ) {
#if INJECT_SOUND
        for(size_t i=0; i<n; ++i) {
            float y = sin(0.0625f*k++);
            p[i] = y;
        }
#endif
    }
    void check( const float* p, size_t n ) {
#if INJECT_SOUND
        for(size_t i=0; i<n; ++i) {
            float y = sin(0.0625f*k++);
            Assert(p[i]==y);
        }
#endif
    }
    SoundInjector() : k(0) {}
private:
    unsigned k;
};

#endif /* Synthesizer_H */