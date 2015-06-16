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

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWFontConverter.hxx"
#include "MWAWGraphicListener.hxx"
#include "MWAWGraphicShape.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictBitmap.hxx"
#include "MWAWPictData.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWPosition.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "MacDraft5Parser.hxx"

#include "MacDraft5StyleManager.hxx"

/** Internal: the structures of a MacDraft5StyleManager */
namespace MacDraft5StyleManagerInternal
{
//!  Internal and low level: a class used to read pack/unpack color pixmap of a MacDraf5StyleManager
struct Pixmap {
  Pixmap() : m_rowBytes(0), m_rect(), m_version(-1), m_packType(0),
    m_packSize(0), m_pixelType(0), m_pixelSize(0), m_compCount(0),
    m_compSize(0), m_planeBytes(0), m_colorTable(), m_indices(), m_colors(), m_mode(0)
  {
    m_resolution[0] = m_resolution[1] = 0;
  }

  //! operator<< for Pixmap
  friend std::ostream &operator<< (std::ostream &o, Pixmap const &f)
  {
    o << "rDim=" << f.m_rowBytes << ", " << f.m_rect;
    o << ", resol=" << f.m_resolution[0] << "x" << f.m_resolution[1];
    return o;
  }

  //! creates the pixmap from the packdata
  bool unpackedData(unsigned char const *pData, int sz, int byteSz, int nSize, std::vector<unsigned char> &res) const
  {
    if (byteSz<1||byteSz>4) {
      MWAW_DEBUG_MSG(("MacDraft5StyleManagerInternal::Pixmap::unpackedData: unknown byteSz\n"));
      return false;
    }
    int rPos = 0, wPos = 0, maxW = m_rowBytes+24;
    while (rPos < sz) {
      if (rPos+2 > sz) return false;
      signed char n = (signed char) pData[rPos++];
      if (n < 0) {
        int nCount = 1-n;
        if (rPos+byteSz > sz || wPos+byteSz *nCount >= maxW) return false;

        unsigned char val[4];
        for (int b = 0; b < byteSz; b++) val[b] = pData[rPos++];
        for (int i = 0; i < nCount; i++) {
          if (wPos+byteSz >= maxW) break;
          for (int b = 0; b < byteSz; b++)res[(size_t)wPos++] = val[b];
        }
        continue;
      }
      int nCount = 1+n;
      if (rPos+byteSz *nCount > sz || wPos+byteSz *nCount >= maxW) return false;
      for (int i = 0; i < nCount; i++) {
        if (wPos+byteSz >= maxW) break;
        for (int b = 0; b < byteSz; b++) res[(size_t)wPos++] = pData[rPos++];
      }
    }
    return wPos+8 >= nSize;
  }

