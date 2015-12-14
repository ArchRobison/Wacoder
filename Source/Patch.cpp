#include "Patch.h"
#include "AssertLib.h"
#include "Synthesizer.h"
#include "ReadError.h"
#include "PoolAllocator.h"
#include <cstdio>
#include <cstdint>
#include <cerrno>

using namespace Synthesizer;

//! Source based on a Patch object.
class PatchSource: public Synthesizer::Source, PatchStateOps {
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
    stateType state;
#if ASSERTIONS
    bool assertOkay();
#endif
    /*override*/ unsigned update( float* acc, unsigned n );
    /*override*/ void destroy();
    /*override*/ void receive( const Synthesizer::PlayerMessage& m );
public:
    static PatchSource* allocate( const PatchSample& patch, float relativeFrequency, float volume );
    void release();
};

PatchInstrument::PatchInstrument() {
    for( int note=0; note<128; ++note )
        keyArray[note] = nullptr;
}

void PatchInstrument::playSource( const PatchSample& ps, int note, float relFreq, float volume ) {
    Assert(!keyArray[note]);
    if( PatchSource* k = PatchSource::allocate(ps, relFreq, volume) ) {
        Play(k);
        if(ps.isLooping()) {
            // Source must be explicitly released
            keyArray[note] = k;
        } else {
            // Source must *not be explicitly released, since it may be destroyed before the release.
        }
    }
}

void PatchInstrument::noteOff( const Event& off ) {
    release(off.note());
}

void PatchInstrument::release( int note ) {
    if( auto*& k = keyArray[note] ) {
        k->release();
        k = nullptr;
    }
}

void PatchInstrument::stop() {
    for( int note=0; note<128; ++note )
        release(note);
}

PatchInstrument::~PatchInstrument() {
    stop();
}

class Patch::parser: PatchStateOps {
    Patch& patch;
    uint8_t samplingMode;
    float scaleFactor;
    enum SamplingModeFlags {
        SM_16bit=1<<0,
        SM_Unsigned=1<<1,
        SM_Looping=1<<2,
        SM_PingPong=1<<3,
        SM_Reverse=1<<4,
        SM_Sustain=1<<5,
        SM_Envelope=1<<6,
        SM_ClampedRelease=1<<7
    };
    void throwError(const char* format, unsigned value=0);
    const uint8_t* parseSample(PatchSample& ps, const uint8_t* first, const uint8_t* last);
    static unsigned get4( const uint8_t* first ) {
        return first[0] | first[1]<<8 | first[2]<<16 | first[3]<<24;
    }
    unsigned get4asSampleCount( const uint8_t* first, bool acceptOdd=false ) {
        unsigned bytes = get4(first);
        Assert( (samplingMode & SM_16bit)==0 || bytes%2==0 || acceptOdd );
        return bytes >> (samplingMode&1);
    }
    //! Get frequency from raw 4 bytes and convert it to hertz.
    float get4asFrequency( const uint8_t* first ) {
        unsigned value = get4(first);
        return value/scaleFactor;   
    }
    static unsigned get2( const uint8_t* first ) {
        return first[0] | first[1]<<8;
    }
    const uint8_t* convertSamples( float* dst, const uint8_t* first, size_t n );
public:
    parser( Patch& patch_ ) : patch(patch_) {}
    void parseFile( const uint8_t* first, const uint8_t* last );
};

const uint8_t* Patch::parser::convertSamples( float* dst, const uint8_t* first, size_t n ) {
    bool isUnsigned = (samplingMode & SM_Unsigned)!=0;
    for(size_t i=0; i<n; ++i) {
        int value = first[0] | first[1]<<8;
        // Convert to normalized float data
        dst[i] = (isUnsigned ? float(value-0x8000) : float(int16_t(value)))*(1.0f/0x8000);
        first += 2;
    }
    return first;
}

