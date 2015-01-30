//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#include <iomanip>

#include "boiler.h"
#include "pump.h"
#include "timing.h"
#include "flow.h"
#include "tsic.h"
#include "ranger.h"
#include "settings.h"
#include "regulator.h"
#include "pigpiomgr.h"
#include "timing.h"

#include "singleton.h"
#include "logger.h"

#include "gaggia.h"

// -----------------------------------------------------------------------------------------

Gaggia::Gaggia( bool activeHeating, bool logging ) 
	:_ready( false )
	,_logging( logging )
	,_flowSensor( nullptr )
	,_tankSensor( nullptr )
	,_regulator( nullptr )
	,_tsicSensor( nullptr )
	,_boilerController( nullptr )
	,_pumpController( nullptr ) 
	,_currentState( State::Heating )
	,_oldState( State::Heating )
	,_preHeatingTime( 30.0 )
	,_flowOffsetOneCup( 0.0 )
	,_flowOffsetTwoCups( 0.0 )
	,_systemStateLog ( nullptr )
	//,_shotStateLog( nullptr )
	,_run( false )
{
	_initialize( activeHeating );
}

// -----------------------------------------------------------------------------------------

Gaggia::~Gaggia() {
	_deinitialize();
}

// -----------------------------------------------------------------------------------------

bool Gaggia::ready() const {
	return _ready;
}

// -----------------------------------------------------------------------------------------

double Gaggia::getBoilerTemperature() const {
	if ( !_ready ) {
		return 0.0;
	}

	std::lock_guard<std::mutex> lock( _mutex );
    
	double temp = 0.0;
	_tsicSensor->getDegrees( temp );

    return temp;
}

// -----------------------------------------------------------------------------------------

double Gaggia::getBoilerTargetTemperature() const {
	if ( !_ready ) {
		return 0.0;
	}

    std::lock_guard<std::mutex> lock( _mutex );
    return _regulator->getTargetTemperature();
}

// -----------------------------------------------------------------------------------------

double Gaggia::getWaterTankLevel() const {
	if ( !_ready ) {
		return 0.0;
	}

	std::lock_guard<std::mutex> lock( _mutex );
	double level = 0.0;
	bool result = _tankSensor->getRange( level );

	if ( !result ) {
		level = 0.0;
	}
	else {
		const double MIN_LEVEL = 0.12;
		const double MAX_LEVEL = 0.018;

		level = ( level - MAX_LEVEL ) / ( MIN_LEVEL - MAX_LEVEL ); // Scale to [0, 1]
		level = 1.0 - level;   // Flip to reflect 100% = filled tank, 0% = empty tank

		if ( level < 0.0) {
			level = 0.0;
		}
		else if ( level > 1.0 ) {
			level = 1.0;
		}
	}

	return level;
}

// -----------------------------------------------------------------------------------------

double Gaggia::getIdleTime() const {
	if ( !_ready ) {
		return 0.0;
	}

    std::lock_guard<std::mutex> lock( _mutex );
	return _idleTimer->getElapsed();  
}

// -----------------------------------------------------------------------------------------

double Gaggia::getSystemTime() const {
    if ( !_ready ) {
		return 0.0;
	}

	std::lock_guard<std::mutex> lock( _mutex );
	return _systemTimer->getElapsed();
}

// -----------------------------------------------------------------------------------------

double Gaggia::getHeatingRestTime() const {
	if ( !_ready ) {
		return 0.0;
	}

    double restTime = _preHeatingTime - _systemTimer->getElapsed();
	if ( restTime < 0.0 ) {
		restTime = 0.0;
	}

	return restTime;
}

// -----------------------------------------------------------------------------------------

double Gaggia::getExtractionTime() const {
	if ( !_ready ) {
		return 0.0;
	}

    std::lock_guard<std::mutex> lock( _mutex );
	return _extractionTimer->getElapsed();  
}

// -----------------------------------------------------------------------------------------

