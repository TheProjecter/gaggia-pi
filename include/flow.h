//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#ifndef __FLOW_H__
#define __FLOW_H__

//-----------------------------------------------------------------------------

#include <thread>
#include <mutex>
#include <atomic>

// Forward decl
class GPIOPin;

//-----------------------------------------------------------------------------

class Flow {
public:
    struct State {
        enum Value {
            Invalid,
            Flowing,
            Stopped
        };
    };

    Flow();
    ~Flow();

    bool ready() const;

    State::Value getState() const;
    double getMilliLitres() const;

    // Returns speed of flow in ml/s (in sampling window)
    double getFlowSpeed() const;

private:
    void _open();
    void _close();
    void _worker();
    void _alertFunction( unsigned pin, bool level, unsigned tick );

    GPIOPin* _flowPin;
    bool _opened;
    State::Value _state;

    unsigned int _samplingRate;
    unsigned int _speedSamplingRate;
    unsigned int _timeout;
    unsigned int _count;

    // Flow speed in sampling rate in [ml/s]
    double _flowSpeed;
    double _milliLitrePerCounts;

    bool _run;
    std::thread _thread;
    mutable std::mutex _mutex;
};

//-----------------------------------------------------------------------------

#endif // __FLOW_H__
