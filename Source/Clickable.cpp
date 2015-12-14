#include "Clickable.h"
#include "Hue.h"

//-----------------------------------------------------------------
// Clickable
//-----------------------------------------------------------------
void Clickable::drawOn( NimblePixMap& map, int x, int y ) {
	myPosition.x = x;
	myPosition.y = y;
	NimblePixMap submap(map,NimbleRect(x,y,map.width(),map.height()));
	doDrawOn(submap);
}

bool Clickable::mouseDown( NimblePoint p ) {
	if( contains(p) ) {
		doMouseDown(p-myPosition);
		return true;
	} else {
		return false;
	}
}

void Clickable::mouseMove( NimblePoint p ) {
    doMouseMove(p-myPosition);
}

Clickable::action Clickable::mouseUp( NimblePoint p ) {
	return doMouseUp(p-myPosition);
}

//-----------------------------------------------------------------
// ToggleButton
//-----------------------------------------------------------------

void ToggleButton::buildFrom( const NimblePixMap& map ) {
    Widget::buildFrom(map);
    int h = map.height()/2;
    setClickableSize(map.width(),h);
    myButtonRect = NimbleRect(1,1,map.width()-1,h-1);
}

void ToggleButton::doDrawOn( NimblePixMap& map ) {
	int w = myPixMap.width();
    int h = myPixMap.height()/2;
    int o = myButtonIsOn ? h : 0;
    NimblePixel blue = NimblePixel(myButtonSelected ? 0xFF : 0x00);
	for( int y=0; y<h; ++y ) {
		const NimblePixel* src = (NimblePixel*)myPixMap.at(0,y+o);
		NimblePixel* dst = (NimblePixel*)map.at(0,y);
        for( int j=0; j<w; ++j ) {
		    dst[j] = src[j] | blue;
        }
    }
}

void ToggleButton::trackMouse( NimblePoint p ) {
    myButtonSelected = myButtonRect.contains(p);
}

void ToggleButton::doMouseDown( NimblePoint p ) {
	trackMouse(p);
}

void ToggleButton::doMouseMove( NimblePoint p ) {
	trackMouse(p);
}

Clickable::action ToggleButton::doMouseUp( NimblePoint p ) {
    if( myButtonRect.contains(p) ) {
        onPress();
    }
	return none;
}

//-----------------------------------------------------------------
// PermutationDialog
//-----------------------------------------------------------------
static Font TheListFont("FontSans16");
static RubberImage TheWordBox("WordBox");

static void Hueify( NimblePixMap& map, Hue hue ) {
    int w = map.width();
    for( int y=0; y<map.height(); ++y ) {
        NimblePixel* p = (NimblePixel*)map.at(0,y);
        for( int x=0; x<w; ++x ) 
            p[x] |= NimblePixel(hue);
    }
}

PermutationDialog::PermutationDialog( int width, int height ) {
    clear();
    setClickableSize(width,height);
}

void PermutationDialog::clear() {
    myAccented = -1;
    mySelected = -1;
    myInsertPoint = -1;
    myMouseDown = false;
    myRowHeight = 0;
}

void PermutationDialog::doDrawOn( NimblePixMap& window ) {
    int w = window.width();
    int h = window.height();
    setClickableSize(w,h);
    myRowHeight = TheListFont.height();
    int indentFactor = 1.618*myRowHeight;
    int n = int(size());
    for( int i=0; i<n; ++i ) {
        const int xmargin = 3;
        const int ymargin = 0;
        int y = i*myRowHeight;
        if( y>=h ) break;   // Quit if run off below bottom of screen
        int l = indent(i)*indentFactor;
        if( l>0 ) {
            window.draw(NimbleRect(0,y,l,y+myRowHeight), NimblePixel::black);
        }
        TheWordBox.drawOn(window,NimbleRect(l,y,w,y+myRowHeight));
        // Create box for the word, clipping on bottom if necessary.
        NimblePixMap box(window,NimbleRect(l,y,w,Min(y+myRowHeight,h))); 
        if( i==mySelected || i==myAccented ) {
            Hue h = HueNone;
            if( i==mySelected )
                h |= HueSelected;
            if( i==myAccented ) 
                h |= HueAccented;
            Hueify(box,h);
        }
        TheListFont.drawOn( box, xmargin, ymargin, name(i), NimblePixel::white );
    }
    if( n*myRowHeight<h ) 
        window.draw(NimbleRect(0,n*myRowHeight,w,h), NimblePixel::black);
    if( myInsertPoint!=-1 ) {
        int y = myInsertPoint*myRowHeight;
        NimblePixMap box(window,NimbleRect(0,Max(0,y-2),w,y+2));
        Hueify(box,HueAccented);
    }
}

short PermutationDialog::choose( NimblePoint p ) {
    if( unsigned(p.x)<unsigned(width()) && unsigned(p.y)<unsigned(height()) ) {
        short i = p.y / myRowHeight;
        if( i<size() ) 
            return i;
    }
    return -1;
}

void PermutationDialog::doMouseDown( NimblePoint p ) {
    int k = choose(p);
    myAccented = k;
    if( k>=0 ) {
        if( k==mySelected )
            mySelected = -1;
        else
            mySelected = choose(p);
    }
    myMouseDown = true;
}

void PermutationDialog::doMouseMove( NimblePoint p ) {
    if( myMouseDown ) {
        if( myAccented!=-1 ) {
            myInsertPoint = Clip(0, int(size()), (p.y+myRowHeight/2) / myRowHeight);
        }
    } else {
        myAccented = choose(p);
    }
}

Clickable::action PermutationDialog::doMouseUp( NimblePoint p ) {
    if( myAccented!=-1 && myInsertPoint!=-1 && myInsertPoint!=myAccented ) {
        doMove( myAccented, myInsertPoint );
        myAccented = -1;
        mySelected = -1;
    }
    myInsertPoint = -1;
    myMouseDown = false;
    return none;
}
