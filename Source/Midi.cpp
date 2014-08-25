/* Copyright 2012-2014 Arch D. Robison 

   Licensed under the Apache License, Version 2.0 (the "License"); 
   you may not use this file except in compliance with the License. 
   You may obtain a copy of the License at 
   
       http://www.apache.org/licenses/LICENSE-2.0 
       
   Unless required by applicable law or agreed to in writing, software 
   distributed under the License is distributed on an "AS IS" BASIS, 
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
   See the License for the specific language governing permissions and 
   limitations under the License.
 */

#include "Midi.h"
#include "Host.h"
#include "Synthesizer.h"
#include "WaSet.h"
#include <cstring>
#include <algorithm>

using namespace Synthesizer;

static void ByteSwap( uint16_t& x ) {
    x = x<<8 | x>>8;
}

static void ByteSwap( uint32_t& x ) {
    x = x<<24 | x<<8&0xFF0000 | x>>8&0xFF00 | x>>24;
}

//-----------------------------------------------------------------
// MidiTrack
//-----------------------------------------------------------------

const MidiTrack::infoType& MidiTrack::getInfo() const {
    if( !info.isValid ) {
        info.hasNotes = false;
        MidiTrackReader mtr;
        for( mtr.setTrack(*this); ; mtr.next() ) {
            switch(mtr.event()) {
                case MEK_EndOfTrack:
                    goto done;
                case MEK_TrackName:
                    info.trackName = mtr.text();
                    break;
                case MEK_Comment:
                    info.firstComment = mtr.text();
                    break;
                case MEK_NoteOn:
                case MEK_NoteOff:
                    info.hasNotes = true;
                    if( !info.trackName.empty() )
                        goto done;
                    break;
            }
        }
done:
        info.isValid = true;
    }
    return info;
}

//-----------------------------------------------------------------
// MidiTrackReader
//-----------------------------------------------------------------

unsigned MidiTrackReader::readVariableLen() {
    unsigned value = 0;
    byte c;
    do {
        c = readByte();
        value = value*128 + (c&0x7F);
    } while( c&0x80 );
    return value;
}

void MidiTrackReader::readDeltaAndEvent() {
retry:
    if( myPtr>=myEnd ) {
        myTime = ~0u;
        myEvent = MEK_EndOfTrack;   // Add missing event
    } else {
        myTime += readVariableLen();
        // MIDI files sometimes omit the status byte if it is the same as for the previous event
        unsigned char c = *myPtr&0x80 ? *myPtr++ : myStatus;
        if( c==0xFF ) {
            // MetaEvent
            byte type = readByte();
            myEvent = MidiEventKind(MIDI_META(type));
            myLen = readVariableLen();
            // Leave myPtr pointing to variable-length data
            Assert( myLen<=size_t(myEnd-myPtr) );
#if ASSERTIONS
            switch( myEvent ) {
                case MEK_Port:
                    Assert( myLen==1 );
                    break;
            }
#endif
        } else {
            myEvent = MidiEventKind(c>>4);
            switch(myEvent) {
                case 0xF:                       // SysEx event - skip it
                    myLen = readVariableLen();
                    Assert( myLen<=size_t(myEnd-myPtr) );
                    Assert( myPtr[myLen-1]==0xF7 );
                    next();
                    goto retry;
                case MEK_PitchBend:
                case MEK_NoteOff:
                case MEK_NoteOn:
                case MEK_ControllerChange:
                    myStatus = c;
                    // Leave myPointer pointing to param1
                    myLen = 2;
                    break;
                case MEK_ProgramChange:
                    myStatus = c;
                    // Leave myPointer pointing to param1
                    myLen = 1;
                    break;
                default:
                    Assert(0);
            }
        }
    }
}

std::string MidiTrackReader::text() const {
    Assert( event()==MEK_Comment||event()==MEK_TrackName );
    return std::string( (const char*)myPtr, myLen );
}

MidiTrackReader::futureEvent MidiTrackReader::findNextOffOrOn() const {
    futureEvent fe;
    Assert(event()==MEK_NoteOn||event()==MEK_NoteOff);
    MidiTrackReader mtr(*this);
    mtr.myTime = 0;
    for(;;) {
        mtr.next();
        switch( mtr.event() ) {
            case MEK_NoteOff:
            case MEK_NoteOn:
                if( mtr.note()==note() && mtr.channel()==channel() ) {
                    fe.velocity = mtr.velocity();
                    goto done;
                }
                break;
            case MEK_EndOfTrack:
                fe.velocity = 0;        // Really should be a "don't know" value.
                goto done;
        }
    }
done:
    fe.event = mtr.event();
    fe.time = mtr.myTime;
    return fe;
}

