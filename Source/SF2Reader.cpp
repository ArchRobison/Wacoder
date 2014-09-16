#include <cstdio>
#include <cstdint>
#include <string>
#include "ReadError.h"
#include "AssertLib.h"
#include "Utility.h"
#include "SF2Bank.h"
#include "SF2Reader.h"

enum class ChunkId {    // Must be in alphbetical order
    LIST,
    RIFF,
    ibag,
    igen,
    imod,
    inst,
    pbag,
    pgen,
    phdr,
    pmod,
    shdr,
    smpl,
    unknown             // Must be last
};

const char ChunkIdName[] =   // Must be in same order as ChunkId.  No commas.
    "LIST"
    "RIFF"
    "ibag"
    "igen"
    "imod"
    "inst"
    "pbag"
    "pgen"
    "phdr"
    "pmod"
    "shdr"
    "smpl"
;

struct SF2Reader::chunkHeader {
    char chunkId[4];
    uint32_t chunkSize;
    ChunkId id() const;
};

ChunkId SF2Reader::chunkHeader::id() const {
    for( int i=0; i<int(ChunkId::unknown); ++i )
        if( memcmp(ChunkIdName+4*i,chunkId,4)==0 )
            return ChunkId(i);
    return ChunkId::unknown;
};

SF2Reader::SF2Reader() : myFile(nullptr), myBank(nullptr) {
}

SF2Reader::~SF2Reader() {
    if(myFile) 
        close();
}

void SF2Reader::close() {
    std::fclose(myFile);
    myFile = nullptr;
}

void SF2Reader::open(const std::string& filename) {
    myFile = fopen(filename.c_str(),"rb");
    if(!myFile) 
        throw ReadError("cannot open "+filename+": "+strerror(errno));
}

void SF2Reader::readBytes( char* dst, size_t n ) {
    if(fread(dst, 1, n, myFile)!=n)
        throw ReadError("read failed");
}

void SF2Reader::readExpect(const char* tag) {
    Assert(std::strlen(tag)==4);
    char tmp[4];
    readBytes(tmp, 4);
    if(memcmp(tmp, tag, 4)!=0)
        throw ReadError(std::string("expected ")+tag);
}

template<typename T, size_t E>
void SF2Reader::readList(SimpleArray<T, E>& a, const chunkHeader& h) {
    size_t n = h.chunkSize;
    if(n%sizeof(T)!=0)
        throw ReadError("list "+std::string(h.chunkId, 4)+" not a multiple of record size");
    Assert(n/sizeof(T)>=E);
    a.resize(n/sizeof(T)-E);
    readArray(a.begin(), a.size()+E);
}

void SF2Reader::readInfoList() {
    chunkHeader infoList;
    read(infoList);
    if( infoList.id()!=ChunkId::LIST )
        throw ReadError("LIST chunk expected");
    char infoHeader[4];
    readArray(infoHeader,4);
    size_t n = infoList.chunkSize-4;
    while(n>0) {
        chunkHeader itemHeader;
        read(itemHeader);
        skip(itemHeader.chunkSize); 
        n -= 8 + itemHeader.chunkSize;
    }
}

void SF2Reader::readSdtaList() {
    chunkHeader listHeader;
    read(listHeader);
    if( listHeader.id()!=ChunkId::LIST )
        throw ReadError("sdta must be LIST");
    if( listHeader.chunkSize<=4 )
        throw ReadError("sdta too short");
    readExpect("sdta");
    chunkHeader smplHeader;
    read(smplHeader);
    if( smplHeader.id()!=ChunkId::smpl )
        throw ReadError("smpl subchunk expected");
    myBank->samples.resize(smplHeader.chunkSize/2);
    readArray(myBank->samples.begin(),myBank->samples.size());
    if( size_t m = listHeader.chunkSize-4-8-smplHeader.chunkSize ) {
        // FIXME - use 24-bit data
        skip(m);
    }
}

void SF2Reader::readPdtaList() {
    chunkHeader pdtaList;
    read(pdtaList);
    char pdtaHeader[4];
    readArray(pdtaHeader,4);
    size_t n = pdtaList.chunkSize-4;
    Assert(sizeof(SF2Bank::Rec_bag)==4);
    Assert(sizeof(SF2Bank::Rec_gen)==4);
    Assert(sizeof(SF2Bank::Rec_mod)==10);
    while(n>0) {
        if( n<sizeof(chunkHeader) )
            throw ReadError("pdta-list corrupted");
        chunkHeader item;
        read(item);
        switch(item.id()) {
            case ChunkId::phdr:
                readList(myBank->phdr,item);
                break;
            case ChunkId::pbag:
                readList(myBank->p.bag,item);
                break;
            case ChunkId::pmod:
                readList(myBank->p.mod,item);
                break;
            case ChunkId::pgen:
                readList(myBank->p.gen,item);
                break;     
            case ChunkId::inst:
                readList(myBank->inst,item);
                break;
            case ChunkId::ibag:
                readList(myBank->i.bag,item);
                break;
            case ChunkId::imod:
                readList(myBank->i.mod,item);
                break;
            case ChunkId::igen:
                readList(myBank->i.gen,item);
                break;            
            case ChunkId::shdr: 
                readList(myBank->shdr,item);
                break;
            default:
                Assert(0);
                skip(item.chunkSize);
                break;
        }
        n -= 8 + item.chunkSize;
    }
}

void SF2Reader::read( SF2Bank& bank ) {
    myBank = &bank;
    chunkHeader riffHeader;
    read(riffHeader);
    readExpect("sfbk");
    readInfoList();
    readSdtaList();
    readPdtaList();
    myBank->createIndex();
    myBank = nullptr;
}

#if 0
void SF2Parse() {
    SF2Reader sr;
    sr.open("C:\\tmp\\midi\\GeneralUser GS FluidSynth v1.44.sf2");
    SF2Bank b;
    sr.read(b);
    b.dump("C:\\tmp\\tmp\\sf2.txt");
}
#endif