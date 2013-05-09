/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */

/* libmwaw
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
* Copyright (C) 2002 William Lachance (wrlach@gmail.com)
* Copyright (C) 2002,2004 Marc Maurer (uwog@uwog.net)
* Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
* Copyright (C) 2006, 2007 Andrew Ziem
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

#include <iomanip>
#include <string>
#include <sstream>

#include <ctype.h>
#include <locale.h>

#include <libwpd-stream/libwpd-stream.h>

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

void appendUnicode(uint32_t val, WPXString &buffer)
{
  uint8_t first;
  int len;
  if (val < 0x80) {
    first = 0;
    len = 1;
  } else if (val < 0x800) {
    first = 0xc0;
    len = 2;
  } else if (val < 0x10000) {
    first = 0xe0;
    len = 3;
  } else if (val < 0x200000) {
    first = 0xf0;
    len = 4;
  } else if (val < 0x4000000) {
    first = 0xf8;
    len = 5;
  } else {
    first = 0xfc;
    len = 6;
  }

  uint8_t outbuf[6] = { 0, 0, 0, 0, 0, 0 };
  int i;
  for (i = len - 1; i > 0; --i) {
    outbuf[i] = uint8_t((val & 0x3f) | 0x80);
    val >>= 6;
  }
  outbuf[0] = uint8_t(val | first);
  for (i = 0; i < len; i++) buffer.append((char)outbuf[i]);
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

std::string numberingValueToString(NumberingType type, int value)
{
  std::stringstream ss;
  std::string s("");
  switch(type) {
  case ARABIC:
    ss << value;
    return ss.str();
  case LOWERCASE:
  case UPPERCASE:
    if (value <= 0) {
      MWAW_DEBUG_MSG(("libmwaw::numberingValueToString: value can not be negative or null for type %d\n", int(type)));
      return (type == LOWERCASE) ? "a" : "A";
    }
    while (value > 0) {
      s = char((type == LOWERCASE ? 'a' : 'A')+((value-1)%26))+s;
      value = (value-1)/26;
    }
    return s;
  case LOWERCASE_ROMAN:
  case UPPERCASE_ROMAN: {
    static char const *(romanS[]) = {"M", "CM", "D", "CD", "C", "XC", "L",
                                     "XL", "X", "IX", "V", "IV", "I"
                                    };
    static char const *(romans[]) = {"m", "cm", "d", "cd", "c", "xc", "l",
                                     "xl", "x", "ix", "v", "iv", "i"
                                    };
    static int const (romanV[]) = {1000, 900, 500, 400,  100, 90, 50,
                                   40, 10, 9, 5, 4, 1
                                  };
    if (value <= 0 || value >= 4000) {
      MWAW_DEBUG_MSG(("libmwaw::numberingValueToString: out of range value for type %d\n", int(type)));
      return (type == LOWERCASE_ROMAN) ? "i" : "I";
    }
    for (int p = 0; p < 13; p++) {
      while (value >= romanV[p]) {
        ss << ((type == LOWERCASE_ROMAN) ? romans[p] : romanS[p]);
        value -= romanV[p];
      }
    }
    return ss.str();
  }
  case NONE:
    return "";
    break;
  case BULLET:
  default:
    MWAW_DEBUG_MSG(("libmwaw::numberingValueToString: must not be called with type %d\n", int(type)));
    break;
  }
  return "";
}
}

// color function
MWAWColor MWAWColor::barycenter(float alpha, MWAWColor const &colA,
                                float beta, MWAWColor const &colB)
{
  uint32_t res = 0;
  for (int i=0, depl=0; i<4; i++, depl+=8) {
    unsigned char comp= (unsigned char)
                        (alpha*float((colA.m_value>>depl)&0xFF)+beta*float((colB.m_value>>depl)&0xFF));
    res+=uint32_t(comp<<depl);
  }
  return res;
}

std::ostream &operator<< (std::ostream &o, MWAWColor const &c)
{
  const std::streamsize width = o.width();
  const char fill = o.fill();
  o << "#" << std::hex << std::setfill('0') << std::setw(6)
    << (c.m_value&0xFFFFFF)
    // std::ios::width() takes/returns std::streamsize (long), but
    // std::setw() takes int. Go figure...
    << std::dec << std::setfill(fill) << std::setw(static_cast<int>(width));
  return o;
}

std::string MWAWColor::str() const
{
  std::stringstream stream;
  stream << *this;
  return stream.str();
}

// border function
int MWAWBorder::compare(MWAWBorder const &orig) const
{
  int diff = int(m_style)-int(orig.m_style);
  if (diff) return diff;
  diff = int(m_type)-int(orig.m_type);
  if (diff) return diff;
  if (m_width < orig.m_width) return -1;
  if (m_width > orig.m_width) return 1;
  if (m_color < orig.m_color) return -1;
  if (m_color > orig.m_color) return 1;
  return 0;
}
bool MWAWBorder::addTo(WPXPropertyList &propList, std::string const which) const
{
  std::stringstream stream, field;
  stream << m_width << "pt ";
  if (m_type==MWAWBorder::Double || m_type==MWAWBorder::Triple) {
    static bool first = true;
    if (first && m_style!=Simple) {
      MWAW_DEBUG_MSG(("MWAWBorder::addTo: find double or tripe border with complex style\n"));
      first = false;
    }
    stream << "double";
  } else {
    switch (m_style) {
    case Dot:
    case LargeDot:
      stream << "dotted";
      break;
    case Dash:
      stream << "dashed";
      break;
    case Simple:
      stream << "solid";
      break;
    case None:
    default:
      stream << "none";
      break;
    }
  }
  stream << " " << m_color;
  field << "fo:border";
  if (which.length())
    field << "-" << which;
  propList.insert(field.str().c_str(), stream.str().c_str());
  size_t numRelWidth=m_widthsList.size();
  if (!numRelWidth)
    return true;
  if (m_type!=MWAWBorder::Double || numRelWidth!=3) {
    static bool first = true;
    if (first) {
      MWAW_DEBUG_MSG(("MWAWBorder::addTo: relative width is only implemented with double style\n"));
      first = false;
    }
    return true;
  }
  double totalWidth=0;
  for (size_t w=0; w < numRelWidth; w++)
    totalWidth+=m_widthsList[w];
  if (totalWidth <= 0) {
    MWAW_DEBUG_MSG(("MWAWBorder::addTo: can not compute total width\n"));
    return true;
  }
  double factor=m_width/totalWidth;
  stream.str("");
  for (size_t w=0; w < numRelWidth; w++) {
    stream << factor*m_widthsList[w]<< "pt";
    if (w+1!=numRelWidth)
      stream << " ";
  }
  field.str("");
  field << "style:border-line-width";
  if (which.length())
    field << "-" << which;
  propList.insert(field.str().c_str(), stream.str().c_str());
  return true;
}

std::ostream &operator<< (std::ostream &o, MWAWBorder::Style const &style)
{
  switch (style) {
  case MWAWBorder::None:
    o << "none";
    break;
  case MWAWBorder::Simple:
    break;
  case MWAWBorder::Dot:
    o << "dot";
    break;
  case MWAWBorder::LargeDot:
    o << "large dot";
    break;
  case MWAWBorder::Dash:
    o << "dash";
    break;
  default:
    MWAW_DEBUG_MSG(("MWAWBorder::operator<<: find unknown style\n"));
    o << "#style=" << int(style);
    break;
  }
  return o;
}

std::ostream &operator<< (std::ostream &o, MWAWBorder const &border)
{
  o << border.m_style << ":";
  switch(border.m_type) {
  case MWAWBorder::Single:
    break;
  case MWAWBorder::Double:
    o << "double:";
    break;
  case MWAWBorder::Triple:
    o << "triple:";
    break;
  default:
    MWAW_DEBUG_MSG(("MWAWBorder::operator<<: find unknown type\n"));
    o << "#type=" << int(border.m_type);
    break;
  }
  if (border.m_width > 1 || border.m_width < 1) o << "w=" << border.m_width << ":";
  if (!border.m_color.isBlack())
    o << "col=" << border.m_color << ":";
  o << ",";
  size_t numRelWidth=border.m_widthsList.size();
  if (numRelWidth) {
    o << "bordW[rel]=[";
    for (size_t i=0; i < numRelWidth; i++)
      o << border.m_widthsList[i] << ",";
    o << "],";
  }
  return o;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
