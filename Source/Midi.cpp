#include "Midi.h"
#include <cstdio>
#include <algorithm>

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
static const char* ProgramNameTable[] ={
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
    "Gunshot" // [127]
};

//! Get string description of value in a MIDI Program Change message. 
const char* ProgramName(int program) {
    static const unsigned n = sizeof(ProgramNameTable)/sizeof(ProgramNameTable[0]);
    Assert(n==128);
    Assert(strcmp(ProgramNameTable[127], "Gunshot")==0);
    if(unsigned(program)<n)
        return ProgramNameTable[program];
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
                j->name = Midi::ProgramName(j->program);
        } else {
            // Fall back to channel number and program name.  Channel numbers are unqiue.  Program name is informational.
            for(auto j=first; j!=last; ++j) {
                char buf[20];
                std::sprintf(buf, "%u. ", j->channel);
                j->name = std::string(buf) + Midi::ProgramName(j->program);
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

void Tune::reportError(const char* format, unsigned value) {
    char buf[128];
    Assert(std::strlen(format) + 8 <= sizeof(buf));
    sprintf(buf, format, value);
    myReadStatus = buf;
}

unsigned Tune::parseVariableLen(const uint8_t*& first, const uint8_t* last) {
    unsigned value = 0;
    uint8_t c;
    do {
        if( first>=last ) {
            myReadStatus = "truncated value";
            return value;
        }
        c = *first++;
        value = value*128 + (c&0x7F);
    } while(c&0x80);
    return value;
}

void Tune::parseTrack(std::string& trackName, const uint8_t* first, const uint8_t* last) {
    // To minimize round-off error, the "current time" is split into two parts.
    Event::timeType timeAtLastTempoChange = 0;      // In our Event time units
    unsigned timeInTicksAfterLastTempoChange = 0;   // In MIDI "delta time"
    double timeScale = timeUnitsPerTick(500000);    // Conversion to our units from MIDI ticks.

    unsigned status = 0;
    unsigned channelRemap[16];
    for( size_t i=0; i<16; ++i )
        channelRemap[i] = ~0u;
    size_t virtualChannelBase = myChannelMap.size();
    while( first<last ) {
        timeInTicksAfterLastTempoChange += parseVariableLen(first, last);
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
                    break;
                case 0x2F:                      // End of track
                    return;
                case 0x51: {                    // Set tempo
                    Assert(len==3);
                    unsigned microsecondsPerQuarterNote = first[0]<<16 | first[1]<<8 | first[2];
                    timeAtLastTempoChange += unsigned(timeInTicksAfterLastTempoChange*timeScale + 0.5);
                    timeInTicksAfterLastTempoChange = 0;
                    timeScale = timeUnitsPerTick(microsecondsPerQuarterNote);
                    break;
                }
                default:
                    break;
            }
            first += len;
        } else {
            auto kind = c>>4;
            unsigned virtualChannel = channelRemap[c&0xF];
            if((kind < 0xF && virtualChannel==~0u) || kind==0xC) {
                virtualChannel = myChannelMap.size();
                myChannelMap.pushBack(Channel());
                channelRemap[c&0xF] = virtualChannel;
            }
            Event::timeType time = timeAtLastTempoChange + unsigned(timeInTicksAfterLastTempoChange*timeScale + 0.5);
            switch(kind) {
                case 0x8:                       // Note off
                case 0x9: {                     // Note on (though it's really "note off" if velocity is 0).
                    Event e(time, virtualChannel, kind==0x8 || first[1]==0 ? Event::noteOff : Event::noteOn);
                    e.myNote = first[0] & 0x7F;
                    e.myVelocity = first[1] & 0x7F;
                    myEventSeq.pushBack(e);
                    first += 2;
                    break;
                }
                case 0xA: {                     // Note Aftertouch
                    Event e(time, virtualChannel, Event::noteAfterTouch);
                    e.myNote = first[0] & 0x7F;
                    e.myAfterTouch = first[1] & 0x7F;
                    first += 2;
                    break;
                }
                case 0xB: {                     // Controller change
                    Event e(time, virtualChannel, Event::controlChange);
                    e.myControllerNumber = first[0] & 0x7F;
                    e.myControllerValue = first[1] & 0x7F;
                    first += 2;
                    break;
                }
                case 0xC: {                      // Program change
                    myChannelMap.myChannels[virtualChannel].myProgram = first[0] & 0x7F;
                    first += 1;
                    break;
                }
                case 0xD: {                     // Channel aftertouch
                    Event e(time, virtualChannel, Event::channelAfterTouch);
                    e.myAfterTouch = first[1] & 0x7F;
                    first += 2;
                    break;  
                }
                case 0xE: {                     // Pitch bend
                    Event e(time, virtualChannel, Event::channelAfterTouch);
                    e.myPitchBend = first[0] & 0x7F | (first[1] & 0x7F)<<7;
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
                    return reportError("Low bit of status not set?");
                    break;
            }
            status = c;
        }
    }
    // Should report warninga about missing end-of-track?
}

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

void Tune::canonicalizeEvents() {
    std::vector<Event> missing;
    const unsigned nullTime = ~0u;
    NoteTracker<Event*> on(*this,nullptr);
    auto& seq = myEventSeq.myEvents;
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
            // Missing event.  Insert one.
            Event final(lastTime,event->channel(),Event::noteOff);
            final.setNote(event->note(),0);
            missing.push_back(final);
        }
    });
    // Insert extra off events first.  Stable sort will keep them in front of following on events.
    seq.insert(seq.begin(), missing.begin(), missing.end());
    myEventSeq.sortByTime();
    // nulled events are now last.  Chop them off.
    auto z = seq.end();
    while(z>seq.begin() && z[-1].time()==nullTime)
        --z;
    seq.resize(z-seq.begin());
}

