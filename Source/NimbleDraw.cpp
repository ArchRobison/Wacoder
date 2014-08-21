/* Copyright 1996-2012 Arch D. Robison 

   Licensed under the Apache License, Version 2.0 (the "License"); 
   you may not use this file except in compliance with the License. 
   You may obtain a copy of the License at 
   
       http://www.apache.org/licenses/LICENSE-2.0 
       
   Unless required by applicable law or agreed to in writing, software 
   distributed under the License is distributed on an "AS IS" BASIS, 
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
   See the License for the specific language governing permissions and 
   limitations under the License.
 */

/******************************************************************************
 Graphics layer for Seismic Duck and Voromoeba
*******************************************************************************/

#include "AssertLib.h"
#include "NimbleDraw.h"
#include "Utility.h"
#include <cstring>

NimbleColor NimblePixMap::color( NimblePixel p ) const {
#if NIMBLE_MAC
#error /* Not yet implemented */
#elif NIMBLE_DIRECTX
    NimbleColor c;
    // Note that our video card differs from description of 16-bit color on page 92 of 
    // Windows Game Programming for Dummies.  Our card has a 6-bit green channel.
    if( sizeof(NimblePixel)==2 ) {
        // 16-bit color
        c.red = p>>8&0xF8;
        c.green = p>>3&0xFC;
        c.blue = p<<3&0xF8;
    } else {
        Assert( sizeof(NimblePixel)==4 );
        c.red = p>>16 & 0xFF;
        c.green = p>>8 & 0xFF;
        c.blue = p & 0xFF;
    }
    return c;
#else
#error Unknown target
#endif
}

NimblePixel NimblePixMap::pixel( const NimbleColor& c ) const {
#if NIMBLE_MAC
    RGBColor d;
    d.red = c.red;
    d.green = c.green;
    d.blue = c.blue;
    return Color2Index(&d);
#elif NIMBLE_DIRECTX
    // Note that our video card differs from description of 16-bit color on page 92 of 
    // Windows Game Programming for Dummies.  Our card has a 6-bit green channel.
    if( sizeof(NimblePixel)==2 ) {
        // 16-bit color (not supported as of 2010 Jan 1)
        return (c.red&0xF8)<<8|(c.green&0xFC)<<3|(c.blue&0xF8)>>3;
    } else {
        Assert( sizeof(NimblePixel)==4 );
        // 32-bit color
        return c.red<<16 | c.green<<8 | c.blue;
    }
#else
#error Unknown target
#endif
}

void NimblePixMap::setBitPixelDepth( int bitsPerPixel ) {
    int d = 0;
    switch( bitsPerPixel ) {
        case 8: d = 3; break;
        case 16: d = 4; break;
        case 32: d = 5; break;
        default: Assert(0);
    }
    // In old versions of NimblePixMap, d was assigned to a field this->myLgBitPixelDepth.
    // But the current version assumes that pixels have a size known at compilation time,
    // hence the assignment as been replaced by an assertion.
    Assert( lgBitPixelDepth()==d );
}

void NimblePixMap::shift( int deltaX, int deltaY ) {
    myBaseAddress = (NimblePixel*)((byte*)myBaseAddress + deltaX*bytePixelDepth() + deltaY*bytesPerRow());
}

void NimblePixMap::adjustTop( int delta ) {
    Assert(delta<=myHeight);
    myBaseAddress = (NimblePixel*)((byte*)myBaseAddress + delta*bytesPerRow());
    myHeight -= delta;
}

NimblePixMap::NimblePixMap( int width, int height, int bitsPerPixel, void* base, int bytesPerRow ) :
    myBaseAddress((NimblePixel*)base),
    myBytesPerRow(bytesPerRow),
    myWidth(width),
    myHeight(height)
{
    setBitPixelDepth(bitsPerPixel);
    Assert( width>=0 );
    Assert( width==myWidth );   // Fails if width does not fit in myWidth
    Assert( height>0 );
    Assert( height==myHeight ); // Fails if height does not fit in myHeight
    Assert( base!=NULL );
} 

