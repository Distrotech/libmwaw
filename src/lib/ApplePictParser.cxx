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

#include "MWAWFont.hxx"
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
#include "MWAWSubDocument.hxx"

#include "ApplePictParser.hxx"

/** Internal: the structures of a ApplePictParser */
namespace ApplePictParserInternal
{

//! internal: low level class to store a region
struct Region {
  //! constructor
  Region() : m_bdBox(), m_points(), m_extra("")
  {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Region const &rgn)
  {
    o << rgn.m_bdBox << ",";
    if (!rgn.m_points.empty()) {
      o << "points=[";
      for (size_t c = 0; c < rgn.m_points.size(); c++)
        o << rgn.m_points[c] << ",";
      o << "],";
    }
    o << rgn.m_extra;
    return o;
  }
  //! the bdbox
  MWAWBox2i m_bdBox;
  //! the set of points which defines the mask
  std::vector<MWAWVec2i> m_points;
  //! extra data
  std::string m_extra;
};

//!  Internal and low level: a class used to read pack/unpack black-white bitmap
struct Bitmap {
  Bitmap() : m_rowBytes(), m_rect(), m_src(), m_dst(), m_region(),
    m_bitmap(), m_mode(0) {}

  //! operator<< for Bitmap
  friend std::ostream &operator<< (std::ostream &o, Bitmap const &f)
  {
    o << "rDim=" << f.m_rowBytes << ", " << f.m_rect << ", " << f.m_src << ", " << f.m_dst;
    if (f.m_region.get()) o << ", " << *f.m_region;
    return o;
  }

  //! creates the bitmap from the packdata
  bool unpackedData(unsigned char const *pData, int sz)
  {
    int rPos = 0;
    size_t wPos = m_bitmap.size(), wNPos = wPos+size_t(m_rowBytes);
    m_bitmap.resize(size_t(wNPos));

    while (rPos < sz) {
      if (rPos+2 > sz) return false;
      signed char n = (signed char) pData[rPos++];
      if (n < 0) {
        int nCount = 1-n;
        if (wPos+size_t(nCount) > wNPos) return false;

        unsigned char val = pData[rPos++];
        for (int i = 0; i < nCount; i++)
          m_bitmap[wPos++] = val;
        continue;
      }
      int nCount = 1+n;
      if (rPos+nCount > sz || wPos+size_t(nCount) > wNPos) return false;
      for (int i = 0; i < nCount; i++)
        m_bitmap[wPos++] = pData[rPos++];
    }
    return (wPos == wNPos);
  }

  //! parses the bitmap data zone
  bool readBitmapData(MWAWInputStream &input, bool packed)
  {
    int numRows = m_rect.size().y(), szRowSize=1;

    if (packed) {
      // CHECKME: the limit(1/2 bytes) is probably 251: the value for a Pict2.0
      //        from collected data files, we have 246 < limit < 254
      if (m_rowBytes > 250) szRowSize = 2;
    }
    else
      m_bitmap.resize(size_t(numRows*m_rowBytes));

    size_t pos=0;
    for (int i = 0; i < numRows; i++) {
      if (input.isEnd()) break;

      if (!packed) {
        unsigned long numR = 0;
        unsigned char const *data = input.read(size_t(m_rowBytes), numR);
        if (!data || int(numR) != m_rowBytes) {
          MWAW_DEBUG_MSG(("ApplePictParserInternal::Bitmap::readBitmapData: can not read line %d/%d (%d chars)\n", i, numRows, m_rowBytes));
          return false;
        }
        for (int j = 0; j < m_rowBytes; j++)
          m_bitmap[pos++]=data[j];
      }
      else {
        int numB = (int) input.readULong(szRowSize);
        if (numB < 0 || numB > 2*m_rowBytes) {
          MWAW_DEBUG_MSG(("ApplePictParserInternal::Bitmap::readBitmapData: odd numB:%d in row: %d/%d\n", numB, i, numRows));
          return false;
        }
        unsigned long numR = 0;
        unsigned char const *data = input.read(size_t(numB), numR);
        if (!data || int(numR) != numB) {
          MWAW_DEBUG_MSG(("ApplePictParserInternal::Bitmap::readBitmapData: can not read line %d/%d (%d chars)\n", i, numRows, numB));
          return false;
        }
        if (!unpackedData(data,numB)) {
          MWAW_DEBUG_MSG(("ApplePictParserInternal::Bitmap::readBitmapData: can not unpacked line:%d\n", i));
          return false;
        }
      }
    }
    return true;
  }
  //! returns the bitmap and the type
  bool get(librevenge::RVNGBinaryData &dt, std::string &type) const
  {
    if (m_rowBytes <= 0) return false;
    int nRows = int(m_bitmap.size())/m_rowBytes;
    MWAWPictBitmapBW bitmap(MWAWVec2i(m_rect.size().x(),nRows));
    if (!bitmap.valid()) return false;

    for (int i = 0; i < nRows; i++)
      bitmap.setRowPacked(i, &m_bitmap[size_t(i*m_rowBytes)]);

    return bitmap.getBinary(dt, type);
  }

  //! the num of bytes used to store a row
  int m_rowBytes;
  MWAWBox2i m_rect /** the bitmap rectangle */, m_src/** the initial dimension */, /** another final dimension */ m_dst  ;
  //! the region
  shared_ptr<Region> m_region;
  //! the bitmap
  std::vector<unsigned char> m_bitmap;
  //! the encoding mode ?
  int m_mode;
};

//! Internal and low level: a class used to read a color map in a Apple Pict
struct ColorTable {
  //! constructor
  ColorTable() : m_flags(0), m_colors() {}

  //! tries to read a colortable
  bool read(MWAWInputStream &input)
  {
    long actPos = input.tell();
    input.seek(4, librevenge::RVNG_SEEK_CUR); // ignore seed
    m_flags = (int) input.readULong(2);
    int n = (int) input.readLong(2)+1;
    if (n < 0 || !input.checkPosition(actPos+8+8*n)) return false;
    m_colors.resize(size_t(n));
    for (size_t i = 0; i < size_t(n); i++) {
      input.readULong(2); // indexId: ignored
      unsigned char col[3];
      for (int c = 0 ; c < 3; c++) {
        col[c] = (unsigned char) input.readULong(1);
        input.readULong(1);
      }
      m_colors[i] = MWAWColor(col[0], col[1], col[2]);
    }
    return long(input.tell()) == actPos + 8+ 8*n;
  }

  //! operator<< for ColorTable
  friend std::ostream &operator<< (std::ostream &o, ColorTable const &f)
  {
    size_t numColor = f.m_colors.size();
    o << "color";
    if (f.m_flags) o << "(" << std::hex << f.m_flags << ")";
    o << "={" << std::dec;
    for (size_t i = 0; i < numColor; i++)
      o << "col" << i << "=" << f.m_colors[i] << ",";
    o << "}";
    return o;
  }

  //! the color table flags
  int m_flags;

  //! the list of colors
  std::vector<MWAWColor> m_colors;
};

//!  Internal and low level: a class used to read pack/unpack color pixmap (version 2)
struct Pixmap {
  Pixmap() : m_rowBytes(0), m_rect(), m_version(-1), m_packType(0),
    m_packSize(0), m_pixelType(0), m_pixelSize(0), m_compCount(0),
    m_compSize(0), m_planeBytes(0), m_colorTable(), m_src(), m_dst(),
    m_region(), m_indices(), m_colors(), m_mode(0)
  {
    m_Res[0] = m_Res[1] = 0;
  }

  //! operator<< for Pixmap
  friend std::ostream &operator<< (std::ostream &o, Pixmap const &f)
  {
    o << "rDim=" << f.m_rowBytes << ", " << f.m_rect << ", " << f.m_src << ", " << f.m_dst;
    o << ", resol=" << f.m_Res[0] << "x" << f.m_Res[1];
    if (f.m_colorTable.get()) o << ", " << *f.m_colorTable;
    if (f.m_region.get()) o << ", " << *f.m_region;
    return o;
  }

