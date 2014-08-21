#include "AssertLib.h"

template<typename T>
class LinearTransform1D {
private:
    T a, b;
    LinearTransform1D( T a_, T b_ ) : a(a_), b(b_) {}
public:
    float operator()( float x ) const {return a*x+b;}
    LinearTransform1D() {}
    // Return transform that maps x0->y0 and x1->y1
    LinearTransform1D( T x0, T y0, T x1, T y1 ) {
        a = (y1-y0)/(x1-x0);
        b = y0 - a*x0;
    }
    // Return inverse transform
    LinearTransform1D inverse() const {
        return LinearTransform(1/a, -b/a);
    }
    // Shorthand for inverse()(x)
    float inverse( T y ) const {
        return (y-b)/a;
    }
};