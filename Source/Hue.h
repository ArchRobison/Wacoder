#ifndef Hue_H
#define Hue_H

#include "AssertLib.h"
#include <bitset>

enum Hue {
    HueNone,
    HueSelected=0x800000,
    HueAccented=0x80
};

inline void operator|=( Hue& h, Hue g ) {
    h = Hue(int(h)|int(g));
}

class HueMap {
public:
    HueMap() : mySelected(-1), myAccented(-1) {}

    /** F should map Hue to int. */
    template<typename F>
    HueMap( const F& f ) : mySelected(f(HueSelected)), myAccented(f(HueAccented)) {}

    bool empty() const {
        return mySelected==-1 && myAccented==-1;
    }
    void clear() {
        mySelected = -1;
        myAccented = -1;
    }
    void set( int k, Hue h ) {
        Assert( h==HueSelected||h==HueAccented );
        if( k==HueSelected ) mySelected = k;
        if( k==HueAccented ) myAccented = k;
    }
    Hue hueOf( int k ) const {
        int h = HueNone;
        if( k==mySelected ) h |= HueSelected;
        if( k==myAccented ) h |= HueAccented;
        return Hue(h);
    }
    int indexOf( Hue h ) const {
        Assert( h==HueSelected||h==HueAccented );
        return h==HueSelected ? mySelected : myAccented;
    }
    template<size_t N>
    Hue hueOf( const std::bitset<N>& s ) {
        int h = HueNone;
        if( size_t(mySelected)<N && s.test(mySelected) ) h |= HueSelected;
        if( size_t(myAccented)<N && s.test(myAccented) ) h |= HueAccented;
        return Hue(h);
    }
private:
    int mySelected;
    int myAccented;
};

#endif /* Hue_H */