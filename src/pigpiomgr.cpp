//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#include "pigpiomgr.h"
#include "timing.h"

#include "logger.h"
#include "singleton.h"

//-----------------------------------------------------------------------------

bool PIGPIOManager::ready() const {
    return (_version != PI_INIT_FAILED);
}

//-----------------------------------------------------------------------------

PIGPIOManager::PIGPIOManager() {
    // guessing this is the same return value as gpioInitialise (undocumented)
    _version = pigpio_start( NULL, NULL );
}

//-----------------------------------------------------------------------------

PIGPIOManager::~PIGPIOManager() {
    pigpio_stop();
}

//-----------------------------------------------------------------------------
