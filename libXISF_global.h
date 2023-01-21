#ifndef LIBXISF_GLOBAL_H
#define LIBXISF_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(LIBXISF_LIBRARY)
#  define LIBXISF_EXPORT Q_DECL_EXPORT
#else
#  define LIBXISF_EXPORT Q_DECL_IMPORT
#endif

#endif // LIBXISF_GLOBAL_H
