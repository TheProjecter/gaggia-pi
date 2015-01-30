//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#include <ios>
#include <iomanip>

#include "singleton.h"
#include "logger.h"
#include "utils.h"
#include "settings.h"

//-----------------------------------------------------------------------------

Settings::Settings() 
    :_opened( false )
{
    _open();
}

//-----------------------------------------------------------------------------

Settings::~Settings() {
    _close();
}

//-----------------------------------------------------------------------------

bool Settings::ready() const {
    return _opened;
}

//-----------------------------------------------------------------------------

double Settings::getFlowOffset30() const {
    if ( !_opened ) {
        return 0.0;
    }

    std::lock_guard<std::mutex> lock( *_mutex );

    return _flowOffset30;
}

//-----------------------------------------------------------------------------

double Settings::getFlowOffset60() const {
    if ( !_opened ) {
        return 0.0;
    }

    std::lock_guard<std::mutex> lock( *_mutex );

    return _flowOffset60;
}

//-----------------------------------------------------------------------------

std::string Settings::getPath() const {
    return _path;
}

//-----------------------------------------------------------------------------

void Settings::getRegulatorSettings( bool steam, double& iGain, double& pGain, double& dGain, double& targetTemperature ) const {
    if ( !_opened ) {
        return;
    }

    std::lock_guard<std::mutex> lock( *_mutex );

    if ( steam ) {
        iGain = _iSteamGain;
        pGain = _pSteamGain;
        dGain = _dSteamGain;
        targetTemperature = _steamTargetTemperature;
    }
    else {
        iGain = _iDefaultGain;
        pGain = _pDefaultGain;
        dGain = _dDefaultGain;
        targetTemperature = _defaultTargetTemperature;
    }
}

//-----------------------------------------------------------------------------

void Settings::getPreHeatingSettings( double& time, double& temperature ) const {
    if ( !_opened ) {
        return;
    }

    std::lock_guard<std::mutex> lock( *_mutex );

    time = _preHeatingTime;
    temperature = _preHeatingTargetTemperature;
}

//-----------------------------------------------------------------------------

bool Settings::_open() {
    _path = Utils::getApplicationPath();

    std::ifstream file;
    file.open( _path + "/settings.cfg" );

    if ( file.is_open() ) {
        std::string placeholder;
        file >> placeholder >> _iDefaultGain
             >> placeholder >> _pDefaultGain
             >> placeholder >> _dDefaultGain
             >> placeholder >> _iSteamGain
             >> placeholder >> _pSteamGain
             >> placeholder >> _dSteamGain
             >> placeholder >> _defaultTargetTemperature
             >> placeholder >> _steamTargetTemperature
             >> placeholder >> _preHeatingTargetTemperature
             >> placeholder >> _preHeatingTime
             >> placeholder >> _flowOffset30
             >> placeholder >> _flowOffset60;

        file.close();
    }
    else {
        LogWarning("No configuration file found, loading default settings");
        _loadDefaults();
    }

    _mutex = new std::mutex();

    _opened = true;
    return true;
}

//-----------------------------------------------------------------------------

void Settings::_close() {
    std::ofstream file;
    file.open( _path + "/settings.cfg", std::ios::trunc );

    if ( !file.is_open() ) {
        LogError("Could not store settings file");
    }
    else {
        file << "iDefaultGain "                << std::fixed << std::setprecision(2) << _iDefaultGain                << std::endl
             << "pDefaultGain "                << std::fixed << std::setprecision(2) << _pDefaultGain                << std::endl
             << "dDefaultGain "                << std::fixed << std::setprecision(2) << _dDefaultGain                << std::endl
             << "iSteamGain "                  << std::fixed << std::setprecision(2) << _iSteamGain                  << std::endl
             << "pSteamGain "                  << std::fixed << std::setprecision(2) << _pSteamGain                  << std::endl
             << "dSteamGain "                  << std::fixed << std::setprecision(2) << _dSteamGain                  << std::endl
             << "defaultTargetTemperature "    << std::fixed << std::setprecision(1) << _defaultTargetTemperature    << std::endl
             << "steamTargetTemperature "      << std::fixed << std::setprecision(1) << _steamTargetTemperature      << std::endl
             << "preHeatingTargetTemperature " << std::fixed << std::setprecision(1) << _preHeatingTargetTemperature << std::endl
             << "preHeatingTime "              << std::fixed << std::setprecision(0) << _preHeatingTime              << std::endl
             << "flowOffset30 "                << std::fixed << std::setprecision(1) << _flowOffset30                << std::endl
             << "flowOffset60 "                << std::fixed << std::setprecision(1) << _flowOffset60                << std::endl;

        file.close();
    }

    if ( _mutex ) {
        delete _mutex;
    }

    return;
}

//-----------------------------------------------------------------------------

void Settings::_loadDefaults() {
    _iDefaultGain = 0.05;
    _pDefaultGain = 0.07;
    _dDefaultGain = 0.90;
    _iSteamGain = 0.05;
    _pSteamGain = 0.07;
    _dSteamGain = 0.90;
    
    _defaultTargetTemperature = 93.0;
    _steamTargetTemperature = 125.0;
    _preHeatingTargetTemperature = 100.0;

    _preHeatingTime = 600;

    _flowOffset30 = 7.5;
    _flowOffset60 = 15.0;
}

//-----------------------------------------------------------------------------
