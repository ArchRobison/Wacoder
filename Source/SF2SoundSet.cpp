#include "SF2SoundSet.h"
#include "AssertLib.h"
#include "Synthesizer.h"
#include "ReadError.h"
#include "PoolAllocator.h"
#include "SF2Bank.h"
#include "Midi.h"
#include <cstdio>
#include <cstdint>

using namespace Synthesizer;

//! Source based on a Patch object.
class SF2Source: public Synthesizer::Source {
public:
private:
    typedef Synthesizer::Waveform Waveform;
    const Waveform* waveform;
    float volume;
    Waveform::timeType waveDelta;
    Waveform::timeType waveIndex;
    Waveform::timeType loopStart;
    Waveform::timeType loopEnd;
    Waveform::timeType tableEnd;
    bool exitLoopOnRelease;
#if ASSERTIONS
    bool assertOkay();
#endif
    /*override*/ unsigned update( float* acc, unsigned n );
    /*override*/ void destroy();
    /*override*/ void receive( const Synthesizer::PlayerMessage& m );
public:
    static SF2Source* allocate( const SF2SoundSet& set, unsigned note, unsigned velocity );
    bool isLooping() const {return loopStart<~0u;}
    void release();
};

Midi::Instrument* SF2SoundSet::makeInstrument() const {
    return new SF2Instrument(*this);
}

//-----------------------------------------------------------
// SF2Instrument
//-----------------------------------------------------------

SF2Instrument::SF2Instrument(const SF2SoundSet& s): mySet(s) {
    for( int note=0; note<128; ++note )
        keyArray[note] = nullptr;
}
void SF2Instrument::noteOn( const Event& on, const Event& /*off*/ ) {
    unsigned note = on.note();
    unsigned velocity = on.velocity();
    Assert(!keyArray[note]);
    if(SF2Source* k = SF2Source::allocate(mySet, note, velocity)) {
        if(k->isLooping()) {
            // Source must be explicitly released
            keyArray[note] = k;
        } else {
            // Source must *not be explicitly released, since it may be destroyed before the release.
        }
        Play(k);
    }
}

void SF2Instrument::noteOff( const Event& off ) {
    release(off.note());
}

void SF2Instrument::release( int note ) {
    if( auto*& k = keyArray[note] ) {
        k->release();
        k = nullptr;
    }
}

void SF2Instrument::stop() {
    for( int note=0; note<128; ++note )
        release(note);
}

SF2Instrument::~SF2Instrument() {
    stop();
}

//-----------------------------------------------------------
// SF2SoundSet::noteVelocityMap
//-----------------------------------------------------------

void SF2SoundSet::noteVelocityMap::initialize( std::pair<noteVelocityRange,playInfo> data[], size_t n ) {
    Assert(n>0);
    std::sort(data,data+n,orderByLow());
    auto* i = data+0;
    for( auto j=i+1; j<data+n; ++j ) {
        // i points to last record to be retained. 
        // j points to next record to either merge into *i, or become the next last record.
        if( i->second==j->second ) {
            // See if two records can be merged
            Assert(i->first.note.low<=j->first.note.low);
            if( i->first.velocity==j->first.velocity && i->first.note.high+1>=j->first.note.low ) {
                // Merge records
                i->first.note.high = j->first.note.high;
                continue;
            }
        }
        ++i;
        *i = *j;
    }
    ++i;
    array.resize(i-data);
    for( size_t k=0; k<array.size(); ++k ) 
        array[k] = data[k];
}

const SF2SoundSet::playInfo& SF2SoundSet::noteVelocityMap::find( unsigned note, unsigned velocity ) const {
    auto* end = array.end();
    for( const auto* p=array.begin(); p!=end; ++p )
        if( p->first.note.contains(note) && p->first.velocity.contains(velocity) )
            return p->second;
    // Construction should ensure that all rangage are covered.
    Assert(0);
    return end[-1].second;
}

//-----------------------------------------------------------
// PatchSource
//-----------------------------------------------------------
static PoolAllocator<SF2Source> SF2SourceAllocator(64,false);