  //! parses the pixmap data zone
  bool readPixmapData(MWAWInputStream &input)
  {
    int W = m_rect.size().x(), H = m_rect.size().y();

    int szRowSize=1;
    if (m_rowBytes > 250) szRowSize = 2;

    int nPlanes = 1, nBytes = 3, rowBytes = m_rowBytes;
    int numValuesByInt = 1;
    int maxValues = (1 << m_pixelSize)-1;
    int numColors = (int) m_colorTable.size();
    int maxColorsIndex = -1;

    bool packed = false;// checkme: find no packed data with m_packType==1 !(m_rowBytes < 8 || m_packType == 1);
    switch (m_pixelSize) {
    case 1:
    case 2:
    case 4:
    case 8: { // indices (associated to a color map)
      nBytes = 1;
      numValuesByInt = 8/m_pixelSize;
      int numValues = (W+numValuesByInt-1)/numValuesByInt;
      if (m_rowBytes < numValues || m_rowBytes > numValues+10) {
        MWAW_DEBUG_MSG(("MacDraft5StyleManagerInternal::Pixmap::readPixmapData invalid number of rowsize : %d, pixelSize=%d, W=%d\n", m_rowBytes, m_pixelSize, W));
        return false;
      }
      if (numColors == 0) {
        MWAW_DEBUG_MSG(("MacDraft5StyleManagerInternal::Pixmap::readPixmapData: readPixmapData no color table \n"));
        return false;
      }
      break;
    }

    case 16:
      nBytes = 2;
      break;
    case 32:
      if (!packed) {
        nBytes=4;
        break;
      }
      if (m_packType == 2) {
        packed = false;
        break;
      }
      if (m_compCount != 3 && m_compCount != 4) {
        MWAW_DEBUG_MSG(("MacDraft5StyleManagerInternal::Pixmap::readPixmapData: do not known how to read cmpCount=%d\n", m_compCount));
        return false;
      }
      nPlanes=m_compCount;
      nBytes=1;
      if (nPlanes == 3) rowBytes = (3*rowBytes)/4;
      break;
    default:
      MWAW_DEBUG_MSG(("MacDraft5StyleManagerInternal::Pixmap::readPixmapData: do not known how to read pixelsize=%d \n", m_pixelSize));
      return false;
    }
    if (m_pixelSize <= 8)
      m_indices.resize(size_t(H*W));
    else {
      if (rowBytes != W * nBytes * nPlanes) {
        MWAW_DEBUG_MSG(("MacDraft5StyleManagerInternal::Pixmap::readPixmapData: find W=%d pixelsize=%d, rowSize=%d\n", W, m_pixelSize, m_rowBytes));
      }
      m_colors.resize(size_t(H*W));
    }

    std::vector<unsigned char> values;
    values.resize(size_t(m_rowBytes+24), (unsigned char)0);

    for (int y = 0; y < H; y++) {
      if (!packed) {
        unsigned long numR = 0;
        unsigned char const *data = input.read(size_t(m_rowBytes), numR);
        if (!data || int(numR) != m_rowBytes) {
          MWAW_DEBUG_MSG(("MacDraft5StyleManagerInternal::Pixmap::readPixmapData: readColors can not read line %d/%d (%d chars)\n", y, H, m_rowBytes));
          return false;
        }
        for (size_t j = 0; j < size_t(m_rowBytes); j++)
          values[j]=data[j];
      }
      else {   // ok, packed
        int numB = (int) input.readULong(szRowSize);
        if (numB < 0 || numB > 2*m_rowBytes) {
          MWAW_DEBUG_MSG(("MacDraft5StyleManagerInternal::Pixmap::readPixmapData: odd numB:%d in row: %d/%d\n", numB, y, H));
          return false;
        }
        unsigned long numR = 0;
        unsigned char const *data = input.read(size_t(numB), numR);
        if (!data || int(numR) != numB) {
          MWAW_DEBUG_MSG(("MacDraft5StyleManagerInternal::Pixmap::readPixmapData: can not read line %d/%d (%d chars)\n", y, H, numB));
          return false;
        }
        if (!unpackedData(data,numB, nBytes, rowBytes, values)) {
          MWAW_DEBUG_MSG(("MacDraft5StyleManagerInternal::Pixmap::readPixmapData: can not unpacked line:%d\n", y));
          return false;
        }
      }

      //
      // ok, we can add it in the pictures
      //
      int wPos = y*W;
      if (m_pixelSize <= 8) { // indexed
        for (int x = 0, rPos = 0; x < W;) {
          unsigned char val = values[(size_t)rPos++];
          for (int v = numValuesByInt-1; v >=0; v--) {
            int index = (val>>(v*m_pixelSize))&maxValues;
            if (index > maxColorsIndex) maxColorsIndex = index;
            m_indices[(size_t)wPos++] = index;
            if (++x >= W) break;
          }
        }
      }
      else if (m_pixelSize == 16) {
        for (int x = 0, rPos = 0; x < W; x++) {
          unsigned int val = 256*(unsigned int)values[(size_t)rPos]+(unsigned int)values[(size_t)rPos+1];
          rPos+=2;
          m_colors[(size_t)wPos++]=MWAWColor((val>>7)& 0xF8, (val>>2) & 0xF8, (unsigned char)(val << 3));
        }
      }
      else if (nPlanes==1) {
        for (int x = 0, rPos = 0; x < W; x++) {
          if (nBytes==4) rPos++;
          m_colors[(size_t)wPos++]=MWAWColor(values[(size_t)rPos], values[size_t(rPos+1)],  values[size_t(rPos+2)]);
          rPos+=3;
        }
      }
      else {
        for (int x = 0, rPos = (nPlanes==4) ? W:0; x < W; x++) {
          m_colors[(size_t)wPos++]=MWAWColor(values[(size_t)rPos], values[size_t(rPos+W)],  values[size_t(rPos+2*W)]);
          rPos+=1;
        }
      }
    }
    if (maxColorsIndex >= numColors) {
      // can be ok for a pixpat ; in this case:
      // maxColorsIndex -> foregroundColor, numColors -> backGroundColor
      // and intermediate index fills with intermediate colors
      int numUnset = maxColorsIndex-numColors+1;

      int decGray = (numUnset==1) ? 0 : 255/(numUnset-1);
      for (int i = 0; i < numUnset; i++)
        m_colorTable.push_back(MWAWColor((unsigned char)(255-i*decGray), (unsigned char)(255-i*decGray), (unsigned char)(255-i*decGray)));
      MWAW_DEBUG_MSG(("MacDraft5StyleManagerInternal::Pixmap::readPixmapData: find index=%d >= numColors=%d\n", maxColorsIndex, numColors));

      return true;
    }
    return true;
  }
  //! returns the pixmap
  bool get(librevenge::RVNGBinaryData &dt, std::string &type, MWAWVec2i &pictSize, MWAWColor &avColor) const
  {
    pictSize=m_rect.size();
    int W = m_rect.size().x();
    if (W <= 0) return false;
    if (!m_colorTable.empty() && m_indices.size()) {
      int nRows = int(m_indices.size())/W;
      MWAWPictBitmapIndexed pixmap(MWAWVec2i(W,nRows));
      if (!pixmap.valid()) return false;

      pixmap.setColors(m_colorTable);

      size_t numColor=m_colorTable.size();
      std::vector<int> colorByIndices(numColor,0);
      size_t rPos = 0;
      for (int i = 0; i < nRows; i++) {
        for (int x = 0; x < W; x++) {
          int id=m_indices[rPos++];
          if (id<0 || id>=(int) numColor) {
            static bool first=true;
            if (first) {
              MWAW_DEBUG_MSG(("MacDraft5StyleManagerInternal::Pixmap::get: find some bad index\n"));
              first=false;
            }
            pixmap.set(x, i, 0);
          }
          else {
            ++colorByIndices[size_t(id)];
            pixmap.set(x, i, id);
          }
        }
      }
      float totalCol[3];
      long numCols=0;
      for (size_t i=0; i<numColor; ++i) {
        if (!colorByIndices[i]) continue;
        numCols+=colorByIndices[i];
        totalCol[0]+=(float)colorByIndices[i]*(float)m_colorTable[i].getRed();
        totalCol[1]+=(float)colorByIndices[i]*(float)m_colorTable[i].getGreen();
        totalCol[2]+=(float)colorByIndices[i]*(float)m_colorTable[i].getBlue();
      }
      if (numCols==0)
        avColor=MWAWColor::black();
      else
        avColor=MWAWColor((unsigned char)(totalCol[0]/float(numCols)),
                          (unsigned char)(totalCol[1]/float(numCols)),
                          (unsigned char)(totalCol[2]/float(numCols)));
      return pixmap.getBinary(dt, type);
    }

    if (m_colors.size()) {
      int nRows = int(m_colors.size())/W;
      MWAWPictBitmapColor pixmap(MWAWVec2i(W,nRows));
      if (!pixmap.valid()) return false;

      size_t rPos = 0;
      long numCols=0;
      float totalCol[3];
      for (int i = 0; i < nRows; i++) {
        for (int x = 0; x < W; x++) {
          MWAWColor col=m_colors[rPos++];
          pixmap.set(x, i, col);
          totalCol[0]+=(float) col.getRed();
          totalCol[1]+=(float) col.getGreen();
          totalCol[2]+=(float) col.getBlue();
          ++numCols;
        }
      }
      if (numCols==0)
        avColor=MWAWColor::black();
      else
        avColor=MWAWColor((unsigned char)(totalCol[0]/float(numCols)),
                          (unsigned char)(totalCol[1]/float(numCols)),
                          (unsigned char)(totalCol[2]/float(numCols)));

      return pixmap.getBinary(dt, type);
    }

    MWAW_DEBUG_MSG(("MacDraft5StyleManagerInternal::Pixmap::get: can not find any indices or colors \n"));
    return false;
  }

  //! the num of bytes used to store a row
  int m_rowBytes;
  MWAWBox2i m_rect /** the pixmap rectangle */;
  int m_version /** the pixmap version */;
  int m_packType /** the packing format */;
  long m_packSize /** size of data in the packed state */;
  int m_resolution[2] /** horizontal/vertical definition */;
  int m_pixelType /** format of pixel image */;
  int m_pixelSize /** physical bit by image */;
  int m_compCount /** logical components per pixels */;
  int m_compSize /** logical bits by components */;
  long m_planeBytes /** offset to the next plane */;

  //! the color table
  std::vector<MWAWColor> m_colorTable;
  //! the pixmap indices
  std::vector<int> m_indices;
  //! the colors
  std::vector<MWAWColor> m_colors;
  //! the encoding mode ?
  int m_mode;
};

////////////////////////////////////////
//! Internal: the state of a MacDraft5StyleManager
struct State {
  //! constructor
  State() : m_dataEnd(-1), m_rsrcBegin(-1), m_colorList(), m_patternList(), m_dashList(),
    m_beginToBitmapEntryMap(), m_pixIdToPixmapMap(), m_pixIdToPatternIdMap()
  {
  }
  //! returns a color if possible
  bool getColor(int id, MWAWColor &col)
  {
    if (m_colorList.empty()) initColors();
    if (id<=0 || id>int(m_colorList.size())) {
      MWAW_DEBUG_MSG(("MacDraft5StyleManagerInternal::getColor: can not find color %d\n", id));
      return false;
    }
    col=m_colorList[size_t(id-1)];
    return true;
  }

  //! returns a pattern if possible
  bool getPattern(int id, MWAWGraphicStyle::Pattern &pat)
  {
    if (m_patternList.empty()) initPatterns();
    if (id<=0 || id>=int(m_patternList.size())) {
      MWAW_DEBUG_MSG(("MacDraft5StyleManagerInternal::getPattern: can not find pattern %d\n", id));
      return false;
    }
    pat=m_patternList[size_t(id)];
    return true;
  }

  //! init the color list
  void initColors();
  //! init the patterns list
  void initPatterns();
  //! init the dashs list
  void initDashs();

