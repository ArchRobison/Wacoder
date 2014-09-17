#ifndef SF2Bank_H
#define SF2Bank_H

#include <string>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <map>
#include "Utility.h"
#include "SF2SoundSet.h"

class SF2Bank {
    // Word-aligned 32-bit value.
    class uint32_w {
        uint16_t low;
        uint16_t high;
    public:
        operator uint32_t() const { return low+(high<<16); }
    };

    enum class Generator : uint16_t {
        startAddrsOffset=0,
        endAddrsOffset=1,
        startLoopAddrsOffset=2,
        endLoopAddrOffset=3,
        startAddrsCoarseOffset=4,
        modLfoToPitch=5,
        vibLfoToPitch=6,
        modEnvToPitch=7,
        pan=17,
        freqModLFO=22,
        attackModEnv=26,
        holdModEnv=27,
        decayModEnv=28,
        sustainModEnv=29,
        releaseModEnv=30,
        attackVolEnv=34,
        holdVolEnv=35,
        decayVolEnv=36,
        sustainVolEnv=37,
        releaseVolEnv=38,
        instrument=41,
        keyRange=43,
        velRange=44,
        initialAttenuation=48,
        sampleId=53,
        sampleModes=54,
        overridingRootKey=58
    };

    struct GenAmountType {
        union {
            int16_t int16;
            uint16_t uint16;
            uint8_t val8[2];
        };
        // Volume envelope in sec
        float volEnvSec() const {return exp2(int16/1200.f);}
    };

    enum class SampleLink : uint16_t {
        mono=1,
        right=2,
        left=4,
        linked=8,
        romMono=0x8001,
        romRight=0x8002,
        romLeft=0x8004,
        romLinked=0x8008
    };

    struct Rec_phdr {
        char name[20];
        uint16_t preset;
        uint16_t bank;
        uint16_t presetBagIndex;
        uint32_w library;
        uint32_w genre;
        uint32_w morphology;
    };

    struct Rec_bag {
        uint16_t genIndex;
        uint16_t modIndex;
    };

    struct Rec_mod {
        uint16_t stuff[5];  // FIXME
    };

    struct Rec_gen {
        Generator oper;
        GenAmountType amount;
    };

    struct bagModGen {
        SimpleArray<Rec_bag, 1> bag;
        SimpleArray<Rec_mod, 1> mod;
        SimpleArray<Rec_gen, 1> gen;
    };

    struct Rec_inst {
        char name[20];
        uint16_t instBagIndex;  // Index into ibag
    };

    struct Rec_shdr {
        char name[20];
        uint32_w start, end, startLoop, endLoop, sampleRate;
        uint8_t originalPitch;
        int8_t pitchCorrection;
        uint16_t sampleLink;
        SampleLink sampleType;
    };

    SimpleArray<Rec_phdr,1> phdr;
    bagModGen p;
    SimpleArray<Rec_inst,1> inst;
    bagModGen i;
    SimpleArray<Rec_shdr,1> shdr;
    SimpleArray<int16_t> samples;
    
    class phdrMapItem {
        uint16_t preset;
        uint16_t bank;
    public:
        uint16_t phdrIndex;
        void setLookupKey( uint16_t preset_, uint16_t bank_ ) {
            preset = preset_;
            bank = bank_;
        }
        friend bool operator<( const phdrMapItem& x, const phdrMapItem& y ) {
            return x.bank<y.bank || x.bank==y.bank && x.preset<y.preset;
        }
    };
    // Sorted by <
    SimpleArray<phdrMapItem> phdrMap;
    void createIndex();

    void dumpGen(FILE* f, const Rec_gen& g) const;

    friend class SF2Reader;
    typedef SF2SoundSet::playInfo playInfo;
    typedef SF2SoundSet::noteVelocityRange noteVelocityRange;
    class indexMap;
    bool constructPlayInfo( noteVelocityRange& r, playInfo& pi, const Rec_gen* first, const Rec_gen* last ) const;
    template<typename Container>
    void constructPlayInfoList( Container& list, unsigned firstZone, unsigned lastZone, const bagModGen& b );
public:
    SF2Bank() {}
    bool empty() const {return phdrMap.size()==0;}
    void load( const std::string& filename );
    void dump( const std::string& filename );
    //! Returns nullptr if instrument/bank combination does not exit.
    SF2SoundSet* createSoundSet( unsigned preset, unsigned bank );
};

#endif