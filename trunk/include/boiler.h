//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#ifndef __BOILER_H__
#define __BOILER_H__

#include <stdlib.h>

//-----------------------------------------------------------------------------

// Forward decls
class GPIOPin;

//-----------------------------------------------------------------------------

class Boiler {
public:
    Boiler();
    ~Boiler();
    
	bool ready() const;

    // Set current power level (0..1)
    void setPower( double value );
    double getPower() const;
	
private:
	void _open();
	void _close();

    bool _opened;
    
    size_t _pwmRange;
    size_t _pwmFrequency;
    double _pwmCurrentPower;
    
    GPIOPin* _gpioPin;
};

//-----------------------------------------------------------------------------

#endif //__BOILER_H__
