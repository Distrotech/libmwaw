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
#include <iomanip>
#include <string>
#include <sstream>

#include <ctype.h>
#include <locale.h>

#include <libwpd-stream/WPXStream.h>

#include "libmwaw_internal.hxx"

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
  case NONE:
  case BULLET:
  default:
    break;
  }
  MWAW_DEBUG_MSG(("libmwaw::numberingTypeToString: must not be called with type %d\n", int(type)));
  return "1";
}
}

int MWAWBorder::compare(MWAWBorder const &orig) const
{
  int diff = int(m_style)-int(orig.m_style);
  if (diff) return diff;
  diff = m_width-orig.m_width;
  if (diff) return diff;
  if (m_color < orig.m_color) return -1;
  if (m_color > orig.m_color) return -1;
  return 0;
}

std::string MWAWBorder::getPropertyValue() const
{
  if (m_style == None) return "";
  std::stringstream stream;
  stream << m_width*0.03 << "cm";
  switch (m_style) {
  case Single:
  case Dot:
  case LargeDot:
  case Dash:
    stream << " solid";
    break;
  case Double:
    stream << " double";
    break;
  case None:
  default:
    break;
  }
  stream << " #" << std::hex << std::setfill('0') << std::setw(6)
         << (m_color&0xFFFFFF);
  return stream.str();
}

std::ostream &operator<< (std::ostream &o, MWAWBorder const &border)
{
  switch (border.m_style) {
  case MWAWBorder::None:
    o << "none:";
    break;
  case MWAWBorder::Single:
    break;
  case MWAWBorder::Dot:
    o << "dot:";
    break;
  case MWAWBorder::LargeDot:
    o << "large dot:";
    break;
  case MWAWBorder::Dash:
    o << "dash:";
    break;
  case MWAWBorder::Double:
    o << "double:";
    break;
  default:
    MWAW_DEBUG_MSG(("MWAWBorder::operator<<: find unknown style\n"));
    o << "#style=" << int(border.m_style) << ":";
    break;
  }
  if (border.m_width > 1) o << "w=" << border.m_width << ":";
  if (border.m_color)
    o << "col=" << std::hex << border.m_color << std::dec << ":";
  o << ",";
  return o;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
