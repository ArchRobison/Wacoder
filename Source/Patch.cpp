#include "Patch.h"
#include "AssertLib.h"
#include "Synthesizer.h"
#include <cstdio>
#include <cstdint>

class Patch::parser {
    Patch& patch;
    void throwError(const char* format, unsigned value=0);
    const uint8_t* parseSample(PatchSample& ps, const uint8_t* first, const uint8_t* last);
    static unsigned get4( const uint8_t* first ) {
        return first[0] | first[1]<<8 | first[2]<<16 | first[3]<<24;
    }
    unsigned get4asSampleCount( const uint8_t* first, uint8_t samplingMode ) {
        unsigned bytes = get4(first);
        Assert( (samplingMode & 1)==0 || bytes%2==0 );
        return bytes >> (samplingMode&1);
    }
    //! Get frequency from raw 4 bytes and convert it to hertz.
    float get4asFrequency( const uint8_t* first ) {
        unsigned value = get4(first);
        return value*(1/1024.f);    // FIXME - 1024 should not be hardwired.  There are bytes in the file that define it.
    }
    static unsigned get2( const uint8_t* first ) {
        return first[0] | first[1]<<8;
    }
public:
    class badFile {};
    parser( Patch& patch_ ) : patch(patch_) {}
    void parseFile( const uint8_t* first, const uint8_t* last );
};

const uint8_t* Patch::parser::parseSample(PatchSample& ps, const uint8_t* first, const uint8_t* last) {
    if( last-first<96 )
        throwError("sample header truncated");
    uint8_t fractions = first[7];
    uint8_t samplingMode = first[55];
    // >> in next three lines converts bytes to 
    uint32_t dataSize = get4asSampleCount(first+8,samplingMode);
    uint32_t banana = dataSize<<PatchSample::timeShift>>PatchSample::timeShift;
    if( dataSize<<PatchSample::timeShift>>PatchSample::timeShift != dataSize ) {
        Assert(0);
        throwError("too much data in a sample");
    }
    uint32_t loopStart = get4asSampleCount(first+12,samplingMode);
    uint32_t loopEnd = get4asSampleCount(first+16,samplingMode);
    ps.mySampleRate = float(get2(first+20));
    ps.myLowFreq = get4asFrequency(first+22); 
    ps.myHighFreq = get4asFrequency(first+26); 
    ps.myRootFreq = get4asFrequency(first+30);    
    Assert((samplingMode&(1<<3|1<<4))==0);     // Ping-pong/reverse not supported yet
    first += 96;
    if( samplingMode&1 ) {
        // 16-bit data
        if( size_t(last-first)<2*dataSize )
            throwError("sample data truncated");
        ps.resize(dataSize);
        int isUnsigned = (samplingMode & 2)!=0;
        float* base = ps.begin();
        for( size_t i=0; i<dataSize; ++i ) {
            int value = first[0] | first[1]<<8;
            // Convert to normalized float data
            base[i] = (isUnsigned ? float(value-0x8000) : float(int16_t(value)))*(1.0f/0x8000);
            first += 2;
        }
        ps.complete(false);
    } else {
        Assert(0);  // Not yet implemented
    }
    if(samplingMode & 1<<2) {
        // Looping
        Assert(PatchSample::timeShift>=4);
        Assert(loopStart<=loopEnd);
        Assert(loopEnd<=ps.size());
        ps.myLoopStart = loopStart << PatchSample::timeShift | (fractions>>4) << (PatchSample::timeShift-4);
        ps.myLoopEnd = loopEnd << PatchSample::timeShift | (fractions&0xF) << (PatchSample::timeShift-4);
    } else {
        // Non-looping
        ps.myLoopStart = ~0u;
        ps.myLoopEnd = ~0u;
    }
    return first;
}

void Patch::parser::throwError(const char* format, unsigned value) {
    char buf[128];
    Assert(std::strlen(format) + 8 <= sizeof(buf));
    sprintf(buf, format, value);
    patch.myReadStatus = buf;
    throw badFile();
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
    size_t nSamples = first[198];
    patch.mySamples.resize(nSamples);
    first += 239;
    for( size_t i=0; i<nSamples; ++i ) 
        first = parseSample(patch.mySamples[i],first,last);
}

bool Patch::readFromFile( const std::string& filename ) {
    mySamples.clear();
    myReadStatus = "";
    FILE* f = std::fopen(filename.c_str(),"rb");
    if(!f) {
        myReadStatus = "cannot open file " + filename + ": " + std::strerror(errno);
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    std::fseek(f, 0, SEEK_SET);
    uint8_t* buf = new uint8_t[fsize];
    long s = fread(buf, 1, fsize, f);
    fclose(f);
    Assert(s==fsize);
    if(s>=0) {
        parser p(*this);
        try {
            p.parseFile(buf, buf+s);
        } catch( parser::badFile ) {
            Assert(!myReadStatus.empty());
            mySamples.clear();
        }
    } else {
        myReadStatus = "empty file";
    }
    delete[] buf;
    return myReadStatus.empty();
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

PatchInstrument::PatchInstrument(const Patch& patch) : myPatch(patch) {
    Assert(!patch.empty());
    for( int note=0; note<128; ++note )
        keyArray[note] = nullptr;
}

void PatchInstrument::noteOn( const Event& on, const Event& off ) {
    if( myPatch.empty() ) 
        return;
    int note = on.note();   // Used signed type, so that "note-69" can go negative
    float freq = 440*std::pow(1.059463094f, note-69);
    const PatchSample& ps = myPatch.find(freq);
    Assert(!keyArray[note]);
    keyArray[note] = Synthesizer::PatchSource::allocate(ps,freq/ps.pitch());
    Play(keyArray[note]);
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