double Gaggia::getExtractionAmount() const {
	if ( !_ready ) {
		return 0.0;
	}

	std::lock_guard<std::mutex> lock( _mutex );

	double flowVolume = _flowSensor->getMilliLitres();

	// Adjust extraction volume for single & double shots by specific offset
	if ( _currentState == State::ExtractingOneCup ) {
		flowVolume -= _flowOffsetOneCup;
	}
	else if ( _currentState == State::ExtractingTwoCups ) {
		flowVolume -= _flowOffsetTwoCups;
	}

	if ( flowVolume < 0.0 ) {
		flowVolume = 0.0;
	}

    return flowVolume;
}

// -----------------------------------------------------------------------------------------

Gaggia::State::Value Gaggia::getState() const {
	if ( !_ready ) {
		return State::Invalid;
	}

    std::lock_guard<std::mutex> lock( _mutex );
    return _currentState;
}

// -----------------------------------------------------------------------------------------

void Gaggia::powerRegulator( bool power ) {
    if ( !_ready ) {
		return;
	}

	std::lock_guard<std::mutex> lock( _mutex );
	_regulator->setPower( power );

	if ( power ) {
		_currentState = State::Heating;
	}
	else {
		_currentState = State::Deactivated;
	}
}

// -----------------------------------------------------------------------------------------

bool Gaggia::getPowerRegulator() const {
    if ( !_ready ) {
		return false;
	}

	std::lock_guard<std::mutex> lock( _mutex );
	return _regulator->getPower();
}

// -----------------------------------------------------------------------------------------

void Gaggia::extractOneCup() {
	if ( !_ready ) {
		return;
	}

    std::lock_guard<std::mutex> lock( _mutex );
		
	// No double extraction at the same time or during steam
	if ( _currentState == State::Extracting || _currentState == State::ExtractingOneCup || _currentState == State::ExtractingTwoCups || _currentState == State::Steam ) {
		return;
	}

	_oldState = _currentState;
	_currentState = State::ExtractingOneCup;

	if ( _logging ) {
		_openShotLog();
	}

	_extractionTimer->reset();
	_pumpController->setPower( true );
}

// -----------------------------------------------------------------------------------------

void Gaggia::extractTwoCups() {
	if ( !_ready ) {
		return;
	}

    std::lock_guard<std::mutex> lock( _mutex );

	// No double extraction at the same time or during steam
	if ( _currentState == State::Extracting || _currentState == State::ExtractingOneCup || _currentState == State::ExtractingTwoCups || _currentState == State::Steam ) {
		return;
	}

	_oldState = _currentState;
	_currentState = State::ExtractingTwoCups;

	if ( _logging ) {
		_openShotLog();
	}

	_extractionTimer->reset();
	_pumpController->setPower( true );
}

// -----------------------------------------------------------------------------------------

void Gaggia::setSteamMode( bool steam ) {
	if ( !_ready ) {
		return;
	}

	std::lock_guard<std::mutex> lock( _mutex );

	// No heating state switches during deactivated boiler or during extraction
	if ( _currentState == State::Deactivated || _currentState == State::Extracting || _currentState == State::ExtractingOneCup || _currentState == State::ExtractingTwoCups ) {
		return;
	}
	
	if ( steam ) {
		_oldState = _currentState;
		_currentState = State::Steam;
	}
	else {
		_currentState = _oldState;
	}

	_setRegulatorSettings();

	/*double iGain = 0.0;
	double pGain = 0.0;
	double dGain = 0.0;
	double targetTemperature = 0.0;

	Singleton<Settings>::pointer()->getRegulatorSettings( steam, iGain, pGain, dGain, targetTemperature );

	if ( _currentState == State::Heating ) {
		double preHeatingTime = 0.0;
		Singleton<Settings>::pointer()->getPreHeatingSettings( preHeatingTime, targetTemperature );
	}

	_regulator->setPIDGains( pGain, iGain, dGain );
	_regulator->setTargetTemperature( targetTemperature );*/
}

// -----------------------------------------------------------------------------------------