  //! creates the pixmap from the packdata
  bool unpackedData(unsigned char const *pData, int sz, int byteSz, int nSize, std::vector<unsigned char> &res) const
  {
    if (byteSz<1||byteSz>4) {
      MWAW_DEBUG_MSG(("ApplePictParserInternal::Pixmap::unpackedData: unknown byteSz\n"));
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
    int numColors = m_colorTable.get() ? int(m_colorTable->m_colors.size()) : 0;
    int maxColorsIndex = -1;

    bool packed = !(m_rowBytes < 8 || m_packType == 1);
    switch (m_pixelSize) {
    case 1:
    case 2:
    case 4:
    case 8: { // indices (associated to a color map)
      nBytes = 1;
      numValuesByInt = 8/m_pixelSize;
      int numValues = (W+numValuesByInt-1)/numValuesByInt;
      if (m_rowBytes < numValues || m_rowBytes > numValues+10) {
        MWAW_DEBUG_MSG(("ApplePictParserInternal::Pixmap::readPixmapData invalid number of rowsize : %d, pixelSize=%d, W=%d\n", m_rowBytes, m_pixelSize, W));
        return false;
      }
      if (numColors == 0) {
        MWAW_DEBUG_MSG(("ApplePictParserInternal::Pixmap::readPixmapData: readPixmapData no color table \n"));
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
        MWAW_DEBUG_MSG(("ApplePictParserInternal::Pixmap::readPixmapData: do not known how to read cmpCount=%d\n", m_compCount));
        return false;
      }
      nPlanes=m_compCount;
      nBytes=1;
      if (nPlanes == 3) rowBytes = (3*rowBytes)/4;
      break;
    default:
      MWAW_DEBUG_MSG(("ApplePictParserInternal::Pixmap::readPixmapData: do not known how to read pixelsize=%d \n", m_pixelSize));
      return false;
    }
    if (m_pixelSize <= 8)
      m_indices.resize(size_t(H*W));
    else {
      if (rowBytes != W * nBytes * nPlanes) {
        MWAW_DEBUG_MSG(("ApplePictParserInternal::Pixmap::readPixmapData: find W=%d pixelsize=%d, rowSize=%d\n", W, m_pixelSize, m_rowBytes));
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
          MWAW_DEBUG_MSG(("ApplePictParserInternal::Pixmap::readPixmapData: readColors can not read line %d/%d (%d chars)\n", y, H, m_rowBytes));
          return false;
        }
        for (size_t j = 0; j < size_t(m_rowBytes); j++)
          values[j]=data[j];
      }
      else {   // ok, packed
        int numB = (int) input.readULong(szRowSize);
        if (numB < 0 || numB > 2*m_rowBytes) {
          MWAW_DEBUG_MSG(("ApplePictParserInternal::Pixmap::readPixmapData: odd numB:%d in row: %d/%d\n", numB, y, H));
          return false;
        }
        unsigned long numR = 0;
        unsigned char const *data = input.read(size_t(numB), numR);
        if (!data || int(numR) != numB) {
          MWAW_DEBUG_MSG(("ApplePictParserInternal::Pixmap::readPixmapData: can not read line %d/%d (%d chars)\n", y, H, numB));
          return false;
        }
        if (!unpackedData(data,numB, nBytes, rowBytes, values)) {
          MWAW_DEBUG_MSG(("ApplePictParserInternal::Pixmap::readPixmapData: can not unpacked line:%d\n", y));
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
      if (!m_colorTable) m_colorTable.reset(new ColorTable);
      std::vector<MWAWColor> &cols = m_colorTable->m_colors;

      // can be ok for a pixpat ; in this case:
      // maxColorsIndex -> foregroundColor, numColors -> backGroundColor
      // and intermediate index fills with intermediate colors
      int numUnset = maxColorsIndex-numColors+1;

      int decGray = (numUnset==1) ? 0 : 255/(numUnset-1);
      for (int i = 0; i < numUnset; i++)
        cols.push_back(MWAWColor((unsigned char)(255-i*decGray), (unsigned char)(255-i*decGray), (unsigned char)(255-i*decGray)));
      MWAW_DEBUG_MSG(("ApplePictParserInternal::Pixmap::readPixmapData: find index=%d >= numColors=%d\n", maxColorsIndex, numColors));

      return true;
    }
    return true;
  }
  //! returns the pixmap
  bool get(librevenge::RVNGBinaryData &dt, std::string &type) const
  {
    int W = m_rect.size().x();
    if (W <= 0) return false;
    if (m_colorTable.get() && m_indices.size()) {
      int nRows = int(m_indices.size())/W;
      MWAWPictBitmapIndexed pixmap(MWAWVec2i(W,nRows));
      if (!pixmap.valid()) return false;

      pixmap.setColors(m_colorTable->m_colors);

      size_t rPos = 0;
      for (int i = 0; i < nRows; i++) {
        for (int x = 0; x < W; x++)
          pixmap.set(x, i, m_indices[rPos++]);
      }

      return pixmap.getBinary(dt, type);
    }

    if (m_colors.size()) {
      int nRows = int(m_colors.size())/W;
      MWAWPictBitmapColor pixmap(MWAWVec2i(W,nRows));
      if (!pixmap.valid()) return false;

      size_t rPos = 0;
      for (int i = 0; i < nRows; i++) {
        for (int x = 0; x < W; x++)
          pixmap.set(x, i, m_colors[rPos++]);
      }

      return pixmap.getBinary(dt, type);
    }

    MWAW_DEBUG_MSG(("ApplePictParserInternal::Pixmap::get: can not find any indices or colors \n"));
    return false;
  }

  //! the num of bytes used to store a row
  int m_rowBytes;
  MWAWBox2i m_rect /** the pixmap rectangle */;
  int m_version /** the pixmap version */;
  int m_packType /** the packing format */;
  long m_packSize /** size of data in the packed state */;
  int m_Res[2] /** horizontal/vertical definition */;
  int m_pixelType /** format of pixel image */;
  int m_pixelSize /** physical bit by image */;
  int m_compCount /** logical components per pixels */;
  int m_compSize /** logical bits by components */;
  long m_planeBytes /** offset to the next plane */;
  shared_ptr<ColorTable> m_colorTable /** the color table */;

  MWAWBox2i m_src/** the initial dimension */, /** another final dimension */ m_dst  ;
  //! the region
  shared_ptr<Region> m_region;
  //! the pixmap indices
  std::vector<int> m_indices;
  //! the colors
  std::vector<MWAWColor> m_colors;
  //! the encoding mode ?
  int m_mode;
};

////////////////////////////////////////
//! Internal: the state of a ApplePictParser
struct State {
  //! constructor
  State() : m_version(0), m_bdBox(), m_origin(0,0), m_penPosition(0,0), m_textPosition(0,0),
    m_penSize(1,1), m_ovalSize(1,1), m_penMode(0), m_textMode(0), m_isHiliteMode(false), m_font(3,12),
    m_foreColor(MWAWColor::black()), m_backColor(MWAWColor::white()), m_hiliteColor(MWAWColor::black()), m_opColor(MWAWColor::black()),
    m_penPattern(), m_backgroundPattern(), m_fillPattern(),
    m_rectangle(), m_roundRectangle(), m_circle(), m_pie(), m_points()
  {
  }
  //! init the patterns list
  void initPatterns();
  //! returns true if the shape is invisible
  bool isInvisible(ApplePictParser::DrawingMethod method) const
  {
    if (method==ApplePictParser::D_INVERT ||
        (method==ApplePictParser::D_TEXT && m_textMode==23) ||
        (method!=ApplePictParser::D_TEXT && m_penMode==23)) return true;
    return (method==ApplePictParser::D_FRAME && (m_penSize[0] == 0 || m_penSize[1] == 0));
  }
  //! update the actual style
  void updateStyle(ApplePictParser::DrawingMethod method, MWAWGraphicStyle &style)
  {
    style=MWAWGraphicStyle();
    if (method!=ApplePictParser::D_FRAME)
      style.m_lineWidth=0;
    else
      style.m_lineWidth=0.5f*float(m_penSize[0]+m_penSize[1]);
    MWAWColor color;
    switch (method) {
    case ApplePictParser::D_FRAME:
    case ApplePictParser::D_TEXT: // set foreColor, it is use for defining the font color
      color=m_foreColor;
      if (!m_penPattern.empty())
        m_penPattern.getAverageColor(color);
      style.m_lineColor=color;
      break;
    case ApplePictParser::D_PAINT:
      if (m_penPattern.empty())
        style.setSurfaceColor(m_foreColor);
      else if (m_penPattern.getUniqueColor(color))
        style.setSurfaceColor(color);
      else
        style.m_pattern=m_penPattern;
      break;
    case ApplePictParser::D_FILL:
      if (m_fillPattern.empty())
        style.setSurfaceColor(m_foreColor);
      else if (m_fillPattern.getUniqueColor(color))
        style.setSurfaceColor(color);
      else
        style.m_pattern=m_fillPattern;
      break;
    case ApplePictParser::D_ERASE:
      if (m_backgroundPattern.empty())
        style.setSurfaceColor(MWAWColor(255,255,255));
      else if (m_backgroundPattern.getUniqueColor(color))
        style.setSurfaceColor(color);
      else
        style.m_pattern=m_backgroundPattern;
      break;
    case ApplePictParser::D_INVERT:
      break;
    case ApplePictParser::D_UNDEFINED:
    default:
      break;
    }

  }
  //! update the position
  void updatePosition(MWAWVec2f const &orig, MWAWPosition &pos)
  {
    pos=MWAWPosition(orig-m_bdBox[0]+m_origin, MWAWVec2f(-1,-1), librevenge::RVNG_POINT);
    pos.m_anchorTo=MWAWPosition::Page;
  }
  //! update the position
  void updatePosition(MWAWBox2f const &bdBox, MWAWPosition &pos)
  {
    pos=MWAWPosition(bdBox[0]-m_bdBox[0]+m_origin, bdBox.size(), librevenge::RVNG_POINT);
    pos.m_anchorTo=MWAWPosition::Page;
  }
  //! the file version
  int m_version;
  //! the bounding rectangle
  MWAWBox2f m_bdBox;
  //! the origin
  MWAWVec2f m_origin;
  //! the actual pen position
  MWAWVec2i m_penPosition;
  //! the actual text position
  MWAWVec2i m_textPosition;
  //! the actual pensize
  MWAWVec2i m_penSize;
  //! the actual ovalsize
  MWAWVec2i m_ovalSize;
  //! the pen mode
  int m_penMode;
  //! the text mode
  int m_textMode;
  //! true if we must use the hilite mode
  bool m_isHiliteMode;
  //! the actual font
  MWAWFont m_font;
  //! the foreground color
  MWAWColor m_foreColor;
  //! the background color
  MWAWColor m_backColor;
  //! the hilite color
  MWAWColor m_hiliteColor;
  //! the op color
  MWAWColor m_opColor;
  //! the pen pattern
  MWAWGraphicStyle::Pattern m_penPattern;
  //! the background pattern
  MWAWGraphicStyle::Pattern m_backgroundPattern;
  //! the fill pattern
  MWAWGraphicStyle::Pattern m_fillPattern;
  //! the last rectangle
  MWAWBox2i m_rectangle;
  //! the last round rectangle
  MWAWBox2i m_roundRectangle;
  //! the last circle
  MWAWBox2i m_circle;
  //! the last pie
  MWAWBox2i m_pie;
  //! the last polygon points
  std::vector<MWAWVec2i> m_points;
};

////////////////////////////////////////
//! Internal: the subdocument of a ApplePictParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(ApplePictParser &pars, MWAWInputStreamPtr input, MWAWEntry const &entry) : MWAWSubDocument(&pars, input, entry) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const
  {
    if (MWAWSubDocument::operator!=(doc)) return true;
    SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
    if (!sDoc) return true;
    return false;
  }

  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const
  {
    return !operator!=(doc);
  }
  //! the parser function
  void parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType)
{
  if (!listener || !listener->canWriteText()) {
    MWAW_DEBUG_MSG(("ApplePictParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  ApplePictParser *parser=dynamic_cast<ApplePictParser *>(m_parser);
  if (!parser) {
    MWAW_DEBUG_MSG(("ApplePictParserInternal::SubDocument::parse: no parser\n"));
    return;
  }
  long pos = m_input->tell();
  parser->drawText(m_zone);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}


}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
ApplePictParser::ApplePictParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWGraphicParser(input, rsrcParser, header), m_state()
{
  init();
}

ApplePictParser::~ApplePictParser()
{
}

void ApplePictParser::init()
{
  resetGraphicListener();
  setAsciiName("main-1");

  m_state.reset(new ApplePictParserInternal::State);

  getPageSpan().setMargins(0.001);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void ApplePictParser::parse(librevenge::RVNGDrawingInterface *docInterface)
{
  if (!getInput().get() || !checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = false;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());
    checkHeader(0L);
    createDocument(docInterface);
    ok = createZones();
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("ApplePictParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetGraphicListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void ApplePictParser::createDocument(librevenge::RVNGDrawingInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getGraphicListener()) {
    MWAW_DEBUG_MSG(("ApplePictParser::createDocument: listener already exist\n"));
    return;
  }

  // create the page list
  MWAWPageSpan ps(getPageSpan());
  ps.setPageSpan(1);
  ps.setFormWidth(float(m_state->m_bdBox.size()[0])/72.f);
  ps.setFormLength(float(m_state->m_bdBox.size()[1])/72.f);
  std::vector<MWAWPageSpan> pageList(1,ps);
  MWAWGraphicListenerPtr listen(new MWAWGraphicListener(*getParserState(), pageList, documentInterface));
  setGraphicListener(listen);
  listen->startDocument();
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool ApplePictParser::createZones()
{
  //m_state->m_penPosition=m_state->m_bdBox[0];
  MWAWInputStreamPtr input = getInput();
  long pos, debPos=input->tell();
  while (!input->isEnd()) {
    pos=input->tell();
    if (!readZone()) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
  }

  bool ok=true;
  if (!input->isEnd()) {
    pos=input->tell();
    MWAW_DEBUG_MSG(("ApplePictParser::createZones: find extra data\n"));
    ascii().addPos(input->tell());
    ascii().addNote("Entries(Data):##");
    ok=(input->size()-debPos)<=2*(pos-debPos);
  }
  return ok;
}

bool ApplePictParser::readZone()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  int const vers=version();
  int const opLength=version()>=2 ? 2 : 1;
  if (!input->checkPosition(pos+opLength))
    return false;
  int opCode=(int) input->readULong(opLength);
  DrawingMethod drawingMethod = D_UNDEFINED;
  switch (opCode & 7) {
  case 0:
    drawingMethod = D_FRAME;
    break;
  case 1:
    drawingMethod = D_PAINT;
    break;
  case 2:
    drawingMethod = D_ERASE;
    break;
  case 3:
    drawingMethod = D_INVERT;
    break;
  case 4:
    drawingMethod = D_FILL;
    break;
  default:
    break;
  }
  long dSz;
  libmwaw::DebugStream f;
  int val;
  long actPos=input->tell();
  switch (opCode) {
  case 0: // NOP
    f << "_";
    break;
  case 1: {
    ApplePictParserInternal::Region region;
    if (!readRegion(region))
      return false;
    f << "Entries(Region)[clip]:" << region;
    break;
  }
  case 2:
    if (!readBWPattern(m_state->m_backgroundPattern))
      return false;
    f << "Entries(Pattern)[back]:" << m_state->m_backgroundPattern << ",";
    break;
  case 3:
    if (!input->checkPosition(actPos+2))
      return false;
    m_state->m_font.setId((int) input->readULong(2));
    f << "Entries(FontId):" <<  m_state->m_font.id() << ",";
    break;
  case 4: {
    if (!input->checkPosition(actPos+1))
      return false;
    f << "Entries(TextFace):";
    int flag=(int) input->readULong(1);
    uint32_t flags=0;
    if (flag&0x1) {
      flags |= MWAWFont::boldBit;
      f << "b:";
    }
    if (flag&0x2) {
      flags |= MWAWFont::italicBit;
      f << "it:";
    }
    if (flag&0x4) {
      m_state->m_font.setUnderlineStyle(MWAWFont::Line::Simple);
      f << "under:";
    }
    else
      m_state->m_font.setUnderlineStyle(MWAWFont::Line::None);
    if (flag&0x8) {
      flags |= MWAWFont::embossBit;
      f << "emboss:";
    }
    if (flag&0x10) {
      flags |= MWAWFont::shadowBit;
      f << "shadow:";
    }
    m_state->m_font.setDeltaLetterSpacing(0);
    if (flag&0x20) {
      m_state->m_font.setDeltaLetterSpacing(-1);
      f << "condensed:";
    }
    if (flag&0x40) {
      m_state->m_font.setDeltaLetterSpacing(1);
      f << "extend:";
    }
    if (flag&0x80) f << "#flag0[0x80],";
    m_state->m_font.setFlags(flags);
    break;
  }
  case 5:
  case 8: {
    if (!input->checkPosition(actPos+2))
      return false;
    int mode=(int) input->readULong(2);
    if (opCode==5) {
      f << "Entries(TextMode):";
      m_state->m_textMode=mode;
    }
    else {
      f << "Entries(PenMode):";
      m_state->m_penMode=mode;
    }
    f << getModeName(mode);
    break;
  }
  case 6:
    if (!input->checkPosition(actPos+4))
      return false;
    f << "Entries(SpaceExtra):" << float(input->readLong(4))/65536.f;
    break;
  case 7:
  case 0xb:
  case 0xc: {
    if (!input->checkPosition(actPos+4))
      return false;
    MWAWVec2i size;
    for (int i=0; i<2; ++i)
      size[1-i]=(int) input->readULong(2);
    if (opCode==7) {
      m_state->m_penSize=size;
      f << "Entries(PenSize):" << size << ",";
    }
    else if (opCode==0xb) {
      m_state->m_ovalSize=size;
      f << "Entries(OvalSize):" << size << ",";
    }
    else {
      m_state->m_origin+=MWAWVec2f((float) size[1],(float) size[0]);
      f << "Entries(Orign):delta=" << MWAWVec2i(size[1],size[0]) << ",";
    }
    break;
  }
  case 9:
    if (!readBWPattern(m_state->m_penPattern))
      return false;
    f << "Entries(Pattern)[pen]:" << m_state->m_penPattern << ",";
    break;
  case 0xa:
    if (!readBWPattern(m_state->m_fillPattern))
      return false;
    f << "Entries(Pattern)[fill]:" << m_state->m_fillPattern << ",";
    break;
  case 0xd:
    if (!input->checkPosition(actPos+2))
      return false;
    val=(int) input->readULong(2);
    f << "Entries(FontSz):" << val;
    m_state->m_font.setSize(float(val));
    break;
  case 0xe:
  case 0xf: {
    if (!input->checkPosition(actPos+2))
      return false;
    val=(int) input->readULong(4);
    MWAWColor color;
    if (opCode==0xe)
      f << "Entries(Color)[fore]:";
    else
      f << "Entries(Color)[back]:";
    switch (val) {
    case 30:
      color=MWAWColor::white();
      break;
    case 33:
      color=MWAWColor::black();
      break;
    case 69:
      color=MWAWColor(255,255,0);
      break;
    case 137:
      color=MWAWColor(255,0,255);
      break;
    case 205:
      color=MWAWColor(255,0,0);
      break;
    case 273:
      color=MWAWColor(0,255,255);
      break;
    case 341:
      color=MWAWColor(0,0,255);
      break;
    case 409:
      color=MWAWColor(0,255,0);
      break;
    default:
      MWAW_DEBUG_MSG(("ApplePictParser::readZone: find unknown color\n"));
      break;
    }
    f << color;
    if (opCode==0xe)
      m_state->m_foreColor=color;
    else
      m_state->m_backColor=color;
    break;
  }
  case 0x10:
    if (!input->checkPosition(actPos+8))
      return false;
    f << "Entries(TextRatio):";
    for (int i=0; i<2; ++i) {
      if (i==0)
        f << "num=";
      else
        f << "denom=";
      for (int j=0; j<2; ++j) {
        f << input->readULong(2);
        if (j==0)
          f << "x";
        else
          f << ",";
      }
    }
    break;
  case 0x11:
    if (!input->checkPosition(actPos+1))
      return false;
    f << "Entries(Version):" << input->readLong(1);
    break;
  case 0x12:
    if (!readColorPattern(m_state->m_backgroundPattern))
      return false;
    f << "Entries(CPat)[back]:" << m_state->m_backgroundPattern << ",";
    break;
  case 0x13:
    if (!readColorPattern(m_state->m_penPattern))
      return false;
    f << "Entries(CPat)[pen]:" << m_state->m_penPattern << ",";
    break;
  case 0x14:
    if (!readColorPattern(m_state->m_fillPattern))
      return false;
    f << "Entries(CPat)[fill]:" << m_state->m_fillPattern << ",";
    break;
  case 0x15:
    if (!input->checkPosition(actPos+2))
      return false;
    f << "Entries(PnLocHFrac):" << float(input->readLong(2))/256.f;
    break;
  case 0x16:
    if (!input->checkPosition(actPos+2))
      return false;
    val=(int) input->readLong(2);
    f << "Entries(ChExtra):" << val;
    m_state->m_font.setDeltaLetterSpacing((float) val);
    break;
  case 0x1c:
    f << "Entries(HiliteMode):";
    m_state->m_isHiliteMode=true;
    break;
  case 0x1e:
    f << "Entries(HiliteDef):";
    m_state->m_hiliteColor=MWAWColor::black();
    break;
  case 0x1a:
  case 0x1b:
  case 0x1d:
  case 0x1f: {
    MWAWColor col;
    if (!readRGBColor(col))
      return false;
    f << "Entries(Color)";
    if (opCode==0x1a) {
      f << "[fore]";
      m_state->m_foreColor=col;
    }
    else if (opCode==0x1b) {
      f << "[back]";
      m_state->m_backColor=col;
    }
    else if (opCode==0x1d) {
      f << "[hilite]";
      m_state->m_hiliteColor=col;
    }
    else {
      f << "[op]";
      m_state->m_opColor=col;
    }
    f << ":" << col;
    break;
  }
  case 0x20: {
    if (!input->checkPosition(8+actPos))
      return false;
    f << "Entries(Line):";
    for (int i=0; i<2; ++i)
      m_state->m_penPosition[1-i]=(int) input->readLong(2);
    MWAWVec2i point;
    for (int i=0; i<2; ++i)
      point[1-i]=(int) input->readLong(2);
    f << m_state->m_penPosition << "->" << point << ",";
    drawLine(point);
    break;
  }
  case 0x21: {
    if (!input->checkPosition(4+actPos))
      return false;
    f << "Entries(Line):";
    MWAWVec2i point;
    for (int i=0; i<2; ++i)
      point[1-i]=(int) input->readLong(2);
    f << m_state->m_penPosition << "->" << point << ",";
    drawLine(point);
    break;
  }
  case 0x22: {
    if (!input->checkPosition(6+actPos))
      return false;
    f << "Entries(Line):";
    for (int i=0; i<2; ++i)
      m_state->m_penPosition[1-i]=(int) input->readLong(2);
    MWAWVec2i point;
    for (int i=0; i<2; ++i)
      point[i]=m_state->m_penPosition[i]+(int) input->readLong(1);
    f << m_state->m_penPosition << "->" << point << ",";
    drawLine(point);
    break;
  }
  case 0x23: {
    if (!input->checkPosition(2+actPos))
      return false;
    f << "Entries(Line):";
    MWAWVec2i point;
    for (int i=0; i<2; ++i)
      point[i]=m_state->m_penPosition[i]+(int) input->readLong(1);
    f << m_state->m_penPosition << "->" << point << ",";
    drawLine(point);
    break;
  }
  case 0x28: {
    if (!input->checkPosition(5+actPos))
      return false;
    f << "Entries(TextData):";
    for (int i=0; i<2; ++i)
      m_state->m_textPosition[1-i]=(int) input->readLong(2);
    f << m_state->m_textPosition << ",";
    std::string text("");
    if (!readAndDrawText(text))
      return false;
    f << text;
    break;
  }
  case 0x29:
  case 0x2a: {
    if (!input->checkPosition(2+actPos))
      return false;
    f << "Entries(TextData):";
    m_state->m_textPosition[opCode-0x29]=
      m_state->m_textPosition[opCode-0x29]+(int) input->readULong(1);
    f << m_state->m_textPosition << ",";
    std::string text("");
    if (!readAndDrawText(text))
      return false;
    f << text;
    break;
  }
  case 0x2b: {
    if (!input->checkPosition(3+actPos))
      return false;
    f << "Entries(TextData):";
    for (int i=0; i<2; ++i)
      m_state->m_textPosition[i]=m_state->m_textPosition[i]+
                                 (int) input->readULong(1);
    f << m_state->m_textPosition << ",";
    std::string text("");
    if (!readAndDrawText(text))
      return false;
    f << text;
    break;
  }
  case 0x2c: {
    if (!input->checkPosition(5+actPos))
      return false;
    f << "Entries(FontName):";
    dSz=(long) input->readULong(2);
    f << "dSz=" << dSz << ",";
    if (!input->checkPosition(2+dSz+actPos))
      return false;
    int id=(int) input->readULong(2);
    f << "id=" << id << ",";
    int sSz=(int) input->readULong(1);
    if (sSz>dSz+3) {
      MWAW_DEBUG_MSG(("ApplePictParser::readZone: font name size seems bad\n"));
      sSz=int(dSz-3);
    }
    if (!input->checkPosition(5+sSz+actPos))
      return false;
    std::string name("");
    for (int i=0; i < sSz; ++i) name+=(char) input->readULong(1);
    f << name;
    if (!name.empty())
      getParserState()->m_fontConverter->setCorrespondance(id, name);
    m_state->m_font.setId(id);
    input->seek(2+dSz+actPos, librevenge::RVNG_SEEK_SET);
    break;
  }
  case 0x2d:
    if (!input->checkPosition(actPos+2))
      return false;
    dSz=(long) input->readULong(2);
    if (!input->checkPosition(actPos+2+dSz))
      return false;
    f << "Entries(LineSpacing):";
    if (dSz!=8) {
      MWAW_DEBUG_MSG(("ApplePictParser::readZone: the data length seems bad\n"));
      f << "###";
      input->seek(actPos+2+dSz, librevenge::RVNG_SEEK_SET);
      break;
    }
    val=(int) input->readLong(4);
    m_state->m_font.setDeltaLetterSpacing(float(val)/65536.f);
    f << "char[spacing]=" << float(val)/65536.f << ",";
    f << "space[spacing]=" << float(input->readLong(4))/65536.f << ",";
    break;
  case 0x2e:
    if (!input->checkPosition(actPos+2))
      return false;
    dSz=(long) input->readULong(2);
    if (!input->checkPosition(actPos+2+dSz))
      return false;
    f << "Entries(Glyph):";
    if (dSz!=4) {
      MWAW_DEBUG_MSG(("ApplePictParser::readZone: the data length seems bad\n"));
      f << "###";
      input->seek(actPos+2+dSz, librevenge::RVNG_SEEK_SET);
      break;
    }
    for (int i=0; i<4; ++i) {
      val=(int) input->readLong(1);
      if (!val) continue;
      f << "f" << i << "=" << val << ",";
    }
    break;
  case 0x30:
  case 0x31:
  case 0x32:
  case 0x33:
  case 0x34:
    if (!readAndDrawRectangle(drawingMethod))
      return false;
    break;
  case 0x38:
  case 0x39:
  case 0x3a:
  case 0x3b:
  case 0x3c:
    f << "Entries(Rectangle)[" << getDrawingName(drawingMethod) << "]:same";
    drawRectangle(drawingMethod);
    break;
  case 0x40:
  case 0x41:
  case 0x42:
  case 0x43:
  case 0x44:
    if (!readAndDrawRoundRectangle(drawingMethod))
      return false;
    break;
  case 0x48:
  case 0x49:
  case 0x4a:
  case 0x4b:
  case 0x4c:
    f << "Entries(RoundRect)[" << getDrawingName(drawingMethod) << "]:same";
    drawRoundRectangle(drawingMethod);
    break;
  case 0x50:
  case 0x51:
  case 0x52:
  case 0x53:
  case 0x54:
    if (!readAndDrawCircle(drawingMethod))
      return false;
    break;
  case 0x58:
  case 0x59:
  case 0x5a:
  case 0x5b:
  case 0x5c:
    f << "Entries(Circle)[" << getDrawingName(drawingMethod) << "]:same";
    drawCircle(drawingMethod);
    break;
  case 0x60:
  case 0x61:
  case 0x62:
  case 0x63:
  case 0x64:
    if (!readAndDrawPie(drawingMethod))
      return false;
    break;
  case 0x68:
  case 0x69:
  case 0x6a:
  case 0x6b:
  case 0x6c: {
    if (!input->checkPosition(actPos+4))
      return false;
    f << "Entries(Pie)[" << getDrawingName(drawingMethod) << "]:same";
    int angles[2];
    for (int i=0; i<2; ++i) angles[i]=(int) input->readLong(2);
    drawPie(drawingMethod, angles[0], angles[1]);
    break;
  }
  case 0x70:
  case 0x71:
  case 0x72:
  case 0x73:
  case 0x74:
    if (!readAndDrawPolygon(drawingMethod))
      return false;
    break;
  case 0x78:
  case 0x79:
  case 0x7a:
  case 0x7b:
  case 0x7c:
    f << "Entries(Polygon)[" << getDrawingName(drawingMethod) << "]:same";
    drawPolygon(drawingMethod);
    break;
  case 0x80:
  case 0x81:
  case 0x82:
  case 0x83:
  case 0x84: {
    ApplePictParserInternal::Region region;
    if (!readRegion(region))
      return false;
    f << "Entries(Region)[" << getDrawingName(drawingMethod) << "]:" << region;
    break;
  }
  case 0x88:
  case 0x89:
  case 0x8a:
  case 0x8b:
  case 0x8c:
    f << "Entries(Region)[" << getDrawingName(drawingMethod) << "]:same";
    break;
  case 0x90:
  case 0x91:
  case 0x98:
  case 0x99: {
    // first check if it is a bitmap or a pixmap
    bool pixmap = input->readULong(2) & 0x8000;
    input->seek(-2, librevenge::RVNG_SEEK_CUR);
    bool packed = (opCode&8);
    bool hasRgn = (opCode&1);
    if (pixmap) {
      ApplePictParserInternal::Pixmap bitmap;
      if (!readPixmap(bitmap, packed, true, true, hasRgn))
        return false;
      drawPixmap(bitmap);
      f << "Entries(Pixmap):";
    }
    else {
      ApplePictParserInternal::Bitmap bitmap;
      if (!readBitmap(bitmap, packed, hasRgn))
        return false;
      drawBitmap(bitmap);
      f << "Entries(Bitmap):";
    }
    break;
  }
  case 0x9a:
  case 0x9b: {
    ApplePictParserInternal::Pixmap bitmap;
    if (!readPixmap(bitmap, false, false, true, (opCode&1) ? true : false))
      return false;
    drawPixmap(bitmap);
    f << "Entries(Pixmap):";
    break;
  }
  case 0xa0:
    if (!input->checkPosition(actPos+2))
      return false;
    f << "Entries(Comment)[short]:kind="<<input->readLong(2) << ",";
    break;
  case 0xa1:
  case 0xa5: // not really a Pict1 code, but it can appear in some pict
    if (!input->checkPosition(actPos+4))
      return false;
    f << "Entries(Comment)[long]:kind="<<input->readLong(2) << ",";
    if (opCode==0xa5) f << "#unusual,";
    dSz=(long) input->readULong(2);
    if (!input->checkPosition(actPos+4+dSz))
      return false;
    input->seek(dSz, librevenge::RVNG_SEEK_CUR);
    break;
  case 0xff:
    f << "Entries(EOP):";
    if (vers>=2) input->seek(2, librevenge::RVNG_SEEK_CUR);
    break;

  //
  // Reserved
  //
  case 0x17: // reserved + 0 byes
  case 0x18:
  case 0x19:
  case 0x3d:
  case 0x3e:
  case 0x3f:
  case 0x4d:
  case 0x4e:
  case 0x4f:
  case 0x5d:
  case 0x5e:
  case 0x5f:
  case 0x6d:
  case 0x6e:
  case 0x6f:
  case 0x7d:
  case 0x7e:
  case 0x7f:
  case 0x8d:
  case 0x8e:
  case 0x8f:
  case 0xcf: // checkme
    f << "Entries(Reserved"<<std::hex << opCode << std::dec << "):";
    break;

  case 0x35: // reserved + 8 bytes
  case 0x36:
  case 0x37:
  case 0x45:
  case 0x46:
  case 0x47:
  case 0x55:
  case 0x56:
  case 0x57:
    if (!input->checkPosition(actPos+8))
      return false;
    f << "Entries(Reserved"<<std::hex << opCode << std::dec << "):";
    input->seek(8, librevenge::RVNG_SEEK_CUR);
    break;

  case 0x65: // reserved + 12 bytes
  case 0x66:
  case 0x67:
    if (!input->checkPosition(actPos+12))
      return false;
    f << "Entries(Reserved"<<std::hex << opCode << std::dec << "):";
    input->seek(8, librevenge::RVNG_SEEK_CUR);
    break;

  case 0x24:  // reserved + N bytes
  case 0x25:
  case 0x26:
  case 0x27:
  case 0x2f:
  case 0x75:
  case 0x76:
  case 0x77:
  case 0x85:
  case 0x86:
  case 0x87:
  case 0x92:
  case 0x93:
  case 0x94:
  case 0x95:
  case 0x96:
  case 0x97:
  case 0x9c:
  case 0x9d:
  case 0x9e:
  case 0x9f:
  case 0xa2:  // checkme
    if (!input->checkPosition(actPos+2))
      return false;
    f << "Entries(Reserved"<<std::hex << opCode << std::dec << "):";
    dSz=(long) input->readULong(2);
    if (!input->checkPosition(actPos+2+dSz))
      return false;
    input->seek(dSz, librevenge::RVNG_SEEK_CUR);
    break;

  default: {
    if (opCode<=0xaf) dSz=2+(int) input->readULong(2);
    else if (opCode<=0xcf) dSz=0;
    else if (opCode<=0x100) dSz=4+(long) input->readULong(4);
    else if (opCode<=0x01ff) dSz=2;
    else if (opCode<=0x0bfe) dSz=4;
    else if (opCode<=0x0bff) dSz=22;
    else if (opCode==0x0c00) dSz=24; // HeaderOp
    else if (opCode<=0x7eff) dSz=24;
    else if (opCode<=0x7fff) dSz=254;
    else if (opCode<=0x80ff) dSz=0;
    else  dSz=4+(long) input->readULong(4);
    if (!input->checkPosition(actPos+dSz))
      return false;
    f << "Entries(Reserved"<<std::hex << opCode << std::dec << "):";
    input->seek(actPos+dSz, librevenge::RVNG_SEEK_SET);
    break;
  }
  }
  if (vers>=2 && ((input->tell()-pos)%2)!=0)
    input->seek(1, librevenge::RVNG_SEEK_CUR);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

bool ApplePictParser::readAndDrawRectangle(ApplePictParser::DrawingMethod method)
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+8))
    return false;
  libmwaw::DebugStream f;
  f << "Entries(Rectangle)[" << getDrawingName(method) << "]:";
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
  m_state->m_rectangle=MWAWBox2i(MWAWVec2i(dim[1],dim[0]),MWAWVec2i(dim[3],dim[2]));
  f << m_state->m_rectangle;
  drawRectangle(method);
  ascii().addPos(pos-(version()==1 ? 1 : 2));
  ascii().addNote(f.str().c_str());
  return true;
}

bool ApplePictParser::readAndDrawRoundRectangle(ApplePictParser::DrawingMethod method)
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+8))
    return false;
  libmwaw::DebugStream f;
  f << "Entries(RoundRect)[" << getDrawingName(method) << "]:";
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
  m_state->m_roundRectangle=MWAWBox2i(MWAWVec2i(dim[1],dim[0]),MWAWVec2i(dim[3],dim[2]));
  f << m_state->m_roundRectangle;
  drawRoundRectangle(method);
  ascii().addPos(pos-(version()==1 ? 1 : 2));
  ascii().addNote(f.str().c_str());
  return true;
}

bool ApplePictParser::readAndDrawCircle(ApplePictParser::DrawingMethod method)
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+8))
    return false;
  libmwaw::DebugStream f;
  f << "Entries(Circle)[" << getDrawingName(method) << "]:";
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
  m_state->m_circle=MWAWBox2i(MWAWVec2i(dim[1],dim[0]),MWAWVec2i(dim[3],dim[2]));
  f << m_state->m_circle;
  drawCircle(method);
  ascii().addPos(pos-(version()==1 ? 1 : 2));
  ascii().addNote(f.str().c_str());
  return true;
}

