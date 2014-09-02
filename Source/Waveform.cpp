#include "Waveform.h"
#include "AssertLib.h"
#include "Utility.h"
#include <cstring>
#include <cstdio>

namespace Synthesizer {

//-----------------------------------------------------------
// Waveform
//-----------------------------------------------------------

typedef short int16_t;
typedef unsigned short uint16_t;
typedef unsigned uint32_t;
typedef int int32_t;

struct WavHeader {
    // Each declaration is 4 bytes
    char chunkId[4];
    uint32_t chunkSize;
    char format[4];
    char subchunk1Id[4];
    uint32_t subchunk1Size;
    uint16_t audioFormat, numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign, bitsPerSample;
    void check();
};

void WavHeader::check() {
    Assert( memcmp(chunkId,"RIFF",4)==0 );
    Assert( memcmp(format,"WAVE",4)==0 );
    Assert( memcmp(subchunk1Id,"fmt ",4)==0 );
    Assert( subchunk1Size==16 );
    Assert( audioFormat==1 );  // PCM/uncompressed
    Assert( sampleRate==44100 );
    Assert( bitsPerSample==16 );
}

struct WavData {
    char subchunk2Id[4];
    uint32_t subchunk2Size;
    void check();
};

void WavData::check() {
    Assert( memcmp(subchunk2Id,"data",4)==0 );
    Assert( subchunk2Size%2==0 );
}

class Waveform::inputType {
    FILE* myFile;
    const char* myData;
    const char* myEnd;
public:
    inputType( const char* data, size_t n ) : myFile(NULL), myData(data), myEnd(data+n) {}
    inputType( FILE* f ) : myFile(f), myData(NULL), myEnd(NULL) {}
    void read( void* dst, size_t n );
};

void Waveform::inputType::read( void* dst, size_t n ) {
    if( myData ) {
        Assert( n <= size_t(myEnd-myData) );
        memcpy( dst, myData, n );
        myData += n;
    } else {
        size_t m = fread(dst,n,1,myFile);
        Assert(m==1);
    }
}

void Waveform::readFromMemory( const char* data, size_t n ) {
    inputType in(data,n);
    readFromInput(in);
}

void Waveform::readFromFile( const char* filename ) {
    FILE* f = fopen(filename,"rb");
    Assert(f);						// FIXME - recover from missing file
    inputType in(f);
    readFromInput(in); 
    fclose(f);
}

void Waveform::readFromInput( inputType& f ) {
    WavHeader w;
    Assert( sizeof(w)==36 );
    f.read(&w,36);
    w.check();
    // FIXME - skip subchunk1Size-16 bytes
#if 0
    Assert( w.extraParamSize==0 );
#endif
    WavData d;
    f.read(&d,8);
    d.check();
    size_t n = d.subchunk2Size/2;
    SimpleArray<int16_t> tmp;
    tmp.resize(n);
    f.read(tmp.begin(),sizeof(int16_t)*n);
    resize(n);
    for( size_t i=0; i<n; ++i ) {
        (*this)[i] = tmp[i]*(1.f/(1<<15));
    }
    complete(false);
}

void Waveform::writeToFile( const char* filename ) {
    size_t n = size();
    FILE* f = fopen(filename,"wb");
    Assert(f);
    WavHeader w;
    std::memset(&w,0,sizeof(w));
    memcpy( w.chunkId, "RIFF", 4 );
    w.chunkSize = 36 + 8 + 2*n - 8;
    memcpy( w.format, "WAVE", 4 );
    memcpy( w.subchunk1Id, "fmt ", 4 );
    w.subchunk1Size = 16;
    w.audioFormat = 1;  // PCM
    w.numChannels = 1;  // Mono
    w.sampleRate = 44100;
    w.bitsPerSample = 16;
    w.byteRate = w.sampleRate * w.numChannels * w.bitsPerSample / 8;
    w.blockAlign = w.numChannels * w.bitsPerSample / 8;
    fwrite(&w,36,1,f);

    WavData d;
    memcpy( d.subchunk2Id, "data", 4 );
    d.subchunk2Size = 2*n;
    fwrite(&d,8,1,f);

    SimpleArray<int16_t> tmp;
    tmp.resize(n);
    for( size_t i=0; i<n; ++i ) {
        const int scale = (1<<15)-1;
        float a = (*this)[i]*scale+(scale+0.5f); 
        if( a<0 ) a = 0;
        if( a>2*scale ) a=2*scale;
        tmp[i] = int(a) - scale;
    }
    fwrite(tmp.begin(),2,n,f);
    fclose(f);
}

} // namespace Synthesizer