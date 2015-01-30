//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#ifndef __GPIOPIN_H__
#define __GPIOPIN_H__

//-----------------------------------------------------------------------------

#include <functional>

#include "pigpiomgr.h"

//-----------------------------------------------------------------------------

class GPIOPin {
public:
    GPIOPin( unsigned pin );
    virtual ~GPIOPin();

    /// Supported edge trigger modes
    enum Edge {
        Falling = FALLING_EDGE,
        Rising  = RISING_EDGE,
        Both    = EITHER_EDGE
    };

    /// Set the pin to be an output (true) or input (false)
    bool setOutput( bool output );

    /// Set the pin state
    bool setState( bool state );

    /// Set PWM duty cycle on a pin (0 to range)
    bool setPWMDuty( unsigned int duty );
    
    /// Set PWM range
    bool setPWMRange( unsigned int range );

    /// Set PWM frequency on a pin (nearest match is used)
    bool setPWMFrequency( unsigned frequency );

    /// Get the pin state
    bool getState() const;

    /// Pulse high or low for specified number of microseconds
    bool usPulse( bool state, unsigned us );

    /// Pulse high or low for given number of milliseconds
    bool msPulse( bool state, unsigned ms );

    /// Set edge trigger
    bool setEdgeTrigger( Edge edge );

    /// Notification function type
    typedef std::function<void(
        unsigned pin,
        bool     level,
        unsigned tick
    )> EdgeFunc;

    /// Set edge notification
    bool edgeFuncRegister( EdgeFunc edgeFunc );

    /// Cancel edge notification
    void edgeFuncCancel();

    /// Poll the pin with timeout
    bool poll( unsigned timeout );

    /// Is the pin ready (is it configured successfully?) 
    bool ready() const;

protected:
    /// Open the pin for IO
    bool _open();

    /// Close the pin
    void _close();

private:
    /// Called when an edge event is received
    void _callback( unsigned pin, bool level, unsigned tick );

private:
    unsigned _pin;         ///< GPIO pin number
    bool     _opened;      ///< Successfully opened
    bool     _output;      ///< Is the pin set as an output?
    bool     _state;       ///< In output mode, was the pin set high or low?
    Edge     _edge;        ///< Edge trigger mode

    EdgeFunc _edgeFunc;    ///< Edge function
    int      _callbackId;  ///< Callback function identifier
};

//-----------------------------------------------------------------------------

#endif // __GPIOPIN_H__
