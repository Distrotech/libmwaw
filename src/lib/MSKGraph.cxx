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
#include <map>
#include <set>
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWGraphicListener.hxx"
#include "MWAWGraphicShape.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictBitmap.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "MSKParser.hxx"
#include "MSKTable.hxx"

#include "MSKGraph.hxx"

/** Internal: the structures of a MSKGraph */
namespace MSKGraphInternal
{
////////////////////////////////////////
//! Internal: a list of zones ( for v4)
struct RBZone {
  RBZone(): m_isMain(true), m_id(-2), m_idList(), m_frame("") {}
  //! returns a unique id
  int getId() const {
    return m_isMain ? -1 : m_id;
  }
  //! the zone type: rbdr(true) or rbil
  bool m_isMain;
  //! the zone id
  int m_id;
  //! the list of rb
  std::vector<int> m_idList;
  //! the frame name ( if it exist )
  std::string m_frame;
};

////////////////////////////////////////
//! Internal: the generic pict
struct Zone {
  enum Type { Unknown, Shape, ChartZone, Group, Pict, Text, Textv4, Bitmap, TableZone, OLE};
  //! constructor
  Zone() : m_subType(-1), m_zoneId(-1), m_pos(), m_dataPos(-1), m_fileId(-1), m_page(-1), m_decal(), m_finalDecal(), m_box(), m_line(-1),
    m_style(), m_order(0), m_extra(""), m_doNotSend(false), m_isSent(false) {
    for (int i = 0; i < 3; i++) m_ids[i] = 0;
  }
  //! destructor
  virtual ~Zone() {}

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Zone const &pict) {
    pict.print(o);
    return o;
  }
  //! return the type
  virtual Type type() const {
    return Unknown;
  }

  //! return a binary data (if known)
  virtual bool getBinaryData(MWAWInputStreamPtr,
                             WPXBinaryData &res, std::string &pictType) const {
    res.clear();
    pictType="";
    return false;
  }

  //! return the extra border space ( if needed)
  virtual float needExtraBorderWidth() const {
    return 0.0;
  }

  //! add frame parameters to propList (if needed )
  virtual void fillFramePropertyList(WPXPropertyList &) const { }

  //! return the box
  Box2f getLocalBox(bool extendWithBord=true) const {
    float x = m_box.size().x(), y=m_box.size().y();
    Vec2f min = m_box.min();
    if (x < 0) {
      min+=Vec2f(x,0);
      x *= -1.0f;
    }
    if (y < 0) {
      min+=Vec2f(0,y);
      y *= -1.0f;
    }
    Box2f res(min, min+Vec2f(x,y));
    if (!extendWithBord) return res;
    float bExtra = needExtraBorderWidth();
    if (bExtra > 0) res.extend(2.0f*bExtra);
    return res;
  }

  MWAWPosition getPosition(MWAWPosition::AnchorTo rel) const {
    MWAWPosition res;
    Box2f box = getLocalBox();
    if (rel==MWAWPosition::Paragraph || rel==MWAWPosition::Frame) {
      res = MWAWPosition(box.min()+m_finalDecal, box.size(), WPX_POINT);
      res.setRelativePosition(rel);
      if (rel==MWAWPosition::Paragraph)
        res.m_wrapping = MWAWPosition::WBackground;
    } else if (rel!=MWAWPosition::Page || m_page < 0) {
      res = MWAWPosition(Vec2f(0,0), box.size(), WPX_POINT);
      res.setRelativePosition(MWAWPosition::Char,
                              MWAWPosition::XLeft, MWAWPosition::YTop);
    } else {
      res = MWAWPosition(box.min()+m_finalDecal, box.size(), WPX_POINT);
      res.setRelativePosition(MWAWPosition::Page);
      res.setPage(m_page+1);
      res.m_wrapping =  MWAWPosition::WBackground;
    }
    if (m_order > 0) res.setOrder(m_order);
    return res;
  }

  //! the virtual print function
  virtual void print(std::ostream &o) const;

  //! the type
  int m_subType;
  //! the zone id
  int m_zoneId;
  //! the file position
  MWAWEntry m_pos;
  //! the data begin position
  long m_dataPos;
  //! the file id
  int m_fileId;
  //! the zones id (main, previous, next)
  long m_ids[3];
  //! the page
  int m_page;
  //! the local position
  Box2f m_decal;
  //! the final local position
  Vec2f m_finalDecal;
  //! local bdbox
  Box2f m_box;
  //! the line position(v1)
  int m_line;
  //! the style
  MSKGraph::Style m_style;
  //! the picture order
  int m_order;
  //! extra data
  std::string m_extra;
  //! a flag used to know if we need to send the data ( or if this is the part of a sub group)
  bool m_doNotSend;
  //! true if the zone is send
  bool m_isSent;
};

void Zone::print(std::ostream &o) const
{
  if (m_fileId >= 0) {
    o << "P" << m_fileId;
    if (m_zoneId >= 0) o << "[" << m_zoneId << "],";
    else o << ",";
  }
  for (int i = 0; i < 3; i++) {
    if (m_ids[i] <= 0) continue;
    switch(i) {
    case 0:
      o << "id=";
      break;
    case 1:
      o << "pId=";
      break;
    default:
      o << "nId=";
      break;
    }
    o << std::hex << m_ids[i] << std::dec << ",";
  }
  switch(m_subType) {
  case 0:
    o << "line,";
    break;
  case 1:
    o << "rect,";
    break;
  case 2:
    o << "rectOval,";
    break;
  case 3:
    o << "circle,";
    break;
  case 4:
    o << "arc,";
    break;
  case 5:
    o << "poly,";
    break;
  case 7:
    o << "pict,";
    break;
  case 8:
    o << "group,";
    break;
  case 9:
    o << "textbox,";
    break;
  case 0xa:
    o << "chart,";
    break;
  case 0xc:
    o << "equation/graph,";
    break;
  case 0xd:
    o << "bitmap,";
    break;
  case 0xe:
    o << "ssheet,";
    break;
  case 0xf:
    o << "textbox2,";
    break;
  case 0x10:
    o << "table,";
    break;
  case 0x100:
    o << "pict,";
    break; // V1 pict
  default:
    o << "#type=" << m_subType << ",";
  }
  if (m_page>=0) o << "page=" << m_page << ",";
  if (m_decal!=Box2f())
    o << "pos=" << m_decal << ",";
  o << "bdbox=" << m_box << ",";
  o << "style=[" << m_style << "],";
  if (m_line >= 0) o << "line=" << m_line << ",";
  if (m_extra.length()) o << m_extra;
}
////////////////////////////////////////
//! Internal: the group of a MSKGraph
struct GroupZone : public Zone {
  // constructor
  GroupZone(Zone const &z) : Zone(z), m_childs() { }

  //! return the type
  virtual Type type() const {
    return Group;
  }
  //! operator<<
  virtual void print(std::ostream &o) const {
    Zone::print(o);
    o << "childs=[";
    for (size_t i = 0; i < m_childs.size(); i++)
      o << "P" << m_childs[i] << ",";
    o << "],";
  }
  //! list of child id
  std::vector<int> m_childs;
};

////////////////////////////////////////
//! Internal: the simple form of a MSKGraph ( line, rect, ...)
struct BasicShape : public Zone {
  //! constructor
  BasicShape(Zone const &z) : Zone(z), m_shape() {
  }
  //! return the type
  virtual Type type() const {
    return Shape;
  }
  //! operator<<
  virtual void print(std::ostream &o) const {
    Zone::print(o);
    o << m_shape << ",";
  }
  //! return the extra border size
  virtual float needExtraBorderWidth() const {
    float res=m_style.m_lineWidth;
    if (m_shape.m_type==MWAWGraphicShape::Line) {
      for (int i=0; i<2; ++i) {
        if (m_style.m_arrows[i]) res+=4;
      }
    }
    return 0.5f*res;
  }
  //! return the shape type
  MWAWGraphicStyle getStyle() const {
    MWAWGraphicStyle style(m_style);
    if (m_subType!=0)
      style.m_arrows[0] = style.m_arrows[1]=false;
    return style;
  }

  //! the basic shape
  MWAWGraphicShape m_shape;
private:
  BasicShape(BasicShape const &);
  BasicShape &operator=(BasicShape const &);
};

////////////////////////////////////////
//! Internal: the table of a MSKGraph
struct Chart : public Zone {
  //! constructor
  Chart(Zone const &z) : Zone(z), m_chartId(0) { }
  //! empty constructor
  Chart() : Zone(), m_chartId(0) { }

  //! return the type
  virtual Type type() const {
    return ChartZone;
  }
  //! the chart id
  int m_chartId;
};

////////////////////////////////////////
//! Internal: the picture of a MSKGraph
struct DataPict : public Zone {
  //! constructor
  DataPict(Zone const &z) : Zone(z), m_dataEndPos(-1), m_naturalBox() { }
  //! empty constructor
  DataPict() : Zone(), m_dataEndPos(-1), m_naturalBox() { }

  //! return the type
  virtual Type type() const {
    return Pict;
  }
  //! return a binary data (if known)
  virtual bool getBinaryData(MWAWInputStreamPtr ip,
                             WPXBinaryData &res, std::string &type) const;

  //! operator<<
  virtual void print(std::ostream &o) const {
    Zone::print(o);
  }
  //! the end of data (only defined when different to m_pos.end())
  long m_dataEndPos;
  //! the pict box (if known )
  mutable Box2f m_naturalBox;
};

