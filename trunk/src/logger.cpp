//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#include <iostream>

#include "logger.h"

//-----------------------------------------------------------------------------
// console color

namespace intern {

inline std::ostream& standard(std::ostream &s)	{ return (s << "\033[0m"); }
inline std::ostream& gray(std::ostream &s)		{ return (s << "\033[0;37m"); }

inline std::ostream& cyan(std::ostream &s)		{ return (s << "\033[0;36m");}
inline std::ostream& magenta(std::ostream &s)	{ return (s << "\033[0;35m"); }
inline std::ostream& yellow(std::ostream &s)	{ return (s << "\033[0;35m"); }

inline std::ostream& red(std::ostream &s)		{ return (s << "\033[1;31m"); }
inline std::ostream& green(std::ostream &s)		{ return (s << "\033[0;32m"); }
inline std::ostream& blue(std::ostream &s)		{ return (s << "\033[0;34m"); }

} // namespace intern

//-----------------------------------------------------------------------------

// helper function
inline std::ostream& operator<<(std::ostream &os, Log::LogSeverity& ls) {
	switch (ls) {
		case Log::LS_None:
			return (os << intern::standard);
		case Log::LS_Info:
			return (os << intern::green << "");
		case Log::LS_Message:
			return (os << intern::green << "");
		case Log::LS_Warning:
            return (os << intern::yellow << "");
		case Log::LS_Error:
			return (os << intern::red << "");
		case Log::LS_Critical:
			return (os << intern::cyan << "");
	}
	return (os << "[UNKNOWN]");
}


//-----------------------------------------------------------------------------

Log::Log(LogSeverity mls) 
	:_minSeverity(mls)
{
}

//-----------------------------------------------------------------------------

Log::~Log() {
}

//-----------------------------------------------------------------------------

ConsoleLog::ConsoleLog(LogSeverity minSeverity) 
	:Log(minSeverity)
{
}

//-----------------------------------------------------------------------------

ConsoleLog::~ConsoleLog() {
}

//-----------------------------------------------------------------------------

void ConsoleLog::addMessage(LogSeverity logSeverity, std::ostringstream& oss) {
    if ( logSeverity >= _minSeverity ) {
		std::lock_guard<std::mutex> lock( _mutex );
        std::cout << logSeverity << oss.str() << intern::standard;
        std::cout.flush();
    }
}

//-----------------------------------------------------------------------------

FileLog::FileLog(std::string filename, LogSeverity minSeverity) 
    :Log( minSeverity )
	,_file( nullptr )
{    
	_file = new std::ofstream();
	_file->open(filename.c_str(), std::ios_base::app);
}

//-----------------------------------------------------------------------------

FileLog::~FileLog() {
    if ( _file != nullptr && _file->good() ) {
        _file->close();
		delete _file;
	}
}

//-----------------------------------------------------------------------------

void FileLog::addMessage( LogSeverity logSeverity, std::ostringstream& oss ) {
    if ( _file != nullptr && _file->good() && logSeverity >= _minSeverity ) {
		std::lock_guard<std::mutex> lock( _mutex );		
        (*_file) << oss.str();
        _file->flush();
    }
}

//-----------------------------------------------------------------------------

Logger::Logger() 
	:_console(0)
{
    //nothing else to do here
}

//-----------------------------------------------------------------------------

Logger::~Logger() {
    delete _console;

	for (std::map<std::string, Log*>::iterator iter = _logmap.begin();
		iter != _logmap.end();
		iter++) 
	{
		delete iter->second;
	}
}

//-----------------------------------------------------------------------------

void Logger::enableConsoleLog(Log::LogSeverity severity) {
    delete _console;
    _console = new ConsoleLog(severity);
}

//-----------------------------------------------------------------------------

void Logger::disableConsoleLog() {
    delete _console;
    _console = 0;
}

//-----------------------------------------------------------------------------

void Logger::addFileLog(const std::string& filename, Log::LogSeverity severity) {
    std::map<std::string, Log*>::iterator it = _logmap.find(filename);
    if(it == _logmap.end()) {
        _logmap.insert(std::make_pair(filename, new FileLog(filename, severity)));
    }
}

//-----------------------------------------------------------------------------

void Logger::removeFileLog(const std::string& filename) {
    std::map<std::string, Log*>::iterator it = _logmap.find(filename);
    if(it != _logmap.end()) {
        _logmap.erase(it);
    }
}

//-----------------------------------------------------------------------------

void Logger::addMessage(Log::LogSeverity severity, std::ostringstream& oss) {
    if(_console != 0) {
        _console->addMessage(severity, oss);
	}

	for (std::map<std::string, Log*>::iterator iter = _logmap.begin();
		iter != _logmap.end();
		iter++) 
	{
		 if(iter->second != 0) {
            iter->second->addMessage(severity, oss);
		 }
	}
}

//-----------------------------------------------------------------------------
