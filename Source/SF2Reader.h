#ifndef SF2Reader_H
#define SF2Reader_H

#include <cstdio>
#include <string>

class SF2Bank;

class SF2Reader {
    struct chunkHeader;

    FILE* myFile;
    SF2Bank* myBank;
    void readBytes( char* dst, size_t n );
    template<typename T>
    void read( T& value ) {
        readBytes((char*)&value,sizeof(T));
    }
    template<typename T>
    void readArray( T* value, size_t n ) {
        readBytes((char*)value,sizeof(T)*n);
    }
    void readExpect( const char* tag );

    template<typename T, size_t E>
    void readList( SimpleArray<T,E>& a, const chunkHeader& h );

    void readInfoList();
    void readSdtaList();
    void readPdtaList();
    void skip( size_t n ) {
        fseek(myFile,n,SEEK_CUR);
    }
    void createIndex();
public:
    SF2Reader();
    void open(const std::string& filename);
    void read( SF2Bank& bank );
    void close();
    ~SF2Reader();
};

#endif /*SF2Reader_H*/