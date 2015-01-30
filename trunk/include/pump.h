//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#ifndef __PUMP_H__
#define __PUMP_H__

//-----------------------------------------------------------------------------

#include <stdlib.h>

//-----------------------------------------------------------------------------

// Forward decls
class GPIOPin;

//-----------------------------------------------------------------------------

class Pump {
public:
    Pump();
    ~Pump();
    
	bool ready() const;
    void setPower( bool power );
    bool getPower() const;
	
private:
	void _open();
	void _close();

    bool _opened;
	bool _power; 
    
    GPIOPin* _gpioPin;
};

//-----------------------------------------------------------------------------

#endif // __PUMP_H__

