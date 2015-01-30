//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <sched.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <string>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <map>

#include "settings.h"
#include "gaggia.h"

#include "singleton.h"
#include "logger.h"

#include "boiler.h"
#include "keyboard.h"
#include "flow.h"
#include "timing.h"
#include "display.h"
#include "tsic.h"
#include "settings.h"
#include "regulator.h"
#include "pigpiomgr.h"

// -----------------------------------------------------------------------------------------

static const char* CMD_START_DELAY = "--start-delay";
static const char* CMD_HELP_SHORT  = "-h";
static const char* CMD_HELP_LONG   = "--help";
static const char* CMD_BOILER_OFF  = "--boiler-off";
static const char* CMD_LOG_STATS   = "--log-stats";

// -----------------------------------------------------------------------------------------

bool shouldQuit = false;
bool shutdownRaspberry = false;
bool logStates = false;
bool boilerActive = true;

// -----------------------------------------------------------------------------------------

bool initialize();
void deinitialize();
void signalHandler( int signal );
bool hookSignals();
void setPriority();
void printHelpText( int argc, char** argv );



std::string findSensorPath()
{
        // open file containing list of W1 slaves
        std::ifstream slaves(
                "/sys/bus/w1/devices/w1_bus_master1/w1_master_slaves"
        );

        // attempt to read first line of file: the name of the first slave
        std::string firstSlave;
        if ( !getline( slaves, firstSlave ) ) return std::string();

        // construct full path
        std::string fullPath(
                std::string("/sys/bus/w1/devices/") + firstSlave + "/w1_slave"
        );

        // return to caller
        return fullPath;
}

double getDegrees(const std::string& path)
{
        // attempt to open the sensor
        std::ifstream sensor( path.c_str() );

        std::string line;
        getline( sensor, line );        // discard first line
        getline( sensor, line );        // contains temperature at end, e.g. t=12345

        // if we don't find "t=" then something is wrong
        size_t pos = line.find("t=");
        if ( pos == std::string::npos ) return -1.0;

        // extract just the number, example: 12345
        line = line.substr( pos+2 );

        // convert to degrees
        return static_cast<double>( atoi(line.c_str()) ) / 1000.0;

}

// -----------------------------------------------------------------------------------------

void signalHandler( int signal ) {
	const bool activeLog = Singleton<Logger>::ready(); 
	switch ( signal ) {
		case SIGINT: {
			if ( activeLog ) {
				LogCritical("SIGINT");
			}
			shouldQuit = true;
			break;
		}

		case SIGTERM: {
			if ( activeLog ) {
				LogCritical("SIGTERM");
			}
			shouldQuit = true;
			break;
		}

		case SIGSEGV: {
			if ( activeLog ) {
				LogCritical("SIGSEGV");
			}
			shouldQuit = true;
			break;
		}
  
		default: {
		}
	}

	deinitialize();
	exit(-1);
}

// -----------------------------------------------------------------------------------------

bool initialize() {
	// -----------------------------------------------------------
	// Initialize logging system
	// -----------------------------------------------------------

	Singleton<Logger>::initialize( new Logger() );

	if ( !Singleton<Logger>::ready() ) {
		deinitialize();
		return false;
	}

	Singleton<Logger>::reference().enableConsoleLog( Log::LS_Info );
	Singleton<Logger>::reference().addFileLog( "/var/log/gaggia.log", Log::LS_Info );

	LogInfo("-----------------------------------------------------------");
	LogInfo("Application started");

	// -----------------------------------------------------------
	// Initialize GPIO system
	// -----------------------------------------------------------

	LogInfo("Initializing GPIO");

	Singleton<PIGPIOManager>::initialize( new PIGPIOManager() );

	if ( !Singleton<PIGPIOManager>::ready() ) {
		LogCritical("Initializing GPIO: Failed");
		deinitialize();
		return false;
	}

	LogInfo("Initializing GPIO: Success");

	// -----------------------------------------------------------
	// Initialize settings
	// -----------------------------------------------------------

	LogInfo("Initializing Settings");

	Singleton<Settings>::initialize( new Settings() );

	if ( !Singleton<Settings>::ready() ) {
		LogCritical("Initializing Settings: Failed");
		deinitialize();
		return false;
	}

	LogInfo("Initializing Settings: Success");

	// -----------------------------------------------------------
	// Initialize Gaggia controller
	// -----------------------------------------------------------
	
	LogInfo("Initializing Gaggia controller");
	
	Singleton<Gaggia>::initialize( new Gaggia( boilerActive, logStates ) );

	if ( !Singleton<Gaggia>::ready() ) {
		LogCritical("Initializing Gaggia: Failed");
		deinitialize();
		return false;
	}
	
	LogInfo("Initializing Gaggia controller: Success");

	// -----------------------------------------------------------
	// Initialize display
	// -----------------------------------------------------------

	LogInfo("Initializing Display");

	Singleton<Display>::initialize( new Display() );

	if ( !Singleton<Display>::ready() ) {
		LogCritical("Initializing Display: Failed");
		deinitialize();
		return false;
	}

	LogInfo("Initializing Display: Success");

	// -----------------------------------------------------------

	return true;
}

// -----------------------------------------------------------------------------------------