bool Gaggia::getSteamMode() const {
	if ( !_ready ) {
		return false;
	}

	std::lock_guard<std::mutex> lock( _mutex );
	return ( _currentState == State::Steam );
}

// -----------------------------------------------------------------------------------------

void Gaggia::_initialize( bool activeHeating ) {	
	// -----------------------------------------------------------
    // Check if Logging system is running
	// -----------------------------------------------------------

	printf("Boo1\n");

    if ( !Singleton<Logger>::ready() ) {
		_deinitialize();
		return;
    }

	printf("Boo2\n");

	// -----------------------------------------------------------
    // Check if GPIO system is running
	// -----------------------------------------------------------

    if ( !Singleton<PIGPIOManager>::ready() ) {
		LogCritical("GPIO system is not running");
		_deinitialize();
		return;
    }
    
	printf("Boo3\n");

	// -----------------------------------------------------------
    // TSIC temperature sensor
	// -----------------------------------------------------------

    LogInfo("Initializing TSIC Sensor");

	printf("Boo4\n");

    _tsicSensor = new TSIC( TSIC_PIN );

	printf("Boo5\n");

    if ( !_tsicSensor->ready() ) {
		LogCritical("Initializing TSIC Sensor: Failed");
		_deinitialize();
		return;
    }
    LogInfo("Initializing TSIC Sensor: Success");
     
	// -----------------------------------------------------------
    // Boiler controller
	// -----------------------------------------------------------

    LogInfo("Initializing Boiler Controller");
    _boilerController = new Boiler();
    if ( !_boilerController->ready() ) {
		LogCritical("Initializing Boiler: Failed");
		_deinitialize();
		return;
    }
    LogInfo("Initializing Boiler: Success");

	// -----------------------------------------------------------
	// Pump controller
	// -----------------------------------------------------------

    LogInfo("Initializing Pump Controller");
    _pumpController = new Pump();
    if ( !_pumpController->ready() ) {
		LogCritical("Initializing Pump: Failed");
		_deinitialize();
		return;
    }
    LogInfo("Initializing Pump: Success");

	// -----------------------------------------------------------
	// Tank sensor
	// -----------------------------------------------------------

    LogInfo("Initializing Tank sensor");
    _tankSensor = new Ranger();
    if ( !_tankSensor->ready() ) {
		LogCritical("Initializing Tank sensor: Failed");
		_deinitialize();
		return;
    }
    LogInfo("Initializing Tank sensor: Success");
    
	// -----------------------------------------------------------
    // Regulator
	// -----------------------------------------------------------

    LogInfo("Initializing Regulator");
    _regulator = new Regulator( _boilerController, _tsicSensor );
    
    if ( !_regulator->ready() ) {
		LogCritical("Initializing Regulator: Failed");
		_deinitialize();
		return;
    }

	// Set PID settings
	_setRegulatorSettings();
	//setSteamMode( false );

	// Set target temperature for pre-heating mode
	//double targetTemperature = 0.0;
	//Singleton<Settings>::pointer()->getPreHeatingSettings( _preHeatingTime, targetTemperature );
	//_regulator->setTargetTemperature( targetTemperature );
	
	// Check for init flag and activate boiler power on demand
	if ( activeHeating ) {
		_regulator->setPower( true );
	}
	else {
		_regulator->setPower( false );
		_currentState = State::Deactivated;
	}

    LogInfo("Initializing Regulator: Success");

	// -----------------------------------------------------------
	// Flow sensor
	// -----------------------------------------------------------

    LogInfo("Initializing Flow sensor");
    _flowSensor = new Flow();
    if ( !_flowSensor->ready() ) {
		LogCritical("Initializing Flow sensor: Failed");
		_deinitialize();
		return;
    }

	_flowOffsetOneCup = Singleton<Settings>::pointer()->getFlowOffset30();
	_flowOffsetTwoCups = Singleton<Settings>::pointer()->getFlowOffset60();

    LogInfo("Initializing Flow sensor: Success");

	// -----------------------------------------------------------
	// Open state log
	// -----------------------------------------------------------

	if ( _logging ) {
		const std::string dateTime = getDateTimeString();
		const std::string prefix = "/home/pi/projects/elmyra/logs/";
		const std::string fileName = prefix + "system_log_" + dateTime + ".csv";

		_systemStateLog = new std::ofstream( fileName );
    
		if ( !_systemStateLog->is_open() ) {
			LogError("Could not open state log for writing");
		}
		else {
			(*_systemStateLog) << getLogHeader();
		}
	}

	// -----------------------------------------------------------

	_systemTimer = new Timer();
	_extractionTimer = new Timer();
	_idleTimer = new Timer();

	_systemTimer->start();
	_extractionTimer->stop();

	_run = true;
    _thread = std::thread( &Gaggia::_worker, this );

    _ready = true;
}

