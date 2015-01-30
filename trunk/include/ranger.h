//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#ifndef __RANGER_H__
#define __RANGER_H__

//-----------------------------------------------------------------------------

#include <thread>
#include <mutex>

//-----------------------------------------------------------------------------
// Forward decls

class GPIOPin;

//-----------------------------------------------------------------------------

class Ranger {
public:
    Ranger();
    ~Ranger();

    /// Returns most recent range measurement in metres
    bool getRange( double& range ) const;
    bool ready() const;

private:
    void _open();
    void _close();

    void _worker();
    double _measureRange();
    void _alertFunction( unsigned gpio, unsigned level, uint32_t tick );

    bool _opened;
    double _timeLastRun;
    double _range;
    unsigned _count;
    unsigned _timeout;
    uint32_t _timeStamp[2];

    GPIOPin* _echoPin;
    GPIOPin* _triggerPin;

    bool _run;
    std::thread _thread;
    mutable std::mutex _rangeMutex;
    mutable std::mutex _countMutex;
};

//-----------------------------------------------------------------------------

#endif // __RANGER_H__
