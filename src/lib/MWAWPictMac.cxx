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

/* This header contains code specific to a pict mac file
 */
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWPictMac.hxx"
#include "MWAWPictBitmap.hxx"

// to parse the picture
#define DEBUG_PICT 0
// and then to save the read bitmap/pixmap
#define DEBUG_BITMAP 0

MWAWPictMac::ReadResult MWAWPictMac::checkOrGet
(MWAWInputStreamPtr input, int size, Box2f &box, MWAWPictData **result)
{
  if (result) *result=0L;

  int version, subvers;
  // we can not read the data, ...
  long actualPos = input->tell();
  input->seek(actualPos,librevenge::RVNG_SEEK_SET);
  if (size < 0xd)
    return MWAW_R_BAD;

  int readSize = int(input->readULong(2));
  long dim[4];
  for (int i = 0; i < 4; i++) dim[i] = input->readLong(2);
  long lastFlag = input->readLong(2);
  bool empty = false;
  switch (lastFlag) {
  case 0x1101: {
    if (readSize != size && readSize+1 != size)
      return MWAW_R_BAD;
    version = subvers = 1;
    empty = (size == 0xd);
    break;
  }
  case 0x0011: {
    if (size < 40) return  MWAW_R_BAD;
    if (input->readULong(2) != 0x2ff) return  MWAW_R_BAD;
    if (input->readULong(2) != 0xC00) return  MWAW_R_BAD;
    subvers = -int(input->readLong(2));
    if (subvers == 1) empty = (size == 42);
    else if (subvers == 2) empty = (size == 40);
    else if (subvers >= -6 && subvers < 6) {
      // find also 0 and -1 and -4 here...
      MWAW_DEBUG_MSG(("MWAWPictMac::checkOrGet: unknown subversion: %d\n", subvers));
      empty = (size == 0xd);
    }
    else return MWAW_R_BAD;
    version = 2;
    break;
  }
  default:
    return MWAW_R_BAD;
  }

  if (empty) {
    input->seek(actualPos+size-1,librevenge::RVNG_SEEK_SET);
    if (input->readULong(1) != 0xff) return MWAW_R_BAD;
  }

  box.set(Vec2f(float(dim[1]),float(dim[0])), Vec2f(float(dim[3]),float(dim[2])));
  if (!empty && (box.size().x() < 0 || box.size().y() < 0)) return MWAW_R_BAD;
  if (box.size().x() <= 0 || box.size().y() <= 0) empty = true;

  if (empty) return MWAW_R_OK_EMPTY;
  if (!result) return MWAW_R_OK;

  MWAWPictMac *res = new MWAWPictMac(box);
  res->m_version = version;
  res->m_subVersion = subvers;
  *result = res;

  // let caller read the data
  return MWAW_R_OK;
}
/** Internal and low level: generic tools about Mac Pict1.0 picture
 *
 * This regroups some functions to parse them and to convert them in Pict2.0 picture
 */
namespace libmwaw_applepict1
{
/** Internal and low level: the different types of arguments.
 *
 * By default, data are signed, excepted if we add U to indicate that they are unsigned,
 * - WP_PATTERN: 8x8bits which defined a 8x8 picture (black or white)
 * - WP_COLOR: 3 bits which defined r,g,b (checkme)
 * - for BITMAP, R indicates Region bitmap while P indicates Packed bitmap
 *
 * Pict2 type
 * - WP_CCOLOR, WP_CPATTERN, WP_CBITMAP, WP_CRBITMAP, WP_QUICKTIME
*/
enum DataType {
  WP_NONE, WP_BYTE, WP_UBYTE, WP_INT, WP_UINT, WP_UFIXED,
  WP_COLOR, WP_PATTERN, WP_POINT, WP_POINTBYTE, WP_POINTUBYTE, WP_POLY, WP_RECT, WP_REGION, WP_TEXT, WP_LTEXT,
  WP_BITMAP, WP_RBITMAP, WP_PBITMAP, WP_RPBITMAP, WP_UNKNOWN,
  // color2 type
  WP_CCOLOR, WP_CPATTERN, WP_CBITMAP, WP_CRBITMAP, WP_QUICKTIME
};

/** Internal and low level: class used to read/store a picture region
 *
 * A region is formed by bounding box followed by an array of bits
 * which indicate which defines a mask */
class Region
{
public:
  Region() : m_box(), m_points() {}
  //! operator << for a Region
  friend std::ostream &operator<< (std::ostream &o, Region const &f)
  {
    o << "reg=" << f.m_box;
    if (f.m_points.size()==0) return o;
    o << ", [";
    for (size_t c = 0; c < f.m_points.size(); c++) {
      if (c) o << ",";
      o << f.m_points[c];
    }
    o << "]";
    return o;
  }
  //! tries to read the data
  bool read(MWAWInputStream &input)
  {
    long actualPos = input.tell();

    // the region size
    int sz = (int)input.readULong(2);
    if ((sz%2) != 0) {
      MWAW_DEBUG_MSG(("Pict1:Region: read odd size: %d\n", sz));
      return false;
    }
    sz /= 2;
    if (sz < 5) {
      MWAW_DEBUG_MSG(("Pict1:Region: read size is too short: %d\n", sz*2));
      return false;
    }
    int val[4];
    for (int i = 0; i < 4; i++) val[i] = (int) input.readLong(2);
    m_box.set(Vec2i(val[0], val[1]), Vec2i(val[2], val[3]));
    sz -= 5;
    m_points.resize(0);
    if (sz == 0) return true;
    if (actualPos+10 != long(input.tell())) {
      MWAW_DEBUG_MSG(("Pict1:Region: rect EOF\n"));
      return false;
    }
    // une liste de point dans la box: x1, y1, .. yn 0x7fff, x2, ... 0x7fff
    input.seek(actualPos+10+2*sz, librevenge::RVNG_SEEK_SET);
    if (actualPos+10+2*sz != long(input.tell())) {
      MWAW_DEBUG_MSG(("Pict1:Region: pixels EOF\n"));
      return false;
    }
    input.seek(actualPos+10, librevenge::RVNG_SEEK_SET);
    while (sz > 0) {
      int x = (int)input.readLong(2);
      sz--;
      if (x == 0x7fff) break;
      if (x < m_box[0].x() || x > m_box[1].x()) {
        MWAW_DEBUG_MSG(("Pict1:Region: found eroneous x value: %d\n", x));
        return false;
      }
      bool endF = false;
      while (sz > 0) {
        int y = (int)input.readLong(2);
        sz--;
        if (y == 0x7fff) {
          endF = true;
          break;
        }
        if (y < m_box[0].y() || y > m_box[1].y()) {
          MWAW_DEBUG_MSG(("Pict1:Region: found eroneous y value\n"));
          return false;
        }
        m_points.push_back(Vec2i(x,y));
      }
      if (!endF) {
        MWAW_DEBUG_MSG(("Pict1:Region: does not find end of file...\n"));
        return false;
      }
    }
    if (sz) {
      MWAW_DEBUG_MSG(("Pict1:Region: find some remaining data ...\n"));
      return false;
    }
    return true;
  }


protected:
  //! the bounding box
  Box2i m_box;
  //! the set of points which defines the mask
  std::vector<Vec2i> m_points;
};

//!  Internal and low level: a class used to read pack/unpack black-white bitmap
struct Bitmap {
  Bitmap() : m_rowBytes(), m_rect(), m_src(), m_dst(), m_region(),
    m_bitmap(), m_mode(0) {}
  //! tries to read a bitmap
  bool read(MWAWInputStream &input, bool packed, bool hasRegion)
  {
    m_rowBytes = (int) input.readULong(2);
    m_rowBytes &= 0x3FFF;
    if (m_rowBytes < 0 || (!packed && m_rowBytes > 8)) {
      MWAW_DEBUG_MSG(("Pict1:Bitmap: find odd rowBytes %d... \n", m_rowBytes));
      return false;
    }
    // read the rectangle: bound
    // ------ end of bitmap ----------
    // and the two general rectangle src, dst
    for (int c = 0; c < 3; c++) {
      int val[4];
      for (int d = 0; d < 4; d++) val[d] = (int) input.readLong(2);
      Box2i box(Vec2i(val[1],val[0]), Vec2i(val[3],val[2]));
      if (box.size().x() <= 0 || box.size().y() <= 0) {
        MWAW_DEBUG_MSG(("Pict1:Bitmap: find odd rectangle %d... \n", c));
        return false;
      }
      if (c == 0) m_rect=box;
      else if (c==1) m_src = box;
      else m_dst = box;
    }

    if (!packed && m_rowBytes*8 < m_rect.size().x()) {
      MWAW_DEBUG_MSG(("Pict1:Bitmap: row bytes seems to short: %d/%d... \n", m_rowBytes*8, m_rect.size().y()));
      return false;
    }
    m_mode = (int) input.readLong(2); // mode: I find 0,1 and 3
    if (m_mode < 0 || m_mode > 64) {
      MWAW_DEBUG_MSG(("Pict1:Bitmap: unknown mode: %d \n", m_mode));
      return false;
    }

    if (hasRegion) { // CHECKME...
      shared_ptr<Region>rgn(new Region);
      if (!rgn->read(input)) return false;
      m_region = rgn;
    }
    if (!readBitmapData(input, packed)) return false;

    if (input.isEnd()) {
      MWAW_DEBUG_MSG(("Pict1:Bitmap: EOF \n"));
      return false;
    }
    return true;
  }