bool DataPict::getBinaryData(MWAWInputStreamPtr ip,
                             WPXBinaryData &data, std::string &pictType) const
{
  data.clear();
  pictType="";
  long endPos = m_dataEndPos<=0 ? m_pos.end() : m_dataEndPos;
  long pictSize = endPos-m_dataPos;
  if (pictSize < 0) {
    MWAW_DEBUG_MSG(("MSKGraphInternal::DataPict::getBinaryData: picture size is bad\n"));
    return false;
  }

#ifdef DEBUG_WITH_FILES
  if (1) {
    WPXBinaryData file;
    ip->seek(m_dataPos, WPX_SEEK_SET);
    ip->readDataBlock(pictSize, file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f;
    f << "Pict-" << ++pictName << ".pct";
    libmwaw::Debug::dumpFile(file, f.str().c_str());
  }
#endif

  ip->seek(m_dataPos, WPX_SEEK_SET);
  MWAWPict::ReadResult res = MWAWPictData::check(ip, (int)pictSize, m_naturalBox);
  if (res == MWAWPict::MWAW_R_BAD) {
    MWAW_DEBUG_MSG(("MSKGraphInternal::DataPict::getBinaryData: can not find the picture\n"));
    return false;
  }

  ip->seek(m_dataPos, WPX_SEEK_SET);
  shared_ptr<MWAWPict> pict(MWAWPictData::get(ip, (int)pictSize));

  return pict && pict->getBinary(data,pictType);
}

////////////////////////////////////////
//! Internal: the bitmap of a MSKGraph
struct DataBitmap : public Zone {
  //! constructor
  DataBitmap(Zone const &z) : Zone(z), m_numRows(0), m_numCols(0), m_dataSize(0),
    m_naturalBox() { }
  //! empty constructor
  DataBitmap() : Zone(), m_numRows(0), m_numCols(0), m_dataSize(0), m_naturalBox() { }

  //! return the type
  virtual Type type() const {
    return Bitmap;
  }
  //! return a binary data (if known)
  bool getPictureData(MWAWInputStreamPtr ip, WPXBinaryData &res,
                      std::string &type, std::vector<MWAWColor> const &palette) const;

  //! operator<<
  virtual void print(std::ostream &o) const {
    o << "nRows=" << m_numRows << ",";
    o << "nCols=" << m_numCols << ",";
    if (m_dataSize > 0)
      o << "dSize=" << std::hex << m_dataSize << std::dec << ",";
    Zone::print(o);
  }

  int m_numRows /** the number of rows*/, m_numCols/** the number of columns*/;
  long m_dataSize /** the bitmap data size */;
  //! the pict box (if known )
  mutable Box2f m_naturalBox;
};

bool DataBitmap::getPictureData
(MWAWInputStreamPtr ip, WPXBinaryData &data, std::string &pictType,
 std::vector<MWAWColor> const &palette) const
{
  data.clear();
  pictType="";
  if (m_dataSize <= 0 || m_dataSize < m_numRows*m_numCols) {
    MWAW_DEBUG_MSG(("MSKGraphInternal::DataBitmap::getPictureData: dataSize size is bad\n"));
    return false;
  }
  int szCol = int(m_dataSize/m_numRows);
  long pos = m_dataPos;

  MWAWPictBitmapIndexed *btmap = new MWAWPictBitmapIndexed(Vec2i(m_numCols, m_numRows));
  if (!btmap) return false;
  btmap->setColors(palette);
  shared_ptr<MWAWPict> pict(btmap);
  for (int i = 0; i < m_numRows; i++) {
    ip->seek(pos, WPX_SEEK_SET);

    unsigned long numRead;
    uint8_t const *value = ip->read((size_t) m_numCols, numRead);
    if (!value || int(numRead) != m_numCols) return false;
    btmap->setRow(i, value);

    pos += szCol;
  }

  return pict->getBinary(data,pictType);
}

////////////////////////////////////////
//! Internal: the table of a MSKGraph
struct Table : public Zone {
  //! constructor
  Table(Zone const &z) : Zone(z), m_tableId(0) { }
  //! empty constructor
  Table() : Zone(), m_tableId(0) { }

  //! return the type
  virtual Type type() const {
    return TableZone;
  }
  //! the table id
  int m_tableId;
};

////////////////////////////////////////
//! Internal: the textbox of a MSKGraph ( v2-v3)
struct TextBox : public Zone {
  //! constructor
  TextBox(Zone const &z) :
    Zone(z), m_numPositions(-1), m_fontsList(), m_positions(), m_formats(), m_text(""), m_justify(MWAWParagraph::JustificationLeft)
  { }

  //! return the type
  virtual Type type() const {
    return Text;
  }
  //! operator<<
  virtual void print(std::ostream &o) const {
    Zone::print(o);
    switch(m_justify) {
    case MWAWParagraph::JustificationLeft:
      break;
    case MWAWParagraph::JustificationCenter:
      o << ",centered";
      break;
    case MWAWParagraph::JustificationRight:
      o << ",right";
      break;
    case MWAWParagraph::JustificationFull:
      o << ",full";
      break;
    case MWAWParagraph::JustificationFullAllLines:
      o << ",fullAllLines";
      break;
    default:
      o << ",#just=" << m_justify;
      break;
    }
  }

  //! add frame parameters to propList (if needed )
  virtual void fillFramePropertyList(WPXPropertyList &extras) const {
    if (!m_style.m_baseSurfaceColor.isWhite())
      extras.insert("fo:background-color", m_style.m_baseSurfaceColor.str().c_str());
  }

  //! the number of positions
  int m_numPositions;
  //! the list of fonts
  std::vector<MWAWFont> m_fontsList;
  //! the list of positions
  std::vector<int> m_positions;
  //! the list of format
  std::vector<int> m_formats;
  //! the text
  std::string m_text;
  //! the paragraph alignement
  MWAWParagraph::Justification m_justify;
private:
  TextBox(TextBox const &);
  TextBox &operator=(TextBox const &);
};

////////////////////////////////////////
//! Internal: the ole zone of a MSKGraph ( v4)
struct OLEZone : public Zone {
  //! constructor
  OLEZone(Zone const &z) : Zone(z), m_oleId(-1), m_dim()
  { }

  //! return the type
  virtual Type type() const {
    return OLE;
  }
  //! operator<<
  virtual void print(std::ostream &o) const {
    if (m_oleId >= 0) o << "ole" << m_oleId << ",";
    if (m_dim[0] > 0 && m_dim[1] > 0) o << "dim=" << m_dim << ",";
    Zone::print(o);
  }

  //! the ole id
  int m_oleId;
  //! the dimension
  Vec2i m_dim;
};

////////////////////////////////////////
//! Internal: the textbox of a MSKGraph ( v4)
struct TextBoxv4 : public Zone {
  //! constructor
  TextBoxv4(Zone const &z) : Zone(z), m_text(), m_frame("")
  { }

  //! return the type
  virtual Type type() const {
    return Textv4;
  }
  //! operator<<
  virtual void print(std::ostream &o) const {
    Zone::print(o);
    if (m_text.valid()) o << ", textPos=[" << m_text.begin() << "-" << m_text.end() << "]";
  }

  //! add frame parameters to propList (if needed )
  virtual void fillFramePropertyList(WPXPropertyList &extras) const {
    if (!m_style.m_baseSurfaceColor.isWhite())
      extras.insert("fo:background-color", m_style.m_baseSurfaceColor.str().c_str());
  }

  //! the text of positions (0-0: means no text)
  MWAWEntry m_text;
  //! the frame name
  std::string m_frame;
};

//! Internal the pattern ressource of a MSKGraph
struct Patterns {
  //! constructor ( 4 int by patterns )
  Patterns(int num, uint16_t const *data) : m_num(num), m_valuesList(), m_percentList() {
    if (m_num<=0) return;
    m_valuesList.resize(size_t(m_num)*8);
    for (size_t i=0; i < size_t(m_num)*4; ++i) {
      uint16_t val=data[i];
      m_valuesList[2*i]=(unsigned char) (val>>8);
      m_valuesList[2*i+1]=(unsigned char) (val&0xFF);
    }
    size_t pat=0;
    for (size_t i=0; i < size_t(num); ++i) {
      int numOnes=0;
      for (int j=0; j < 8; ++j) {
        uint8_t val=m_valuesList[pat++];
        for (int b=0; b < 8; b++) {
          if (val&1) ++numOnes;
          val = uint8_t(val>>1);
        }
      }
      m_percentList.push_back(float(numOnes)/64.f);
    }
  }
  //! return the pattern corresponding to an id
  bool get(int id, MWAWGraphicStyle::Pattern &pat) const {
    if (id < 0 || id > m_num) {
      MWAW_DEBUG_MSG(("MSKGraphInternal::Patterns::get: can not find pattern %d\n", id));
      return false;
    }
    pat.m_dim=Vec2i(8,8);
    unsigned char const *ptr=&m_valuesList[8*size_t(id)];
    pat.m_data.resize(8);
    for (size_t i=0; i < 8; i++)
      pat.m_data[i]=*(ptr++);
    return true;
  }
  //! return the percentage corresponding to a pattern
  float getPercent(int id) const {
    if (id < 0 || id > m_num) {
      MWAW_DEBUG_MSG(("MSKGraphInternal::Patterns::getPatternPercent: can not find pattern %d\n", id));
      return 1.0;
    }
    return m_percentList[size_t(id)];
  }

  //! the number of patterns
  int m_num;
  //! the pattern values (8 data by pattern)
  std::vector<unsigned char> m_valuesList;
  //! the pattern percent values
  std::vector<float> m_percentList;
};
////////////////////////////////////////
//! Internal: the state of a MSKGraph
struct State {
  //! constructor
  State() : m_version(-1), m_zonesList(), m_RBsMap(), m_font(20,12), m_chartId(0), m_tableId(0), m_numPages(0), m_rsrcPatternsMap() { }
  //! return the pattern corresponding to an id
  bool getPattern(MWAWGraphicStyle::Pattern &pat, int id, long rsid=-1);
  //! return the percentage corresponding to a pattern
  float getPatternPercent(int id, long rsid=-1);
  //! init the pattern value
  void initPatterns(int vers);
  //! the version
  int m_version;
  //! the list of zone
  std::vector<shared_ptr<Zone> > m_zonesList;
  //! the RBIL zone id->list id
  std::map<int, RBZone> m_RBsMap;
  //! the actual font
  MWAWFont m_font;
  //! an index used to store chart
  int m_chartId;
  //! an index used to store table
  int m_tableId;
  //! the number of pages
  int m_numPages;
  //! a map ressource id -> patterns
  std::map<long, Patterns> m_rsrcPatternsMap;
};

void State::initPatterns(int vers)
{
  if (!m_rsrcPatternsMap.empty()) return;
  if (vers <= 2) {
    static uint16_t const (valuesV2[]) = {
      0xffff, 0xffff, 0xffff, 0xffff,  0xddff, 0x77ff, 0xddff, 0x77ff,  0xdd77, 0xdd77, 0xdd77, 0xdd77,  0xaa55, 0xaa55, 0xaa55, 0xaa55,
      0x55ff, 0x55ff, 0x55ff, 0x55ff,  0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa,  0xeedd, 0xbb77, 0xeedd, 0xbb77,  0x8888, 0x8888, 0x8888, 0x8888,
      0xb130, 0x031b, 0xd8c0, 0x0c8d,  0x8010, 0x0220, 0x0108, 0x4004,  0xff88, 0x8888, 0xff88, 0x8888,  0xff80, 0x8080, 0xff08, 0x0808,
      0x0000, 0x0002, 0x0000, 0x0002,  0x8040, 0x2000, 0x0204, 0x0800,  0x8244, 0x3944, 0x8201, 0x0101,  0xf874, 0x2247, 0x8f17, 0x2271,
      0x55a0, 0x4040, 0x550a, 0x0404,  0x2050, 0x8888, 0x8888, 0x0502,  0xbf00, 0xbfbf, 0xb0b0, 0xb0b0,  0x0000, 0x0000, 0x0000, 0x0000,
      0x8000, 0x0800, 0x8000, 0x0800,  0x8800, 0x2200, 0x8800, 0x2200,  0x8822, 0x8822, 0x8822, 0x8822,  0xaa00, 0xaa00, 0xaa00, 0xaa00,
      0x00ff, 0x00ff, 0x00ff, 0x00ff,  0x1122, 0x4488, 0x1122, 0x4488,  0x8040, 0x2000, 0x0204, 0x0800,  0x0102, 0x0408, 0x1020, 0x4080,
      0xaa00, 0x8000, 0x8800, 0x8000,  0xff80, 0x8080, 0x8080, 0x8080,  0x0814, 0x2241, 0x8001, 0x0204,  0x8814, 0x2241, 0x8800, 0xaa00,
      0x40a0, 0x0000, 0x040a, 0x0000,  0x0384, 0x4830, 0x0c02, 0x0101,  0x8080, 0x413e, 0x0808, 0x14e3,  0x1020, 0x54aa, 0xff02, 0x0408,
      0x7789, 0x8f8f, 0x7798, 0xf8f8,  0x0008, 0x142a, 0x552a, 0x1408,  0x0000, 0x0000, 0x0000, 0x0000,
    };
    m_rsrcPatternsMap.insert(std::map<long, Patterns>::value_type(-1,Patterns(39, valuesV2)));
  }
  static uint16_t const (values4002[]) = {
    0xffff, 0xffff, 0xffff, 0xffff,  0x7fff, 0xffff, 0xf7ff, 0xffff,  0x7fff, 0xf7ff, 0x7fff, 0xf7ff,  0x77ff, 0xddff, 0x77ff, 0xddff,
    0x55ff, 0xddff, 0x55ff, 0xddff,  0x55ff, 0xeeff, 0x55ff, 0xbbff,  0x55ff, 0x55ff, 0x55ff, 0x55ff,  0x77dd, 0x77dd, 0x77dd, 0x77dd,
    0x55bf, 0x55ff, 0x55fb, 0x55ff,  0x55bb, 0x55ff, 0x55bb, 0x55ff,  0x55bf, 0x55ee, 0x55fb, 0x55ee,  0x55bb, 0x55ee, 0x55bb, 0x55ee,
    0x55bb, 0x55ea, 0x55bb, 0x55ae,  0x55ba, 0x55ab, 0x55ba, 0x55ab,  0x55ea, 0x55aa, 0x55ae, 0x55aa,  0xaa55, 0xaa55, 0xaa55, 0xaa55,
    0xaa15, 0xaa55, 0xaa51, 0xaa55,  0xaa45, 0xaa54, 0xaa45, 0xaa54,  0xaa44, 0xaa15, 0xaa44, 0xaa51,  0xaa44, 0xaa11, 0xaa44, 0xaa11,
    0xaa40, 0xaa11, 0xaa04, 0xaa11,  0xaa44, 0xaa00, 0xaa44, 0xaa00,  0xaa40, 0xaa00, 0xaa04, 0xaa00,  0x8822, 0x8822, 0x8822, 0x8822,
    0xaa00, 0xaa00, 0xaa00, 0xaa00,  0xaa00, 0x1100, 0xaa00, 0x4400,  0xaa00, 0x2200, 0xaa00, 0x2200,  0x8800, 0x2200, 0x8800, 0x2200,
    0x8800, 0x2000, 0x8800, 0x0200,  0x8000, 0x0800, 0x8000, 0x0800,  0x8000, 0x0000, 0x0800, 0x0000,  0x0000, 0x0000, 0x0000, 0x0000
  };
  m_rsrcPatternsMap.insert(std::map<long, Patterns>::value_type(4002,Patterns(32, values4002)));
  static uint16_t const (values4003[]) = {
    0x0000, 0x0000, 0x0000, 0x0000,  0x8000, 0x0000, 0x0800, 0x0000,  0x8000, 0x0800, 0x8000, 0x0800,  0x8800, 0x2000, 0x8800, 0x0200,
    0x8800, 0x2200, 0x8800, 0x2200,  0xaa00, 0x2200, 0xaa00, 0x2200,  0xaa00, 0x1100, 0xaa00, 0x4400,  0xaa00, 0xaa00, 0xaa00, 0xaa00,
    0x8822, 0x8822, 0x8822, 0x8822,  0xaa44, 0xaa11, 0xaa44, 0xaa11,  0xaa45, 0xaa54, 0xaa45, 0xaa54,  0xaa55, 0xaa55, 0xaa55, 0xaa55,
    0x55ea, 0x55aa, 0x55ae, 0x55aa,  0x55ba, 0x55ab, 0x55ba, 0x55ab,  0x55bb, 0x55ee, 0x55bb, 0x55ee,  0x77dd, 0x77dd, 0x77dd, 0x77dd,
    0x55ff, 0x55ff, 0x55ff, 0x55ff,  0x55ff, 0xeeff, 0x55ff, 0xbbff,  0x77ff, 0xddff, 0x77ff, 0xddff,  0x7fff, 0xf7ff, 0x7fff, 0xf7ff,
    0x7fff, 0xffff, 0xf7ff, 0xffff,  0xffff, 0xffff, 0xffff, 0xffff
  };
  m_rsrcPatternsMap.insert(std::map<long, Patterns>::value_type(4003,Patterns(22, values4003)));
  static uint16_t const (values4004[]) = {
    0xf0f0, 0xf0f0, 0x0f0f, 0x0f0f,  0xcccc, 0x3333, 0xcccc, 0x3333,  0x3333, 0xcccc, 0x3333, 0xcccc
  };
  m_rsrcPatternsMap.insert(std::map<long, Patterns>::value_type(4004,Patterns(3, values4004)));
  static uint16_t const (values7000[]) = {
    0x0101, 0x1010, 0x0101, 0x1010,  0xcc00, 0x0000, 0x3300, 0x0000,  0x1122, 0x4400, 0x1122, 0x4400,  0x4422, 0x0088, 0x4422, 0x0088,
    0xf0f0, 0xf0f0, 0x0f0f, 0x0f0f,  0x9966, 0x6699, 0x9966, 0x6699,  0x0008, 0x1c3e, 0x7f3e, 0x1c08,  0x0008, 0x142a, 0x552a, 0x1408,
    0xb130, 0x031b, 0xd8c0, 0x0c8d,  0x8010, 0x0220, 0x0108, 0x4004,  0x0814, 0x2241, 0x8001, 0x0204,  0x80c0, 0x2112, 0x0c04, 0x0201,
    0xff80, 0x8080, 0xff08, 0x0808,  0x007f, 0x7f7f, 0x00f7, 0xf7f7,  0x8040, 0x2000, 0x0204, 0x0800,  0x8244, 0x3944, 0x8201, 0x0101,
    0xf078, 0x2442, 0x870f, 0x1221,  0x1020, 0x54aa, 0xff02, 0x0408,  0xf874, 0x2247, 0x8f17, 0x2271,  0xbfa0, 0xbfbd, 0xbdfd, 0x05fd,
    0x2050, 0x8888, 0x8888, 0x0502,  0x55a0, 0x4040, 0x550a, 0x0404,  0x8844, 0x2211, 0x1122, 0x4488,  0x8142, 0x2418, 0x8142, 0x2418,
    0xaa00, 0x8000, 0x8800, 0x8000,  0x0384, 0x4830, 0x0c02, 0x0101,  0x8080, 0x413e, 0x0808, 0x14e3,  0xaf5f, 0xaf5f, 0x0d0b, 0x0d0b,
    0x7789, 0x8f8f, 0x7798, 0xf8f8,  0x8814, 0x2241, 0x8800, 0xaa00,  0x40a0, 0x0000, 0x040a, 0x0000,  0xbf00, 0xbfbf, 0xb0b0, 0xb0b0
  };
  m_rsrcPatternsMap.insert(std::map<long, Patterns>::value_type(7000,Patterns(32, values7000)));
  static uint16_t const (values14001[]) = {
    0x8844, 0x2211, 0x8844, 0x2211,  0x77bb, 0xddee, 0x77bb, 0xddee,  0x1122, 0x4488, 0x1122, 0x4488,  0xeedd, 0xbb77, 0xeedd, 0xbb77,
    0x8040, 0x2010, 0x0804, 0x0201,  0x7fbf, 0xdfef, 0xf7fb, 0xfdfe,  0x0102, 0x0408, 0x1020, 0x4080,  0xfefd, 0xfbf7, 0xefdf, 0xbf7f,
    0xe070, 0x381c, 0x0e07, 0x83c1,  0x99cc, 0x6633, 0x99cc, 0x6633,  0x8307, 0x0e1c, 0x3870, 0xe0c1,  0x3366, 0xcc99, 0x3366, 0xcc99,
    0x8142, 0x2418, 0x1824, 0x4281,  0x7ebd, 0xdbe7, 0xe7db, 0xbd7e,  0x8244, 0x2810, 0x2844, 0x8201,  0x7dbb, 0xd7ef, 0xd7bb, 0x7dfe,
    0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa,  0x00ff, 0x00ff, 0x00ff, 0x00ff,  0x8888, 0x8888, 0x8888, 0x8888,  0x7777, 0x7777, 0x7777, 0x7777,
    0xff00, 0x0000, 0xff00, 0x0000,  0x00ff, 0xffff, 0x00ff, 0xffff,  0x8080, 0x8080, 0x8080, 0x8080,  0x7f7f, 0x7f7f, 0x7f7f, 0x7f7f,
    0xff00, 0x0000, 0x0000, 0x0000,  0x00ff, 0xffff, 0xffff, 0xffff,  0xcccc, 0xcccc, 0xcccc, 0xcccc,  0xffff, 0x0000, 0xffff, 0x0000,
    0xff88, 0x8888, 0xff88, 0x8888,  0x0077, 0x7777, 0x0077, 0x7777,  0xff80, 0x8080, 0x8080, 0x8080,  0x007f, 0x7f7f, 0x7f7f, 0x7f7f
  };
  m_rsrcPatternsMap.insert(std::map<long, Patterns>::value_type(14001,Patterns(32, values14001)));
}

float State::getPatternPercent(int id, long rsid)
{
  if (m_rsrcPatternsMap.empty())
    initPatterns(m_version);
  if (m_rsrcPatternsMap.find(rsid)==m_rsrcPatternsMap.end()) {
    MWAW_DEBUG_MSG(("MSKGraphInternal::State::getPatternPercent unknown map for rsdid=%ld\n",rsid));
    return 1.0;
  }
  return m_rsrcPatternsMap.find(rsid)->second.getPercent(id);
}

bool State::getPattern(MWAWGraphicStyle::Pattern &pat, int id, long rsid)
{
  if (m_rsrcPatternsMap.empty())
    initPatterns(m_version);
  if (m_rsrcPatternsMap.find(rsid)==m_rsrcPatternsMap.end()) {
    MWAW_DEBUG_MSG(("MSKGraphInternal::State::getPattern unknown map for rsdid=%ld\n",rsid));
    return false;
  }
  return m_rsrcPatternsMap.find(rsid)->second.get(id, pat);
}

////////////////////////////////////////
//! Internal: the subdocument of a MSKGraph
class SubDocument : public MWAWSubDocument
{
public:
  enum Type { RBILZone, Chart, Empty, Group, Table, TextBox, TextBoxv4 };
  SubDocument(MSKGraph &pars, MWAWInputStreamPtr input, Type type,
              int zoneId) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_graphParser(&pars), m_type(type), m_id(zoneId), m_frame("") {}
  SubDocument(MSKGraph &pars, MWAWInputStreamPtr input, Type type,
              MWAWEntry const &entry, std::string frame=std::string("")) :
    MWAWSubDocument(pars.m_mainParser, input, entry), m_graphParser(&pars), m_type(type), m_id(-1), m_frame(frame) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);
  //! the graphic parser function
  void parseGraphic(MWAWGraphicListenerPtr &listener, libmwaw::SubDocumentType type);
private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);

