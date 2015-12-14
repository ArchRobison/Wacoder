#ifndef PoolAllocator_H
#define PoolAllocator_H

#include <cstddef>
#include <cstring>

//! No-frills allocator class suitable for real-time use.
template<typename T>
class PoolAllocator {
    //! Pointer to beginning of block from which to allocate objects of type T
    T* begin;
    //! End of the block
    T* end;
    //! Pointer within block.  Items in [avail,end) have never been allocated yet.
    T* avail;
    //! Pointer to list of free items
    T* free;
    static T*& next(T* item) {return *(T**)(void*)item;}
public:
    PoolAllocator( size_t maxSize, bool freeWhenDestroyed=true ) {
        Assert( sizeof(T)>=sizeof(T*) );
        T* p = (T*)operator new( sizeof(T)*maxSize );
        end = p + maxSize;
        avail = p;
        free = NULL;
        begin = freeWhenDestroyed ? p : NULL;
    }
    ~PoolAllocator() {
        if( begin ) {
#if ASSERTIONS
            size_t k = 0;
            for( T* p=free; p; p=next(p) )
                ++k;
            //! Failure of following assertion indicates that pool was freed before all items in it were destroyed.
            Assert( k==avail-begin );
#endif 
            operator delete(begin);
        }
    }
    //! Allocate raw memory for a T and return a pointer to it.  Return NULL if out of space.  Takes O(1) time.
    T* allocate() {
        T* result = NULL;
        if( free ) {
            // Return pointer to previously destroyed item.
            result = free;
            free = next(free);
        } else if( avail<end ) {
            // Return ponter to fresh memory.
            result = avail++;
        } 
        return result;
    }
    //! Call destructor for *x and deallocate it.   Deallocation takes O(1) time.
    void destroy( T* x ) {
        Assert( begin<=x && x<end );
        x->~T();
#if ASSERTIONS
        std::memset( x, 0xcd, sizeof(T) );
#endif
        next(x) = free;
        free = x;
    }
};

#endif /* PoolAllocator_H */