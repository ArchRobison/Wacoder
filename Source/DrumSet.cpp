#include "DrumSet.h"

class DrumSetInstrument: public PatchInstrumentBase {
    const DrumSet& myDrumSet;
    typedef Midi::Event Event;
    /*override*/ void noteOn( const Event& on, const Event& off );
    void release( int note );
public:
    DrumSetInstrument( const DrumSet& drumSet );
};

DrumSetInstrument::DrumSetInstrument( const DrumSet& drumSet ) : myDrumSet(drumSet) {
}

void DrumSetInstrument::noteOn( const Event& on, const Event& off ) {
    unsigned note = on.note();
    Assert(note<128);
    const PatchSample& ps = myDrumSet[note];
    playSource(ps,note,1.0f,on.velocity()*(1.0f/127));
}

Midi::Instrument* DrumSet::makeInstrument() const{
    return new DrumSetInstrument(*this);
}