protected:
  /** the graph parser */
  MSKGraph *m_graphParser;
  /** the type */
  Type m_type;
  /** the subdocument id*/
  int m_id;
  /** the frame name: for textv4 */
  std::string m_frame;
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("MSKParser::SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_graphParser);

  long pos = m_input->tell();
  switch(m_type) {
  case Empty:
    break;
  case Chart:
    m_graphParser->sendChart(m_id);
    break;
  case Group: {
    MWAWPosition gPos;
    gPos.setRelativePosition(MWAWPosition::Frame,
                             MWAWPosition::XLeft, MWAWPosition::YTop);
    m_graphParser->sendGroupChild(m_id, gPos);
    break;
  }
  case Table:
    m_graphParser->sendTable(m_id);
    break;
  case TextBoxv4:
    m_graphParser->sendFrameText(m_zone, m_frame);
    break;
  case RBILZone: {
    MSKGraph::SendData sendData;
    sendData.m_type = MSKGraph::SendData::RBIL;
    sendData.m_id = m_id;
    sendData.m_anchor =  MWAWPosition::Frame;
    m_graphParser->sendObjects(sendData);
    break;
  }
  case TextBox:
  default:
    MWAW_DEBUG_MSG(("MSKGraph::SubDocument::parse: unexpected zone type\n"));
    break;
  }
  m_input->seek(pos, WPX_SEEK_SET);
}

void SubDocument::parseGraphic(MWAWGraphicListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("MSKParser::SubDocument::parse: no listener\n"));
    return;
  }
  if (m_type != TextBox) {
    MWAW_DEBUG_MSG(("MSKGraph::SubDocument::parseGraphic: unexpected zone type\n"));
    return;
  }
  assert(m_graphParser);

  long pos = m_input->tell();
  m_graphParser->sendTextBox(m_id);
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_graphParser != sDoc->m_graphParser) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_type != sDoc->m_type) return true;
  if (m_frame != sDoc->m_frame) return true;
  return false;
}

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MSKGraph::MSKGraph(MSKParser &parser) :
  m_parserState(parser.getParserState()), m_state(new MSKGraphInternal::State),
  m_mainParser(&parser), m_tableParser()
{
  m_tableParser.reset(new MSKTable(parser, *this));
}

MSKGraph::~MSKGraph()
{ }

int MSKGraph::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int MSKGraph::numPages(int zoneId) const
{
  if (m_state->m_numPages > 0)
    return m_state->m_numPages;

  int maxPage = 0;
  size_t numZones = m_state->m_zonesList.size();
  for (size_t i = 0; i < numZones; i++) {
    shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[i];
    if (zoneId >= 0 && zone->m_zoneId!=zoneId) continue;
    if (zone->m_page > maxPage)
      maxPage = zone->m_page;
  }
  m_state->m_numPages = maxPage+1;
  return m_state->m_numPages;
}

void MSKGraph::sendFrameText(MWAWEntry const &entry, std::string const &frame)
{
  m_mainParser->sendFrameText(entry, frame);
}

void MSKGraph::sendChart(int zoneId)
{
  m_tableParser->sendChart(zoneId);
}

