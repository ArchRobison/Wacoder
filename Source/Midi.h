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
#ifndef Midi_H
#define Midi_H

#include "Utility.h"
#include <cstdio>
#include <cctype>
#include <string>
#include <vector>
#include <map>

class WaSet;
typedef std::map<std::string,const WaSet*> NameToWaSetMap;

//! Midi track
class MidiTrack: NoCopy {
public:
    struct infoType {
        bool isValid;
        bool hasNotes;
        std::string trackName;
        std::string firstComment;
        infoType() : isValid(false), hasNotes(false) {}
    };
    const infoType& getInfo() const;
    std::string trackId( int k ) const;
private:
    SimpleArray<byte> data;
    mutable infoType info;
    friend class MidiTrackReader;
    friend class MidiTune;
};

//! Tune read from a MIDI file/resource.
class MidiTune {
public:
    MidiTune() {myReadStatus[0] = 0;}
    ~MidiTune() {clear();}
    //! Returns true if successful, false otherwise.
    bool readFromFile( const char* filename );
    //! Description of why last read failed, or "" if successful.
    const char* readStatus() const {return myReadStatus;}
    void clear();
    //! Assign new MIDI data.
    //! Return number of tracks
    size_t nTrack() const {return myTrack.size();}
    const MidiTrack& operator[]( size_t i ) const {
        return myTrack[i];
    }
    float tickPerSec() const {return myTickPerSec;}
private:
    friend class MidiPlayer;
    SimpleArray<MidiTrack> myTrack;
    float myTickPerSec;
    void noteError( const char* format, unsigned value=0 );
    char myReadStatus[64];
    /** The data is copied, so it's safe to free [first,last) afterwards. */
    void assign(const byte* first, const byte* last);
};

#define MIDI_META(x) ((x)+0x10)

enum MidiEventKind {
    MEK_NoteOff=0x8,
    MEK_NoteOn=0x9,
    MEK_ControllerChange=0xB,
    MEK_ProgramChange=0xC,
    MEK_PitchBend=0xE,
    MEK_Comment=MIDI_META(0x1),
    MEK_TrackName=MIDI_META(0x3),
    MEK_Port=MIDI_META(0x21),
    MEK_EndOfTrack=MIDI_META(0x2f)
};

//! Class for parsing one Midi track.
class MidiTrackReader {
public:
    //! Construct reader for empty track.
    MidiTrackReader() : myEvent(MEK_EndOfTrack),  myTime(0) {}
    //! Initialize reader to read Midi events from given track
    void setTrack( const MidiTrack& track ) {
        myPtr = track.data.begin();
        myEnd = track.data.end();
#if ASSERTIONS
        myBegin = myPtr;
#endif
        readDeltaAndEvent();
    }
    typedef unsigned timeType;
    //! Return time for current record, in MIDI units.
    /** This is the sum of all deltaTime up to and including the current record. */
    timeType time() const {return myTime;}
    //! Return MidiEvent for current record.
    MidiEventKind event() const {
        return MidiEventKind(myEvent);
    }
    int channel() const {
        Assert( myEvent==MEK_NoteOff||myEvent==MEK_NoteOn );
        return myStatus&0xF;
    }
    int note() const {
        Assert( myEvent==MEK_NoteOff||myEvent==MEK_NoteOn );
        Assert( myPtr[0]<128 );
        return myPtr[0];
    }
    int velocity() const {
        Assert( myEvent==MEK_NoteOff||myEvent==MEK_NoteOn );
        Assert( myPtr[1]<128 );
        return myPtr[1];
    }
    float pitch() const {
        return 440*std::pow(1.059463094f, note()-69);
    }
    std::string text() const;
    struct futureEvent {
        MidiEventKind event;
        unsigned time;
        byte velocity;
    };
    // Return off MEK_Node_Off corresponding to current MEK_NoteOn and note.
    futureEvent findNextOffOrOn() const;

    void next() {
        Assert( myEvent!=MEK_EndOfTrack );
        myPtr += myLen;
        readDeltaAndEvent();
    }
private:
    timeType myTime;
    MidiEventKind myEvent;
    byte myStatus;
    size_t myLen;
    const byte* myPtr;
    const byte* myEnd;
#if ASSERTIONS
    const byte* myBegin;
    void dump( const char* filename );
#endif
    byte readByte() {return *myPtr++;}
    unsigned readVariableLen();
    void readDeltaAndEvent();
};

class MidiInstrument: public MidiTrackReader {
public:
    virtual void processEvent() = 0;
    virtual void stop() = 0;
};

//! Instance plays a tune.
class MidiPlayer {
    typedef std::vector<MidiInstrument*> ensembleType;
    ensembleType myEnsemble;
    /** Units = seconds */
    double myZeroTime;
    //! Conversion factor for deltaTime per second 
    double myTickPerSec;
public:
    //! Construct player with no tune to play.
    MidiPlayer() {}
    ~MidiPlayer() {
        for( size_t i=myEnsemble.size(); i-->0; )
            delete myEnsemble[i];
    }
    //! Initialize player to play given tune.
    void play( const MidiTune& tune, const NameToWaSetMap& trackMap );
    //! Stop current tune
    void stop();
    //! Update player
    /** Should be polled rapidly (e.g. at video frame rate) */
    void update();
};

extern NameToWaSetMap TheWaSetMap; 
void LoadWaCoder( const std::string id, const std::string path );

#endif /* Midi_H */