bool ApplePictParser::readAndDrawPie(ApplePictParser::DrawingMethod method)
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+12))
    return false;
  libmwaw::DebugStream f;
  f << "Entries(Pie)[" << getDrawingName(method) << "]:";
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
  m_state->m_pie=MWAWBox2i(MWAWVec2i(dim[1],dim[0]),MWAWVec2i(dim[3],dim[2]));
  f << m_state->m_pie << ",";
  int angles[2];
  for (int i=0; i<2; ++i) angles[i]=(int) input->readLong(2);
  f << "angl=" << angles[0] << "x" << angles[0]+angles[1] << ",";
  drawPie(method, angles[0], angles[1]);
  ascii().addPos(pos-(version()==1 ? 1 : 2));
  ascii().addNote(f.str().c_str());
  return true;
}

bool ApplePictParser::readAndDrawPolygon(ApplePictParser::DrawingMethod method)
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  int dSz=(int) input->readULong(2);
  if (dSz<10 || (dSz%4)!=2 || !input->checkPosition(pos+dSz))
    return false;
  libmwaw::DebugStream f;
  f << "Entries(Polygon)[" << getDrawingName(method) << "]:";
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
  f << MWAWBox2i(MWAWVec2i(dim[1],dim[0]),MWAWVec2i(dim[3],dim[2])) << ",";
  int N=(dSz-10)/4;
  f << "pts=[";
  m_state->m_points.clear();
  for (int i=0; i<N; ++i) {
    float coord[2];
    for (int j=0; j<2; ++j) coord[j]=float(input->readLong(2));
    m_state->m_points.push_back(MWAWVec2f(coord[1],coord[0]));
    f << MWAWVec2f(coord[1],coord[0]) << ",";
  }
  f << "],";
  drawPolygon(method);
  ascii().addPos(pos-(version()==1 ? 1 : 2));
  ascii().addNote(f.str().c_str());
  return true;
}

