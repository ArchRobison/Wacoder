#include "Clickable.h"

#ifndef TrackToWaDialog_H
#define TrackToWaDialog_H

#include <string>
#include <vector>
#include "Midi.h"

class Wack;

class TrackToWaDialog: public PermutationDialog {
    //! A track or a wacoder.
    struct trackOrWa {
        bool isWaCoder;
        //! If a track, the index of the track within the tune.  -1 for wacoders.
        short index;
        Wack* waCoder;
		//! Name that appears in dialog - either name of track or name of WaCoder
        std::string name;
		//! If a WaCoder, full path to WaCoder input file.  Empty tracks.
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

	//! Set tracks entries from given tune.  Clears wack entries.
    void setFromTune( MidiTune& tune );

    //! Add a wacoder to end.  path is the path to the .wav file.  Returns name of wacoder.
    std::string addWaCoder( const std::string& path );

    //! Add a track to end.
    void addTrack( const char* trackname );

	//! Read dialog layout from a file, and merge information into existing track entries. 
    void clear() { 
        myItems.clear(); 
        PermutationDialog::clear();
    }

    //! Invoke f(bool,string) for each item.  
    /** For a wack, the bool is true and the string is the full path.
        For a track, the bool is false and the string is the track name. */
    template<typename F>
    void forEach( F f ) const {
        for( const trackOrWa& t: myItems )
            f(t.isWaCoder, t.isWaCoder ? t.fullpath : t.name);
    }

	void loadTrackMap( NameToWackMap& trackMap );
};

#endif /* TrackToWaDialog_H */