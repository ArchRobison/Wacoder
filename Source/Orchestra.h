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

#ifndef Orchestra_H
#define Orchestra_H

#include "Midi.h"

namespace Midi {

//! An object capable of translating MIDI events to issuance of Source objects.
class Instrument {
public:
    virtual void noteOn( const Event& on, const Event& off ) = 0;
    virtual void noteOff( const Event& off) = 0;
    virtual void stop() = 0;
    virtual ~Instrument() {}
};

//! Object that holds mapping of Midi channels to Instrument, and can use them to play a Tune.
class Orchestra {
    typedef std::vector<Instrument*> ensembleType;
    ensembleType myEnsemble;
    /** Units = seconds */
    double myZeroTime;
    std::vector<uint16_t> myDuration;
    EventSeq::iterator myEventPtr;
    EventSeq::iterator myEndPtr;
    std::vector<uint16_t>::const_iterator myDurationPtr;
    const Midi::Tune* myTune;
    Orchestra( const Orchestra& ) = delete;
    void operator=( const Orchestra& ) = delete;
    void clear();
    void computeDuration(const Tune& tune);
public:
    //! Construct player with no tune to play.
    Orchestra() {}
    ~Orchestra();
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

// The namespace for SoundSet could be Midi too, since it connects Midi stuff to Synthesizer stuff.
namespace Synthesizer {

//! A set of sounds and factory for producing instruments to play them.
class SoundSet {
public:
    virtual Midi::Instrument* makeInstrument() const = 0;

protected:
    SoundSet() {}
    ~SoundSet() {}
};

} // Synthesizer

#endif /*Orchestra_H */