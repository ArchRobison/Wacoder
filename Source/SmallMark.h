/* Copyright 1996-2014 Arch D. Robison 

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

#ifndef SmallMark_H
#define SmallMark_H

#include <vector>
#include "NimbleDraw.h"
#include "Hue.h"

class SmallMark {
    struct pointType {
        signed char x;
        signed char y;
        pointType() {}
        pointType( int x_, int y_ ) : x(x_), y(y_) {}
        friend bool operator<( const pointType& a, const pointType& b ) {
            return a.y!=b.y ? a.y<b.y : a.x<b.x;
        }
        friend bool operator==( const pointType& a, const pointType& b ) {
            return a.y==b.y && a.x==b.x;
        }
    };
    typedef std::vector<pointType> arrayType;
    arrayType myArray;
    void setPixel( int x, int y ) {
        myArray.push_back(pointType(x,y));
    }
    template<typename F>
    void doOp( NimblePixMap& window, int x, int y, F f ) const {
        auto q = myArray.end();
        for( auto p = myArray.begin(); p<q; ++p ) {
            int px = p->x+x;
            int py = p->y+y;
            if( unsigned(px)<unsigned(window.width()) )
                if( unsigned(py)<unsigned(window.height()) )
                    f(*(NimblePixel*)window.at(px,py)); 
    }
}
public:
    bool isEmpty() const {return myArray.empty();}
    void makeCircle( int r );
    void makeCross( int r );
    void makeFat( const SmallMark& m );
    static float minCrossDist( int r, int x, int y );
    void drawOn( NimblePixMap& screen, int x, int y, NimblePixel color ) const;
    void hueify( NimblePixMap& screen, int x, int y, Hue h ) const;
};

#endif /* SmallMark_H */