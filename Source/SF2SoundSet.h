#ifndef SF2Patch_H
#define SF2Patch_H

#include "Synthesizer.h"
#include "Utility.h"
#include "Orchestra.h"
#include <string>
#include <Utility>

class SF2Source;
class SF2SoundSet;
class SF2Bank;

class SF2Sample: public Synthesizer::Waveform {
    // ~timeType(0) if not a looping patch.
    timeType myLoopStart;
    // ~timeType(0) if not a looping patch.
    timeType myLoopEnd;
    // Nominal pitch as a Midi note
    uint8_t myOriginalPitch;
    int8_t myPitchCorrection;
    // Sampe rate at which sample was taken
    float mySampleRate;  
    friend class SF2Source;
    friend class SF2SoundSet;
    friend class SF2Bank;
public:
    bool isLooping() const {
        return myLoopEnd!=~timeType(0); 
    }
    timeType loopStart() const {
        Assert(isLooping());
        return myLoopStart;
    }
    timeType loopEnd() const {
        Assert(isLooping());
        return myLoopEnd;
    }
    timeType soundEnd() const {return size()<<Waveform::timeShift;}
    float sampleRate() const {return mySampleRate;}
};

class SF2SoundSet: public Synthesizer::SoundSet {

    // If you add another field to playInfo, be sure to update playInfo::setDefault *AND* 
    // the switch statement in SF2Bank::constructPlayInfo.
    struct playInfo {
        uint16_t index;
        int16_t releaseVolEnv;
        uint8_t sampleModes:2;
        int8_t overridingRootKey;

        friend bool operator<(const playInfo& x, const playInfo& y) {
            return std::memcmp(&x,&y,sizeof(playInfo))<0;
        }
        friend bool operator==( const playInfo& x, const playInfo& y ) {
            return std::memcmp(&x,&y,sizeof(playInfo))==0;
        }
    };

    struct range {
        uint8_t low, high;
        bool contains( uint8_t x ) const {return low<=x && x<=high;}
        friend bool operator==( const range& x, const range& y ) {
            return x.low==y.low && x.high==y.high;
        }
     };

    struct noteVelocityRange {
        range note;
        range velocity;
    };

    class noteVelocityMap {
        typedef std::pair<noteVelocityRange,playInfo> itemType;
        SimpleArray<itemType> array;
        // Major key=low end of note range.  Minor key=low end of velocity range.
        struct orderByLow {
            bool operator()( const itemType& x, const itemType& y ) {
                return x.first.note.low<y.first.note.low ||
                       x.first.note.low==y.first.note.low && x.first.velocity.low<y.first.velocity.low;
            }
        };
    public:
        //! May modify data.
        void initialize( std::pair<noteVelocityRange,playInfo> data[], size_t n );
        const playInfo& find( unsigned note, unsigned velocity ) const;
    };

    noteVelocityMap myPresetMap
        ;                    // Index is into myInstrumentMap
    SimpleArray<noteVelocityMap> myInstrumentMap;   // Index is into mySamples
    SimpleArray<SF2Sample> mySamples;
    bool myIsDrum;
    std::string myName;
    friend class SF2Bank;
    friend class SF2Source;
public:
    const SF2Sample* begin() const {return mySamples.begin();}
    const SF2Sample* end() const {return mySamples.end();} 
    size_t size() const {return mySamples.size();}
    bool empty() const {return size()==0;}
    const SF2Sample& operator[]( size_t k ) const {return mySamples[k];}
    const SF2Sample& find( float freq ) const;
    /*override*/ Midi::Instrument* makeInstrument() const;
};

//! Base class for ToneInstrument and DrumInstrument
class SF2Instrument: public Midi::Instrument {   
    typedef Midi::Event Event;
    ~SF2Instrument();
    SF2Source* keyArray[128]; 
    const SF2SoundSet& mySet;
    /*override*/ void noteOn( const Event& on, const Event& off );
    /*override*/ void noteOff( const Event& off );
    /*override*/ void stop();
    void release( int note );
public:
    SF2Instrument(const SF2SoundSet&);
};

#endif /* SF2Patch_H */