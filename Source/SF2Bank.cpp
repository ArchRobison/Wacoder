#include "SF2Bank.h"
#include "SF2Reader.h"
#include "SF2SoundSet.h"
#include <algorithm>
#include <vector>

void SF2Bank::dumpGen( FILE* f, const Rec_gen& g) const {
    switch( g.oper ) {
        case Generator::startAddrsOffset:
            fprintf(f,"startAddrsOffset=%d",g.amount.int16);
            break;
        case Generator::endAddrsOffset:
            fprintf(f,"endAddrsOffset=%d",g.amount.int16);
            break;
        case Generator::startLoopAddrsOffset:
            fprintf(f,"startLoopAddrsOffset=%d",g.amount.uint16);
            break;
        case Generator::endLoopAddrOffset:
            fprintf(f,"endLoopAddrOffset=%d",g.amount.uint16);
            break;
        case Generator::freqModLFO:
            fprintf(f,"freqModLFO=%d",g.amount.int16);
            break;
        case Generator::attackVolEnv:
            fprintf(f,"attackVolEnv=%g",g.amount.volEnvSec());
            break;
        case Generator::releaseVolEnv:
            fprintf(f,"releaseVolEnv=%g",g.amount.volEnvSec());
            break;
        case Generator::instrument: {
            unsigned i = g.amount.uint16;
            fprintf(f,"instrument=%u[%s]",i,std::string(inst[i].name,20).c_str());
            break;
        }
        case Generator::keyRange:
            fprintf(f,"keyRange=[%d,%d]",g.amount.val8[0],g.amount.val8[1]);
            break;
        case Generator::velRange:
            fprintf(f,"velRange=[%d,%d]",g.amount.val8[0],g.amount.val8[1]);
            break;
        case Generator::initialAttenuation:
            fprintf(f,"initialAttenuation=%d",g.amount.int16);
            break;
        case Generator::sampleId:
            fprintf(f,"sampleId=%d",g.amount.uint16);
            break;
        case Generator::sampleModes: {
            static const char* const mode[4]= {"simple","continuous","noloop2","loop"};
            fprintf(f,"sampleModes=%s",mode[g.amount.int16&3]);
            break;
        }
        case Generator::overridingRootKey: 
            fprintf(f,"overridingRootKey=%d",g.amount.uint16);
            break;
        default:
            fprintf(f,"<oper %d>",int(g.oper));
            break;
    }
}

void SF2Bank::dump( const std::string& filename ) {
    FILE* d = fopen(filename.c_str(),"w+");
    if(!d)
        return;
    for( size_t i=0; i<phdr.size(); ++i ) {
        const auto& h = phdr[i];
        fprintf(d,"phdr[%u] %-20s preset=%u bank=%u\n",i,std::string(h.name,20).c_str(),h.preset,h.bank);
        for( size_t j=phdr[i].presetBagIndex; j<phdr[i+1].presetBagIndex; ++j ) {
            fprintf(d,"\tpreset zone %u\n",unsigned(j));
            fprintf(d,"\t    ");
            for( size_t k=p.bag[j].genIndex; k<p.bag[j+1].genIndex; ++k ) {
                 dumpGen(d,p.gen[k]);
                 fprintf(d," ");
             }
            fprintf(d,"\n");
       }
    }

    for( size_t i=0; i<inst.size(); ++i ) {
        const auto& I = inst[i];
        fprintf(d,"inst[%u] %-20s\n",i,std::string(I.name,20).c_str());
         for( size_t j=inst[i].instBagIndex; j<inst[i+1].instBagIndex; ++j ) {
            fprintf(d,"\tinst zone %u\n",unsigned(j));
            fprintf(d,"\t    ");
            for( size_t k=this->i.bag[j].genIndex; k<this->i.bag[j+1].genIndex; ++k ) {
                 dumpGen(d,this->i.gen[k]);
                 fprintf(d," ");
             }
            fprintf(d,"\n");
       }
    }

    for( unsigned i=0;i<shdr.size(); ++i ) {
        const auto& s = shdr[i];
        fprintf(d, "shdr[%u] %20s start=%u end=%u loop=[%u %u] rate=%u pitch=%u correction=%u\n", i, std::string(s.name, s.name+20).c_str(), s.start, s.end, s.startLoop, s.endLoop, s.sampleRate, s.originalPitch, s.pitchCorrection);
    }

    fclose(d);
}

