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

/******************************************************************************
MIDI Parsing
*******************************************************************************/

#ifndef Midi_H
#define Midi_H

// Suppo
#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>
#include "AssertLib.h"

namespace Midi {

class Tune;
class ChannelMap;

//! Get string description of value in a MIDI Program Change message. 
const char* ProgramName(int program);

//! Number of seconds per unit time increment in an Event.
static const float SecondsPerTimeUnit = 1.0f/4096;

class Event {
public:
    typedef uint32_t timeType;
    typedef uint8_t channelType;
    typedef uint8_t noteType;
    typedef uint8_t velocityType;
private:
    timeType myTime;
    channelType myChannel;
    uint8_t myKind;
    union {
        struct {
            union {
                noteType myNote;
                uint8_t myControllerNumber;
            };
            union {
                velocityType myVelocity;
                uint8_t myAfterTouch;
                uint8_t myControllerValue;
            };
        };
        int16_t myPitchBend;
    };
    friend class Tune;
public:
    enum class eventKind {
        noteOff,
        noteOn,
        noteAfterTouch,
        channelAfterTouch,
        pitchBend,
        controlChange
    };

    static const eventKind noteOn = eventKind::noteOn;
    static const eventKind noteOff = eventKind::noteOff;
    static const eventKind noteAfterTouch = eventKind::noteAfterTouch;
    static const eventKind channelAfterTouch = eventKind::channelAfterTouch;
    static const eventKind pitchBend = eventKind::pitchBend;
    static const eventKind controlChange = eventKind::controlChange;

    Event() : myTime(~0u), myKind(0xFF) {}
    Event( unsigned time, unsigned channel, eventKind kind ) : myTime(time), myChannel(channel), myKind(uint8_t(kind)) {}
    void setNote( noteType note, velocityType velocity ) {
        Assert(kind()==noteOn||kind()==noteOff);
        myNote = note;
        myVelocity = velocity;
    }
    // Absolute time of the event.  Multiply by SecondsPerTimeUnit to get time in seconds.
    timeType time() const { return myTime; }

    // Kind of the event
    eventKind kind() const { return eventKind(myKind); }

    // Channel of the event, renumbered so that channels on different tracks have unique channels.
    channelType channel() const {
        return myChannel;
    } 
    velocityType velocity() const {
        Assert(kind()==noteOn || kind()==noteOff);
        return myVelocity;
    }
    noteType note() const {
        Assert(kind()==noteOn || kind()==noteOff || kind()==noteAfterTouch);
        return myNote;
    }
};

//! Return pitch, in hertz, of MIDI note.
inline float PitchOfNote(Event::noteType note) {
    return 440*std::pow(1.059463094f, (int(note)-69));
}

class EventSeq {
    std::vector<Event> myEvents;
    friend class Tune;
public:
    typedef std::vector<Event>::const_iterator iterator;
    iterator begin() const { return myEvents.begin(); }
    iterator end() const { return myEvents.end(); }
    void clear() {
        myEvents.clear();
    }
    void pushBack( const Event& e ) {
        myEvents.push_back(e);
    }
    void sortByTime() {
        std::stable_sort(myEvents.begin(),myEvents.end(),[](const Event& x, const Event& y) {
            return x.time()<y.time();
        });
    }
};

class Channel {
    std::string myName;
    uint8_t myProgram;
    friend class Tune;
    friend class ChannelMap;
public:
    const std::string& name() const {return myName;}
    uint8_t program() const {return myProgram;}
    Channel() : myProgram(0) {}
};

//! Map from channel numbers in a Event to its Channel
class ChannelMap {
    std::vector<Channel> myChannels;
    std::vector<Event::channelType> mySortedByName;
    struct channelInfo;
    void assign(channelInfo* first, channelInfo* last);
public:
    const Channel& operator[]( unsigned channel ) const {return myChannels[channel];}
    size_t indexOf( const std::string& name );
    size_t size() const {return myChannels.size();}
    bool empty() const {return myChannels.empty();}
    void clear() {
        myChannels.clear();
        mySortedByName.clear();
    }
    void pushBack( const Channel& c ) {
        myChannels.push_back(c);
    }
    //! Return index of channel with given name, or ~0u if none found.
    unsigned findByName( const std::string& name ) const;
    friend class Tune;
};

class Tune {
    EventSeq myEventSeq;
    ChannelMap myChannelMap;
    uint16_t myTicksPerQuarterNote;
    std::string myReadStatus;
    void parse(const uint8_t* first, const uint8_t* last);
    void parseTrack(std::string& trackName, const uint8_t* first, const  uint8_t* last);
    unsigned parseVariableLen(const uint8_t*& first, const uint8_t* last);
    double timeUnitsPerTick(unsigned microsecondsPerQuarterNote ) {
        return microsecondsPerQuarterNote/ (1000000 * SecondsPerTimeUnit * myTicksPerQuarterNote);
    }
    void reportError(const char* format, unsigned value=0);
    // Ensure that "on note" and "off note" events are paired correctly.  
    // Inserts/erases "off note" events to enforce pairing.
    void canonicalizeEvents();
#if ASSERTIONS
    // Check that Tune is in canonical form.
    bool assertOkay() const;
#endif
public:
    Tune() : myTicksPerQuarterNote(0) {}
    bool empty() const {
        return myChannelMap.empty();
    }
    void clear() {
        myEventSeq.clear();
        myChannelMap.clear();
        myTicksPerQuarterNote = 0;
    }
    // Events, sorted by time, channel, note, velocity
    const EventSeq& events() const {return myEventSeq;}
    // Number of channels
    const ChannelMap& channels() const {return myChannelMap;}
    // Read from a file.  Return true if successful, false otherwise.
    bool readFromFile( const std::string& filename );
    // Return reason that more recent readFromFile failed, or empty string if read succeeded.
    const std::string& readStatus() const {return myReadStatus;}
    // Invoke f(eventOn,eventOff) for all notes 
    template<typename F>
    void forEachNote(const F& f) const;
};

template<typename T>
class NoteTracker {
    const size_t nRow;
    std::vector<T> myArray;
    static const size_t nCol = 128;
public:
    NoteTracker(const Tune& tune, T init=T()) : nRow(tune.channels().size()), myArray(nRow*nCol, init) {}
    T& operator[]( const Event& e ) {
        Assert(e.kind()==Event::noteOn || e.kind()==Event::noteOff);
        Assert(e.channel()<nRow);
        Assert(e.note()<nCol);
        return myArray[nCol*e.channel() + e.note()];
    }
    template<typename F>
    void forEach( const F& f ) {
        for( const T& value: myArray )
            f(value);
    }
};

template<typename F>
void Tune::forEachNote(const F& f) const {
    NoteTracker<const Event*> onEvent(*this,nullptr);
    for( const Event& e: events() ) {
        switch( e.kind() ) {
            case Event::noteOn: {
                onEvent[e] = &e;
                break;
            }
            case Event::noteOff: {
                const Event*& start = onEvent[e];
                if( start ) {
                    f( *start, e );
                    start = nullptr;
                }
                break;  
            }
        }
    }
}

} // namespace Midi


#endif /* Midi_H */