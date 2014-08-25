#include <bitset>
#include <map>
#include <vector>
#include <climits>
#include "NimbleDraw.h"
#include "Clickable.h"
#include "LinearTransform1D.h"
#include "Hue.h"

//! Must be supplied by client of WaPlot
extern void PlayWa( const std::string& waSetName, float freq, float duration );

//! Plot of marks in "Wa-space"
class WaPlot: public Clickable {
    //! A point in the WaPlot area.
    struct waPoint {
        //! Coordinates of the mark in world coordinates
        float x, y;
        /** Set coordinates of this point.
            x coordinate is log(pitch*duration), y coordinate is log(pitch).
            Thus vertical changes preserve the total recorded waveform. */
        void setFromPitchAndDuration( float pitch, float duration ); 
        void getPitchAndDuration( float& pitch, float& duration ) const;
    };

    //! Mark denoting a note
    struct noteMark: waPoint {
        //! Radius (proportional to volume)
        signed char r;
        bool friend operator<( const noteMark& a, const noteMark& b ) {
            return a.y!=b.y ? a.y<b.y : 
                   a.x!=b.x ? a.x<b.x :
                   a.r<b.r;
        }
    };

    //! Mark denoting a Wa
    struct waMark: waPoint {
        bool friend operator<( const waMark& a, const waMark& b ) {
            return a.y!=b.y ? a.y<b.y : a.x<b.x;
        }
    };

	typedef std::map<std::string, int> waSetIdMapType;
	//! Map from waSet names to ids
	waSetIdMapType myWaSetIds;

    // Set of marks and their associated tracks/waSets
	template<typename Mark, size_t MaxId>
	struct collection {
		static const size_t maxId = MaxId;
		typedef std::map<Mark,std::bitset<maxId> > setType;
		setType set;
		HueMap chosen;
		void clear() {
			set.clear();
			chosen.clear();
		}
	};

    //! Collection of notes
	collection<noteMark,64> myNotes;

    //! Collection of was
	collection<waMark,32> myWas;

	//! Screen locations of most recently drawn was
	/** Sequence of locations corresponds to sequence of items in myWas */
	std::vector<NimblePoint> myWaBuf;

	//! Location of precisely selected Wa, or (-1,-1) if there is no precisely selected Wa
	NimblePoint myPreciseSelectedWa;

    void drawNotesAndWas( NimblePixMap& window );
#if 0 /* Remove ? */
    void drawWas( NimblePixMap& window );
#endif
    struct limits {
        float min, max;
        void clear() {min=FLT_MAX; max=FLT_MIN;}
        void include( float z ) {
            if( z<min ) min = z;
            if( z>max ) max = z;
        }
    };
    limits xLimit, yLimit, rLimit;
    short myCrossRadius;
    bool haveLiveWa;
    waMark liveWa;
	/*override*/void doDrawOn( NimblePixMap& map );
	/*override*/void doMouseDown( NimblePoint p );
	/*override*/void doMouseMove( NimblePoint p );
	/*override*/action doMouseUp( NimblePoint p );
    //! Returns index into myWas if a wa was selected, or -1.
    int preciseSelect( NimblePoint p );
public:
    WaPlot() : myCrossRadius(16) {
        clear();
    }
    void clear();
    void insertNote( float pitch, float duration, int velocity, int trackId );
    void setTrackHues( HueMap h ) {
        myNotes.chosen = h;
    }
	void setWaSetHues( HueMap h ) {
        myWas.chosen = h;
	}
    //! Given name of a WaSet, get corresponding index (or create one)
	int getWaSetId( const std::string& waSetName );
    void insertWa( float pitch, float duration, int waSetId );
    void setDynamicWa( float pitch, float duration );
	void setWindowSize( int width, int height ) {
		setClickableSize(width,height);
	}
    void drawOn( NimblePixMap& window ); 
};