  //! operator<< for Bitmap
  friend std::ostream &operator<< (std::ostream &o, Bitmap const &f)
  {
    o << "rDim=" << f.m_rowBytes << ", " << f.m_rect << ", " << f.m_src << ", " << f.m_dst;
    if (f.m_region.get()) o << ", " << *f.m_region;
    static char const *(mode0[]) = { // 0-15
      "srcCopy", "srcOr", "srcXOr", "srcBic",
      "notSrcCopy", "notSrcOr", "notSrcXOr", "notSrcBic",
      "patCopy", "patOr", "patXOr", "patBic",
      "notPatCopy", "notPatOr", "notPatXOr", "notPatBic"
    };
    static char const *(mode1[]) = { // 32-39
      "blend", "addPin", "addOver", "subPin",
      "transparent", "addMax", "subOver", "addMin"
    };

    if (f.m_mode >= 0 && f.m_mode < 16)
      o << ", " << mode0[f.m_mode] << ", [...]";
    else if (f.m_mode >= 32 && f.m_mode < 40)
      o << ", " << mode1[f.m_mode-32] << ", [...]";
    else if (f.m_mode == 49) o << ", grayishTextOr, [...]";
    else if (f.m_mode == 50) o << ", hilitetransfermode, [...]";
    else if (f.m_mode == 64) o << ", ditherCopy, [...]";
    else
      o << ", ###mod=" << f.m_mode << ", [...]";
#if DEBUG_BITMAP
    f.saveBitmap();
#endif
    return o;
  }

  //! saves the bitmap in file (debugging function)
  bool saveBitmap() const
  {
    if (m_rowBytes <= 0) return false;
    int nRows = int(m_bitmap.size())/m_rowBytes;
    MWAWPictBitmapBW bitmap(Vec2i(m_rect.size().x(),nRows));
    if (!bitmap.valid()) return false;

    for (int i = 0; i < nRows; i++)
      bitmap.setRowPacked(i, &m_bitmap[size_t(i*m_rowBytes)]);

    librevenge::RVNGBinaryData dt;
    std::string type;
    if (!bitmap.getBinary(dt, type)) return false;

    static int ppmNumber = 0;
    std::stringstream f;
    f << "PictBitmap" << ppmNumber++ << ".pbm";
    return libmwaw::Debug::dumpFile(dt, f.str().c_str());
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
          MWAW_DEBUG_MSG(("Pict1:Bitmap: can not read line %d/%d (%d chars)\n", i, numRows, m_rowBytes));
          return false;
        }
        for (int j = 0; j < m_rowBytes; j++)
          m_bitmap[pos++]=data[j];
      }
      else {
        int numB = (int) input.readULong(szRowSize);
        if (numB < 0 || numB > 2*m_rowBytes) {
          MWAW_DEBUG_MSG(("Pict1:Bitmap: odd numB:%d in row: %d/%d\n", numB, i, numRows));
          return false;
        }
        unsigned long numR = 0;
        unsigned char const *data = input.read(size_t(numB), numR);
        if (!data || int(numR) != numB) {
          MWAW_DEBUG_MSG(("Pict1:Bitmap: can not read line %d/%d (%d chars)\n", i, numRows, numB));
          return false;
        }
        if (!unpackedData(data,numB)) {
          MWAW_DEBUG_MSG(("Pict1:Bitmap: can not unpacked line:%d\n", i));
          return false;
        }
      }
    }
    return true;
  }
  //! the num of bytes used to store a row
  int m_rowBytes;
  Box2i m_rect /** the bitmap rectangle */, m_src/** the initial dimension */, /** another final dimension */ m_dst  ;
  //! the region
  shared_ptr<Region> m_region;
  //! the bitmap
  std::vector<unsigned char> m_bitmap;
  //! the encoding mode ?
  int m_mode;
};

