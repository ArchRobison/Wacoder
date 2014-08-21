#include "Synthesizer.h"
#include "NonblockingQueue.h"
#include "WaPlot.h"
#include "Host.h"
#include <cstdio>

const size_t VoiceChunkSize = 2048;

using namespace Synthesizer;

struct VoiceBuf {
    Waveform::sampleType chunk[VoiceChunkSize];
};

static NonblockingQueue<VoiceBuf> TheVoiceQueue(3);

namespace Synthesizer {

void InputInterruptHandler( const Waveform::sampleType* wave, size_t n ) {
    static VoiceBuf* b;  
    static size_t bend;
    static SoundInjector sj;
    sj.check(wave,n);
    while( n>0 ) {
        if( !b ) {
            b = TheVoiceQueue.startPush();
            if( !b ) 
                return;
        }
        size_t m = Min(n,VoiceChunkSize-bend);
        memcpy( b->chunk+bend, wave, m*sizeof(Waveform::sampleType) );
        static SoundInjector si;
        si.check( b->chunk+bend, m );
        wave += m;
        n -= m;
        bend += m;
        Assert( bend<=VoiceChunkSize );
        if( bend==VoiceChunkSize ) {
            TheVoiceQueue.finishPush();
            b = NULL;
            bend = 0;
        }
    }
}

} // namespace Synthesizer

using namespace Synthesizer;

float VoicePitch;
float VoicePeak;

// Return absolute peak amplitude of a[0:n)
static float PeakAmplitude( const float a[], int n ) {
    float b = 0;
    for( int i=0; i<n; ++i ) {
        float x = fabs(a[i]);
        if( x>b ) b=x;
    }
    return b;
}

void VoiceUpdate() {
    while( VoiceBuf* b = TheVoiceQueue.startPop() ) {
        const int n = 2*VoiceChunkSize;
        static float window[n];
        memcpy( window, window+VoiceChunkSize, VoiceChunkSize*sizeof(float) );
        memcpy( window+VoiceChunkSize, b->chunk, VoiceChunkSize*sizeof(float) );
        extern float FrequencyOfWa( const float* a, int n );
        VoicePitch = FrequencyOfWa( window, VoiceChunkSize );
        VoicePeak = PeakAmplitude( window, VoiceChunkSize );
        static double waStart;
        static bool waRecording;
        extern WaPlot TheWaPlot;
        if( 1 ) {
            float th1=0.1f;
            float th2=0.1f;
            if( !waRecording ) {
                if( VoicePeak>=th1 ) {
                    waStart = HostClockTime();
                    waRecording = true;
                }
            } else {
                if( VoicePeak<=th2 ) {
                    TheWaPlot.setDynamicWa(0,0);
                    waRecording = false;
                } else {
                    TheWaPlot.setDynamicWa(VoicePitch,HostClockTime()-waStart);
                }
            }
        }
        TheVoiceQueue.finishPop();
    }
}