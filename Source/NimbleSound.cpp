/* Sound support.  It's in a file separate from Host_win.cpp because <dsound.h> generates error
 * if included in Host_win.cpp */ 
#include <math.h>
#include <dsound.h>
#include <MMReg.h>
#include "NimbleSound.h"
#include "AssertLib.h"
#include "Synthesizer.h"

static LPDIRECTSOUND TheDirectSound;
static DSBUFFERDESC DirectSoundBufferDesc;
static LPDIRECTSOUNDBUFFER DirectSoundBuffer;
static WAVEFORMATEX WaveFormat;

#define USE_WAVE_FORMAT_IEEE_FLOAT 1

const size_t N_OutputChannel = 2;
#if USE_WAVE_FORMAT_IEEE_FLOAT 
typedef float NativeSample;
#else
typedef short NativeSample;
#endif
const size_t BytesPerOutputSample = sizeof(NativeSample)*N_OutputChannel;
const size_t SamplesPerBuffer = 8192;
const size_t SamplesPerPlayAheadMargin = 4410;
const DWORD BytesPerOutputBuffer = BytesPerOutputSample*SamplesPerBuffer;
const DWORD MilliSecPerInterrupt = 10;

typedef float Accumulator[N_OutputChannel][SamplesPerBuffer];
typedef float (*AccumulatorPtr)[SamplesPerBuffer];

#pragma warning( disable : 4146 ) // "unary minus operator applied to unsigned type, result still unsigned"

static volatile long IsRunning;

static const char* DecodeDirectSoundError( HRESULT x ) {
	switch( x ) {
	    case DSERR_ALLOCATED:
		    return "DSERR_ALLOCATED";
	    case DSERR_NODRIVER:
			return "DSERR_NODRIVER";
		case DSERR_INVALIDPARAM:
			return "DSERR_INVALIDPARAM";
		case DSERR_OUTOFMEMORY:
			return "DSERR_OUTOFMEMORY";
		default:
			return NULL;
	}
}

//! Convert floating-point samples in src[0:N_Channel][0:n] to 16-bit samples in dst[0:n]
static void ConvertAccumulatorToSamples( NativeSample dst[], Accumulator src, int n ) {
    NativeSample*d = dst;
    for( int i=0; i<n; ++i ) 
        for( int j=0; j<N_OutputChannel; ++j ) {
#if USE_WAVE_FORMAT_IEEE_FLOAT
            *d++ = src[j][i];
#else
            *d++ = short(0x4000*src[j][i]);
#endif
            src[j][i] = 0;
        }
}

static void DoSoundOutput() {
    static DWORD begin;
    DWORD playCursor;
    DWORD writeCursor;
    Assert(TheDirectSound);
    HRESULT status = DirectSoundBuffer->GetCurrentPosition( &playCursor, &writeCursor );
    DWORD end = (playCursor+SamplesPerPlayAheadMargin*BytesPerOutputSample) % BytesPerOutputBuffer;
    if( end!=begin ) {
        int n = (end+BytesPerOutputBuffer-begin)%BytesPerOutputBuffer/BytesPerOutputSample;
        Assert( n<=SamplesPerBuffer );
        void *ptr1, *ptr2;
        DWORD size1, size2;
        HRESULT status = DirectSoundBuffer->Lock(begin,n*BytesPerOutputSample,&ptr1,&size1,&ptr2,&size2,0);
        if( status==DS_OK ) {
            static Accumulator temp;
            Synthesizer::OutputInterruptHandler( temp[0], temp[1], n );
#if 0
            for( WaveStream* w=TheWaveStreams; w<TheWaveStreams+N_WaveStream; ++w ) 
                w->addTo(temp[w->channel()],n);
#endif
            ConvertAccumulatorToSamples( (NativeSample*)ptr1, temp, size1/BytesPerOutputSample );
            if( ptr2 )
                ConvertAccumulatorToSamples( (NativeSample*)ptr2, (AccumulatorPtr)&temp[0][size1/BytesPerOutputSample], size2/BytesPerOutputSample );
            DirectSoundBuffer->Unlock(ptr1,size1,ptr2,size2);
            begin = end;
        } else {
            Assert(false);
        }
    }
}

static bool InitializeDirectSoundOutput( HWND hwnd ) {
	HRESULT status = DirectSoundCreate(NULL,&TheDirectSound,NULL);
	if( FAILED(status) ) {
		DecodeDirectSoundError(status);
		return false;
	}
	status = IDirectSound_SetCooperativeLevel(TheDirectSound, hwnd, DSSCL_PRIORITY); 
	if( FAILED(status) )
		return false;

	WAVEFORMATEX& w = WaveFormat;
	ZeroMemory(&w,sizeof(WaveFormat));
#if USE_WAVE_FORMAT_IEEE_FLOAT
    w.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    w.wBitsPerSample = 32;
#else
	w.wFormatTag = WAVE_FORMAT_PCM;
	w.wBitsPerSample = 16;
#endif
	w.nChannels = N_OutputChannel;
	w.nSamplesPerSec = NimbleSoundSamplesPerSec;
	w.nBlockAlign = w.nChannels*w.wBitsPerSample/8;
	w.nAvgBytesPerSec = w.nSamplesPerSec*w.nBlockAlign;

    DSBUFFERDESC& d = DirectSoundBufferDesc;
	ZeroMemory(&d,sizeof(d));
	d.dwSize = sizeof(d);
	d.dwBufferBytes = BytesPerOutputBuffer;
	d.lpwfxFormat = &w;
	if( FAILED( TheDirectSound->CreateSoundBuffer(&d,&DirectSoundBuffer,NULL) ) )
		return false;

	status = DirectSoundBuffer->Play(0,0,/*dwFlags=*/DSBPLAY_LOOPING);

 	return true;
}