//
// Code for version 2, ColorTable, Pixmap, PixPatttern
//
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
    if (n < 0) return false;
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
  //! tries to read a pixmap
  bool read(MWAWInputStream &input, bool packed, bool colorTable, bool hasRectsMode, bool hasRegion)
  {
    if (!colorTable) input.readULong(4); // skip the base address

    m_rowBytes = (int) input.readULong(2);
    m_rowBytes &= 0x3FFF;

    // read the rectangle: bound
    int val[4];
    for (int d = 0; d < 4; d++) val[d] = (int) input.readLong(2);
    m_rect = Box2i(Vec2i(val[1],val[0]), Vec2i(val[3],val[2]));
    if (m_rect.size().x() <= 0 || m_rect.size().y() <= 0) {
      MWAW_DEBUG_MSG(("Pict1:Pixmap: find odd bound rectangle ... \n"));
      return false;
    }
    m_version = (int) input.readLong(2);
    m_packType = (int) input.readLong(2);
    m_packSize = (int) input.readLong(4);
    for (int c = 0; c < 2; c++) {
      m_Res[c] = (int) input.readLong(2);
      input.readLong(2);
    }
    m_pixelType = (int) input.readLong(2);
    m_pixelSize = (int) input.readLong(2);
    m_compCount = (int) input.readLong(2);
    m_compSize = (int) input.readLong(2);
    m_planeBytes = (int) input.readLong(4);

    // ignored: colorHandle+reserved
    input.seek(8, librevenge::RVNG_SEEK_CUR);

    // the color table
    if (colorTable) {
      m_colorTable.reset(new ColorTable);
      if (!m_colorTable->read(input)) return false;
    }

    if (!packed && m_rowBytes*8 < m_rect.size().y()) {
      MWAW_DEBUG_MSG(("Pict1:Pixmap: row bytes seems to short: %d/%d... \n", m_rowBytes*8, m_rect.size().y()));
      return false;
    }

    // read the two general rectangle src, dst
    if (hasRectsMode) {
      for (int c = 0; c < 2; c++) {
        int dim[4];
        for (int d = 0; d < 4; d++) dim[d] = (int) input.readLong(2);
        Box2i box(Vec2i(dim[1],dim[0]), Vec2i(dim[3],dim[2]));
        if (box.size().x() <= 0 || box.size().y() <= 0) {
          MWAW_DEBUG_MSG(("Pict1:Bitmap: find odd rectangle %d... \n", c));
          return false;
        }
        else if (c==0) m_src = box;
        else m_dst = box;
      }
      m_mode = (int) input.readLong(2); // mode: I find 0,1 and 3
      if (m_mode < 0 || m_mode > 64) {
        MWAW_DEBUG_MSG(("Pict1:Pixmap: unknown mode: %d \n", m_mode));
        return false;
      }
    }

    if (hasRegion) { // CHECKME...
      shared_ptr<Region> rgn(new Region);
      if (!rgn->read(input)) return false;
      m_region = rgn;
    }
    if (!readPixmapData(input)) return false;

    if (input.isEnd()) {
      MWAW_DEBUG_MSG(("Pict1:Pixmap: EOF \n"));
      return false;
    }
    return true;
  }

  //! operator<< for Pixmap
  friend std::ostream &operator<< (std::ostream &o, Pixmap const &f)
  {
    o << "rDim=" << f.m_rowBytes << ", " << f.m_rect << ", " << f.m_src << ", " << f.m_dst;
    o << ", resol=" << f.m_Res[0] << "x" << f.m_Res[1];
    if (f.m_colorTable.get()) o << ", " << *f.m_colorTable;
    if (f.m_region.get()) o << ", " << *f.m_region;
    static char const *(mode0[]) = {
      "srcCopy", "srcOr", "srcXOr", "srcBic",
      "notSrcCopy", "notSrcOr", "notSrcXOr", "notSrcBic",
      "patCopy", "patOr", "patXOr", "patBic",
      "notPatCopy", "notPatOr", "notPatXOr", "notPatBic"
    };
    static char const *(mode1[]) = { // 32-39
      "blend", "addPin", "addOver", "subPin",
      "transparent", "addMax", "subOver", "addMin"
    };

    if (f.m_mode >= 0 && f.m_mode < 16)
      o << ", " << mode0[f.m_mode] << ", [...]";
    else if (f.m_mode >= 32 && f.m_mode < 40)
      o << ", " << mode1[f.m_mode-32] << ", [...]";
    else if (f.m_mode == 49) o << ", grayishTextOr, [...]";
    else if (f.m_mode == 50) o << ", hilitetransfermode, [...]";
    else if (f.m_mode == 64) o << ", ditherCopy, [...]";
    else
      o << ", ###mod=" << f.m_mode << ", [...]";
#if DEBUG_BITMAP
    f.savePixmap();
#endif
    return o;
  }

  //! saves the pixmap in file (debugging function)
  bool savePixmap() const
  {
    int W = m_rect.size().x();
    if (W <= 0) return false;
    if (m_colorTable.get() && m_indices.size()) {
      int nRows = int(m_indices.size())/W;
      MWAWPictBitmapIndexed pixmap(Vec2i(W,nRows));
      if (!pixmap.valid()) return false;

      pixmap.setColors(m_colorTable->m_colors);

      size_t rPos = 0;
      for (int i = 0; i < nRows; i++) {
        for (int x = 0; x < W; x++)
          pixmap.set(x, i, m_indices[rPos++]);
      }

      librevenge::RVNGBinaryData dt;
      std::string type;
      if (!pixmap.getBinary(dt, type)) return false;

      static int ppmNumber = 0;
      std::stringstream f;
      f << "PictPixmap" << ppmNumber++ << ".ppm";
      return libmwaw::Debug::dumpFile(dt, f.str().c_str());
    }
    else if (m_colors.size()) {
      int nRows = int(m_colors.size())/W;
      MWAWPictBitmapColor pixmap(Vec2i(W,nRows));
      if (!pixmap.valid()) return false;

      size_t rPos = 0;
      for (int i = 0; i < nRows; i++) {
        for (int x = 0; x < W; x++)
          pixmap.set(x, i, m_colors[rPos++]);
      }

      librevenge::RVNGBinaryData dt;
      std::string type;
      if (!pixmap.getBinary(dt, type)) return false;

      static int ppmNumber = 0;
      std::stringstream f;
      f << "PictDirect" << ppmNumber++ << ".ppm";
      return libmwaw::Debug::dumpFile(dt, f.str().c_str());

    }
    else {
      MWAW_DEBUG_MSG(("Pict1:Pixmap: can not find any indices or colors \n"));
      return false;
    }

    return true;
  }

  //! creates the pixmap from the packdata
  bool unpackedData(unsigned char const *pData, int sz, int byteSz, int nSize, std::vector<unsigned char> &res) const
  {
    assert(byteSz >= 1 && byteSz <= 4);
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
    return wPos >= nSize;
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
        MWAW_DEBUG_MSG(("Pict1:Pixmap: readPixmapData invalid number of rowsize : %d, pixelSize=%d, W=%d\n", m_rowBytes, m_pixelSize, W));
        return false;
      }
      if (numColors == 0) {
        MWAW_DEBUG_MSG(("Pict1:Pixmap: readPixmapData no color table \n"));
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
        MWAW_DEBUG_MSG(("Pict1:Pixmap: do not known how to read cmpCount=%d\n", m_compCount));
        return false;
      }
      nPlanes=m_compCount;
      nBytes=1;
      if (nPlanes == 3) rowBytes = (3*rowBytes)/4;
      break;
    default:
      MWAW_DEBUG_MSG(("Pict1:Pixmap: do not known how to read pixelsize=%d \n", m_pixelSize));
      return false;
    }
    if (m_pixelSize <= 8)
      m_indices.resize(size_t(H*W));
    else {
      if (rowBytes != W * nBytes * nPlanes) {
        MWAW_DEBUG_MSG(("Pict1:Pixmap: find W=%d pixelsize=%d, rowSize=%d\n", W, m_pixelSize, m_rowBytes));
      }
      m_colors.resize(size_t(H*W));
    }

    std::vector<unsigned char> values;
    values.resize(size_t(m_rowBytes+24));

    for (int y = 0; y < H; y++) {
      if (!packed) {
        unsigned long numR = 0;
        unsigned char const *data = input.read(size_t(m_rowBytes), numR);
        if (!data || int(numR) != m_rowBytes) {
          MWAW_DEBUG_MSG(("Pict1:Pixmap: readColors can not read line %d/%d (%d chars)\n", y, H, m_rowBytes));
          return false;
        }
        for (size_t j = 0; j < size_t(m_rowBytes); j++)
          values[j]=data[j];
      }
      else {   // ok, packed
        int numB = (int) input.readULong(szRowSize);
        if (numB < 0 || numB > 2*m_rowBytes) {
          MWAW_DEBUG_MSG(("Pict1:Pixmap: odd numB:%d in row: %d/%d\n", numB, y, H));
          return false;
        }
        unsigned long numR = 0;
        unsigned char const *data = input.read(size_t(numB), numR);
        if (!data || int(numR) != numB) {
          MWAW_DEBUG_MSG(("Pict1:Pixmap: can not read line %d/%d (%d chars)\n", y, H, numB));
          return false;
        }
        if (!unpackedData(data,numB, nBytes, rowBytes, values)) {
          MWAW_DEBUG_MSG(("Pict1:Pixmap: can not unpacked line:%d\n", y));
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
      MWAW_DEBUG_MSG(("Pict1:Pixmap: find index=%d >= numColors=%d\n", maxColorsIndex, numColors));

      return true;
    }
    return true;
  }

  //! the num of bytes used to store a row
  int m_rowBytes;
  Box2i m_rect /** the pixmap rectangle */;
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

  Box2i m_src/** the initial dimension */, /** another final dimension */ m_dst  ;
  //! the region
  shared_ptr<Region> m_region;
  //! the pixmap indices
  std::vector<int> m_indices;
  //! the colors
  std::vector<MWAWColor> m_colors;
  //! the encoding mode ?
  int m_mode;
};


//! Internal and low level: a class used to read pack/unpack color pixmap (version 2)
struct Pixpattern {
  Pixpattern() : m_color(), m_pixmap()
  {
    std::memset(m_pat, 0, sizeof(m_pat));
  }
  //! tries to read a pixpat
  bool read(MWAWInputStream &input)
  {
    int type = (int)input.readULong(2);
    if (type !=1 && type != 2) {
      MWAW_DEBUG_MSG(("PixPat:Read: unknown type=%d... \n", type));
      return false;
    }
    for (int i = 0; i < 8; i++) m_pat[i] = (int)input.readULong(1);

    if (type == 2) {
      int val[3]; // checkme
      for (int i = 0; i < 3; i++) val[i] = (int)input.readULong(2);
      m_color = MWAWColor((unsigned char)val[0], (unsigned char)val[1], (unsigned char)val[2]);
      return true;
    }

    m_pixmap.reset(new Pixmap);
    return m_pixmap->read(input, false, true, false, false);
  }


  //! operator<< for Pixmap
  friend std::ostream &operator<< (std::ostream &o, Pixpattern const &f)
  {
    o << "pat=(" << std::hex;
    for (int c= 0; c < 8; c++) {
      if (c) o << ",";
      o << f.m_pat[c];
    }
    o << ")," << std::dec;
    if (!f.m_pixmap.get()) {
      o << "col=" << f.m_color << ",";
      return o;
    }
    o << *f.m_pixmap;
    return o;
  }

  //! the color
  MWAWColor m_color;
  //! the pattern
  int m_pat[8];
  //! the pixmap (if this is not an rgb color)
  shared_ptr<Pixmap> m_pixmap;
};

//! Internal and low level: a class used to read and store all possible value
struct Value {
  Value() : m_type(), m_int(0), m_rgb(MWAWColor::white()), m_text(""), m_point(), m_box(), m_listPoint(),
    m_region(), m_bitmap(), m_pixmap(), m_pixpattern()
  {
    std::memset(m_pat, 0, sizeof(m_pat));
  }
  virtual ~Value() {}

  /** the stored type of the data
   *
   * This can only be WP_INT, WP_COLOR, WP_PATTERN, WP_POINT, WP_POLY, WP_RECT, WP_REGION, WP_TEXT, WP_BITMAP */
  DataType m_type;

