#include "Synthesizer.h"
#include <utility>
#include <vector>
#include "Utility.h"
#include "Wack.h"

#define USE_IPP 1

#if USE_IPP
#include "ipp\include\ipps.h"
#endif

using namespace Synthesizer;

typedef std::vector<std::pair<int,int> > WaBounds;

static float AveragePower( const float* a, int n ) {
    Assert(n>0);
    double sum = 0;
    for( int i=0; i<n; ++i )
        sum += Square(a[i]);
    return float(sum/n);
}

static void FindLeadingSlopes( const float* b, int n, bool* c, int stride, float ratio ) {
    float minb = FLT_MAX;
    float maxb = FLT_MIN;
    int d = 0;
    for( int i=0; i<n; ++i, b+=stride, c+=stride ) {
        float x = *b;
        if( x<minb ) 
            minb = x;
        if( x>maxb ) 
            maxb = x;
        switch( d ) {
            case 0:
                if( maxb/minb>=ratio )
                    if( x-minb < (maxb-minb)/2 )
                        d = -1;
                    else 
                        d = 1;
                break;
            case 1:
                if( x<maxb/ratio ) {
                    d = -1;     // Going down
                    minb = x;
                }
                break;
            case -1:
                if( x>minb*ratio ) {
                    d = 1;      // Going up
                    maxb = x;
                }
                break;
        } 
        if( d>0 )
            *c = true;
    }
}

static void SegmentWas( const float* a, int n, WaBounds& bounds ) {
    // Find average power within windows of width 2h. 
    const int h = 44100/16;
    SimpleArray<float> b(n);
    double sum = 0;
    for( int i=-h; i<n+h; ++i ) {
        sum += i+h<n ? Square(a[i+h]) : 0;
        sum -= i-h>=0 ? Square(a[i-h]) : 0;
        if( 0<=i && i<n ) 
            b[i] = float(sum);
    }
    SimpleArray<bool>  c(n);
    c.fill(false);
    float ratio = 4;
    FindLeadingSlopes(b.begin(),n,c.begin(),1,ratio);
    FindLeadingSlopes(b.begin()+n-1,n,c.begin()+n-1,-1,ratio);
#if 0
    FILE* f = fopen("C:/tmp/o.dat","w");
    for( int i=0; i<n; ++i ) {
        if( i%200==0 || (i<n-1 && c[i]!=c[i+1]) )
            fprintf(f,"%d %g %d %g\n", i, log(b[i])/log(2.), c[i], 20*a[i] );
    }
    fclose(f);
#endif
    int startWa = -1;
    for( int i=0; i<=n; ++i ) {
        if( startWa==-1 ) {
            if( i<n && c[i] )
                startWa = i;
        } else {
            if( i==n || !c[i] ) {
                int finishWa = Min(i+h,n);
                float p = AveragePower(a+startWa, finishWa-startWa);
                if( p>=0.0005f ) 
                    bounds.push_back(std::make_pair(startWa,finishWa));
                else {
                    // Reject as noise
                }
                startWa = -1; 
            }
        }
    }
}

static void NormalizeAmplitude( float* a, int n ) {
    float maxa = 0;
    for( int i=0; i<n; ++i )
        if( fabs(a[i])>maxa )
            maxa = fabs(a[i]);
    for( int i=0; i<n; ++i )
        a[i] /= maxa;
}

static void AutoCorrelate( const float src[], size_t srcLen, float dst[], size_t dstLen ) {
#if 0 /* IPP 7.0 */
    IppStatus status = ippsAutoCorr_32f(src, srcLen, dst, dstLen);
    Assert( status==ippStsNoErr );
#else /* IPP 8.0 */
    IppEnum funCfg = IppEnum(ippAlgAuto|ippsNormB);
    int bufSize = 0;
    IppStatus status = ippsAutoCorrNormGetBufferSize(srcLen, dstLen, ipp32f, funCfg, &bufSize);
    Ipp8u *buffer = ippsMalloc_8u( bufSize );
    Assert( status==ippStsNoErr );
    status = ippsAutoCorrNorm_32f(src, srcLen, dst, dstLen, funCfg, buffer);
    Assert( status==ippStsNoErr );
    ippsFree( buffer );
#endif
}