void SF2Bank::load( const std::string& filename ) {
    SF2Reader r;
    r.open(filename);
    r.read(*this);
    r.close();
}

void SF2Bank::createIndex() {
    size_t n = phdr.size();
    phdrMap.resize(n);
    for( size_t i=0; i<n; ++i ) {
        auto& h = phdrMap[i];
        h.setLookupKey(phdr[i].preset,phdr[i].bank);
        h.phdrIndex = i;
    }
    std::sort(phdrMap.begin(),phdrMap.end());
}

bool SF2Bank::constructPlayInfo( noteVelocityRange& r, playInfo& pi, const Rec_gen* first, const Rec_gen* last ) const {
    auto* g = first;
    // keyRange must come first if present.
    if( g->oper==Generator::keyRange ) {
        r.note.low = g->amount.val8[0];
        r.note.high = g->amount.val8[1];
        ++g;
    } else {
        r.note.low = 0;
        r.note.high = 127;
    }
    // velRange must come next if present
    if(g<last && g->oper==Generator::velRange) {
        r.velocity.low = g->amount.val8[0];
        r.velocity.high = g->amount.val8[1];
        ++g;
    } else {
        r.velocity.low=0;
        r.velocity.high=127;
    }
    // Set default values
    pi.index = uint16_t(~0u);
    pi.releaseVolEnv = -12000;
    pi.sampleModes = 0;
    pi.overridingRootKey = -1;

    // Read any overrides
    for(; g<last; ++g) {
        switch(g->oper) {
            case Generator::releaseVolEnv:
                pi.releaseVolEnv = g->amount.int16;
                break;
            case Generator::instrument:
            case Generator::sampleId:
                pi.index = g->amount.uint16;
                return true;
            case Generator::sampleModes:
                pi.sampleModes = g->amount.int16;
                break;
            case Generator::overridingRootKey:
                Assert(-1<=g->amount.int16 && g->amount.int16<=127);
                pi.overridingRootKey = g->amount.int16;
                break;
        }
    }
    return false;
}

template<typename Container>
void SF2Bank::constructPlayInfoList( Container& list, unsigned firstZone, unsigned lastZone, const bagModGen& b ) {
    for(size_t j=firstZone; j<lastZone; ++j) {
        noteVelocityRange r;
        playInfo pi;
        if(constructPlayInfo(r, pi, &b.gen[b.bag[j].genIndex], &b.gen[b.bag[j+1].genIndex]))
            list.push_back(std::make_pair(r, pi));
    }
}

class SF2Bank::indexMap {
public:
    typedef std::vector<uint16_t>::iterator iterator;
    iterator end() {return myArray.end();}
    iterator begin() {return myArray.begin();}
    size_t size() const {return myArray.size();}
    indexMap( const std::vector<std::pair<noteVelocityRange,playInfo>>& list );
    indexMap( const std::vector<std::vector<std::pair<noteVelocityRange,playInfo>>>& list );
    size_t find( uint16_t index ) {
        iterator i = std::lower_bound(begin(),end(),index);
        Assert( i!=end() );
        Assert( *i==index );
        return i-begin();
    }
    uint16_t operator[]( size_t k ) const {
        return myArray[k];
    }
    void crunch( std::vector<std::pair<noteVelocityRange,playInfo>>& list );
private:
    std::vector<uint16_t> myArray;
};

