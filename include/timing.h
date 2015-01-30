//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#ifndef __TIMING_H__
#define __TIMING_H__

//-----------------------------------------------------------------------------

void delayms( unsigned ms );

void delayus ( unsigned us );

double getClock();

//-----------------------------------------------------------------------------

class Timer {
public:
    Timer();

    void reset();
    void start();
    double stop();

    /// Returns the elapsed time in seconds
    double getElapsed() const;
    bool isRunning() const;

private:
    double _startTime;
    double _stopTime; 
    bool _running;  
};

//-----------------------------------------------------------------------------

#endif // __TIMING_H__
