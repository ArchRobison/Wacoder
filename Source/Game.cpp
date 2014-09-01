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
#include "MidiPlayer.h"
#include "Synthesizer.h"
#include "ChannelToWaDialog.h"
#include "WaPlot.h"
#include "WaSet.h"
#include "Widget.h"

#define GAME_LOG 0
#if GAME_LOG
static std::ofstream GameLog("C:\\tmp\\gamelog.txt");
#endif

static Midi::Player TheMidiPlayer;
static std::string TheMidiTuneFileName;  // If empty, then not yet set
static Midi::Tune TheMidiTune;
ChannelToWaDialog TheChannelToWaDialog;
WaPlot TheWaPlot;                               // Not static, because it is referenced in VoiceInput.cpp

#if 0 /* To be deleted */
void InitializeMidi( const char* filename ) {
    TheMidiTune.readFromFile(filename);
}
#endif

static void MidiUpdate() {
    TheMidiPlayer.update();
}

static void CopyTuneToWaPlot(WaPlot& plot, const Midi::Tune& tune) {
    plot.clear();
    tune.forEachNote( [&]( const Midi::Event& on, const Midi::Event& off ) {
        Assert(on.channel()==off.channel());
        plot.insertNote(Midi::PitchOfNote(on.note()), (off.time()-on.time())*Midi::SecondsPerTock, on.velocity(), on.channel());
    });
}

static void CopyWaSetToWaPlot(WaPlot& plot, std::string name, const WaSet& w) {
    auto waId = TheWaPlot.getWaSetId(name);
    w.forEach([&](const Wa& w) {
        plot.insertWa(w.freq,w.duration, waId);
    });
}

static void ReadMidiTune(const char* filename) {
    // Make sure player is stopped.
    TheMidiPlayer.stop();
    if( TheMidiTune.readFromFile(filename) ) {
        TheMidiTuneFileName = filename;
        TheChannelToWaDialog.clear();
        TheChannelToWaDialog.setFromTune(TheMidiTune);
        CopyTuneToWaPlot(TheWaPlot,TheMidiTune);
    } else {
        // FIXME - HostWarning will exit
        HostWarning(TheMidiTune.readStatus().c_str());
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
    w: .* denotes a waSet file.
    t: .* denotes a MIDI track name.  (Use canonical synthetic one if track has no name) */
static void OpenWacoderProject(const std::string& filename) {
#if GAME_LOG
    GameLog << "enter OpenWacoderProject(" << filename << ")\n" << std::flush;
#endif
    if(FileSuffix(filename)!="wacoder") {
        // Ignore files with incorrect suffix
        return;
    }
    std::ifstream f(filename);
    std::string buf;
    while(getline(f, buf)) {
#if GAME_LOG
        GameLog << "getline returned:" << buf << "\n" << std::flush; 
#endif
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
                    TheWaSetCollection.addWaSet(TheChannelToWaDialog.addWaSet(path), path);
                    break;
                case 'c': {
                    unsigned channel = TheMidiTune.channels().findByName(path);
                    if( channel<TheMidiTune.channels().size() )
                        TheChannelToWaDialog.addChannel(path, channel);
                    break;
                }
            }
        } else {
            // Shortest possible line has regex 't .'.  So something is wrong.
            Assert(0);  // Corrupt file.
        }
    }
    f.close();
#if GAME_LOG
    GameLog << "closed file " << filename << "\n" << std::flush;
#endif
    CurrentProjectFileName = filename;
    TheWaSetCollection.forEach( [&]( const std::string& name, const WaSet& w ) {
        CopyWaSetToWaPlot(TheWaPlot, name, w);
    });
#if GAME_LOG
    GameLog << "leave OpenWacoderProject(" << filename << ")\n" << std::flush;
#endif
}

static void OpenWacoderProject() {
    OpenWacoderProject( HostGetFileName(GetFileNameOp::open, "Wacoder Project", "wacoder") );
}

static void SaveWacoderProject(const std::string& filename) {
    Assert(!TheMidiTuneFileName.empty() );
    std::ofstream f(CurrentProjectFileName);
    f << "m " << TheMidiTuneFileName << std::endl;
    TheChannelToWaDialog.forEach( [&f](bool isWaSet, const std::string& s ) {
        f << (isWaSet ? "w " : "c ");
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
    if( TheMidiTune.empty() ) return;
    TheMidiPlayer.preparePlay(TheMidiTune);
    TheChannelToWaDialog.setupMidiPlayer(TheMidiPlayer);
	TheMidiPlayer.commencePlay();
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
        case 'n': {
            std::string buf = ( GetFileNameOp::create, "Wacoder Project", "wacoder" );
            static volatile int banana=1;
            banana++;
            break;
        }
#endif
#if ASSERTIONS && 0
        case 'd':
            for( unsigned i=0; i<TheMidiTune.channels.size(); ++i ) {
                char name[64];
                sprintf(name,"/tmp/tmp/track_%u",i+1);
                DumpTrack(name,TheMidiTune[i]);
            }
            break;
#endif
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
        ClickableSetEnd = ClickableSet;
        int x = screen.width()-ThePlayButton.width()-TheRecordButton.width()-InputVolumeMeter.width();
        TheWaPlot.setWindowSize(x,screen.height());
        TheWaPlot.setTrackHues(TheChannelToWaDialog.hilightedTracks());
        TheWaPlot.setWaSetHues( HueMap( []( Hue h ) -> int {
            if( auto* s = TheChannelToWaDialog.hilightedWaSet(h) ) 
                return TheWaPlot.getWaSetId(*s);
            else
                return -1;
        }));
        DrawClickable( TheWaPlot, screen, 0, 0 );
        DrawClickable( TheChannelToWaDialog, screen, x, InputVolumeMeter.height() );
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


}

void GameDroppedFile(NimblePoint where, const char* filename) {
    if( TheChannelToWaDialog.contains(where)) {
        if( FileSuffix suffix = FileSuffix(filename) ) {
            if( suffix=="mid" ) {
                // Read MIDI file
                ReadMidiTune( filename );
            } else if( suffix=="wav" ) {
                // Read a WaSet
                std::string name = TheChannelToWaDialog.addWaSet(filename);
                const WaSet*w = TheWaSetCollection.addWaSet(name, filename);
                CopyWaSetToWaPlot( TheWaPlot, name, *w );
            } else if( suffix=="wacoder" ) {
                OpenWacoderProject(filename); 
            }
        }
    }
}

void PlayWa( const std::string& waSetName, float freq, float duration ) {
    const Wa* wa = TheWaSetCollection.find(waSetName)->lookup(freq, duration);
    Play(Synthesizer::SimpleSource::allocate(wa->waveform));
}