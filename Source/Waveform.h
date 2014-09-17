#ifndef Waveform_H
#define Waveform_H

#include "Utility.h"

namespace Synthesizer {

static const size_t SampleRate = 44100;

template<typename T, int Shift> 
class SampledSignalBase: public SimpleArray<T,1> {
public:
    typedef T sampleType;
    typedef unsigned timeType;	                            // Unsigned type       
    static const int timeShift = Shift;
    static const unsigned unitTime = (1<<timeShift);
    timeType limit() const {return size()<<timeShift;}
    // Deprecate?
    float interpolate( const T* w, timeType t ) const {
        size_t i = t>>timeShift;
        Assert( w+i<end() );
        sampleType s0 = w[i];
        sampleType s1 = w[i+1];
        float f = (t & unitTime-1)*(1.0f/unitTime);
        return s0+(s1-s0)*f;
    }
    // Compute n samples and accumulate them into output. Returns t0+n*dt.
    timeType resample( T* output, timeType t, timeType dt, size_t n ) const;
};

template<typename T, int Shift>
auto SampledSignalBase<T,Shift>::resample( T* output, timeType t, timeType dt, size_t n ) const -> timeType {
    Assert( t < size()<<timeShift );
    Assert( t+(n-1)*dt < size()<<timeShift );
    const T* w = begin();
    for( ; n>0; t+=dt, --n ) {
        // FIXME - use FIR filter bank to interpolate more accurately? 
        size_t i = t>>timeShift;
        sampleType s0 = w[i];
        sampleType s1 = w[i+1];
        float f = (t & unitTime-1)*(1.0f/unitTime);
        // FIXME - redo algebra to shorten dependence chain or exploit FMA?
        *output++ = (s0+(s1-s0)*f);
    }
    return t;
}

class Waveform: public SampledSignalBase<float,12> {
public:
    Waveform() : myIsCyclic(2) {}
    Waveform( size_t n ) : myIsCyclic(2) {resize(n);}

    ~Waveform() {}
    //! True if waveform is cyclic.
    bool isCyclic() const {
        Assert(isCompleted());
        return myIsCyclic!=0;
    }
    void complete( bool cyclic=true ) {
        myIsCyclic = cyclic;
        *end() = cyclic ? *begin() : 0;
    }
    bool isCompleted() const {return myIsCyclic<2;}
    //! Read from a ".wav" file
    void readFromFile( const char* filename );
    //! Write to a ".wav" file
    void writeToFile( const char* filename );
    //! Read from a ".wav" file in memory
    void readFromMemory( const char* data, size_t size );
private:
    class inputType;
    void readFromInput( inputType& f );
    /** 0-> non-cyclic waveform (conceptually tailed by zeros).
        1-> cyclic waveform 
        2-> method complete has not been called yet */
    char myIsCyclic;
};

} // namespace Synthesizer

#endif /* WaveForm_H */