bool ApplePictParser::readRGBColor(MWAWColor &color)
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+6))
    return false;
  uint8_t col[3];
  for (int i=0; i<3; ++i) col[i]=uint8_t(input->readULong(2)>>8);
  color=MWAWColor(col[0],col[1],col[2]);
  return true;
}

bool ApplePictParser::readBWPattern(MWAWGraphicStyle::Pattern &pat)
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+8))
    return false;
  pat.m_dim=MWAWVec2i(8,8);
  pat.m_data.resize(8);
  pat.m_colors[0]=MWAWColor::white();
  pat.m_colors[1]=MWAWColor::black();

  for (size_t i=0; i<8; ++i)
    pat.m_data[i]=uint8_t(input->readULong(1));
  return true;
}

bool ApplePictParser::readColorPattern(MWAWGraphicStyle::Pattern &pat)
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+10))
    return false;
  int type = (int)input->readULong(2);
  if (type !=1 && type != 2) {
    MWAW_DEBUG_MSG(("ApplePictParser::readColorPattern: unknown type=%d... \n", type));
    return false;
  }

  pat.m_dim=MWAWVec2i(8,8);
  pat.m_data.resize(8);
  pat.m_colors[0]=MWAWColor::white();
  pat.m_colors[1]=MWAWColor::black();

  for (size_t i=0; i<8; ++i)
    pat.m_data[i]=uint8_t(input->readULong(1));
  if (type==2) {
    // a color pattern -> create a uniform color pattern
    if (!readRGBColor(pat.m_colors[0]))
      return false;
    for (size_t i=0; i<8; ++i)
      pat.m_data[i]=0;
    return true;
  }
  ApplePictParserInternal::Pixmap pixmap;
  return readPixmap(pixmap, false, true, false, false);
}

