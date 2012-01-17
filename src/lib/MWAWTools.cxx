/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
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
 * For further information visit http://libmwaw.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#include <time.h>
#include <cmath>
#include <sstream>

#include "MWAWTools.hxx"

#include "IMWAWCell.hxx"
#include "IMWAWContentListener.hxx"
#include "TMWAWFont.hxx"

namespace MWAWTools
{
Convertissor::Convertissor() : m_fontManager(new libmwaw_tools_mac::Font)
{ }
Convertissor::~Convertissor() {}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
int Convertissor::getUnicode(MWAWStruct::Font const &f, unsigned char c) const
{
  return m_fontManager->unicode(f.id(),c);
}

WPXString Convertissor::getUnicode(MWAWStruct::Font const &f, std::string const &str) const
{
  int len = str.length();
  WPXString res("");
  for (int l = 0; l < len; l++) {
    long unicode = m_fontManager->unicode(f.id(), str[l]);
    if (unicode != -1) IMWAWContentListener::appendUnicode(unicode,res);
    else if (str[l] >= 28) res.append(str[l]);
  }
  return res;
}

void Convertissor::setFontCorrespondance(int macId, std::string const &name)
{
  m_fontManager->setCorrespondance(macId, name);
}

std::string Convertissor::getFontName(int macId) const
{
  return m_fontManager->getName(macId);
}


int Convertissor::getFontId(std::string const &name) const
{
  return m_fontManager->getId(name);
}

void Convertissor::getFontOdtInfo(int macId, std::string &name, int &deltaSize) const
{
  m_fontManager->getOdtInfo(macId, name, deltaSize);
}

#ifdef DEBUG
std::string Convertissor::getFontDebugString(MWAWStruct::Font const &ft) const
{
  std::stringstream o;
  int flags = ft.flags();
  o << std::dec;
  if (ft.id() != -1)
    o << "nam='" << m_fontManager->getName(ft.id()) << "',";
  if (ft.size() > 0) o << "sz=" << ft.size() << ",";

  if (flags) o << "fl=";
  if (flags&DMWAW_BOLD_BIT) o << "b:";
  if (flags&DMWAW_ITALICS_BIT) o << "it:";
  if (flags&DMWAW_UNDERLINE_BIT) o << "underL:";
  if (flags&DMWAW_EMBOSS_BIT) o << "emboss:";
  if (flags&DMWAW_SHADOW_BIT) o << "shadow:";
  if (flags&DMWAW_DOUBLE_UNDERLINE_BIT) o << "2underL:";
  if (flags&DMWAW_STRIKEOUT_BIT) o << "strikeout:";
  if ((flags&DMWAW_SUPERSCRIPT_BIT) || (flags&DMWAW_SUPERSCRIPT100_BIT))
    o << "superS:";
  if ((flags&DMWAW_SUBSCRIPT_BIT) || (flags&DMWAW_SUBSCRIPT100_BIT))
    o << "subS:";
  if (flags) o << ",";

  if (ft.hasColor()) {
    int col[3];
    ft.getColor(col);
    o << "col=(";
    for (int i = 0; i < 3; i++) o << col[i] << ",";
    o << "),";
  }
  return o.str();
}
#endif

}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