// -----------------------------------------------------------------------------------------

void Gaggia::_deinitialize() {	
	if ( _run ) {
		_run = false;
		_thread.join();
	}

	LogInfo("Deinitializing regulator");

	if ( _regulator ) {
		delete _regulator;
	}

	LogInfo("Deinitializing boiler controller");

	if ( _boilerController ) {
		delete _boilerController;
	}

	LogInfo("Deinitializing pump controller");

	if ( _pumpController ) {
		delete _pumpController;
	}
	
	LogInfo("Deinitializing TSIC sensor");

	if ( _tsicSensor ) {
		delete _tsicSensor;
	}

	LogInfo("Deinitializing flow sensor");

	if ( _flowSensor ) {
		delete _flowSensor;
	}

	LogInfo("Deinitializing tank sensor");

	if ( _tankSensor ) {
		delete _tankSensor;
	}

	if ( _systemTimer ) {
		delete _systemTimer;
	}

	if ( _extractionTimer ) {
		delete _extractionTimer;
	}

	if ( _idleTimer ) {
		delete _idleTimer;
	}

	if ( _logging && _systemStateLog ) {
		_systemStateLog->close();
		delete _systemStateLog;
	}

	//if ( _logging && _shotStateLog ) {
	//	//_shotStateLog->close();
	//	delete _shotStateLog;
	//}
}

// -----------------------------------------------------------------------------------------

