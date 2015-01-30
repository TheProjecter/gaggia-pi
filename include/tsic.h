#ifndef __TSIC_H__
#define __TSIC_H__

//-----------------------------------------------------------------------------
//
// Copyright (C) 2014-2015 James Ward
// Copyright (C) 2015 Alexander Giesler
//
// This software is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
//-----------------------------------------------------------------------------

#include <inttypes.h>
#include <mutex>

//-----------------------------------------------------------------------------

// Forward decls
class GPIOPin;

//-----------------------------------------------------------------------------

/// TSIC Temperature Sensor Class, designed for use with the TSIC 306 sensor,
/// but may be adaptable for use with other devices of the same class.
class TSIC
{
public:
    TSIC( unsigned gpio );
    ~TSIC();

    bool getDegrees( double& value ) const;
    bool ready() const;

private:
    void _open();
    void _close();
    void _alertFunction( int gpio, int level, uint32_t tick );

	GPIOPin* _pin;

    unsigned _gpio;        ///< the GPIO pin used for the sensor
    bool     _opened;      ///< true if the sensor is open
    bool     _valid;       ///< temperature data is valid
    double   _temperature; ///< current temperature

    uint32_t _count;       ///< number of bits received in current packet
    uint32_t _lastLow;     ///< time when GPIO pin last went low (us)
	uint32_t _lastHigh;
    int      _word;        ///< used to consolidate incoming packet bits

	bool _running;

    /// Mutex to control access to the sensor data
    mutable std::mutex _mutex;
};

//-----------------------------------------------------------------------------

#endif // __TSIC_H__