bool ApplePictParser::readAndDrawText(std::string &text)
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+1))
    return false;
  int dSz=(int) input->readULong(1);
  if (!input->checkPosition(pos+1+dSz))
    return false;
  text="";
  MWAWEntry entry;
  entry.setBegin(input->tell());
  entry.setLength(dSz);
  for (int i=0; i<dSz; ++i) text+=(char) input->readULong(1);
  if (m_state->isInvisible(D_TEXT)) return true;

  MWAWListenerPtr listener=getGraphicListener();
  if (!listener || listener->canWriteText()) {
    MWAW_DEBUG_MSG(("ApplePictParser::readAndDrawText: can not find the listener\n"));
    return true;
  }
  shared_ptr<MWAWSubDocument> doc(new ApplePictParserInternal::SubDocument(*this, getInput(), entry));
  MWAWGraphicStyle style;
  m_state->updateStyle(D_TEXT, style);
  MWAWVec2f orig=m_state->m_textPosition;
  orig[1]=orig[1]-m_state->m_font.size();
  MWAWPosition position;
  m_state->updatePosition(orig, position);
  listener->insertTextBox(position, doc, style);

  input->seek(pos+1+dSz, librevenge::RVNG_SEEK_SET);
  return true;
}

bool ApplePictParser::readBitmap(ApplePictParserInternal::Bitmap &bitmap, bool packed, bool hasRegion)
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+28))
    return false;
  libmwaw::DebugStream f;
  f << "Bitmap:";
  bitmap.m_rowBytes = (int) input->readULong(2);
  bitmap.m_rowBytes &= 0x3FFF;
  if (bitmap.m_rowBytes < 0 || (!packed && bitmap.m_rowBytes > 8)) {
    MWAW_DEBUG_MSG(("ApplePictParser::readBitmap: find odd rowBytes %d... \n", bitmap.m_rowBytes));
    return false;
  }
  // read the rectangle: bound
  // ------ end of bitmap ----------
  // and the two general rectangle src, dst
  for (int c = 0; c < 3; c++) {
    int val[4];
    for (int d = 0; d < 4; d++) val[d] = (int) input->readLong(2);
    MWAWBox2i box(MWAWVec2i(val[1],val[0]), MWAWVec2i(val[3],val[2]));
    if (box.size().x() <= 0 || box.size().y() <= 0) {
      MWAW_DEBUG_MSG(("ApplePictParser::readBitmap: find odd rectangle %d... \n", c));
      return false;
    }
    if (c == 0) bitmap.m_rect=box;
    else if (c==1) bitmap.m_src = box;
    else bitmap.m_dst = box;
  }

  if (!packed && bitmap.m_rowBytes*8 < bitmap.m_rect.size().x()) {
    MWAW_DEBUG_MSG(("ApplePictParser::readBitmap: row bytes seems to short: %d/%d... \n", bitmap.m_rowBytes*8, bitmap.m_rect.size().y()));
    return false;
  }
  bitmap.m_mode = (int) input->readLong(2); // mode: I find 0,1 and 3
  if (bitmap.m_mode < 0 || bitmap.m_mode > 64) {
    MWAW_DEBUG_MSG(("ApplePictParser::readBitmap: unknown mode: %d \n", bitmap.m_mode));
    return false;
  }

  if (hasRegion) { // CHECKME...
    shared_ptr<ApplePictParserInternal::Region>rgn(new ApplePictParserInternal::Region);
    if (!readRegion(*rgn)) return false;
    bitmap.m_region = rgn;
  }
  long actPos=input->tell();
  if (!bitmap.readBitmapData(*input, packed)) return false;
  ascii().skipZone(actPos,input->tell()-1);
  f << bitmap;
  f << getModeName(bitmap.m_mode) << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

