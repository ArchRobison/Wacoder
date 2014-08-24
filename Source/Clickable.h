#ifndef Clickable_H
#define Clickable_H

#include "NimbleDraw.h"
#include "Widget.h"
#include "Hue.h"

//! Something that can be clicked on.
class Clickable {
public:
	enum action {
		none,
		// Returned by mouseUp when a slider was moved.
		update,
		hide
	};
	void drawOn( NimblePixMap& map, int x, int y );
	//! Returns true if mouse hits, false otherwise.
	bool mouseDown( NimblePoint p );
	void mouseMove( NimblePoint p );
	action mouseUp( NimblePoint p );
	//! True if *this, where more recently drawn, contains *this.
	bool contains( NimblePoint p ) const {
		return myPosition.x<=p.x && p.x-myPosition.x<myClickableSize.x &&
			   myPosition.y<=p.y && p.y-myPosition.y<myClickableSize.y;
	}
private:
	//! Coordinate of upper left corner where most recently drawn.
	NimblePoint myPosition; 
	//! Dimensions of clickable area.
	NimblePoint myClickableSize;
	virtual void doDrawOn( NimblePixMap& map ) = 0;
	virtual void doMouseDown( NimblePoint p ) = 0;
	virtual void doMouseMove( NimblePoint p ) = 0;
	virtual action doMouseUp( NimblePoint p ) = 0;
protected:
	void setClickableSize( int width, int height ) {
		myClickableSize.x=width;
		myClickableSize.y=height;
	}
    int clickableWidth() const {return myClickableSize.x;}
    int clickableHeight() const {return myClickableSize.y;}
};

//! Dialog with a single button that has one of two states.
class ToggleButton: public Clickable, Widget {
public:
	/*override*/ void doDrawOn( NimblePixMap& map );
	/*override*/ void doMouseDown( NimblePoint p );
	/*override*/ void doMouseMove( NimblePoint p );
	/*override*/ action doMouseUp( NimblePoint p );
    virtual void onPress() = 0;
    bool isOn() const {return myButtonIsOn;}
    void setIsOn( bool on ) {myButtonIsOn=on;}
    int width() const {return myPixMap.width();}
    int height() const {return myPixMap.height()>>1;}
protected:
    /** Resource should have "off" image above "on" image. */
    ToggleButton( const char* resourceName ) : 
         Widget(resourceName), 
         myButtonSelected(false),
         myButtonIsOn(false)
    {
    }
private:
    void trackMouse( NimblePoint p );
    NimbleRect myButtonRect;
    bool myButtonSelected;
    bool myButtonIsOn;
    void buildFrom( const NimblePixMap& map );
};

class PermutationDialog: public Clickable {
public:
    virtual size_t size() const = 0;
    virtual const char* name( size_t i ) const = 0;
    virtual int indent( size_t i ) const {return 0;}
    virtual void doMove( size_t from, size_t to ) = 0;
    /*override*/ void drawOn( NimblePixMap& map, int x, int y ) const;
    int width() const {return clickableWidth();}
    int height() const {return clickableHeight();}
    PermutationDialog( int width, int height );
    int hilighted( Hue h ) const {
        Assert(h==HueAccented||h==HueSelected);
        return h==HueAccented ? myAccented : mySelected;
    }
    virtual void clear();
private:
    void doDrawOn( NimblePixMap& map );
	void doMouseDown( NimblePoint p );
	void doMouseMove( NimblePoint p );
	action doMouseUp( NimblePoint p );
    // Height of a row as last drawn
    short myRowHeight;
    // Item to be accented.  -1 if no item is to be accented.
    short myAccented;
    // -1 if no item is selected
    short mySelected;
    // Row before which to insert selected item if mouse goes up.
    // -1 if no item is being moved
    short myInsertPoint;
    // True if mouse is down
    bool myMouseDown;
    short choose( NimblePoint p );
};

#endif /* Clickable_H */