void Tune::parse(const uint8_t* first, const uint8_t* last) {
    Assert(first<=last);
    clear();
    myReadStatus.clear();
    // Read header
    MidiHeader h;
    first = readHeader(h, first, last, "MThd");
    if(!first) 
        return reportError("bad MThd header");
    Assert(h.format<=1);
    Assert(h.chunkSize==6);
    if(h.format==0) {
        if(h.numTracks!=1) 
            return reportError("format 0 must have one track, not %u tracks", h.numTracks);
    } else {
        if(h.numTracks<=0) 
            return reportError("nonzero format 0 shold have at least one track");
    }
    if(h.division&0x8000)
        return reportError("SMTPE not implemented");
    else
        myTicksPerQuarterNote = h.division;   

    std::vector<ChannelMap::channelInfo> channelNames;

    // Parse each track 
    for( unsigned i=0; i<h.numTracks; ++i) {
        MidiTrackHeader t;
        first = readHeader(t, first, last, "MTrk");
        if(!first)
            return reportError("track %u has bad MTrk header", i);
        if(t.chunkSize > size_t(last-first))
            return reportError("track %u has bad size", i);
        unsigned virtualChannelBase = myChannelMap.size();
        ChannelMap::channelInfo c;
        parseTrack(c.name, first, first+t.chunkSize);
        if( !myReadStatus.empty() )
            return;
        for(unsigned i=virtualChannelBase; i<myChannelMap.size(); ++i ) {
            c.channel = i;
            c.program = myChannelMap[i].program();
            channelNames.push_back(c);
        }
        first += t.chunkSize;
    }
    canonicalizeEvents();

    // Finish setting up myChannelMap
    myChannelMap.assign(channelNames.data(), channelNames.data()+channelNames.size());

    Assert(assertOkay());
}

bool Tune::readFromFile(const std::string& filename) {
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
        parse(buf, buf+s);
    } else {
        myReadStatus = "empty file";
    }
    delete[] buf;
    return myReadStatus.empty();
};

} // namespace Midi