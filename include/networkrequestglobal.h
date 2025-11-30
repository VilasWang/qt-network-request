#pragma once

#include <QtCore/qglobal.h>

#ifndef QT_MTNETWORK_STATIC

#ifdef QT_MTNETWORK_LIB
#define NETWORK_EXPORT Q_DECL_EXPORT
#else
#define NETWORK_EXPORT Q_DECL_IMPORT
#endif

#else

#define NETWORK_EXPORT

#endif