  //! the end of the main data zone
  long m_dataEnd;
  //! the begin of the rsrc data
  long m_rsrcBegin;
  //! the color list
  std::vector<MWAWColor> m_colorList;
  //! the patterns list
  std::vector<MWAWGraphicStyle::Pattern> m_patternList;
  //! the list of dash
  std::vector< std::vector<float> > m_dashList;
  //! a map file position to entry ( used to stored intermediar zones )
  std::map<long, MWAWEntry> m_beginToBitmapEntryMap;
  //! a map pixmapId to pixmap map
  std::map<int, shared_ptr<Pixmap> > m_pixIdToPixmapMap;
  //! a map pixmapId to patternId map
  std::map<int, size_t> m_pixIdToPatternIdMap;
};

void State::initColors()
{
  if (!m_colorList.empty()) return;
  for (int i=0; i<131; ++i) {
    static uint32_t const(colors[131])= {
      0xffffff,0x000000,0x7f7f7f,0xdd0806,0x008011,0x0000d4,0x02abea,0xf20884,
      0xfcf305,0xff1b00,0xff3700,0xff5300,0xff6f00,0xff8b00,0xffa700,0xffc300,
      0xffdf00,0xfffb00,0xe8ff00,0xccff00,0xb0ff00,0x94ff00,0x79ff00,0x5dff00,
      0x41ff00,0x25ff00,0x09ff00,0x00ff12,0x00ff2e,0x00ff4a,0x00ff66,0x00ff82,
      0x00ff9e,0x00ffba,0x00ffd6,0x00fff2,0x00f2ff,0x00d6ff,0x00baff,0x009eff,
      0x0082ff,0x0066ff,0x004aff,0x002eff,0x0012ff,0x0900ff,0x2500ff,0x4100ff,
      0x5d00ff,0x7800ff,0x9400ff,0xb000ff,0xcc00ff,0xe800ff,0xff00fb,0xff00df,
      0xff00c3,0xff00a7,0xff008b,0xff006f,0xff0053,0xff0037,0xff001b,0xff0000,
      0xc31500,0xc32a00,0xc33f00,0xc35500,0xc36a00,0xc37f00,0xc39500,0x3a3a3a,
      0x535353,0x6d6d6d,0x868686,0xa0a0a0,0xb9b9b9,0xd2d2d2,0xeaeaea,0xcccccc,
      0x999999,0x878787,0x00ffff
    };
    m_colorList.push_back(colors[i]);
  }
}

void State::initPatterns()
{
  if (!m_patternList.empty()) return;
  for (int i=0; i<71; ++i) {
    static uint16_t const(patterns[71*4]) = {
      0x0,0x0,0x0,0x0,0xffff,0xffff,0xffff,0xffff,0x40,0x400,0x10,0x100,0x8040,0x2010,0x804,0x201,
      0x102,0x408,0x1020,0x4080,0x842,0x90,0x440,0x1001,0xe070,0x381c,0xe07,0x83c1,0x8307,0xe1c,0x3870,0xe0c1,
      0x8000,0x0,0x800,0x0,0x42a,0x4025,0x251,0x2442,0x4422,0x88,0x4422,0x88,0x1122,0x4400,0x1122,0x4400,
      0x8000,0x800,0x8000,0x800,0x4aa4,0x8852,0x843a,0x4411,0x8844,0x2211,0x8844,0x2211,0x1122,0x4488,0x1122,0x4488,
      0x8800,0x2200,0x8800,0x2200,0x4cd2,0x532d,0x9659,0x46b3,0x99cc,0x6633,0x99cc,0x6633,0x3366,0xcc99,0x3366,0xcc99,
      0x8822,0x8822,0x8822,0x8822,0xdbbe,0xedbb,0xfeab,0xbeeb,0xcc00,0x0,0x3300,0x0,0x101,0x1010,0x101,0x1010,
      0xaa55,0xaa55,0xaa55,0xaa55,0xf7bd,0xff6f,0xfbbf,0xeffe,0x2040,0x8000,0x804,0x200,0x40a0,0x0,0x40a,0x0,
      0x77dd,0x77dd,0x77dd,0x77dd,0x8244,0x3944,0x8201,0x101,0xff00,0x0,0xff00,0x0,0x8888,0x8888,0x8888,0x8888,
      0x8142,0x3c18,0x183c,0x4281,0xb130,0x31b,0xb8c0,0xc8d,0x6c92,0x8282,0x4428,0x1000,0xff80,0x8080,0xff80,0x8080,
      0x8142,0x2418,0x1020,0x4080,0xff80,0x8080,0xff08,0x808,0x8080,0x413e,0x808,0x14e3,0xff88,0x8888,0xff88,0x8888,
      0xff80,0x8080,0x8080,0x8080,0xbf00,0xbfbf,0xb0b0,0xb0b0,0xaa00,0x8000,0x8800,0x8000,0xaa44,0xaa11,0xaa44,0xaa11,
      0x8244,0x2810,0x2844,0x8201,0x8,0x142a,0x552a,0x1408,0x1038,0x7cfe,0x7c38,0x1000,0x1020,0x54aa,0xff02,0x408,
      0x8080,0x8080,0x8094,0xaa55,0x804,0x2a55,0xff40,0x2010,0x7789,0x8f8f,0x7798,0xf8f8,0x8814,0x2241,0x8800,0xaa00,
      0x77eb,0xddbe,0x77ff,0x55ff,0x1022,0x408a,0x4022,0x108a,0xefdd,0xbf75,0xbfdd,0xef75,0x9f90,0x909f,0xf909,0x9f9,
      0xf078,0x2442,0x870f,0x1221,0xfe82,0xfeee,0xef28,0xefee,0xf9fc,0x664f,0x9f3f,0x66f3,0xaf5f,0xaf5f,0xd0b,0xd0b,
      0xa011,0xa1c,0x2844,0x82c1,0xf0f0,0xf0f0,0xf0f,0xf0f,0xc864,0x3219,0x9923,0x468c,0xc000,0x0,0xc,0x1221,
      0x101,0x8040,0x2020,0x4080,0x8844,0x2211,0x1121,0x4284,0xf87c,0x3e1f,0x1121,0x4284,0x1c32,0x71f0,0xf8e4,0xc281,
      0xd86c,0x3613,0xa141,0x8205,0x810,0x1038,0xcf07,0x204,0x8851,0x2254,0x8814,0x2241
    };

    MWAWGraphicStyle::Pattern pat;
    pat.m_dim=MWAWVec2i(8,8);
    pat.m_data.resize(8);
    pat.m_colors[0]=MWAWColor::white();
    pat.m_colors[1]=MWAWColor::black();
    uint16_t const *patPtr=&patterns[4*i];
    for (size_t j=0; j<8; j+=2, ++patPtr) {
      pat.m_data[j]=uint8_t((*patPtr)>>8);
      pat.m_data[j+1]=uint8_t((*patPtr)&0xFF);
    }
    m_patternList.push_back(pat);
  }
}

void State::initDashs()
{
  if (!m_dashList.empty()) return;
  std::vector<float> dash;
  // 0: full
  m_dashList.push_back(dash);
  // 1: 6x2
  dash.push_back(6);
  dash.push_back(2);
  m_dashList.push_back(dash);
  // 2: 12x2
  dash[0]=12;
  m_dashList.push_back(dash);
  // 3: 24x3
  dash[0]=24;
  dash[1]=3;
  m_dashList.push_back(dash);
  // 4: 48x4
  dash[0]=48;
  dash[1]=4;
  m_dashList.push_back(dash);
  // 5: 6,2,1,2
  dash.resize(4);
  dash[0]=6;
  dash[1]=dash[3]=2;
  dash[2]=1;
  m_dashList.push_back(dash);
  // 6: 12,2,1,2
  dash[0]=12;
  m_dashList.push_back(dash);
  // 7: 24,3,2,3
  dash[0]=24;
  dash[1]=dash[3]=3;
  dash[2]=2;
  m_dashList.push_back(dash);
  // 8: 48,4,2,4
  dash[0]=48;
  dash[1]=dash[3]=4;
  dash[2]=2;
  m_dashList.push_back(dash);
  // 9:6,2,1,2,1,2,
  dash.resize(6);
  dash[0]=6;
  dash[1]=dash[3]=dash[5]=2;
  dash[2]=dash[4]=1;
  m_dashList.push_back(dash);
  // 10:12,2,1,2,1,2,
  dash[0]=12;
  m_dashList.push_back(dash);
  // 11:24,3,2,2,2,3
  dash[0]=24;
  dash[2]=dash[3]=dash[4]=2;
  dash[1]=dash[5]=3;
  m_dashList.push_back(dash);
  // 12: 48,4,2,2,2,4
  dash[0]=48;
  dash[1]=dash[5]=4;
  m_dashList.push_back(dash);
  // 13: 12,2,1,2,1,2,1,2
  dash.resize(8);
  dash[0]=6;
  dash[1]=dash[3]=dash[5]=dash[7]=2;
  dash[2]=dash[4]=dash[6]=1;
  m_dashList.push_back(dash);
  // 14: 24,3,2,2,2,2,2,3
  dash[0]=24;
  dash[2]=dash[3]=dash[4]=dash[5]=dash[6]=2;
  dash[1]=dash[7]=3;
  m_dashList.push_back(dash);
  // 15: 48,4,2,3,2,3,2,4
  dash[0]=48;
  dash[1]=dash[7]=4;
  dash[2]=dash[4]=dash[6]=2;
  dash[3]=dash[5]=3;
  m_dashList.push_back(dash);
}

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MacDraft5StyleManager::MacDraft5StyleManager(MacDraft5Parser &parser) :
  m_parser(parser), m_parserState(parser.getParserState()), m_state(new MacDraft5StyleManagerInternal::State)
{
}

MacDraft5StyleManager::~MacDraft5StyleManager()
{
}

long MacDraft5StyleManager::getEndDataPosition() const
{
  return m_state->m_dataEnd;
}

bool MacDraft5StyleManager::getColor(int colId, MWAWColor &color) const
{
  return m_state->getColor(colId, color);
}

bool MacDraft5StyleManager::getPixmap(int pId, librevenge::RVNGBinaryData &data, std::string &type,
                                      MWAWVec2i &pictSize, MWAWColor &avColor) const
{
  if (m_state->m_pixIdToPixmapMap.find(pId)==m_state->m_pixIdToPixmapMap.end() ||
      !m_state->m_pixIdToPixmapMap.find(pId)->second ||
      !m_state->m_pixIdToPixmapMap.find(pId)->second->get(data,type,pictSize,avColor)) {
    MWAW_DEBUG_MSG(("MWAWMacDraft5StyleManager::getPixmap: can not find pixmap %d\n", pId));
    return false;
  }
#ifdef DEBUG_WITH_FILES
  std::stringstream s;
  s << "PixPat" << pId << ".ppm";
  libmwaw::Debug::dumpFile(data, s.str().c_str());
#endif

  return true;
}

void MacDraft5StyleManager::updatePatterns()
{
  for (std::map<int, size_t>::const_iterator it=m_state->m_pixIdToPatternIdMap.begin();
       it!=m_state->m_pixIdToPatternIdMap.end(); ++it) {
    librevenge::RVNGBinaryData data;
    std::string type;
    MWAWVec2i bitmapSize;
    MWAWColor averageColor;
    if (!getPixmap(it->first, data, type, bitmapSize, averageColor)) continue;
    if (it->second>=m_state->m_patternList.size()) {
      MWAW_DEBUG_MSG(("MWAWMacDraft5StyleManager::updatePatterns: oops patterns id seems bad for %d\n", it->first));
      continue;
    }
    shared_ptr<MacDraft5StyleManagerInternal::Pixmap> pixmap=m_state->m_pixIdToPixmapMap.find(it->first)->second;
    m_state->m_patternList[it->second]=MWAWGraphicStyle::Pattern(bitmapSize, data, type, averageColor);
    // FIXME: update the pattern here
  }
  // check for unused pixmap
  std::map<int, shared_ptr<MacDraft5StyleManagerInternal::Pixmap> >::const_iterator it;
  for (it=m_state->m_pixIdToPixmapMap.begin(); it!=m_state->m_pixIdToPixmapMap.end(); ++it) {
    if (m_state->m_pixIdToPatternIdMap.find(it->first)!=m_state->m_pixIdToPatternIdMap.end() || !it->second)
      continue;
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("MWAWMacDraft5StyleManager::updatePatterns: find unread pixmap %d\n", it->first));
      first=false;
    }
    librevenge::RVNGBinaryData data;
    std::string type;
    MWAWVec2i bitmapSize;
    MWAWColor averageColor;
    getPixmap(it->first, data, type, bitmapSize, averageColor);
  }
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool MacDraft5StyleManager::readBitmapZones()
{
  if (m_state->m_beginToBitmapEntryMap.empty()) {
    m_state->m_dataEnd=m_state->m_rsrcBegin;
    return true;
  }
  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  if (m_state->m_rsrcBegin>0)
    input->pushLimit(m_state->m_rsrcBegin);
  std::map<long, MWAWEntry>::iterator it=m_state->m_beginToBitmapEntryMap.begin();
  m_state->m_dataEnd=it->first;
  long lastPos=it->first;
  while (it!=m_state->m_beginToBitmapEntryMap.end()) {
    if (it->first!=lastPos) {
      MWAW_DEBUG_MSG(("MacDraft5StyleManager::readBitmapZones: find some unknown zone\n"));
      ascFile.addPos(lastPos);
      ascFile.addNote("Entries(UnknZone):");
    }
    MWAWEntry &entry=it++->second;
    lastPos=entry.end();
    if (entry.type()=="bitmap" && readBitmap(entry))
      continue;
    ascFile.addPos(entry.begin());
    ascFile.addNote("Entries(BITData):");
  }
  if (m_state->m_rsrcBegin>0)
    input->popLimit();
  return true;
}

