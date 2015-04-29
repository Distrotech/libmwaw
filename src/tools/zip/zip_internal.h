/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */

/* libmwaw: tools
* Version: MPL 2.0 / LGPLv2+
*
* The contents of this file are subject to the Mozilla Public License Version
* 2.0 (the "License"); you may not use this file except in compliance with
* the License or as specified alternatively below. You may obtain a copy of
* the License at http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* Major Contributor(s):
* Copyright (C) 2011, 2012 Alonso Laurent (alonso@loria.fr)
*
*
* All Rights Reserved.
*
* For minor contributions see the git repository.
*
* Alternatively, the contents of this file may be used under the terms of
* the GNU Lesser General Public License Version 2 or later (the "LGPLv2+"),
* in which case the provisions of the LGPLv2+ are applicable
* instead of those above.
*/

#ifndef ZIP_INTERNAL_H
#  define ZIP_INTERNAL_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include <stdio.h>

#ifdef DEBUG
#define MWAW_DEBUG_MSG(M) printf M
#else
#define MWAW_DEBUG_MSG(M)
#endif

#if defined(SHAREDPTR_TR1)
#include <tr1/memory>
using std::tr1::shared_ptr;
#elif defined(SHAREDPTR_STD)
#include <memory>
using std::shared_ptr;
#else
#include <boost/shared_ptr.hpp>
using boost::shared_ptr;
#endif

#if defined(_MSC_VER) || defined(__DJGPP__)

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;
typedef __int64 int64_t;

#else /* !_MSC_VER && !__DJGPP__*/

#  ifdef HAVE_CONFIG_H

#    include <config.h>
#    ifdef HAVE_STDINT_H
#      include <stdint.h>
#    endif
#    ifdef HAVE_INTTYPES_H
#      include <inttypes.h>
#    endif

#  else

// assume that the headers are there inside LibreOffice build when no HAVE_CONFIG_H is defined
#    include <stdint.h>
#    include <inttypes.h>

#  endif

#endif

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
