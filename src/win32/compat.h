#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

#include <io.h>
#include <fcntl.h>

#define read _read
#define write _write
#define strncasecmp _strnicmp
#define strcasecmp _stricmp

#endif
