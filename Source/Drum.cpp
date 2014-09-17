#include "Drum.h"

#if 0
class DrumInstrument: public PatchInstrument {
    const Drum& myDrum;
    typedef Midi::Event Event;
    /*override*/ void noteOn( const Event& on, const Event& off );
    void release( int note );
public:
    DrumInstrument( const Drum& drum );
};

DrumInstrument::DrumInstrument( const Drum& drumSet ) : myDrum(drumSet) {
}

void DrumInstrument::noteOn( const Event& on, const Event& off ) {
    unsigned note = on.note();
    Assert(note<128);
    const PatchSample& ps = myDrum.patchSample();
    playSource(ps,note,1.0f,on.velocity()*(1.0f/127));
}

Midi::Instrument* Drum::makeInstrument() const{
    return new DrumInstrument(*this);
}
#endif