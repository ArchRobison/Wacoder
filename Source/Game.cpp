#define _CRT_SECURE_NO_WARNINGS 1
#include "NimbleSound.h"
#include "Host.h"
#include "Game.h"
#include "Clickable.h"
#include "HorizontalBarMeter.h"
#include <cmath>
#include <stdlib.h>
#include <algorithm>
#include <vector>
#include <cstring>
#include <fstream>
#include "Midi.h"
#include "Synthesizer.h"
#include "TrackToWaDialog.h"
#include "WaPlot.h"
#include "Wack.h"
#include "Widget.h"

static MidiPlayer TheMidiPlayer;
static MidiTune TheMidiTune;
TrackToWaDialog TheTrackToWaDialog;

void InitializeMidi( const char* filename ) {
    TheMidiTune.readFromFile(filename);
}

static void MidiUpdate() {
    TheMidiPlayer.update();
}

WaPlot TheWaPlot;

const char* GameTitle() {
#if ASSERTIONS
    return "Wacoder [ASSERTIONS]";
#else
    return "Wacoder";
#endif
}

bool GameInitialize() {
    BuiltFromResource::loadAll();
#if 0
    ConstructSounds();
#endif
#if 0
    Menu::constructAll();
#endif
    return true;
}

static void PlayTune() {
	NameToWackMap trackMap;
	TheTrackToWaDialog.loadTrackMap(trackMap); 
	TheMidiPlayer.play(TheMidiTune,trackMap);
}

class PlayButton: public ToggleButton {
public:
    PlayButton() : ToggleButton("Play") {}
    void onPress() {
        // Play selected Midi tune, track, or wa.
        setIsOn( !isOn() );
        if( isOn() ) {
			PlayTune();
        } else {
            TheMidiPlayer.stop();
        }
    }
} static ThePlayButton;

class RecordButton: public ToggleButton {
public:
    RecordButton() : ToggleButton("Record") {}
    void onPress(/*FIXME*/) {
        // Record was
        setIsOn( !isOn() );
    }
} static TheRecordButton;

static HorizontalBarMeter InputVolumeMeter("Volume");

static const int ClickableSetMaxSize = 4;
static Clickable* ClickableSet[ClickableSetMaxSize]; 
static Clickable** ClickableSetEnd = ClickableSet;

void DrawClickable( Clickable& clickable, NimblePixMap& map, int x, int y ) {
	Assert( ClickableSetEnd<&ClickableSet[ClickableSetMaxSize]);
    *ClickableSetEnd++ = &clickable;
	clickable.drawOn(map,x,y);
}

void GameMouseButtonDown( const NimblePoint& point, int k ) {
	switch(k) {
		case 0:
			for( Clickable** c=ClickableSetEnd; --c>=ClickableSet;  ) 
				if( (*c)->mouseDown(point) )
					return;
			break;
	}
}

void GameMouseButtonUp( const NimblePoint& point, int k ) {
    for( Clickable** c=ClickableSet; c!=ClickableSetEnd; ++c ) 
	    (*c)->mouseUp(point);
}

void GameMouseMove( const NimblePoint& point ) {
	for( Clickable** c=ClickableSet; c!=ClickableSetEnd; ++c )
		(*c)->mouseMove(point);
}

bool BananaFlag;

void GameKeyDown( int key ) {
    if( key==HOST_KEY_ESCAPE ) {
        HostExit();
        return;
    }
    if( key=='m' ) {
		PlayTune();
	}
	if( key==',' ) {
		BananaFlag = 1;
    }
}

unsigned Counter;