bool ApplePictParser::readPixmap(ApplePictParserInternal::Pixmap &pixmap, bool packed, bool colorTable, bool hasRectsMode, bool hasRegion)
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+46))
    return false;
  libmwaw::DebugStream f;
  f << "Pixmap:";

  if (!colorTable) input->seek(4, librevenge::RVNG_SEEK_CUR); // skip the base address

  pixmap.m_rowBytes = (int) input->readULong(2);
  pixmap.m_rowBytes &= 0x3FFF;

  // read the rectangle: bound
  int val[4];
  for (int d = 0; d < 4; d++) val[d] = (int) input->readLong(2);
  pixmap.m_rect = MWAWBox2i(MWAWVec2i(val[1],val[0]), MWAWVec2i(val[3],val[2]));
  if (pixmap.m_rect.size().x() <= 0 || pixmap.m_rect.size().y() <= 0) {
    MWAW_DEBUG_MSG(("ApplePictParser::readPixmap: find odd bound rectangle ... \n"));
    return false;
  }
  pixmap.m_version = (int) input->readLong(2);
  pixmap.m_packType = (int) input->readLong(2);
  pixmap.m_packSize = (int) input->readLong(4);
  for (int c = 0; c < 2; c++) {
    pixmap.m_Res[c] = (int) input->readLong(2);
    input->readLong(2);
  }
  pixmap.m_pixelType = (int) input->readLong(2);
  pixmap.m_pixelSize = (int) input->readLong(2);
  pixmap.m_compCount = (int) input->readLong(2);
  pixmap.m_compSize = (int) input->readLong(2);
  pixmap.m_planeBytes = (int) input->readLong(4);

  // ignored: colorHandle+reserved
  input->seek(8, librevenge::RVNG_SEEK_CUR);

  // the color table
  if (colorTable) {
    pixmap.m_colorTable.reset(new ApplePictParserInternal::ColorTable);
    if (!pixmap.m_colorTable->read(*input)) return false;
  }

  if (!packed && pixmap.m_rowBytes*8 < pixmap.m_rect.size().y()) {
    MWAW_DEBUG_MSG(("ApplePictParser::readPixmap: row bytes seems to short: %d/%d... \n", pixmap.m_rowBytes*8, pixmap.m_rect.size().y()));
    return false;
  }

  // read the two general rectangle src, dst
  if (hasRectsMode) {
    for (int c = 0; c < 2; c++) {
      int dim[4];
      for (int d = 0; d < 4; d++) dim[d] = (int) input->readLong(2);
      MWAWBox2i box(MWAWVec2i(dim[1],dim[0]), MWAWVec2i(dim[3],dim[2]));
      if (box.size().x() <= 0 || box.size().y() <= 0) {
        MWAW_DEBUG_MSG(("ApplePictParser::readPixmap: find odd rectangle %d... \n", c));
        return false;
      }
      else if (c==0) pixmap.m_src = box;
      else pixmap.m_dst = box;
    }
    pixmap.m_mode = (int) input->readLong(2); // mode: I find 0,1 and 3
    f << "mode=" << getModeName(pixmap.m_mode) << ",";
  }

  if (hasRegion) { // CHECKME...
    shared_ptr<ApplePictParserInternal::Region> rgn(new ApplePictParserInternal::Region);
    if (!readRegion(*rgn)) return false;
    pixmap.m_region = rgn;
  }
  long actPos=input->tell();
  if (!pixmap.readPixmapData(*input)) return false;
  ascii().skipZone(actPos,input->tell()-1);
  f << pixmap << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return true;
}

