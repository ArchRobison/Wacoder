#include "NimbleDraw.h"
#include <map>
#include <vector>
#include <algorithm>
#include "WaPlot.h"
#include "SmallMark.h"

class SmallMarkCache {
    void (SmallMark::*make)(int r);
    std::map<short,SmallMark> cache;
    SmallMarkCache* fatof;
public:
    SmallMarkCache( void (SmallMark::*make_)(int r) ) : make(make_), fatof(NULL) {}
    SmallMarkCache( SmallMarkCache* fatof_=NULL ) : make(NULL), fatof(fatof_) {}
    const SmallMark& operator[]( int r );
};

const SmallMark& SmallMarkCache::operator[]( int r ) {
    Assert( (fatof!=NULL)+(make!=NULL) == 1 ); 
    SmallMark& m = cache[r];
    if( m.isEmpty() ) {
        if( fatof )
            m.makeFat((*fatof)[r]);
        else
            (m.*make)(r);
    }
    return m;
};

static SmallMarkCache CircleMarkCache(&SmallMark::makeCircle);
static SmallMarkCache FatCircleMarkCache(&CircleMarkCache);
static SmallMarkCache CrossMarkCache(&SmallMark::makeCross);
static SmallMarkCache FatCrossMarkCache(&CrossMarkCache);

void WaPlot::waPoint::setFromPitchAndDuration( float pitch, float duration ) {
    x = std::log(pitch*duration);
    y = std::log(pitch);
}

void WaPlot::waPoint::getPitchAndDuration( float& pitch, float& duration ) const {
    pitch = std::exp(y);  
    duration = std::exp(x)/pitch;
}

void WaPlot::drawNotesAndWas( NimblePixMap& window ) {
    // Compute transform from Wa-space coordinates to Window coordinates
    LinearTransform1D<float> tx(xLimit.min,0,xLimit.max,window.width());
    LinearTransform1D<float> ty(yLimit.max,0,yLimit.min,window.height());
    LinearTransform1D<float> tr(0,0,rLimit.max,16); 
    // Draw pitch lines
    const float logMiddleC = 5.5669143f;
    const float log2 = 0.69314718f;
    for( int i=-1; i<=1; ++i ) {
        int h = i==0 ? 1 : 0;
        int y = ty(logMiddleC+i*log2);
        window.draw(NimbleRect(0,y-h,window.width(),y+h+1),0x808080);
    }
	// FIXME - factor note circles and wa cross mechanics into two instances of a template

    // Draw note circles
    for( auto i=myNotes.set.begin(); i!=myNotes.set.end(); ++i ) {
        short r = tr(i->first.r);
        auto& m = CircleMarkCache[r];
        m.drawOn( window, tx(i->first.x), ty(i->first.y), 0xFFFFFF );
    }
   
    if( !myNotes.chosen.empty() ) {
        // Hueify every note belonging to that track
        for( auto i=myNotes.set.begin(); i!=myNotes.set.end(); ++i ) {
            if( Hue h = myNotes.chosen.hueOf(i->second) ) {
                short r = tr(i->first.r);
                auto& m = FatCircleMarkCache[r];
                m.hueify( window, tx(i->first.x), ty(i->first.y), h );
            }
        }
    }
    // Draw wa crosses
	myWaBuf.clear();
    for( auto i=myWas.set.begin(); i!=myWas.set.end(); ++i ) {
        auto& m = CrossMarkCache[myCrossRadius];
		NimblePoint p(tx(i->first.x), ty(i->first.y));
        m.drawOn( window, p.x, p.y, 0xFFFFFF );
		// Save coordinates so that cross can be found given a mouse coordinate
		myWaBuf.push_back(p);
    }
    if( !myWas.chosen.empty() ) {
		// Hueify every wa cross with that id
		for( auto i=myWas.set.begin(); i!=myWas.set.end(); ++i ) {
			if( Hue h = myWas.chosen.hueOf(i->second)) {
                auto& m = FatCrossMarkCache[myCrossRadius];
                m.hueify( window, tx(i->first.x), ty(i->first.y), h );
			}
		}
    }
	if( myPreciseSelectedWa.x>=0 && myPreciseSelectedWa.y>=0 ) {
		// Draw the selected mark in red (FIXME - be friendlier to users with limited color acuity)
		auto& m = CrossMarkCache[myCrossRadius];
		m.drawOn( window, myPreciseSelectedWa.x, myPreciseSelectedWa.y, 0xFF0000 );
	}
    if( haveLiveWa ) {
        int x = tx(liveWa.x);
        int y = ty(liveWa.y);
        window.draw( NimbleRect(x-4,y-4,x+4,y+4),0x00FF00 );
    }
}

