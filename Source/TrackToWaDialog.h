#include "Clickable.h"

#ifndef TrackToWaDialog_H
#define TrackToWaDialog_H

#include <string>
#include <vector>
#include "Midi.h"

class Wack;

class TrackToWaDialog: public PermutationDialog {
    struct trackOrWa {
        bool isWaCoder;
        //! If a track, the index of the track within the tune.
        short index;
        Wack* waCoder;
		//! Name that appears in dialog - either name of track or name of WaCoder
        std::string name;
		//! If a WaCoder, full path to WaCoder input file
        std::string fullpath;
        trackOrWa() : waCoder(NULL), isWaCoder(false), index(-1) {}
    };
    typedef std::vector<trackOrWa> itemVectorType;
    itemVectorType myItems;
    /*override*/size_t size() const {return myItems.size();}
    const char* name( size_t i ) const {
        return myItems[i].name.c_str();
    }
    int indent( size_t i ) const {
        return myItems[i].isWaCoder ? 0 : 1;
    }
    /*override*/void doMove( size_t from, size_t to ) {
        Assert( from<myItems.size() );
        Assert( to<=myItems.size() );
        auto b = myItems.begin();
        if( from<to ) 
            std::rotate( b+from, b+from+1, b+to );
        else
            std::rotate( b+to, b+from, b+(from+1) );
    }
public:
    TrackToWaDialog() : PermutationDialog(192,100) {}
	//! If a track has been hilighted, return the id of the track.  Otherwise return -1.
    int hilightedTrack( Hue h ) const;
    HueMap hilightedTracks() const {
        return HueMap( [this]( Hue h ) {return hilightedTrack(h);} );
    }
	//! If a wack is highlighted, return a pointer to a string with its name.  Otherwise return NULL.
    const std::string* hilightedWack( Hue h ) const;
	//! Set tracks entries from given tune.
    void setFromTune( MidiTune& tune );
	//! Read dialog layout from a file, and merge information into existing track entries. 
	/** File format is sequence of lines.  Each line denotes either path to a WaCoder file or a trackname.
	    Each track is associated with the nearest WaCoder above it.
	    @ path1
		trackname1
		trackname2
		...
		@ path2
		trackname1
		trackname2
		...
    */
    void readFromFile( const char* filename );
	void loadWaCoders();
	void loadTrackMap( NameToWackMap& trackMap );
};

#endif /* TrackToWaDialog_H */