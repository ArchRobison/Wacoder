#include "Orchestra.h"
#include "AssertLib.h"
#include "Synthesizer.h"
#include "Host.h"
#include "Patch.h"
#include "DefaultSoundSet.h"
#include "ReadError.h"

//-----------------------------------------------------------------
// MidiInstrument subclasses
//-----------------------------------------------------------------

static Synthesizer::Waveform KeyWave;
static float Key440AFreq;

static Synthesizer::Envelope KeyAttack;
static Synthesizer::Envelope KeyRelease;

struct KeySoundInit {
    KeySoundInit();
} static TheKeySoundInit;

KeySoundInit::KeySoundInit() {
    double pi = 3.14159265358979;
    const size_t keyLength = 3207;
    KeyWave.resize(keyLength);
    for(int i=0; i<keyLength; ++i)
        KeyWave[i] = 0;
#if 1
    // Organ
    double phase = 0;
    for(int h=1; h<=19; h+=2) {
        for(int i=0; i<keyLength; ++i) {
            double omega = 2*pi/keyLength;
            const double x = i*(1.0/keyLength);
            const double a = 0.05;
            KeyWave[i] += float(sin(phase+i*omega*h))/h;
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
    rissetType r[11] ={
        { 1, 0, 1, 0.56, 0 },
        { 2, -3.48, 0.9, 0.56, 1 },
        { 3, 0, 0.65, 0.92, 0 },
        { 4, 5.11, 0.55, 0.92, 1.7 },
        { 5, 8.53, 0.325, 1.19, 0 },
        { 6, 4.45, 0.35, 1.7, 0 },
        { 7, 3.29, 0.25, 2, 0 },
        { 8, 2.48, 0.2, 2.74, 0 },
        { 9, 2.48, 0.15, 3, 0 },
        { 10, 0, 0.1, 3.76, 0 },
        { 11, 2.48, 0.075, 4.07, 0 }
    };

    // ?
    float phase = 0;
    for(int j=0; j<11; ++j) {
        for(int i=0; i<keyLength; ++i) {
            double omega = 2*pi/keyLength;
            const double x = i*(1.0/keyLength);
            const double a = 0.05;
            KeyWave[i] += 0.25f*float(sin(phase+i*omega*r[j].freqMul));
        }
    }
#endif
    KeyWave.complete();
    Key440AFreq = 440.f*keyLength/Synthesizer::SampleRate;

    KeyAttack.resize(100);
    for(int i=0; i<100; ++i) {
        const double x = i*(1.0/99);
        KeyAttack[i] = float(x);
    }
    KeyAttack.complete(true);

    KeyRelease.resize(100);
    for(int i=0; i<100; ++i) {
        const double x = i*(1.0/100);
        KeyRelease[i] = float(exp(-4*x));
    }
    KeyRelease.complete(false);
}

class AdditiveInstrument : public Midi::Instrument {
    Synthesizer::AsrSource* keyArray[128];
    int counter;
    /*override*/ void noteOn(const Midi::Event& on, const Midi::Event& off);
    /*override*/ void noteOff(const Midi::Event& off);
    /*override*/ void stop();
public:
    AdditiveInstrument() {
        std::fill_n(keyArray, 128, (Synthesizer::AsrSource*)nullptr);
        counter = 0;
    }
};

void AdditiveInstrument::noteOn(const Midi::Event& on, const Midi::Event& off) {
    unsigned n = on.note();
    unsigned v = on.velocity();
    Assert(n==off.note());
    Assert(on.channel()==off.channel());
    // Input parsing should remove on-without-off
    Assert(!keyArray[n]);
    float freq = Key440AFreq*(std::pow(1.059463094f, (int(n)-69))*(1+(counter+=19)%32*(.005f/32)));
    float speed = KeyAttack.size()*(16.f/Synthesizer::SampleRate);
    Synthesizer::AsrSource* k = Synthesizer::AsrSource::allocate(KeyWave, freq, KeyAttack, speed);
    // N.B. k is nullptr if allocation failed.  
    keyArray[n] = k;
    Play(k, v*(1.0f/512));
}

void AdditiveInstrument::noteOff(const Midi::Event& off) {
    unsigned n = off.note();
    Assert(n<128);
    // Canonicalization of the events should ensure that there was a preceeding noteOn.
    // However, allocation of the AsrSource may have failed, hence the check here.
    if( Synthesizer::AsrSource* k = keyArray[n] ) {
        k->changeEnvelope(KeyRelease, KeyRelease.size()*(4.0f/Synthesizer::SampleRate));
        keyArray[n] = nullptr;
    }
 }

void AdditiveInstrument::stop() {
    Midi::Event e(0,0,Midi::Event::noteOff);
    // Send implicit "off" to any keys that are down
    for(unsigned n=0; n<sizeof(keyArray)/sizeof(keyArray[0]); ++n) {
        if( keyArray[n] ) {
            e.setNote(n,0);
            noteOff(e);
        }
    }
}

class NullInstrument : public Midi::Instrument {
    /*override*/ void noteOn(const Midi::Event& on, const Midi::Event& off) {}
    /*override*/ void noteOff(const Midi::Event& off) {}
    /*override*/ void stop() {}
public:
    NullInstrument() {}
};

namespace Midi {

void Orchestra::clear() {
    for(size_t i=myEnsemble.size(); i-->0;)
        delete myEnsemble[i];
    myEnsemble.clear();
}

Orchestra::~Orchestra() {
    clear();
}

void Orchestra::preparePlay( const Tune& tune ) {
    clear();
    myTune = &tune;
    computeDuration(tune);
    myDurationPtr = myDuration.begin();
    myEventPtr = tune.events().begin();
    myEndPtr = tune.events().end();
    myEnsemble.resize(tune.channels().size(),nullptr);
}

void Orchestra::commencePlay() {
    // Add default instruments
    for( unsigned k=0; k<myEnsemble.size(); ++k ) {
        Instrument*& i = myEnsemble[k];
        if(!i) {
            const Channel& c = myTune->channels()[k];
            try {
                const auto* s = GetDefaultSoundSet(c.program());
                if( s )
                    i = s->makeInstrument();
                else
                    i = nullptr;
            } catch( const ReadError& ) {
                // FIXME - report error to user
            }
        }
        if(!i) {
            if(myTune->channels()[k].isDrum() )
                i = new NullInstrument();       // FIXME
            else
                i = new AdditiveInstrument();
        }
    }
    myTune = nullptr;
}

void Orchestra::stop() {
    for(Instrument* i: myEnsemble)
        i->stop();
}

/** Since this routine is tightly tied to method update, please keep it lexically close
    to that method, so that the correspondence can be checked. */
void Orchestra::computeDuration( const Tune& tune ) {
    myDuration.clear();
    NoteTracker<std::pair<const Event*, size_t>> noteStart(tune);
    for( const Event& e: tune.events() )
        switch( e.kind() ) {
            case Event::noteOn:
            case Event::noteOff: {
                auto& start = noteStart[e];
                if(const Event* on = start.first) {
                    Assert(on->note()==e.note());
                    // FIXME - should check if difference really fits
                    myDuration[start.second] = &e-on;
                    start.first = nullptr;
                }
                if( e.kind()==Event::noteOn ) {
                    start.first = &e;
                    start.second = myDuration.size();
                    myDuration.push_back(0);
                }
                break;
            }
        }
#if ASSERTIONS
    noteStart.forEach([](const std::pair<const Event*, size_t>& start) {
        // Input parsing should canonicalize input so that for any note, 
        // there is an off for every on, and off precedes next on
        Assert(!start.first);
    });
    auto d = myDuration.begin();
    Event::timeType t = 0;
    for(const Event& e: tune.events()) {
        Assert(e.time()>=t);
        t = e.time();
        if( e.kind()==Event::noteOn ) {
            Assert(d<myDuration.end());
            const Event& off = (&e)[*d];
            Assert(e.note()==off.note());
            Assert(e.channel()==off.channel());
            ++d;
        }
    }
#endif
}

void Orchestra::update(double secondsSinceTime0) {
    Assert(Key440AFreq>0);

    // Get current time in MIDI "tick" units
    auto t = Event::timeType(secondsSinceTime0/SecondsPerTock);

    // Process MIDI events up to time t
    for( ; myEventPtr<myEndPtr && myEventPtr->time()<=t; ++myEventPtr) {
        auto& e = *myEventPtr;
        switch(e.kind()) {
            case Event::noteOn: {
                const Event& off = myEventPtr[*myDurationPtr];
                Assert(e.note()==off.note());
                Assert(e.channel()==off.channel());
                Instrument* i = myEnsemble[e.channel()];
                i->noteOn(e,off);
                ++myDurationPtr;
                break;
            }
            case Event::noteOff:
                myEnsemble[e.channel()]->noteOff(e);
                break;
        }
    }
}

} // namespace Midi