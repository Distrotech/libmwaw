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
Convertissor::Convertissor() : m_fontManager(new libmwaw_tools_mac::Font), m_palette3(), m_palette4()
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
  if (flags&DMWAW_SUPERSCRIPT_BIT) o << "superS:";
  if (flags&DMWAW_SUBSCRIPT_BIT) o << "subS:";
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

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
void Convertissor::initPalettes() const
{
  if (m_palette3.size()) return;

  //
  // v3 color map
  //
  m_palette3.resize(256);
  // the first 6 lines of 32 colors consisted of 6x6x6 blocks(clampsed) with gratuated color
  int ind=0;
  for (int k = 0; k < 6; k++) {
    for (int j = 0; j < 6; j++) {
      for (int i = 0; i < 6; i++, ind++) {
        if (j==5 && i==2) break;
        m_palette3[ind]=Vec3uc(255-51*i, 255-51*k, 255-51*j);
      }
    }
  }

  // the last 2 lines
  for (int r = 0; r < 2; r++) {
    // the black, red, green, blue zone of 5*2
    for (int c = 0; c < 4; c++) {
      for (int i = 0; i < 5; i++, ind++) {
        int val = 17*r+51*i;
        if (c == 0) {
          m_palette3[ind]=Vec3uc(val, val, val);
          continue;
        }
        int col[3]= {0,0,0};
        col[c-1]=val;
        m_palette3[ind]=Vec3uc(col[0],col[1],col[2]);
      }
    }
    // last part of j==5, i=2..5
    for (int k = r; k < 6; k+=2) {
      for (int i = 2; i < 6; i++, ind++) m_palette3[ind]=Vec3uc(255-51*i, 255-51*k, 255-51*5);
    }
  }

  //
  // v4 color map
  //
  m_palette4.resize(256);
  // the first 6 lines of 32 colors consisted of 6x6x6 blocks with gratuated color
  // excepted m_palette4[215]=black -> set latter
  ind=0;
  for (int k = 0; k < 6; k++) {
    for (int j = 0; j < 6; j++) {
      for (int i = 0; i < 6; i++, ind++) {
        m_palette4[ind]=Vec3uc(255-51*k, 255-51*j, 255-51*i);
      }
    }
  }

  ind--; // remove the black color
  for (int c = 0; c < 4; c++) {
    int col[3] = {0,0,0};
    int val=251;
    for (int i = 0; i < 10; i++) {
      val -= 17;
      if (c == 3) m_palette4[ind++]=Vec3uc(val, val, val);
      else {
        col[c] = val;
        m_palette4[ind++]=Vec3uc(col[0],col[1],col[2]);
      }
      if ((i%2)==1) val -=17;
    }
  }

  // last is black
  m_palette4[ind++]=Vec3uc(0,0,0);
}
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