void GameUpdateDraw( NimblePixMap& screen, NimbleRequest request ) {
    if( request &  NimbleUpdate ) { 
        ++Counter;
        MidiUpdate();
        extern void VoiceUpdate();
        VoiceUpdate();
#if 0
        if( Voice.isActive() ) {
            if( Counter%30==0 ) {
                ChangeVolume(4.0f);
            }
            if( Counter%30==15 ) {
                ChangeVolume(.25f);
            }
        }
#endif
    }
    if( request & NimbleDraw ) {
		extern bool BananaFlag;
        ClickableSetEnd = ClickableSet;
        int x = screen.width()-ThePlayButton.width()-TheRecordButton.width()-InputVolumeMeter.width();
        TheWaPlot.setWindowSize(x,screen.height());
		if( BananaFlag ) {
			BananaFlag = 2;
		}
        TheWaPlot.setTrackHues(TheTrackToWaDialog.hilightedTracks());
        TheWaPlot.setWackHues( HueMap( []( Hue h ) -> int {
            if( auto* s = TheTrackToWaDialog.hilightedWack(h) ) 
                return TheWaPlot.getWackId(*s);
            else
                return -1;
        }));
        DrawClickable( TheWaPlot, screen, 0, 0 );
        DrawClickable( TheTrackToWaDialog, screen, x, InputVolumeMeter.height() );
        extern float VoicePeak;
         InputVolumeMeter.setLevel(Min(1.0f,VoicePeak));
        InputVolumeMeter.drawOn( screen, x, 0 );
        x += InputVolumeMeter.width();
        DrawClickable( ThePlayButton, screen, x, 0 );
        x += ThePlayButton.width();
        DrawClickable( TheRecordButton, screen, x, 0 );
        x += TheRecordButton.width();
#if 0
        screen.draw(NimbleRect(0,0,screen.width(),screen.height()),0);
        extern float VoicePitch;
        float minPitch = 440.f/8;
        float maxPitch = 440.f*4;
        float pitch = Clip(minPitch,maxPitch,VoicePitch);
        float frac = log(pitch/minPitch)/log(maxPitch/minPitch);
        screen.draw(NimbleRect(0,(1.0f-frac)*screen.height(),screen.width(),screen.height()),~0);
#endif
    }

}

void GameResizeOrMove( NimblePixMap& map ) {
#if 0
    ReadWav("C:/tmp/High Score.wav");
#endif
#if 0
    InitializeMidi("C:/tmp/chromaticescale.mid");m
#endif
#if 0
    InitializeMidi("C:/tmp/tfdm.mid");
#endif
#if 0
    InitializeMidi("C:/tmp/SimpleScale.mid");
#endif
#if 0
    InitializeMidi("C:/tmp/MoonlightSonata.mid");
#endif
#if 1
    InitializeMidi("C:/tmp/midi/the_beatles-eleanor_rigby.mid");
#endif
#if 0
    InitializeMidi("C:/tmp/beethoven-ode-to-joy-from-4th-movement-9th-symphony.mid");
#endif
#if 0
    InitializeMidi("C:/tmp/beethoven_ode_to_joy.mid");
#endif

    TheTrackToWaDialog.setFromTune(TheMidiTune);
    TheTrackToWaDialog.readFromFile("C:/tmp/francie.txt");
	TheTrackToWaDialog.loadWaCoders();

    float timeScale = 1/TheMidiTune.tickPerSec();
    for( size_t i=0; i<TheMidiTune.nTrack(); ++i ) {
        MidiTrackReader mtr;
        for( mtr.setTrack(TheMidiTune[i]); mtr.event()!=MEK_EndOfTrack; mtr.next() ) {
            switch( mtr.event() ) {
                case MEK_NoteOn: {
                    MidiTrackReader::futureEvent fe = mtr.findNextOffOrOn();
                    TheWaPlot.insertNote( mtr.pitch(), fe.time*timeScale, mtr.velocity(), i );
                    break;
                }
            }
        }
    }
    for( auto i=TheWackMap.begin(); i!=TheWackMap.end(); ++i ) {
        const Wack& w = *i->second;
		auto waId = TheWaPlot.getWackId(i->first);
        for( size_t j=0; j<w.size(); ++j )
            TheWaPlot.insertWa( w[j].freq, w[j].duration, waId );
    }
}

void PlayWa( const std::string& wackName, float freq, float duration ) {
    const Wa* wa = TheWackMap[wackName]->lookup(freq,duration);
    Play(Synthesizer::SimpleSource::allocate(wa->waveform));
}

