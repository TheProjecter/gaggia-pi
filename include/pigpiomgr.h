//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#ifndef __PIGPIOMGR_H__
#define __PIGPIOMGR_H__

//-----------------------------------------------------------------------------

#include <iostream>

extern "C" {
    #include <pigpiod_if.h>
    #include <pigpio.h>
}

//-----------------------------------------------------------------------------

/// Singleton class to manage initialisation of PIGPIO
class PIGPIOManager {
public:
    PIGPIOManager();
    ~PIGPIOManager();

    /// Returns true if PIPGIO is available
    bool ready() const;

    /// Returns the PIGPIO version number
    int version() const;

private:
    int _version;  ///< PIGPIO version number (or PI_INIT_FAILED)
};

//-----------------------------------------------------------------------------

#endif // __PIGPIOMGR_H__