#if 0
static void DumpString( FILE* f, const byte* s, size_t n, bool newline=false ) {
    fprintf(f,"\"");
    for( size_t i=0; i<n; ++i )
        fprintf(f,"%c",s[i]);
    fprintf(f,"\"%s",newline?"\n":0);
}

void MidiTrackReader::dump( const char* filename ) {
    static FILE* f;
    if( f ) {
        for( int k=0; k<30; ++k )
            fprintf(f,"%x ",myPtr[k]);
        fprintf(f,"\n");
        fclose(f);
        exit(1);
    }
    f = fopen(filename,"w");
    MidiTrackReader mtr;
    mtr.init(myBegin,myEnd);
    while( mtr.myPtr<myEnd ) {
        const char* hint = "";
        switch( mtr.event() ) {
            case MEK_TrackName:
                fprintf(f,"time=%d metaevent=%x [TrackName] len=%d ",mtr.time(),mtr.event()-0x10, mtr.myLen );
                DumpString(f,mtr.myPtr,mtr.myLen,true);
                break;
            case MEK_NoteOn:
                hint = " [note on]";
                goto simple;
            case MEK_NoteOff:
                hint = " [note off]";
                goto simple;
            case MEK_ControllerChange:
                fprintf(f,"time=%d event=%x [controller change] len=%d c=%d v=%d\n",mtr.time(), mtr.event(), mtr.myLen, mtr.myPtr[0], mtr.myPtr[1]);
                break;
            case MEK_ProgramChange:
                hint = " [program change]";
                goto simple;
            case MEK_PitchBend:
                hint = " [pitch bend]";
                goto simple;
            default:
simple:
                fprintf(f,"time=%d event=%x%s len=%d\n",mtr.time(), mtr.event(), hint, mtr.myLen);
                break;
        }
        mtr.next();
    }
    fclose(f);
}
#endif

//-----------------------------------------------------------------
// Header layouts
// WARNING: compilers may trailing padding.
// Member headerSize shows the actual number of bytes of the header
// as represented in a MIDI file.
//-----------------------------------------------------------------

struct MidiHeader {
    // Each declaration is 4 bytes
    char chunkId[4];
    uint32_t chunkSize;
    uint16_t format;
    uint16_t numTracks;
    uint16_t division;
    static const size_t headerSize = 14;
};

static void ByteSwap( MidiHeader& h ) {
    ByteSwap(h.chunkSize);
    ByteSwap(h.format);
    ByteSwap(h.numTracks);
    ByteSwap(h.division);
}

struct MidiTrackHeader {
    char chunkId[4];
    uint32_t chunkSize;
    static const size_t headerSize = 8;
};

static void ByteSwap( MidiTrackHeader& t ) {
    ByteSwap(t.chunkSize);
}

template<typename H>
static const byte* readHeader( H& h, const byte* first, const byte* last, const char* id ) {
    Assert( H::headerSize<=sizeof(H) );
    if( memcmp(first, id, 4)!=0 )
        return nullptr;
    Assert( last-first>=H::headerSize );
    memcpy( &h, first, H::headerSize );
    first += H::headerSize;
    ByteSwap(h);
    return first;
}

//-----------------------------------------------------------------
// MidiTune
//-----------------------------------------------------------------

void MidiTune::noteError( const char* format, unsigned value ) {
    Assert(strlen(format) + 8 <= sizeof(myReadStatus) );
    sprintf(myReadStatus,format,value);
}

void MidiTune::clear() {
    myTrack.clear();
}

void MidiTune::assign(const byte* first, const byte* last) {
    clear();
    // Read header
    MidiHeader h;
    first = readHeader(h,first,last,"MThd");
    if(!first) 
        return noteError("bad MThd header");
    Assert( h.format<=1 );
    Assert( h.chunkSize==6 );
    if( h.format==0 ) {
        if( h.numTracks!=1 ) 
            return noteError("format 0 must have one track, not %u tracks", h.numTracks);
    } else {
        if( h.numTracks<=0 ) {
            return noteError("nonzero format 0 shold have at least one track");
        }
    }
    if( h.division&0x8000 ) 
        return noteError("SMTPE not implemented");
    else 
        myTickPerSec = 2*float(h.division);    // 120 beats per minute

    // Allocate trackDesc structures
    myTrack.resize(h.numTracks);
    // Read each track
    for( unsigned i=0; i<h.numTracks; ++i ) {
        MidiTrackHeader t;
        first = readHeader(t,first,last,"MTrk");
        if( !first ) 
            return noteError("track %u has bad MTrk header", i);
        myTrack[i].data.assign( first, t.chunkSize );
        first += t.chunkSize;
    }
}

