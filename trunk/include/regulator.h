//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#ifndef __REGULATOR_H__
#define __REGULATOR_H__

//-----------------------------------------------------------------------------

#include <thread>
#include <mutex>
#include <atomic>

//-----------------------------------------------------------------------------
// Forward decls

class TSIC;
class Boiler;

//-----------------------------------------------------------------------------

class Regulator {
public:
    Regulator(Boiler* boiler, TSIC* tsic);
    ~Regulator();

    bool ready() const;

    /// Set the Proportional, Integral and Derivative gains
    void setPIDGains( double pGain, double iGain, double dGain );

    void setTargetTemperature( double targetTemperature );
    double getTargetTemperature() const;

    void setPower( bool power );
    bool getPower() const;
    
private:
    void _open();
    void _close();
    void _worker();
    double _update( double error, double position, double iGain, double pGain, double dGain );

private:
    bool _opened;
    bool _run;
    std::thread* _thread;
    std::mutex* _mutex;

    bool _power;
    double _timeStep;    
    
    double _latestTemp;    
    double _latestPower;

    TSIC* _temperature;    
    Boiler* _boiler;    
    
    double _targetTemperature;

    double _dState;    
    double _iState;    
           
    double _iMax;    
    double _iMin;    
           
    double _iGain;    
    double _pGain;    
    double _dGain;    
};

//-----------------------------------------------------------------------------

#endif // __REGULATOR_H__
