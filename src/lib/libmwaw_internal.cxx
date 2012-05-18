/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libwpd
 * Copyright (C) 2002, 2005 William Lachance (wrlach@gmail.com)
 * Copyright (C) 2002, 2004 Marc Maurer (uwog@uwog.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * For further information visit http://libwpd.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */
#include "libmwaw_internal.hxx"
#include <libwpd-stream/WPXStream.h>
#include <ctype.h>
#include <locale.h>
#include <string>

// OSNOLA
/** namespace used to regroup all libwpd functions, enumerations which we have redefined for internal usage */
namespace libmwaw
{
uint8_t readU8(WPXInputStream *input)
{
  unsigned long numBytesRead;
  uint8_t const *p = input->read(sizeof(uint8_t), numBytesRead);

  if (!p || numBytesRead != sizeof(uint8_t))
    throw libmwaw::FileException();

  return *p;
}
}

namespace libmwaw
{
std::string numberingTypeToString(NumberingType type)
{
  switch (type) {
  case ARABIC:
    return "1";
  case LOWERCASE:
    return "a";
  case UPPERCASE:
    return "A";
  case LOWERCASE_ROMAN:
    return "i";
  case UPPERCASE_ROMAN:
    return "I";
  default:
    break;
  }
  MWAW_DEBUG_MSG(("libmwaw::numberingTypeToString: must not be called with type %d\n", int(type)));
  return "1";
}
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
