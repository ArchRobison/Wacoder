#ifndef ToneSet_H
#define ToneSet_h

#include "Orchestra.h"
#include "Patch.h"

class ToneSet: public Synthesizer::SoundSet {
    Patch* myPatch; // FIXME - use std::unique_ptr here?
public:
   /*override*/ Midi::Instrument* makeInstrument() const;
   const Patch& patch() const {return *myPatch;}
   /** Note: takes ownershp. */
   ToneSet(Patch* p) : myPatch(p) {}
};

#endif /*ToneSet_H*/