void deinitialize() {
	const bool activeLog = Singleton<Logger>::ready(); 

	if ( activeLog ) {
		LogInfo("Control loop closed, starting to deinitialize systems");
	}

	if ( Singleton<Display>::ready() ) {
		Singleton<Display>::deinitialize();
	}

	if ( activeLog ) {
		LogInfo("Display offline");
	}

	if ( Singleton<Gaggia>::ready() ) {
		Singleton<Gaggia>::deinitialize();
	}

	if ( activeLog ) {
		LogInfo("Hardware systems offline");
	}

	if ( Singleton<Settings>::ready() ) {
		Singleton<Settings>::deinitialize();
	}
	
	if ( activeLog ) {
		LogInfo("Settings offline");
	}

	if ( Singleton<PIGPIOManager>::ready() ) {
		Singleton<PIGPIOManager>::deinitialize();
	}
	
	if ( activeLog ) {
		LogInfo("GPIO system offline");
	}

	if ( activeLog ) {
		LogInfo("All systems properly deinitialized, application is closing");
	}
  
	if ( shutdownRaspberry ) {
		if ( activeLog ) {
			LogInfo("Invoking SHUTDOWN NOW");
		}
		system("shutdown now");
	}
  
	if ( activeLog ) {
		Singleton<Logger>::deinitialize();
	}
}

// -----------------------------------------------------------------------------------------

bool hookSignals() {
	if ( signal( SIGINT, signalHandler ) == SIG_ERR ) {
		LogCritical("Failed to hook SIGINT\n");
		return false;
	}
  
	if ( signal( SIGTERM, signalHandler ) == SIG_ERR ) {
		LogCritical("Failed to hook SIGTERM\n");
		return false;
	}
  
	if ( signal( SIGSEGV, signalHandler ) == SIG_ERR ) {
		LogCritical("Failed to hook SIGSEGV\n");
		return false;
	}

	return true;
}

// -----------------------------------------------------------------------------------------

void setPriority() {
	// Run at high priority
	struct sched_param sched = {};
	sched.sched_priority = sched_get_priority_max(SCHED_RR);
	sched_setscheduler(0, SCHED_RR, &sched);
}

// -----------------------------------------------------------------------------------------

void printHelpText(int argc, char** argv) {
    std::cout << "Usage: " << argv[0] << " [options]" << std::endl
        << std::endl << "Options:" << std::endl
		<< "  " << CMD_START_DELAY << " N\tWait N seconds before starting" << std::endl
		<< "  " << CMD_BOILER_OFF << "\t\tNo heating" << std::endl
		<< "  " << CMD_LOG_STATS << "\t\tLog all stats into file" << std::endl
        << "  " << CMD_HELP_LONG << " or " << CMD_HELP_SHORT << "\t\tPrint this message and exit" << std::endl
        << "\n";
}

// -----------------------------------------------------------------------------------------

int main(int argc, char** argv) {   
	// -----------------------------------------------------------
	// Get start parameters
	// -----------------------------------------------------------

	size_t startDelay = 0;

	if ( argc > 1 ) {
		for ( int i = 1; i < argc; ++i ) {
			if ( strcmp( argv[ i ], CMD_START_DELAY ) == 0 ) {
				if ( i + 1 > argc ) {
					printHelpText( argc, argv );
					exit(0);
				}
				else {
					startDelay = atoi( argv[ ++i ] );
				}
			}
			else if ( strcmp( argv[ i ], CMD_BOILER_OFF ) == 0 ) {
				boilerActive = false;
			}
			else if ( strcmp( argv[ i ], CMD_LOG_STATS ) == 0 ) {
				logStates = true;
			}
			else if ( strcmp( argv[ i ], CMD_HELP_SHORT ) == 0 || strcmp( argv[ i ], CMD_HELP_LONG ) == 0 ) {
				printHelpText( argc, argv );
				exit(0);
			}
			else {
				std::cout << "ERROR: Invalid parameter: " << argv[ i ] << std::endl;
				printHelpText( argc, argv );
				return -1;
			}
		}
	}

	if ( startDelay > 0 ) {
		sleep( startDelay );
	}

	// -----------------------------------------------------------
	// Register signals and set process priority
	// -----------------------------------------------------------
	
	if ( !hookSignals() ) {
		return -1;
	}

	setPriority();

	// -----------------------------------------------------------
	// Initialize all systems and singletons
	// -----------------------------------------------------------
  
	if ( !initialize() ) {
		deinitialize();
		return -1;
	}

	LogInfo("All systems properly initialized, entering controller loop");

	// -----------------------------------------------------------
	// Main loop
	// -----------------------------------------------------------
  
	const unsigned int samplingRate = 500;
	const std::string sensorPath = findSensorPath();
  
	//Gaggia* gaggia = Singleton<Gaggia>::pointer();
	Display* display = Singleton<Display>::pointer();
	
	while ( !shouldQuit ) {    
		// Check if we should shutdown the PI
		if ( display->getShutdown() ) {
			LogInfo("Received SHUTDOWN command");
			shutdownRaspberry = true;
			shouldQuit = true;
		}

		double degrees = getDegrees( sensorPath );
		LogInfo("Extern Degrees: " << degrees);

		delayms( samplingRate );
	}

	// -----------------------------------------------------------
	// Deinitialize and exit
	// -----------------------------------------------------------

	deinitialize();

	// -----------------------------------------------------------
  
	return 0;
}

// -----------------------------------------------------------------------------------------
