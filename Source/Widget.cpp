/* Copyright 1996-2013 Arch D. Robison 

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
 Various widgets for Wacoder
*******************************************************************************/

#include "NimbleDraw.h"
#include "Widget.h"
#include "Utility.h"
#include <cstring>

//! Given point inside transparent box, find bounds of box.
static NimbleRect FindTransparentBox( const NimblePixMap& map, int xInside, int yInside ) {
    NimbleRect box;
    Assert( map.alpha(xInside,yInside)<NimbleColor::full/2 );
    int x=xInside;
    int y=yInside;
    while( x>0 && map.alpha(x-1,y)<NimbleColor::full/2 ) 
        --x;
    while( y>0 && map.alpha(x,y-1)<NimbleColor::full/2 ) 
        --y;
    box.left=x;
    box.top=y;
    x=xInside;
    y=yInside;
    while( y+1<map.height() && map.alpha(x,y+1)<NimbleColor::full/2 ) 
        ++y;
    while( x+1<map.width() && map.alpha(x+1,y)<NimbleColor::full/2 ) 
        ++x;
    box.right=x+1;
    box.bottom=y+1;
    return box;
}

//-----------------------------------------------------------------
// Widget
//-----------------------------------------------------------------

void Widget::buildFrom( const NimblePixMap& map ) {
    myPixMap.deepCopy(map);
}

//-----------------------------------------------------------------
// RubberImage
//-----------------------------------------------------------------

RubberImage::RubberImage( const char* resourceName ) : 
    Widget( resourceName )
{}

void RubberImage::drawOn( NimblePixMap& window, const NimbleRect& rect ) const {
    Assert( 0<=rect.left && rect.right<=window.width() );  // Horizontal clipping not yet implemented
    int dWidth=rect.width();
    int dHeight=rect.height();
    int sWidth=myPixMap.width();
    int sHeight=myPixMap.height();
    int x1=sWidth/2;
    int x2=dWidth-x1;
    int y1=sHeight/2;
    int y2=dHeight-y1;
    int yFirst = Max(+rect.top,0);
    int yLast = Min(+rect.bottom,window.height());
    int dstX = rect.left;
    for( int dstY=yFirst; dstY<yLast; ++dstY ) { 
        int y = dstY-rect.top;
        Assert( 0<=dstY && dstY<=window.height() ); 
        int srcY = y<y1 ? y : y<y2 ? y1 : y-(y2-y1);
        NimblePixel* src = (NimblePixel*)myPixMap.at(0,srcY);
        NimblePixel* dst = (NimblePixel*)window.at(dstX,dstY);
        memcpy( dst, src, x1*sizeof(NimblePixel) );
        NimblePixel fill = src[x1];
        for( int k=x1; k<x2; ++k )
            dst[k] = fill;
        memcpy( dst+x2, src+x1,(dWidth-x2)*sizeof(NimblePixel) );
    }
}

//-----------------------------------------------------------------
// Font
//-----------------------------------------------------------------

inline bool Font::isBlankColumn( const NimblePixMap& map, int x ) {
    Assert(0<=x && x<map.width());
    int h = map.height();
    for( int i=0; i<h; ++i )
        if( map.pixel(x,i)&0xFF )
            return false;
    return true;
}

void Font::buildFrom( const NimblePixMap& map ) {
    Assert(!storage);
    myHeight = map.height();
    const int nByte = map.height()*map.width(); 
    storage = new byte[nByte];
    byte* p = storage;
    int x = 0;
    // Zeroth column must be blank
    Assert( isBlankColumn( map, 0 ));
    for( int k=charMin; k<=charMax; ++k ) {
        while( isBlankColumn( map, x ) ) {
            ++x;
        }
        int xStart = x-1;
        int width=2;
        while( !isBlankColumn( map, x )) {
            ++x;
            ++width;
        }
        start[k-charMin] = p-storage;
        for( int i=0; i<myHeight; ++i )
            for( int j=0; j<width; ++j ) {
                Assert(p<storage+nByte);
                // Bottom row of pixels is used only as internal marker for ' ' and ".
                *p++ = i==myHeight-1 ? 0 : map.pixel(xStart+j,i)&0xFF; 
            }
    }
    start[charMax+1-charMin] = p-storage;
}

int Font::width( const char* s ) const {
    Assert(storage);
    Assert(myHeight>0);
    // Accumulate width times height into w.
    int w = 0;
    for(; int c = *s; ++s) {
        Assert(charMin<=c && c<=charMax);
        w  += start[c+1-charMin]-start[c-charMin];
    }
    return w/myHeight;
}