  //! operator<< for Value
  friend std::ostream &operator<< (std::ostream &o, Value const &f)
  {
    switch (f.m_type) {
    case WP_INT:
      o << f.m_int;
      break;
    case WP_COLOR:
    case WP_CCOLOR:
      o << "col=(" << f.m_rgb << ")";
      break;
    case WP_PATTERN:
      o << "pat=(" << std::hex;
      for (int c= 0; c < 8; c++) {
        if (c) o << ",";
        o << f.m_pat[c];
      }
      o << ")" << std::dec;
      break;
    case WP_POINT:
      o << f.m_point;
      break;
    case WP_RECT:
      o << f.m_box;
      break;
    case WP_REGION:
      if (f.m_region.get()) {
        o << *f.m_region;
        break;
      }
      MWAW_DEBUG_MSG(("Pict1:Value: I do not find my region... \n"));
      break;
    case WP_POLY:
      o << "[reg=" << f.m_box << ":";
      for (size_t c = 0; c < f.m_listPoint.size(); c++) {
        if (c) o << ",";
        o << f.m_listPoint[c];
      }
      o << "]";
      break;
    case WP_TEXT:
      o << "\"" << f.m_text << "\"";
      break;
    case WP_BITMAP:
      if (f.m_bitmap.get()) {
        o << *f.m_bitmap;
        break;
      }
      MWAW_DEBUG_MSG(("Pict1:Value: I do not find my bitmap... \n"));
      break;
    case WP_CBITMAP:
      if (f.m_pixmap.get()) {
        o << *f.m_pixmap;
        break;
      }
      MWAW_DEBUG_MSG(("Pict1:Value: I do not find my pixmap... \n"));
      break;
    case WP_CPATTERN:
      if (f.m_pixpattern.get()) {
        o << *f.m_pixpattern;
        break;
      }
      MWAW_DEBUG_MSG(("Pict1:Value: I do not find my pixpat... \n"));
      break;
    case WP_QUICKTIME:
      break;
    case WP_NONE:
    case WP_BYTE:
    case WP_UBYTE:
    case WP_UINT:
    case WP_UFIXED:
    case WP_POINTBYTE:
    case WP_POINTUBYTE:
    case WP_LTEXT:
    case WP_RBITMAP:
    case WP_PBITMAP:
    case WP_RPBITMAP:
    case WP_UNKNOWN:
    case WP_CRBITMAP:
    default:
      MWAW_DEBUG_MSG(("Pict1:Value: does not know how to print my values... \n"));
    }
    return o;
  }

  //! the int value when type=WP_INT
  int m_int;
  //! the color when type=WP_COLOR or WP_CCOLOR
  MWAWColor m_rgb;
  //! the pattern when type=WP_PATTERN
  int m_pat[8];
  //! the text when type=WP_TEXT
  std::string m_text;
  //! the point when type=WP_POINT
  Vec2i m_point;
  //! the rectangle when type=WP_RECT
  Box2i m_box;
  //! the list of points which defined the polygon when type=WP_POLY
  std::vector<Vec2i> m_listPoint;
  //! the region when type=WP_REGION
  shared_ptr<Region> m_region;
  //! the bitmap when type=WP_BITMAP
  shared_ptr<Bitmap> m_bitmap;
  //! the pixmap when type=WP_CBITMAP
  shared_ptr<Pixmap> m_pixmap;
  //! the pixpat when type=WP_CPATTERN
  shared_ptr<Pixpattern> m_pixpattern;
};

//! Internal and low level: a class to define each opcode and their arguments and read their data
struct OpCode {
  /** constructor
   *
   * \param id is the code of the opcode in the file
   * \param nm is the short name of the opcode
   * \param type1 \param type2 \param type3 \param type4 \param type5 the type of the first, second, third arguments (if they exist)
   */
  OpCode(int id, char const *nm, DataType type1=WP_NONE, DataType type2=WP_NONE, DataType type3=WP_NONE, DataType type4=WP_NONE, DataType type5=WP_NONE)
    : m_id(id), m_name(nm), m_types()
  {
    if (type1==WP_NONE) return;
    else m_types.push_back(type1);
    if (type2==WP_NONE) return;
    else m_types.push_back(type2);
    if (type3==WP_NONE) return;
    else m_types.push_back(type3);
    if (type4==WP_NONE) return;
    else m_types.push_back(type4);
    if (type5==WP_NONE) return;
    else m_types.push_back(type5);
  }
  virtual ~OpCode() {}

