#include "Midi.h"
#include <cstdio>
#include <algorithm>
#include <cerrno>

void SanityCheck() {
    char buf[1024];
    FILE* f = std::fopen("C:\\tmp\\midi\\abba-waterloo.mid", "r");
    long s = fread(buf, 1, 1024, f);
    Assert(s==1024);
    fclose(f);
}

static void ByteSwap(uint16_t& x) {
    x = x<<8 | x>>8;
}

static void ByteSwap(uint32_t& x) {
    x = x<<24 | x<<8&0xFF0000 | x>>8&0xFF00 | x>>24;
}

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

static void ByteSwap(MidiHeader& h) {
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

static void ByteSwap(MidiTrackHeader& t) {
    ByteSwap(t.chunkSize);
}

template<typename H>
static const uint8_t* readHeader(H& h, const uint8_t* first, const uint8_t* last, const char* id) {
    Assert(H::headerSize<=sizeof(H));
    if(memcmp(first, id, 4)!=0)
        return nullptr;
    Assert(last-first>=H::headerSize);
    memcpy(&h, first, H::headerSize);
    first += H::headerSize;
    ByteSwap(h);
    return first;
}

namespace Midi {

//! Reference: http://www.midi.org/techspecs/gm1sound.php
/** Table here is zero-based (documentation is mysteriously one-based) */
static const char* Bank0NameTable[] ={
    "Acoustic Grand Piano", // [0]
    "Bright Acoustic Piano",
    "Electric Grand Piano",
    "Honky-tonk Piano",
    "Electric Piano 1",
    "Electric Piano 2",
    "Harpsichord",
    "Clavi",
    "Celesta",
    "Glockenspiel",
    "Music Box",
    "Vibraphone",
    "Marimba",
    "Xylophone",
    "Tubular Bells",
    "Dulcimer",
    "Drawbar Organ", // [16]
    "Percussive Organ",
    "Rock Organ",
    "Church Organ",
    "Reed Organ",
    "Accordion",
    "Harmonica",
    "Tango Accordion",
    "Acoustic Guitar",
    "Acoustic Steel Guitar",
    "Electric Guitar (jazz)",
    "Electric Guitar (clean)",
    "Electric Guitar (muted)",
    "Overdriven Guitar",
    "Distortion Guitar",
    "Guitar harmonics",
    "Acoustic Bass",
    "Electric Bass (finger)",
    "Electric Bass (pick)",
    "Fretless Bass",
    "Slap Bass 1",
    "Slap Bass 2",
    "Synth Bass 1",
    "Synth Bass 2",
    "Violin",
    "Viola",
    "Cello",
    "Contrabass",
    "Tremolo Strings",
    "Pizzicato Strings",
    "Orchestral Harp",
    "Timpani",
    "String Ensemble 1",
    "String Ensemble 2",
    "SynthStrings 1",
    "SynthStrings 2",
    "Choir Aahs",
    "Voice Oohs",
    "Synth Voice",
    "Orchestra Hit",
    "Trumpet",
    "Trombone",
    "Tuba",
    "Muted Trumpet",
    "French Horn",
    "Brass Section",
    "SynthBrass 1",
    "SynthBrass 2",
    "Soprano Sax", // [64]
    "Alto Sax",
    "Tenor Sax",
    "Baritone Sax",
    "Oboe",
    "English Horn",
    "Bassoon",
    "Clarinet",
    "Piccolo",
    "Flute",
    "Recorder",
    "Pan Flute",
    "Blown Bottle",
    "Shakuhachi",
    "Whistle",
    "Ocarina",
    "Lead 1 (square)",
    "Lead 2 (sawtooth)",
    "Lead 3 (calliope)",
    "Lead 4 (chiff)",
    "Lead 5 (charang)",
    "Lead 6 (voice)",
    "Lead 7 (fifths)",
    "Lead 8 (bass + lead)",
    "Pad 1 (new age)",
    "Pad 2 (warm)",
    "Pad 3 (polysynth)",
    "Pad 4 (choir)",
    "Pad 5 (bowed)",
    "Pad 6 (metallic)",
    "Halo Pad",
    "Pad 8 (sweep)",
    "FX 1 (rain)",
    "FX 2 (soundtrack)",
    "FX 3 (crystal)",
    "FX 4 (atmosphere)",
    "FX 5 (brightness)",
    "FX 6 (goblins)",
    "FX 7 (echoes)",
    "FX 8 (sci-fi)",
    "Sitar",
    "Banjo",
    "Shamisen",
    "Koto",
    "Kalimba",
    "Bag pipe",
    "Fiddle",
    "Shanai",
    "Tinkle Bell",
    "Agogo",
    "Steel Drums",
    "Woodblock",
    "Taiko Drum",
    "Melodic Tom",
    "Synth Drum",
    "Reverse Cymbal",
    "Guitar Fret Noise",
    "Breath Noise",
    "Seashore",
    "Bird Tweet",
    "Telephone Ring",
    "Helicopter",
    "Applause",
    "Gunshot"       // [127]
};

static const char* PercussionNameTable[] = {
    "Acoustic Bass Drum", // [0] corresponds to instrument 35
    "Bass Drum 1",
    "Side Stick",
    "Acoustic Snare",
    "Hand Clap",
    "Electric Snare",
    "Low Floor Tom",
    "Closed Hi Hat",
    "High Floor Tom",
    "Pedal Hi-Hat",
    "Low Tom",
    "Open Hi-Hat",
    "Low-Mid Tom",
    "Hi-Mid Tom",
    "Crash Cymbal 1",
    "High Tom",
    "Ride Cymbal 1",
    "Chinese Cymbal",
    "Ride Bell",
    "Tambourine",
    "Splash Cymbal",
    "Cowbell",
    "Crash Cymbal 2",
    "Vibrasla",
    "Ride Cymbal 2",
    "Hi Bongo",
    "Low Bongo",
    "Mute Hi Conga",
    "Open Hi Conga",
    "Low Conga",
    "High Timbale",
    "Low Timbale",
    "High Agogo",
    "Low Agogo",
    "Cabasa",
    "Maracas",
    "Short Whistle",
    "Long Whistle",
    "Short Guiro",
    "Long Guiro",
    "Claves",
    "Hi Wood Block",
    "Low Wood Block",
    "Mute Cuica",
    "Open Cuica",
    "Mute Triangle",
    "Open Triangle"             // Corresponds to instrument 81
};

//! Get string description of value in a MIDI Program Change message. 
const char* Channel::programName(unsigned program) {
    static const unsigned n0 = sizeof(Bank0NameTable)/sizeof(Bank0NameTable[0]);
    Assert(n0==128);
    Assert(strcmp(Bank0NameTable[127], "Gunshot")==0);
    static const unsigned n1 = sizeof(PercussionNameTable)/sizeof(PercussionNameTable[0]);
    Assert(n1==81-35+1);
    Assert(strcmp(PercussionNameTable[81-35], "Open Triangle")==0);
    if(program<128)
        return Bank0NameTable[program];
    else if(35<=program-128 && program-128<=81 )
        return PercussionNameTable[program-128-35];
    else 
        return "unknown MIDI Program Change program";
}

struct ChannelMap::channelInfo {
    std::string name;
    unsigned program;
    unsigned channel;   // Virtual channel number
    static bool lessByName(const channelInfo& x, const channelInfo& y) {
        return x.name<y.name;
    }
    static bool equalByName(const channelInfo& x, const channelInfo& y) {
        return x.name==y.name;
    }
    static bool lessByProgram(const channelInfo& x, const channelInfo& y) {
        return x.program<y.program;
    }
    static bool equalByProgram(const channelInfo& x, const channelInfo& y) {
        return x.program==y.program;
    }
};

void ChannelMap::assign(channelInfo* first, channelInfo* last) {
    Assert( myChannels.size() == last-first );
    // See if we can use the existing names as unique identifiers
    std::sort(first, last, channelInfo::lessByName);
    auto i = std::adjacent_find(first, last, channelInfo::equalByName);
    if(i!=last) {
        // Nams were not unique.
        // See if we can use the existing program names as unique identifiers
        std::sort(first, last, channelInfo::lessByProgram);
        i = std::adjacent_find(first, last, channelInfo::equalByProgram);
        if(i==last) {
            // Programs are unique.  Convert them to names.
            for(auto j=first; j!=last; ++j)
                j->name = Midi::Channel::programName(j->program);
        } else {
            // Fall back to channel number and program name.  Channel numbers are unqiue.  Program name is informational.
            for(auto j=first; j!=last; ++j) {
                char buf[20];
                std::sprintf(buf, "%u. ", j->channel);
                j->name = std::string(buf) + Midi::Channel::programName(j->program);
            }
        }
        std::sort(first, last, channelInfo::lessByName);
    }
    // Initialized the "sorted by name" vector.
    mySortedByName.resize(myChannels.size());
    auto j = mySortedByName.begin();
    for( auto i=first; i<last; ++i, ++j ) {
        myChannels[i->channel].myName = i->name;
        *j = i->channel;
    }
#if ASSERTIONS
    for( unsigned k=0; k<myChannels.size(); ++k )
        Assert( findByName(myChannels[k].name())==k );
#endif
}

unsigned ChannelMap::findByName(const std::string& name) const {
    auto i = mySortedByName.begin();
    auto j = mySortedByName.end();
    if( i<j ) {
        while(j-i>1) {
            auto m = i + (j-i)/2u;
            if(name < myChannels[*m].name())
                j = m;
            else
                i = m;
        }
        Assert(j-i==1);
        if(myChannels[*i].name()==name)
            return *i;
    }
    return ~0u;
}

//==================================== Tune::parser ====================================

#define TUNE_LOG 0
#if TUNE_LOG
static const char* TuneLogFileName = "C:\\tmp\\tmp\\tunelog.txt";
static FILE* TuneLog;
#endif

class Tune::parser {
    Tune& tune;
    struct tempoMark {
        unsigned tickTime;          // MIDI time in ticks
        Event::timeType tockTime;   // Time in our units
        double timeScale;           // subsequent tock/tick ratio until next mark
        Event::timeType tockFromTick( unsigned tick ) const {
            Assert(tick >= tickTime);
            // Following assertion works because we call tockFromTick only for non-last tempoMarks in tempoMap
            Assert(tick < (this+1)->tickTime);
            return unsigned((tick-tickTime)*timeScale+0.5) + tockTime;
        }
    };
    std::vector<tempoMark> tempoMap;
    void assignTempo(tempoMark& m, unsigned tickTime, Event::timeType tockTime, unsigned microsecondsPerQuarterNote=500000) {
        Assert(myTicksPerQuarterNote>0);
        m.tickTime = tickTime;
        m.tockTime = tockTime;
        m.timeScale = microsecondsPerQuarterNote / (1000000 * SecondsPerTock * myTicksPerQuarterNote);
    }
    /** Returns pointer to the inserted tempoMark */
    std::vector<tempoMark>::iterator addTempoMark(unsigned tickTime, Event::timeType tockTime, unsigned microsecondsPerQuarterNote) {
        auto m = tempoMap.end()-2;
        Assert(tickTime>=m->tickTime);
        if( tickTime > m->tickTime ) {
            // Need a new entry.  Insert it just before the dummy entry.
            tempoMap.push_back(tempoMap.back());
            m = tempoMap.end()-2;
        }
        assignTempo(*m, tickTime, tockTime, microsecondsPerQuarterNote);
        return m;
     }
    std::string trackName;
    uint16_t myTicksPerQuarterNote;
    unsigned parseVariableLen(const uint8_t*& first, const uint8_t* last);
    void parseTrack(const uint8_t* first, const uint8_t* last);
    void throwError(const char* format, unsigned value=0);
    // Ensure that "on note" and "off note" events are paired correctly.  
    // Inserts/erases "off note" events to enforce pairing.
    void canonicalizeEvents();
public:
    class badFile {};
    parser(Tune& t) : tune(t), myTicksPerQuarterNote(0) {}

    /** Throws badFile exception if there is a problem parsing the file. */
    void parseFile(const uint8_t* first, const uint8_t* last);
};

void Tune::parser::throwError(const char* format, unsigned value) {
    char buf[128];
    Assert(std::strlen(format) + 8 <= sizeof(buf));
    sprintf(buf, format, value);
    tune.myReadStatus = buf;
    throw badFile();
}

unsigned Tune::parser::parseVariableLen(const uint8_t*& first, const uint8_t* last) {
    unsigned value = 0;
    uint8_t c;
    do {
        if( first>=last ) 
            throwError("truncated value");
        c = *first++;
        value = value*128 + (c&0x7F);
    } while(c&0x80);
    return value;
}

void Tune::parser::parseTrack(const uint8_t* first, const uint8_t* last) {
    unsigned tickTime = 0;
    auto currentTempo = tempoMap.begin();
    trackName.clear();
#if ASSERTIONS
    Event::timeType prevTockTime = 0;
#endif
    const size_t nPhysicalChannel = 16 + 128;   // 16 channels + 128 fake channels for percussion
    const unsigned nil = ~0u;                   // Denotes empty slow in channelRemap
    unsigned channelRemap[nPhysicalChannel];
    std::fill_n( channelRemap, nPhysicalChannel, nil );
    size_t virtualChannelBase = tune.myChannelMap.size();
    unsigned status = 0;                        // Status byte
    while( first<last ) {
        // Compute tockTime.
        if( unsigned deltaTime = parseVariableLen(first, last) ) {
            unsigned old = tickTime;
            tickTime += deltaTime;
            if( tickTime<old )
                throwError("delta time overflow");
            while(tickTime>=currentTempo[1].tickTime)
                // Advance to next tempoMark
                ++currentTempo;
        }
        Event::timeType tockTime = currentTempo->tockFromTick(tickTime);
#if ASSERTIONS
        Assert( tockTime>=prevTockTime );
        prevTockTime = tockTime;
#endif
        // MIDI files sometimes omit the status byte if it is the same as for the previous event.
        // Search Internet for "running status" to learn more.
        unsigned char c = *first&0x80 ? *first++ : status;
        if(c==0xFF) {
            // MetaEvent
            uint8_t type = *first++;
            unsigned len = parseVariableLen(first,last);
            switch(type) {
                case 0x3:                       // Track name
                    trackName = std::string((const char*)first, len);
#if TUNE_LOG
                    fprintf(TuneLog, "trackname %s\n", trackName.c_str());
#endif
                    break;
                case 0x2F:                      // End of track
#if TUNE_LOG
                    fprintf(TuneLog, "end of track\n");
                    fflush(TuneLog);
#endif               
                    return;
                case 0x51: {                    // Set tempo
                    Assert(len==3);
                    unsigned microsecondsPerQuarterNote = first[0]<<16 | first[1]<<8 | first[2];
#if TUNE_LOG
                    fprintf(TuneLog, "microsecondsPerQuarterNote=%u\n", microsecondsPerQuarterNote);
#endif
                    currentTempo = addTempoMark(tickTime,tockTime,microsecondsPerQuarterNote);
                    break;
                }
                default:
                    break;
            }
            first += len;
        } else {
            const unsigned physicalDrumChannel = 9;
            const unsigned allDrums = ~1u;
            const auto kind = c>>4;
            const unsigned physicalChannel = c&0xF;
            // Figure out where to look in the channelRemap table.
            unsigned remapIndex;
            if( physicalChannel==physicalDrumChannel ) {
                // Drum track has multiple virtual channels encoded on it.
                switch(kind) {
                    case 0x8:           // Note off
                    case 0x9:           // Note on
                    case 0xA:           // Note Aftertouch
                        remapIndex = 16 + (first[0] & 0x7F);  // Note signifies the "channel"
                        break;
                    default:
                        remapIndex = allDrums;   
                        break;
                }
            } else {
                remapIndex = physicalChannel;
            }
            // Get the virtual channel (or create one).
            unsigned virtualChannel;
            if( remapIndex != allDrums ) {
                virtualChannel = channelRemap[remapIndex];
                if((kind < 0xF && virtualChannel==nil) || kind==0xC) {
                    // Need to create a new virtual channel
                    virtualChannel = tune.myChannelMap.size();
                    tune.myChannelMap.pushBack(Channel(remapIndex<16 ? 0 : remapIndex-16+128));
                    channelRemap[remapIndex] = virtualChannel;
                }
            } else {
                virtualChannel = allDrums;
            }
#if TUNE_LOG
            fprintf(TuneLog, "virtual channel %u = physical channel %d\n", int(virtualChannel), int(c&0xF));
#endif
            switch(kind) {
                case 0x8:                       // Note off
                case 0x9: {                     // Note on (though it's really "note off" if velocity is 0).
                    Event e(tockTime, virtualChannel, kind==0x8 || first[1]==0 ? Event::noteOff : Event::noteOn);
                    e.myNote = first[0] & 0x7F;
                    e.myVelocity = first[1] & 0x7F;
                    tune.myEventSeq.pushBack(e);
                    first += 2;
                    break;
                }
                case 0xA: {                     // Note Aftertouch
                    Event e(tockTime, virtualChannel, Event::noteAfterTouch);
                    e.myNote = first[0] & 0x7F;
                    e.myAfterTouch = first[1] & 0x7F;
                    tune.myEventSeq.pushBack(e);
                    first += 2;
                    break;
                }
                case 0xB: {                     // Controller change
                    Event e(tockTime, virtualChannel, Event::controlChange);
                    e.myControllerNumber = first[0] & 0x7F;
                    e.myControllerValue = first[1] & 0x7F;
                    tune.myEventSeq.pushBack(e);
                    first += 2;
                    break;
                }
                case 0xC: {                     // Program change
                    if( virtualChannel==allDrums ) {
                        // Not implemented
                    } else {
                        tune.myChannelMap.myChannels[virtualChannel].myProgram = first[0] & 0x7F;
                        first += 1;
                    }
                    break;
                }
                case 0xD: {                     // Channel aftertouch
                    if( virtualChannel==allDrums ) {
                        // Not implemented
                    } else {
                        Event e(tockTime, virtualChannel, Event::channelAfterTouch);
                        e.myAfterTouch = first[1] & 0x7F;
                        tune.myEventSeq.pushBack(e);
                    }
                    first += 2;
                    break;  
                }
                case 0xE: {                     // Pitch bend
                    if( virtualChannel==allDrums ) {
                        // Not implemented
                    } else {
                        Event e(tockTime, virtualChannel, Event::channelAfterTouch);
                        e.myPitchBend = first[0] & 0x7F | (first[1] & 0x7F)<<7;
                        tune.myEventSeq.pushBack(e);
                    }
                    first += 2;
                    break;
                }
                case 0xF: {                     // SysEx event - skip it
                    unsigned len = parseVariableLen(first, last);
                    Assert(first[len-1]==0xF7);
                    first += len;
                    break;
                }
                default:
                    Assert(0);
                    throwError("High bit of status not set");
                    break;
            }
            status = c;
        }
    }
    // Should report warnings about missing end-of-track?
}

void Tune::parser::canonicalizeEvents() {
    std::vector<Event> missing;
    const unsigned nullTime = ~0u;
    NoteTracker<Event*> on(tune,nullptr);
    auto& seq = tune.myEventSeq.myEvents;
    unsigned lastTime = 0;
    for( Event& e: seq )
        switch( e.kind() ) {
            case Event::noteOn:
                if( Event*& f = on[e] ) {
                    Assert(e.time()>=f->time());
                    if( e.time()>f->time() ) {
                        // "note off" is missing.  Create one with same time as second "note on".
                        Event g(e.time(), e.channel(), Event::noteOff);
                        g.setNote(e.note(), e.velocity());
                        missing.push_back(g);
                        f = nullptr;
                    } else {
                        // Have a duplicate "note on".  Mark it for erasure.
                        e.myTime = nullTime;
                    }
                }
                on[e] = &e;
                break;
            case Event::noteOff:
                if( Event*& f = on[e]) {
                    f = nullptr;
                    if(e.time()>lastTime)
                        lastTime = e.time();
                } else {
                    // "note off" with no preceding "note on".  Mark it for erasure.
                    e.myTime = nullTime;
                }
                break;
        }
    // Now check for missing final "off note" events.
    on.forEach( [&]( Event* event ){
        if( event ) {
            // Last "on note" is missing a following "off note".  Create an "note off" for it.
            Event final(lastTime,event->channel(),Event::noteOff);
            final.setNote(event->note(),0);
            missing.push_back(final);
        }
    });
    // Insert extra off events first.  Stable sort will keep them in front of following on events.
    seq.insert(seq.begin(), missing.begin(), missing.end());
    std::stable_sort(seq.begin(),seq.end(),[](const Event& x, const Event& y) {
        return x.time()<y.time();
    });
    // nulled events are now last.  Chop them off.
    auto z = seq.end();
    while(z>seq.begin() && z[-1].time()==nullTime)
        --z;
    seq.resize(z-seq.begin());
}

void Tune::parser::parseFile(const uint8_t* first, const uint8_t* last) {
    Assert(first<=last);
    tune.clear();
    tune.myReadStatus.clear();
    // Read header
    MidiHeader h;
    first = readHeader(h, first, last, "MThd");
    if(!first) 
        throwError("bad MThd header");
    Assert(h.format<=1);
    Assert(h.chunkSize==6);
    if(h.format==0) {
        if(h.numTracks!=1) 
            throwError("format 0 must have one track, not %u tracks", h.numTracks);
    } else {
        if(h.numTracks<=0) 
            throwError("nonzero format 0 shold have at least one track");
    }
    if(h.division&0x8000)
        throwError("SMTPE not implemented");
    else
        myTicksPerQuarterNote = h.division;   
#if TUNE_LOG
    fprintf(TuneLog,"format=%d numTracks=%d division=%d\n",int(h.format),int(h.numTracks),int(h.division));
#endif
    // Initialized the tempoMap
    tempoMap.resize(2);
    assignTempo(tempoMap[0],0,0);       // Time zero
    assignTempo(tempoMap[1],~0u,~0u);   // Approximation of time infinity

    std::vector<ChannelMap::channelInfo> channelNames;

    // Parse each track 
    for( unsigned i=0; i<h.numTracks; ++i) {
        MidiTrackHeader t;
        first = readHeader(t, first, last, "MTrk");
        if(!first)
            throwError("track %u has bad MTrk header", i);
        if(t.chunkSize > size_t(last-first))
            throwError("track %u has bad size", i);
        unsigned virtualChannelBase = tune.myChannelMap.size();
        ChannelMap::channelInfo c;
        parseTrack(first, first+t.chunkSize);
        for(unsigned i=virtualChannelBase; i<tune.myChannelMap.size(); ++i ) {
            c.channel = i;
            c.program = tune.myChannelMap[i].program();
            c.name = trackName;         // Tentative name.  
            channelNames.push_back(c);
        }
        first += t.chunkSize;
    }
    // Fix up so that on/off are properly paired.
    canonicalizeEvents();

    // Finish setting up myChannelMap
    tune.myChannelMap.assign(channelNames.data(), channelNames.data()+channelNames.size());
}

//==================================== Tune ====================================

#if ASSERTIONS
bool Tune::assertOkay() const {
    NoteTracker<const Event*> on(*this, nullptr);
    unsigned lastTime = 0;
    for( const Event& e: events() ) {
        Assert(lastTime<=e.time());
        lastTime = e.time();
        switch( e.kind() ) {
            case Event::noteOn:
                Assert(!on[e]);
                on[e] = &e;
                break;
            case Event::noteOff:
                Assert(on[e]);
                on[e] = nullptr;
                break;
        }
    }
    return true;
}
#endif

bool Tune::readFromFile(const std::string& filename) {
#if TUNE_LOG
    TuneLog = std::fopen(TuneLogFileName,"w+");
    Assert(TuneLog);
#endif
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
            Assert(assertOkay());
        } catch( parser::badFile ) {
            Assert(!myReadStatus.empty());
        }
    } else {
        myReadStatus = "empty file";
    }
    delete[] buf;
#if TUNE_LOG
    std::fclose(TuneLog);
#endif
    return myReadStatus.empty();
};

} // namespace Midi