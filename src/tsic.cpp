//-----------------------------------------------------------------------------
//
// Copyright (C) 2014-2015 James Ward
//
// This software is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
//-----------------------------------------------------------------------------

#include "tsic.h"
#include <stdio.h>
#include <unistd.h>

#include "pigpiomgr.h"
#include "gpiopin.h"
#include "timing.h"

#include "singleton.h"
#include "logger.h"

//-----------------------------------------------------------------------------

using namespace std;

/// the total number of bits to read from the TSIC sensor
static const unsigned TSIC_BITS = 20;

/// the length of the bit frame used by the TSIC sensor in microseconds
static const unsigned TSIC_FRAME_US = 125;

/// scale factor used to convert sensor values to fixed point integer
static const int SCALE_FACTOR = 1000;

/// minimum temperature for sensor (must match device data)
static const int MIN_TEMP = -50;

/// maximum temperature for sensor (must match device data)
static const int MAX_TEMP = 150;

/// special value used to denote invalid sensor data
static const int INVALID_TEMP = -100000;

//-----------------------------------------------------------------------------

/// calculate parity for an eight bit value
static int parity8( int value ) {
    value = (value ^ (value >> 4)) & 0x0F;
    return (0x6996 >> value) & 1;
}

//-----------------------------------------------------------------------------

// Decode two 9-bit packets from the sensor, and return the temperature.
// Returns either a fixed point integer temperature multiplied by SCALE_FACTOR,
// or INVALID_TEMP in case of error
static int tsicDecode( int packet0, int packet1 ) {
    // strip off the parity bits (LSB)
    int parity0 = packet0 & 1;
    packet0 >>= 1;
    int parity1 = packet1 & 1;
    packet1 >>= 1;

    // check the parity on both bytes
    bool valid =
        ( parity0 == parity8(packet0) ) &&
        ( parity1 == parity8(packet1) );

    // if the parity is wrong, return INVALID_TEMP
    if ( !valid ) {
        LogWarning("TSIC: parity error");
        return INVALID_TEMP;
    }

    // if any of the top 5 bits of packet 0 are high, that's an error
    if ( (packet0 & 0xF8) != 0 ) {
        LogWarning("TSIC: prefix error");
        return INVALID_TEMP;
    }

    // this is our raw 11 bit word */
    int raw = (packet0 << 8) | packet1;

    // convert raw integer to temperature in degrees C
    int temp = (MAX_TEMP - MIN_TEMP) * SCALE_FACTOR * raw / 2047 + MIN_TEMP * SCALE_FACTOR;

    // check that the temperature lies in the measurable range
    if ( (temp >= MIN_TEMP * SCALE_FACTOR) && (temp <= MAX_TEMP * SCALE_FACTOR) ) {
        // all looks good
        return temp;
    } 
    else {
        // parity looked good, but the value is out of the valid range
        return INVALID_TEMP;
    }
}

//-----------------------------------------------------------------------------

TSIC::TSIC( unsigned gpio ) 
    :_pin( 0 )
    ,_gpio( gpio )
    ,_opened( false )
    ,_valid( false )
    ,_temperature( 0.0 )
    //,_callback( -1 )
    ,_count( 0 )
    ,_lastLow( 0 )
    ,_lastHigh( 0 )
    ,_word( 0 )
    ,_running( false )
{
    _open();
}

//-----------------------------------------------------------------------------

TSIC::~TSIC() {
    _close();
}

//-----------------------------------------------------------------------------

void TSIC::_open() {
    if ( !Singleton<PIGPIOManager>::ready() ) {
        LogError("PIGPIOManager not ready, aborting TSIC initialization");
        return;
    }

    _pin = new GPIOPin( _gpio );

    if ( !_pin->ready() ) {
        LogError("TSIC GPIO-Pin could not be opened");
        _close();
        return;
    }
    
    if ( !_pin->setOutput( false ) ) {
        LogError("TSIC GPIO-Pin could not be set as input");
        _close();
        return;
    }

    if ( set_pull_up_down( 15, 2 ) != 0) {
        LogError("Could not register pull down resistor for TSIC pin");
        _close();
        return;
    }

    if ( !_pin->setEdgeTrigger( GPIOPin::Both ) ) {
        LogError("Could not register edge trigger for TSIC pin");
        _close();
        return;
    }

    if ( !_pin->edgeFuncRegister( std::bind( &TSIC::_alertFunction, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) ) ) {
        LogError("Could not register callback for TSIC pin");
        _close();
        return;
    }

    // Wait for a packet to arrive
    bool success = false;
    for ( size_t count = 0; !success & ( count < 10 ); ++count ) {
        // Sample rate is 10Hz, so we need to wait at least 1/10th second
        delayms( 100 );

        // Attempt to read the value
        double value = 0.0;
        success = getDegrees( value );
    }

    // Did we receive some data?
    if ( !success ) {
        LogError("Could not take a sampling reading for TSIC sensor, aborting TSIC initialization");
        _close();
        return;
    }

    // set flag to indicate we are open
    _opened = true;
}

//-----------------------------------------------------------------------------

void TSIC::_close() {
    delete _pin;
}

//-----------------------------------------------------------------------------

bool TSIC::getDegrees( double & value ) const {
    std::lock_guard<std::mutex> lock( _mutex );
    value = _temperature;
    return _valid;
}

//-----------------------------------------------------------------------------

bool TSIC::ready() const {
    return _opened;
}

//-----------------------------------------------------------------------------

void TSIC::_alertFunction( int gpio, int level, uint32_t tick ) {
    if ( level == 1 ) {
        _lastHigh = tick;

        /*if ( _lastLow == 0 ) {
            return;
        }*/

        // Bus went high
        const uint32_t timeLow = tick - _lastLow;

        if ( timeLow < TSIC_FRAME_US / 2 ) {
            // High bit
            _word = (_word << 1) | 1;
        } 
        else if ( timeLow < TSIC_FRAME_US ) {
            // Low bit
            _word <<= 1;
        } 
        else if ( timeLow > TSIC_FRAME_US * 2 ) {
            // Low for more than one frame, which should never happen and
            // must therefore be an invalid bit: start again
            _count = 0;
            _word  = 0;
            return;
        }

        if ( ++_count == TSIC_BITS ) {
            // Decode the packet
            int result = tsicDecode(
                (_word >> 10) & 0x1FF, // packet 0
                _word & 0x1FF          // packet 1
            );

            // Update the temperature value
            {
                // Lock the mutex
                std::lock_guard<std::mutex> lock( _mutex );

                // Update the temperature value and validity flag
                if ( result != INVALID_TEMP ) {
                    _temperature = static_cast<double>( result ) / static_cast<double>( SCALE_FACTOR );
                    _valid = true;
                } 
                else {
                    _valid = false;
                }
            }

            // prepare to receive a new packet
            _count = 0;
            _word = 0;
        }
    } 
    else {
         // bus went low
        _lastLow = tick;

        // calculate time spent high
        const uint32_t timeHigh = tick - _lastHigh;
            
        // If the bus has been high for more than one frame, reset the
        // counters to start a new packet
        if ( timeHigh > TSIC_FRAME_US * 2 ) {
            _count = 0;
            _word  = 0;
        }
    }
}

//-----------------------------------------------------------------------------
