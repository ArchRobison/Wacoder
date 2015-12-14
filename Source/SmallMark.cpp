#include "SmallMark.h"
#include <algorithm>

void SmallMark::makeCircle( int r ) {
    // Adapted from http://free.pages.at/easyfilter/bresenham.html
    int x = -r, y = 0, err = 2-2*r; /* II. Quadrant */ 
    do {
        setPixel(-x, +y); /*   I. Quadrant */
        setPixel(-y, -x); /*  II. Quadrant */
        setPixel(+x, -y); /* III. Quadrant */
        setPixel(+y, +x); /*  IV. Quadrant */
        r = err;
        if (r <= y) err += ++y*2+1;           /* e_xy+e_y < 0 */
        if (r > x || err > y) err += ++x*2+1; /* e_xy+e_x > 0 or no 2nd y-step */
    } while (x < 0);
}

void SmallMark::makeCross( int r ) {
    setPixel(0,0);
    for( int z=1; z<=r; ++z ) {
        setPixel( 0, z);
        setPixel( 0,-z);
        setPixel( z,0);
        setPixel(-z,0);
    }
}

void SmallMark::makeFat( const SmallMark& m ) {
    for( auto i=m.myArray.begin(); i!=m.myArray.end(); ++i ) {
        int delta[][2] = {{0,2},{-1,1},{0,1},{1,1},{-2,0},{-1,0},{1,0},{2,0},{-1,-1},{0,-1},{1,1},{0,-2}};
        for( int j=0; j<sizeof(delta)/sizeof(delta[0]); ++j )
            setPixel(i->x+delta[j][0],i->y+delta[j][1]);
    }
    // Remove duplicates
    std::sort(myArray.begin(),myArray.end());
    auto j = std::unique(myArray.begin(),myArray.end());
    myArray.resize( j-myArray.begin() );
}

inline float Hypot( float dx, float dy ) {
    return std::sqrt(dx*dx+dy*dy);
}

float SmallMark::minCrossDist( int r, int x, int y ) {
    if(x<0) x=-x;
    if(y<0) y=-y;
    if(x<y) std::swap(x,y);
    if(x>=r) 
        return Hypot(x-r,y);
    else 
        return y;
}

void SmallMark::drawOn( NimblePixMap& window, int x, int y, NimblePixel color ) const {
    doOp(window,x,y,[=](NimblePixel& p) {p=color;});
}

void SmallMark::hueify( NimblePixMap& window, int x, int y, Hue h ) const {
    NimblePixel g = NimblePixel(h|0x404040);
    doOp(window,x,y,[g](NimblePixel& p) {p|=g;});
}