bool MidiTune::readFromFile( const char* filename ) {
    myReadStatus[0] = 0;
    FILE* f = fopen(filename,"rb");
    if( !f ) {
        noteError("cannot open file");
        return false;
    }
    fseek(f,0,SEEK_END);
    long fsize = ftell(f);
    fseek(f,0,SEEK_SET);
    // FIXME - avoid temporary buffer.
    byte* buf = new byte[fsize];
    long s = fread(buf,1,fsize,f);
    fclose(f);
    Assert( s==fsize );
    bool result = false;
    if( s>=0 )
        assign(buf,buf+s);
    delete[] buf;
    return myReadStatus[0]==0;
}

//-----------------------------------------------------------------
// MidiInstrument subclasses
//-----------------------------------------------------------------

static Waveform MidiKeyWave;  
static float MidiKey440AFreq;

static Envelope MidiKeyAttack;
static Envelope MidiKeyRelease;

struct KeySoundInit {
    KeySoundInit();
} static TheKeySoundInit;

KeySoundInit::KeySoundInit() {
    double pi = 3.14159265358979;
    const size_t keyLength = 3207;
    MidiKeyWave.resize(keyLength);
    for( int i=0; i<keyLength; ++i ) 
        MidiKeyWave[i] = 0;
#if 1
    // Organ
    double phase = 0;
    for( int h=1; h<=19; h+=2 ) {
        for( int i=0; i<keyLength; ++i ) {
            double omega = 2*pi/keyLength;
            const double x = i*(1.0/keyLength);
            const double a = 0.05;
            MidiKeyWave[i] += float(sin(phase+i*omega*h))/h;
        }
        phase += pi;
    }
#else
    struct rissetType {  // from http://cnx.org/content/m15476/latest/
        int partialNumber;
        double intensity;    // in dB
        double duration;
        double freqMul;
        double freqOffset;   // in Hz
    };
    rissetType r[11] = {
        {1,0,1,0.56,0},
        {2,-3.48,0.9,0.56,1},
        {3,0,0.65,0.92,0},
        {4,5.11,0.55,0.92,1.7},
        {5,8.53,0.325,1.19,0},
        {6,4.45,0.35,1.7,0},
        {7,3.29,0.25,2,0},
        {8,2.48,0.2,2.74,0},
        {9,2.48,0.15,3,0},
        {10,0,0.1,3.76,0},
        {11,2.48,0.075,4.07,0}
        };

     // ?
    float phase = 0;
    for( int j=0; j<11; ++j ) {
        for( int i=0; i<keyLength; ++i ) {
            double omega = 2*pi/keyLength;
            const double x = i*(1.0/keyLength);
            const double a = 0.05;
            MidiKeyWave[i] += 0.25f*float(sin(phase+i*omega*r[j].freqMul));
        }
    }
#endif
    MidiKeyWave.complete();
    MidiKey440AFreq = 440.f*keyLength/SampleRate;

    MidiKeyAttack.resize(100);
    for( int i=0; i<100; ++i ) {
        const double x = i*(1.0/99);
        MidiKeyAttack[i] = float(x);
    }
    MidiKeyAttack.complete(true);

    MidiKeyRelease.resize(100);
    for( int i=0; i<100; ++i ) {
        const double x = i*(1.0/100);
        MidiKeyRelease[i] = float(exp(-4*x));
    }
    MidiKeyRelease.complete(false);
}

class AdditiveInstrument: public MidiInstrument {
    MidiSource* keyArray[128];
    int counter;
#if ASSERTIONS
    int onLevel[128];
#endif
    /*override*/ void processEvent();
    /*override*/ void stop();
    void attack( int note );
    void release( int note );
public:
    AdditiveInstrument() {
#if ASSERTIONS
        std::fill_n( onLevel,128, 0 );
#endif
        std::fill_n( keyArray, 128, (MidiSource*)NULL );
        counter = 0;
    }
};

void AdditiveInstrument::attack( int note ) {
    float freq = MidiKey440AFreq*std::pow(1.059463094f,(note-69))*(1+(counter+=19)%32*(.005f/32));
    float speed = MidiKeyAttack.size()*(16.f/SampleRate);
    MidiSource* k = MidiSource::allocate( MidiKeyWave, freq, MidiKeyAttack, speed );
    keyArray[note] = k;
    Play( k, velocity()*(1.0f/512) );
}

void AdditiveInstrument::release( int note ) {
    Assert(0<=note && note<128);
    if( MidiSource* k = keyArray[note] ) {
        k->changeEnvelope( MidiKeyRelease, MidiKeyRelease.size()*(4.0f/SampleRate) );
        keyArray[note] = NULL;
    }
}

