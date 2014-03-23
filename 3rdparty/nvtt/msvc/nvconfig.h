#ifndef NV_CONFIG
#define NV_CONFIG

#define HAVE_STDARG_H
#define HAVE_MALLOC_H

#if !defined(_M_X64)
#define HAVE_PNG
#define HAVE_JPEG
#endif

#if defined(_MSC_VER) && (_MSC_VER>=1700)
#define HAVE_XTGMATH_H
#endif

#endif // NV_CONFIG
