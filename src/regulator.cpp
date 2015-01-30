//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#include <unistd.h>

#include "regulator.h"
#include "timing.h"
#include "logger.h"
#include "singleton.h"

//-----------------------------------------------------------------------------

Regulator::Regulator(Boiler* boiler, TSIC* tsic) 
	:_opened( false )
    ,_run( false )
    ,_power( false )
    ,_timeStep( 1.0 )
    ,_latestTemp( 20.0 )
    ,_latestPower( 0.0 )
    ,_temperature( tsic )
    ,_boiler( boiler )
	,_targetTemperature( 95.0 )
    ,_dState( 0.0 )
    ,_iState( 0.0 )
    ,_iMax(  1.0 )
    ,_iMin( -1.0 )
    ,_iGain( 0.0 )
    ,_pGain( 1.0 )
    ,_dGain( 0.0 )
{
	_open();
}

//-----------------------------------------------------------------------------

Regulator::~Regulator() {
    _close();
}

//-----------------------------------------------------------------------------

bool Regulator::ready() const {
    return _opened;
}

//-----------------------------------------------------------------------------

void Regulator::setPIDGains( double pGain, double iGain, double dGain ) {
	if ( !_opened ) {
		return;
	}

	std::lock_guard<std::mutex> lock( *_mutex );

	_pGain = pGain;
	_iGain = iGain;
	_dGain = dGain;
}

//-----------------------------------------------------------------------------

void Regulator::setTargetTemperature( double targetTemperature ) {
	if ( !_opened ) {
		return;
	}

	std::lock_guard<std::mutex> lock( *_mutex );

	_targetTemperature = targetTemperature;
}

//-----------------------------------------------------------------------------

double Regulator::getTargetTemperature() const {
	if ( !_opened ) {
		return 0.0;
	}

	std::lock_guard<std::mutex> lock( *_mutex );

	return _targetTemperature;
}

//-----------------------------------------------------------------------------

void Regulator::setPower( bool power ) {
	if ( !_opened ) {
		return;
	}

    std::lock_guard<std::mutex> lock( *_mutex );
    _power = power;
}

//-----------------------------------------------------------------------------

bool Regulator::getPower() const {
	if ( !_opened ) {
		return false;
	}

    std::lock_guard<std::mutex> lock( *_mutex );
    return _power;
}

//-----------------------------------------------------------------------------

void Regulator::_open() {   
	if ( !_boiler->ready() ) {
		LogError("Boiler controller not ready");
		_close();
		return;
    }

	if ( !_temperature->ready() ) {
		LogError("Temperature sensor not ready");
		_close();
		return;
    }

	// Set regulator properties
	_pGain = 0.07;
	_iGain = 0.05;
	_dGain = 0.90;

	_iMin = 0.0;
	_iMax = 1.0;

	_timeStep = 1.0;
	_targetTemperature = 93.0;

	_opened = true;
    _run = true;
	_mutex = new std::mutex();
    _thread = new std::thread( &Regulator::_worker, this );
}

//-----------------------------------------------------------------------------

void Regulator::_close() {    
	_run = false;

	if ( _thread ) {
		_thread->join();
		delete _thread;
	}

	if ( _mutex ) {
		delete _mutex;
	}
}

//-----------------------------------------------------------------------------

double Regulator::_update( double error, double position, double iGain, double pGain, double dGain ) {	
    // Calculate proportional term
    double pTerm = pGain * error;

    // Calculate integral state with appropriate limiting
    _iState += error;
    if ( _iState > _iMax ) {
	    _iState = _iMax;
	}
    else if ( _iState < _iMin ) {
	    _iState = _iMin;
	}

    // Calculate integral term
    double iTerm = iGain * _iState;

    // Calculate derivative term
    double dTerm = dGain * (_dState - position);
    _dState = position;

    return pTerm + dTerm + iTerm;
}

//-----------------------------------------------------------------------------

void Regulator::_worker() {
    // Start time and next time step
    double start = getClock();
    double next  = start;
            
    while ( _run ) {
		// get elapsed time since start
		//double elapsed = getClock() - start;

		// take temperature measurement
		double latestTemp = 0.0;
	
		if ( !_temperature->getDegrees( latestTemp ) ) {
			latestTemp = 0.0;
		}

		// boiler drive (duty cycle)
		double drive = 0.0;

		// if the temperature is near zero, we assume there's an error
		// reading the sensor and drive (duty cycle) will be zero
		if ( _power && latestTemp > 0.5 ) {
			// lock shared data before use
			std::lock_guard<std::mutex> lock( *_mutex );

			// calculate next time step
			next += _timeStep;

			// calculate PID update
			drive = _update( _targetTemperature - latestTemp, latestTemp, _iGain, _pGain, _dGain );
		}

		// clamp the output power to sensible range
		if ( drive > 1.0 ) {
			drive = 1.0;
		} 
		else if ( drive < 0.0 ) {
			drive = 0.0;
		}	

		// Set the boiler power (uses pulse width modulation)
		_boiler->setPower( drive );

		// Store the latest temperature reading
		{
			std::lock_guard<std::mutex> lock( *_mutex );
			_latestTemp  = latestTemp;
			_latestPower = drive;
		}

		// sleep for the remainder of the time step
		double remain = next - getClock();
		if ( remain > 0.0 ) {	  
			delayms( static_cast<int>(1.0E3 * remain) );
		}
    };

    // Ensure the boiler is turned off before exit
	_boiler->setPower( 0.0 );
}

//-----------------------------------------------------------------------------
