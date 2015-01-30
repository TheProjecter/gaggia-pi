//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#include "gpiopin.h"
#include "timing.h"

#include "pigpiomgr.h"
#include "logger.h"
#include "singleton.h"

using namespace std;

//-----------------------------------------------------------------------------

GPIOPin::GPIOPin( unsigned pin )
    :_pin( pin )
    ,_opened( false )
    ,_output( false )
    ,_state( false )
    ,_edge( Rising )
    ,_edgeFunc( nullptr )
    ,_callbackId( -1 )
{
    _open();
}

//-----------------------------------------------------------------------------

GPIOPin::~GPIOPin() {
    _close();
}

//-----------------------------------------------------------------------------

bool GPIOPin::setOutput( bool output ) {
    if ( !_opened ) {
		return false;
	}

    // set pin to input/output as appropriate
    if ( set_mode( _pin, output ? PI_OUTPUT : PI_INPUT ) != 0 ) {
		return false;
	}

    // read pin state: when the pin is set as an output, this may be
    // unreliable, but decided it's better not to enforce the pin
    // being low/high initially (should be sensibly initialised by the
    // caller in any case)
    _state = getState();

    // store input/output state
    _output = output;

    return true;
}

//-----------------------------------------------------------------------------

bool GPIOPin::setState( bool state ) {
    if ( !_opened ) {
		return false;
	}

	int result = gpio_write( _pin, state ? 1 : 0 );
    
	if ( result < 0 ) {
		switch ( result ) {
			case PI_BAD_GPIO: {
				LogError("GPIOPin::setState - PI_BAD_GPIO");
				break;
			}
			case PI_BAD_LEVEL: {
				LogError("GPIOPin::setState - PI_BAD_LEVEL");
				break;
			}
			case PI_NOT_PERMITTED: {
				LogError("GPIOPin::setState - PI_NOT_PERMITTED");
				break;
			}
			default: {
				LogError("GPIOPin::setState - UNKNOWN");
			}
		}

		return false;
	}
	
    _state = state;

	return true;
}

//-----------------------------------------------------------------------------

bool GPIOPin::setPWMDuty( unsigned int duty ) {
	if ( !_opened ) {
		return false;
	}

    if ( set_PWM_dutycycle( _pin, duty ) < 0 ) {
	    return false;
	}
    
    return true;
}

//-----------------------------------------------------------------------------

bool GPIOPin::setPWMRange( unsigned int range ) {
    if ( !_opened ) {
		return false;
	}

	if ( set_PWM_range( _pin, range ) < 0 ) {
	    return false;
	}
    
    return true;
}

//-----------------------------------------------------------------------------

bool GPIOPin::setPWMFrequency( unsigned frequency ) {
    if ( !_opened ) {
		return false;
	}

    if ( set_PWM_frequency( _pin, frequency ) < 0 ) {
		return false;
	}
    
    return true;
}

//-----------------------------------------------------------------------------

bool GPIOPin::getState() const {
	if ( !_opened ) {
		return false;
	}

    if ( _output ) {
        // pin is set as an output: return the last state set by caller
        return _state;
    } else {
        // pin is set as an input: read the actual pin state
        return ( gpio_read( _pin ) != 0 );
    }
}

//-----------------------------------------------------------------------------

bool GPIOPin::usPulse( bool state, unsigned us ) {
    if ( !setState( state ) ) {
		return false;
    }
    
    delayus( us );
    
    if ( !setState( !state ) ) {
		return false;
    }
    
    return true;
}

//-----------------------------------------------------------------------------

bool GPIOPin::msPulse( bool state, unsigned ms ) {
    if ( !setState( state ) ) {
		return false;
    }
    
    delayms( ms );
    
    if ( !setState( !state ) ) {
		return false;
    }
    
    return true;
}

//-----------------------------------------------------------------------------

bool GPIOPin::setEdgeTrigger( GPIOPin::Edge edge ) {
    if ( !_opened || _edgeFunc ) {
        return false;
    }
    
    _edge = edge;
    return true;
}

//-----------------------------------------------------------------------------

bool GPIOPin::edgeFuncRegister( EdgeFunc edgeFunc ) {
    // check that the device is open
    if ( !_opened ) {
		return false;
    }

    // remove any existing function
    edgeFuncCancel();

    // local static function used to forward events to the parent class
    struct local {
        static void callback( unsigned gpio, unsigned level, uint32_t tick, void* userData ) {
            GPIOPin *self = reinterpret_cast<GPIOPin*>(userData);
            if ( self != 0 ) {
				self->_callback( gpio, (level != 0), static_cast<unsigned>(tick) );
			}
        }
    };

    // install the function
    _edgeFunc = edgeFunc;

    // register a callback
    unsigned edge = static_cast<unsigned>(_edge);
    _callbackId = callback_ex( _pin, edge, local::callback, this );

    return true;
}//edgeFuncRegister

//-----------------------------------------------------------------------------

void GPIOPin::edgeFuncCancel() {
    // cancel the pigpiod callback
    if ( _callbackId >= 0 ) {
        callback_cancel( _callbackId );
        _callbackId = -1;
    }

    // remove the user function
    _edgeFunc = nullptr;
}//edgeFuncCancel

//-----------------------------------------------------------------------------

bool GPIOPin::poll( unsigned timeout ) {
    // check that the device is open
    if ( !_opened ) {
		return false;
	}

    // timeout in seconds
    double seconds = static_cast<double>(timeout) / 1000.0;

    // wait for specified edge
    unsigned edge = static_cast<unsigned>(_edge);
    return ( wait_for_edge( _pin, edge, seconds ) == 1 );
}//poll

//-----------------------------------------------------------------------------

bool GPIOPin::_open() {
    _opened = Singleton<PIGPIOManager>::ready();
    _edge = Rising;
    return _opened;
}

//-----------------------------------------------------------------------------

void GPIOPin::_close() {
    // close any callback function
    edgeFuncCancel();

    // always set back to an input when closed
    setOutput( false );

    _opened = false;
}

//-----------------------------------------------------------------------------

void GPIOPin::_callback( unsigned pin, bool level, unsigned tick) {
    if ( _edgeFunc ) {
		_edgeFunc( pin, level, tick );
	}
}

//-----------------------------------------------------------------------------

bool GPIOPin::ready() const {
    return _opened;
}

//-----------------------------------------------------------------------------
