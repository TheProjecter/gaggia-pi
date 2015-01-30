//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#ifndef __SINGLETON_H__
#define __SINGLETON_H__

//-------------------------------------------------------------------------------------------------

#include <assert.h>

//-------------------------------------------------------------------------------------------------

template <typename C> 
class Singleton {
public:
    // initialization
    static void initialize(C* inst) {
        assert(!_instance && "singleton already initialized");
        //static Guard g;
        _instance = inst;
    }

    // deinitialization
    static void deinitialize() {
        assert(_instance && "singleton never initialized");
        delete _instance;
        _instance = nullptr;
    }

    // get the instance
    static C& reference() {
        assert(_instance && "access to an uninitialized singleton class");
        return *_instance;
    }

    // get pointer
    static C* pointer() {
        assert(_instance && "access to an uninitialized singleton class");
        return _instance;
    }

    // Return true if singleton is initialized
    static bool ready() {
        return ( _instance != nullptr );
    }
    
private:
    static C* _instance;
};

//-------------------------------------------------------------------------------------------------

template <typename C> C* Singleton <C>::_instance = 0;

//-------------------------------------------------------------------------------------------------

#endif // __SINGLETON_H__