float FrequencyOfWa( const float* a, int n ) {
    int upperRate = int(44100/27.5);
#if USE_IPP
    int lowerRate = 0;
#else
    int lowerRate = int(44100/1760);
#endif
    std::vector<float> ac;
    ac.resize(upperRate+1-lowerRate);
#if USE_IPP
    AutoCorrelate(a, n, &ac[0], ac.size());
#else
    for( int r=lowerRate; r<upperRate; ++r ) {
        double s = 0;
        for( int i=0; i<n-r; ++i )
            s += a[i]*a[i+r];
        ac[r-lowerRate] = s;m
    }
#endif
    int j = 0;
    int m = int(ac.size());
    while( j<m && ac[j]>=0 )
        ++j;

    for( int k=j+1; k<m; ++k )
        if( ac[k]>ac[j] )
            j = k;
    float freq = 44100.f/(j+lowerRate);
    Assert( freq>=20 );
    return freq;
}

Wack::Wack( const std::string& wavFilename ) : myFileName(wavFilename) {
    Waveform w;
    w.readFromFile( wavFilename.c_str() );
    WaBounds waBounds;
    SegmentWas(w.begin(),w.size(),waBounds);
    myArray.resize(waBounds.size());
    for( size_t i=0; i<waBounds.size(); ++i ) {
        auto& wa = myArray[i];
        size_t m = waBounds[i].second-waBounds[i].first;
        float freq = FrequencyOfWa( w.begin()+waBounds[i].first, m );
        wa.waveform.assign( w.begin()+waBounds[i].first, m );
        wa.waveform.complete(/*isCyclic=*/false);
        wa.freq = freq;
        wa.duration = m/44100.f;
        NormalizeAmplitude( wa.waveform.begin(), m );
    }
#if HAVE_WriteWaPlot
    {
        size_t i = wavFilename.find_last_of(".");
        std::string datFileName = wavFilename.substr(0,i) + ".dat";
        writeWaPlot(datFileName.c_str());
    }
#endif
}

static inline float Hypot( float x, float y ) {
    return sqrt(x*x+y*y);
}

const Wa* Wack::lookup( float freq, float duration ) const {
    float bestD = FLT_MAX;
    int bestI = -1;
    for( int i=0; i<int(myArray.size()); ++i ) {
#if 0
        float d = Square(log( WaArray[i].freq/freq ));
#else
        float d = Hypot(log(freq*duration/(myArray[i].duration*myArray[i].freq)),log(freq/myArray[i].freq));
#endif
        if( d<bestD ) {
            bestD = d;
            bestI = i;
        }
    }
    Assert(0<=bestI);
    return &myArray[bestI];
}

#if HAVE_WriteWaPlot
void Wack::writeWaPlot( const char* filename ) const {
    FILE* f = fopen(filename,"w");
    for( int i=0; i<int(myArray.size()); ++i )
        fprintf(f,"%d %g %g\n", i, myArray[i].duration*myArray[i].freq, myArray[i].freq );
    fclose(f);
}
#endif

void WaInstrument::processEvent() {
    switch( event() ) {
        case MEK_NoteOn: {
            MidiTrackReader::futureEvent fe = findNextOffOrOn();
            float desiredPitch = 440*std::pow(1.059463094f,note()-69);
            auto wa = myWaCoder.lookup( desiredPitch, fe.time/myTickPerSec );
            float relativeFreq = desiredPitch/wa->freq;
            SimpleSource* k = SimpleSource::allocate(wa->waveform, relativeFreq);
            Play(k);
            break;
        }
    }
}

void WaInstrument::stop() {
    // No action required.
}

#if HAVE_WriteWaPlot
void WriteWaPlot( const char* filename, const MidiTrack& track, double ticksPerSec ) {
    FILE* f = fopen(filename,"w");
    MidiTrackReader mtr;
    int onCount = 0;
    for( mtr.setTrack(track); mtr.event()!=MEK_EndOfTrack; mtr.next() ) {
        if( mtr.event()==MEK_NoteOn ) {
            MidiTrackReader::futureEvent fe = mtr.findNextOffOrOn();
            float duration = fe.time/ticksPerSec;
            float pitch = mtr.pitch();
            fprintf(f,"%d %g %g\n", onCount++, duration*pitch, pitch ); 
        }
    } 
    fclose(f);
}
#endif