  /** tries to read the data in the file
   *
   * If the read is succefull, fills listValue with the read argument */
  bool readData(MWAWInputStream &input, std::vector<Value> &listValue) const
  {
    size_t numTypes = m_types.size();
    listValue.resize(numTypes);
    Value newVal;
    for (size_t i = 0; i < numTypes; i++) {
      long actualPos = input.tell();
      if (readValue(input, m_types[i], newVal)) {
        listValue[i] = newVal;
        continue;
      }
      input.seek(actualPos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    return true;
  }

  //! computes the size of the data
  bool computeSize(MWAWInputStream &input, int &sz) const
  {
    long actPos = input.tell();
    sz = 0;

    size_t numTypes = m_types.size();
    for (size_t i = 0; i < numTypes; i++) {
      input.seek(actPos+sz, librevenge::RVNG_SEEK_SET);
      int newSz = getSize(input, m_types[i]);
      if (newSz < 0) return false;
      sz += newSz;
    }
    input.seek(actPos, librevenge::RVNG_SEEK_SET);
    return true;
  }

  /** read a rectangles field

  \note can be used to read the first dimensions of a picture */
  static bool readRect(MWAWInputStream &input, DataType type, Box2i &res)
  {
    Vec2i v[2];
    DataType valType;
    switch (type) {
    case WP_RECT:
      valType = WP_POINT;
      break;
    case WP_NONE:
    case WP_BYTE:
    case WP_UBYTE:
    case WP_INT:
    case WP_UINT:
    case WP_UFIXED:
    case WP_COLOR:
    case WP_PATTERN:
    case WP_POINT:
    case WP_POINTBYTE:
    case WP_POINTUBYTE:
    case WP_POLY:
    case WP_REGION:
    case WP_TEXT:
    case WP_LTEXT:
    case WP_BITMAP:
    case WP_RBITMAP:
    case WP_PBITMAP:
    case WP_RPBITMAP:
    case WP_UNKNOWN:
    case WP_CCOLOR:
    case WP_CPATTERN:
    case WP_CBITMAP:
    case WP_CRBITMAP:
    case WP_QUICKTIME:
    default:
      MWAW_DEBUG_MSG(("Pict1:OpCode: readRect is called with %d\n", type));
      return false;
    };
    for (int p = 0; p < 2; p++) {
      if (!readPoint(input, valType, v[p])) return false;
    }
    res.set(v[0], v[1]);
    return true;
  }
  //! the opCode
  int m_id;
  //! the opCode name
  std::string m_name;
  //! the different argument types
  std::vector<DataType> m_types;

protected:
  /** returns the size of the next argument of type \a type.
   *
   * \note This function can update the next reading position in the input, if it uses the input to compute the size of this argument */
  static int getSize(MWAWInputStream &input, DataType type)
  {
    switch (type) {
    case WP_BYTE:
    case WP_UBYTE:
      return 1;
    case WP_INT:
    case WP_UINT:
    case WP_POINTBYTE:
    case WP_POINTUBYTE:
      return 2;
    case WP_UFIXED:
    case WP_COLOR:
    case WP_POINT:
      return 4;
    case WP_CCOLOR:
      return 6;
    case WP_PATTERN:
    case WP_RECT:
      return 8;
    case WP_CPATTERN: {
      // can not guess, so we read the bitmap...
      long actPos = input.tell();
      shared_ptr<Pixpattern> pattern(new Pixpattern);
      if (!pattern->read(input)) return -1;
      return int(input.tell()-actPos);
    }
    case WP_POLY:
    case WP_REGION:
      return (int)input.readULong(2);
    case WP_TEXT:
      return 1+(int)input.readULong(1);
    case WP_LTEXT:
      return 2+(int)input.readULong(2);
    case WP_BITMAP:
    case WP_RBITMAP:
    case WP_PBITMAP:
    case WP_RPBITMAP: {
      // can not guess, so we read the bitmap...
      long actPos = input.tell();
      // first check if it is a bitmap or a pixmap
      bool pixmap = input.readULong(2) & 0x8000;
      input.seek(-2, librevenge::RVNG_SEEK_CUR);
      bool packed = type==WP_PBITMAP || type == WP_RPBITMAP;
      bool hasRgn = type ==WP_RBITMAP || type == WP_RPBITMAP;
      if (pixmap) {
        shared_ptr<Pixmap> pxmap(new Pixmap);
        if (!pxmap->read(input, packed, true, true, hasRgn)) return -1;
      }
      else {
        shared_ptr<Bitmap> btmap(new Bitmap);
        if (!btmap->read(input, packed, hasRgn)) return -1;
      }
      return int(input.tell()-actPos);
    }
    case WP_CBITMAP:
    case WP_CRBITMAP: {
      // can not guess, so we read the pixmap...
      long actPos = input.tell();
      bool hasRgn = type ==WP_CRBITMAP;
      shared_ptr<Pixmap> pxmap(new Pixmap);
      if (!pxmap->read(input, false, false, true, hasRgn)) return -1;
      return int(input.tell()-actPos);
    }
    case WP_QUICKTIME:
      return 4+(int)input.readULong(4);
    case WP_UNKNOWN:
    case WP_NONE:
    default:
      return -1;
    }
    return -1;
  }
  //! reads a argument of type \a type, if successfull updates \a val.
  static bool readValue(MWAWInputStream &input, DataType type, Value &val)
  {
    switch (type) {
    case WP_BYTE:
    case WP_UBYTE:
    case WP_INT:
    case WP_UINT:
    case WP_UFIXED:
      val.m_type = WP_INT;
      return readInt(input, type, val.m_int);
    case WP_COLOR:
      val.m_type = WP_COLOR;
      return readColor(input, type, val.m_rgb);
    case WP_PATTERN:
      val.m_type = WP_PATTERN;
      return readPattern(input, type, val.m_pat);
    case WP_CPATTERN: {
      shared_ptr<Pixpattern> pattern(new Pixpattern);
      if (!pattern->read(input)) return false;
      val.m_type = WP_CPATTERN;
      val.m_pixpattern = pattern;
      return true;
    }
    case WP_POINT:
    case WP_POINTBYTE:
    case WP_POINTUBYTE:
      val.m_type = WP_POINT;
      return readPoint(input, type, val.m_point);
    case WP_POLY:
      val.m_type = WP_POLY;
      return readPoly(input, type, val.m_box, val.m_listPoint);
    case WP_RECT:
      val.m_type = WP_RECT;
      return readRect(input, type, val.m_box);
    case WP_REGION: {
      shared_ptr<Region> rgn(new Region);
      if (!rgn->read(input)) return false;
      val.m_type = WP_REGION;
      val.m_region = rgn;
      return true;
    }
    case WP_TEXT:
    case WP_LTEXT:
      val.m_type = WP_TEXT;
      return readText(input, type, val.m_text);
    case WP_BITMAP:
    case WP_RBITMAP:
    case WP_PBITMAP:
    case WP_RPBITMAP: {
      // first check if it is a bitmap or a pixmap
      bool pixmap = input.readULong(2) & 0x8000;
      input.seek(-2, librevenge::RVNG_SEEK_CUR);
      bool packed = type==WP_PBITMAP || type == WP_RPBITMAP;
      bool hasRgn = type ==WP_RBITMAP || type == WP_RPBITMAP;
      if (pixmap) {
        shared_ptr<Pixmap> pxmap(new Pixmap);
        if (!pxmap->read(input, packed, true, true, hasRgn)) return false;
        val.m_type = WP_CBITMAP;
        val.m_pixmap = pxmap;
      }
      else {
        shared_ptr<Bitmap> btmap(new Bitmap);
        if (!btmap->read(input, packed, hasRgn)) return false;
        val.m_type = WP_BITMAP;
        val.m_bitmap = btmap;
      }
      return true;
    }
    case WP_CBITMAP:
    case WP_CRBITMAP: {
      // can not guess, so we read the pixmap...
      bool hasRgn = type ==WP_CRBITMAP;
      shared_ptr<Pixmap> pxmap(new Pixmap);
      if (!pxmap->read(input, false, false, true, hasRgn)) return false;
      val.m_type = WP_CBITMAP;
      val.m_pixmap = pxmap;
      return true;
    }
    case WP_CCOLOR: // version 2
      val.m_type = WP_CCOLOR;
      return readCColor(input, type, val.m_rgb);
    case WP_QUICKTIME: { // version 2
      val.m_type = WP_QUICKTIME;
      long size = (long) input.readULong(4);
      return input.seek(size, librevenge::RVNG_SEEK_CUR) == 0;
    }
    case WP_UNKNOWN:
      MWAW_DEBUG_MSG(("Pict1:readValue: find unknown type... \n"));
      break;
    case WP_NONE:
    default:
      MWAW_DEBUG_MSG(("Pict1:readValue: does not know how to read type %d... \n", type));
    }
    return false;
  }
  //! low level: reads a integer ( bytes or 2 bytes, signed or unsigned)
  static bool readInt(MWAWInputStream &input, DataType type, int &res)
  {
    int sz = 0;
    long actualPos = input.tell();
    res = 0;
    switch (type) {
    case WP_BYTE:
      res = (int) input.readLong((sz=1));
      break;
    case WP_UBYTE:
      res = (int) input.readULong((sz=1));
      break;
    case WP_INT:
      res = (int) input.readLong((sz=2));
      break;
    case WP_UINT:
      res = (int) input.readULong((sz=2));
      break;
    case WP_UFIXED:
      res = (int) input.readULong((sz=4));
      break;
    case WP_NONE:
    case WP_COLOR:
    case WP_PATTERN:
    case WP_POINT:
    case WP_POINTBYTE:
    case WP_POINTUBYTE:
    case WP_POLY:
    case WP_RECT:
    case WP_REGION:
    case WP_TEXT:
    case WP_LTEXT:
    case WP_BITMAP:
    case WP_RBITMAP:
    case WP_PBITMAP:
    case WP_RPBITMAP:
    case WP_UNKNOWN:
    case WP_CCOLOR:
    case WP_CPATTERN:
    case WP_CBITMAP:
    case WP_CRBITMAP:
    case WP_QUICKTIME:
    default:
      MWAW_DEBUG_MSG(("Pict1:OpCode: readInt is called with %d\n", type));
      return false;
    };
    if (actualPos+sz != input.tell()) {
      MWAW_DEBUG_MSG(("Pict1:OpCode: readInt find end of file...\n"));
      return false;
    }
    return true;
  }
  /** low level: reads a color argument
   *
   * \note check if this is not an indexed color */
  static bool readColor(MWAWInputStream &input, DataType type, MWAWColor &col)
  {
    if (type != WP_COLOR) {
      MWAW_DEBUG_MSG(("Pict1:OpCode: readColor is called with %d\n", type));
      return false;
    }
    long actualPos = input.tell();
    long val = (long) input.readULong(4);
    switch (val) {
    case 30:
      col = MWAWColor::white();
      break; // white
    case 33:
      col = MWAWColor::black();
      break; // black
    case 69:
      col = MWAWColor(255,255,0);
      break; // yellow
    case 137:
      col = MWAWColor(255,0,255);
      break; // magenta
    case 205:
      col = MWAWColor(255,0,0);
      break; // red
    case 273:
      col = MWAWColor(0,255,255);
      break; // cyan
    case 341:
      col = MWAWColor(0,255,0);
      break; // green
    case 409:
      col = MWAWColor(0,0,255);
      break; // blue
    default:
      MWAW_DEBUG_MSG(("Pict1:OpCode: unknown color %ld\n", val));
      col = MWAWColor(128,128,128);
      break; // gray
    }

    if (actualPos+4 != input.tell()) {
      MWAW_DEBUG_MSG(("Pict1:OpCode: readColor find end of file...\n"));
      return false;
    }
    return true;
  }

  /** low level: reads a color argument (version 2)
   *
   * \note check if this is not an indexed color */
  static bool readCColor(MWAWInputStream &input, DataType type, MWAWColor &col)
  {
    if (type != WP_CCOLOR) {
      MWAW_DEBUG_MSG(("Pict1:OpCode: readCColor is called with %d\n", type));
      return false;
    }
    long actualPos = (long) input.tell();

    unsigned char color[3];
    for (int i = 0; i < 3; i++)
      color[i] = (unsigned char)(input.readULong(2)>>8);
    col = MWAWColor(color[0],color[1],color[2]);

    if (actualPos+6 != input.tell()) {
      MWAW_DEBUG_MSG(("Pict1:OpCode: readCColor find end of file...\n"));
      return false;
    }
    return true;
  }

  //! low level: reads a pattern argument
  static bool readPattern(MWAWInputStream &input, DataType type, int (&pat)[8])
  {
    if (type != WP_PATTERN) {
      MWAW_DEBUG_MSG(("Pict1:OpCode: readPattern is called with %d\n", type));
      return false;
    }
    long actualPos = input.tell();

    for (int i = 0; i < 8; i++) pat[i]=(int) input.readULong(1);

    if (actualPos+8 != input.tell()) {
      MWAW_DEBUG_MSG(("Pict1:OpCode: readPattern find end of file...\n"));
      return false;
    }
    return true;
  }

  //! low level: reads a point argument
  static bool readPoint(MWAWInputStream &input, DataType type, Vec2i &res)
  {
    int v[2];
    DataType valType;
    switch (type) {
    case WP_POINT:
      valType = WP_INT;
      break;
    case WP_POINTBYTE:
      valType = WP_BYTE;
      break;
    case WP_POINTUBYTE:
      valType = WP_UBYTE;
      break;
    case WP_NONE:
    case WP_BYTE:
    case WP_UBYTE:
    case WP_INT:
    case WP_UINT:
    case WP_UFIXED:
    case WP_COLOR:
    case WP_PATTERN:
    case WP_POLY:
    case WP_RECT:
    case WP_REGION:
    case WP_TEXT:
    case WP_LTEXT:
    case WP_BITMAP:
    case WP_RBITMAP:
    case WP_PBITMAP:
    case WP_RPBITMAP:
    case WP_UNKNOWN:
    case WP_CCOLOR:
    case WP_CPATTERN:
    case WP_CBITMAP:
    case WP_CRBITMAP:
    case WP_QUICKTIME:
    default:
      MWAW_DEBUG_MSG(("Pict1:OpCode: readPoint is called with %d\n", type));
      return false;
    };
    for (int p = 0; p < 2; p++) {
      if (!readInt(input, valType, v[p])) return false;
    }
    res.set(v[0], v[1]);
    return true;
  }

  //! low level: reads a polygon argument
  static bool readPoly(MWAWInputStream &input, DataType type, Box2i &box, std::vector<Vec2i> &res)
  {
    DataType boxType, valType;
    switch (type) {
    case WP_POLY:
      valType = WP_POINT;
      boxType = WP_RECT;
      break;
    case WP_NONE:
    case WP_BYTE:
    case WP_UBYTE:
    case WP_INT:
    case WP_UINT:
    case WP_UFIXED:
    case WP_COLOR:
    case WP_PATTERN:
    case WP_POINT:
    case WP_POINTBYTE:
    case WP_POINTUBYTE:
    case WP_RECT:
    case WP_REGION:
    case WP_TEXT:
    case WP_LTEXT:
    case WP_BITMAP:
    case WP_RBITMAP:
    case WP_PBITMAP:
    case WP_RPBITMAP:
    case WP_UNKNOWN:
    case WP_CCOLOR:
    case WP_CPATTERN:
    case WP_CBITMAP:
    case WP_CRBITMAP:
    case WP_QUICKTIME:
    default:
      MWAW_DEBUG_MSG(("Pict1:OpCode: readPoly is called with %d\n", type));
      return false;
    };
    int sz;
    if (!readInt(input, WP_UINT, sz)) return false;
    if ((sz%2) != 0) {
      MWAW_DEBUG_MSG(("Pict1:OpCode: readPoly odd size: %d\n", sz));
      return false;
    }
    sz /= 2;
    if (sz < 5) {
      MWAW_DEBUG_MSG(("Pict1:OpCode: readPoly size is too short: %d\n", sz*2));
      return false;
    }
    if (!readRect(input, boxType, box)) return false;
    int numPt = sz-5;
    if ((numPt%2) != 0) {
      MWAW_DEBUG_MSG(("Pict1:OpCode: readPoly odd point number: %d\n", numPt));
      return false;
    }
    numPt /= 2;
    res.resize(size_t(numPt));

    Vec2i pt;
    for (int p = 0; p < numPt; p++) {
      if (!readPoint(input, valType, pt)) return false;
      res[size_t(p)] = pt;
    }
    return true;
  }
  //! low level: reads a string argument
  static bool readText(MWAWInputStream &input, DataType type, std::string &res)
  {
    int sz = 0;
    switch (type) {
    case WP_TEXT:
      // CHECKME:in at least one file: DHText, sz=5 but the text contains only
      //         one char
      if (!readInt(input, WP_UBYTE, sz)) return false;
      break;
    case WP_LTEXT:
      if (!readInt(input, WP_INT, sz) || sz < 0) return false;
      break;
    case WP_NONE:
    case WP_BYTE:
    case WP_UBYTE:
    case WP_INT:
    case WP_UINT:
    case WP_UFIXED:
    case WP_COLOR:
    case WP_PATTERN:
    case WP_POINT:
    case WP_POINTBYTE:
    case WP_POINTUBYTE:
    case WP_POLY:
    case WP_RECT:
    case WP_REGION:
    case WP_BITMAP:
    case WP_RBITMAP:
    case WP_PBITMAP:
    case WP_RPBITMAP:
    case WP_UNKNOWN:
    case WP_CCOLOR:
    case WP_CPATTERN:
    case WP_CBITMAP:
    case WP_CRBITMAP:
    case WP_QUICKTIME:
    default:
      MWAW_DEBUG_MSG(("Pict1:OpCode: readText is called with %d\n", type));
      return false;
    }

    long actualPos = input.tell();
    res = "";
    for (int i = 0; i < sz; i++) {
      char c = (char) input.readULong(1);
      res += c;
    }
    if (actualPos+sz != input.tell()) {
      MWAW_DEBUG_MSG(("Pict1:OpCode: readText: find EOF\n"));
      return false;
    }
    return true;
  }

};

/** internal and low level: list of known opcodes
 *
 * \note codes 0x2c, 0x2e, 0xa5 are not standard opcodes, but I find them in some pictures */
static OpCode const s_listCodes[] = {
  OpCode(0,"NOP"), OpCode(1,"ClipRgn",WP_REGION), OpCode(2,"BkPat",WP_PATTERN),
  OpCode(3,"TxFont",WP_INT), OpCode(4,"TxFace",WP_UBYTE), OpCode(5,"TxMode",WP_INT), OpCode(6,"SpExtra",WP_UFIXED),
  OpCode(7,"PnSize",WP_POINT), OpCode(8,"PnMode",WP_INT), OpCode(9,"PnPat",WP_PATTERN),
  OpCode(0xa,"FillPat",WP_PATTERN), OpCode(0xb,"OvSize",WP_POINT), OpCode(0xc,"Origin",WP_POINT),
  OpCode(0xd,"TxSize",WP_INT), OpCode(0xe,"FgColor",WP_COLOR), OpCode(0xf,"BkColor",WP_COLOR),
  OpCode(0x10,"TxRatio",WP_POINT,WP_POINT), OpCode(0x11,"picVersion",WP_UBYTE),

  OpCode(0x20,"Line",WP_POINT,WP_POINT),OpCode(0x21,"LineFrom",WP_POINT),
  OpCode(0x22,"ShortLine",WP_POINT, WP_POINTBYTE), OpCode(0x23,"ShortLineFrom", WP_POINTBYTE),

  OpCode(0x28,"LongText",WP_POINT,WP_TEXT), OpCode(0x29,"DHText",WP_UBYTE,WP_TEXT), OpCode(0x2a,"DVText",WP_UBYTE,WP_TEXT),
  OpCode(0x2b,"DHDVText",WP_POINTUBYTE,WP_TEXT),
  // not really a Pict1. code, but it can appear in some pict
  OpCode(0x2c,"FontName",WP_INT,WP_INT,WP_TEXT),
  // can we find 0x2d ?
  // Fixme: add 0x2e: not really a Pict1. code, but it can appear in some pict
  OpCode(0x2e,"GlyphState?", WP_INT, WP_INT, WP_INT),

  OpCode(0x30,"frameRect",WP_RECT), OpCode(0x31,"paintRect",WP_RECT), OpCode(0x32,"eraseRect",WP_RECT),
  OpCode(0x33,"invertRect",WP_RECT), OpCode(0x34,"fillRect",WP_RECT),
  OpCode(0x38,"frameSameRect"), OpCode(0x39,"paintSameRect"), OpCode(0x3a,"eraseSameRect"),
  OpCode(0x3b,"invertSameRect"), OpCode(0x3c,"fillSameRect"),

  OpCode(0x40,"frameRRect",WP_RECT), OpCode(0x41,"paintRRect",WP_RECT), OpCode(0x42,"eraseRRect",WP_RECT),
  OpCode(0x43,"invertRRect",WP_RECT), OpCode(0x44,"fillRRect",WP_RECT),
  OpCode(0x48,"frameSameRRect"), OpCode(0x49,"paintSameRRect"), OpCode(0x4a,"eraseSameRRect"),
  OpCode(0x4b,"invertSameRRect"), OpCode(0x4c,"fillSameRRect"),

  OpCode(0x50,"frameOval",WP_RECT), OpCode(0x51,"paintOval",WP_RECT), OpCode(0x52,"eraseOval",WP_RECT),
  OpCode(0x53,"invertOval",WP_RECT), OpCode(0x54,"fillOval",WP_RECT),
  OpCode(0x58,"frameSameOval"), OpCode(0x59,"paintSameOval"), OpCode(0x5a,"eraseSameOval"),
  OpCode(0x5b,"invertSameOval"), OpCode(0x5c,"fillSameOval"),

  OpCode(0x60,"frameArc",WP_RECT,WP_INT,WP_INT), OpCode(0x61,"paintArc",WP_RECT,WP_INT,WP_INT), OpCode(0x62,"eraseArc",WP_RECT,WP_INT,WP_INT),
  OpCode(0x63,"invertArc",WP_RECT,WP_INT,WP_INT), OpCode(0x64,"fillArc",WP_RECT,WP_INT,WP_INT),
  OpCode(0x68,"frameSameArc",WP_INT,WP_INT), OpCode(0x69,"paintSameArc",WP_INT,WP_INT), OpCode(0x6a,"eraseSameArc",WP_INT,WP_INT),
  OpCode(0x6b,"invertSameArc",WP_INT,WP_INT), OpCode(0x6c,"fillSameArc",WP_INT,WP_INT),

  OpCode(0x70,"framePoly",WP_POLY), OpCode(0x71,"paintPoly",WP_POLY), OpCode(0x72,"erasePoly",WP_POLY),
  OpCode(0x73,"invertPoly",WP_POLY), OpCode(0x74,"fillPoly",WP_POLY),
  // reserved: but not implemented
  OpCode(0x78,"frameSamePoly"), OpCode(0x79,"paintSamePoly"), OpCode(0x7a,"eraseSamePoly"),
  OpCode(0x7b,"invertSamePoly"), OpCode(0x7c,"fillSamePoly"),

  OpCode(0x80,"frameRgn",WP_REGION), OpCode(0x81,"paintRgn",WP_REGION), OpCode(0x82,"eraseRgn",WP_REGION),
  OpCode(0x83,"invertRgn",WP_REGION), OpCode(0x84,"fillRgn",WP_REGION),
  // reserved: but not implemented
  OpCode(0x88,"frameSameRgn"), OpCode(0x89,"paintSameRgn"), OpCode(0x8a,"eraseSameRgn"),
  OpCode(0x8b,"invertSameRgn"), OpCode(0x8c,"fillSameRgn"),

  // fixme: bitmap to implement
  OpCode(0x90,"BitsRect", WP_BITMAP),
  OpCode(0x91,"BitsRgn", WP_RBITMAP),

  OpCode(0x98,"PackBitsRect", WP_PBITMAP),
  OpCode(0x99,"PackBitsRgn",  WP_RPBITMAP),

  OpCode(0xa0,"ShortComment", WP_INT),
  OpCode(0xa1,"LongComment", WP_INT, WP_LTEXT),
  // not really a Pict1. code, but it can appear in some pict
  OpCode(0xa5,"LongComment????", WP_INT, WP_LTEXT),
  OpCode(0xff,"EndOfPicture")
};

/** internal and low level: map opcode id -> OpCode */
class PictParser
{
public:
  //! the constructor
  PictParser() : m_mapIdOp()
  {
    size_t numCodes = sizeof(s_listCodes)/sizeof(OpCode);
    for (size_t i = 0; i < numCodes; i++)
      m_mapIdOp[s_listCodes[i].m_id] = &(s_listCodes[i]);
  }
  /** internal and low level: parses a picture and stores the parsing in dFile
   *
   * \note this is mainly a debugging function but this function
   * can probably serve as model if we want to convert a Pict1.0
   * in another format
   */
  void parse(MWAWInputStreamPtr input, libmwaw::DebugFile &dFile);

  /**  internal and low level: tries to convert a Pict1.0 picture stored in \a orig in a Pict2.0 picture */
  bool convertToPict2(librevenge::RVNGBinaryData const &orig, librevenge::RVNGBinaryData &result);
protected:

  //! the map
  std::map<int,OpCode const *> m_mapIdOp;
};

/* internal and low level: parses a picture and stores the parsing in dFile
 *
 * \note this is mainly a debugging function but this function
 * can probably serve as model if we want to convert a Pict1.0
 * in another format
 */
void PictParser::parse(MWAWInputStreamPtr input, libmwaw::DebugFile &dFile)
{
  libmwaw::DebugStream s;
  long actPos = 0L;

  input->seek(0, librevenge::RVNG_SEEK_SET);
  int sz = (int) input->readULong(2);
  s.str("");
  s << "PictSize=" << sz;
  dFile.addPos(0);
  dFile.addNote(s.str().c_str());
  actPos = 2;

  Box2i box;
  bool ok = OpCode::readRect(*input, WP_RECT, box);
  if (ok) {
    s.str("");
    s << "PictBox=" << box;
    dFile.addPos(actPos);
    dFile.addNote(s.str().c_str());
    actPos = input->tell();
  }

  while (ok && !input->isEnd()) {
    actPos = input->tell();
    int code = (int) input->readULong(1);
    std::map<int,OpCode const *>::iterator it = m_mapIdOp.find(code);
    if (it == m_mapIdOp.end() || it->second == 0L) {
      MWAW_DEBUG_MSG(("Pict1:OpCode:parsePict can not find opCode 0x%x\n", code));
      input->seek(actPos, librevenge::RVNG_SEEK_SET);
      ok = false;
      break;
    }

    OpCode const &opCode = *(it->second);
    std::vector<Value> readData;
    if (!opCode.readData(*input, readData)) {
      MWAW_DEBUG_MSG(("Pict1:OpCode:parsePict error for opCode 0x%x\n", code));
      input->seek(actPos, librevenge::RVNG_SEEK_SET);
      ok = false;
      break;
    }
    s.str("");
    s << opCode.m_name << ":";
    for (size_t i = 0; i < readData.size(); i++) {
      if (i) s << ", ";
      s << readData[i];
    }
    dFile.addPos(actPos);
    dFile.addNote(s.str().c_str());
  }
  if (!ok) {
    dFile.addPos(actPos);
    dFile.addNote("###");
  }
}

/**  internal and low level: tries to convert a Pict1.0 picture stored in \a orig in a Pict2.0 picture */
bool PictParser::convertToPict2(librevenge::RVNGBinaryData const &orig, librevenge::RVNGBinaryData &result)
{
#  ifdef ADD_DATA_SHORT
#    undef ADD_DATA_SHORT
#  endif
#  define ADD_DATA_SHORT(resPtr,val) do {			\
  *(resPtr++) = (unsigned char)((val & 0xFF00) >> 8);	\
  *(resPtr++) = (unsigned char) (val & 0xFF); } while(0)

  long pictSize = (long) orig.size();
  if (pictSize < 10) return false;

  unsigned char *res = new unsigned char [size_t(2*pictSize+50)], *resPtr = res;
  if (!res) return false;

  MWAWInputStreamPtr input=MWAWInputStream::get(orig, false);
  if (!input) {
    delete [] res;
    return false;
  }

  input->seek(0, librevenge::RVNG_SEEK_SET);
  int sz = (int) input->readULong(2);
  if (pictSize != sz && pictSize != sz+1) {
    delete [] res;
    return false;
  }

  ADD_DATA_SHORT(resPtr,0); // size, we must fill it latter
  long dim[4];
  for (int i = 0; i < 4; i++) { // read the rectangle
    dim[i] = input->readLong(2);
    ADD_DATA_SHORT(resPtr,dim[i]);
  }
  if (input->readLong(2) != 0x1101) {
    delete [] res;
    return false;
  }
  ADD_DATA_SHORT(resPtr,0x11);
  ADD_DATA_SHORT(resPtr,0x2FF);
  ADD_DATA_SHORT(resPtr,0xC00);  // HeaderOp
  ADD_DATA_SHORT(resPtr, -1);
  ADD_DATA_SHORT(resPtr, -1);
  for (int i = 0; i < 4; i++) {
    int depl = (i%2) ? -1 : 1;
    ADD_DATA_SHORT(resPtr,dim[i+depl]);
    ADD_DATA_SHORT(resPtr,0);
  }
  ADD_DATA_SHORT(resPtr, 0);
  ADD_DATA_SHORT(resPtr, 0);
#  undef ADD_DATA_SHORT

  bool findEnd = false;
  while (!findEnd && !input->isEnd()) {
    long actPos = input->tell();
    int code = (int) input->readULong(1);
    std::map<int,OpCode const *>::iterator it = m_mapIdOp.find(code);
    if (it == m_mapIdOp.end() || it->second == 0L) {
      MWAW_DEBUG_MSG(("Pict1:convertToPict2 can not find opCode 0x%x\n", code));
      delete [] res;
      return false;
    }

    OpCode const &opCode = *(it->second);
    sz = 0;
    if (!opCode.computeSize(*input, sz)) {
      MWAW_DEBUG_MSG(("Pict1:convertToPict2 can not compute size for opCode 0x%x\n", code));
      delete [] res;
      return false;
    }
    bool skip = (code == 0x2e) || (code == 0xa5); // normally unemplemented, so..
    findEnd = code == 0xff;

    if (!skip) {
      *(resPtr++) = 0;
      *(resPtr++) = (unsigned char) code;
      input->seek(actPos+1, librevenge::RVNG_SEEK_SET);
      for (int i = 0; i < sz; i++)
        *(resPtr++) = (unsigned char) input->readULong(1);
      if ((sz%2)==1) *(resPtr++) = 0;
    }
    input->seek(actPos+1+sz, librevenge::RVNG_SEEK_SET);
  }

  bool endOk = false;
  if (findEnd) {
    if (input->isEnd()) endOk = true;
    else { // allows a final caracter for alignment
      input->seek(1, librevenge::RVNG_SEEK_CUR);
      endOk = input->isEnd();
    }
  }
  if (!endOk) {
    MWAW_DEBUG_MSG(("Pict1:convertToPict2 find EOF or EndOfPict prematurely \n"));
    delete [] res;
    return false;
  }

  long newSize = resPtr - res;
  res[0] = (unsigned char)((newSize & 0xFF00) >> 8);
  res[1] = (unsigned char)(newSize & 0xFF);
  result.clear();
  result.append(res, (unsigned long)newSize);
  delete [] res;

  return true;
}
}