bool ApplePictParser::readRegion(ApplePictParserInternal::Region &region)
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+10))
    return false;
  int dSz=(int) input->readULong(2);
  if (dSz<10||!input->checkPosition(pos+dSz))
    return false;
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
  region.m_bdBox=MWAWBox2i(MWAWVec2i(dim[1],dim[0]),MWAWVec2i(dim[3],dim[2]));
  int remain=(dSz-10)/2;
  // une liste de point dans la box: x1, y1, .. yn 0x7fff, x2, ... 0x7fff
  while (remain > 0) {
    int y = (int)input->readLong(2);
    --remain;
    if (y == 0x7fff) break;
    if (y < region.m_bdBox[0].y() || y > region.m_bdBox[1].y()) {
      MWAW_DEBUG_MSG(("ApplePictParser::readRegion: found eroneous y value: %d\n", y));
      break;
    }
    bool endF = false;
    while (remain > 0) {
      int x = (int)input->readLong(2);
      --remain;
      if (x == 0x7fff) {
        endF = true;
        break;
      }
      if (x < region.m_bdBox[0].x() || x > region.m_bdBox[1].x()) {
        MWAW_DEBUG_MSG(("ApplePictParser::readRegion: found eroneous x value\n"));
        break;
      }
      region.m_points.push_back(MWAWVec2i(x,y));
    }
    if (!endF) {
      MWAW_DEBUG_MSG(("ApplePictParser::readRegion: does not find end of file...\n"));
      break;
    }
  }
  if (remain) {
    MWAW_DEBUG_MSG(("ApplePictParser::readRegion: find some remaining data ...\n"));
    region.m_extra="###,";
  }

  input->seek(pos+dSz, librevenge::RVNG_SEEK_SET);
  return true;
}

//
// helper function
//
std::string ApplePictParser::getModeName(int mode)
{
  switch (mode) {
  case 0:
    return "srcCopy";
  case 1:
    return "srcOr";
  case 2:
    return "srcXOr";
  case 3:
    return "srcBic";
  case 4:
    return "notSrcCopy";
  case 5:
    return "notSrcOr";
  case 6:
    return "notSrcXOr";
  case 7:
    return "notSrcBic";
  case 8:
    return "patCopy";
  case 9:
    return "patOr";
  case 10:
    return "patXOr";
  case 11:
    return "patBic";
  case 12:
    return "notPatCopy";
  case 13:
    return "notPatOr";
  case 14:
    return "notPatXOr";
  case 15:
    return "notPatBic";
  case 23:
    return "postscript";
  case 32:
    return "blend";
  case 33:
    return "addPin";
  case 34:
    return "addOver";
  case 35:
    return "subPin";
  case 36:
    return "transparent";
  case 37:
    return "addMax";
  case 38:
    return "subOver";
  case 39:
    return "addMin";
  case 49:
    return "grayishTextOr";
  case 50:
    return "hilite";
  case 64:
    return "mask";
  default:
    break;
  }
  MWAW_DEBUG_MSG(("ApplePictParser::getModeName: find unknown mode\n"));
  std::stringstream s;
  s << "##mode=" << mode;
  return s.str();
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool ApplePictParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = ApplePictParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->checkPosition(13))
    return false;

  int vers=0;
  for (int st=0; st<2; ++st) {
    if (!input->checkPosition(512*st+13))
      return false;
    long pos=st*512;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    libmwaw::DebugStream f;
    f << "FileHeader:";
    int dSz=(int) input->readULong(2);
    if (dSz)
      f << "dSz=" << dSz << ",";
    int dim[4];
    for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
    if (dim[0]>dim[2]||dim[1]>dim[3])
      continue;
    m_state->m_bdBox=MWAWBox2i(MWAWVec2i(dim[1],dim[0]),MWAWVec2i(dim[3],dim[2]));
    f << "dim=" << m_state->m_bdBox << ",";
    long lastFlag = input->readLong(2);
    switch (lastFlag) {
    case 0x1101:
      if (!input->checkPosition(pos+dSz))
        break;
      f << "pict1,";
      vers = 1;
      break;
    case 0x11: {
      if (!input->checkPosition(pos+40))
        break;
      if (input->readULong(2) != 0x2ff || input->readULong(2) != 0xC00) break;
      int fileVersion = -int(input->readLong(2));
      int subvers = -int(input->readLong(2));
      float dim2[4];
      switch (fileVersion)
      case 1: {
      f << "pict2[1:" << subvers << "],";
      for (int i=0; i<4; ++i) dim2[i]=float(input->readLong(4))/65536.f;
      if (strict && (dim2[0]>dim2[2]||dim2[1]>dim2[3]))
        break;
      m_state->m_bdBox=MWAWBox2f(MWAWVec2f(dim2[0],dim2[1]),MWAWVec2f(dim2[2],dim2[3]));
      f << "dim[fixed]=" << m_state->m_bdBox << ",";
      vers=2;
      break;
      case 2:
        f << "pict2[2:" << subvers << "],";
        for (int i=0; i<2; ++i) dim2[i]=float(input->readLong(4))/65536.f;
        if (strict && (dim2[0]<0 || dim2[1]<=0)) break;
        f << "res=" << MWAWVec2f(dim2[1],dim2[0]) << ",";
        for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
        if (dim[0]>dim[2]||dim[1]>dim[3])
          break;
        f << "dim[optimal]=" << MWAWBox2i(MWAWVec2i(dim[1],dim[0]),MWAWVec2i(dim[3],dim[2])) << ",";
        vers=2;
        break;
      default:
        break;
      }
      if (vers==0 || !input->checkPosition(input->tell()+4)) {
        vers=0;
        break;
      }
      input->seek(4, librevenge::RVNG_SEEK_CUR); // reserved
      break;
    }
    default:
      break;
    }
    if (vers) {
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      break;
    }
    if (st==0) {
      ascii().addPos(0);
      ascii().addNote("Entries(Pref):");
    }
  }
  if (vers==0) return false;
  setVersion(vers);
  m_state->m_version=vers;
  if (header)
    header->reset(MWAWDocument::MWAW_T_APPLEPICT, vers, MWAWDocument::MWAW_K_DRAW);

  return true;
}

