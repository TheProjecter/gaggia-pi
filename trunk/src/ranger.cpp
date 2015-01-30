//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#include <math.h>
#include <functional>

#include "gpiopin.h"
#include "pigpiomgr.h"
#include "settings.h"
#include "timing.h"
#include "ranger.h"

#include "logger.h"
#include "singleton.h"

//-----------------------------------------------------------------------------

Ranger::Ranger() 
    :_opened( false )
    ,_timeLastRun( 0 )
    ,_range( 0.0 )
    ,_count( 0 )
    ,_timeout( 60 )
    ,_echoPin( 0 )
    ,_triggerPin( 0 )
    ,_run( false )
{
    _timeStamp[0] = 0;
    _timeStamp[1] = 0;

    _open();
}

//-----------------------------------------------------------------------------

Ranger::~Ranger() {
    _close();    
}

//-----------------------------------------------------------------------------

bool Ranger::ready() const {
    return _opened;
}

//-----------------------------------------------------------------------------

bool Ranger::getRange( double& range ) const {
    if ( !_opened )  {
        return false;
    }

    std::lock_guard<std::mutex> lock( _rangeMutex );
    range = _range;
    return _range > 0.0;
}

//-----------------------------------------------------------------------------

void Ranger::_open() {
    if ( !Singleton<PIGPIOManager>::ready() ) {
        return;
    }

    _echoPin = new GPIOPin( RANGER_ECHO_IN );

    if ( !_echoPin->ready() ) {
        LogError("Ranger GPIO-Pin 'echo' could not be opened");
        _close();
        return;
    }
    
    if ( !_echoPin->setOutput( false ) ) {
        LogError("Ranger GPIO-Pin 'echo' could not be set as input");
        _close();
        return;
    }

    _triggerPin = new GPIOPin( RANGER_TRIGGER_OUT );

    if ( !_triggerPin->ready() ) {
        LogError("Ranger GPIO-Pin 'trigger' could not be opened");
        _close();
        return;
    }

    if ( !_triggerPin->setOutput( true ) ) {
        LogError("Ranger GPIO-Pin 'trigger' could not be set as output");
        _close();
        return;
    }

    if ( !_echoPin->setEdgeTrigger( GPIOPin::Both ) ) {
        LogError("Could not register edge trigger for echo pin");
        _close();
        return;
    }

    if ( !_echoPin->edgeFuncRegister( std::bind( &Ranger::_alertFunction, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) ) ) {
        LogError("Could not register callback for echo pin");
        _close();
        return;
    }
    
    // Record current time as last run time as initialzation
    _timeLastRun = getClock();

    _opened = true;
    _run = true;
    _thread = std::thread( &Ranger::_worker, this );
    
    double range = 0.0;
    bool success = false;

    // Wait a bit for first data (sampling is set to 10Hz)
    for ( size_t index = 0; !success && index < 3; ++index ) {
        delayms( 100 );
        success = getRange( range );
    }
}

//-----------------------------------------------------------------------------

void Ranger::_close() {
    if ( _run ) {
        _run = false;
        _thread.join();
    }

    if ( _echoPin ) {
        delete _echoPin;
    }

    if ( _triggerPin ) {
        delete _triggerPin;
    }
}

//-----------------------------------------------------------------------------

void Ranger::_worker() {
    double currentRange = 0.0;
    double oldRange = 0.0;
    bool firstTime = true;
    const double k = 0.5;

    while (_run) {
        oldRange = currentRange;

        // Take a range measurement (will block)
        currentRange = _measureRange();

        // Does this measurement look dubious?
        bool outlier =
            ( !firstTime && (fabs( currentRange - oldRange ) > 0.01) ) ||
            ( currentRange < 0.001 );

        // Have another attempt
        if ( outlier ) {
            currentRange = _measureRange();
        }

        {
            // lock the mutex
            std::lock_guard<std::mutex> lock( _rangeMutex );

            // initialisation of filter
            if ( firstTime ) {
                _range = currentRange;
                firstTime = false;
            }

            // store filtered value
            _range = _range + k * ( currentRange - _range );
        }
    }
}

//-----------------------------------------------------------------------------

double Ranger::_measureRange() {
    // Minimum time (in seconds) between successive calls
    // This is to prevent the ranger from being triggered too frequently
    const double minimumInterval = 0.1;

    // The speed of sound in mm/s
    const long speedSound_mms = 340270;

    // Calculate interval since we were last run
    const double interval = getClock() - _timeLastRun;
    if ( interval < minimumInterval ) {
        // Delay if needed to avoid sending a trigger when echoes from the
        // previous trigger are still incoming
        delayms( 1000.0 * ( minimumInterval - interval ) );
    }

    _timeLastRun = getClock();

    // Attempt to get the range
    long us = 0;
    long mm = 0;

    {
        std::lock_guard<std::mutex> lock( _countMutex );
        // Initialise interrupt counter
        _count = 0;
    }

    // Ensure GPIO is low initially
    if ( !_triggerPin->setState( false ) ) {
        return 0.0;
    }

    // Send 10us pulse
    if ( !_triggerPin->usPulse( true, 10 ) ) {
        return 0.0;
    }

    // Polling interval (ms)
    const unsigned pollInterval = 1;

    // Wait for the result to arrive
    bool complete = false;
    unsigned slept = 0;
    do {
        delayms( pollInterval );
        slept += pollInterval;

        std::lock_guard<std::mutex> lock( _countMutex );
        us = _timeStamp[1] - _timeStamp[0];
        complete = ( _count == 2 );
    } 
    while ( !complete && (slept < _timeout) );

    if ( complete ) {
        mm = us * speedSound_mms / 2000000;
    }
    else {
        return 0.0;
    }

    // Convert range to m
    const double distance = static_cast<double>(mm) / 1000.0;

    return distance;
}

//-----------------------------------------------------------------------------

void Ranger::_alertFunction( unsigned gpio, unsigned level, uint32_t tick )  {
    std::lock_guard<std::mutex> lock( _countMutex );

    // For the first two interrupts received, store the time-stamp
    if ( _count < 2 ) {
        _timeStamp[_count] = tick;
        ++_count;
    }
}

//-----------------------------------------------------------------------------
