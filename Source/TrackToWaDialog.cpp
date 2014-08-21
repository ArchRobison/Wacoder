#include "TrackToWaDialog.h"
#include "Midi.h"
#include <fstream>

int TrackToWaDialog::hilightedTrack( Hue h ) const {
    int i = hilighted(h);
    if( i>=0 ) {
        Assert( unsigned(i)<=size() );
        auto& t = myItems[i];
        if( !t.isWaCoder )
            return t.index;

    }
    return -1;
}

const std::string* TrackToWaDialog::hilightedWack( Hue h ) const {
    int i = hilighted(h);
    if( i>=0 ) {
        Assert( unsigned(i)<=size() );
        auto& t = myItems[i];
        if( t.isWaCoder )
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

static std::string GetSimpleName( const std::string& s ) {
    size_t j = s.find_last_of('.');
    size_t i = s.find_last_of(":/");
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
            tw.isWaCoder = false;
            tw.index = i;
            myItems.push_back(tw);
        }
    }
}

void TrackToWaDialog::readFromFile( const char* filename ) {
    std::ifstream f(filename);
    std::string buf;
    while( getline(f,buf) ) {
        trackOrWa t;
        const char* ptr = buf.c_str();
        while( *ptr && isspace(*ptr) )
            ++ptr;
        if( *ptr=='@' ) {
			// Specification of a WaCoder to use.
            t.fullpath = TrimWhiteSpace(std::string(ptr+1));
            t.name = GetSimpleName(t.fullpath);
            t.isWaCoder = true;
            myItems.push_back(t);
        } else if( *ptr ) {
            t.name = TrimWhiteSpace(std::string(ptr));
            t.isWaCoder = false;
			// Check if track is defined.  If so, save track index and erase it from its current position.
			for( auto j=myItems.begin(); j!=myItems.end(); ++j )
				if( !j->isWaCoder && j->name==t.name ) {
					t.index = j->index;
					myItems.erase(j);
			        break;
				}
			// Append track to end
			myItems.push_back(t);
        } else {
			// Ignore blank line
		}
    }
    f.close();
}

void TrackToWaDialog::loadWaCoders() {
	for( auto i=myItems.begin(); i!=myItems.end(); ++i )
		if( i->isWaCoder )
			LoadWaCoder( i->name, i->fullpath );
}

void TrackToWaDialog::loadTrackMap( NameToWackMap& trackMap ) {
	const Wack* w = NULL;
	trackMap.clear();
	for( auto i=myItems.begin(); i!=myItems.end(); ++i )
		if( i->isWaCoder )
			w = TheWackMap.find(i->name)->second;
		else 
			if( w )
				trackMap[i->name] = w;
}
