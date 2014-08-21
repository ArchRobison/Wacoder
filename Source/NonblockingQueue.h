#ifndef NonblockingQueue_H
#define NonblockingQueue_H

#include "AssertLib.h"

//! Nonblocking queue for single producer and single consumer.
template<typename T>
class NonblockingQueue {
    volatile unsigned myPush;    // Count of pushes.  May wrap.
    volatile unsigned myPop;     // Count of pops.  May wrap.
    T* myArray;
    T* myEnd;
    T* myHead;
    T* myTail;
    int myCapacity;
public:
    NonblockingQueue( size_t maxSize ) {
        myPush = myPop = 0;
        myHead = myTail = myArray = new T[maxSize];
        myEnd = myArray+maxSize;
        myCapacity = maxSize;
    }
    T& tail() {
        return *myTail;
    }
    // Methods for producer

    //! Return pointer to fresh slot, or return NULL if queue is full.
    /** Caller must call finishPush after filling in slot. */
    T* startPush() {
        int d = int(myPush-myPop);          // FIXME - read of myPop needs load-with-acquire semantics
        if( d<myCapacity )
            return myTail;
        else
            return 0;
    }
    //! Push slot returned by previous call to startPush.
    void finishPush() {
        ++myPush;                            // FIXME - write of myPush needs load-with-acquire semantics
        if( ++myTail==myEnd )
            myTail=myArray;
    }

    // Methods for consumer

    //! Return pointer to slot at head of queue, or return NULL if queue is empty.
    T* startPop() {                   
        int d = int(myPush-myPop);            // FIXME - read of myPush needs load-with-acquire semantics
        Assert(d>=0);
        if( d ) 
            return myHead;
        else
            return 0;
    }
    void finishPop() {
        ++myPop;                // FIXME - write of myPop deeds store-with-release semantics
        if( ++myHead==myEnd ) 
            myHead=myArray;
    }
};

#endif /* NonblockingQueue */