//-----------------------------------------------------------------------------
//
// Gaggia-PI: Raspberry PI Controller for the Gaggia Classic Coffee
//
//  Copyright 2014, 2015 by it's authors. 
//  Some rights reserved. See COPYING, AUTHORS.
//
//-----------------------------------------------------------------------------

#include <sys/types.h>
#include <unistd.h>
#include <string>

#include "utils.h"

std::string Utils::getApplicationPath() {
    // Extract application path (from: http://stackoverflow.com/questions/143174/how-do-i-get-the-directory-that-a-program-is-running-from, accessed: 30.01.2015) 
    char buf[256];
    char szTmp[32];
    size_t len = 256;

    sprintf( szTmp, "/proc/%d/exe", getpid() );
    int bytes = readlink( szTmp, buf, len );
    if ( bytes >= 0 ) {
	    buf[bytes] = '\0';
    }

    std::string directory = std::string( buf );
    const unsigned int found = directory.find_last_of("/\\");
    return directory.substr(0, found);
}