void Gaggia::_worker() {
	State::Value state           = State::Invalid;
	Flow::State::Value flowState = Flow::State::Stopped;

	bool extractionTimerRunning = false;
	bool pumpRunning            = false;

	double systemTime             = 0.0;
	double flowVolume             = 0.0;
	double flowVolumeCorrected    = 0.0;
	double flowSpeed              = 0.0;
	double temperature            = 0.0;
	double targetTemperature      = 0.0;
	double timeSinceLastSystemLog = 0.0;
	double timeSinceLastShotLog   = 0.0;

	const unsigned int systemLogRate = 500;
	const unsigned int shotLogRate   = 50;
	const unsigned int sampleRate    = 25;

	while ( _run ) {
		delayms( sampleRate );

		// -----------------------------------------------------------
		// Copy current states
		// -----------------------------------------------------------
		
		{
			std::lock_guard<std::mutex> lock( _mutex );
			state = _currentState;
			systemTime = _systemTimer->getElapsed();

			flowState  = _flowSensor->getState();
			flowVolume = _flowSensor->getMilliLitres();
			flowSpeed  = _flowSensor->getFlowSpeed();

			flowVolumeCorrected = flowVolume;
	
			// Adjust extraction volume for 30 & 60 shots by specific offset
			if ( state == State::ExtractingOneCup ) {
				flowVolumeCorrected -= _flowOffsetOneCup;
			}
			else if ( state == State::ExtractingTwoCups ) {
				flowVolumeCorrected -= _flowOffsetTwoCups;
			}

			_tsicSensor->getDegrees( temperature );
			targetTemperature = _regulator->getTargetTemperature();

			extractionTimerRunning = _extractionTimer->isRunning();
			pumpRunning            = _pumpController->getPower();
		}

		// -----------------------------------------------------------
		// State logging
		// -----------------------------------------------------------

		timeSinceLastSystemLog += sampleRate;
		timeSinceLastShotLog   += sampleRate;

		const bool systemLogScheduled = timeSinceLastSystemLog >= systemLogRate;
		const bool shotLogScheduled   = timeSinceLastShotLog   >= shotLogRate;

		if ( _logging && ( systemLogScheduled || shotLogScheduled ) ) {
			const std::string flowStateText = ( flowState == Flow::State::Stopped ) ? "Stopped" : "Flowing";
			const std::string gaggiaStateText = getStateString( state );

			if ( systemLogScheduled && _systemStateLog != nullptr && _systemStateLog->is_open() ) {
				timeSinceLastSystemLog = 0.0;

				(*_systemStateLog) << getLogString(systemTime, _extractionTimer->getElapsed(), temperature, targetTemperature, flowVolume, flowVolumeCorrected, flowSpeed, flowStateText, gaggiaStateText);
			}

			if ( shotLogScheduled && ( state == State::Extracting || state == State::ExtractingOneCup || state == State::ExtractingTwoCups ) /*&& _shotStateLog != nullptr && _shotStateLog->is_open()*/ ) {
				timeSinceLastShotLog = 0.0;

				_shotStateLog.push_back( getLogString(systemTime, _extractionTimer->getElapsed(), temperature, targetTemperature, flowVolume, flowVolumeCorrected, flowSpeed, flowStateText, gaggiaStateText) );
			}
		}

		// -----------------------------------------------------------
		// Flow meter update and automatic extraction control
		// -----------------------------------------------------------

		// No flow meter for steam
		if ( state != State::Steam ) {

			// -------------------------------------------------------
			// Check for automatic extraction
			// -------------------------------------------------------

			if ( state == State::ExtractingOneCup || state == State::ExtractingTwoCups ) {
				const double extractionTarget = ( state == State::ExtractingOneCup ) ? 25.0 : 50.0;
				const double extractionOffset = ( state == State::ExtractingOneCup ) ? _flowOffsetOneCup : _flowOffsetTwoCups;

				// Check if extraction offset reached => start time measurement
				if ( !extractionTimerRunning && flowVolume >= extractionOffset && pumpRunning ) {
					std::lock_guard<std::mutex> lock( _mutex );

					_extractionTimer->reset();
					_extractionTimer->start();
					extractionTimerRunning = true;
				}

				// Check if target volume reached => stop pump and timer
				if ( flowVolumeCorrected >= extractionTarget ) {
					std::lock_guard<std::mutex> lock( _mutex );

					_pumpController->setPower( false );
					pumpRunning = false;

					_extractionTimer->stop();
					extractionTimerRunning = false;

					_idleTimer->reset();
					_idleTimer->start();

					_currentState = _oldState;
					state = _currentState;

					if ( _logging ) {
						_closeShotLog( _extractionTimer->getElapsed() );
					}
				}

				// Check if flow has stopped by schedule and flow sensor stopped => change state
				/*if ( !extractionTimerRunning && flowState == Flow::State::Stopped ) {
					std::lock_guard<std::mutex> lock( _mutex );

					_currentState = _oldState;
					state = _currentState;
				}*/
			}

			// -------------------------------------------------------
			// Check for user triggered extraction
			// -------------------------------------------------------

			if ( flowState == Flow::State::Flowing && state != State::Extracting && state != State::ExtractingOneCup && state != State::ExtractingTwoCups ) {
				std::lock_guard<std::mutex> lock( _mutex );

				// Check for some idle time, otherwise the flow may be some rest of the previous extraction
				if ( _idleTimer->getElapsed() >= 5.0 ) {
					_oldState = _currentState;
					_currentState = State::Extracting;
					state = _currentState;

					_extractionTimer->reset();
					_extractionTimer->start();
					extractionTimerRunning = true;

					if ( _logging ) {
						_openShotLog();
					}
				}
			}

			// -------------------------------------------------------
			// Check for stopped user triggered extraction
			// -------------------------------------------------------

			if ( state == State::Extracting && flowState != Flow::State::Flowing ) {
				std::lock_guard<std::mutex> lock( _mutex );
				
				_currentState = _oldState;
				state = _currentState;

				_extractionTimer->stop();
				extractionTimerRunning = false;

				_idleTimer->reset();
				_idleTimer->start();

				if ( _logging ) {
					_closeShotLog( _extractionTimer->getElapsed() );
				}
			}
		}

		// -----------------------------------------------------------
		// Check for pre-heating finish
		// -----------------------------------------------------------

		if ( state == State::Heating && systemTime >= _preHeatingTime ) {
			std::lock_guard<std::mutex> lock( _mutex );

			_currentState = State::Active;
			_idleTimer->reset();
			_idleTimer->start();

			_setRegulatorSettings();
		}
	}
}

