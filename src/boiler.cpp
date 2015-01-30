//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#include "boiler.h"
#include "settings.h"
#include "timing.h"
#include "gpiopin.h"

#include "logger.h"
#include "singleton.h"

//-----------------------------------------------------------------------------

Boiler::Boiler() 
    :_opened( false )
    ,_pwmRange( 20000 )
    ,_pwmFrequency( 10 )
    ,_pwmCurrentPower( 0.0 )
    ,_gpioPin( nullptr )
{
    _open();
}

//-----------------------------------------------------------------------------

Boiler::~Boiler() {
    _close();
}

//-----------------------------------------------------------------------------

bool Boiler::ready() const {
    return _opened;
}

//-----------------------------------------------------------------------------

void Boiler::setPower( double value ) {
    if ( !_opened ) {
        return;
    }
    
    // clamp width to 0% minimum
    if ( value < 0.0 ) {
        value = 0.0;
    }

    // clamp width to 100% maximum
    if ( value > 1.0 ) {
        value = 1.0;
    }
    
    _pwmCurrentPower = value;
    _gpioPin->setPWMDuty( value * _pwmRange );
}

//-----------------------------------------------------------------------------

double Boiler::getPower() const {
    if ( !_opened ) {
        return 0.0;
    }
    
    return _pwmCurrentPower;
}

//-----------------------------------------------------------------------------

void Boiler::_open() {    
    _gpioPin = new GPIOPin( BOILER_PIN );
    
    if ( !_gpioPin->ready() ) {
        LogError("Boiler GPIO-Pin not ready");
        _close();
        return;
    }
    
    if ( !_gpioPin->setOutput( true ) ) {
        LogError("Boiler GPIO-Pin could not be set as output");
        _close();
        return;
    }
    
    if ( !_gpioPin->setPWMFrequency( _pwmFrequency ) ) {
        LogError("Boiler GPIO-Pin PWM set frequency failed");
        _close();
        return;
    }
    
    if ( !_gpioPin->setPWMRange( _pwmRange ) ) {
        LogError("Boiler GPIO-Pin PWM set range failed");
        _close();
        return;
    }
    
    if ( !_gpioPin->setPWMDuty( 0 ) ) {
        LogError("Boiler GPIO-Pin PWM set duty failed");
        _close();
        return;
    }
    
    const unsigned int realFrequency = get_PWM_frequency( BOILER_PIN );
    const unsigned int realRange = get_PWM_real_range( BOILER_PIN );
    
    if ( realFrequency != _pwmFrequency || realRange != _pwmRange ) {
        LogInfo( "Boiler PWM Setup: Frequency = " << realFrequency << ", Range = " << realRange );
    }
    
    _opened = true;
}

//-----------------------------------------------------------------------------

void Boiler::_close() {    
    if ( _gpioPin ) {
        _gpioPin->setState( false );
        delete _gpioPin;
    }
}

//-----------------------------------------------------------------------------
