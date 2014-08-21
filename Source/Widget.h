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
 Various widgets for the screen
*******************************************************************************/

#pragma once
#ifndef Widget_H
#define Widget_H

#include "BuiltFromResource.h"
#include "Nimbledraw.h"

class Widget: BuiltFromResourcePixMap {
public:
    Widget( const char* resourceName ) : 
        BuiltFromResourcePixMap(resourceName) 
    {}
protected:
    NimblePixMapWithOwnership myPixMap;   
    /*override*/ void buildFrom( const NimblePixMap& map );
private:
    Widget( const Widget& );
    void operator=( const Widget& );
};

//! Image that can be stretched to fit a rectangle.
/** Stretching does not distort the borders. */
class RubberImage: public Widget {
public:
    RubberImage( const char* resourceName );
    //! Draw image on window, stretching to fit
    void drawOn( NimblePixMap& window ) const {drawOn(window,NimbleRect(0,0,window.width(),window.height()));}
    //! Draw image on rect portion of window.  Clip if rect goes outside bounds of window.
    void drawOn( NimblePixMap& window, const NimbleRect& rect ) const;
};

class Font: BuiltFromResourcePixMap {
public:
    Font( const char* resourceName ) : 
        BuiltFromResourcePixMap(resourceName),
        storage(NULL), myHeight(0)
    {
    }
    ~Font() {delete[] storage;}
    /** Returns x coordinate that next character would have. */
    int drawOn( NimblePixMap& map, int x, int y, const char* s, NimblePixel ink );
    int height() const {return myHeight;}
    int width( const char* string ) const;
private:
    static const char charMin=32;
    static const char charMax=127;
    byte* storage;
    byte myHeight;
    unsigned short start[charMax-charMin+2];
    /*override*/ void buildFrom( const NimblePixMap& map );
    Font( const Widget& );
    void operator=( const Font& );
    static bool isBlankColumn( const NimblePixMap& map, int x );
};

class DigitalMeter: public Widget {
public:
    /** nDigit is total number of digits, including decimal point.
        and the nDecimal digits after the point. */
    DigitalMeter( int nDigit, int nDecimal );
    void drawOn( NimblePixMap& map, int x, int y ) const;
    void setValue( float value ) {myValue=value;}
    float value() const {return myValue;}
    void operator+=( float addend ) {myValue+=addend;}
    int width() const {return myPixMap.width()*myNdigit;}
    int height() const {return myPixMap.height()/13;}
private:
    float myValue;
    signed char myNdigit;
    signed char myNdecimal;
};

class InkOverlay: public BuiltFromResourcePixMap {
public:
    InkOverlay( const char* resourceName ) : 
        BuiltFromResourcePixMap(resourceName),
        myWidth(0),
        myHeight(0)
    {}
    ~InkOverlay() {delete[] myArray;}
    void drawOn( NimblePixMap& map, int top, int bottom ) const;
    int height() const {
        Assert(myWidth>=32);
        return myHeight;
    }
    int width() const {
        Assert(myWidth>=32);
        return myWidth;
    }
protected:  
    /*override*/ void buildFrom( const NimblePixMap& map );
private:
    InkOverlay( const InkOverlay& );
    void operator=( const InkOverlay& );
    struct runType {
        static const unsigned dyMax = 255;
        NimblePixel color;
        unsigned x:12, dy:8;
        unsigned len:12;
    };
    runType* myArray;
    int myWidth, myHeight;
    size_t mySize;
};

#endif /* Widget_H */