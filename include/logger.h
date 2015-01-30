//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <sstream>
#include <fstream>
#include <map>
#include <time.h>
#include <mutex>

#include "singleton.h"

//-----------------------------------------------------------------------------

class Log {
public:
    enum LogSeverity {
        LS_None,
        LS_Info,
        LS_Message,
        LS_Warning,
        LS_Error,
        LS_Critical
    };
        
    virtual void addMessage(LogSeverity logSeverity, std::ostringstream& oss) = 0;

    Log(LogSeverity mls);
    virtual ~Log();

protected:
    LogSeverity _minSeverity;
};

//-----------------------------------------------------------------------------

class ConsoleLog : public Log {
public:
    ConsoleLog(LogSeverity minSeverity = LS_Warning);
    ~ConsoleLog();
    void addMessage(LogSeverity logSeverity, std::ostringstream& oss);

private:
    mutable std::mutex _mutex;
};

//-----------------------------------------------------------------------------

class FileLog : public Log {

public:
    FileLog(std::string filename, LogSeverity minSeverity = LS_Info);
    ~FileLog();

    void addMessage(LogSeverity logSeverity, std::ostringstream& oss);

private:
    std::ofstream* _file;
    mutable std::mutex _mutex;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

class Logger {
public:
    /// ctor
    ///
    Logger();

    /// dtor
    ///
    ~Logger();
    
    /// Enable log output on console
    ///
    void enableConsoleLog(Log::LogSeverity severity = Log::LS_Warning);

    /// Disable log output on console
    ///
    void disableConsoleLog();

    /// Add log output to given file
    ///
    void addFileLog(const std::string& filename, Log::LogSeverity severity = Log::LS_Warning);

    /// Remove log output from given file
    ///
    void removeFileLog(const std::string& filename);

    /// Add a message to log
    ///
    void addMessage(Log::LogSeverity severity, std::ostringstream& oss);

private:
    /// Collection of attached logs
    ///
    std::map<std::string, Log*> _logmap;

    /// Console log
    ///
    Log* _console;

};

// macros
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#define LOG_DESCRIPTION "\n\tAt: " << __PRETTY_FUNCTION__ << " in \"" << __FILE__ << "\" (line: " << __LINE__ << ")\n"

//-----------------------------------------------------------------------------

#define LOGGER_TIME_FORMAT(oss)\
{\
    time_t rawtime;\
    struct tm * timeinfo;\
    time (&rawtime);\
    timeinfo = localtime (&rawtime);\
    oss << "[" << (timeinfo->tm_year + 1900) << "-" << (timeinfo->tm_mon + 1) << "-" << timeinfo->tm_mday << " " << timeinfo->tm_hour << ":" << timeinfo->tm_min << ":" << timeinfo->tm_sec << "] ";\
}

//-----------------------------------------------------------------------------

#define LogError(msg)\
{\
    std::ostringstream oss;\
    LOGGER_TIME_FORMAT(oss)\
    oss << "ERROR: " << msg;\
    oss << LOG_DESCRIPTION;\
    Singleton<Logger>::reference().addMessage(Log::LS_Error, oss);\
}

#define LogCritical(msg)\
{\
    std::ostringstream oss;\
    LOGGER_TIME_FORMAT(oss)\
    oss << "CRITICAL: " << msg;\
    oss << LOG_DESCRIPTION;\
    Singleton<Logger>::reference().addMessage(Log::LS_Critical, oss);\
}

#define LogInfo(msg)\
{\
    std::ostringstream oss;\
    LOGGER_TIME_FORMAT(oss)\
    oss<<"INFO: "<<msg;\
    oss << "\n";\
    Singleton<Logger>::reference().addMessage(Log::LS_Info, oss);\
}

#define LogMessage(msg)\
{\
    std::ostringstream oss;\
    LOGGER_TIME_FORMAT(oss)\
    oss<<"MESSAGE: "<<msg;\
    oss << "\n";\
    Singleton<Logger>::reference().addMessage(Log::LS_Message, oss);\
}

#define LogWarning(msg)\
{\
    std::ostringstream oss;\
    LOGGER_TIME_FORMAT(oss)\
    oss << "WARNING: " << msg;\
    oss << LOG_DESCRIPTION;\
    Singleton<Logger>::reference().addMessage(Log::LS_Warning, oss);\
}

#define LogCustom(msg)\
{\
    std::ostringstream oss;\
    oss << msg << "\n";\
    Singleton<Logger>::reference().addMessage(Log::LS_Info, oss);\
}

#endif // __LOGGER_H__
