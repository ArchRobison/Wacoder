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
static char TheMidiTuneFileName[HostMaxPath+1];  // If [0] is zero, then not yet set
static MidiTune TheMidiTune;
TrackToWaDialog TheTrackToWaDialog;
WaPlot TheWaPlot;                               // Not static, because it is referenced in VoiceInput.cpp

void InitializeMidi( const char* filename ) {
    TheMidiTune.readFromFile(filename);
}

static void MidiUpdate() {
    TheMidiPlayer.update();
}

static void CopyTuneToWaPlot(WaPlot& plot, const MidiTune& tune) {
    plot.clear();
    float timeScale = 1/tune.tickPerSec();
    for(size_t i=0; i<tune.nTrack(); ++i) {
        MidiTrackReader mtr;
        for(mtr.setTrack(tune[i]); mtr.event()!=MEK_EndOfTrack; mtr.next()) {
            switch(mtr.event()) {
                case MEK_NoteOn: {
                    MidiTrackReader::futureEvent fe = mtr.findNextOffOrOn();
                    plot.insertNote(mtr.pitch(), fe.time*timeScale, mtr.velocity(), i);
                    break;
                }
            }
        }
    }
}

static void CopyWackToWaPlot(WaPlot& plot, std::string name, const Wack& w) {
    auto waId = TheWaPlot.getWackId(name);
    for(size_t j=0; j<w.size(); ++j)
        plot.insertWa(w[j].freq, w[j].duration, waId);
}

static void ReadMidiTune(const char* filename) {
    if( TheMidiTune.readFromFile(filename) ) {
        strcpy( TheMidiTuneFileName, filename );
        TheTrackToWaDialog.clear();
        TheTrackToWaDialog.setFromTune(TheMidiTune);
        CopyTuneToWaPlot(TheWaPlot,TheMidiTune);
        // FIXME - update the  waplot...
    } else {
        // FIXME - HostWarning will exit
        HostWarning(TheMidiTune.readStatus());
    }
}

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

void GameKeyDown( int key ) {
    switch(key) {
        case HOST_KEY_ESCAPE:
            HostExit();
            return;
#if 1 // For development only 
        case 'm':  
		    PlayTune();
            break;
        case 'n':
            char buf[1024];
            HostGetFileName( GetFileNameOp::create, buf, sizeof(buf), "Wacoder Project", "wacoder" );
            static volatile int banana=1;
            banana++;
            break;
    }
#endif
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
        ClickableSetEnd = ClickableSet;
        int x = screen.width()-ThePlayButton.width()-TheRecordButton.width()-InputVolumeMeter.width();
        TheWaPlot.setWindowSize(x,screen.height());
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
    InitializeMidi("C:/tmp/midi/MoonlightSonata.mid");
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

    CopyTuneToWaPlot( TheWaPlot, TheMidiTune );

    for( auto i=TheWackMap.begin(); i!=TheWackMap.end(); ++i ) 
        CopyWackToWaPlot( TheWaPlot, i->first, *i->second );
  
}

//! Get suffix to null-terminated suffix (without .) of the filename.
/** The suffix is converted to lowercase.  Suffixes longer than 7 characters are ignored. */
static bool GetSuffix( char suffix[8], const char* filename ) {
    const char* s = nullptr;
    const char* t = filename;
    for(; *t; ++t)
        if( *t=='.' )
            s = t+1;
    if( s && t-s<=7 ) {
        // Found a suffix, and it's short enough to be interesting.
        char* t = suffix;
        while(*s)
            *t++ = tolower(*s++);
        *t++ = '\0';
        return true;
    } else {
        return false;
    }
}

void GameDroppedFile(NimblePoint where, const char* filename) {
    if( TheTrackToWaDialog.contains(where)) {
        char suffix[8];
        if( GetSuffix(suffix,filename) ) {
            if( strcmp(suffix, "mid")==0 ) {
                ReadMidiTune( filename );
            } else if( strcmp(suffix,"wav")==0 ) {
                std::string name = TheTrackToWaDialog.addWaCoder(filename);
                CopyWackToWaPlot( TheWaPlot, name, *TheWackMap.find(name)->second );
            } else if( strcmp(suffix,"wacoder")==0 ) {
                // FIXME - read the wacoder file after first asking if current project should be saved
            }
        }
#if 0
       if a .wav file (use case-insensitive comparison), treat it as a set of was.  Insert in permutation dialog where dropped.
       if a .mid file (use case-insensitive comparison), treat as new midi to play.  Reassign old was based on closest match to average pitch of track.
       if a .wacoder file, treat as a new track to wa mapping.
#endif
    }
}

void PlayWa( const std::string& wackName, float freq, float duration ) {
    const Wa* wa = TheWackMap[wackName]->lookup(freq,duration);
    Play(Synthesizer::SimpleSource::allocate(wa->waveform));
}