NimblePixMap::NimblePixMap( const NimblePixMap& src, const NimbleRect& rect ) {
    Assert( 0 <= rect.left && rect.right <= src.width() );
    Assert( 0 <= rect.top && rect.bottom <= src.height() );
    myBytesPerRow = src.myBytesPerRow;
    myHeight = rect.height();
    myWidth = rect.width();
    myBaseAddress = (NimblePixel*)((byte*)src.myBaseAddress + rect.top*myBytesPerRow + (rect.left<<lgBytePixelDepth()));
}

void NimblePixMap::draw( const NimbleRect& r, NimblePixel pixel ) {
    int xl = Max(0,int(r.left));
    int xr = Min(int(r.right),width() );
    int w = xr-xl;
    if( w>0 ) {
        int yt = Max(0,int(r.top));
        int yb = Min(height(),int(r.bottom));
        for( int y=yt; y<yb; ++y ) {
            NimblePixel* out = (NimblePixel*)at(xl,y);
            for( int x=0; x<w; ++x )
                *out++ = pixel;
        }
    }
}

void NimblePixMap::drawOn( NimblePixMap& dst, int x, int y ) const {
    Assert( dst.lgBitPixelDepth()==lgBitPixelDepth() );
    // Clip left side of source
    int w = width();
    int j = 0;
    if( x<0 ) {
        w+=x;
        j-=x;
        x=0;
    }
    // Reject if width was zero or clipped to zero.
    if( w<=0 ) return;
    // Clip right side of source
    if( x+w>dst.width() ) {
        w = dst.width()-x;
        if( w<=0 ) return;
    }
    // Clip bottom of source
    int h = height();
    if( y+h>dst.height() ) {
        h = dst.height()-y;
        if( h<=0 ) return;
    }
    for( int i=Max(0,-y); i<h; ++i ) {
        std::memcpy( dst.at(x,y+i), at(j,i), w*sizeof(NimblePixel) );
    }
}

NimblePixel NimblePixMap::interpolatePixelAt( float x, float y ) {
    Assert(0<=x);
    Assert(0<=y);
    Assert(x<=width());
    Assert(y<=height());
    int j = Floor(x);
    int i = Floor(y);
    float u = x-j;
    float v = y-i;
    float f[4];
    f[0] = u*v;
    f[1] = (1-u)*v;
    f[2] = u*(1-v);
    f[3] = (1-u)*(1-v);
    Assert( fabs( f[0]+f[1]+f[2]+f[3]-1 ) <= 1E-6 );
    NimbleColor c[4];
    c[0] = colorAt(j,i);
    c[1] = j+1<width() ? colorAt(j+1,i) : c[0];
    c[2] = i+1<height() ? colorAt(j,i+1) : c[0];
    c[3] = j+1<width() && i+1<height() ? colorAt(j+1,i+1) : c[0];
    float r=0, g=0, b=0;
    for( int k=0; k<4; ++k ) {
        r += c[k].red*f[k];
        g += c[k].green*f[k];
        b += c[k].blue*f[k];
    }
    NimbleColor average = NimbleColor(Round(r),Round(g),Round(b));
    return pixel(average);
}

void NimblePixMapWithOwnership::deepCopy( const NimblePixMap& src ) {
    // In all current use cases, the destination map is initially empty.
    Assert(!myBaseAddress);
    int h = myHeight = src.height();
    int w = myWidth = src.width();
    // Use operator new here to get most favorable alignment.
    myBaseAddress = (NimblePixel*)operator new(h*w*sizeof(NimblePixel));
    myHeight = h;
    myBytesPerRow = w*sizeof(NimblePixel);
    for( int y=0; y<h; ++y )
        memcpy( at(0,y), src.at(0,y), w*sizeof(NimblePixel) );
}

NimblePixMapWithOwnership::~NimblePixMapWithOwnership() {
    operator delete(myBaseAddress);
}