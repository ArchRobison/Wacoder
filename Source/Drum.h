#ifndef Drum_H
#define Drum_H

#include "Patch.h"

class Drum: public Synthesizer::SoundSet {
    Patch* myPatch; // FIXME - use std::unique_ptr here?
    Drum(const Drum&) = delete;
    void operator=(const Drum&) = delete;
public:
    const PatchSample& patchSample() const {return (*myPatch)[0];}
   /*override*/ Midi::Instrument* makeInstrument() const;
   /** Note: takes ownershp. */
   Drum(Patch* p) : myPatch(p) {}
};

#endif /* Drum_H */