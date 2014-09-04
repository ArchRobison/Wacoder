#ifndef NonblockingQueue_H
#define NonblockingQueue_H

#include <atomic>
#include "AssertLib.h"

//! Nonblocking queue for single producer and single consumer, possibly on different threads.
template<typename T>
class NonblockingQueue {
    std::atomic<unsigned> myPush;   //!< Number of pushes
    std::atomic<unsigned> myPop;    //!< Number of pops
    T* myArray;
    T* myEnd;
    T* myHead;                      //!< Private to popper
    T* myTail;                      //!< Private to pusher
    int myCapacity;
public:
    NonblockingQueue( size_t maxSize ) : myPush(0), myPop(0) {
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
        int nPop = myPop.load(std::memory_order_acquire);
        int nPush = myPush.load(std::memory_order_relaxed);   
        int d = int(nPush-nPop);
        if( d<myCapacity )
            return myTail;
        else
            return 0;
    }
    //! Push slot returned by previous call to startPush.
    void finishPush() {
        auto tmp = myPush.load(std::memory_order_relaxed);     
        myPush.store(tmp+1,std::memory_order_release);
        if( ++myTail==myEnd )
            myTail=myArray;
    }

    // Methods for consumer

    //! Return pointer to slot at head of queue, or return NULL if queue is empty.
    T* startPop() {                   
        int nPush = myPush.load(std::memory_order_acquire);   
        int nPop = myPop.load(std::memory_order_relaxed);
        int d = int(nPush-nPop);            
        Assert(d>=0);
        if( d ) 
            return myHead;
        else
            return 0;
    }
    void finishPop() {
        auto tmp = myPop.load(std::memory_order_relaxed);     
        myPop.store(tmp+1,std::memory_order_release);
        if( ++myHead==myEnd ) 
            myHead=myArray;
    }
};

#endif /* NonblockingQueue */