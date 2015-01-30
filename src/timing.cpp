//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#include <sys/time.h>
#include <time.h>

#include "timing.h"

//-----------------------------------------------------------------------------

/// defines which clock we use for timing:
/// CLOCK_MONOTONIC_RAW = raw hardware based time, not subject to NTP
///   adjustments or incremental adjustments performed by adjtime. Reports 2ns
///   resolution on Raspbian and two successive reads take about 2us.
static const clockid_t g_timingClockId = CLOCK_MONOTONIC_RAW;

//-----------------------------------------------------------------------------

void delayms( unsigned ms ) {
	struct timespec req, rem;

	req.tv_sec  = (time_t)(ms / 1000) ;
  	req.tv_nsec = (long)(ms % 1000) * 1000000L;

  	nanosleep( &req, &rem );
}

//-----------------------------------------------------------------------------

void delayus ( unsigned us ) {
  	struct timeval now;
  	gettimeofday( &now, 0 );

	struct timeval delay, end;
  	delay.tv_sec  = us / 1000000 ;
  	delay.tv_usec = us % 1000000 ;
  	timeradd( &now, &delay, &end );

  	while ( timercmp( &now, &end, <) )
		gettimeofday( &now, 0 );
}

//-----------------------------------------------------------------------------

double getClock() {
	struct timespec now;
	clock_gettime( g_timingClockId, &now );

	return (double)now.tv_sec + (double)now.tv_nsec * 1.0E-9;
}

//-----------------------------------------------------------------------------

Timer::Timer() 
	:_startTime( 0.0 )
	,_stopTime( 0.0 )
	,_running( false )
{
    reset();
}

//-----------------------------------------------------------------------------

void Timer::reset() {
    _startTime = getClock();
    _stopTime  = _startTime;
    _running   = false;
}

//-----------------------------------------------------------------------------

void Timer::start() {
    _startTime = getClock();
    _running   = true;
}

//-----------------------------------------------------------------------------

double Timer::stop() {
    _stopTime  = getClock();
    _running   = false;
    return _stopTime - _startTime;
}

//-----------------------------------------------------------------------------

double Timer::getElapsed() const {
    if ( _running ) {
        return getClock() - _startTime;
	}
    else {
        return _stopTime - _startTime;
	}
}

//-----------------------------------------------------------------------------

bool Timer::isRunning() const {
    return _running;
}

//-----------------------------------------------------------------------------