// -----------------------------------------------------------------------------------------

void Gaggia::_setRegulatorSettings() {
	double iGain = 0.0;
	double pGain = 0.0;
	double dGain = 0.0;
	double targetTemperature = 0.0;

	const bool steam = ( _currentState == State::Steam );

	Singleton<Settings>::pointer()->getRegulatorSettings( steam, iGain, pGain, dGain, targetTemperature );

	if ( _currentState == State::Heating ) {
		double preHeatingTime = 0.0;
		Singleton<Settings>::pointer()->getPreHeatingSettings( preHeatingTime, targetTemperature );
	}

	_regulator->setPIDGains( pGain, iGain, dGain );
	_regulator->setTargetTemperature( targetTemperature );
}

// -----------------------------------------------------------------------------------------

void Gaggia::_openShotLog() {
	/*time_t rawtime;
	struct tm* timeinfo;
	time( &rawtime );
	timeinfo = localtime( &rawtime );

	std::stringstream sstream;
	sstream << (timeinfo->tm_year + 1900) << "_" << (timeinfo->tm_mon + 1) << "_" << timeinfo->tm_mday << "__" << timeinfo->tm_hour << "_" << timeinfo->tm_min << "_" << timeinfo->tm_sec;

	const std::string dateTime = sstream.str();
	const std::string prefix = "/home/pi/projects/elmyra/logs/";

	if ( _shotStateLog != nullptr ){
		_shotStateLog->close();
		delete _shotStateLog;
	}

	_shotStateLog = new std::ofstream( prefix + "shot_log_" + dateTime + ".csv" );
    
	if ( !_shotStateLog->is_open() ) {
		LogError("Could not open state log for writing");
	}
	else {
		(*_shotStateLog) << "systemTime" << ";"
						 << "extractionTime" << ";"
						 << "temperature" << ";"
						 << "targetTemperature" << ";"
						 << "flowVolume" << ";"
						 << "flowVolumeCorrected" << ";"
						 << "flowSpeed" << ";"
						 << "flowState" << ";"
						 << "gaggiaState" << std::endl;
	}*/

	_shotStateLog.clear();


	/*if ( _shotStateLog ){
		delete _shotStateLog;
	}

	_shotStateLog = new std::stringstream();*/

	//const std::string logHeader = "systemTime;extractionTime;temperature;targetTemperature;flowVolume;flowVolumeCorrected;flowSpeed;flowState;gaggiaState\n";
	_shotStateLog.push_back( getLogHeader() );

	//(*_shotStateLog) << "systemTime" << ";"
	//				 << "extractionTime" << ";"
	//				 << "temperature" << ";"
	//				 << "targetTemperature" << ";"
	//				 << "flowVolume" << ";"
	//				 << "flowVolumeCorrected" << ";"
	//				 << "flowSpeed" << ";"
	//				 << "flowState" << ";"
	//				 << "gaggiaState" << std::endl;
}

// -----------------------------------------------------------------------------------------