void WaPlot::insertNote( float pitch, float duration, int velocity, int trackId ) {
	Assert(unsigned(trackId)<myNotes.maxId);
    noteMark c;
    c.setFromPitchAndDuration(pitch,duration);
    c.r = velocity;
    myNotes.set[c].set(trackId);
    xLimit.include(c.x);
    yLimit.include(c.y);
    rLimit.include(c.r);
}

void WaPlot::insertWa( float pitch, float duration, int wackId ) {
	Assert(unsigned(wackId)<myWas.maxId);
    waMark w;
    w.setFromPitchAndDuration(pitch,duration);
    myWas.set[w].set(wackId);
    xLimit.include(w.x);
    yLimit.include(w.y);
}

int WaPlot::getWackId( const std::string& wackName ) {
	auto i = myWackIds.find(wackName);
	if( i==myWackIds.end() ) {
		// The name has no id yet, so create one.
		int n = myWackIds.size();
		myWackIds[wackName] = n;
		return n;
	} else {
		return i->second;
	}
}

void WaPlot::clear() {
    myNotes.clear();
	myWas.clear();
	myWackIds.clear();
    xLimit.clear();
    yLimit.clear();
    rLimit.clear();
    haveLiveWa = false;
	myPreciseSelectedWa.x = myPreciseSelectedWa.y = -1;
}

void WaPlot::doDrawOn( NimblePixMap& window ) {
    NimblePixMap area( window, NimbleRect(0,0,clickableWidth(),clickableHeight()));
    // Clear window
    area.draw(NimbleRect(0,0,area.width(),area.height()),0);
    drawNotesAndWas(area);
}

void WaPlot::setDynamicWa( float pitch, float duration ) {
    if( pitch>0 && duration>0 ) {
        haveLiveWa = true;
        liveWa.setFromPitchAndDuration(pitch,duration);
    } else {
        haveLiveWa = false;
    }
}

static inline float Dist( int x0, int y0, int x1, int y1 ) {
    float dx = float(x1-x0);
    float dy = float(y1-y0);
    return std::sqrt(dx*dx+dy*dy);
}

static float MaxSelectD = 8;

int WaPlot::preciseSelect( NimblePoint p ) {
    // Find upper bound on distance from p to a selectable mark.
	// The result is an upper bound since the distance is measured to the center of a mark.
    float uMin = FLT_MAX;
    for( auto i=myWaBuf.begin(); i!=myWaBuf.end(); ++i ) {
        float u = Dist(p.x,p.y,i->x,i->y);
        if( u<uMin ) 
			uMin=u;
    }
    // Find closest mark, accounting for shape of the mark.
    float r = myCrossRadius;
    float dMin = FLT_MAX;
    int iMin = -1;
    for( auto i=myWaBuf.begin(); i!=myWaBuf.end(); ++i ) {
		// Quick rejection test
        float l = Dist(p.x,p.y,i->x,i->y)-r;
        if( l<=uMin ) {
			// Mark is a candidate.  Compute minimum distance to it.
            float d = SmallMark::minCrossDist(myCrossRadius,p.x-i->x,p.y-i->y);
            if( d<dMin ) {
                iMin = i-myWaBuf.begin();
                dMin = d;
            }
        }
    }
    if( dMin<=MaxSelectD ) {
        myPreciseSelectedWa = myWaBuf[iMin];
    } else {
		// Closest mark was too far for consideration.
        myPreciseSelectedWa.x = myPreciseSelectedWa.x = -1;
        iMin = -1;
    }
    return iMin;
}

void WaPlot::doMouseDown( NimblePoint p ) {
    int i = preciseSelect(p);
    if( i>=0 ) {
        Assert( size_t(i)<myWas.set.size() );
        auto j = myWas.set.begin();
        advance(j,i);
        float pitch, duration;
        j->first.getPitchAndDuration(pitch,duration);
        for( auto k=myWackIds.begin(); k!=myWackIds.end(); ++k )
            if( j->second[k->second] ) {
                PlayWa( k->first, pitch, duration );
            }
     }

#if 0
    search myWas to find wack ids
    compute approximate pitch/duration
    for each wack id
        play
#endif
}

void WaPlot::doMouseMove( NimblePoint p ) {
    preciseSelect(p);
}

Clickable::action WaPlot::doMouseUp( NimblePoint p ) {
    return none;
}
