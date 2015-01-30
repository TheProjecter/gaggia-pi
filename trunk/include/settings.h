//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

//-----------------------------------------------------------------------------

#include <fstream>
#include <mutex>

//-----------------------------------------------------------------------------

#define TSIC_PIN 15

// GPIO pin used for DS18B20
#define DS_TEMP_PIN 2

// GPIO pin used for Solid State Relay on Boiler
#define BOILER_PIN 24

// GPIO pin used for the Pump
#define PUMP_PIN 23

// GPIO pin used by flow sensor
#define FLOW_PIN 14

// GPIO pins used by ultrasonic ranger
#define RANGER_TRIGGER_OUT 27
#define RANGER_ECHO_IN 22

//-----------------------------------------------------------------------------

class Settings {
public:
	Settings();
	~Settings();

	bool ready() const;

	void getRegulatorSettings( bool steam, double& iGain, double& pGain, double& dGain, double& targetTemperature ) const;
	void getPreHeatingSettings( double& time, double& temperature ) const;
	
	double getFlowOffset30() const;
	double getFlowOffset60() const;

private:
	bool _open();
	void _close();
	void _loadDefaults();

	// Regulator settings
	double _iDefaultGain;
    double _pDefaultGain;
    double _dDefaultGain;
	double _iSteamGain;
    double _pSteamGain;
    double _dSteamGain;

	double _defaultTargetTemperature;
	double _steamTargetTemperature;
	double _preHeatingTargetTemperature;
	double _preHeatingTime;
	
	double _flowOffset30;
	double _flowOffset60;

	bool _opened;
	std::mutex* _mutex;

}; // Settings

//-----------------------------------------------------------------------------

#endif // __SETTINGS_H__
