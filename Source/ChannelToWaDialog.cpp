#include "ChannelToWaDialog.h"
#include "Midi.h"
#include "WaSet.h"
#include <fstream>

int ChannelToWaDialog::hilightedTrack( Hue h ) const {
    int i = hilighted(h);
    if( i>=0 ) {
        Assert( unsigned(i)<=size() );
        auto& t = myItems[i];
        if( !t.isWaSet )
            return t.channel;

    }
    return -1;
}

const std::string* ChannelToWaDialog::hilightedWaSet( Hue h ) const {
    int i = hilighted(h);
    if( i>=0 ) {
        Assert( unsigned(i)<=size() );
        auto& t = myItems[i];
        if( t.isWaSet )
            return &t.name;
    }
    return NULL;
}

// Return filename with any drive prefix, directory prefix, or extension suffix removed.
static std::string GetSimpleName( const std::string& s ) {
    size_t j = s.find_last_of('.');
    size_t i = s.find_last_of(":/\\");
    i = i>=s.size() ? 0 : i+1;
    j = j<i ? s.size() : j;
    return s.substr(i,j-i);
}

void ChannelToWaDialog::setFromTune( const Midi::Tune& tune ) {
	Assert(myItems.empty());
    for( size_t i=0, n=tune.channels().size(); i<tune.channels().size(); ++i ) {
        channelOrWaSet tw(false, tune.channels()[i].name());
        tw.channel = i;
        myItems.push_back(tw);
    }
}

std::string ChannelToWaDialog::addWaSet(const std::string& path) {
    channelOrWaSet w(true, GetSimpleName(path));
    myItems.push_back(w);
    return w.name;
}

void ChannelToWaDialog::addChannel( const std::string& name, Midi::Event::channelType channel ) {
    // To protect against accidental duplicates, check if name is already used. 
    // If so, erase it from its current position.
    for(auto j=myItems.begin(); j!=myItems.end(); ++j)
        if( !j->isWaSet && j->name==name ) {
            myItems.erase(j);
            break;
        }
    // Append channel to end
    channelOrWaSet item(false, name);
    item.channel = channel;
    myItems.push_back(item);
}

void ChannelToWaDialog::setupMidiPlayer(Midi::Player& player) {
    const Midi::Tune& tune = player.tune();
    WaSet* w = nullptr;
    for( const channelOrWaSet& i : myItems )
        if( i.isWaSet ) {
            // WaSet to use for subsquent channels, until another WaSet is seen.
            w = TheWaSetCollection.find(i.name);
        } else {
            if( w ) {
                player.setInstrument(i.channel, new WaInstrument(*w));
            } else {
                // No Waset specified
            }
        }
}