void MSKGraph::sendTable(int zoneId)
{
  m_tableParser->sendTable(zoneId);
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool MSKGraph::getZoneGraphicStyle(int id, MWAWGraphicStyle &style) const
{
  if (id<0 || id>=int(m_state->m_zonesList.size()) || !m_state->m_zonesList[size_t(id)]) {
    MWAW_DEBUG_MSG(("MSKGraph::getZoneGraphicStyle: unknown zone %d\n", id));
    return false;
  }
  style = m_state->m_zonesList[size_t(id)]->m_style;
  return true;
}

bool MSKGraph::getZonePosition(int id, MWAWPosition::AnchorTo anchor, MWAWPosition &pos) const
{
  if (id<0 || id>=int(m_state->m_zonesList.size()) || !m_state->m_zonesList[size_t(id)]) {
    MWAW_DEBUG_MSG(("MSKGraph::getZoneGraphicStyle: unknown zone %d\n", id));
    return false;
  }
  pos = m_state->m_zonesList[size_t(id)]->getPosition(anchor);
  return true;
}

bool MSKGraph::readPictHeader(MSKGraphInternal::Zone &pict)
{
  MWAWInputStreamPtr input=m_mainParser->getInput();
  if (input->readULong(1) != 0) return false;
  pict = MSKGraphInternal::Zone();
  pict.m_subType = (int) input->readULong(1);
  if (pict.m_subType > 0x10 || pict.m_subType == 6 || pict.m_subType == 0xb)
    return false;
  int vers = version();
  if (vers <= 3 && pict.m_subType > 9)
    return false;

  libmwaw::DebugStream f;
  int val;
  if (vers >= 3) {
    val = (int) input->readLong(2);
    if (vers == 4)
      pict.m_page = val==0 ? -2 : val-1;
    else if (val)
      f << "f0=" << val << ",";
  }
  // color
  Style &style=pict.m_style;
  for (int i = 0; i < 2; i++) {
    int rId = (int) input->readLong(2);
    int cId = (vers <= 2) ? rId+1 : rId;
    MWAWColor col;
    if (m_mainParser->getColor(cId,col,vers <= 3 ? vers : 3)) {
      if (i) style.m_baseSurfaceColor = col;
      else style.m_baseLineColor = col;
    } else
      f << "#col" << i << "=" << rId << ",";
  }
  bool hasSurfPatFunction=false;
  if (vers <= 2) {
    for (int i = 0; i < 2; i++) {
      int pId = (int) input->readLong(2);
      if (pId==38) { // empty
        if (i==0)
          style.m_lineWidth=0;
        continue;
      }
      float percent = m_state->getPatternPercent(pId);
      MWAWGraphicStyle::Pattern pattern;
      if (i==0)
        style.m_lineColor=MWAWColor::barycenter(percent, style.m_baseLineColor, 1.f-percent, style.m_baseSurfaceColor);
      else if (m_state->getPattern(pattern, pId)) {
        style.m_pattern=pattern;
        style.m_pattern.m_colors[0] = style.m_baseSurfaceColor;
        style.m_pattern.m_colors[1] = style.m_baseLineColor;
      } else
        style.setSurfaceColor(MWAWColor::barycenter(percent, style.m_baseLineColor, 1.f-percent, style.m_baseSurfaceColor));
    }
    int lineType=(int) input->readLong(2);
    if (style.m_lineWidth>0) {
      switch(lineType) {
      case 0:
        style.m_lineWidth=0.;
        break;
      case 1:
        style.m_lineWidth=0.5;
        break;
      case 2: // lineW=1
        break;
      case 3:
        style.m_lineWidth=2;
        break;
      case 4:
        style.m_lineWidth=4;
        break;
      default:
        f << "#lineType=" << lineType << ",";
        break;
      }
    }
  } else {
    style.m_lineColor=style.m_baseLineColor;
    style.m_surfaceColor=style.m_baseSurfaceColor;
    for (int i = 0; i < 2; i++) {
      if (i) f << "surface";
      else f << "line";
      f << "Pattern=[";
      long rsid= input->readLong(2);
      if (rsid==0) f << "noColor,";
      else if (rsid==-1) f << "grad,";
      else f << "rsid=" << rsid << ",";
      int patId = (int) input->readULong(2);
      if (patId) f << "pat=" << patId << ",";
      else f << "_";
      if (vers==4 && rsid==-1 && patId==0xFFFF)
        hasSurfPatFunction=true;
      val = (int) input->readLong(1);
      if (val) f << "unkn=" << val << ",";
      int per = (int) input->readULong(1);
      f << per << "%,";
      if (rsid<=0) {
        if (i==0 && rsid==0)
          style.m_lineWidth=0.;
      } else {
        float percent=1.0;
        bool done=false;
        MWAWGraphicStyle::Pattern pattern;
        if (per >= 0 && per < 100)
          percent = float(per)/100.f;
        else if (m_state->getPattern(pattern, patId, rsid)) {
          percent = m_state->getPatternPercent(patId, rsid);
          if (i) {
            style.m_pattern=pattern;
            style.m_pattern.m_colors[0] = style.m_baseSurfaceColor;
            style.m_pattern.m_colors[1] = style.m_baseLineColor;
            done = true;
          }
        } else {
          MWAW_DEBUG_MSG(("MSKGraph::readPictHeader:find odd pattern\n"));
          f << "##";
        }
        if (done) {
        } else if (i==0)
          style.m_lineColor=MWAWColor::barycenter(percent, style.m_baseLineColor, 1.f-percent, style.m_baseSurfaceColor);
        else
          style.setSurfaceColor(MWAWColor::barycenter(percent, style.m_baseLineColor, 1.f-percent, style.m_baseSurfaceColor));
      }
      f << "],";
    }
    int penSize[2];
    for (int i = 0; i < 2; i++)
      penSize[i] = (int) input->readLong(2);
    if (style.m_lineWidth<=0)
      f << "pen=" << penSize[0] << "x" << penSize[1] << ",";
    else if (penSize[0]==penSize[1])
      style.m_lineWidth=(float) penSize[0];
    else {
      f << "pen=" << penSize[0] << "x" << penSize[1] << ",";
      style.m_lineWidth=0.5f*float(penSize[0]+penSize[1]);
    }
    if (style.m_lineWidth < 0 || style.m_lineWidth > 10) {
      f << "##penSize=" << style.m_lineWidth << ",";
      style.m_lineWidth = 1;
    }
    val =  (int) input->readLong(2);
    if (val)
      f << "f1=" << val << ",";
  }

  float offset[4];
  for (int i = 0; i < 4; i++)
    offset[i] = (float) input->readLong(2);
  pict.m_decal = Box2f(Vec2f(offset[0],offset[1]), Vec2f(offset[3],offset[2]));
  pict.m_finalDecal = Vec2f(float(offset[0]+offset[3]), float(offset[1]+offset[2]));

  // the two point which allows to create the form ( in general the bdbox)
  float dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = float(input->readLong(4))/65536.f;
  pict.m_box=Box2f(Vec2f(dim[0],dim[1]),Vec2f(dim[2],dim[3]));

  int flags = (int) input->readLong(1);
  // 2: rotations, 1:lock ?, 0: nothing, other ?
  if (vers >= 4 && (flags&1)) {
    f << "locked,";
    flags &= 0xFE;
  }
  if (vers >= 4 && (flags&2)) {
    f << "Rot=[";
    for (int i = 0; i < 32; i++)
      f << input->readLong(2) << ",";
    f << "],";
    flags &= 0xFC;
  }
  if (flags) f << "fl0=" << flags << ",";
  int lineFlags = (int) input->readULong(1);
  switch(lineFlags&3) {
  case 2:
    style.m_arrows[0]=true;
  case 1:
    style.m_arrows[1]=true;
    break;
  default:
    f << "#arrow=3,";
  case 0:
    break;
  }
  if (lineFlags&0xFC) f << "#lineFlags=" << std::hex << (lineFlags&0xFC) << std::dec << ",";
  if (vers >= 3) pict.m_ids[0] = (long) input->readULong(4);
  if (vers >= 4 && hasSurfPatFunction) {
    long pos = input->tell();
    if (!readGradient(style)) {
      f << "##gradient,";
      input->seek(pos, WPX_SEEK_SET);
    }
  }
  pict.m_extra = f.str();
  pict.m_dataPos = input->tell();
  return true;
}

bool MSKGraph::readGradient(MSKGraph::Style &style)
{
  MWAWInputStreamPtr input=m_mainParser->getInput();
  long pos = input->tell();

  if (!input->checkPosition(pos+22))
    return false;

  libmwaw::DebugStream f;
  f << "gradient[unknown]=[";
  int type=(int) input->readLong(2);
  int val=(int) input->readLong(2); // always 0?
  if (val) f << "f0=" << val << ",";
  val=(int) input->readLong(1); // always 8?
  if (val!=8) f << "f1=" << val << ",";
  val=(int) input->readLong(2); // find 1 in square
  if (val) f << "f2=" << val << ",";
  val=(int) input->readULong(2); // always 0 ?
  if (val) f << "f3=" << std::hex << val << std::dec << ",";
  int angle =(int) input->readLong(2);
  val=(int) input->readLong(2); // 89[square]|156[square:linearbi]|255
  if (val!=0xff) f << "f4=" << val << ",";
  val=(int) input->readLong(2); // 54[square]|0
  if (val) f << "f5=" << val << ",";
  val=(int) input->readLong(2); // 18
  if (val!=0x18) f << "f6=" << val << ",";
  val=(int) input->readULong(2);
  int subType = (val&0xf);
  val = (val>>4);
  if (val!=0xFF)
    f << "sType[high]=" << std::hex << val << std::dec << ",";
  val=(int) input->readLong(2); // 0
  if (val) f << "f7=" << val << ",";
  val=(int) input->readLong(1); // 0
  if (val) f << "f8=" << val << ",";
  f << "],";
  switch(type) {
  case 1:
    style.m_gradientStopList.resize(2);
    style.m_gradientStopList[0]=MWAWGraphicStyle::GradientStop(0.0, style.m_baseSurfaceColor);
    style.m_gradientStopList[1]=MWAWGraphicStyle::GradientStop(1.0, style.m_baseLineColor);
    style.m_gradientAngle = float(90+angle);
    style.m_gradientType = MWAWGraphicStyle::G_Linear;
    angle=type=0;
    break;
  case 2:
    style.m_gradientStopList.resize(2);
    style.m_gradientStopList[0]=MWAWGraphicStyle::GradientStop(0.0, style.m_baseSurfaceColor);
    style.m_gradientStopList[1]=MWAWGraphicStyle::GradientStop(1.0, style.m_baseLineColor);
    style.m_gradientAngle = float(90+angle);
    style.m_gradientType = MWAWGraphicStyle::G_Axial;
    angle=type=0;
    break;
  case 3:
    style.m_gradientStopList.resize(2);
    style.m_gradientStopList[0]=MWAWGraphicStyle::GradientStop(0.0, style.m_baseSurfaceColor);
    style.m_gradientStopList[1]=MWAWGraphicStyle::GradientStop(1.0, style.m_baseLineColor);
    switch (subType) {
    case 9:
      style.m_gradientPercentCenter=Vec2f(0.25f,0.25f);
      break;
    case 10:
      style.m_gradientPercentCenter=Vec2f(0.25f,0.75f);
      break;
    case 11:
      style.m_gradientPercentCenter=Vec2f(0.75f,0.75f);
      break;
    case 12:
      style.m_gradientPercentCenter=Vec2f(1.f,1.f);
      break;
    case 13:
      style.m_gradientPercentCenter=Vec2f(0.f,0.f);
      break;
    default:
      f << "#subType=" << subType << ",";
    case 8: // centered
      break;
    }
    style.m_gradientType = MWAWGraphicStyle::G_Rectangular;
    angle=type=0;
    break;
  case 7:
    style.m_gradientStopList.resize(2);
    style.m_gradientStopList[0]=MWAWGraphicStyle::GradientStop(0.0, style.m_baseSurfaceColor);
    style.m_gradientStopList[1]=MWAWGraphicStyle::GradientStop(1.0, style.m_baseLineColor);
    style.m_gradientType = MWAWGraphicStyle::G_Radial;
    type = 0;
    break;
  default:
    break;
  }
  if (type) f << "#type=" << type << ",";
  if (angle) f << "#angle=" << angle << ",";
  f << "subType=" << subType << ",";
  f << "],";
  style.m_extra = f.str();
  return true;
}

int MSKGraph::getEntryPicture(int zoneId, MWAWEntry &zone, bool autoSend, int order)
{
  MSKGraphInternal::Zone pict;
  MWAWInputStreamPtr input=m_mainParser->getInput();
  long pos = input->tell();

  if (!readPictHeader(pict))
    return -1;
  pict.m_zoneId = zoneId;
  pict.m_pos.setBegin(pos);
  libmwaw::DebugFile &ascFile = m_mainParser->ascii();
  libmwaw::DebugStream f;
  int vers = version();
  long debData = input->tell();
  long dataSize = 0;
  int versSize = 0;
  switch(pict.m_subType) {
  case 0:
  case 1:
  case 2:
  case 3:
    dataSize = 1;
    break;
  case 4: // arc
    dataSize = 0xd;
    break;
  case 5: { // poly
    input->seek(3, WPX_SEEK_CUR);
    int N = (int) input->readULong(2);
    dataSize = 9+N*8;
    break;
  }
  case 7: { // picture
    if (vers >= 3) versSize = 0x14;
    dataSize = 5;
    input->seek(debData+5+versSize-2, WPX_SEEK_SET);
    dataSize += (int) input->readULong(2);
    break;
  }
  case 8: // group
    if (vers >= 3) versSize = 4;
    dataSize = 0x1b;
    break;
  case 9: // textbox v<=3
    dataSize = 0x21;
    if (vers >= 3) dataSize += 0x10;
    break;
  case 0xa: // chart v4
    dataSize = 50;
    break;
  case 0xc: // equation v4
    dataSize = 0x11;
    break;
  case 0xd: { // bitmap v4
    input->seek(debData+0x29, WPX_SEEK_SET);
    long sz = (long) input->readULong(4);
    dataSize = 0x29+4+sz;
    break;
  }
  case 0xe: { // spreadsheet v4
    input->seek(debData+0xa7, WPX_SEEK_SET);
    int pSize = (int) input->readULong(2);
    if (pSize == 0) return -1;
    dataSize = 0xa9+pSize;
    if (!input->checkPosition(debData+dataSize))
      return -1;

    input->seek(debData+dataSize, WPX_SEEK_SET);
    for (int i = 0; i < 2; i++) {
      long sz = (long) input->readULong(4);
      if (sz<0 || (sz>>28)) return -1;
      dataSize += 4 + sz;
      input->seek(sz, WPX_SEEK_CUR);
    }
    break;
  }
  case 0xf: { // textbox v4
    input->seek(debData+0x39, WPX_SEEK_SET);
    dataSize = 0x3b+ (long) input->readULong(2);
    break;
  }
  case 0x10: { // table v4
    input->seek(debData+0x57, WPX_SEEK_SET);
    dataSize = 0x59+ (long) input->readULong(2);
    input->seek(debData+dataSize, WPX_SEEK_SET);

    for (int i = 0; i < 3; i++) {
      long sz = (long) input->readULong(4);
      if (sz<0 || ((sz>>28))) return -1;
      dataSize += 4 + sz;
      input->seek(sz, WPX_SEEK_CUR);
    }

    break;
  }
  default:
    MWAW_DEBUG_MSG(("MSKGraph::getEntryPicture: type %d is not umplemented\n", pict.m_subType));
    return -1;
  }

  pict.m_pos.setEnd(debData+dataSize+versSize);
  if (!input->checkPosition(pict.m_pos.end()))
    return -1;

  input->seek(debData, WPX_SEEK_SET);
  if (versSize) {
    switch(pict.m_subType) {
    case 7: {
      long ptr = (long) input->readULong(4);
      f << std::hex << "ptr2=" << ptr << std::dec << ",";
      f << "depth?=" << input->readLong(1) << ",";
      float dim[4];
      for (int i = 0; i < 4; i++)
        dim[i] = float(input->readLong(4))/65536.f;
      Box2f box(Vec2f(dim[1], dim[0]), Vec2f(dim[3], dim[2]));
      f << "bdbox2=" << box << ",";
      break;
    }
    default:
      break;
    }
  }
  int val = (int) input->readLong(1); // 0 and sometimes -1
  if (val) f << "g0=" << val << ",";
  pict.m_dataPos++;

  if (pict.m_subType > 0xd) {
    f << ", " << std::hex << input->readULong(4) << std::dec << ", BdBox2=(";
    for (int i = 0; i < 4; i++)
      f << float(input->readLong(4))/65536.f << ", ";
    f << ")";
  }

  shared_ptr<MSKGraphInternal::Zone> res;
  switch (pict.m_subType) {
  case 0: { // line
    MSKGraphInternal::BasicShape *form = new MSKGraphInternal::BasicShape(pict);
    res.reset(form);
    form->m_shape = MWAWGraphicShape::line(pict.m_box.min(), pict.m_box.max());
    break;
  }
  case 1: // rect
  case 2: // rectoval
  case 3: { // circle
    Box2f bdbox = pict.m_box;
    MSKGraphInternal::BasicShape *form = new MSKGraphInternal::BasicShape(pict);
    res.reset(form);
    form->m_shape.m_bdBox = form->m_shape.m_formBox = bdbox;
    form->m_shape.m_type = (pict.m_subType==3) ? MWAWGraphicShape::Circle :
                           MWAWGraphicShape::Rectangle;
    if (pict.m_subType==2) {
      float sz=10;
      if (bdbox.size().x() > 0 && bdbox.size().x() < 2*sz)
        sz = bdbox.size().x()/2.f;
      if (bdbox.size().y() > 0 && bdbox.size().y() < 2*sz)
        sz = bdbox.size().y()/2.f;
      form->m_shape.m_cornerWidth=Vec2f(sz,sz);
    }
    break;
  }
  case 4: {
    MSKGraphInternal::BasicShape *form  = new MSKGraphInternal::BasicShape(pict);
    res.reset(form);
    float angle = (float) input->readLong(2);
    float deltaAngle = (float) input->readLong(2);
    float angl2 = angle+((deltaAngle>0) ? deltaAngle : -deltaAngle);
    float dim[4]; // real Bdbox
    for (int i = 0; i < 4; i++)
      dim[i] = (float) input->readLong(2);
    Box2f realBox(Vec2f(dim[1],dim[0]), Vec2f(dim[3],dim[2]));
    form->m_shape=MWAWGraphicShape::arc(realBox,pict.m_box,Vec2f(450.f-angl2,450.f-angle));
    form->m_box = realBox;
    break;
  }
  case 5: {
    MSKGraphInternal::BasicShape *form  = new MSKGraphInternal::BasicShape(pict);
    res.reset(form);
    val = (int) input->readULong(2);
    bool smooth=false;
    if (val==1)
      smooth=true;
    else if (val) f << "#smooth=" << val << ",";
    int numPt = (int) input->readLong(2);
    long ptr = (long) input->readULong(4);
    f << std::hex << "ptr2=" << ptr << std::dec << ",";
    std::vector<Vec2f> vertices;
    for (int i = 0; i < numPt; i++) {
      float x = float(input->readLong(4))/65336.f;
      float y = float(input->readLong(4))/65336.f;
      vertices.push_back(Vec2f(x,y));
    }
    if (!smooth || numPt <= 2) {
      form->m_shape=MWAWGraphicShape::polygon(pict.m_box);
      form->m_shape.m_vertices = vertices;
      break;
    }
    form->m_shape=MWAWGraphicShape::path(pict.m_box);
    form->m_shape.m_path.push_back(MWAWGraphicShape::PathData('M', vertices[0]));

    Vec2f middle=0.5f*(vertices[1]+vertices[0]);
    form->m_shape.m_path.push_back(MWAWGraphicShape::PathData('L', middle));
    for (size_t pt=1; pt+1 < size_t(numPt); ++pt) {
      middle=0.5f*(vertices[pt+1]+vertices[pt]);
      form->m_shape.m_path.push_back(MWAWGraphicShape::PathData('Q', middle, vertices[pt]));
    }
    form->m_shape.m_path.push_back(MWAWGraphicShape::PathData('L',vertices[size_t(numPt-1)]));
    if (vertices[0]==vertices[size_t(numPt)-1])
      form->m_shape.m_path.push_back(MWAWGraphicShape::PathData('Z'));
    break;
  }
  case 7: {
    val =  (int) input->readULong(vers >= 3 ? 1 : 2);
    if (val) f << "g1=" << val << ",";
    // skip size (already read)
    pict.m_dataPos = input->tell()+2;
    MSKGraphInternal::DataPict *pct  = new MSKGraphInternal::DataPict(pict);
    res.reset(pct);
    ascFile.skipZone(pct->m_dataPos, pct->m_pos.end()-1);
    break;
  }
  case 8:
    res = readGroup(pict);
    if (!res)
      return -1;
    break;
  case 9: { // textbox normal
    MWAWParagraph::Justification justify = MWAWParagraph::JustificationLeft;
    val = (int) input->readLong(2);
    switch(val) {
    case 0:
      break;
    case 1:
      justify = MWAWParagraph::JustificationCenter;
      break;
    case 2:
      justify = MWAWParagraph::JustificationFull;
      break;
    case -1:
      justify = MWAWParagraph::JustificationRight;
      break;
    default:
      f << "##align=" << val << ",";
      break;
    }
    if (vers >= 3) {
      f << "h=" << input->readLong(4) << ",";
      for (int i = 0; i < 6; i++) {
        val = (int) input->readLong(2);
        if (val) f << "g" << i+2 << "=" << val << ",";
      }
      pict.m_dataPos += 0x10;
    }
    f << "Fl=[";
    for (int i = 0; i < 4; i++) {
      val = (int) input->readLong(2);
      if (val) f << std::hex << val << std::dec << ",";
      else f << ",";
    }
    f << "],";
    int numPos = (int) input->readLong(2);
    if (numPos < 0) return -1;
    f << "numFonts=" << input->readLong(2);

    long off[4];
    for (int i = 0; i < 4; i++)
      off[i] = (long) input->readULong(4);
    f << ", Ptrs=[" <<  std::hex << std::setw(8) << off[2] << ", " << std::setw(8) << off[0]
      << ", " << std::dec << long(off[1]-off[0])
      << ", "	<< std::dec << long(off[3]-off[0]) << "]";

    MSKGraphInternal::TextBox *text  = new MSKGraphInternal::TextBox(pict);
    text->m_justify = justify;
    text->m_numPositions = numPos;
    res.reset(text);
    if (!readText(*text)) return -1;
    res->m_pos.setEnd(input->tell());
    break;
  }
  case 0xa: { // chart
    MSKGraphInternal::Chart *chart  = new MSKGraphInternal::Chart(pict);
    int chartId = m_state->m_chartId++;
    if (!m_tableParser->readChart(chartId, chart->m_style))
      return -1;
    m_tableParser->setChartZoneId(chartId, int(m_state->m_zonesList.size()));
    chart->m_chartId = chartId;
    res.reset(chart);
    res->m_pos.setEnd(input->tell());
    break;
  }
  case 0xc: { // equation
    MSKGraphInternal::OLEZone *ole  = new MSKGraphInternal::OLEZone(pict);
    res.reset(ole);
    int dim[2];
    for (int i = 0; i < 2; i++)
      dim[i] = (int) input->readLong(4);
    ole->m_dim = Vec2i(dim[0], dim[1]);
    val = (int) input->readULong(2); // always 0x4f4d ?
    f << "g0=" << std::hex << val << std::dec << ",";
    ole->m_oleId=(int) input->readULong(4);
    val = (int) input->readLong(2); // always 0?
    if (val) f << "g1=" << val << ",";
    break;
  }
  case 0xd: { // bitmap
    libmwaw::DebugStream f2;
    f2 << "Graphd(II): fl(";

    long actPos = input->tell();
    for (int i = 0; i < 2; i++)
      f2 << input->readLong(2) << ", ";
    f2 << "), ";
    int nCol = (int) input->readLong(2);
    int nRow = (int) input->readLong(2);
    if (nRow <= 0 || nCol <= 0) return -1;

    f2 << "nRow=" << nRow << ", " << "nCol=" << nCol << ", ";

    f2 << std::hex << input->readULong(4) << std::dec << ", ";

    for (int i = 0; i < 3; i++) {
      f2 << "bdbox" << i << "=(";
      for (int d= 0; d < 4; d++) f2 << input->readLong(2) << ", ";
      f2 << "), ";
      if (i == 1) f2 << "unk=" << input->readLong(2) << ", ";
    }
    int sizeLine =  (int) input->readLong(2);
    f2 << "lineSize(?)=" << sizeLine << ", ";
    long bitmapSize = input->readLong(4);
    f2 << "bitmapSize=" << std::hex << bitmapSize << ", ";

    if (bitmapSize <= 0 || (bitmapSize%nRow) != 0) {
      // sometimes, another row is added: only for big picture?
      if (bitmapSize>0 && (bitmapSize%(nRow+1)) == 0) nRow++;
      else if (bitmapSize < nCol*nRow || bitmapSize > 2*nCol*nRow)
        return -1;
      else { // maybe a not implemented case
        MWAW_DEBUG_MSG(("MSKGraph::getEntryPicture: bitmap size is a little odd\n"));
        f2 << "###";
        ascFile.addPos(actPos);
        ascFile.addNote(f2.str().c_str());
        ascFile.addDelimiter(input->tell(),'|');
        break;
      }
    }

    int szCol = int(bitmapSize/nRow);
    if (szCol < nCol) return -1;

    ascFile.addPos(actPos);
    ascFile.addNote(f2.str().c_str());

    pict.m_dataPos = input->tell();
    MSKGraphInternal::DataBitmap *pct  = new MSKGraphInternal::DataBitmap(pict);
    pct->m_numRows = nRow;
    pct->m_numCols = nCol;
    pct->m_dataSize = bitmapSize;
    res.reset(pct);
    break;
  }
  case 0xe: {
    long actPos = input->tell();
    ascFile.addPos(actPos);
    ascFile.addNote("Graphe(I)");

    // first: the picture ( fixme: kept while we do not parse the spreadsheet )
    input->seek(144, WPX_SEEK_CUR);
    actPos = input->tell();
    ascFile.addPos(actPos);
    ascFile.addNote("Graphe(pict)");
    long dSize = (long) input->readLong(4);
    if (dSize < 0) return -1;
    pict.m_dataPos = actPos+4;

    MSKGraphInternal::DataPict *pct  = new MSKGraphInternal::DataPict(pict);
    pct->m_dataEndPos = actPos+4+dSize;
    res.reset(pct);
    ascFile.skipZone(pct->m_dataPos, pct->m_dataEndPos-1);
    input->seek(actPos+4+dSize, WPX_SEEK_SET);

    // now the spreadsheet ( a classic WKS file )
    actPos = input->tell();
    dSize = (long) input->readULong(4);
    if (dSize < 0) return -1;
    ascFile.addPos(actPos);
    ascFile.addNote("Graphe(sheet)");
    ascFile.skipZone(actPos+4, actPos+3+dSize);
#ifdef DEBUG_WITH_FILES
    if (dSize > 0) {
      WPXBinaryData file;
      input->seek(actPos+4, WPX_SEEK_SET);
      input->readDataBlock(dSize, file);
      static int volatile sheetName = 0;
      libmwaw::DebugStream f2;
      f2 << "Sheet-" << ++sheetName << ".wks";
      libmwaw::Debug::dumpFile(file, f2.str().c_str());
    }
#endif
    input->seek(actPos+4+dSize, WPX_SEEK_SET);

    actPos = input->tell();
    ascFile.addPos(actPos);
    ascFile.addNote("Graphe(colWidth?)"); // blocksize, unknown+list of 100 w
    break;
  }
  case 0xf: { // new text box v4 (a picture is stored)
    if (vers < 4) return -1;
    MSKGraphInternal::TextBoxv4 *textbox = new MSKGraphInternal::TextBoxv4(pict);
    res.reset(textbox);
    textbox->m_ids[1] = (long) input->readULong(4);
    textbox->m_ids[2] = (long) input->readULong(4);
    f << "," << std::hex << input->readULong(4)<< std::dec << ",";
    // always 0 ?
    for (int i = 0; i < 6; i++) {
      val = (int) input->readLong(2);
      if (val) f << "f" << i << "=" << val << ",";
    }
    textbox->m_text.setBegin(input->readLong(4));
    textbox->m_text.setEnd(input->readLong(4));

    // always 0 ?
    val = (int) input->readLong(2);
    if (val) f << "f10=" << val << ",";
    long sz = (long) input->readULong(4);
    if (sz+0x3b != dataSize)
      f << "###sz=" << sz << ",";

    pict.m_dataPos = input->tell();
    if (pict.m_dataPos != pict.m_pos.end()) {
#ifdef DEBUG_WITH_FILES
      WPXBinaryData file;
      input->readDataBlock(pict.m_pos.end()-pict.m_dataPos, file);
      static int volatile textboxName = 0;
      libmwaw::DebugStream f2;
      f2 << "TextBox-" << ++textboxName << ".pct";
      libmwaw::Debug::dumpFile(file, f2.str().c_str());
#endif
      ascFile.skipZone(pict.m_dataPos, pict.m_pos.end()-1);
    }
    break;
  }
  case 0x10: { // basic table
    libmwaw::DebugStream f2;
    f2 << "Graph10(II): fl=(";
    long actPos = input->tell();
    for (int i = 0; i < 3; i++)
      f2 << input->readLong(2) << ", ";
    f2 << "), ";
    int nRow = (int) input->readLong(2);
    int nCol = (int) input->readLong(2);
    f2 << "nRow=" << nRow << ", " << "nCol=" << nCol << ", ";

    // basic name font
    int nbChar = (int) input->readULong(1);
    if (nbChar > 31) return -1;
    std::string fName;
    for (int c = 0; c < nbChar; c++)
      fName+=(char) input->readLong(1);
    f2 << fName << ",";
    input->seek(actPos+10+32, WPX_SEEK_SET);
    int fSz = (int) input->readLong(2);
    if (fSz) f << "fSz=" << fSz << ",";

    ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(actPos);
    ascFile.addNote(f2.str().c_str());
    input->seek(actPos+0x40, WPX_SEEK_SET);

    // a pict
    actPos = input->tell();
    ascFile.addPos(actPos);
    ascFile.addNote("Graph10(pict)");
    long dSize = (long) input->readLong(4);
    if (dSize < 0) return -1;
    pict.m_dataPos = actPos+4;

    MSKGraphInternal::DataPict *pct  = new MSKGraphInternal::DataPict(pict);
    pct->m_dataEndPos = actPos+4+dSize;
    res.reset(pct);
    ascFile.skipZone(pct->m_dataPos, pct->m_dataEndPos-1);
    input->seek(actPos+4+dSize, WPX_SEEK_SET);

    // the table
    f << "numRows=" << nRow << ",nCols=" << nCol << ",";
    shared_ptr<MSKGraphInternal::Table> table(new MSKGraphInternal::Table(pict));
    int tableId = m_state->m_tableId++;
    if (m_tableParser->readTable(nCol, nRow, tableId, table->m_style)) {
      table->m_tableId = tableId;
      res=table;
    }
    break;
  }
  default:
    ascFile.addDelimiter(debData, '|');
    break;
  }

  if (!res)
    res.reset(new MSKGraphInternal::Zone(pict));
  res->m_extra += f.str();

  if (order > -1000)
    res->m_order = order;
  if (!autoSend)
    res->m_doNotSend = true;
  res->m_fileId = int(m_state->m_zonesList.size());
  m_state->m_zonesList.push_back(res);

  f.str("");
  f << "Entries(Graph" << std::hex << res->m_subType << std::dec << "):" << *res;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  zone = res->m_pos;
  zone.setType("Graphic");
  input->seek(res->m_pos.end(), WPX_SEEK_SET);

  return res->m_fileId;
}

void MSKGraph::computePositions(int zoneId, std::vector<int> &linesH, std::vector<int> &pagesH)
{
  int numLines = int(linesH.size());
  int nPages = int(pagesH.size());
  size_t numZones = m_state->m_zonesList.size();
  for (size_t i = 0; i < numZones; i++) {
    shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[i];
    if (zone->m_zoneId != -1 && zoneId != zone->m_zoneId) continue;
    if (zone->m_line >= 0) {
      int h = 0;
      if (zone->m_line >= numLines) {
        MWAW_DEBUG_MSG(("MSKGraph::computePositions: linepos is too big\n"));
        if (numLines)
          h = linesH[(size_t) numLines-1];
      } else
        h = linesH[(size_t) zone->m_line];
      zone->m_finalDecal = Vec2f(0, float(h));
    }
    if (zone->m_page < 0 && zone->m_page != -2) {
      float h = zone->m_finalDecal.y();
      float middleH=zone->m_box.center().y();
      h+=middleH;
      int p = 0;
      while (p < nPages) {
        if (h < pagesH[(size_t) p]) break;
        h -= float(pagesH[(size_t) p++]);
      }
      zone->m_page = p;
      zone->m_finalDecal.setY(h-middleH);
    }
  }
}

int MSKGraph::getEntryPictureV1(int zoneId, MWAWEntry &zone, bool autoSend)
{
  MWAWInputStreamPtr input=m_mainParser->getInput();
  if (input->atEOS()) return -1;

  long pos = input->tell();
  if (input->readULong(1) != 1) return -1;

  libmwaw::DebugFile &ascFile = m_mainParser->ascii();
  libmwaw::DebugStream f;
  long ptr = (long) input->readULong(2);
  int flag = (int) input->readULong(1);
  long size = (long) input->readULong(2)+6;
  if (size < 22) return -1;

  // check if we can go to the next zone
  if (!input->checkPosition(pos+size)) return -1;
  shared_ptr<MSKGraphInternal::DataPict> pict(new MSKGraphInternal::DataPict());
  pict->m_zoneId = zoneId;
  pict->m_subType = 0x100;
  pict->m_pos.setBegin(pos);
  pict->m_pos.setLength(size);

  if (ptr) f << std::hex << "ptr0=" << ptr << ",";
  if (flag) f << std::hex << "fl=" << flag << ",";

  ptr = input->readLong(4);
  if (ptr)
    f << "ptr1=" << std::hex << ptr << std::dec << ";";
  pict->m_line = (int) input->readLong(2);
  int val = (int) input->readLong(2); // almost always equal to m_linePOs
  if (val !=  pict->m_line)
    f <<  "linePos2=" << std::hex << val << std::dec << ",";
  int dim[4]; // pictbox
  for (int i = 0; i < 4; i++)
    dim[i] = (int) input->readLong(2);
  pict->m_box = Box2f(Vec2f(float(dim[1]), float(dim[0])), Vec2f(float(dim[3]),float(dim[2])));

  Vec2i pictMin = pict->m_box.min(), pictSize = pict->m_box.size();
  if (pictSize.x() < 0 || pictSize.y() < 0) return -1;

  if (pictSize.x() > 3000 || pictSize.y() > 3000 ||
      pictMin.x() < -200 || pictMin.y() < -200) return -1;
  pict->m_dataPos = input->tell();

  zone = pict->m_pos;
  zone.setType("GraphEntry");

  pict->m_extra = f.str();
  if (!autoSend)
    pict->m_doNotSend=true;
  pict->m_fileId = int(m_state->m_zonesList.size());
  m_state->m_zonesList.push_back(pict);
  f.str("");
  f << "Entries(GraphEntry):" << *pict;

  ascFile.skipZone(pict->m_dataPos, pict->m_pos.end()-1);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  input->seek(pict->m_pos.end(), WPX_SEEK_SET);
  return pict->m_fileId;
}

// a list of picture
bool MSKGraph::readRB(MWAWInputStreamPtr input, MWAWEntry const &entry)
{
  if (entry.length() < 0x164) return false;
  entry.setParsed(true);
  libmwaw::DebugFile &ascFile = m_mainParser->ascii();
  libmwaw::DebugStream f;
  MSKGraphInternal::RBZone zone;
  zone.m_isMain = entry.name()=="RBDR";
  zone.m_id = entry.id();

  uint32_t page_offset = (uint32_t) entry.begin();
  long endOfPage = entry.end();

  input->seek(long(page_offset), WPX_SEEK_SET);
  f << input->readLong(4) << ", ";
  for (int i = 0; i < 4; i++) {
    long val = input->readLong(4);
    if (val) f << "#t" << i << "=" << val << ", ";
  }
  f << "type?=" << std::hex << input->readLong(2) << std::dec << ", ";
  f << "numPage=" << input->readLong(2) << ", ";
  for (int i = 0; i < 11; i++) {
    long val = input->readLong(2);
    if (i >= 8 && (val < -100 || val > 100)) f << "###";
    f << val << ", ";
  }
  f << ", unk=(";
  for (int i = 0; i < 2; i++)
    f << input->readLong(4) << ",";
  f << "), ";
  for (int i = 0; i < 9; i++) {
    long val = input->readLong(2);
    if (val) f << "#u" << i << "=" << val << ", ";
  }
  f << std::hex << "sz?=" << input->readLong(4) << std::dec << ", ";
  for (int i = 0; i < 2; i++) {
    long val = input->readLong(2);
    if (val) f << "#v" << i << "=" << val << ", ";
  }

  f << "unk1=(";
  for (int i = 0; i < 9; i++) {
    long val = input->readLong(2);
    if (val) f << val << ",";
    else f << "_,";
  }
  f << "), ";
  std::string oleName;
  while(input->tell() < long(page_offset)+0x162) {
    char val  = (char) input->readLong(1);
    if (val == 0) break;
    oleName+= val;
    if (oleName.length() > 30) break;
  }
  if (!oleName.empty()) {
    zone.m_frame = oleName;
    f << "ole='" << oleName << "', ";
  }

  int i = int(input->tell()-long(page_offset));
  if ((i%2) == 1) {
    int val = (int) input->readLong(1);
    if (val) f << "f" << i << "=" << val << ",";
    i++;
  }
  while (i != 0x162) {
    int val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
    i+=2;
  }
  int n = (int) input->readLong(2);
  f << "N= " << n;
  ascFile.addPos(long(page_offset));
  ascFile.addNote(f.str().c_str());

  if (n == 0) return true;

  input->pushLimit(endOfPage);
  while (input->tell()+20 < endOfPage) {
    long debPos = input->tell();
    size_t actId = m_state->m_zonesList.size();
    MWAWEntry pict;
    if (getEntryPicture(0, pict)<0 || input->tell() <= debPos) {
      f.str("");
      MWAW_DEBUG_MSG(("MSKGraph::readRB: oops can not read end of file\n"));
      f << "###" << entry.name();
      ascFile.addPos(debPos);
      ascFile.addNote(f.str().c_str());
      break;
    }
    for (size_t z = actId; z < m_state->m_zonesList.size(); z++) {
      shared_ptr<MSKGraphInternal::Zone> pictZone = m_state->m_zonesList[z];
      if (!pictZone) continue;
      zone.m_idList.push_back(int(z));
      if (!zone.m_isMain)
        pictZone->m_page = -2;
    }
  }
  input->popLimit();

  if (zone.m_idList.size() == 0) return false;
  int zId = zone.getId();
  if (m_state->m_RBsMap.find(zId) != m_state->m_RBsMap.end()) {
    MWAW_DEBUG_MSG(("MSKGraph::readRB: zone %d is already filled\n", zId));
    return false;
  }
  m_state->m_RBsMap[zId]=zone;
  checkTextBoxLinks(zId);
  return true;
}

void MSKGraph::checkTextBoxLinks(int zId)
{
  std::map<int, MSKGraphInternal::RBZone>::iterator rbIt = m_state->m_RBsMap.find(zId);
  if (rbIt==m_state->m_RBsMap.end())
    return;
  MSKGraphInternal::RBZone &rbZone=rbIt->second;
  std::vector<int> listIds = rbZone.m_idList;
  std::string const &fName = rbZone.m_frame;
  int numZones = int(m_state->m_zonesList.size());
  std::set<long> textIds;
  std::map<long,long> prevLinks, nextLinks;
  bool ok = true;
  for (size_t z = 0; z < listIds.size(); z++) {
    int id = listIds[z];
    if (id < 0 || id >= numZones) continue;
    shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[size_t(id)];
    if (zone->type() != MSKGraphInternal::Zone::Textv4)
      continue;
    reinterpret_cast<MSKGraphInternal::TextBoxv4 &>(*zone).m_frame = fName;
    if (textIds.find(zone->m_ids[0]) != textIds.end()) {
      MWAW_DEBUG_MSG(("MSKGraph::checkTextBoxLinks: id %lX already exists\n", zone->m_ids[0]));
      ok = false;
      break;
    }
    textIds.insert(zone->m_ids[0]);
    if (zone->m_ids[1]>0)
      prevLinks.insert(std::map<long,long>::value_type(zone->m_ids[0],zone->m_ids[1]));
    if (zone->m_ids[2]>0)
      nextLinks.insert(std::map<long,long>::value_type(zone->m_ids[0],zone->m_ids[2]));
  }
  size_t numLinks = nextLinks.size();
  for (std::map<long,long>::const_iterator link=nextLinks.begin();
       link!=nextLinks.end(); ++link) {
    if (prevLinks.find(link->second)==prevLinks.end() ||
        prevLinks.find(link->second)->second!=link->first) {
      MWAW_DEBUG_MSG(("MSKGraph::checkTextBoxLinks: can not find prevLinks: %lX<->%lX already exists\n", link->first, link->second));
      ok = false;
      break;
    }
    // check loops
    size_t w = 0;
    long actText = link->second;
    while (1) {
      if (nextLinks.find(actText)==nextLinks.end())
        break;
      actText = nextLinks.find(actText)->second;
      if (w++ > numLinks) {
        MWAW_DEBUG_MSG(("MSKGraph::checkTextBoxLinks:find a loop for id %lX\n", link->first));
        ok = false;
        break;
      }
    }
  }
  if (!ok) {
    MWAW_DEBUG_MSG(("MSKGraph::checkTextBoxLinks: problem find with text links\n"));
    for (size_t z = 0; z < m_state->m_zonesList.size(); z++) {
      shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[z];
      if (zone->type() != MSKGraphInternal::Zone::Textv4)
        continue;
      zone->m_ids[1] = zone->m_ids[2] = 0;
    }
  }
}

bool MSKGraph::readPictureV4(MWAWInputStreamPtr /*input*/, MWAWEntry const &entry)
{
  if (!entry.hasType("PICT")) {
    MWAW_DEBUG_MSG(("MSKGraph::readPictureV4: unknown type='%s'\n", entry.type().c_str()));
    return false;
  }
  entry.setParsed(true);

  MSKGraphInternal::Zone pict;
  pict.m_pos = entry;
  pict.m_dataPos = entry.begin();
  pict.m_page = -2;
  pict.m_zoneId = -1;

  MSKGraphInternal::DataPict *pct  = new MSKGraphInternal::DataPict(pict);
  shared_ptr<MSKGraphInternal::Zone>res(pct);
  m_mainParser->ascii().skipZone(entry.begin(), entry.end()-1);

  int zId = int(m_state->m_zonesList.size());
  res->m_fileId = zId;
  m_state->m_zonesList.push_back(res);

  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

// read/send a group
void MSKGraph::sendGroup(int id, MWAWPosition const &pos)
{
  if (id<0 || id>=int(m_state->m_zonesList.size()) || !m_state->m_zonesList[size_t(id)] ||
      m_state->m_zonesList[size_t(id)]->type()!=MSKGraphInternal::Zone::Group) {
    MWAW_DEBUG_MSG(("MSKGraph::sendGroup: can not find group %d\n", id));
    return;
  }
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) return;
  MSKGraphInternal::GroupZone &group=
    reinterpret_cast<MSKGraphInternal::GroupZone &>(*m_state->m_zonesList[size_t(id)]);
  group.m_isSent = true;

  MWAWGraphicListenerPtr graphicListener = m_parserState->m_graphicListener;
  if (!graphicListener || graphicListener->isDocumentStarted()) {
    MWAW_DEBUG_MSG(("MSKGraph::sendGroup: can not use the graphic listener\n"));
    MWAWPosition undefPos(pos);
    undefPos.setSize(Vec2f(0,0));
    for (size_t c=0; c < group.m_childs.size(); ++c)
      send(group.m_childs[c], undefPos);
    return;
  }
  if (!canCreateGraphic(group)) {
    if (pos.m_anchorTo == MWAWPosition::Char || pos.m_anchorTo == MWAWPosition::CharBaseLine) {
      shared_ptr<MSKGraphInternal::SubDocument> subdoc
      (new MSKGraphInternal::SubDocument(*this, m_mainParser->getInput(), MSKGraphInternal::SubDocument::Group, id));
      listener->insertTextBox(pos, subdoc);
      return;
    }
    MWAWPosition childPos(pos);
    childPos.setSize(Vec2f(0,0));
    sendGroupChild(id, childPos);
    return;
  }
  graphicListener->startGraphic(group.m_box);
  sendGroup(group, graphicListener);
  WPXBinaryData data;
  std::string type;
  if (graphicListener->endGraphic(data,type))
    listener->insertPicture(pos, data, type);
}

void MSKGraph::sendGroupChild(int id, MWAWPosition const &pos)
{
  if (id<0 || id>=int(m_state->m_zonesList.size()) || !m_state->m_zonesList[size_t(id)] ||
      m_state->m_zonesList[size_t(id)]->type()!=MSKGraphInternal::Zone::Group) {
    MWAW_DEBUG_MSG(("MSKGraph::sendGroupChild: can not find group %d\n", id));
    return;
  }
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  MWAWGraphicListenerPtr graphicListener = m_parserState->m_graphicListener;
  if (!listener || !graphicListener || graphicListener->isDocumentStarted()) return;
  MSKGraphInternal::GroupZone &group=
    reinterpret_cast<MSKGraphInternal::GroupZone &>(*m_state->m_zonesList[size_t(id)]);
  group.m_isSent = true;

  MWAWInputStreamPtr input=m_mainParser->getInput();
  size_t numZones=m_state->m_zonesList.size();
  size_t numChild=group.m_childs.size(), childNotSent=0;
  int numDataToMerge=0;
  Box2f partialBdBox;
  MWAWPosition partialPos(pos);
  for (size_t c=0; c < numChild; ++c) {
    int cId = group.m_childs[c];
    if (cId < 0 || cId >= int(numZones) || !m_state->m_zonesList[size_t(cId)])
      continue;
    MSKGraphInternal::Zone const &child=*(m_state->m_zonesList[size_t(cId)]);
    bool isLast=false;
    bool canMerge=false;
    if (child.type()==MSKGraphInternal::Zone::Shape || child.type()==MSKGraphInternal::Zone::Text) {
      Box2f origBdBox=child.getLocalBox();
      Vec2f decal = child.m_decal[0] + child.m_decal[1];
      Box2f localBdBox(origBdBox[0]+decal, origBdBox[1]+decal);
      if (numDataToMerge == 0)
        partialBdBox=localBdBox;
      else
        partialBdBox=partialBdBox.getUnion(localBdBox);
      canMerge=true;
    } else if (child.type()==MSKGraphInternal::Zone::Group &&
               canCreateGraphic(reinterpret_cast<MSKGraphInternal::GroupZone const &>(child))) {
      if (numDataToMerge == 0)
        partialBdBox=child.getLocalBox();
      else
        partialBdBox=partialBdBox.getUnion(child.getLocalBox());
      canMerge=true;
    }
    if (canMerge) {
      ++numDataToMerge;
      if (c+1 < numChild)
        continue;
      isLast=true;
    }

    if (numDataToMerge>1) {
      graphicListener->startGraphic(partialBdBox);
      size_t lastChild = isLast ? c : c-1;
      for (size_t ch=childNotSent; ch <= lastChild; ++ch) {
        int localCId = group.m_childs[ch];
        if (localCId < 0 || localCId >= int(numZones) || !m_state->m_zonesList[size_t(localCId)])
          continue;
        MSKGraphInternal::Zone const &localChild=*(m_state->m_zonesList[size_t(localCId)]);
        Box2f origBdBox=localChild.getLocalBox(false);
        Vec2f decal=localChild.m_decal[0]+localChild.m_decal[1];
        Box2f box(origBdBox[0]+decal,origBdBox[1]+decal);
        if (localChild.type()==MSKGraphInternal::Zone::Group)
          sendGroup(reinterpret_cast<MSKGraphInternal::GroupZone const &>(localChild), graphicListener);
        else if (localChild.type()==MSKGraphInternal::Zone::Shape) {
          MSKGraphInternal::BasicShape const &shape=reinterpret_cast<MSKGraphInternal::BasicShape const &>(localChild);
          graphicListener->insertPicture(box, shape.m_shape, shape.getStyle());
        } else if (localChild.type()==MSKGraphInternal::Zone::Text) {
          shared_ptr<MSKGraphInternal::SubDocument> subdoc
          (new MSKGraphInternal::SubDocument(const_cast<MSKGraph&>(*this), input, MSKGraphInternal::SubDocument::TextBox, localCId));
          // a textbox can not have border
          MWAWGraphicStyle style(localChild.m_style);
          style.m_lineWidth=0;
          graphicListener->insertTextBox(box, subdoc, style);
        }
      }
      WPXBinaryData data;
      std::string type;
      if (graphicListener->endGraphic(data,type)) {
        partialPos.setOrigin(pos.origin()+partialBdBox[0]-group.m_box[0]);
        partialPos.setSize(partialBdBox.size());
        listener->insertPicture(partialPos, data, type);
        if (isLast)
          break;
        childNotSent=c;
      }
    }
    // time to send back the data
    for ( ; childNotSent <= c; ++childNotSent)
      send(group.m_childs[childNotSent],pos);
    numDataToMerge=0;
  }
}

bool MSKGraph::canCreateGraphic(MSKGraphInternal::GroupZone const &group) const
{
  int numZones = int(m_state->m_zonesList.size());
  for (size_t c=0; c < group.m_childs.size(); ++c) {
    int cId = group.m_childs[c];
    if (cId < 0 || cId >= numZones || !m_state->m_zonesList[size_t(cId)])
      continue;
    MSKGraphInternal::Zone const &child=*(m_state->m_zonesList[size_t(cId)]);
    if (child.m_page!=group.m_page)
      return false;
    switch (child.type()) {
    case MSKGraphInternal::Zone::Shape:
    case MSKGraphInternal::Zone::Text:
      break;
    case MSKGraphInternal::Zone::Group:
      if (!canCreateGraphic(reinterpret_cast<MSKGraphInternal::GroupZone const &>(child)))
        return false;
      break;
    case MSKGraphInternal::Zone::Bitmap:
    case MSKGraphInternal::Zone::ChartZone:
    case MSKGraphInternal::Zone::OLE:
    case MSKGraphInternal::Zone::Pict:
    case MSKGraphInternal::Zone::TableZone:
    case MSKGraphInternal::Zone::Textv4:
    case MSKGraphInternal::Zone::Unknown:
    default:
      return false;
    }
  }
  return true;
}

void MSKGraph::sendGroup(MSKGraphInternal::GroupZone const &group, MWAWGraphicListenerPtr &listener) const
{
  if (!listener || !listener->isDocumentStarted()) {
    MWAW_DEBUG_MSG(("MSKGraph::sendGroup: the listener is bad\n"));
    return;
  }
  int numZones = int(m_state->m_zonesList.size());
  MWAWInputStreamPtr input=m_mainParser->getInput();
  for (size_t c=0; c < group.m_childs.size(); ++c) {
    int cId = group.m_childs[c];
    if (cId < 0 || cId >= numZones || !m_state->m_zonesList[size_t(cId)])
      continue;
    MSKGraphInternal::Zone const &child=*(m_state->m_zonesList[size_t(cId)]);
    Vec2f decal=child.m_decal[0]+child.m_decal[1];
    Box2f box(child.m_box[0]+decal,child.m_box[1]+decal);

    if (child.type()==MSKGraphInternal::Zone::Group)
      sendGroup(reinterpret_cast<MSKGraphInternal::GroupZone const &>(child), listener);
    else if (child.type()==MSKGraphInternal::Zone::Shape) {
      MSKGraphInternal::BasicShape const &shape=reinterpret_cast<MSKGraphInternal::BasicShape const &>(child);
      listener->insertPicture(box, shape.m_shape, shape.getStyle());
    } else if (child.type()==MSKGraphInternal::Zone::Text) {
      shared_ptr<MSKGraphInternal::SubDocument> subdoc
      (new MSKGraphInternal::SubDocument(const_cast<MSKGraph&>(*this), input, MSKGraphInternal::SubDocument::TextBox, cId));
      // a textbox can not have border
      MWAWGraphicStyle style(child.m_style);
      style.m_lineWidth=0;
      listener->insertTextBox(box, subdoc, style);
    } else {
      MWAW_DEBUG_MSG(("MSKGraph::sendGroup: find some unexpected child\n"));
    }
  }
}

shared_ptr<MSKGraphInternal::GroupZone> MSKGraph::readGroup(MSKGraphInternal::Zone &header)
{
  shared_ptr<MSKGraphInternal::GroupZone> group(new MSKGraphInternal::GroupZone(header));
  libmwaw::DebugStream f;
  MWAWInputStreamPtr input=m_mainParser->getInput();
  input->seek(header.m_dataPos, WPX_SEEK_SET);
  float dim[4];
  for (int i = 0; i < 4; i++) dim[i] = (float) input->readLong(4);
  group->m_box=Box2f(Vec2f(dim[0],dim[1]), Vec2f(dim[2],dim[3]));
  group->m_finalDecal=Vec2f(0,0);
  long ptr[2];
  for (int i = 0; i < 2; i++)
    ptr[i] = (long) input->readULong(4);
  f << "ptr0=" << std::hex << ptr[0] << std::dec << ",";
  if (ptr[0] != ptr[1])
    f << "ptr1=" << std::hex << ptr[1] << std::dec << ",";
  if (version() >= 3) {
    int val = (int) input->readULong(4);
    if (val) f << "g1=" << val << ",";
  }

  input->seek(header.m_pos.end()-2, WPX_SEEK_SET);
  int N = (int) input->readULong(2);
  for (int i = 0; i < N; i++) {
    long pos = input->tell();
    MWAWEntry childZone;
    int childId = getEntryPicture(header.m_zoneId, childZone, false);
    if (childId < 0) {
      MWAW_DEBUG_MSG(("MSKGraph::readGroup: can not find child\n"));
      input->seek(pos, WPX_SEEK_SET);
      f << "#child,";
      break;
    }
    group->m_childs.push_back(childId);
  }
  group->m_extra += f.str();
  group->m_pos.setEnd(input->tell());
  return group;
}

// read/send a textbox zone
bool MSKGraph::readText(MSKGraphInternal::TextBox &textBox)
{
  if (textBox.m_numPositions < 0) return false; // can an empty text exist

  libmwaw::DebugFile &ascFile = m_mainParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(SmallText):";
  MWAWInputStreamPtr input=m_mainParser->getInput();
  long pos = input->tell();
  if (!input->checkPosition(pos+4*(textBox.m_numPositions+1))) return false;

  // first read the set of (positions, font)
  f << "pos=[";
  int nbFonts = 0;
  for (int i = 0; i <= textBox.m_numPositions; i++) {
    int fPos = (int) input->readLong(2);
    int form = (int) input->readLong(2);
    f << fPos << ":" << form << ", ";

    if (fPos < 0 || form < -1) return false;
    if ((form == -1 && i != textBox.m_numPositions) ||
        (i && fPos < textBox.m_positions[(size_t) i-1])) {
      MWAW_DEBUG_MSG(("MSKGraph::readText: find odd positions\n"));
      f << "#";
      continue;
    }

    textBox.m_positions.push_back(fPos);
    textBox.m_formats.push_back(form);
    if (form >= nbFonts) nbFonts = form+1;
  }
  f << "] ";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos = input->tell();
  f.str("");
  f << "SmallText:Fonts ";

  // actualPos, -1, only exists if actualPos!= 0 ? We ignored it.
  input->readLong(2);
  if (input->readLong(2) != -1)
    input->seek(pos,WPX_SEEK_SET);
  else {
    ascFile.addPos(pos);
    ascFile.addNote("SmallText:char Pos");
    pos = input->tell();
  }
  f.str("");

  long endFontPos = input->tell();
  long sizeOfData = (long) input->readULong(4);
  int numFonts = (sizeOfData%0x12 == 0) ? int(sizeOfData/0x12) : 0;

  if (numFonts >= nbFonts) {
    endFontPos = input->tell()+4+sizeOfData;

    ascFile.addPos(pos);
    ascFile.addNote("SmallText: Fonts");

    for (int i = 0; i < numFonts; i++) {
      pos = input->tell();
      MWAWFont font;
      if (!readFont(font)) {
        input->seek(endFontPos, WPX_SEEK_SET);
        break;
      }
      textBox.m_fontsList.push_back(font);

      f.str("");
      f << "SmallText:Font"<< i
        << "(" << font.getDebugString(m_parserState->m_fontConverter) << "),";

      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      pos = input->tell();
    }
  }
  int nChar = textBox.m_positions.back()-1;
  if (nbFonts > int(textBox.m_fontsList.size())) {
    MWAW_DEBUG_MSG(("MSKGraph::readText: can not read the fonts\n"));
    ascFile.addPos(pos);
    ascFile.addNote("SmallText:###");
    input->seek(endFontPos,WPX_SEEK_SET);
    textBox.m_fontsList.resize(0);
    textBox.m_positions.resize(0);
    textBox.m_numPositions = 0;
  }

  // now, syntax is : long(size) + size char
  //      - 0x16 - 0 - 0 - Fonts (default fonts)
  //      - 0x08 followed by two long, maybe interesting to look
  //      - 0x0c (or 0x18) seems followed by small int
  //      - nbChar : the strings (final)

  f.str("");
  f << "SmallText:";
  while(1) {
    if (input->atEOS()) return false;

    pos = input->tell();
    sizeOfData = (long) input->readULong(4);
    if (sizeOfData == nChar) {
      bool ok = true;
      // ok we try to read the string
      std::string chaine("");
      for (int i = 0; i < sizeOfData; i++) {
        unsigned char c = (unsigned char)input->readULong(1);
        if (c == 0) {
          ok = false;
          break;
        }
        chaine += (char) c;
      }

      if (!ok)
        input->seek(pos+4,WPX_SEEK_SET);
      else {
        textBox.m_text = chaine;
        f << "=" << chaine;
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        return true;
      }
    }

    if (sizeOfData <= 100+nChar && (sizeOfData%2==0) ) {
      if (input->seek(sizeOfData, WPX_SEEK_CUR) != 0) return false;
      f << "#";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      f.str("");
      f << "SmallText:Text";
      continue;
    }

    // fixme: we can try to find the next string
    MWAW_DEBUG_MSG(("MSKGraph::readText: problem reading text\n"));
    f << "#";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    return false;
  }
  return true;
}

bool MSKGraph::readFont(MWAWFont &font)
{
  int vers = version();
  MWAWInputStreamPtr input=m_mainParser->getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;
  if (!input->checkPosition(pos+18))
    return false;
  font = MWAWFont();
  for (int i = 0; i < 3; i++) {
    int val = (int) input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  font.setFont((int) input->readULong(2));
  int flags = (int) input->readULong(1);
  uint32_t flag = 0;
  if (flags & 0x1) flag |= MWAWFont::boldBit;
  if (flags & 0x2) flag |= MWAWFont::italicBit;
  if (flags & 0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
  if (flags & 0x8) flag |= MWAWFont::embossBit;
  if (flags & 0x10) flag |= MWAWFont::shadowBit;
  if (flags & 0x20) {
    if (vers==1)
      font.set(MWAWFont::Script(20,WPX_PERCENT,80));
    else
      font.set(MWAWFont::Script::super100());
  }
  if (flags & 0x40) {
    if (vers==1)
      font.set(MWAWFont::Script(-20,WPX_PERCENT,80));
    else
      font.set(MWAWFont::Script::sub100());
  }
  if (flags & 0x80) f << "#smaller,";
  font.setFlags(flag);

  int val = (int) input->readULong(1);
  if (val) f << "#flags2=" << val << ",";
  font.setSize((float) input->readULong(2));

  unsigned char color[3];
  for (int i = 0; i < 3; i++) color[i] = (unsigned char) (input->readULong(2)>>8);
  font.setColor(MWAWColor(color[0],color[1],color[2]));
  font.m_extra = f.str();
  return true;
}

void MSKGraph::sendTextBox(int zoneId)
{
  MWAWGraphicListenerPtr listener=m_parserState->m_graphicListener;
  if (!listener || !listener->canWriteText()) {
    MWAW_DEBUG_MSG(("MSKGraph::sendTextBox: can not find get access to the graphicListener\n"));
    return;
  }
  if (zoneId < 0 || zoneId >= int(m_state->m_zonesList.size())) {
    MWAW_DEBUG_MSG(("MSKGraph::sendTextBox: can not find textbox %d\n", zoneId));
    return;
  }
  shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[(size_t)zoneId];
  if (!zone) return;
  MSKGraphInternal::TextBox &textBox = reinterpret_cast<MSKGraphInternal::TextBox &>(*zone);
  listener->setFont(MWAWFont(20,12));
  MWAWParagraph para;
  para.m_justify=textBox.m_justify;
  listener->setParagraph(para);
  int numFonts = int(textBox.m_fontsList.size());
  int actFormatPos = 0;
  int numFormats = int(textBox.m_formats.size());
  if (numFormats != int(textBox.m_positions.size())) {
    MWAW_DEBUG_MSG(("MSKGraph::sendTextBox: positions and formats have different length\n"));
    if (numFormats > int(textBox.m_positions.size()))
      numFormats = int(textBox.m_positions.size());
  }
  for (size_t i = 0; i < textBox.m_text.length(); i++) {
    if (actFormatPos < numFormats && textBox.m_positions[(size_t)actFormatPos]==int(i)) {
      int id = textBox.m_formats[(size_t)actFormatPos++];
      if (id < 0 || id >= numFonts) {
        MWAW_DEBUG_MSG(("MSKGraph::sendTextBox: can not find a font\n"));
      } else
        listener->setFont(textBox.m_fontsList[(size_t)id]);
    }
    unsigned char c = (unsigned char) textBox.m_text[i];
    switch(c) {
    case 0x9:
      MWAW_DEBUG_MSG(("MSKGraph::sendTextBox: find some tab\n"));
      listener->insertChar(' ');
      break;
    case 0xd:
      if (i+1 != textBox.m_text.length())
        listener->insertEOL();
      break;
    case 0x19:
      listener->insertField(MWAWField(MWAWField::Title));
      break;
    case 0x18:
      listener->insertField(MWAWField(MWAWField::PageNumber));
      break;
    case 0x16:
      MWAW_DEBUG_MSG(("MSKGraph::sendTextBox: find some time\n"));
      listener->insertField(MWAWField(MWAWField::Time));
      break;
    case 0x17:
      MWAW_DEBUG_MSG(("MSKGraph::sendTextBox: find some date\n"));
      listener->insertField(MWAWField(MWAWField::Date));
      break;
    case 0x14: // fixme
      MWAW_DEBUG_MSG(("MSKGraph::sendTextBox: footnote are not implemented\n"));
      break;
    default:
      listener->insertCharacter(c);
      break;
    }
  }
}

void MSKGraph::send(int id, MWAWPosition const &pos)
{
  if (id < 0 || id >= int(m_state->m_zonesList.size())) {
    MWAW_DEBUG_MSG(("MSKGraph::send: can not find zone %d\n", id));
    return;
  }
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) return;
  shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[(size_t)id];
  zone->m_isSent = true;

  MWAWPosition pictPos(pos);
  if (pos.size()[0]<=0 || pos.size()[1]<=0)
    pictPos = zone->getPosition(pos.m_anchorTo);
  if (pictPos.m_anchorTo == MWAWPosition::Page)
    pictPos.setOrigin(pictPos.origin()+72.*m_mainParser->getPageLeftTop());
  WPXPropertyList extras;
  zone->fillFramePropertyList(extras);

  MWAWInputStreamPtr input=m_mainParser->getInput();
  MWAWGraphicListenerPtr graphicListener=m_parserState->m_graphicListener;
  if ((zone->type()==MSKGraphInternal::Zone::Shape || zone->type()==MSKGraphInternal::Zone::Text) &&
      (!graphicListener || graphicListener->isDocumentStarted())) {
    MWAW_DEBUG_MSG(("MSKGraph::send: can not use the graphic listener for zone %d\n", id));
    shared_ptr<MSKGraphInternal::SubDocument> subdoc
    (new MSKGraphInternal::SubDocument(*this, input, MSKGraphInternal::SubDocument::Empty, id));
    listener->insertTextBox(pictPos, subdoc, extras);
    return;
  }
  switch (zone->type()) {
  case MSKGraphInternal::Zone::Text: {
    MSKGraphInternal::TextBox &textbox = reinterpret_cast<MSKGraphInternal::TextBox &>(*zone);
    Box2f box(Vec2f(0,0),textbox.m_box.size());
    graphicListener->startGraphic(box);
    shared_ptr<MSKGraphInternal::SubDocument> subdoc
    (new MSKGraphInternal::SubDocument(*this, input, MSKGraphInternal::SubDocument::TextBox, id));
    // a textbox can not have border
    MWAWGraphicStyle style(textbox.m_style);
    style.m_lineWidth=0;
    graphicListener->insertTextBox(box, subdoc, style);
    WPXBinaryData data;
    std::string type;
    if (graphicListener->endGraphic(data, type))
      listener->insertPicture(pictPos, data, type);
    return;
  }
  case MSKGraphInternal::Zone::TableZone: {
    MSKGraphInternal::Table &table = reinterpret_cast<MSKGraphInternal::Table &>(*zone);
    shared_ptr<MSKGraphInternal::SubDocument> subdoc
    (new MSKGraphInternal::SubDocument(*this, input, MSKGraphInternal::SubDocument::Table, table.m_tableId));
    listener->insertTextBox(pictPos, subdoc, extras);
    return;
  }
  case MSKGraphInternal::Zone::ChartZone: {
    MSKGraphInternal::Chart &chart = reinterpret_cast<MSKGraphInternal::Chart &>(*zone);
    shared_ptr<MSKGraphInternal::SubDocument> subdoc
    (new MSKGraphInternal::SubDocument(*this, input, MSKGraphInternal::SubDocument::Chart, chart.m_chartId));
    listener->insertTextBox(pictPos, subdoc, extras);
    return;
  }
  case MSKGraphInternal::Zone::Group:
    sendGroup(id, pictPos);
    return;
  case MSKGraphInternal::Zone::Bitmap: {
    MSKGraphInternal::DataBitmap &bmap = reinterpret_cast<MSKGraphInternal::DataBitmap &>(*zone);
    WPXBinaryData data;
    std::string type;
    if (!bmap.getPictureData(input, data,type,m_mainParser->getPalette(4)))
      break;
    m_mainParser->ascii().skipZone(bmap.m_dataPos, bmap.m_pos.end()-1);
    listener->insertPicture(pictPos, data, type, extras);
    return;
  }
  case MSKGraphInternal::Zone::Shape: {
    MSKGraphInternal::BasicShape &shape = reinterpret_cast<MSKGraphInternal::BasicShape &>(*zone);
    listener->insertPicture(pictPos, shape.m_shape, shape.getStyle());
    return;
  }
  case MSKGraphInternal::Zone::Pict: {
    WPXBinaryData data;
    std::string type;
    if (!zone->getBinaryData(input, data,type))
      break;
    listener->insertPicture(pictPos, data, type, extras);
    return;
  }
  case MSKGraphInternal::Zone::Textv4: {
    MSKGraphInternal::TextBoxv4 &textbox = reinterpret_cast<MSKGraphInternal::TextBoxv4 &>(*zone);
    shared_ptr<MSKGraphInternal::SubDocument> subdoc
    (new MSKGraphInternal::SubDocument(*this, input, MSKGraphInternal::SubDocument::TextBoxv4, textbox.m_text, textbox.m_frame));
    WPXPropertyList textboxExtra;
    if (zone->m_ids[1] > 0) {
      WPXString fName;
      fName.sprintf("Frame%ld", zone->m_ids[0]);
      extras.insert("libwpd:frame-name",fName);
    }
    if (zone->m_ids[2] > 0) {
      WPXString fName;
      fName.sprintf("Frame%ld", zone->m_ids[2]);
      textboxExtra.insert("libwpd:next-frame-name",fName);
    }
    listener->insertTextBox(pictPos, subdoc, extras, textboxExtra);
    return;
  }
  case MSKGraphInternal::Zone::OLE: {
    MSKGraphInternal::OLEZone &ole = reinterpret_cast<MSKGraphInternal::OLEZone &>(*zone);
    m_mainParser->sendOLE(ole.m_oleId, pictPos, extras);
    return;
  }
  case MSKGraphInternal::Zone::Unknown:
  default:
    break;
  }

  MWAW_DEBUG_MSG(("MSKGraph::send: can not send zone %d\n", id));
}

void MSKGraph::sendAll(int zoneId, bool mainZone)
{
  MWAWPosition undefPos;
  for (size_t i = 0; i < m_state->m_zonesList.size(); i++) {
    shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[i];
    if (zoneId >= 0 && zoneId!=zone->m_zoneId)
      continue;
    if (zone->m_doNotSend || (zone->m_isSent && mainZone))
      continue;
    undefPos.m_anchorTo = mainZone ? MWAWPosition::Page : MWAWPosition::Paragraph;
    send(int(i), undefPos);
  }
}

void MSKGraph::sendObjects(MSKGraph::SendData what)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("MSKGraph::sendObjects: listener is not set\n"));
    return;
  }

  bool first = true;
  int numZones = int(m_state->m_zonesList.size());
  std::vector<int> listIds;
  MSKGraphInternal::RBZone *rbZone=0;
  switch(what.m_type) {
  case MSKGraph::SendData::ALL: {
    listIds.resize(size_t(numZones));
    for (int i = 0; i < numZones; i++) listIds[size_t(i)]=i;
    break;
  }
  case MSKGraph::SendData::RBDR:
  case MSKGraph::SendData::RBIL: {
    int zId = what.m_type==MSKGraph::SendData::RBDR ? -1 : what.m_id;
    if (m_state->m_RBsMap.find(zId)!=m_state->m_RBsMap.end())
      rbZone = &m_state->m_RBsMap.find(zId)->second;
    break;
  }
  default:
    break;
  }
  if (rbZone)
    listIds=rbZone->m_idList;
  if (what.m_type==MSKGraph::SendData::RBIL) {
    if (!rbZone) {
      MWAW_DEBUG_MSG(("MSKGraph::sendObjects: can find RBIL zone %d\n", what.m_id));
      return;
    }
    if (listIds.size() != 1) {
      if (what.m_anchor == MWAWPosition::Char ||
          what.m_anchor == MWAWPosition::CharBaseLine) {
        shared_ptr<MSKGraphInternal::SubDocument> subdoc
        (new MSKGraphInternal::SubDocument(*this, m_mainParser->getInput(), MSKGraphInternal::SubDocument::RBILZone, what.m_id));
        MWAWPosition pictPos(Vec2f(0,0), what.m_size, WPX_POINT);
        pictPos.setRelativePosition(MWAWPosition::Char,
                                    MWAWPosition::XLeft, MWAWPosition::YTop);
        pictPos.m_wrapping =  MWAWPosition::WBackground;
        listener->insertTextBox(pictPos, subdoc);
        return;
      }
    }
  }
  MWAWPosition undefPos;
  undefPos.m_anchorTo = what.m_anchor;
  for (size_t i = 0; i < listIds.size(); i++) {
    int id = listIds[i];
    if (id < 0 || id >= numZones) continue;
    shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[size_t(id)];
    if (!zone || zone->m_doNotSend) continue;
    if (zone->m_isSent) {
      if (what.m_type == MSKGraph::SendData::ALL ||
          what.m_anchor == MWAWPosition::Page) continue;
    }
    if (what.m_anchor == MWAWPosition::Page) {
      if (what.m_page > 0 && zone->m_page+1 != what.m_page) continue;
      else if (what.m_page==0 && zone->m_page < 0) continue;
    }

    if (first) {
      first = false;
      if (what.m_anchor == MWAWPosition::Page && (!listener->isSectionOpened() && !listener->isParagraphOpened()))
        listener->insertChar(' ');
    }
    send(int(id), undefPos);
  }
}

void MSKGraph::flushExtra()
{
  MWAWPosition undefPos;
  undefPos.m_anchorTo=MWAWPosition::Char;
  for (size_t i = 0; i < m_state->m_zonesList.size(); i++) {
    shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[i];
    if (!zone || zone->m_isSent || zone->m_doNotSend) continue;
    send(int(i), undefPos);
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
