#include "ToneSet.h"

class ToneInstrument: public PatchInstrument {
    const ToneSet& myToneSet;
    /*override*/ void noteOn( const Event& on, const Event& off );
public:
    ToneInstrument( const ToneSet& toneset );
};

ToneInstrument::ToneInstrument(const ToneSet& toneSet) : myToneSet(toneSet) {
    Assert(!toneSet.patch().empty());
}

void ToneInstrument::noteOn( const Event& on, const Event& off ) {
    if( myToneSet.patch().empty() ) 
        return;
    int note = on.note();   // Used signed type, so that "note-69" can go negative
    float freq = 440*std::pow(1.059463094f, note-69);
    const PatchSample& ps = myToneSet.patch().find(freq);
    playSource(ps,note,freq/ps.pitch(),on.velocity()*(1.0f/127));
}

Midi::Instrument* ToneSet::makeInstrument() const {
    return new ToneInstrument(*this);
};