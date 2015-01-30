//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#include "pump.h"
#include "settings.h"
#include "timing.h"
#include "gpiopin.h"

#include "logger.h"
#include "singleton.h"

//-----------------------------------------------------------------------------

Pump::Pump() 
    :_opened( false )
	,_power( false )
    ,_gpioPin( nullptr )
{
	_open();
}

//-----------------------------------------------------------------------------

Pump::~Pump() {
	_close();
}

//-----------------------------------------------------------------------------

bool Pump::ready() const {
	return _opened;
}

//-----------------------------------------------------------------------------

void Pump::setPower( bool power ) {
    if ( !_opened ) {
		return;
    }
    
	_gpioPin->setState( power );
	_power = power;
}

//-----------------------------------------------------------------------------

bool Pump::getPower() const {
    if ( !_opened ) {
		return false;
    }
    
    return _power;
}

//-----------------------------------------------------------------------------

void Pump::_open() {    
    _gpioPin = new GPIOPin( PUMP_PIN );
    
    if ( !_gpioPin->ready() ) {
		LogError("Pump GPIO-Pin not ready");
		_close();
		return;
    }
    
    if ( !_gpioPin->setOutput( true ) ) {
		LogError("Pump GPIO-Pin could not be set as output");
		_close();
		return;
    }
    
    _opened = true;
}

//-----------------------------------------------------------------------------

void Pump::_close() {    
	if ( _gpioPin ) {
		_gpioPin->setState( false );
		delete _gpioPin;
	}
}

//-----------------------------------------------------------------------------