////////////////////////////////////////////////////////////
// resource fork
////////////////////////////////////////////////////////////
bool MacDraft5StyleManager::readResources()
{
  // first look the resource manager
  MWAWRSRCParserPtr rsrcParser = m_parserState->m_rsrcParser;
  if (rsrcParser) {
    std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
    std::multimap<std::string, MWAWEntry>::iterator it=entryMap.begin();
    while (it!=entryMap.end())
      readResource(it++->second, true);
  }

  MWAWInputStreamPtr input = m_parserState->m_input;
  long endPos=input->size();
  if (endPos<=28) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readResources: the file seems too short\n"));
    return false;
  }
  input->seek(-10, librevenge::RVNG_SEEK_END);
  int dSz=(int) input->readULong(2);
  if (dSz<28 || dSz>=endPos)
    return false;
  std::string name("");
  for (int i=0; i<8; ++i) name+=(char) input->readULong(1);
  if (name!="RBALRPH ") return false;
  input->seek(-dSz, librevenge::RVNG_SEEK_END);
  long pos=input->tell();
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(RSRCMap):";
  long depl=(long) input->readULong(4);
  long debRSRCPos=endPos-depl;
  if (depl>endPos || debRSRCPos>pos) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readResources: the depl0 is bad\n"));
    return false;
  }
  f << "debPos=" << std::hex << debRSRCPos << std::dec << ",";
  depl=(long) input->readULong(4);
  if (pos-depl!=debRSRCPos) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readResources: the depl1 is bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    return false;
  }

  name="";
  for (int i=0; i<4; ++i) name +=(char) input->readULong(1);
  if (name!="RSRC") {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readResources: can not find the resource name\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    return false;
  }
  int N=(dSz-22)/2;
  for (int i=0; i<N; ++i) { // f0=1
    int val=(int)input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  input->pushLimit(pos);
  input->seek(debRSRCPos, librevenge::RVNG_SEEK_SET);
  while (!input->isEnd()) {
    pos=input->tell();
    long fSz=(long) input->readULong(4);
    if (fSz==0) {
      ascFile.addPos(pos);
      ascFile.addNote("_");
      continue;
    }
    endPos=pos+fSz;
    if (!input->checkPosition(endPos)) {
      input->seek(pos,librevenge::RVNG_SEEK_SET);
      ascFile.addPos(pos);
      ascFile.addNote("Entries(rsrcBAD):");
      MWAW_DEBUG_MSG(("MacDraft5StyleManager::readResources: find some bad resource\n"));
      break;
    }
    if (fSz<16) {
      ascFile.addPos(pos);
      ascFile.addNote("Entries(rsrcBAD):");
      MWAW_DEBUG_MSG(("MacDraft5StyleManager::readResources: find unknown resource\n"));
      input->seek(endPos, librevenge::RVNG_SEEK_SET);
      continue;
    }
    MWAWEntry entry;
    entry.setBegin(pos+16);
    entry.setLength((long) input->readULong(4));
    name="";
    for (int i=0; i<4; ++i) name+=(char) input->readULong(1);
    entry.setType(name);
    entry.setId((int) input->readLong(2));
    if (entry.end()>endPos || name.empty()) {
      ascFile.addPos(pos);
      ascFile.addNote("Entries(rsrcBAD):###");
      MWAW_DEBUG_MSG(("MacDraft5StyleManager::readResources: problem reading rsrc data\n"));
      input->seek(endPos, librevenge::RVNG_SEEK_SET);
      continue;
    }
    if (readResource(entry, false)) {
      input->seek(endPos, librevenge::RVNG_SEEK_SET);
      continue;
    }
    f.str("");
    f << "Entries(rsrc" << name << ")[" << entry.id() << "]:###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    if (fSz>120)
      ascFile.skipZone(pos+100, endPos-1);
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
  }
  input->popLimit();
  m_state->m_rsrcBegin=debRSRCPos;
  updatePatterns();
  return true;
}

