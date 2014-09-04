#ifndef DrumSet_H
#define DrumSet_H

#include "Patch.h"

class DrumSet: Synthesizer::SoundSet {
    Patch* myPatch[128];
    DrumSet(const DrumSet&) = delete;
    void operator=(const DrumSet&) = delete;
public:
   const PatchSample& operator[]( size_t k ) const {
       Assert(k<128);
       Assert(myPatch[k]);
       return (*myPatch[k])[0];
   }
   /*override*/ Midi::Instrument* makeInstrument() const;
};

#endif /* DrumSet_H */