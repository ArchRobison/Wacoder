/* Copyright 2014 Arch D. Robison

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
Playing MIDI files through a Synthesizer
*******************************************************************************/

#ifndef MidiPlayer_H
#define MidiPlayer_H

#include "Midi.h"

namespace Midi {

class Instrument {
public:
    virtual void noteOn( const Event& on, const Event& off ) = 0;
    virtual void noteOff( const Event& off) = 0;
    virtual void stop() = 0;
    virtual ~Instrument() {}
};

//! Instance plays a tune.
class Player {
    typedef std::vector<Instrument*> ensembleType;
    ensembleType myEnsemble;
    /** Units = seconds */
    double myZeroTime;
    //! Conversion factor for deltaTime per second 
    float myTicksPerSec;
    std::vector<uint16_t> myDuration;
    EventSeq::iterator myEventPtr;
    EventSeq::iterator myEndPtr;
    std::vector<uint16_t>::const_iterator myDurationPtr;
    const Midi::Tune* myTune;
    Player( const Player& ) = delete;
    void operator=( const Player& ) = delete;
    void clear();
    void computeDuration(const Tune& tune);
public:
    //! Construct player with no tune to play.
    Player() {}
    ~Player();
    //! Prepare to play tune
    void preparePlay(const Tune& tune);
    //! Tune passed to preparePlay.  Valid until commencePlay is called.
    const Midi::Tune& tune() const {return *myTune;}
    //! May be called between preparePlay and commencePlay to assign an instrument.
    /** Instrument will eventually be deleted with "delete". */
    void setInstrument(Event::channelType k, Instrument* i) {
        myEnsemble[k] = i;
    }
    //! Assign default instruments and commence playing tune
    void commencePlay();
    //! Stop current tune
    void stop();
    //! Update player
    /** Should be polled rapidly (e.g. at video frame rate) */
    void update();
};

} // namespace Midi

#endif /* MidiPlayer_H */