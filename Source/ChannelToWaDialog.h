#include "Clickable.h"

#ifndef ChannelToWaDialog_H
#define ChannelToWaDialog_H

#include <string>
#include <vector>
#include "Midi.h"
#include "Orchestra.h"

class WaSet;

class ChannelToWaDialog: public PermutationDialog {
    //! Index of a channel or a reference to a WaSet.
    struct channelOrWaSet {
        //! True if WaSet, false otherwise.
        bool isWaSet;
        //! If a channel, the index of the channel within the tune.  
        Midi::Event::channelType channel;
		//! Name that appears in dialog - either name of channel or name of WaCoder
        std::string name;
        channelOrWaSet( bool isWaSet_, const std::string& name_ ) : isWaSet(isWaSet_), name(name_) {}
    };
    typedef std::vector<channelOrWaSet> itemVectorType;
    itemVectorType myItems;
    /*override*/size_t size() const {return myItems.size();}
    const char* name( size_t i ) const {
        return myItems[i].name.c_str();
    }
    int indent( size_t i ) const {
        return myItems[i].isWaSet ? 0 : 1;
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
    ChannelToWaDialog() : PermutationDialog(192,100) {}

	//! If a channel has been hilighted, return the id of the channel.  Otherwise return -1.
    int hilightedTrack( Hue h ) const;

    HueMap hilightedTracks() const {
        return HueMap( [this]( Hue h ) {return hilightedTrack(h);} );
    }
	//! If a waSet is highlighted, return a pointer to a string with its name.  Otherwise return NULL.
    const std::string* hilightedWaSet( Hue h ) const;

	//! Set entries from given tune.  Clears waSet entries.
    void setFromTune( const Midi::Tune& tune );

    //! Add a WaSet to end.  path is the path to the .wav file.  Returns name of the WaSet.
    std::string addWaSet( const std::string& path );

    //! Add a channel name to end.
    void addChannel( const std::string& name, Midi::Event::channelType channel );

	//! Read dialog layout from a file, and merge information into existing track entries. 
    void clear() { 
        myItems.clear(); 
        PermutationDialog::clear();
    }

    //! Invoke f(bool,string) for each item.  The string is the name.
    /** The bool is true for a WaSet and false for a channel. */
    template<typename F>
    void forEach( F f ) const {
        for(const channelOrWaSet& cow: myItems)
            f(cow.isWaSet, cow.name);
    }

	void setupOrchestra( Midi::Orchestra& orchestra );
};

#endif /* ChannelToWaDialog_H */