const uint8_t* Patch::parser::parseSample(PatchSample& ps, const uint8_t* first, const uint8_t* last) {
    if( last-first<96 )
        throwError("sample header truncated");
    uint8_t fractions = first[7];
    samplingMode = first[55];
    scaleFactor = get2(first+58);
    uint32_t dataSize = get4asSampleCount(first+8);
    if( dataSize<<PatchSample::timeShift>>PatchSample::timeShift != dataSize ) {
        Assert(0);
        throwError("too much data in a sample");
    }
    // 042_Hi-Hat_Closed.pat has odd LoopStart and LoopEnd.  Maybe extra bit is an extra fraction bit?
    uint32_t loopStart = get4asSampleCount(first+12,/*acceptOdd=*/true);
    uint32_t loopEnd = get4asSampleCount(first+16,/*acceptOdd=*/true);
    ps.mySampleRate = float(get2(first+20));
    ps.myLowFreq = get4asFrequency(first+22); 
    ps.myHighFreq = get4asFrequency(first+26); 
    ps.myRootFreq = get4asFrequency(first+30);  
    Assert((samplingMode&(SM_Reverse))==0);     // reverse not supported yet
    first += 96;
    if( samplingMode & SM_16bit ) {
        // 16-bit data
        if( size_t(last-first)<2*dataSize )
            throwError("sample data truncated");
        ps.resize(dataSize);
        first = convertSamples(ps.begin(), first, dataSize); 
        ps.complete(false);
    } else {
        Assert(0);  // Not yet implemented
        throwError("8-bit patches not supported");
    }
    if( samplingMode & (SM_Looping|SM_PingPong) ) {
        // Looping
        Assert(PatchSample::timeShift>=4);
        Assert(loopStart<=loopEnd);
        Assert(loopEnd<=ps.size());
        ps.myLoopStart = loopStart << PatchSample::timeShift | (fractions>>4) << (PatchSample::timeShift-4);
        ps.myLoopEnd = loopEnd << PatchSample::timeShift | (fractions&0xF) << (PatchSample::timeShift-4);
        ps.myInitialState = samplingMode&SM_PingPong ? stateType::forwardBounce : stateType::forwardLoop;
   } else {
        // Non-looping
        ps.myLoopStart = ~0u;
        ps.myLoopEnd = ~0u;
        ps.myInitialState = stateType::forwardFinal;
    }
    return first;
}

void Patch::parser::throwError(const char* format, unsigned value) {
    patch.mySamples.clear();
    char buf[128];
    Assert(std::strlen(format) + 8 <= sizeof(buf));
    sprintf(buf, format, value);
    throw ReadError(buf); 
}

void Patch::parser::parseFile( const uint8_t* first, const uint8_t* last ) {
    if( last-first<239 ) 
        throwError("file less than 239 bytes");
    if( memcmp(first,"GF1PATCH110\0ID#000002\0",22)!=0 &&
        memcmp(first,"GF1PATCH100\0ID#000002\0",22)!=0 ) 
        throwError("file not prefixed with GF1PATCH1.0\0ID#000002");
    if( first[82]>1 )
        throwError("too many instruments (only one supported)");
    if( first[151]>1 )
        throwError("too many layers (only one supported)");
    char tmp[17];
    std::memcpy(tmp,first+131,16);   // 
    tmp[16] = 0;
    patch.myInstrumentName = tmp;
    int id = get2(first+129);
    size_t nSamples = first[198];
    patch.mySamples.resize(nSamples);
    first += 239;
    for( size_t i=0; i<nSamples; ++i ) 
        first = parseSample(patch.mySamples[i],first,last);
}

void Patch::readFromFile( const std::string& filename ) {
    mySamples.clear();
    FILE* f = std::fopen(filename.c_str(),"rb");
    if(!f) 
        throw ReadError("cannot open file " + filename + ": " + std::strerror(errno));

    std::fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    std::fseek(f, 0, SEEK_SET);
    SimpleArray<uint8_t> buf(fsize); 
    long s = fread(buf.begin(), 1, fsize, f);
    fclose(f);
    Assert(s==fsize);
    if(s>=0) {
        parser p(*this);
        p.parseFile(buf.begin(), buf.end());
    } else {
        throw ReadError("file " + filename + "is empty");
    }
}

Patch::Patch( const std::string& filename ) {
    readFromFile(filename);
}

const PatchSample& Patch::find( float freq ) const {
    const PatchSample* result = mySamples.begin();
    for( const PatchSample& ps: mySamples )
        if( ps.myLowFreq<=freq ) {
            result = &ps;
            if( ps.myHighFreq>=freq )
                break;
        }
    return *result;      
}

