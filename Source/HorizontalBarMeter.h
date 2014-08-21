#include "Widget.h"

class HorizontalBarMeter: public Widget {
public:
    /** Resource should have "off" image above "on" image. */
    HorizontalBarMeter( const char* resourceName ) : 
         Widget(resourceName), 
         myLevel(0)
    {
    }
    void drawOn( NimblePixMap& map, int x, int y ) const;
    void setLevel( float level ) {
        Assert(0<=level);
        Assert(level<=1.0f);
        myLevel = level;
    }
    int width() const {return myPixMap.width();}
    int height() const {return myPixMap.height()>>1;}
private:
    float myLevel;
};