void SF2Bank::indexMap::crunch( std::vector<std::pair<noteVelocityRange,playInfo>>& list ) {
    for( auto& pi: list )
        pi.second.index = find(pi.second.index);
}

 SF2Bank::indexMap::indexMap( const std::vector<std::pair<noteVelocityRange,playInfo>>& list ) : myArray(list.size()) {
    iterator i = myArray.begin();
    for( const auto& pi: list )
        *i++ = pi.second.index;
    std::sort(myArray.begin(),myArray.end());
    auto e = std::unique(myArray.begin(),myArray.end());
    myArray.resize(e-myArray.begin());
}

SF2Bank::indexMap::indexMap( const std::vector<std::vector<std::pair<noteVelocityRange,playInfo>>>& list ) {
    size_t n = 0;
    for( const auto& sublist: list )
        n += sublist.size();
    myArray.resize(n);
    iterator i = myArray.begin();
    for( const auto& sublist: list )
        for( const auto& pi: sublist )
            *i++ = pi.second.index;
    std::sort(myArray.begin(),myArray.end());
    auto e = std::unique(myArray.begin(),myArray.end());
    myArray.resize(e-myArray.begin());
}

SF2SoundSet* SF2Bank::createSoundSet( unsigned preset, unsigned bank ) {
    phdrMapItem k;
    k.setLookupKey(preset,bank);
    auto mapi = std::lower_bound(phdrMap.begin(),phdrMap.end(),k);
    if( mapi==phdrMap.end() || k<*mapi ) 
        return nullptr;
    auto& s = *new SF2SoundSet();
    s.myIsDrum = bank==1;           // FIXME - is there better way than hardwiring this?

    // Get presets
    unsigned i = mapi->phdrIndex;
    unsigned firstZone = phdr[i].presetBagIndex;
    unsigned lastZone = phdr[i+1].presetBagIndex;
    std::vector<std::pair<noteVelocityRange,playInfo>> presetList;
    constructPlayInfoList( presetList, firstZone, lastZone, this->p );
    indexMap instrumentMap( presetList );

    // Get instruments
    std::vector<std::vector<std::pair<noteVelocityRange,playInfo>> > instrumentList(instrumentMap.size());
    for( unsigned ii=0; ii<instrumentMap.size(); ++ii ) {
        unsigned instIndex = instrumentMap[ii];
        unsigned firstZone = inst[instIndex].instBagIndex;
        unsigned lastZone = inst[instIndex+1].instBagIndex;
        // FIXME - save time/space by gathering only instruments whose ranges overlap ranges of preset[ii]
        constructPlayInfoList( instrumentList[ii], firstZone, lastZone, this->i );
    }
    indexMap sampleIdMap( instrumentList );

    // Get samples and add them to s
    s.mySamples.resize(sampleIdMap.size());
    for( unsigned si=0; si<sampleIdMap.size(); ++si ) {
        auto& sh = this->shdr[sampleIdMap[si]];
        Assert(samples[sh.end]==0);
        SF2Sample& w = s.mySamples[si];
        size_t n = sh.end-sh.start;
        w.resize(n);
        std::transform( samples.begin()+sh.start, samples.begin()+sh.end, w.begin(), [&]( int in ) {
            return in*(1.0f/32767);
        });
        w.complete(false);
        w.mySampleRate = sh.sampleRate;
        w.myOriginalPitch = sh.originalPitch;
        w.myPitchCorrection = sh.pitchCorrection;
        w.myLoopStart = (sh.startLoop-sh.start)*w.unitTime;
        w.myLoopEnd = (sh.endLoop-sh.start)*w.unitTime;
    }

    // Add instruments to s
    s.myInstrumentMap.resize(instrumentList.size());
    for( unsigned ii=0; ii<instrumentList.size(); ++ii ) {
        sampleIdMap.crunch(instrumentList[ii]);
        s.myInstrumentMap[ii].initialize(instrumentList[ii].data(), instrumentList[ii].size());
    }

    // Add presets to s
    instrumentMap.crunch(presetList);
    s.myPresetMap.initialize(presetList.data(), presetList.size());
    return &s;
}