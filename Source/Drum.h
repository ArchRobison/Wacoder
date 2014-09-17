#ifndef Drum_H
#define Drum_H

class SF2SoundSet;

#if 0
class Drum: public Synthesizer::SoundSet {
    Drum(const Drum&) = delete;
    void operator=(const Drum&) = delete;
public:
   /*override*/ Midi::Instrument* makeInstrument() const;
   Drum(SF2SoundSet& s, unsigned instrument, unsigned bank) : SoundSet(s,instrument,bank) {
       Assert(bank==1);
   }
};
#endif

#endif /* Drum_H */