#if HAVE_SOUND_INPUT
const size_t N_InputChannel = 1;
const size_t BytesPerInputSample = sizeof(NativeSample)*N_InputChannel;
const DWORD BytesPerInputBuffer = BytesPerInputSample*SamplesPerBuffer;

static LPDIRECTSOUNDCAPTURE TheDirectCapture;
static DSCBUFFERDESC DirectCaptureBufferDesc;
static LPDIRECTSOUNDCAPTUREBUFFER DirectCaptureBuffer;

static void DoSoundInput() {
    static DWORD cur;
    DWORD readCursor;
    Assert(TheDirectCapture);
    HRESULT status = DirectCaptureBuffer->GetCurrentPosition( NULL, &readCursor );
    if( status )
        return;
    long lockSize = readCursor - cur;   
    if( lockSize<0 )   
        lockSize += BytesPerInputBuffer;    
    Assert( lockSize>=0 );
    if( !lockSize )   
        return;  
    if(1) {
        void *ptr1, *ptr2;
        DWORD size1, size2;
        HRESULT status = DirectCaptureBuffer->Lock(cur,lockSize,&ptr1,&size1,&ptr2,&size2,0);
        if( status==DS_OK ) {
            static SoundInjector si;
            si.inject( (NativeSample*)ptr1, size1/BytesPerInputSample );
            Assert( size1%BytesPerInputSample==0 );
            Synthesizer::InputInterruptHandler( (NativeSample*)ptr1, size1/BytesPerInputSample );
            if( ptr2 ) {
                Assert( size2%BytesPerInputSample==0 );
                si.inject( (NativeSample*)ptr2, size2/BytesPerInputSample );
                Synthesizer::InputInterruptHandler( (NativeSample*)ptr2, size2/BytesPerInputSample );
            }
            DirectCaptureBuffer->Unlock(ptr1,size1,ptr2,size2);
            cur += lockSize;  
            if( cur>=BytesPerInputBuffer )
                cur -= BytesPerInputBuffer;
        } else {
            Assert(false);
        }
    }
}

static bool InitializeDirectSoundInput( HWND hwnd ) {
	HRESULT status = DirectSoundCaptureCreate(NULL,&TheDirectCapture,NULL);
	if( FAILED(status) ) {
		DecodeDirectSoundError(status);
		return false;
	}
    
    WAVEFORMATEX& w = WaveFormat;
	ZeroMemory(&w,sizeof(WaveFormat));
#if USE_WAVE_FORMAT_IEEE_FLOAT
    w.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    w.wBitsPerSample = 32;
#else
	w.wFormatTag = WAVE_FORMAT_PCM;
	w.wBitsPerSample = 16;
#endif
	w.nChannels = 1; // Monophonic
	w.nSamplesPerSec = NimbleSoundSamplesPerSec;
	w.nBlockAlign = w.nChannels*w.wBitsPerSample/8;
	w.nAvgBytesPerSec = w.nSamplesPerSec*w.nBlockAlign;

    DSCBUFFERDESC& d = DirectCaptureBufferDesc;
	ZeroMemory(&d,sizeof(d));
	d.dwSize = sizeof(d);
    d.dwFlags = DSCBCAPS_WAVEMAPPED;
    d.dwBufferBytes = BytesPerInputBuffer;
    d.lpwfxFormat = &w;
    status =  TheDirectCapture->CreateCaptureBuffer(&DirectCaptureBufferDesc, &DirectCaptureBuffer, NULL);
	if( FAILED(status) ) {
		DecodeDirectSoundError(status);
		return false;
	}

    status = DirectCaptureBuffer->Start(DSCBSTART_LOOPING);
    if( FAILED(status) ) {
        DecodeDirectSoundError(status);
        return false;
    }

    return true;
}
#endif /* HAVE_SOUND_INPUT */

static void CALLBACK TimerHandler(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
	if (!InterlockedExchange (&IsRunning, 1)) {
        DoSoundOutput();
#if HAVE_SOUND_INPUT
        DoSoundInput();
#endif /* HAVE_SOUND_INPUT */
		InterlockedExchange(&IsRunning, 0 );
	}
}

/** Called from Host_win.cpp */
bool InitializeDirectSound( HWND hwnd ) {
    Synthesizer::Initialize();
    if( !InitializeDirectSoundOutput(hwnd) ) 
        return false;
#if HAVE_SOUND_INPUT
    if( !InitializeDirectSoundInput(hwnd) )
        return false;
#endif    
    LPTIMECALLBACK callback = &TimerHandler;
    MMRESULT mmresult = timeSetEvent(/*msecDelay=*/MilliSecPerInterrupt, /*msecResolution=*/0, callback, 0, TIME_PERIODIC|TIME_CALLBACK_FUNCTION);
    return mmresult!=NULL;
}

/** Called from Host_win.cpp */
void ShutdownDirectSound() {
	if( TheDirectSound ) {
		if (!InterlockedExchange (&IsRunning, 1)) 
			Sleep(1);
		IDirectSound_Release(TheDirectSound);
		TheDirectSound = NULL;
	}
}