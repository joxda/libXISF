/************************************************************************
 * LibXISF - library to load and save XISF files                        *
 * Copyright (C) 2023 Dušan Poizl                                       *
 *                                                                      *
 * This program is free software: you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or    *
 * (at your option) any later version.                                  *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.*
 ************************************************************************/

#ifndef LIBXISF_GLOBAL_H
#define LIBXISF_GLOBAL_H

#include <QtCore/qglobal.h>

#ifdef LIBXISF_SHARED_LIB
#  if defined(LIBXISF_LIBRARY)
#    define LIBXISF_EXPORT Q_DECL_EXPORT
#  else
#    define LIBXISF_EXPORT Q_DECL_IMPORT
#  endif
#else
#  define LIBXISF_EXPORT
#endif

#endif // LIBXISF_GLOBAL_H