bool MacDraft5StyleManager::readResource(MWAWEntry &entry, bool inRsrc)
{
  if (inRsrc && !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MWAWMacDraft5StyleManager::readResource: can not find the resource parser\n"));
    return false;
  }
  if (entry.type()=="PICT") {
    librevenge::RVNGBinaryData data;
    if (inRsrc)
      return m_parserState->m_rsrcParser->parsePICT(entry,data);
    else
      return m_parser.readPICT(entry, data);
  }
  if (entry.type()=="ppat")
    return readPixPat(entry, inRsrc);
  if (entry.type()=="vers") {
    if (inRsrc) {
      MWAWRSRCParser::Version vers;
      return m_parserState->m_rsrcParser->parseVers(entry,vers);
    }
    else
      return readVersion(entry);
  }
  // 0 resources
  if (entry.type()=="BITL")
    return readBitmapList(entry, inRsrc);
  if (entry.type()=="LAYI")
    return m_parser.readLayoutDefinitions(entry, inRsrc);
  if (entry.type()=="pnot")
    return m_parser.readPICTList(entry, inRsrc);
  // 1 resources
  if (entry.type()=="VIEW")
    return m_parser.readViews(entry, inRsrc);
  if (entry.type()=="FNUS")
    return readFonts(entry, inRsrc);
  // 1-2-3 resources
  if (entry.type()=="OPST") {
    if (inRsrc && !m_parserState->m_rsrcParser) return false;
    MWAWInputStreamPtr input = inRsrc ? m_parserState->m_rsrcParser->getInput() : m_parserState->m_input;
    libmwaw::DebugFile &ascFile = inRsrc ? m_parserState->m_rsrcParser->ascii() : m_parserState->m_asciiFile;
    libmwaw::DebugStream f;
    f << "Entries(OPST)[" << entry.id() << "]:";
    entry.setParsed(true);
    if (!input || !entry.valid() || entry.length()!=2) {
      MWAW_DEBUG_MSG(("MacDraft5StyleManager::readResource: unexpected OPST length\n"));
      f << "###";
    }
    else {
      input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
      int val=(int) input->readLong(2);
      if (val!=100) // always 100?
        f << "f0=" << val << ",";
    }
    if (input && entry.valid()) {
      ascFile.addPos(entry.begin()-(inRsrc ? 4 : 16));
      ascFile.addNote(f.str().c_str());
      input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    }
    return true;
  }

  // 128 resources
  if (entry.type()=="pltt")
    return readColors(entry, inRsrc);
  if (entry.type()=="DASH")
    return readDashes(entry, inRsrc);
  if (entry.type()=="PLDT")
    return readPatterns(entry, inRsrc);
  // PATL: link to PLDT(ie. pattern point), Opac: link to Opac
  if (entry.type()=="PATL" || entry.type()=="Opac")
    return readRSRCList(entry, inRsrc);

  // 256+x
  if (entry.type()=="Link")
    return m_parser.readLinks(entry, inRsrc);

  //
  if (entry.type()=="Opcd") // associated with "Opac"
    return readOpcd(entry, inRsrc);

  if (entry.type()=="icns") { // file icone, safe to ignore
    MWAWInputStreamPtr input = inRsrc ? m_parserState->m_rsrcParser->getInput() : m_parserState->m_input;
    libmwaw::DebugFile &ascFile = inRsrc ? m_parserState->m_rsrcParser->ascii() : m_parserState->m_asciiFile;
    entry.setParsed(true);
    ascFile.addPos(entry.begin()-(inRsrc ? 4 : 16));
    ascFile.addNote("Entries(Icone):...");
    ascFile.skipZone(entry.begin(), entry.end()-1);
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  if (inRsrc) return false;

  if (entry.type()=="flPF") { // printer plist, safe to ignore
    libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
    ascFile.addPos(entry.begin()-16);
    ascFile.addNote("Entries(PrintPList):...");
    ascFile.skipZone(entry.begin(), entry.end()-1);
    m_parserState->m_input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  f << "Entries(rsrc" << entry.type() << ")[" << entry.id() << "]:";
  ascFile.addPos(entry.begin()-16);
  ascFile.addNote(f.str().c_str());
  m_parserState->m_input->seek(entry.end(), librevenge::RVNG_SEEK_SET);

  return true;
}

bool MacDraft5StyleManager::readFonts(MWAWEntry const &entry, bool inRsrc)
{
  if (inRsrc && !m_parserState->m_rsrcParser) return false;
  MWAWInputStreamPtr input = inRsrc ? m_parserState->m_rsrcParser->getInput() : m_parserState->m_input;
  if (!input || !entry.valid()) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readFNUS: entry is invalid\n"));
    return false;
  }
  entry.setParsed(true);
  libmwaw::DebugFile &ascFile = inRsrc ? m_parserState->m_rsrcParser->ascii() : m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  f << "Entries(Fonts):";
  if (entry.id()!=1) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readFNUS: id seems bad\n"));
    f << "##id=" << entry.id() << ",";
  }
  ascFile.addPos(entry.begin()-(inRsrc ? 4 : 16));
  ascFile.addNote(f.str().c_str());
  int n=0;
  while (!input->isEnd()) {
    long pos=input->tell();
    if (pos+3>entry.end())
      break;
    f.str("");
    f << "Fonts-" << n++ << ":";
    int fId=(int) input->readULong(2);
    f<<"fId=" << fId << ",";
    int sSz=(int) input->readULong(1);
    if (pos+3+sSz>entry.end()) {
      input->seek(pos,librevenge::RVNG_SEEK_SET);
      break;
    }
    std::string name("");
    for (int c=0; c<sSz; ++c) name += (char) input->readULong(1);
    f << name << ",";
    if (!name.empty())
      m_parserState->m_fontConverter->setCorrespondance(fId, name);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  if (input->tell()!=entry.end()) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readFNUS: find extra data\n"));
    ascFile.addPos(input->tell());
    ascFile.addNote("FNUS-extra:###");
  }
  input->seek(entry.end(),librevenge::RVNG_SEEK_SET);
  return true;
}

