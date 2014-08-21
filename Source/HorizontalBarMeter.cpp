#include "HorizontalBarMeter.h"
#include "NimbleDraw.h"

//-----------------------------------------------------------------
// HorizontalBarMeter
//-----------------------------------------------------------------

void HorizontalBarMeter::drawOn( NimblePixMap& map, int x, int y ) const {
    int wl = int((myLevel*width())+0.5f);
    int h = height();
    NimblePixMap(myPixMap,NimbleRect(0,h,wl,2*h)).drawOn( map, x, y );
    NimblePixMap(myPixMap,NimbleRect(wl,0,width(),h)).drawOn( map, x+wl, y );
}