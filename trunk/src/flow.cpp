//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#include <chrono>
#include <future>
#include <iostream>

#include "pigpiomgr.h"
#include "gpiopin.h"
#include "flow.h"
#include "settings.h"
#include "timing.h"

#include "singleton.h"
#include "logger.h"

// Using the Digmesa FHKSC 932-9521-B flow sensor with 1.2mm diameter bore
// With flow sensor in situ, pumping fresh water, measured:
//  count=1033, weight=248g = 4165 counts/l
//  count=1272, weight=316g = 4025 counts/l
//  average of these: 4095 counts/l
// Manufacturer data suggests 1925 pulses/l and we trigger on both rising and
// falling edge which equates to 3850 pulses/l, about 6% difference

//-----------------------------------------------------------------------------

Flow::Flow() 
    :_opened( false )
    ,_state( State::Stopped )
    ,_samplingRate( 50 ) // Gaggia pump works with 50Hz/2 = 25Hz = 40ms, so limit flow measure to the pump intervall
    ,_speedSamplingRate( 500 ) // Speed sampling must be higher, because it is highly affected by noise
    ,_timeout( 1000 )
    ,_count( 0 )
    ,_flowSpeed( 0.0 )
    ,_milliLitrePerCounts( 0.229247353 )
    ,_run( false )    
{
    _flowPin = nullptr;
    _open();
}

//-----------------------------------------------------------------------------

Flow::~Flow() {
    _close();
}

//-----------------------------------------------------------------------------

bool Flow::ready() const {
    return _opened;
}

//-----------------------------------------------------------------------------

Flow::State::Value Flow::getState() const {
    if ( !_opened ) {
        return State::Invalid;
    }

    std::lock_guard<std::mutex> lock( _mutex );
    return _state;
}

//-----------------------------------------------------------------------------

double Flow::getMilliLitres() const {
    if ( !_opened ) {
        return 0.0;
    }

    std::lock_guard<std::mutex> lock( _mutex );
    //return _flowVolume;
    return _milliLitrePerCounts * static_cast<double>( _count );
}

//-----------------------------------------------------------------------------

double Flow::getFlowSpeed() const {
    if ( !_opened ) {
        return 0.0;
    }

    std::lock_guard<std::mutex> lock( _mutex );
    return _flowSpeed;
}

//-----------------------------------------------------------------------------

void Flow::_open() {
    if ( !Singleton<PIGPIOManager>::ready() ) {
        return;
    }

    _flowPin = new GPIOPin( FLOW_PIN );

    if ( !_flowPin->ready() ) {
        LogError("Flow GPIO-Pin could not be opened");
        _close();
        return;
    }
    
    if ( !_flowPin->setOutput( false ) ) {
        LogError("Flow GPIO-Pin could not be set as input");
        _close();
        return;
    }

    if ( !_flowPin->setEdgeTrigger( GPIOPin::Both ) ) {
        LogError("Flow GPIO-Pin could not register edge trigger");
        _close();
        return;
    }

    if ( !_flowPin->edgeFuncRegister( std::bind( &Flow::_alertFunction, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) ) ) {
        LogError("Flow GPIO-Pin could not register callback");
        _close();
        return;
    }

    _run = true;
    _thread = std::thread( &Flow::_worker, this );
    _opened = true;
}

//-----------------------------------------------------------------------------

void Flow::_close() {
    if ( _run ) {
        _run = false;
        _thread.join();
    }
    
    if ( _flowPin ) {
        delete _flowPin;
    }
}

//-----------------------------------------------------------------------------

void Flow::_worker() {
    bool flowing = false;

    unsigned int speedTimer = 0;
    unsigned int startCount = 0;

    unsigned int idleTime = 0;
    unsigned int count = 0;
    unsigned int oldCount = _count;
    
    while ( _run ) {
        delayms( _samplingRate );

        speedTimer += _samplingRate;
        const bool wasFlowing = flowing;

        // Update state
        {
            std::lock_guard<std::mutex> lock( _mutex );

            //const unsigned int countSinceLastSample = _count - oldCount;
            //const double flowSinceLastSample = ( static_cast<double>( countSinceLastSample ) * _milliLitrePerCounts );
            
            //flowCounter += flowSinceLastSample;
            //speedTimer += _samplingRate;

            //_flowVolume = _milliLitrePerCounts * static_cast<double>( _count );
            count = _count;

            if ( speedTimer >= _speedSamplingRate ) {
                const unsigned int countSinceLastSample = _count - startCount;
                _flowSpeed = ( countSinceLastSample / _speedSamplingRate ) * 1000.0;
                
                startCount = _count;
                speedTimer = 0;
            }
        }
        
        if ( count != oldCount ) {
            flowing = true;
            idleTime = 0;
            
            // First time flowing since idle?
            if ( flowing != wasFlowing ) {
                std::lock_guard<std::mutex> lock( _mutex );

                // Start flow measurement
                _state = State::Flowing;
                _count = 0;
                count = 0;
                _flowSpeed = 0;
                speedTimer = 0;
                startCount = 0;
            }
        } 
        else {
            // Count how long the flow sensor has been idle
            idleTime += _samplingRate; 

            // Check if idle time is now reached
            if ( flowing && idleTime >= _timeout ) {
                flowing = false;
            }

            // First time idle since flowing?
            if ( flowing != wasFlowing ) {
                std::lock_guard<std::mutex> lock( _mutex );

                // Stop flow measurement
                _state = State::Stopped;
                _count = 0;
                count = 0;
                _flowSpeed = 0;
                speedTimer = 0;
                startCount = 0;
            }
        }

        oldCount = count;
    }
}

//-----------------------------------------------------------------------------

void Flow::_alertFunction( unsigned pin, bool level, unsigned tick ) {
    std::lock_guard<std::mutex> lock( _mutex );
    ++_count;
}

//-----------------------------------------------------------------------------