bool MacDraft5StyleManager::readColors(MWAWEntry const &entry, bool inRsrc)
{
  if (inRsrc && !m_parserState->m_rsrcParser) return false;
  MWAWInputStreamPtr input = inRsrc ? m_parserState->m_rsrcParser->getInput() : m_parserState->m_input;
  if (!input || !entry.valid() || entry.length()<16 || (entry.length()%16)!=0) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readColors: entry is invalid\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = inRsrc ? m_parserState->m_rsrcParser->ascii() : m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Color):";
  if (entry.id()!=128) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readColors: entry id seems odd\n"));
    f << "##id=" << entry.id() << ",";
  }
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  if (N*16+16!=entry.length()) {
    f << "###";
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readColors: the N values seems odd\n"));
    N=int(entry.length()/16)-1;
  }
  int val;
  for (int i=0; i<5; ++i) { // f2=[8c]0[01][0-c]
    val=(int) input->readULong(2);
    if (val) f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  val=(int) input->readULong(4);
  if (val) f << "unkn=" << std::hex << val << std::dec << ",";
  ascFile.addPos(entry.begin()-(inRsrc ? 4 : 16));
  ascFile.addNote(f.str().c_str());

  m_state->m_colorList.clear();
  for (long i=0; i<N; ++i) {
    long pos=input->tell();
    f.str("");
    f << "Color-" << i << ":";
    uint8_t col[3];
    for (int j=0; j<3; ++j) col[j]=uint8_t(input->readULong(2)>>8);
    MWAWColor color(col[0],col[1],col[2]);
    f << color << ",";
    m_state->m_colorList.push_back(color);
    for (int j=0; j<5; ++j) { // f0=0|2, f1=0|5(gray?), f3=id?
      val=(int) input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    input->seek(pos+16, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

bool MacDraft5StyleManager::readDashes(MWAWEntry const &entry, bool inRsrc)
{
  if (inRsrc && !m_parserState->m_rsrcParser) return false;
  MWAWInputStreamPtr input = inRsrc ? m_parserState->m_rsrcParser->getInput() : m_parserState->m_input;
  if (!input || !entry.valid() || entry.length()<16 || (entry.length()%16)!=0) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readDashes: entry is invalid\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = inRsrc ? m_parserState->m_rsrcParser->ascii() : m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Dash):";
  if (entry.id()!=128) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readDashes: entry id seems odd\n"));
    f << "##id=" << entry.id() << ",";
  }
  int N=int(entry.length()/16);
  ascFile.addPos(entry.begin()-(inRsrc ? 4 : 16));
  ascFile.addNote(f.str().c_str());

  for (long i=0; i<N; ++i) {
    long pos=input->tell();
    f.str("");
    f << "Dash-" << i << ":";
    int n=(int) input->readULong(1);
    if (n>15) {
      MWAW_DEBUG_MSG(("MacDraft5StyleManager::readDashes: n is bad\n"));
      f << "##n=" << n << ",";
      n=0;
    }
    std::vector<float> dash;
    f << "[";
    for (int j=0; j<n; ++j) {
      dash.push_back((float) input->readULong(1));
      f << dash.back() << ",";
    }
    f << "],";
    m_state->m_dashList.push_back(dash);
    input->seek(pos+16, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

bool MacDraft5StyleManager::readPatterns(MWAWEntry const &entry, bool inRsrc)
{
  if (inRsrc && !m_parserState->m_rsrcParser) return false;
  MWAWInputStreamPtr input = inRsrc ? m_parserState->m_rsrcParser->getInput() : m_parserState->m_input;
  if (!input || !entry.valid() || entry.length()<12 || (entry.length()%12)!=0) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readPatterns: entry is invalid\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = inRsrc ? m_parserState->m_rsrcParser->ascii() : m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Pattern):";
  if (entry.id()!=128) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readPatterns: entry id seems odd\n"));
    f << "##id=" << entry.id() << ",";
  }
  int val;
  for (int i=0; i<4; ++i) { // f0=0|1|-1, f1=0|-1
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  val=(int) input->readULong(4);
  if (val) f << "unkn=" << std::hex << val << std::dec << ",";
  ascFile.addPos(entry.begin()-(inRsrc ? 4 : 16));
  ascFile.addNote(f.str().c_str());

  size_t N=size_t(entry.length()/12-1);
  m_state->m_patternList.resize(N);
  for (size_t i=0; i<N; ++i) {
    long pos=input->tell();
    f.str("");
    f << "Pattern-" << i << ":";
    int type=(int) input->readLong(2);
    switch (type) {
    case 0: {
      val=(int) input->readLong(2); // always 0
      if (val) f << "f0=" << val << ",";
      MWAWGraphicStyle::Pattern pat;
      pat.m_dim=MWAWVec2i(8,8);
      pat.m_data.resize(8);
      pat.m_colors[0]=MWAWColor::white();
      pat.m_colors[1]=MWAWColor::black();
      for (size_t j=0; j<8; ++j) pat.m_data[j]=(uint8_t) input->readULong(1);
      f << pat << ",";
      m_state->m_patternList[i]=pat;
      break;
    }
    case 1: {
      int pixId=(int) input->readLong(2);
      f << "id[pixpat]=" << pixId << ",";
      if (m_state->m_pixIdToPatternIdMap.find(pixId)!=m_state->m_pixIdToPatternIdMap.end()) {
        MWAW_DEBUG_MSG(("MacDraft5StyleManager::readPatterns: find a dupplicated pixId\n"));
        f << "###";
      }
      else
        m_state->m_pixIdToPatternIdMap[pixId]=i;
      break;
    }
    default:
      MWAW_DEBUG_MSG(("MacDraft5StyleManager::readPatterns: find unknown type\n"));
      f << "##type=" << type << ",";
      break;
    }
    input->seek(pos+12, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

bool MacDraft5StyleManager::readBitmap(MWAWEntry const &entry)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  if (!input || !entry.valid() || entry.length()<54) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readBitmap: entry is invalid\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Bitmap)[" << entry.id() << "]:";
  long fSz=(long) input->readULong(4);
  if (fSz+4!=entry.length()) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readBitmap: zone size seems bad\n"));
    f << "#sz[entry]=" << fSz << ",";
  }
  fSz=(long) input->readULong(4);
  if (fSz+8>entry.length()) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readBitmap: data size seems bad\n"));
    f << "#sz[data]=" << fSz << ",";
  }
  // now the pixmap
  MacDraft5StyleManagerInternal::Pixmap pixmap;
  pixmap.m_rowBytes = (int) input->readULong(2);
  pixmap.m_rowBytes &= 0x3FFF;

  // read the rectangle: bound
  int dim[4];
  for (int d = 0; d < 4; d++) dim[d] = (int) input->readLong(2);
  pixmap.m_rect = MWAWBox2i(MWAWVec2i(dim[1],dim[0]), MWAWVec2i(dim[3],dim[2]));
  if (pixmap.m_rect.size().x() <= 0 || pixmap.m_rect.size().y() <= 0) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readBitmap: find odd bound rectangle ... \n"));
    return false;
  }
  pixmap.m_version = (int) input->readLong(2);
  pixmap.m_packType = (int) input->readLong(2);
  pixmap.m_packSize = (int) input->readLong(4);
  for (int c = 0; c < 2; c++) {
    pixmap.m_resolution[c] = (int) input->readLong(2);
    input->readLong(2);
  }
  pixmap.m_pixelType = (int) input->readLong(2);
  pixmap.m_pixelSize = (int) input->readLong(2);
  pixmap.m_compCount = (int) input->readLong(2);
  pixmap.m_compSize = (int) input->readLong(2);
  pixmap.m_planeBytes = (int) input->readLong(4);
  f << pixmap;
  input->seek(8, librevenge::RVNG_SEEK_CUR); // color handle, reserved
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());

  long pos=input->tell();
  if (pixmap.m_rowBytes*8 < pixmap.m_rect.size().y()) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readBitmap: row bytes seems to short: %d/%d... \n", pixmap.m_rowBytes*8, pixmap.m_rect.size().y()));
    ascFile.addPos(pos);
    ascFile.addNote("Bitmap:###");
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }

  if (!pixmap.readPixmapData(*input)) {
    ascFile.addPos(pos);
    ascFile.addNote("Bitmap:###");
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  ascFile.skipZone(pos,input->tell()-1);
  if (input->tell()!=entry.end()) {
    // find 00000018000000000000000000000000
    ascFile.addPos(input->tell());
    ascFile.addNote("Bitmap-A");
  }

  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

bool MacDraft5StyleManager::readPixPat(MWAWEntry const &entry, bool inRsrc)
{
  if (inRsrc && !m_parserState->m_rsrcParser) return false;
  MWAWInputStreamPtr input = inRsrc ? m_parserState->m_rsrcParser->getInput() : m_parserState->m_input;
  if (!input || !entry.valid() || entry.length()<74) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readPixPat: entry is invalid\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = inRsrc ? m_parserState->m_rsrcParser->ascii() : m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(PixPat)[" << entry.id() << "]:";
  int val;
  for (int i=0; i<16; ++i) {
    val=(int) input->readLong(2);
    static int const expected[]= {1,0,0x1c,0,0x4e,0,0,-1,0,0,
                                  -21931,-21931,-21931,-21931,0,0
                                 }; // pattern, 0, 0
    if (val!=expected[i])
      f << "f" << i << "=" << val << ",";
  }
  // now the pixmap
  shared_ptr<MacDraft5StyleManagerInternal::Pixmap> pixmap(new MacDraft5StyleManagerInternal::Pixmap);
  pixmap->m_rowBytes = (int) input->readULong(2);
  pixmap->m_rowBytes &= 0x3FFF;

  // read the rectangle: bound
  int dim[4];
  for (int d = 0; d < 4; d++) dim[d] = (int) input->readLong(2);
  pixmap->m_rect = MWAWBox2i(MWAWVec2i(dim[1],dim[0]), MWAWVec2i(dim[3],dim[2]));
  if (pixmap->m_rect.size().x() <= 0 || pixmap->m_rect.size().y() <= 0) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readPixPat: find odd bound rectangle ... \n"));
    return false;
  }
  pixmap->m_version = (int) input->readLong(2);
  pixmap->m_packType = (int) input->readLong(2);
  pixmap->m_packSize = (int) input->readLong(4);
  for (int c = 0; c < 2; c++) {
    pixmap->m_resolution[c] = (int) input->readLong(2);
    input->readLong(2);
  }
  pixmap->m_pixelType = (int) input->readLong(2);
  pixmap->m_pixelSize = (int) input->readLong(2);
  pixmap->m_compCount = (int) input->readLong(2);
  pixmap->m_compSize = (int) input->readLong(2);
  pixmap->m_planeBytes = (int) input->readLong(4);
  f << *pixmap;
  long colorDepl=(long) input->readULong(4);
  input->seek(4, librevenge::RVNG_SEEK_CUR); // reserved
  ascFile.addPos(entry.begin()-(inRsrc ? 4 : 16));
  ascFile.addNote(f.str().c_str());

  long pos=input->tell();
  if (pixmap->m_rowBytes*8 < pixmap->m_rect.size().y()) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readPixPat: row bytes seems to short: %d/%d... \n", pixmap->m_rowBytes*8, pixmap->m_rect.size().y()));
    ascFile.addPos(pos);
    ascFile.addNote("PixPat:###");
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }

  if (colorDepl && (colorDepl<68 || !input->checkPosition(entry.begin()+colorDepl+6))) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readPixPat: the color handle seems bad\n"));
    ascFile.addPos(pos);
    ascFile.addNote("PixPat-A:###");
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  if (colorDepl) {
    input->seek(entry.begin()+colorDepl+6, librevenge::RVNG_SEEK_SET);
    long colorPos=input->tell();
    f.str("");
    f << "PixPat-colors:";
    int N=(int) input->readULong(2);
    if (N>2000) {
      MWAW_DEBUG_MSG(("MacDraft5StyleManager::readPixPat: the number of color seems bad\n"));
      f << "###";
      N=2000;
    }
    f << "N=" << N << ",";
    pixmap->m_colorTable.resize(size_t(N+1));

    int numColor=int(entry.end()-colorPos)/8;
    bool ok=true;
    for (int i=0; i<numColor; ++i) {
      int id=(int) input->readULong(2);
      uint8_t col[3];
      for (int j=0; j<3; ++j) col[j]=(uint8_t)(input->readULong(2)>>2);
      MWAWColor color(col[0],col[1],col[2]);
      if (id!=i) f << "col" << id << "=" << color;
      else f << color;
      if (id>N) {
        if (ok) {
          MWAW_DEBUG_MSG(("MacDraft5StyleManager::readPixPat: first field size seems bad\n"));
          ok=false;
        }
        f << "###" << ",";
        continue;
      }
      f << ",";
      pixmap->m_colorTable[size_t(id)]=color;
    }
    if (!ok) {
      ascFile.addPos(colorPos);
      ascFile.addNote(f.str().c_str());
    }
    else
      ascFile.skipZone(colorPos,entry.end()-1);
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (!pixmap->readPixmapData(*input)) {
    ascFile.addPos(pos);
    ascFile.addNote("PixPat:###");
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  ascFile.skipZone(pos,input->tell()-1);
  if (input->tell()!=entry.end() && input->tell()!=entry.begin()+colorDepl+6) {
    // remain 6 empty byte: maybe some pointer
    ascFile.addPos(input->tell());
    ascFile.addNote("PixPat-A");
  }

  m_state->m_pixIdToPixmapMap[entry.id()]=pixmap;
#ifdef DEBUG_WITH_FILES
  librevenge::RVNGBinaryData data;
  std::string type;
  MWAWVec2i pictSize;
  MWAWColor avColor;
  if (pixmap->get(data, type, pictSize, avColor)) {
    f.str("");
    f << "PixPat" << entry.id() << ".ppm";
    libmwaw::Debug::dumpFile(data, f.str().c_str());
  }
#endif

  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

bool MacDraft5StyleManager::readVersion(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  if (!input || !entry.valid() || entry.length()<8) {
    MWAW_DEBUG_MSG(("MWAWMacDraft5StyleManager::readVersion: entry is invalid\n"));
    return false;
  }
  MWAWRSRCParser::Version vers;
  entry.setParsed(true);
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  vers.m_majorVersion = (int) input->readULong(1);
  vers.m_minorVersion = (int) input->readULong(1);
  long val = (long) input->readULong(1);
  if (val) f << "devStage=" << val << ",";
  val = (long) input->readULong(1);
  if (val) f << "preReleaseLevel=" << std::hex << val << std::dec << ",";
  vers.m_countryCode = (int) input->readULong(2);
  for (int i = 0; i < 2; i++) {
    int sz = (int) input->readULong(1);
    long pos = input->tell();
    if (pos+sz > entry.end()) {
      MWAW_DEBUG_MSG(("MWAWMacDraft5StyleManager::readVersion: can not read strings %d\n",i));
      return false;
    }
    std::string str("");
    for (int c = 0; c < sz; c++)
      str+=(char) input->readULong(1);
    if (i==0)
      vers.m_versionString = str;
    else
      vers.m_string = str;
  }
  vers.m_extra = f.str();
  f << "Entries(RSRCvers)[" << entry.id() << "]:" << vers;
  ascFile.addPos(entry.begin()-16);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// PICT list
////////////////////////////////////////////////////////////
bool MacDraft5StyleManager::readBitmapList(MWAWEntry const &entry, bool inRsrc)
{
  if (inRsrc && !m_parserState->m_rsrcParser) return false;
  MWAWInputStreamPtr input = inRsrc ? m_parserState->m_rsrcParser->getInput() : m_parserState->m_input;
  if (!input || !entry.valid() || entry.length()<30 || (entry.length()%12)<6 || (entry.length()%12)>7) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readBitmapList: entry is invalid\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = inRsrc ? m_parserState->m_rsrcParser->ascii() : m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(BitmList):";
  if (entry.id()!=0) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readBitmapList: entry id seems odd\n"));
    f << "##id=" << entry.id() << ",";
  }
  int val;
  for (int i=0; i<3; ++i) { // always 0
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  int N=(int) input->readULong(2);
  if (30+12*N!=entry.length() && 31+12*N!=entry.length()) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readBitmapList:N seems bad\n"));
    f << "##N=" << N << ",";
    if (30+12*N>entry.length())
      N=int((entry.length()-30)/12);
  }
  val=(int) input->readLong(2); // always 0 ?
  if (val) f << "f3=" << val << ",";
  val=(int) input->readLong(2); // always c
  if (val!=12) f << "#fSz=" << val << ",";
  long dataSz=input->readLong(4);
  if ((dataSz%12) || dataSz>12*N) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readBitmapList:dataSize seems bad\n"));
    f << "##dataSz=" << dataSz << ",";
  }
  for (int i=0; i<7; ++i) {
    val=(int) input->readLong(2);
    static int const expected[]= {0,0xc,0,4,0,0,0};
    if (val!=expected[i]) f << "f" << i+4 << "=" << val << ",";
  }

  ascFile.addPos(entry.begin()-(inRsrc ? 4 : 16));
  ascFile.addNote(f.str().c_str());

  for (long i=0; i<N; ++i) {
    long pos=input->tell();
    f.str("");
    f << "BitmList-" << i << ":";
    MWAWEntry dataEntry;
    dataEntry.setBegin((long) input->readULong(4));
    dataEntry.setLength((long) input->readULong(4));

    if (dataEntry.begin()==0) // none
      ;
    else if (!dataEntry.valid() || !input->checkPosition(dataEntry.end())) {
      MWAW_DEBUG_MSG(("MacDraft5StyleManager::readBitmapList:dataEntry seems bad\n"));
      f << "###data=" << std::hex << dataEntry.begin() << ":" << dataEntry.length() << ",";
    }
    else {
      f << "data=" << std::hex << dataEntry.begin() << ":" << dataEntry.length() << ",";
      dataEntry.setId((int) i);
      dataEntry.setType("bitmap");
      if (m_state->m_beginToBitmapEntryMap.find(dataEntry.begin())==m_state->m_beginToBitmapEntryMap.end())
        m_state->m_beginToBitmapEntryMap[dataEntry.begin()]=dataEntry;
      else {
        MWAW_DEBUG_MSG(("MacDraft5StyleManager::readBitmapList:dataEntry already exist\n"));
        f << "###dupplicated,";

      }
    }
    for (int j=0; j<4; ++j) { // fl0=fl1=1
      val=(int) input->readLong(1);
      if (val==1) f << "fl" << j << ",";
      else if (val) f << "#fl" << j << "=" << val << ",";
    }
    input->seek(pos+12, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}


////////////////////////////////////////////////////////////
// RSRC various
////////////////////////////////////////////////////////////
bool MacDraft5StyleManager::readRSRCList(MWAWEntry const &entry, bool inRsrc)
{
  if (inRsrc && !m_parserState->m_rsrcParser) return false;
  MWAWInputStreamPtr input = inRsrc ? m_parserState->m_rsrcParser->getInput() : m_parserState->m_input;
  if (!input || !entry.valid() || entry.length()!=0x1f) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readRSRCList: entry is invalid\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = inRsrc ? m_parserState->m_rsrcParser->ascii() : m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(RSRCList)[" << entry.type() << "-" << entry.id() << "]:";
  int val=(int) input->readLong(2);
  if (val!=entry.id()) f << "#id=" << val << ",";
  std::string name("");
  for (int i=0; i<4; ++i) name+=(char) input->readULong(1);
  if (name!=entry.type()) f << "#type=" << name << ",";
  val=(int) input->readULong(2); // 40|48|..|28d6
  if (val)
    f << "fl=" << std::hex << val << ",";
  for (int i=0; i<8; ++i) { // f3=f5=c|78, f6=0|4, f7=1|6|8
    val=(int) input->readLong(2);
    static int const expected[]= {0,0xc,0,0xc,0,0xc,0,0};
    if (val!=expected[i]) f << "f" << i << "=" << val << ",";
  }
  int id=(int) input->readULong(2);
  name="";
  for (int i=0; i<4; ++i) name+=(char) input->readULong(1);
  f << name << ":" << id << ",";
  val=(int) input->readLong(1); // 0|-1|3f
  if (val)
    f << "fl2=" << std::hex << val << ",";
  ascFile.addPos(entry.begin()-(inRsrc ? 4 : 16));
  ascFile.addNote(f.str().c_str());
  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

bool MacDraft5StyleManager::readOpcd(MWAWEntry const &entry, bool inRsrc)
{
  if (inRsrc && !m_parserState->m_rsrcParser) return false;
  MWAWInputStreamPtr input = inRsrc ? m_parserState->m_rsrcParser->getInput() : m_parserState->m_input;
  if (!input || !entry.valid() || (entry.length()%4)) {
    MWAW_DEBUG_MSG(("MacDraft5StyleManager::readOpcd: entry is invalid\n"));
    return false;
  }
  entry.setParsed(true);
  libmwaw::DebugFile &ascFile = inRsrc ? m_parserState->m_rsrcParser->ascii() : m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(RSRCOpcd)[" << entry.id() << "]:";
  long N=entry.length()/4;
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  for (long i=0; i<N; ++i) { // find a serie of 125k 0x3f800000: double4?
    long val=(long) input->readULong(4);
    if (val!=0x3f800000)
      f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  if (N>25)
    ascFile.skipZone(entry.begin()+100, entry.end()-1);

  ascFile.addPos(entry.begin()-(inRsrc ? 4 : 16));
  ascFile.addNote(f.str().c_str());
  return true;
}


////////////////////////////////////////////////////////////
//
// send data
//
////////////////////////////////////////////////////////////

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
