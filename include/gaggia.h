//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#ifndef __GAGGIA_H__
#define __GAGGIA_H__

//-----------------------------------------------------------------------------

#include <mutex>
#include <thread>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

//-----------------------------------------------------------------------------
// Forward decls

class TSIC;
class Flow;
class Boiler;
class Pump;
class Regulator;
class Ranger;

class Timer;

//-----------------------------------------------------------------------------

class Gaggia {
public:
    struct State {
		enum Value {
			Invalid,
			Deactivated,       // Regulator deactivated
			Heating,           // Regulator in pre-heating mode
			Active,            // Regulator active in PID mode
			Steam,             // Regulator active in Steam PID mode
			Extracting,        // Extraction by user
			ExtractingOneCup,  // Extracting by system (25 ml)
			ExtractingTwoCups  // Extracting by sytsem (50 ml)
		};
    };

	Gaggia( bool activeHeating = true, bool logging = false );
    ~Gaggia();
    
    bool ready() const;
	void update();

    double getBoilerTemperature() const;
    double getBoilerTargetTemperature() const;
    
    double getWaterTankLevel() const;
    
	double getIdleTime() const;
    double getSystemTime() const;
    double getHeatingRestTime() const;
    
    double getExtractionTime() const;
    double getExtractionAmount() const;

    State::Value getState() const;
    
    void powerRegulator( bool power );
	bool getPowerRegulator() const;
    
    void extractOneCup();
    void extractTwoCups();

	void setSteamMode( bool steam );
	bool getSteamMode() const;
      
private:
	void _initialize( bool activeHeating );
	void _deinitialize();
	void _worker(); 

	void _setRegulatorSettings();

	void _openShotLog();
	void _closeShotLog( double extractionTime );

	std::string getDateTimeString() const;
	std::string getLogString(double systemTime, double extractionTime, double temperature, double targetTemperature, double flowVolume, double flowVolumeCorrected, double flowSpeed, const std::string& flowStateText, const std::string& gaggiaStateText) const;
	std::string getLogHeader() const;
	std::string getStateString(State::Value state) const;

    bool _ready;
	bool _logging;

	Timer* _systemTimer;
	Timer* _extractionTimer;
	Timer* _idleTimer;

    Flow* _flowSensor;
    Ranger* _tankSensor;
    Regulator* _regulator;
	TSIC* _tsicSensor;
    Boiler* _boilerController;
    Pump* _pumpController;

    State::Value _currentState;
	State::Value _oldState;

	double _preHeatingTime;
	double _flowOffsetOneCup;
	double _flowOffsetTwoCups;

	std::ofstream* _systemStateLog;
	//std::ofstream* _shotStateLog;
	std::vector<std::string> _shotStateLog;

	bool _run;
	std::thread _thread;
	mutable std::mutex _mutex;
    
}; // Gaggia

//-----------------------------------------------------------------------------

#endif // __GAGGIA_H__
