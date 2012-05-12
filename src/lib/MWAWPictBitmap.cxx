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

/* This header contains code specific to some bitmap
 */

#include <sstream>
#include <string>

#include <libwpd/WPXBinaryData.h>

#include "libmwaw_tools.hxx"

#include "MWAWPictBitmap.hxx"

//! Internal: helper function to create a PBM
template <class T>
bool getPBMData(MWAWPictBitmapContainer<T> const &orig, WPXBinaryData &data, T white)
{
  Vec2i sz = orig.size();
  if (sz[0] <= 0 || sz[1] <= 0) return false;

  data.clear();
  std::stringstream f;
  f << "P4\n" << sz[0] << " " << sz[1] << "\n";
  std::string const &header = f.str();
  data.append((const unsigned char *)header.c_str(), (int)header.size());

  for (int j = 0; j < sz[1]; j++) {
    T const *row = orig.getRow(j);

    unsigned char mask = 0x80, value = 0;
    for (int i = 0; i < sz[0]; i++) {
      if (row[i] != white) value |= mask;
      mask >>= 1;
      if (mask != 0) continue;
      data.append(value);
      value = 0;
      mask = 0x80;
    }
    if (mask!= 0x80) data.append(value);
  }
  return true;
}

//! Internal: helper function to create a PPM
template <class T>
bool getPPMData(MWAWPictBitmapContainer<T> const &orig, WPXBinaryData &data, std::vector<Vec3uc> const &indexedColor)
{
  Vec2i sz = orig.size();
  if (sz[0] <= 0 || sz[1] <= 0) return false;

  int nColors = indexedColor.size();

  data.clear();
  std::stringstream f;
  f << "P6\n" << sz[0] << " " << sz[1] << " 255\n";
  std::string const &header = f.str();
  data.append((const unsigned char *)header.c_str(), (int)header.size());
  for (int j = 0; j < sz[1]; j++) {
    T const *row = orig.getRow(j);

    for (int i = 0; i < sz[0]; i++) {
      int ind = row[i];
      if (ind < 0 || ind >= nColors) {
        MWAW_DEBUG_MSG(("MWAWPictBitmap::getPPMData invalid index %d\n", ind));
        return false;
      }
      Vec3uc const &col = indexedColor[ind];
      for (int c = 0; c < 3; c++) data.append((unsigned char) col[c]);
    }
  }
  return true;
}

//! Internal: helper function to create a PPM for a color bitmap
bool getPPMData(MWAWPictBitmapContainer<Vec3uc> const &orig, WPXBinaryData &data)
{
  Vec2i sz = orig.size();
  if (sz[0] <= 0 || sz[1] <= 0) return false;

  data.clear();
  std::stringstream f;
  f << "P6\n" << sz[0] << " " << sz[1] << " 255\n";
  std::string const &header = f.str();
  data.append((const unsigned char *)header.c_str(), (int)header.size());
  for (int j = 0; j < sz[1]; j++) {
    Vec3uc const *row = orig.getRow(j);

    for (int i = 0; i < sz[0]; i++) {
      Vec3uc const &col = row[i];
      for (int c = 0; c < 3; c++) data.append((unsigned char) col[c]);
    }
  }
  return true;
}

////////////////////////////////////////////////////////////
// BW bitmap
////////////////////////////////////////////////////////////

bool MWAWPictBitmapBW::createFileData(WPXBinaryData &result) const
{
  return getPBMData<bool>(m_data,result,false);
}

////////////////////////////////////////////////////////////
// Color bitmap
////////////////////////////////////////////////////////////

bool MWAWPictBitmapColor::createFileData(WPXBinaryData &result) const
{
  return getPPMData(m_data,result);
}

////////////////////////////////////////////////////////////
// Indexed bitmap
////////////////////////////////////////////////////////////

bool MWAWPictBitmapIndexed::createFileData(WPXBinaryData &result) const
{
  if (m_colors.size() && getPPMData<int>(m_data,result,m_colors)) return true;
  return getPBMData<int>(m_data,result,0);
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