/** Internal and low level: generic tools about Mac Pict2.0 picture
 *
 * This regroups some functions to parse them.
 */
namespace libmwaw_applepict2
{
using namespace libmwaw_applepict1;

/** internal and low level: list of new opcodes */
static OpCode const s_listCodes[] = {
  OpCode(0x12,"BackCPat",WP_CPATTERN), OpCode(0x13,"PenCPat",WP_CPATTERN), OpCode(0x14,"FillCPat",WP_CPATTERN),
  OpCode(0x15, "PnLocHFrac", WP_INT), OpCode(0x16, "ChExtra", WP_INT),
  OpCode(0x1A, "RGBFgColor", WP_CCOLOR), OpCode(0x1B, "RGBBKColor", WP_CCOLOR),
  OpCode(0x1C, "HiliteMode"), OpCode(0x1D, "HiliteColor", WP_CCOLOR),
  OpCode(0x1E, "DefHilite"), OpCode(0x1F, "OpColor", WP_CCOLOR),
  OpCode(0x2D, "LineJustify", WP_INT, WP_UFIXED, WP_UFIXED),
  OpCode(0x2E, "GlyphState", WP_INT, WP_BYTE, WP_BYTE, WP_BYTE, WP_BYTE),
  OpCode(0x9A, "DirectBitsRect", WP_CBITMAP),
  OpCode(0x9B, "DirectBitsRgn",  WP_CRBITMAP),
  OpCode(0x8200, "CompressedQuicktime", WP_QUICKTIME),
  OpCode(0x8201, "UncompressedQuicktime", WP_QUICKTIME)
};

/** internal and low level: map opcode id -> OpCode */
class PictParser
{
public:
  //! the constructor
  PictParser() : m_mapIdOp()
  {
    size_t numCodes = sizeof(libmwaw_applepict1::s_listCodes)/sizeof(OpCode);
    for (size_t i = 0; i < numCodes; i++)
      m_mapIdOp[libmwaw_applepict1::s_listCodes[i].m_id] = &(libmwaw_applepict1::s_listCodes[i]);
    numCodes = sizeof(s_listCodes)/sizeof(OpCode);
    for (size_t i = 0; i < numCodes; i++)
      m_mapIdOp[s_listCodes[i].m_id] = &(s_listCodes[i]);
  }
  /** internal and low level: parses a picture and stores the parsing in dFile
   *
   * \note this is mainly a debugging function but this function
   * can probably serve as model if we want to convert a Pict1.0
   * in another format
   */
  void parse(MWAWInputStreamPtr input, libmwaw::DebugFile &dFile);
protected:

