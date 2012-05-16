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

_RGBSColor::_RGBSColor(uint8_t r, uint8_t g, uint8_t b, uint8_t s)
  :	m_r(r),
    m_g(g),
    m_b(b),
    m_s(s)
{
}

_RGBSColor::_RGBSColor()
  :	m_r(0),
    m_g(0),
    m_b(0),
    m_s(0)
{
}

_RGBSColor::_RGBSColor(uint16_t red, uint16_t green, uint16_t blue)
  :	m_r((uint8_t)((red >> 8) & 0xFF)),
    m_g((uint8_t)((green >> 8) & 0xFF)),
    m_b((uint8_t)((blue >> 8) & 0xFF)),
    m_s(100)
{
}
}

_DMWAWTabStop::_DMWAWTabStop(double position, DMWAWTabAlignment alignment, uint16_t leaderCharacter, uint8_t leaderNumSpaces)
  :	m_position(position),
    m_alignment(alignment),
    m_leaderCharacter(leaderCharacter),
    m_leaderNumSpaces(leaderNumSpaces)
{
}

_DMWAWTabStop::_DMWAWTabStop()
  :	m_position(0.0),
    m_alignment(LEFT),
    m_leaderCharacter('\0'),
    m_leaderNumSpaces(0)
{
}
void _DMWAWTabStop::printTabs(std::ostream &o, std::vector<DMWAWTabStop> const &tabs)
{
  int nbTabs = tabs.size();
  if (!nbTabs) return;

  o << "tabs=(" << std::dec;
  for (int i = 0; i < nbTabs; i++) {
    o << tabs[i].m_position;

    switch (tabs[i].m_alignment) {
    case LEFT:
      o << "L";
      break;
    case CENTER:
      o << "C";
      break;
    case RIGHT:
      o << "R";
      break;
    case DECIMAL:
      o << ":decimal";
      break; // decimal align
    case BAR:
      o << ":bar";
      break;
    default:
      break;
    }
    if (tabs[i].m_leaderCharacter != '\0')
      o << ":sep='"<< (char) tabs[i].m_leaderCharacter << "'";
    o << ",";
  }
  o << ")";
}

_DMWAWColumnDefinition::_DMWAWColumnDefinition()
  :	m_width(0.0),
    m_leftGutter(0.0),
    m_rightGutter(0.0)
{
}

_DMWAWColumnProperties::_DMWAWColumnProperties()
  :	m_attributes(0x00000000),
    m_alignment(0x00)
{
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


WPXString doubleToString(const double value)
{
  WPXString tempString;
  if (value < 0.0001 && value > -0.0001)
    tempString.sprintf("0.0000");
  else
    tempString.sprintf("%.4f", value);
#ifndef __ANDROID__
  std::string decimalPoint(localeconv()->decimal_point);
#else
  std::string decimalPoint(".");
#endif
  if ((decimalPoint.size() == 0) || (decimalPoint == "."))
    return tempString;
  std::string stringValue(tempString.cstr());
  if (!stringValue.empty()) {
    std::string::size_type pos;
    while ((pos = stringValue.find(decimalPoint)) != std::string::npos)
      stringValue.replace(pos,decimalPoint.size(),".");
  }
  return WPXString(stringValue.c_str());
}
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
