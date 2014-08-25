#include "TrackToWaDialog.h"
#include "Midi.h"
#include <fstream>

int TrackToWaDialog::hilightedTrack( Hue h ) const {
    int i = hilighted(h);
    if( i>=0 ) {
        Assert( unsigned(i)<=size() );
        auto& t = myItems[i];
        if( !t.isWaSet )
            return t.index;

    }
    return -1;
}

const std::string* TrackToWaDialog::hilightedWaSet( Hue h ) const {
    int i = hilighted(h);
    if( i>=0 ) {
        Assert( unsigned(i)<=size() );
        auto& t = myItems[i];
        if( t.isWaSet )
            return &t.name;
    }
    return NULL;
}

//! Return s with all leading and trailing whitespace removed
static std::string TrimWhiteSpace( const std::string& s ) {
    size_t i = 0;
    while( i<s.size() && isspace(s[i]) )
        ++i;
    size_t j=s.size();
    while( i<j && isspace(s[j-1]) )
        --j;
    return std::string(s,i,j-i);
}

// Return filename with any drive prefix, directory prefix, or extension suffix removed.
static std::string GetSimpleName( const std::string& s ) {
    size_t j = s.find_last_of('.');
    size_t i = s.find_last_of(":/\\");
    i = i>=s.size() ? 0 : i+1;
    j = j<i ? s.size() : j;
    return s.substr(i,j-i);
}

void TrackToWaDialog::setFromTune( MidiTune& tune ) {
	Assert(myItems.empty());
	// Counter used to give name to anonymous tracks
    int k = 0;
    for( size_t i=0; i<tune.nTrack(); ++i ) {
        const MidiTrack& t = tune[i];
        if( t.getInfo().hasNotes ) {
            ++k;
            trackOrWa tw;
            tw.name = t.trackId(k);
            tw.isWaSet = false;
            tw.index = i;
            myItems.push_back(tw);
        }
    }
}

std::string TrackToWaDialog::addWaSet(const std::string& path) {
    trackOrWa t;
    t.fullpath = path;
    t.name = GetSimpleName(path);
    t.isWaSet = true;
    myItems.push_back(t);
    LoadWaCoder(t.name, t.fullpath);
    return t.name;
}

void TrackToWaDialog::addTrack( const char* trackname ) {
    trackOrWa t;
    t.name = trackname;
    t.isWaSet = false;
    // Check if track is defined.  If so, save track index and erase it from its current position.
    for(auto j=myItems.begin(); j!=myItems.end(); ++j)
        if(!j->isWaSet && j->name==t.name) {
            t.index = j->index;
            myItems.erase(j);
            break;
        }
    // Append track to end
    myItems.push_back(t);
}

void TrackToWaDialog::loadTrackMap( NameToWaSetMap& trackMap ) {
	const WaSet* w = NULL;
	trackMap.clear();
	for( auto i=myItems.begin(); i!=myItems.end(); ++i )
		if( i->isWaSet )
			w = TheWaSetMap.find(i->name)->second;
		else 
			if( w )
				trackMap[i->name] = w;
}