int Font::drawOn( NimblePixMap& map, int x, int y, const char* s, NimblePixel ink ) {
    for(; int c = *s; ++s) {
        Assert(charMin<=c && c<=charMax);
        byte* p = storage + start[c-charMin];
        byte* q = storage + start[c+1-charMin];
        Assert((q-p)%myHeight==0);
        int width = (q-p)/myHeight;
        int skip = 0;
        if( x+width>map.width() ) {
            skip = x+width-map.width();
            width -= skip;
            if( width==0 )
                return x;
        }
        int h = Min(int(myHeight),map.height()-y);
        for( int i=0; i<h; ++i ) {
            NimblePixel* dst = (NimblePixel*)map.at(x,y+i);
            for( int j=0; j<width; ++j, ++dst )
                if( *p++>=0x80 ) 
                    *dst = ink;
            p += skip;
        }
        // Assert(p==q);
        x+=width;
    }
    return x;
}

//-----------------------------------------------------------------
// DecimalNumeral
//-----------------------------------------------------------------

const int DIGIT_MAX = 10;

struct DecimalNumeral {
    DecimalNumeral( unsigned value, bool zeroIsEmptyString=false );
    byte digit[DIGIT_MAX];
    //! Number of valid digits in array digit.
    byte size;
};

DecimalNumeral::DecimalNumeral( unsigned value, bool zeroIsEmptyString ) {
    // Convert to decimal. 
    int k=0;
    if( value || !zeroIsEmptyString ) {
        while( value>=10u ) {
            digit[k++] = value%10u;
            value/=10u;
        }
        digit[k++]=value;
    }
    size=k;
}

//-----------------------------------------------------------------
// DigitalMeter
//-----------------------------------------------------------------

DigitalMeter::DigitalMeter( int nDigit, int nDecimal ) :
    Widget( "DigitalMeter" ),
    myValue(0),
    myNdigit(nDigit),
    myNdecimal(nDecimal)
{
    Assert( 0<nDigit && nDigit<=DIGIT_MAX );
    Assert( 0<=nDecimal && nDecimal<=DIGIT_MAX );
}

void DigitalMeter::drawOn( NimblePixMap& map, int x, int y ) const {
    static const double powerOfTen[] = {1,10,100,1000};
    Assert(myValue>=0); // Negative values not yet supported
    Assert( size_t(myNdecimal)<sizeof(powerOfTen)/sizeof(powerOfTen[0]));
    DecimalNumeral numeral(myValue*powerOfTen[myNdecimal]+0.5f);
    int faceWidth = myPixMap.width();
    int faceHeight = height();
    for( int k=0; k<myNdigit; ++k ) {
        int i = myNdecimal ?
                    k<myNdecimal ? (k<numeral.size ? numeral.digit[k] : 0) :
                    k==myNdecimal ? 12 :
                    k-1<numeral.size ? numeral.digit[k-1] : 10 :
                k<numeral.size ? numeral.digit[k] : 10;
        int faceTop=i*faceHeight;
        NimblePixMap subrect( myPixMap, NimbleRect(0, faceTop, faceWidth, faceTop+faceHeight ) );
        subrect.drawOn( map, x+faceWidth*(myNdigit-1-k), y );
    }
}

//-----------------------------------------------------------------
// InkOverlay
//-----------------------------------------------------------------

void InkOverlay::buildFrom( const NimblePixMap& map ) {
    myWidth=map.width();
    myHeight=map.height();
    NimblePixel transparent = map.pixel(0,0);
    for( int pass=0; pass<2; ++pass ) {
        size_t count = 0;
        unsigned oldY = 0;
        for( int y=0; y<map.height(); ++y )
            for( int x=0; x<map.width(); ++x ) 
                if( map.pixel(x,y)!=transparent ) {
                    while( y-oldY>runType::dyMax ) {
                        if( pass==1 ) {
                            runType* r = myArray+count;
                            r->color = NimblePixel::black;   // Color not used
                            r->x = 0; 
                            r->dy = runType::dyMax;
                            r->len = 0;    
                        }
                        oldY += runType::dyMax;
                        ++count;
                    }
                    NimblePixel* p = (NimblePixel*)map.at(x,y);
                    NimblePixel c = map.pixel(x,y);
                    int len = 1;
                    while( x+len<map.width() && p[len]==c ) 
                        ++len;
                    if( pass==1 ) {
                        runType* r = myArray+count;
                        r->color = c;
                        r->x = x;
                        r->dy = y-oldY;
                        r->len = len;
                    }
                    x += len-1;
                    oldY = y;
                    ++count;
                }
        if( pass==0 ) {
            Assert( !myArray );
            myArray = new runType[count];
            mySize = count;
        } else {
            Assert(mySize==count);
        }
    }
}

void InkOverlay::drawOn( NimblePixMap& map, int left, int top ) const {
    runType* end = myArray+mySize;
    unsigned y = 0;
    for( runType* r=myArray; r<end; ++r ) {
        y += r->dy;
        NimblePixel* p = (NimblePixel*)map.at(left+r->x,top+y);
        NimblePixel c = r->color;
        for( unsigned len=r->len; len>0; --len )  
            *p++ = c;
    }
}