void Gaggia::_closeShotLog( double extractionTime ) {
	/*if ( _shotStateLog != nullptr ){
		_shotStateLog->close();
		delete _shotStateLog;
		_shotStateLog = nullptr;
	}*/

	// Avoid storing short water flows as shots
	if ( extractionTime < 5.0 ) {
		return;
	}

	/*time_t rawtime;
	struct tm* timeinfo;
	time( &rawtime );
	timeinfo = localtime( &rawtime );

	std::stringstream sstream;
	sstream << (timeinfo->tm_year + 1900) << "_" << (timeinfo->tm_mon + 1) << "_" << timeinfo->tm_mday << "__" << timeinfo->tm_hour << "_" << timeinfo->tm_min << "_" << timeinfo->tm_sec;*/
		
	const std::string dateTime = getDateTimeString();//sstream.str();
	const std::string prefix = "/home/pi/projects/elmyra/logs/";
	const std::string fileName = prefix + "shot_log_" + dateTime + ".csv";

	std::ofstream shotStateLogFile( fileName );
    
	if ( !shotStateLogFile.is_open() ) {
		LogError("Could not open shot log '" << fileName << "' for writing");
	}
	else {
		for ( size_t index = 0; index < _shotStateLog.size(); ++index ) {
			shotStateLogFile << _shotStateLog[index];
		}

		shotStateLogFile.close();
	}
}

// -----------------------------------------------------------------------------------------

std::string Gaggia::getDateTimeString() const {
	time_t rawtime;
	struct tm* timeinfo;
	time( &rawtime );
	timeinfo = localtime( &rawtime );

	std::stringstream sstream;
	sstream << (timeinfo->tm_year + 1900) << "_" << (timeinfo->tm_mon + 1) << "_" << timeinfo->tm_mday << "__" << timeinfo->tm_hour << "_" << timeinfo->tm_min << "_" << timeinfo->tm_sec;
		
	return sstream.str();
}

// -----------------------------------------------------------------------------------------

std::string Gaggia::getLogString(double systemTime, double extractionTime, double temperature, double targetTemperature, double flowVolume, double flowVolumeCorrected, double flowSpeed, const std::string& flowStateText, const std::string& gaggiaStateText) const {
	std::stringstream sstream;
	sstream << std::fixed << std::setprecision(2) << systemTime << ";"
			<< std::fixed << std::setprecision(2) << extractionTime << ";"
			<< std::fixed << std::setprecision(1) << temperature << ";"
			<< std::fixed << std::setprecision(1) << targetTemperature << ";"
			<< flowVolume << ";"
			<< flowVolumeCorrected << ";"
			<< flowSpeed << ";"
			<< "\"" << flowStateText << "\"" << ";"
			<< "\"" << gaggiaStateText << "\"" << std::endl;

	return sstream.str();
}

// -----------------------------------------------------------------------------------------

std::string Gaggia::getLogHeader() const {
	return "systemTime;extractionTime;temperature;targetTemperature;flowVolume;flowVolumeCorrected;flowSpeed;flowState;gaggiaState\n";
}

// -----------------------------------------------------------------------------------------

std::string Gaggia::getStateString(State::Value state) const {
	std::string gaggiaStateText = "";

	switch ( state ) {
		case State::Deactivated: {
			gaggiaStateText = "Deactivated";
			break;
		}

		case State::Heating: {
			gaggiaStateText = "Heating";
			break;
		}

		case State::Active: {
			gaggiaStateText = "Active";
			break;
		}

		case State::Steam: {
			gaggiaStateText = "Steam";
			break;
		}

		case State::Extracting: {
			gaggiaStateText = "Manual Extraction";
			break;
		}

		case State::ExtractingOneCup: {
			gaggiaStateText = "Extraction (30ml)";
			break;
		}

		case State::ExtractingTwoCups: {
			gaggiaStateText = "Extraction (60ml)";
			break;
		}

		case State::Invalid: 
		default: {
			gaggiaStateText = "Invalid";
			break;
		}
	}

	return gaggiaStateText;
}

// -----------------------------------------------------------------------------------------
