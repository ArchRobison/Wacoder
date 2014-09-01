#include "MidiPlayer.h"
#include "AssertLib.h"
#include "Synthesizer.h"
#include "Host.h"

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
            MidiKeyWave[i] += 0.25f*float(sin(phase+i*omega*r[j].freqMul));
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
    Synthesizer::MidiSource* keyArray[128];
    int counter;
#if ASSERTIONS
    int onLevel[128];
#endif
    /*override*/ void noteOn(const Midi::Event& on, const Midi::Event& off);
    /*override*/ void noteOff(const Midi::Event& off);
    /*override*/ void stop();
public:
    AdditiveInstrument() {
#if ASSERTIONS
        std::fill_n(onLevel, 128, 0);
#endif
        std::fill_n(keyArray, 128, (Synthesizer::MidiSource*)nullptr);
        counter = 0;
    }
};

void AdditiveInstrument::noteOn(const Midi::Event& on, const Midi::Event& off) {
    unsigned n = on.note();
    unsigned v = on.velocity();
    Assert(n==off.note());
    Assert(on.channel()==off.channel());
    float freq = Key440AFreq*float(std::pow(1.059463094f, (n-69))*(1+(counter+=19)%32*(.005f/32)));
    float speed = KeyAttack.size()*(16.f/Synthesizer::SampleRate);
    Synthesizer::MidiSource* k = Synthesizer::MidiSource::allocate(KeyWave, freq, KeyAttack, speed);
    keyArray[n] = k;
    Play(k, v*(1.0f/512));
#if ASSERTIONS
    ++onLevel[n];
#endif
}

void AdditiveInstrument::noteOff(const Midi::Event& off) {
    unsigned n = off.note();
    Assert(n<128);
    if(Synthesizer::MidiSource* k = keyArray[n]) {
        k->changeEnvelope(KeyRelease, KeyRelease.size()*(4.0f/Synthesizer::SampleRate));
        keyArray[n] = NULL;
    }
#if ASSERTIONS
    Assert(onLevel[n]>0);
    --onLevel[n];
#endif
}

void AdditiveInstrument::stop() {
    Midi::Event e(0,0,Midi::Event::noteOff);
    for(size_t i=0; i<sizeof(keyArray)/sizeof(keyArray[0]); ++i) {
        e.setNote(i,0);
        noteOff(e);
    }
}

namespace Midi {

void Player::clear() {
    for(size_t i=myEnsemble.size(); i-->0;)
        delete myEnsemble[i];
}

Player::~Player() {
    clear();
}

void Player::preparePlay( const Tune& tune) {
    myTune = &tune;
    myZeroTime = 0;
    myTicksPerSec = float(tune.ticksPerSecond());
    Assert(myTicksPerSec>0);
    computeDuration(tune);
    myDurationPtr = myDuration.begin();
    myEventPtr = tune.events().begin();
    myEndPtr = tune.events().end();
}

void Player::commencePlay() {
    // Add default instruments
    for(Instrument*& i: myEnsemble)
        if(!i)
            i = new AdditiveInstrument();
    // Get absolute time
    myZeroTime = HostClockTime();
    myTune = nullptr;
}

void Player::computeDuration( const Tune& tune ) {
    myDuration.clear();
    NoteTracker<std::pair<const Event*, size_t>> noteStart(tune);
    for( const Event& e: tune.events() )
        switch( e.kind() ) {
            case Event::noteOn: {
                auto& start = noteStart[e];
                start.first = &e;
                start.second = myDuration.size();
                myDuration.push_back(0);
                break;
            }
            case Event::noteOff: {
                auto& start = noteStart[e];
                if( start.first ) {
                    // FIXME - should check if difference really fits
                    myDuration[start.second] = &e-start.first;
                    start.first = nullptr;
                }
                break;
            }
        }
}

void Player::update() {
    if(!myZeroTime)
        // Nothing to play
        return;
    Assert(myTicksPerSec>0);
    Assert(Key440AFreq>0);

    // Get current time in MIDI "tick" units
    auto t = Event::timeType((HostClockTime()-myZeroTime)*myTicksPerSec);

    // Process MIDI events up to time t
    while(myEventPtr<myEndPtr && myEventPtr->time()<=t) {
        auto& e = *myEventPtr;
        switch(e.kind()) {
            case Event::noteOn:
                myEnsemble[e.channel()]->noteOn(e,myEventPtr[*myDurationPtr++]);
                break;
            case Event::noteOff:
                myEnsemble[e.channel()]->noteOff(e);
                break;
        }
    }
}

} // namespace Midi