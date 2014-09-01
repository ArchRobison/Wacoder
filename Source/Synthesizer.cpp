#include "NonblockingQueue.h"
#include "PoolAllocator.h"
#include "Synthesizer.h"
#include <cstring>
#include <cstdio>

namespace Synthesizer {

//---------------------------------------------------------------
// MessageQueue (transmits Player pointers from normal code to interrupt handler.
//---------------------------------------------------------------
class Player;

enum PlayerMessageKind {
    WMK_Start,
    WMK_ChangeVolume,
    WMK_ChangeEnvelope
};

class PlayerMessage {
public:
    char kind;                          // Really a PlayerMessageKind
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

//-----------------------------------------------------------
// Player
//-----------------------------------------------------------

static const size_t PlayerCountMax = 256;

class Player {
    void output( float* dst, const float* src, float vol, unsigned n ) {
        for( unsigned k=0; k<n; ++k )
            dst[k] += src[k]*vol;
    }
public:
    Source* source;
    unsigned delay[2];
    float volume[2]; 
    //! Number of samples taken after source finished.
    /** Always <=delayBufSize until Player is done.*/
    unsigned postSource;
    static const unsigned delayBufSize = 64;
    float delayBuf[delayBufSize];
    static const unsigned chunkMaxSize = 1024;
    //! Accumulate into left and right.  Return false if done.
    bool update( float* left, float* right, unsigned n );
    unsigned delayDiff() const {
        int d = delay[0]-delay[1];
        return d>=0 ? d : -d;
    }
};

bool Player::update( float* left, float* right, unsigned n ) {
    Assert( 0<n );
    Assert( n<=chunkMaxSize );
    if( delay[0]>0 && delay[1]>0 ) {
        Assert( postSource==0 );
        unsigned d = Min( n, Min( delay[0], delay[1] ) );
        delay[0] -= d;
        delay[1] -= d;
        n -= d;
        if( n==0 ) 
            return true;
        left += d;
        right += d;
        Assert( delay[0]==0 || delay[1]==0 );
    }
    unsigned d = Max(delay[0],delay[1]);
    Assert( d<=delayBufSize );
    Assert( d==delayDiff() );
    float buf[chunkMaxSize+delayBufSize];
    std::memcpy( buf, delayBuf, sizeof(float)*d );
    if( postSource>0 ) {
        std::memset( buf+d, 0, n*sizeof(float) );
        postSource += n;
    } else {
        unsigned m = source->update( buf+d, n );
        if( m<n ) {
            std::memset( buf+d+m, 0, (n-m)*sizeof(float) );
            postSource = n-m;
        }
    }
    output( left, buf+d-delay[0], volume[0], n );
    output( right, buf+d-delay[1], volume[1], n );
    std::memcpy( delayBuf, buf+n, sizeof(float)*d );
    return postSource<=d;
}

//-----------------------------------------------------------
// Communication between thread and interrupt handler
//-----------------------------------------------------------

//! Queue for sending messages from main code to interrupt handler.
/** Allow for one sustain and one release message.  FIXME - determine right queue bound */
static NonblockingQueue<PlayerMessage> PlayerMessageQueue(1024);

//! Queue for sending freed Players from interrupt handler to normal code. 
static NonblockingQueue<Player*> FreePlayerQueue(PlayerCountMax);

static inline float Hypot( float x, float y ) {
    return sqrt(x*x+y*y);
}

void Play( Source* src, float volume, float x, float y ) {
    static PoolAllocator<Player> playerAllocator(PlayerCountMax, false);

    // Drain queue of freed players
    while( Player** p = FreePlayerQueue.startPop() ) {
        (*p)->source->destroy();
        playerAllocator.destroy(*p);
        FreePlayerQueue.finishPop();
    }
    if( !src ) 
        // Allocation of Source failed.
        return;
    Player* p = playerAllocator.allocate();
    src->player = p;
    p->source = src;
    p->volume[0] = volume*cos(atan2(y,-x)/2);
    p->volume[1] = volume*cos(atan2(y,x)/2);
    float c = (Player::delayBufSize-1)/2;         // The "-1" is supposed to provide a safety margin for roundoff issues
    p->delay[0] = Min( ~0u, unsigned(Hypot(x+1,y)*c) );
    p->delay[1] = Min( ~0u, unsigned(Hypot(x-1,y)*c) );
    Assert( p->delayDiff()<=Player::delayBufSize );                  
    p->postSource = 0;
    std::memset( p->delayBuf, 0, sizeof(float)*p->delayDiff() );

    // Send message to interrupt handler.
    PlayerMessage* m = PlayerMessageQueue.startPush();
    m->kind = WMK_Start;
    m->player = p;
    PlayerMessageQueue.finishPush();
}

void OutputInterruptHandler( Waveform::sampleType* left, Waveform::sampleType* right, unsigned n ) {
    static SimpleBag<Player*> livePlayerSet(PlayerCountMax);

    // Read incoming messages
    while(PlayerMessage* m = PlayerMessageQueue.startPop()) {
        Player* p = m->player;
        Assert( (size_t(p)&3)==0 );
        Assert( p->source->player==p ); 
        if( m->kind==WMK_Start ) {
            // Starting a new player
            livePlayerSet.push(p);
        } else {
            // Continuing an old player
            p->source->receive(*m);
        }
        PlayerMessageQueue.finishPop();
    }

    // Get n samples
    while(n>0) {
        // Get samples in chunks of up to Player::chunkMaxSize
        unsigned m = Min(n,Player::chunkMaxSize);
        // For each player, make it contribute m samples 
        for(Player** pp = livePlayerSet.begin(); pp<livePlayerSet.end();) {
            Player& p = **pp;
            if( p.update(left,right,m) ) {
                // Player not finished
                ++pp;
            } else {
                // Player is finished.  Send back for reclamation and erase from LivePlayerSet.
                Player** f = FreePlayerQueue.startPush();
                Assert(f);
                *f = &p;
                FreePlayerQueue.finishPush();
                livePlayerSet.erase(pp);
            }
        }
        n-=m;
        left+=m;
        right+=m;
    }
}

//-----------------------------------------------------------
// SimpleSource
//-----------------------------------------------------------
static PoolAllocator<SimpleSource> SimpleSourceAllocator(64,false);

SimpleSource* SimpleSource::allocate( const Waveform& w, float freq ) {
    Assert( !w.isCyclic() );
    Assert( 1.f/1000 <= freq && freq <= 1000.f );   // Sanity check
    SimpleSource* s = SimpleSourceAllocator.allocate();
    Assert(s);
    if( s ) {
        new(s) SimpleSource;
        s->waveform = &w;
        s->waveLowIndex = 0;
        s->waveHighIndex = 0;
        s->waveDelta = Waveform::timeType(freq*Waveform::unitTime);
        Assert( s->waveDelta>0 );
        Assert( s->waveDelta<=Waveform::unitTime*128 );   // Sanity check
    }
    return s;
}

void SimpleSource::destroy() {
    SimpleSourceAllocator.destroy(this);
}

void SimpleSource::receive( const PlayerMessage& m ) {
    Assert(0);
}

unsigned SimpleSource::update( float* acc, unsigned n ) {
    Assert( (((long long)waveHighIndex<<Waveform::timeShift)+waveLowIndex) % waveDelta == 0 );
    const Waveform::sampleType* w = waveform->begin() + waveHighIndex;
    Waveform::timeType di = waveDelta;
    Waveform::timeType i = waveLowIndex;
    unsigned m = n;
    int o = waveform->size() - waveHighIndex;
    Assert( int(o)>=0 );
    if( o <= ~0u<<Waveform::timeShift>>Waveform::timeShift ) 
        m = Min(m,((o<<Waveform::timeShift)-i)/di);
    for( unsigned k=0; k<m; ++k ) {
        // Create wave sample by interpolating waveTable  
        acc[k] = waveform->interpolate(w,i);
        // Update waveIndex. 
        i += di;
    }
    waveLowIndex = i & Waveform::unitTime-1;
    waveHighIndex += i>>Waveform::timeShift;
    Assert( (((long long)waveHighIndex<<Waveform::timeShift)+waveLowIndex) % waveDelta == 0 );
    return m;
}

//-----------------------------------------------------------
// DynamicSource
//-----------------------------------------------------------
static PoolAllocator<DynamicSource> DynamicSourceAllocator(256,false);

DynamicSource* DynamicSource::allocate( const Waveform& w, float freq ) {
    Assert( w.size()<<Waveform::timeShift>>Waveform::timeShift == w.size() );
    Assert( w.isCompleted() );
    Assert( 1.f/1000 <= freq && freq <= 1000.f );   // Sanity check
    DynamicSource* s = DynamicSourceAllocator.allocate();
    Assert(s);
    if( s ) {
        new(s) DynamicSource;
        s->waveform = &w;
        s->waveIndex = 0;
        s->waveDelta = Waveform::timeType(freq*Waveform::unitTime);
        Assert(s->waveDelta<=Waveform::unitTime*128);   // Sanity check
        s->currentVolume = 0;
        s->targetVolume = 0;
        s->deadline = 0;
        s->release = false;
        Assert( s->waveDelta>0 );
    }
    return s;
}

void DynamicSource::destroy() {
    DynamicSourceAllocator.destroy(this);
}

void DynamicSource::receive( const PlayerMessage& m ) {
    Assert( m.kind==WMK_ChangeVolume );
    targetVolume = m.dynamic.newVolume;
    deadline = m.dynamic.deadline;
    Assert( !(release && m.dynamic.release) );
    release = m.dynamic.release;
}

unsigned DynamicSource::update( float* acc, unsigned n ) {
    unsigned requested = n;
    const Waveform::sampleType* w = waveform->begin();
    Waveform::timeType wrap = waveform->limit(); 
    Waveform::timeType di = waveDelta;
    Waveform::timeType i = waveIndex;
    while( n>0 && !(deadline==0 && release) ) {
        unsigned m;
        float dv;
        if( deadline==0 ) {
            dv = 0;
            currentVolume = targetVolume;
            m = n;
        } else {
            m = Min(n,deadline);
            dv = (targetVolume-currentVolume)/deadline;
        }
        float v = currentVolume;
        for( unsigned k=0; k<m; ++k ) {
            // Create wave sample by interpolating waveTable  
            acc[k] = v*waveform->interpolate(w,i);
            Assert(fabs(acc[k])<=1.0);
            // Update waveIndex and wrap around if necessary. 
            i += di;
            if( i>=wrap )
                i -= wrap;
            v += dv;
        }
        acc += m;
        n -= m;
        currentVolume = v;
        deadline -= m;
    }
    waveIndex = i;
    return requested-n;
}

void DynamicSource::changeVolume( float newVolume, float deadline, bool releaseWhenDone ) {
    // Send message
    PlayerMessage* m = PlayerMessageQueue.startPush();
    m->kind = WMK_ChangeVolume;
    m->player = player;
    m->dynamic.newVolume = newVolume;
    m->dynamic.deadline = unsigned(SampleRate*deadline);
    m->dynamic.release = releaseWhenDone;
    Assert( (size_t(player)&3)==0 );
    PlayerMessageQueue.finishPush();
}

//-----------------------------------------------------------
// AsrSource
//-----------------------------------------------------------
static PoolAllocator<AsrSource> AsrSourceAllocator(64,false);

AsrSource* AsrSource::allocate( const Waveform& w, float freq, const Envelope& attack, float speed ) {
    Assert( w.size()<<Waveform::timeShift>>Waveform::timeShift == w.size() );
    Assert( w.isCompleted() );
    Assert( 1.f/1000 <= freq && freq <= 1000.f );   // Sanity check
    Assert( 1.f/1000000 <= speed && speed <= 1.0f/20 );
    AsrSource* s = AsrSourceAllocator.allocate();
    if( s ) {
        new(s) AsrSource;
        s->waveform = &w;
        s->waveIndex = 0;
        s->waveDelta = Waveform::timeType(freq*Waveform::unitTime);
        Assert( s->waveDelta>0 );
        s->envelope = &attack;
        s->envIndex = 0;
        s->envDelta = Envelope::timeType(speed*Envelope::unitTime);
        Assert( s->envDelta>0 );
        s->relEnvDelta = 0;
        s->relEnvelope = NULL;
    } 
    return s;
}

void AsrSource::destroy() {
    AsrSourceAllocator.destroy(this);
}

void AsrSource::receive( const PlayerMessage& m ) {
    Assert( m.kind==WMK_ChangeEnvelope );
    if( envDelta==0 ) {
        envDelta = m.midi.envDelta;
        envelope = m.midi.envelope;
        envIndex = 0;
    } else {
        // Defer until attack finishes
        relEnvelope = m.midi.envelope;
        relEnvDelta = m.midi.envDelta;
    }
}

unsigned AsrSource::update( float* acc, unsigned n ) {
    unsigned requested = n;
    const Waveform::sampleType* w = waveform->begin();
    Waveform::timeType wrap = waveform->limit(); 
    Waveform::timeType di = waveDelta;
    while( n>0 ) {
        const Envelope::sampleType* e = envelope->begin();
        Envelope::timeType limit = envelope->limit(); 
        // Set m to number of samples to compute this time around the while loop.
        Waveform::timeType dj = envDelta;
        Envelope::timeType j = envIndex;
        unsigned m = dj==0 ? n : Min(unsigned(n),(limit-j+dj-1)/dj);
        Waveform::timeType i = waveIndex;
        for( unsigned k=0; k<m; ++k ) {
            // Create wave sample by interpolating waveTable  
            acc[k] = waveform->interpolate(w,i)*envelope->interpolate(e,j);
            // Update waveIndex and wrap around if necessary. 
            i += di;
            if( i>=wrap )
                i -= wrap;
            j += dj;
        }
        n -= m;
        acc += m;
        waveIndex = i;
        envIndex = j;
        if( j>=limit ) {
            if( !envelope->isSustain() ) 
                break;
            if( relEnvelope ) {
                // Start release
                envDelta = relEnvDelta;
                envelope = relEnvelope;
                envIndex = 0;
                relEnvDelta = 0;
                relEnvelope = NULL;
            } else {
                // Release not sent yet -- sustain last value
                envIndex = limit-1;
                envDelta = 0;
            }
        }
    } 
    return requested-n;
}

void AsrSource::changeEnvelope(Envelope& e, float speed) {
    // Send message
    PlayerMessage* m = PlayerMessageQueue.startPush();
    m->kind = WMK_ChangeEnvelope;
    m->player = player;
    m->midi.envelope = &e;
    m->midi.envDelta = Envelope::timeType(speed*Envelope::unitTime);
    Assert( (size_t(player)&3)==0 );
    PlayerMessageQueue.finishPush();
}

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

void Initialize() {
}

} // namespace Synthesizer