void AdditiveInstrument::processEvent() {
    switch( event() ) {
        case MEK_NoteOn: {
            if( velocity()==0 ) 
                // Some MIDI files use "Note On" with velocity of zero to indicate "Note off".
                goto off;
            int n = note() & 0x7F;
            if( keyArray[n] ) {
                // "Moonlight Sonata" file seems to have two "NoteOn" without intervening "NoteOff"
                // Alternative strategy here would be to issue a changeEnvelope to repeat the attack 
                release(n);
            }
            attack(n);
#if ASSERTIONS
            ++onLevel[n];
#endif
            break;
        }
        off:
        case MEK_NoteOff: {
            int n = note()& 0x7F;
            release(n);
            Assert(onLevel[n]>0);
#if ASSERTIONS
            --onLevel[n];
#endif
            break;
        }
    }
}

void AdditiveInstrument::stop() {
    for( size_t i=0; i<sizeof(keyArray)/sizeof(keyArray[0]); ++i ) 
        release(i);
}

NameToWaSetMap TheWaSetMap;    //!< Map from ids (e.g. "adr-oom" to WaCoders. 

void LoadWaCoder( const std::string id, const std::string path ) {
	auto i = TheWaSetMap.insert( std::make_pair(id,(const WaSet*)NULL)).first;
	if( !i->second ) {
		i->second = new WaSet(path);
	}
}

#if 0 /* Is this code used any more? */

static const WaSet* GetWaCoder( const std::string& filename ) {
    auto i = TheWaSetMap.insert( std::make_pair(filename,(const WaSet*)NULL)).first;
    if( !i->second ) {
        i->second = new WaSet(filename);
    }
    return i->second;
}
#endif

//-----------------------------------------------------------------
// MidiPlayer
//-----------------------------------------------------------------

#define MIDI_LOG 0

#if MIDI_LOG
static FILE* TheLogFile;

static void OpenLogFile() {
    if( !TheLogFile ) {
        TheLogFile = fopen("C:/tmp/midi.log","w");
    }
}
#endif

void MidiPlayer::stop() {
    for( auto ip = myEnsemble.begin(); ip!=myEnsemble.end(); ++ip ) {
        MidiInstrument* i = *ip;
        i->stop();
        delete i;
    }
    myEnsemble.clear();
}

std::string MidiTrack::trackId( int k ) const {
    std::string id;
    getInfo();
    if( !info.trackName.empty() )
        id = info.trackName;
    else if( !info.firstComment.empty() )
        id = info.firstComment;
    else {
        char buf[20];
        sprintf(buf,"Track %d",k);
        id = buf;
    }
    return id;
}

void MidiPlayer::play( const MidiTune& tune, const NameToWaSetMap& trackMap ) {
    myZeroTime = 0;
    myTickPerSec = tune.myTickPerSec;
    Assert(myTickPerSec>0);
#if MIDI_LOG
    OpenLogFile();
#endif
    Assert( myEnsemble.empty() );
    int k = 0;
    for( size_t i=0; i<tune.myTrack.size(); ++i ) {
        const MidiTrack& t = tune.myTrack[i];
        MidiTrack::infoType info( t.getInfo() ); 
        if( info.hasNotes ) {
            ++k;
            std::string id = t.trackId(k);
            MidiInstrument* it;
            auto j = trackMap.find( id );
            if( j!=trackMap.end() ) {
#if HAVE_WriteWaPlot
                fprintf( TheLogFile, "[%d] %s: %s\n",k,id.c_str(),j->second->name().c_str());
                char buf[128];
                sprintf(buf,"C:/tmp/waplot.%d.dat",k);
                WriteWaPlot(buf,tune->myTrack[i],myTickPerSec);
#endif
                it = new WaInstrument(myTickPerSec,*j->second);
            } else {
#if MIDI_LOG
                fprintf( TheLogFile, "%s: (additive)\n",id.c_str());
#endif
                it = new AdditiveInstrument();
            }
            it->setTrack( tune.myTrack[i] );
            myEnsemble.push_back(it);
        }
    }
#if MIDI_LOG
    fflush(TheLogFile);
#endif
}

void MidiPlayer::update() {
    if( !myEnsemble.size() )
        // Nothing to play
        return;
    Assert(myTickPerSec>0);
    Assert( MidiKey440AFreq>0 );

    // Get absolute time
    double currentTime = HostClockTime();
    if( !myZeroTime ) 
        myZeroTime=currentTime;
    // Convert to MIDI time
    MidiTrackReader::timeType t = MidiTrackReader::timeType((currentTime-myZeroTime)*myTickPerSec);
    for( MidiInstrument*ip : myEnsemble) {
        MidiInstrument& i = *ip;
        // Update track i 
        while( i.event()!=MEK_EndOfTrack && i.time()<=t ) {
            i.processEvent();
            i.next();
        }
    }
}