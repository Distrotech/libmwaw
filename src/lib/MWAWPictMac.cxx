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

// and then to save the read bitmap/pixmap
#define DEBUG_BITMAP 0

MWAWPictMac::ReadResult MWAWPictMac::checkOrGet
(MWAWInputStreamPtr input, int size, MWAWBox2f &box, MWAWPictData **result)
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

  box.set(MWAWVec2f(float(dim[1]),float(dim[0])), MWAWVec2f(float(dim[3]),float(dim[2])));
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
*/
enum DataType {
  WP_NONE, WP_BYTE, WP_UBYTE, WP_INT, WP_UINT, WP_UFIXED,
  WP_COLOR, WP_PATTERN, WP_POINT, WP_POINTBYTE, WP_POINTUBYTE, WP_POLY, WP_RECT, WP_REGION, WP_TEXT, WP_LTEXT,
  WP_BITMAP, WP_RBITMAP, WP_PBITMAP, WP_RPBITMAP, WP_UNKNOWN
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
    if ((sz%2) != 0 || sz<10 || !input.checkPosition(actualPos+sz)) {
      MWAW_DEBUG_MSG(("Pict1:Region: read odd size: %d\n", sz));
      return false;
    }
    sz /= 2;
    int val[4];
    for (int i = 0; i < 4; i++) val[i] = (int) input.readLong(2);
    m_box.set(MWAWVec2i(val[1], val[0]), MWAWVec2i(val[3], val[2]));
    sz -= 5;
    m_points.resize(0);
    if (sz == 0) return true;
    // une liste de point dans la box: x1, y1, .. yn 0x7fff, x2, ... 0x7fff
    while (sz > 0) {
      int y = (int)input.readLong(2);
      sz--;
      if (y == 0x7fff) break;
      if (y < m_box[0].y() || y > m_box[1].y()) {
        MWAW_DEBUG_MSG(("Pict1:Region: found eroneous y value: %d\n", y));
        return false;
      }
      bool endF = false;
      while (sz > 0) {
        int x = (int)input.readLong(2);
        sz--;
        if (x == 0x7fff) {
          endF = true;
          break;
        }
        if (x < m_box[0].x() || x > m_box[1].x()) {
          MWAW_DEBUG_MSG(("Pict1:Region: found eroneous x value %d\n", x));
          return false;
        }
        m_points.push_back(MWAWVec2i(x,y));
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
  MWAWBox2i m_box;
  //! the set of points which defines the mask
  std::vector<MWAWVec2i> m_points;
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
      MWAWBox2i box(MWAWVec2i(val[1],val[0]), MWAWVec2i(val[3],val[2]));
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
    MWAWPictBitmapBW bitmap(MWAWVec2i(m_rect.size().x(),nRows));
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
  MWAWBox2i m_rect /** the bitmap rectangle */, m_src/** the initial dimension */, /** another final dimension */ m_dst  ;
  //! the region
  shared_ptr<Region> m_region;
  //! the bitmap
  std::vector<unsigned char> m_bitmap;
  //! the encoding mode ?
  int m_mode;
};

//! Internal and low level: a class used to read and store all possible value
struct Value {
  Value() : m_type(), m_int(0), m_rgb(MWAWColor::white()), m_text(""), m_point(), m_box(), m_listPoint(),
    m_region(), m_bitmap()
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
    default:
      MWAW_DEBUG_MSG(("Pict1:Value: does not know how to print my values... \n"));
    }
    return o;
  }

  //! the int value when type=WP_INT
  int m_int;
  //! the color when type=WP_COLOR
  MWAWColor m_rgb;
  //! the pattern when type=WP_PATTERN
  int m_pat[8];
  //! the text when type=WP_TEXT
  std::string m_text;
  //! the point when type=WP_POINT
  MWAWVec2i m_point;
  //! the rectangle when type=WP_RECT
  MWAWBox2i m_box;
  //! the list of points which defined the polygon when type=WP_POLY
  std::vector<MWAWVec2i> m_listPoint;
  //! the region when type=WP_REGION
  shared_ptr<Region> m_region;
  //! the bitmap when type=WP_BITMAP
  shared_ptr<Bitmap> m_bitmap;
};

//! Internal and low level: a class to define each opcode and their arguments and read their data
struct OpCode {
  /** constructor
   *
   * \param id is the code of the opcode in the file
   * \param nm is the short name of the opcode
   * \param type1 type of the first component
   * \param type2 type of the second component (if it exists)
   * \param type3 type of the third component (if it exists)
   * \param type4 type of the fourst component (if it exists)
   * \param type5 type of the fifth component (if it exists)
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
  static bool readRect(MWAWInputStream &input, DataType type, MWAWBox2i &res)
  {
    MWAWVec2i v[2];
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
    case WP_PATTERN:
    case WP_RECT:
      return 8;
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
      bool packed = type==WP_PBITMAP || type == WP_RPBITMAP;
      bool hasRgn = type ==WP_RBITMAP || type == WP_RPBITMAP;
      shared_ptr<Bitmap> btmap(new Bitmap);
      if (!btmap->read(input, packed, hasRgn)) return -1;
      return int(input.tell()-actPos);
    }
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
      bool packed = type==WP_PBITMAP || type == WP_RPBITMAP;
      bool hasRgn = type ==WP_RBITMAP || type == WP_RPBITMAP;
      shared_ptr<Bitmap> btmap(new Bitmap);
      if (!btmap->read(input, packed, hasRgn)) return false;
      val.m_type = WP_BITMAP;
      val.m_bitmap = btmap;
      return true;
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
  static bool readPoint(MWAWInputStream &input, DataType type, MWAWVec2i &res)
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
  static bool readPoly(MWAWInputStream &input, DataType type, MWAWBox2i &box, std::vector<MWAWVec2i> &res)
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

    MWAWVec2i pt;
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
    default:
      MWAW_DEBUG_MSG(("Pict1:OpCode: readText is called with %d\n", type));
      return false;
    }

    long actualPos = input.tell();
    res = "";
    if (!input.checkPosition(actualPos+sz)) {
      MWAW_DEBUG_MSG(("Pict1:OpCode: readText: find EOF\n"));
      return false;
    }

    for (int i = 0; i < sz; i++) {
      char c = (char) input.readULong(1);
      res += c;
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

  /**  internal and low level: tries to convert a Pict1.0 picture stored in \a orig in a Pict2.0 picture */
  bool convertToPict2(librevenge::RVNGBinaryData const &orig, librevenge::RVNGBinaryData &result);
protected:

  //! the map
  std::map<int,OpCode const *> m_mapIdOp;
};


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
      MWAW_DEBUG_MSG(("Pict1:convertToPict2 can not find opCode 0x%x\n", (unsigned int) code));
      delete [] res;
      return false;
    }

    OpCode const &opCode = *(it->second);
    sz = 0;
    if (!opCode.computeSize(*input, sz)) {
      MWAW_DEBUG_MSG(("Pict1:convertToPict2 can not compute size for opCode 0x%x\n", (unsigned int) code));
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

namespace libmwaw_applepict1
{
//! the map id -> opcode
static PictParser s_parser;
}

bool MWAWPictMac::convertPict1To2(librevenge::RVNGBinaryData const &orig, librevenge::RVNGBinaryData &result)
{
  static bool volatile conversion = false;
  while (conversion) ;
  conversion = true;
  bool ok = libmwaw_applepict1::s_parser.convertToPict2(orig, result);
  conversion = false;
  return ok;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