//-----------------------------------------------------------
// PatchSource
//-----------------------------------------------------------
static PoolAllocator<PatchSource> PatchSourceAllocator(64,false);

PatchSource* PatchSource::allocate( const PatchSample& ps, float relativeFrequency, float volume ) {
    Assert( 0.0625f <= relativeFrequency && relativeFrequency <= 8.0f ); // Sanity check
    PatchSource* s = PatchSourceAllocator.allocate();
    if( s ) {
        new(s) PatchSource;
        s->waveform = &ps;
        s->waveIndex = 0;
        s->waveDelta = unsigned( ps.sampleRate()/Synthesizer::SampleRate*Waveform::unitTime*relativeFrequency + 0.5f);
        s->volume = volume;
        Assert( s->waveDelta<=Waveform::unitTime*256 ); // Sanity check
        Assert( s->waveDelta>=Waveform::unitTime/256 ); // Sanity check
        s->loopStart = ps.myLoopStart;
        s->loopEnd = ps.myLoopEnd;
        s->tableEnd = ps.size() << PatchSample::timeShift;
        s->state = ps.myInitialState;
        Assert(s->assertOkay());
    } 
    return s;
}

void PatchSource::destroy() {
    PatchSourceAllocator.destroy(this);
}

void PatchSource::release() {
    Assert( (size_t(player)&3)==0 );
    PlayerMessage* m = PlayerMessageQueue.startPush();
    m->kind = PlayerMessageKind::Release;
    m->player = player;
    PlayerMessageQueue.finishPush();
}

void PatchSource::receive( const PlayerMessage& m ) {
    Assert( m.kind==PlayerMessageKind::Release );
    state = PatchStateOps::release(state); 
}

#if ASSERTIONS
bool PatchSource::assertOkay() {
    switch(state) {
        default:
            Assert(0);
        case stateType::finished:
            Assert( waveIndex>=tableEnd );
            break;
        case stateType::forwardFinal:
            Assert( waveIndex<tableEnd );
            break;
        case stateType::forwardBounce:
        case stateType::forwardLoop:
            Assert( waveIndex < loopEnd );
            Assert( loopEnd <= waveform->size()<<waveform->timeShift );
            break;
        case stateType::reverseBounce:
        case stateType::reverseFinal:
            Assert( loopStart <= waveIndex );
            Assert( waveIndex <= loopEnd );
            break;
    }
    return true;
}
#endif

unsigned PatchSource::update( float* acc, unsigned requested ) {
    Assert(assertOkay());
    unsigned n = requested;
    while(n>0 && state!=stateType::finished) {
        // Set d to maximum time difference that can be covered before state change.
        Waveform::timeType d;
        switch( state ) {
            default:
                Assert(0);
            case stateType::forwardFinal: 
                d = tableEnd-waveIndex;
                break;
            case stateType::forwardBounce:
            case stateType::forwardLoop:
                d = loopEnd-waveIndex;
                break;
            case stateType::reverseFinal:
            case stateType::reverseBounce:
                d = waveIndex-loopStart;
                break;
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
        waveIndex = waveform->resample(acc, volume, waveIndex, isReverse(state) ? -waveDelta : waveDelta, m);
        acc += m;
        if( newState ) {
            switch( state ) {
                default:
                    Assert(0);
                case stateType::forwardFinal:
                    state = stateType::finished;
                    break;
                case stateType::forwardLoop:
                    Assert(waveIndex-waveDelta<loopEnd);
                    Assert(loopEnd<=waveIndex);
                    waveIndex -= loopEnd-loopStart;
                    Assert(loopStart <= waveIndex);
                    Assert(waveIndex-waveDelta<loopStart);
                    break;
                case stateType::forwardBounce:
                    waveIndex = 2*loopEnd - waveIndex;
                    state = stateType::reverseBounce;
                    break;
                case stateType::reverseBounce:
                case stateType::reverseFinal:
                    waveIndex = 2*loopStart-waveIndex;
                    state = bounce(state);
                    break;
            }
        }
        Assert(assertOkay());
    }
    return requested-n;
}