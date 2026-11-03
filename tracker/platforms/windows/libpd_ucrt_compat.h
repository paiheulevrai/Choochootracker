#ifndef CHOOCHOO_LIBPD_UCRT_COMPAT_H
#define CHOOCHOO_LIBPD_UCRT_COMPAT_H

#ifdef _WIN32
#include <windows.h>
#ifndef _waccess
#define _waccess(path, mode) \
  (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES ? -1 : 0)
#endif
#endif

#endif