////////////////////////////////////////////////////////////
//
// send data
//
////////////////////////////////////////////////////////////
void ApplePictParser::drawLine(MWAWVec2i const &pt)
{
  MWAWVec2f orig=m_state->m_penPosition;
  m_state->m_penPosition=pt;
  MWAWListenerPtr listener=getGraphicListener();
  if (m_state->isInvisible(D_FRAME) || !listener)
    return;
  MWAWGraphicStyle style;
  m_state->updateStyle(D_FRAME, style);
  MWAWGraphicShape shape=MWAWGraphicShape::line(orig, pt);
  MWAWPosition pos;
  m_state->updatePosition(shape.getBdBox(), pos);
  listener->insertPicture(pos,shape, style);
}

void ApplePictParser::drawRectangle(ApplePictParser::DrawingMethod method)
{
  MWAWListenerPtr listener=getGraphicListener();
  if (m_state->isInvisible(method) || !listener)
    return;
  MWAWGraphicStyle style;
  m_state->updateStyle(method, style);
  MWAWBox2f rect=m_state->m_rectangle;
  if (method==D_ERASE) // the rectangle can be very large
    rect=rect.getIntersection(m_state->m_bdBox);
  MWAWGraphicShape shape=MWAWGraphicShape::rectangle(rect);
  MWAWPosition pos;
  m_state->updatePosition(shape.getBdBox(), pos);
  listener->insertPicture(pos,shape, style);
}

void ApplePictParser::drawRoundRectangle(ApplePictParser::DrawingMethod method)
{
  MWAWListenerPtr listener=getGraphicListener();
  if (m_state->isInvisible(method) || !listener)
    return;
  MWAWGraphicStyle style;
  m_state->updateStyle(D_FRAME, style);
  MWAWGraphicShape shape=MWAWGraphicShape::rectangle(m_state->m_roundRectangle, m_state->m_ovalSize);
  MWAWPosition pos;
  m_state->updatePosition(shape.getBdBox(), pos);
  listener->insertPicture(pos,shape, style);
}

void ApplePictParser::drawCircle(ApplePictParser::DrawingMethod method)
{
  MWAWListenerPtr listener=getGraphicListener();
  if (m_state->isInvisible(method) || !listener)
    return;
  MWAWGraphicStyle style;
  m_state->updateStyle(method, style);
  MWAWGraphicShape shape=MWAWGraphicShape::circle(m_state->m_circle);
  MWAWPosition pos;
  m_state->updatePosition(shape.getBdBox(), pos);
  listener->insertPicture(pos,shape, style);
}

void ApplePictParser::drawPie(DrawingMethod method, int startAngle, int dAngle)
{
  MWAWListenerPtr listener=getGraphicListener();
  if (m_state->isInvisible(method) || !listener)
    return;
  MWAWGraphicStyle style;
  m_state->updateStyle(method, style);
  //
  int angle[2] = { 90-startAngle-dAngle, 90-startAngle };
  if (dAngle<0) {
    angle[0]=90-startAngle;
    angle[1]=90-startAngle-dAngle;
  }
  if (angle[1]>360) {
    int numLoop=int(angle[1]/360)-1;
    angle[0]-=numLoop*360;
    angle[1]-=numLoop*360;
    while (angle[1] > 360) {
      angle[0]-=360;
      angle[1]-=360;
    }
  }
  if (angle[0] < -360) {
    int numLoop=int(angle[0]/360)+1;
    angle[0]-=numLoop*360;
    angle[1]-=numLoop*360;
    while (angle[0] < -360) {
      angle[0]+=360;
      angle[1]+=360;
    }
  }

  MWAWVec2f axis = 0.5*MWAWVec2f(m_state->m_pie.size());
  // we must compute the real bd box
  float minVal[2] = { 0, 0 }, maxVal[2] = { 0, 0 };
  int limitAngle[2];
  for (int i = 0; i < 2; i++)
    limitAngle[i] = (angle[i] < 0) ? int(angle[i]/90)-1 : int(angle[i]/90);
  for (int bord = limitAngle[0]; bord <= limitAngle[1]+1; bord++) {
    float ang = (bord == limitAngle[0]) ? float(angle[0]) :
                (bord == limitAngle[1]+1) ? float(angle[1]) : float(90 * bord);
    ang *= float(M_PI/180.);
    float actVal[2] = { axis[0] *std::cos(ang), -axis[1] *std::sin(ang)};
    if (actVal[0] < minVal[0]) minVal[0] = actVal[0];
    else if (actVal[0] > maxVal[0]) maxVal[0] = actVal[0];
    if (actVal[1] < minVal[1]) minVal[1] = actVal[1];
    else if (actVal[1] > maxVal[1]) maxVal[1] = actVal[1];
  }
  MWAWVec2f center = m_state->m_pie.center();
  MWAWBox2f realBox(MWAWVec2f(center[0]+minVal[0],center[1]+minVal[1]),
                    MWAWVec2f(center[0]+maxVal[0],center[1]+maxVal[1]));
  MWAWGraphicShape shape = method==D_FRAME ?
                           MWAWGraphicShape::arc(realBox, m_state->m_pie, MWAWVec2f(float(angle[0]),float(angle[1]))) :
                           MWAWGraphicShape::pie(realBox, m_state->m_pie, MWAWVec2f(float(angle[0]),float(angle[1])));
  MWAWPosition pos;
  m_state->updatePosition(shape.getBdBox(), pos);
  listener->insertPicture(pos,shape, style);
}

void ApplePictParser::drawPolygon(ApplePictParser::DrawingMethod method)
{
  if (m_state->m_points.empty()) {
    MWAW_DEBUG_MSG(("ApplePictParser::drawPolygon: can not find the main polygon\n"));
    return;
  }
  MWAWListenerPtr listener=getGraphicListener();
  if (m_state->isInvisible(method) || !listener)
    return;
  // first compute the bdbox
  MWAWGraphicShape shape;
  shape.m_type=MWAWGraphicShape::Polygon;
  MWAWBox2f box(m_state->m_points[0],m_state->m_points[0]);
  shape.m_vertices.push_back(m_state->m_points[0]);
  for (size_t i=1; i<m_state->m_points.size(); ++i) {
    box=box.getUnion(MWAWBox2f(m_state->m_points[i],m_state->m_points[i]));
    shape.m_vertices.push_back(m_state->m_points[i]);
  }
  shape.m_bdBox=box;
  MWAWGraphicStyle style;
  m_state->updateStyle(method, style);
  MWAWPosition pos;
  m_state->updatePosition(shape.getBdBox(), pos);
  listener->insertPicture(pos,shape, style);
}

void ApplePictParser::drawText(MWAWEntry const &entry)
{
  MWAWListenerPtr listener=getGraphicListener();
  if (!listener || !listener->canWriteText()) {
    MWAW_DEBUG_MSG(("ApplePictParser::drawText: can not find the listener\n"));
    return;
  }
  MWAWGraphicStyle style;
  m_state->updateStyle(D_TEXT, style);
  MWAWFont font(m_state->m_font);
  font.setColor(style.m_lineColor);
  listener->setFont(font);
  if (!entry.valid())
    return;
  MWAWInputStreamPtr input = getInput();
  long actPos=input->tell();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  long endPos=entry.end();
  while (!input->isEnd() && input->tell()<endPos) {
    char c = (char) input->readULong(1);
    if (c==0) {
      MWAW_DEBUG_MSG(("ApplePictParser::drawText: find char 0\n"));
      continue;
    }
    switch (c) {
    case 9:
      listener->insertTab();
      break;
    case 0xd:
      listener->insertEOL();
      break;
    default:
      listener->insertCharacter((unsigned char)c, input, entry.end());
      break;
    }
  }
  input->seek(actPos, librevenge::RVNG_SEEK_SET);
}

void ApplePictParser::drawBitmap(ApplePictParserInternal::Bitmap const &bitmap)
{
  MWAWListenerPtr listener=getGraphicListener();
  if (!listener)
    return;
  librevenge::RVNGBinaryData data;
  std::string type;
  if (!bitmap.get(data, type))
    return;
  MWAWGraphicStyle style;
  MWAWPosition pos;
  m_state->updatePosition(bitmap.m_rect, pos);
  listener->insertPicture(pos,data,type);
}

void ApplePictParser::drawPixmap(ApplePictParserInternal::Pixmap const &pixmap)
{
  MWAWListenerPtr listener=getGraphicListener();
  if (!listener)
    return;
  librevenge::RVNGBinaryData data;
  std::string type;
  if (!pixmap.get(data, type))
    return;
  MWAWGraphicStyle style;
  MWAWPosition pos;
  m_state->updatePosition(pixmap.m_dst, pos);
  listener->insertPicture(pos,data,type);
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
