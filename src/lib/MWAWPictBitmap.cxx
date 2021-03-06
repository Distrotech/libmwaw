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

/* This header contains code specific to some bitmap
 */

#include <sstream>
#include <string>

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"

#include "MWAWPictBitmap.hxx"

//! Internal: helper function to create a PBM
template <class T>
bool getPBMData(MWAWPictBitmapContainer<T> const &orig, librevenge::RVNGBinaryData &data, T white)
{
  Vec2i sz = orig.size();
  if (sz[0] <= 0 || sz[1] <= 0) return false;

  data.clear();
  std::stringstream f;
  f << "P4\n" << sz[0] << " " << sz[1] << "\n";
  std::string const &header = f.str();
  data.append((const unsigned char *)header.c_str(), header.size());

  for (int j = 0; j < sz[1]; j++) {
    T const *row = orig.getRow(j);

    unsigned char mask = 0x80, value = 0;
    for (int i = 0; i < sz[0]; i++) {
      if (row[i] != white) value |= mask;
      mask = (unsigned char)(mask >> 1);
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
bool getPPMData(MWAWPictBitmapContainer<T> const &orig, librevenge::RVNGBinaryData &data, std::vector<MWAWColor> const &indexedColor)
{
  Vec2i sz = orig.size();
  if (sz[0] <= 0 || sz[1] <= 0) return false;

  int nColors = int(indexedColor.size());

  data.clear();
  std::stringstream f;
  f << "P6\n" << sz[0] << " " << sz[1] << " 255\n";
  std::string const &header = f.str();
  data.append((const unsigned char *)header.c_str(), header.size());
  for (int j = 0; j < sz[1]; j++) {
    T const *row = orig.getRow(j);

    for (int i = 0; i < sz[0]; i++) {
      int ind = row[i];
      if (ind < 0 || ind >= nColors) {
        MWAW_DEBUG_MSG(("MWAWPictBitmap::getPPMData invalid index %d\n", ind));
        return false;
      }
      uint32_t col = indexedColor[size_t(ind)].value();
      for (int c = 0, depl=16; c < 3; c++, depl-=8)
        data.append((unsigned char)((col>>depl)&0xFF));
    }
  }
  return true;
}

//! Internal: namespace used to define some internal function
namespace MWAWPictBitmapInternal
{
//! Internal: helper function to create a PPM for a color bitmap
static bool getPPMData(MWAWPictBitmapContainer<MWAWColor> const &orig, librevenge::RVNGBinaryData &data)
{
  Vec2i sz = orig.size();
  if (sz[0] <= 0 || sz[1] <= 0) return false;

  data.clear();
  std::stringstream f;
  f << "P6\n" << sz[0] << " " << sz[1] << " 255\n";
  std::string const &header = f.str();
  data.append((const unsigned char *)header.c_str(), header.size());
  for (int j = 0; j < sz[1]; j++) {
    MWAWColor const *row = orig.getRow(j);

    for (int i = 0; i < sz[0]; i++) {
      uint32_t col = row[i].value();
      for (int c = 0, depl=16; c < 3; c++, depl-=8)
        data.append((unsigned char)((col>>depl)&0xFF));
    }
  }
  return true;
}

//
// functions used by getPBMData (freely inspired from libpwg::WPGBitmap.cpp)
//
static void writeU16(unsigned char *buffer, unsigned &position, const unsigned value)
{
  buffer[position++] = (unsigned char)(value & 0xFF);
  buffer[position++] = (unsigned char)((value >> 8) & 0xFF);
}

static void writeU32(unsigned char *buffer, unsigned &position, const unsigned value)
{
  buffer[position++] = (unsigned char)(value & 0xFF);
  buffer[position++] = (unsigned char)((value >> 8) & 0xFF);
  buffer[position++] = (unsigned char)((value >> 16) & 0xFF);
  buffer[position++] = (unsigned char)((value >> 24) & 0xFF);
}

//! Internal: helper function to create a BMP for a color bitmap (freely inspired from libpwg::WPGBitmap.cpp)
static bool getBMPData(MWAWPictBitmapContainer<MWAWColor> const &orig, librevenge::RVNGBinaryData &data)
{
  Vec2i sz = orig.size();
  if (sz[0] <= 0 || sz[1] <= 0) return false;

  unsigned tmpPixelSize = unsigned(sz[0]*sz[1]);
  unsigned tmpBufferPosition = 0;

  unsigned tmpDIBImageSize = tmpPixelSize * 4;
  if (tmpPixelSize > tmpDIBImageSize) // overflow !!!
    return false;

  unsigned const headerSize=56;
  unsigned tmpDIBOffsetBits = 14 + headerSize;
  unsigned tmpDIBFileSize = tmpDIBOffsetBits + tmpDIBImageSize;
  if (tmpDIBImageSize > tmpDIBFileSize) // overflow !!!
    return false;

  unsigned char *tmpDIBBuffer = new unsigned char[tmpDIBFileSize];
  if (!tmpDIBBuffer) {
    MWAW_DEBUG_MSG(("getBMPData: fail to allocated the data buffer\n"));
    return false;
  }
  // Create DIB file header
  writeU16(tmpDIBBuffer, tmpBufferPosition, 0x4D42);  // Type
  writeU32(tmpDIBBuffer, tmpBufferPosition, (unsigned) tmpDIBFileSize); // Size
  writeU16(tmpDIBBuffer, tmpBufferPosition, 0); // Reserved1
  writeU16(tmpDIBBuffer, tmpBufferPosition, 0); // Reserved2
  writeU32(tmpDIBBuffer, tmpBufferPosition, (unsigned) tmpDIBOffsetBits); // OffsetBits

  // Create DIB Info header
  writeU32(tmpDIBBuffer, tmpBufferPosition, headerSize); // Size
  writeU32(tmpDIBBuffer, tmpBufferPosition, (unsigned) sz[0]);  // Width
  writeU32(tmpDIBBuffer, tmpBufferPosition, (unsigned) sz[1]); // Height
  writeU16(tmpDIBBuffer, tmpBufferPosition, 1); // Planes
  writeU16(tmpDIBBuffer, tmpBufferPosition, 32); // BitCount
  writeU32(tmpDIBBuffer, tmpBufferPosition, 0); // Compression
  writeU32(tmpDIBBuffer, tmpBufferPosition, (unsigned)tmpDIBImageSize); // SizeImage
  writeU32(tmpDIBBuffer, tmpBufferPosition, 5904); // XPelsPerMeter: 300ppi
  writeU32(tmpDIBBuffer, tmpBufferPosition, 5904); // YPelsPerMeter: 300ppi
  writeU32(tmpDIBBuffer, tmpBufferPosition, 0); // ColorsUsed
  writeU32(tmpDIBBuffer, tmpBufferPosition, 0); // ColorsImportant

  // Create DIB V3 Info header

  /* this is needed to create alpha picture ; but as both LibreOffice/OpenOffice ignore the alpha channel... */
  writeU32(tmpDIBBuffer, tmpBufferPosition, 0x00FF0000); /* biRedMask */
  writeU32(tmpDIBBuffer, tmpBufferPosition, 0x0000FF00); /* biGreenMask */
  writeU32(tmpDIBBuffer, tmpBufferPosition, 0x000000FF); /* biBlueMask */
  writeU32(tmpDIBBuffer, tmpBufferPosition, 0xFF000000); /* biAlphaMask */

  // Write DIB Image data
  for (int i = sz[1] - 1; i >= 0 && tmpBufferPosition < tmpDIBFileSize; i--) {
    MWAWColor const *row = orig.getRow(i);

    for (int j = 0; j < sz[0] && tmpBufferPosition < tmpDIBFileSize; j++) {
      uint32_t col = row[j].value();

      tmpDIBBuffer[tmpBufferPosition++]=(unsigned char)(col&0xFF);
      tmpDIBBuffer[tmpBufferPosition++]=(unsigned char)((col>>8)&0xFF);
      tmpDIBBuffer[tmpBufferPosition++]=(unsigned char)((col>>16)&0xFF);
      tmpDIBBuffer[tmpBufferPosition++]=(unsigned char)((col>>24)&0xFF);
    }
  }
  data.clear();
  data.append(tmpDIBBuffer, tmpDIBFileSize);
  // Cleanup things before returning
  delete [] tmpDIBBuffer;

  return true;
}
}
////////////////////////////////////////////////////////////
// BW bitmap
////////////////////////////////////////////////////////////

bool MWAWPictBitmapBW::createFileData(librevenge::RVNGBinaryData &result) const
{
  return getPBMData<bool>(m_data,result,false);
}

////////////////////////////////////////////////////////////
// Color bitmap
////////////////////////////////////////////////////////////

bool MWAWPictBitmapColor::createFileData(librevenge::RVNGBinaryData &result) const
{
  if (m_hasAlpha) return MWAWPictBitmapInternal::getBMPData(m_data,result);
  return MWAWPictBitmapInternal::getPPMData(m_data,result);
}

////////////////////////////////////////////////////////////
// Indexed bitmap
////////////////////////////////////////////////////////////

bool MWAWPictBitmapIndexed::createFileData(librevenge::RVNGBinaryData &result) const
{
  if (m_colors.size() && getPPMData<int>(m_data,result,m_colors)) return true;
  return getPBMData<int>(m_data,result,0);
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