  //! the map
  std::map<int,OpCode const *> m_mapIdOp;
};

/* internal and low level: parses a picture and stores the parsing in dFile
 *
 * \note this is mainly a debugging function but this function
 * can probably serve as model if we want to convert a Pict2.0
 * in another format
 */
void PictParser::parse(MWAWInputStreamPtr input, libmwaw::DebugFile &dFile)
{
  libmwaw::DebugStream s;
  long actPos = 0L;

  input->seek(0, librevenge::RVNG_SEEK_SET);
  int sz = (int) input->readULong(2);
  s.str("");
  s << "PictSize=" << sz;
  dFile.addPos(0);
  dFile.addNote(s.str().c_str());
  actPos = 2;

  Box2i box;
  bool ok = OpCode::readRect(*input, WP_RECT, box);
  if (ok) {
    s.str("");
    s << "PictBox=" << box;
    dFile.addPos(actPos);
    dFile.addNote(s.str().c_str());
    actPos = input->tell();
  }

  // version
  if (ok && input->readULong(2) == 0x11 && input->readULong(2) == 0x2FF) {
    dFile.addPos(actPos);
    dFile.addNote("Version=0x2ff");
    actPos = input->tell();
  }
  else if (!ok) {
    MWAW_DEBUG_MSG(("Pict2:OpCode:parsePict no/bad version\n"));
    ok = false;
  }

  // header
  long headerOp = (long)input->readULong(2);
  long version = -input->readLong(2);
  long subVersion = input->readLong(2);
  if (ok && headerOp == 0xC00 && (version == 1 || version == 2)) {
    s.str("");
    s << "Header=(" << version << ":" << subVersion << ")";
    switch (version) {
    case 1: {
      s << ", dim=(";
      for (int i = 0; i < 4; i++) {
        long dim = input->readLong(2);
        long dim2 = (long)input->readULong(2);
        s << dim;
        if (dim2) s << "." << float(dim2)/65336.;
        s << ",";
      }
      s << ")";
      input->readULong(4); // reserved
      break;
    }
    case 2: {
      s << ", res=(";
      for (int i = 0; i < 2; i++) {
        long dim = (long)input->readULong(2);
        long dim2 = (long)input->readULong(2);
        s << dim;
        if (dim2) s << "." << float(dim2)/65336.;
        s << ",";
      }
      s << "), dim=(";
      for (int i = 0; i < 4; i++) {
        long dim = (long)input->readULong(2);
        s << dim << ",";
      }
      s << ")";
      input->readULong(4); // reserved
      break;
    }

    default:
      break;
    }
    dFile.addPos(actPos);
    dFile.addNote(s.str().c_str());
    actPos = input->tell();
  }
  else if (!ok) {
    MWAW_DEBUG_MSG(("Pict2:OpCode:parsePict no header\n"));
    ok = false;
  }
  while (ok && !input->isEnd()) {
    actPos = input->tell();
    int code = (int)input->readULong(2);
    std::map<int,OpCode const *>::iterator it = m_mapIdOp.find(code);
    if (it == m_mapIdOp.end() || it->second == 0L) {
      MWAW_DEBUG_MSG(("Pict2:OpCode:parsePict can not find opCode 0x%x\n", code));
      input->seek(actPos, librevenge::RVNG_SEEK_SET);
      ok = false;
      break;
    }

    OpCode const &opCode = *(it->second);
    std::vector<Value> readData;
    if (!opCode.readData(*input, readData)) {
      MWAW_DEBUG_MSG(("Pict2:OpCode:parsePict error for opCode 0x%x\n", code));
      input->seek(actPos, librevenge::RVNG_SEEK_SET);
      ok = false;
      break;
    }
    // we must check alignment
    if ((input->tell() - actPos)%2 == 1)
      input->seek(1, librevenge::RVNG_SEEK_CUR);

    s.str("");
    s << opCode.m_name << ":";
    for (size_t i = 0; i < readData.size(); i++) {
      if (i) s << ", ";
      s << readData[i];
    }
    dFile.addPos(actPos);
    dFile.addNote(s.str().c_str());
  }
  if (!ok) {
    dFile.addPos(actPos);
    dFile.addNote("###");
  }
}
}

