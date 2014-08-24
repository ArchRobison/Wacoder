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
#include "FileSuffix.h"
#include "Midi.h"
#include "Synthesizer.h"
#include "TrackToWaDialog.h"
#include "WaPlot.h"
#include "Wack.h"
#include "Widget.h"

static MidiPlayer TheMidiPlayer;
static std::string TheMidiTuneFileName;  // If empty, then not yet set
static MidiTune TheMidiTune;
TrackToWaDialog TheTrackToWaDialog;
WaPlot TheWaPlot;                               // Not static, because it is referenced in VoiceInput.cpp

#if 0 /* To be deleted */
void InitializeMidi( const char* filename ) {
    TheMidiTune.readFromFile(filename);
}
#endif

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
        TheMidiTuneFileName = filename;
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

static std::string CurrentProjectFileName;

//! Open a Wacoder project file (.wacoder extension).
/** Format of the file is as follows.  Each line has the form [mwt] .*
    The first character denotes the type of line.
    m: .* denotes a midi file.  This line must come first.
    w: .* denotes a wack file.
    t: .* denotes a MIDI track name.  (Use canonical synthetic one if track has no name) */
static void OpenWacoderProject(const std::string& filename) {
    if(FileSuffix(filename)!="wacoder") {
        // Ignore files with incorrect suffix
        return;
    }
    std::ifstream f(filename);
    std::string buf;
    while(getline(f, buf)) {
        if(buf.size()>=3 && buf[1]==' ') {
            const char* path = buf.c_str()+2;
            switch(buf[0]) {
                default:
                    // Corrupted file
                    Assert(0);
                    break;
                case 'm':
                    ReadMidiTune(path);
                    break;
                case 'w':
                    TheTrackToWaDialog.addWaCoder(path);
                    break;
                case 't':
                    TheTrackToWaDialog.addTrack(path);
                    break;
            }
        } else {
            // Shortest possible line has regex 't .'.  So something is wrong.
            Assert(0);  // Corrupt file.
        }
    }
    f.close();
    CurrentProjectFileName = filename;
    for(auto i=TheWackMap.begin(); i!=TheWackMap.end(); ++i)
        CopyWackToWaPlot(TheWaPlot, i->first, *i->second);
}

static void OpenWacoderProject() {
    OpenWacoderProject( HostGetFileName(GetFileNameOp::open, "Wacoder Project", "wacoder") );
}

static void SaveWacoderProject(const std::string& filename) {
    Assert(!TheMidiTuneFileName.empty() );
    std::ofstream f(CurrentProjectFileName);
    f << "m " << TheMidiTuneFileName << std::endl;
    TheTrackToWaDialog.forEach( [&f](bool isWack, const std::string& s ) {
        f << (isWack ? "w " : "t ");
        f << s << std::endl;
    });
    f.close();
}

static void SaveWacoderProject() {
    if( TheMidiTuneFileName.empty() )
        // Nothing to save
        return;
    if( CurrentProjectFileName.empty() ) {
        // No name for this project yet.  Get one from the user.
        std::string s = HostGetFileName(GetFileNameOp::saveAs, "Wacoder Project", "wacoder");
        if(s.empty()) {
            // User cancelled
            return;
        }
        CurrentProjectFileName = s;
    }
    SaveWacoderProject(CurrentProjectFileName);
}


bool GameInitialize() {
    BuiltFromResource::loadAll();
#if 0
    ConstructSounds();
#endif
#if 0
    Menu::constructAll();
#endif
    std::string s = HostGetAssociatedFileName();
    if( FileSuffix(s.c_str())=="wacoder" ) {
        OpenWacoderProject(s);
    }
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
        case 'o'&0x1F:
            OpenWacoderProject();
            break;
        case 's'&0x1F:
            SaveWacoderProject();
            break;;
#if 1 // For development only 
        case 'm':  
		    PlayTune();
            break;
        case 'n':
            std::string buf = ( GetFileNameOp::create, "Wacoder Project", "wacoder" );
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
#if 0
    InitializeMidi("C:/tmp/midi/the_beatles-eleanor_rigby.mid");
#endif
#if 0
    InitializeMidi("C:/tmp/beethoven-ode-to-joy-from-4th-movement-9th-symphony.mid");
#endif
#if 0
    InitializeMidi("C:/tmp/beethoven_ode_to_joy.mid");
#endif

}

void GameDroppedFile(NimblePoint where, const char* filename) {
    if( TheTrackToWaDialog.contains(where)) {
        if( FileSuffix suffix = FileSuffix(filename) ) {
            if( suffix=="mid" ) {
                ReadMidiTune( filename );
            } else if( suffix=="wav" ) {
                std::string name = TheTrackToWaDialog.addWaCoder(filename);
                CopyWackToWaPlot( TheWaPlot, name, *TheWackMap.find(name)->second );
            } else if( suffix=="wacoder" ) {
                OpenWacoderProject(filename); 
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