SF2Source* SF2Source::allocate( const SF2SoundSet& set, unsigned note, unsigned velocity ) {
    SF2Source* s = SF2SourceAllocator.allocate();
    if( s ) {
        new(s) SF2Source; 
        auto& preset = set.myPresetMap.find(note,velocity);
        auto& inst = set.myInstrumentMap[preset.index].find(note,velocity);
        auto& sample = set.mySamples[inst.index];
        int originalKey = inst.overridingRootKey>=0 ? inst.overridingRootKey : sample.myOriginalPitch;
        if( set.myIsDrum )
            note = originalKey;
        // FIXME - really need only one pow in statement below, instead of three.
        float relativeFrequency = Midi::PitchOfNote(note)/Midi::PitchOfNote(originalKey)*pow(2.0f,sample.myPitchCorrection*(1.0f/1200));
        s->waveform = &sample;
        s->waveIndex = 0;
        s->waveDelta = unsigned( sample.sampleRate()/Synthesizer::SampleRate*Waveform::unitTime*relativeFrequency + 0.5f);
        s->volume = velocity*(1.0f/127);
        Assert( s->waveDelta<=Waveform::unitTime*256 ); // Sanity check
        Assert( s->waveDelta>=Waveform::unitTime/256 ); // Sanity check
        s->tableEnd = sample.size() << SF2Sample::timeShift;
        if( inst.sampleModes&1 ) {
            s->loopStart = sample.myLoopStart;
            s->loopEnd = sample.myLoopEnd;
        } else {
            s->loopStart = ~0u;
            s->loopEnd = ~0u;
        }
        s->exitLoopOnRelease = inst.sampleModes==3;
        Assert(s->assertOkay());
    } 
    return s;
}

void SF2Source::destroy() {
    SF2SourceAllocator.destroy(this);
}

void SF2Source::release() {
    Assert( (size_t(player)&3)==0 );
    PlayerMessage* m = PlayerMessageQueue.startPush();
    m->kind = PlayerMessageKind::Release;
    m->player = player;
    PlayerMessageQueue.finishPush();
}

void SF2Source::receive( const PlayerMessage& m ) {
    Assert( m.kind==PlayerMessageKind::Release );
    if(exitLoopOnRelease)
        loopEnd = ~0u;
    else 
#if 1
        // FIXME
        loopEnd = ~0u;
#else
        Assert(0);  // Not yet implemented 
#endif
}

#if ASSERTIONS
bool SF2Source::assertOkay() {
    Assert( waveIndex < loopEnd );
    Assert( loopStart<=loopEnd );
    Assert( !isLooping() || waveDelta < (loopEnd-loopStart)/2 );    // Nyquest limit on looping
    return true;
}
#endif

unsigned SF2Source::update( float* acc, unsigned requested ) {
    Assert(assertOkay());
    unsigned n = requested;
    while(n>0 && waveIndex<tableEnd) {
        // Set d to maximum time difference that can be covered before state change.
        Waveform::timeType d;
        if( loopEnd==~0u ) {
            d = tableEnd-waveIndex;
        } else {
            Assert(loopEnd<=tableEnd);
            d = loopEnd-waveIndex;
        }
        unsigned m = (d+waveDelta-1)/waveDelta;
        bool newState = m<=n;   // True if state machine should be advanced after calling resample
        if( newState ) {
            n -= m;
        } else {
            m = n;
            n = 0;
        }
#pragma warning( disable : 4146 )
        waveIndex = waveform->resample(acc, volume, waveIndex, waveDelta, m);
        acc += m;
        if( waveIndex>=loopEnd ) {
            Assert(waveIndex-waveDelta<loopEnd);
            Assert(loopEnd<=waveIndex);
            waveIndex -= loopEnd-loopStart;
            Assert(loopStart <= waveIndex);
            Assert(waveIndex-waveDelta<loopStart);
        }
        Assert(assertOkay());
    }
    return requested-n;
}