namespace libmwaw_applepict1
{
//! the map id -> opcode
static PictParser s_parser;
}
namespace libmwaw_applepict2
{
//! the map id -> opcode
static PictParser s_parser;
}

void MWAWPictMac::parsePict1(librevenge::RVNGBinaryData const &pict, std::string const &fname)
{
  MWAWInputStreamPtr ip=MWAWInputStream::get(pict, false);
  if (!ip) return;

  libmwaw::DebugFile dFile(ip);
  dFile.open(fname);
  libmwaw_applepict1::s_parser.parse(ip, dFile);
}

void MWAWPictMac::parsePict2(librevenge::RVNGBinaryData const &pict, std::string const &fname)
{
  MWAWInputStreamPtr ip=MWAWInputStream::get(pict, false);
  if (!ip) return;

  libmwaw::DebugFile dFile(ip);
  dFile.open(fname);
  libmwaw_applepict2::s_parser.parse(ip, dFile);
}

bool MWAWPictMac::convertPict1To2(librevenge::RVNGBinaryData const &orig, librevenge::RVNGBinaryData &result)
{
  static bool volatile conversion = false;
  while (conversion) ;
  conversion = true;
  bool ok = libmwaw_applepict1::s_parser.convertToPict2(orig, result);
  conversion = false;
  if (!ok) return false;

#if DEBUG_PICT
  if (1) {
    static bool first = true;
    static int actPict;
    static std::string mainName;
    if (first) {
      actPict = 0;
      mainName="Pict";
      first = false;
    }

    std::stringstream f;
    f << mainName << actPict << ".pict";
    libmwaw::Debug::dumpFile(const_cast<librevenge::RVNGBinaryData &>(orig), f.str().c_str());

    std::stringstream s;
    s << mainName << actPict;
    parsePict1(orig, s.str());

    actPict++;
  }
#endif

  return true;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
