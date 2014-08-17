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

#include "MacDrawParser.hxx"

/** Internal: the structures of a MacDrawParser */
namespace MacDrawParserInternal
{
// generic class used to store shape in MWAWDrawParser
struct Shape {
  //! the different shape
  enum Type { Basic, Bitmap, Group, GroupEnd, Text, Unknown };

  //! constructor
  Shape() : m_type(Unknown), m_box(), m_style(), m_shape(), m_id(-1), m_nextId(-1),
    m_font(), m_paragraph(), m_textEntry(), m_childList(),
    m_numBytesByRow(0), m_bitmapDim(), m_bitmapFileDim(), m_bitmapEntry(), m_isSent(false)
  {
  }

  //! return the shape bdbox
  Box2f getBdBox() const
  {
    return m_type==Basic ? m_shape.getBdBox() : m_box;
  }
  //! the graphic type
  Type m_type;
  //! the shape bdbox
  Box2f m_box;
  //! the graphic style
  MWAWGraphicStyle m_style;
  //! the graphic shape ( for basic geometric form )
  MWAWGraphicShape m_shape;
  //! the shape id
  int m_id;
  //! the following id (if set)
  int m_nextId;
  //! the font ( for a text box)
  MWAWFont m_font;
  //! the paragraph ( for a text box)
  MWAWParagraph m_paragraph;
  //! the textbox entry (main text)
  MWAWEntry m_textEntry;
  //! the child list ( for a group )
  std::vector<size_t> m_childList;
  //! the number of bytes by row (for a bitmap)
  int m_numBytesByRow;
  //! the bitmap dimension (in page)
  Box2i m_bitmapDim;
  //! the bitmap dimension (in the file)
  Box2i m_bitmapFileDim;
  //! the bitmap entry (data)
  MWAWEntry m_bitmapEntry;
  //! a flag used to know if the object is sent to the listener or not
  mutable bool m_isSent;
};

////////////////////////////////////////
//! Internal: the state of a MacDrawParser
struct State {
  //! constructor
  State() : m_version(0), m_patternList(), m_shapeList()
  {
  }
  //! returns a pattern if posible
  bool getPattern(int id, MWAWGraphicStyle::Pattern &pat)
  {
    if (m_patternList.empty()) initPatterns();
    if (id<=0 || id>int(m_patternList.size())) {
      MWAW_DEBUG_MSG(("MacDrawParserInternal::getPattern: can not find pattern %d\n", id));
      return false;
    }
    pat=m_patternList[size_t(id-1)];
    return true;
  }
  //! init the patterns list
  void initPatterns();
  //! the file version
  int m_version;
  //! the patterns list
  std::vector<MWAWGraphicStyle::Pattern> m_patternList;
  //! the shape list
  std::vector<Shape> m_shapeList;
};

void State::initPatterns()
{
  if (!m_patternList.empty()) return;
  for (int i=0; i<35; ++i) {
    static uint16_t const(patterns0[]) = {
      0x0000,0x0000,0x0000,0x0000, 0xffff,0xffff,0xffff,0xffff,
      0xbbee,0xbbee,0xbbee,0xbbee, 0x55aa,0x55aa,0x55aa,0x55aa,
      0x8822,0x8822,0x8822,0x8822, 0x8800,0x2200,0x8800,0x2200,
      0x8000,0x0800,0x8000,0x0800, 0x0800,0x0000,0x8000,0x0000,
      0x8080,0x413e,0x0808,0x14e3, 0x081c,0x2241,0x8001,0x0204,
      0xff80,0x8080,0xff08,0x0808, 0x0180,0x4020,0x1008,0x0402,
      0x81c0,0x6030,0x180c,0x0603, 0x1188,0x4400,0x1188,0x4400,
      0x1188,0x4422,0x1188,0x4422, 0x3399,0xcc66,0x3399,0xcc66,
      0x0180,0x4000,0x0204,0x0800, 0x6600,0x0000,0x9900,0x0000,
      0xff00,0x0000,0xff00,0x0000, 0x5020,0x2020,0x5088,0x2788,
      0x849f,0x8080,0x0404,0xe784, 0x0101,0x01ff,0x0101,0x01ff,
      0x5588,0x5522,0x5588,0x5522, 0x8001,0x0204,0x0810,0x2040,
      0xc081,0x0306,0x0c18,0x3060, 0x8811,0x2200,0x8811,0x2200,
      0x8811,0x2244,0x8811,0x2244, 0xcc99,0x3366,0xcc99,0x3366,
      0x2050,0x0000,0x0205,0x0000, 0x0808,0x0808,0x0808,0x0808,
      0x0404,0x4040,0x0404,0x4040, 0x0384,0x4830,0x0c02,0x0101,
      0x0a11,0xa040,0x00b1,0x4a4a, 0x4040,0x40ff,0x4040,0x4040,
      0x4122,0x1408,0x1422,0x4180
    };
    static uint16_t const(patterns1[]) = {
      0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff,
      0x77dd, 0x77dd, 0x77dd, 0x77dd, 0xaa55, 0xaa55, 0xaa55, 0xaa55,
      0x8822, 0x8822, 0x8822, 0x8822, 0x8800, 0x2200, 0x8800, 0x2200,
      0x8000, 0x0800, 0x8000, 0x0800, 0x8000, 0x0000, 0x0800, 0x0000,
      0x8080, 0x413e, 0x0808, 0x14e3, 0xff80, 0x8080, 0xff08, 0x0808,
      0x8142, 0x2418, 0x8142, 0x2418, 0x8040, 0x2010, 0x0804, 0x0201,
      0xe070, 0x381c, 0x0e07, 0x83c1, 0x77bb, 0xddee, 0x77bb, 0xddee,
      0x8844, 0x2211, 0x8844, 0x2211, 0x99cc, 0x6633, 0x99cc, 0x6633,
      0x2040, 0x8000, 0x0804, 0x0200, 0xff00, 0xff00, 0xff00, 0xff00,
      0xff00, 0x0000, 0xff00, 0x0000, 0xcc00, 0x0000, 0x3300, 0x0000,
      0xf0f0, 0xf0f0, 0x0f0f, 0x0f0f, 0xff88, 0x8888, 0xff88, 0x8888,
      0xaa44, 0xaa11, 0xaa44, 0xaa11, 0x0102, 0x0408, 0x1020, 0x4080,
      0x8307, 0x0e1c, 0x3870, 0xe0c1, 0xeedd, 0xbb77, 0xeedd, 0xbb77,
      0x1122, 0x4488, 0x1122, 0x4488, 0x3366, 0xcc99, 0x3366, 0xcc99,
      0x40a0, 0x0000, 0x040a, 0x0000, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa,
      0x8888, 0x8888, 0x8888, 0x8888, 0x0101, 0x1010, 0x0101, 0x1010,
      0x0008, 0x142a, 0x552a, 0x1408, 0xff80, 0x8080, 0x8080, 0x8080,
      0x8244, 0x2810, 0x2844, 0x8201
    };
    MWAWGraphicStyle::Pattern pat;
    pat.m_dim=Vec2i(8,8);
    pat.m_data.resize(8);
    pat.m_colors[0]=MWAWColor::white();
    pat.m_colors[1]=MWAWColor::black();

    uint16_t const *patPtr=m_version==0 ? &patterns0[4*i] : &patterns1[4*i];
    for (size_t j=0; j<8; j+=2, ++patPtr) {
      pat.m_data[j]=uint8_t((*patPtr)>>8);
      pat.m_data[j+1]=uint8_t((*patPtr)&0xFF);
    }
    if (i==0) m_patternList.push_back(pat); // none pattern
    m_patternList.push_back(pat);
  }
}

////////////////////////////////////////
//! Internal: the subdocument of a MacDrawParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MacDrawParser &pars, MWAWInputStreamPtr input, int zoneId) : MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const
  {
    if (MWAWSubDocument::operator!=(doc)) return true;
    SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
    if (!sDoc) return true;
    if (m_id != sDoc->m_id) return true;
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
  //! the subdocument id
  int m_id;
private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType)
{
  if (!listener || !listener->canWriteText()) {
    MWAW_DEBUG_MSG(("MacDrawParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  MacDrawParser *parser=dynamic_cast<MacDrawParser *>(m_parser);
  if (!m_parser) {
    MWAW_DEBUG_MSG(("MacDrawParserInternal::SubDocument::parse: no parser\n"));
    return;
  }
  long pos = m_input->tell();
  parser->sendText(m_id);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}


}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MacDrawParser::MacDrawParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWGraphicParser(input, rsrcParser, header), m_state()
{
  init();
}

MacDrawParser::~MacDrawParser()
{
}

void MacDrawParser::init()
{
  resetGraphicListener();
  setAsciiName("main-1");

  m_state.reset(new MacDrawParserInternal::State);

  getPageSpan().setMargins(0.1);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MacDrawParser::parse(librevenge::RVNGDrawingInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = false;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());
    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      for (size_t i=0; i<m_state->m_shapeList.size(); ++i) {
        MacDrawParserInternal::Shape const &shape=m_state->m_shapeList[i];
        if (shape.m_isSent) continue;
        send(shape);
        if (shape.m_nextId>0 && shape.m_nextId>int(i))
          i=size_t(shape.m_nextId-1);
      }
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MacDrawParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetGraphicListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MacDrawParser::createDocument(librevenge::RVNGDrawingInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getGraphicListener()) {
    MWAW_DEBUG_MSG(("MacDrawParser::createDocument: listener already exist\n"));
    return;
  }

  // create the page list
  MWAWPageSpan ps(getPageSpan());
  ps.setPageSpan(1);
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
bool MacDrawParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  readPrefs();
  input->seek(512,librevenge::RVNG_SEEK_SET);
  long pos=input->tell();
  while (!input->isEnd()) {
    if (readObject()<0)
      break;
    pos=input->tell();
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  if (!input->isEnd()) {
    pos=input->tell();
    MWAW_DEBUG_MSG(("MacDrawParser::createZones: find extra data\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Data):##");
  }
  return !m_state->m_shapeList.empty();
}

int MacDrawParser::readObject()
{
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd()) return false;
  long pos=input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Object):";

  if (!input->checkPosition(pos+4+4)) {
    MWAW_DEBUG_MSG(("MacDrawParser::readObject: the zone seems to small\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return -1;
  }
  size_t shapeId= m_state->m_shapeList.size();
  m_state->m_shapeList.push_back(MacDrawParserInternal::Shape());
  MacDrawParserInternal::Shape &shape=m_state->m_shapeList.back();
  shape.m_id=int(shapeId);
  shape.m_nextId=shape.m_id+1; // default value
  int type=(int) input->readULong(1);
  switch (type) {
  case 0:
    shape.m_type=MacDrawParserInternal::Shape::GroupEnd;
    f << "end[group],";
    break;
  case 1:
    shape.m_type=MacDrawParserInternal::Shape::Text;
    f << "text,";
    break;
  case 2:
    shape.m_type=MacDrawParserInternal::Shape::Basic;
    shape.m_shape.m_type=MWAWGraphicShape::Line;
    f << "line[axis],";
    break;
  case 3:
    shape.m_type=MacDrawParserInternal::Shape::Basic;
    shape.m_shape.m_type=MWAWGraphicShape::Line;
    f << "line,";
    break;
  case 4:
    shape.m_type=MacDrawParserInternal::Shape::Basic;
    shape.m_shape.m_type=MWAWGraphicShape::Rectangle;
    f << "rect,";
    break;
  case 5:
    shape.m_type=MacDrawParserInternal::Shape::Basic;
    shape.m_shape.m_type=MWAWGraphicShape::Rectangle;
    f << "roundrect,";
    break;
  case 6:
    shape.m_type=MacDrawParserInternal::Shape::Basic;
    shape.m_shape.m_type=MWAWGraphicShape::Circle;
    f << "circle,";
    break;
  case 7:
    shape.m_type=MacDrawParserInternal::Shape::Basic;
    shape.m_shape.m_type=MWAWGraphicShape::Arc;
    f << "arc,";
    break;
  case 8:
    shape.m_type=MacDrawParserInternal::Shape::Basic;
    shape.m_shape.m_type=MWAWGraphicShape::Polygon;
    f << "poly[free],";
    break;
  case 9:
    shape.m_type=MacDrawParserInternal::Shape::Basic;
    shape.m_shape.m_type=MWAWGraphicShape::Polygon;
    f << "poly,";
    break;
  case 10:
    shape.m_type=MacDrawParserInternal::Shape::Group;
    f << "begin[group],";
    break;
  case 11:
    shape.m_type=MacDrawParserInternal::Shape::Bitmap;
    f << "bitmap,";
    break;
  default:
    MWAW_DEBUG_MSG(("MacDrawParser::readObject: find unknown object type %d\n", type));
    f << "#type=" << type << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_state->m_shapeList.pop_back();
    return -1;
  }
  int val=(int) input->readULong(1);
  if (val==1) f << "locked,";
  else if (val) f << "#lock=" << val << ",";
  val=(int) input->readLong(2);
  if (val) f << "f0=" << val << ",";

  int lineType=(int) input->readULong(1);
  if (lineType>=1 && lineType<6) {
    static float const(widths[]) = { 0, 1, 2, 3.5f, 5 };
    shape.m_style.m_lineWidth=widths[lineType-1];
    if (lineType!=2) // default
      f << "line[width]=" << widths[lineType-1] << ",";
  }
  else {
    MWAW_DEBUG_MSG(("MacDrawParser::readObject: find unexpected line type\n"));
    f << "#line[width]=" << lineType << ",";
  }
  int linePat=(int) input->readULong(1);
  if (linePat>=1&&linePat<37) {
    if (linePat==1)
      shape.m_style.m_lineWidth=0; // no pattern
    else {
      MWAWGraphicStyle::Pattern pat;
      if (m_state->getPattern(linePat, pat))
        pat.getAverageColor(shape.m_style.m_lineColor);
      else
        f << "###";
    }
    f << "line[pat]=" << linePat << ",";
  }
  else {
    MWAW_DEBUG_MSG(("MacDrawParser::readObject: find unexpected line pattern\n"));
    f << "#line[pat]=" << linePat << ",";
  }
  int surfPat=(int) input->readULong(1);
  if (surfPat>=1&&surfPat<37) {
    if (surfPat!=1) {
      f << "surf[pat]=" << surfPat << ",";
      MWAWGraphicStyle::Pattern pat;
      if (m_state->getPattern(surfPat, pat)) {
        MWAWColor col;
        if (pat.getUniqueColor(col))
          shape.m_style.setSurfaceColor(col);
        else
          shape.m_style.m_pattern=pat;
      }
      else
        f << "###surf[pat],";
    }
  }
  else {
    MWAW_DEBUG_MSG(("MacDrawParser::readObject: find unexpected surface pattern\n"));
    f << "#surf[pat]=" << surfPat << ",";
  }
  val=(int) input->readULong(1);
  float cornerWidth=0;
  if (type==2 || type==3) {
    if (val&2) {
      shape.m_style.m_arrows[0]=true;
      f << "arrow[beg],";
    }
    if (val&1) {
      shape.m_style.m_arrows[1]=true;
      f << "arrow[end],";
    }
    val &= 0xFC;
  }
  else if (type==4 || type==5) {
    if (val>=1 && val <= 5) {
      cornerWidth=float(val+1)*4.5f;
      f << "corner[length]=" << cornerWidth << ",";
      val=0;
    }
    else if (val) {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: find unknown corner values\n"));
      f << "###";
    }
  }
  if (val) f << "#flags=" << std::hex << val << std::dec << ",";
  bool ok=true;
  long actPos=input->tell();
  switch (type) {
  case 0:
    break;
  case 1: {
    if (!input->checkPosition(actPos+20)) {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: the zone length seems bad\n"));
      f << "###";
      ok=false;
      break;
    }
    long id=(long) input->readULong(4);
    if (id) f << "id=" << std::hex << id << std::dec << ",";

    MWAWFont &font=shape.m_font;
    int flag = (int) input->readULong(1);
    uint32_t flags=0;
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0x8) flags |= MWAWFont::embossBit;
    if (flag&0x10) flags |= MWAWFont::shadowBit;
    if (flag&0x20) font.setDeltaLetterSpacing(-1);
    if (flag&0x40) font.setDeltaLetterSpacing(1);
    if (flag&0x80) f << "#flag0[0x80],";
    font.setFlags(flags);
    font.setId((int) input->readULong(1));
    int fSz=(int) input->readULong(1);
    if (fSz>=1 && fSz<9) {
      static int const fontSize[]= {9,10,12,14,18,24,36,48};
      font.setSize((float) fontSize[fSz-1]);
    }
    else {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: find unknown font size\n"));
      f << "#sz=" << fSz << ",";
    }
    f << "font=[" << font.getDebugString(getParserState()->m_fontConverter) << "],";
    val=(int) input->readULong(1);
    switch (val) {
    case 1:
      break;
    case 2:
      shape.m_paragraph.setInterline(1.5, librevenge::RVNG_PERCENT);
      f << "interline=150%,";
      break;
    case 3:
      shape.m_paragraph.setInterline(2, librevenge::RVNG_PERCENT);
      f << "interline=200%,";
      break;
    default:
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: find unknown interline\n"));
      f << "#interline=" << val << ",";
    }
    val=(int) input->readULong(1);
    switch (val) {
    case 1:
      break;
    case 2:
      shape.m_paragraph.m_justify = MWAWParagraph::JustificationCenter;
      f << "align=center,";
      break;
    case 3:
      shape.m_paragraph.m_justify = MWAWParagraph::JustificationRight;
      f << "align=right,";
      break;
    default:
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: find unknown align\n"));
      f << "#align=" << val << ",";
    }
    val=(int) input->readULong(1);
    if (val&3) {
      int rotation=90*(val&3);
      if (val&4) {
        if (rotation==90) rotation=180;
        else if (rotation==180) rotation=90;
      }
      shape.m_style.m_rotate=float(rotation);
      f << "rot=" << rotation << ",";
    }
    if (val&4) {
      shape.m_style.m_flip[0]=true;
      f << "sym,";
    }
    if (val&0xF8) f << "#rot=" << std::hex << (val&0xF8) << std::dec << ",";
    int N=(int) input->readULong(2);
    if (!input->checkPosition(actPos+20+N)) {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: the number of character seems bad\n"));
      f << "##N=" << N << ",";
      ok=false;
      break;
    }
    float dim[4];
    for (int i=0; i<4; ++i) dim[i]=float(input->readLong(2));
    shape.m_box=Box2f(Vec2f(dim[1],dim[0]), Vec2f(dim[3],dim[2]));
    shape.m_textEntry.setBegin(input->tell());
    shape.m_textEntry.setLength(N);
    shape.m_style.m_lineWidth=0; // no border for textbox
    f << shape.m_box << ",";
    input->seek(actPos+20+N, librevenge::RVNG_SEEK_SET);
    break;
  }
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
  case 7: {
    if (!input->checkPosition(actPos+16+(type==7 ? 4 : 0))) {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: the zone length seems bad\n"));
      f << "###";
      ok=false;
      break;
    }
    float dim[4];
    for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
    Box2f box(Vec2f(dim[1],dim[0]), Vec2f(dim[3],dim[2]));
    f << box << ",";
    shape.m_box=box;
    switch (type) {
    case 2:
    case 3:
      shape.m_shape=MWAWGraphicShape::line(box[0], box[1]);
      break;
    case 4:
    case 5:
      shape.m_shape=MWAWGraphicShape::rectangle(box, Vec2f(cornerWidth,cornerWidth));
      break;
    case 6:
      shape.m_shape=MWAWGraphicShape::circle(box);
      break;
    case 7: {
      int fileAngle[2];
      for (int i=0; i<2; ++i) fileAngle[i]=(int) input->readLong(2);
      f << "angle=" << fileAngle[0] << "x" << fileAngle[0]+fileAngle[1] << ",";

      if (fileAngle[1]<0) {
        fileAngle[0]+=fileAngle[1];
        fileAngle[1]*=-1;
      }
      int angle[2] = { 90-fileAngle[0]-fileAngle[1], 90-fileAngle[0] };
      while (angle[1] > 360) {
        angle[0]-=360;
        angle[1]-=360;
      }
      while (angle[0] < -360) {
        angle[0]+=360;
        angle[1]+=360;
      }

      Vec2f center = box.center();
      Vec2f axis = 0.5*Vec2f(box.size());
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
      Box2f realBox(Vec2f(center[0]+minVal[0],center[1]+minVal[1]),
                    Vec2f(center[0]+maxVal[0],center[1]+maxVal[1]));
      shape.m_box=Box2f(Vec2f(shape.m_box[0])+realBox[0],Vec2f(shape.m_box[0])+realBox[1]);
      if (shape.m_style.hasSurface())
        shape.m_shape = MWAWGraphicShape::pie(realBox, box, Vec2f(float(angle[0]),float(angle[1])));
      else
        shape.m_shape = MWAWGraphicShape::arc(realBox, box, Vec2f(float(angle[0]),float(angle[1])));
      break;
    }
    default:
      break;
    }
    break;
  }
  case 8:
  case 9: {
    long dSz=(long)input->readULong(4);
    if (dSz<18||!input->checkPosition(actPos+4+dSz)) {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: the polygon data size seems bad\n"));
      f << "###";
      ok=false;
      break;
    }
    int N=(int) input->readULong(2);
    f << "N=" << N << ",";
    float dim[4];
    for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
    Box2f box(Vec2f(dim[1],dim[0]), Vec2f(dim[3],dim[2]));
    f << box << ",";
    shape.m_shape.m_type = MWAWGraphicShape::Polygon;
    shape.m_shape.m_bdBox= shape.m_box=box;
    int coordSz=type==8 ? 1 : 4;
    if (2+16+(type==8 ? 6 : 0)+2*N*coordSz>dSz) {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: the number of points seems bad\n"));
      f << "###";
      ok=false;
      break;
    }
    val=(int) input->readLong(2); // find a copy of the type here
    if (val!=type) f << "g0=" << val << ",";
    std::vector<Vec2f> &vertices=shape.m_shape.m_vertices;
    if (type==8) {
      for (int i=0; i<2; ++i) dim[i]=float(input->readLong(4))/65536.f;
      Vec2f point(dim[1],dim[0]);
      vertices.push_back(point);
      f << "orig=" << point << ",";
      f << "delta=[";
      for (int i=0; i<N-1; ++i) {
        int delta[2];
        for (int j=0; j<2; ++j) delta[j]=(int) input->readLong(1);
        Vec2f deltaPt((float) delta[0], (float) delta[1]);
        point+=deltaPt;
        vertices.push_back(point);
        f << deltaPt << ",";
      }
      f << "],";
    }
    else {
      f << "pts=[";
      for (int i=0; i<N; ++i) {
        float coord[2];
        for (int j=0; j<2; ++j) coord[j]=float(input->readLong(4))/65536.f;
        Vec2f point(coord[1], coord[0]);
        f << point << ",";
        vertices.push_back(point);
      }
      f << "],";
    }
    input->seek(actPos+4+dSz, librevenge::RVNG_SEEK_SET);
    break;
  }
  case 10: {
    if (!input->checkPosition(actPos+32)) {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: the zone length seems bad\n"));
      f << "###";
      ok=false;
      break;
    }
    int const vers=version();
    if (vers==1) {
      float dim[4];
      for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
      Box2f box(Vec2f(dim[1],dim[0]), Vec2f(dim[3],dim[2]));
      f << box << ",";
      shape.m_box=box;
    }
    int N=(int) input->readULong(2);
    f << "N=" << N << ",";
    val=(int) input->readLong(2);
    if (val!=N) f << "N[child]=" << val << ",";
    long dSz=(int) input->readULong(4); // related to size (but can be only an approximation)
    f << "groupSize=" << std::hex << dSz << std::dec << ",";
    if (vers==0) {
      float dim[4];
      for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
      Box2f box(Vec2f(dim[1],dim[0]), Vec2f(dim[3],dim[2]));
      f << box << ",";
      shape.m_box=box;
    }
    for (int i=0; i<2; ++i) {
      val=(int) input->readULong(4);
      if (val) f << "id" << i << "=" << std::hex << val << std::dec << ",";
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    for (int i=0; i<N; ++i) {
      int cId=readObject();
      if (cId<0) {
        MWAW_DEBUG_MSG(("MacDrawParser::readObject: can not find a child\n"));
        return int(shapeId);
      }
      // do not use shape's childList as the vector can have grown
      m_state->m_shapeList[shapeId].m_childList.push_back(size_t(cId));
    }
    int cId=readObject(); // read end group
    int nextId=int(m_state->m_shapeList.size());
    if (cId<0) {
      m_state->m_shapeList[shapeId].m_nextId=nextId;
      return int(shapeId);
    }
    m_state->m_shapeList[shapeId].m_nextId=nextId-1;
    if (m_state->m_shapeList[size_t(cId)].m_type!=MacDrawParserInternal::Shape::GroupEnd) {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: oops, can not find the end group data\n"));
      ascii().addPos(pos);
      ascii().addNote("###");
    }
    else
      m_state->m_shapeList.pop_back();
    return int(shapeId);
  }
  case 11: {
    if (!input->checkPosition(actPos+42))  {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: the zone length seems bad\n"));
      f << "###";
      ok=false;
      break;
    }
    for (int i=0; i<2; ++i) { // maybe a dim
      val=(int) input->readULong(2);
      if (val) f << "f0=" << val << ",";
    }
    int dim[4];
    for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
    Box2i &bitmapBox=shape.m_bitmapDim;
    bitmapBox=Box2i(Vec2i(dim[1],dim[0]), Vec2i(dim[3],dim[2]));
    f << "bitmap[dim]="<< bitmapBox << ",";
    float fDim[4];
    for (int i=0; i<4; ++i) fDim[i]=float(input->readLong(4))/65536.f;
    Box2f box(Vec2f(fDim[1],fDim[0]), Vec2f(fDim[3],fDim[2]));
    f << box << ",";
    shape.m_box=box;

    val=(int) input->readULong(4); // find 8063989c maybe type + id
    if (val) f << "id=" << std::hex << val << std::dec << ",";
    shape.m_numBytesByRow=(int) input->readULong(2);

    for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
    Box2i &fileBox=shape.m_bitmapFileDim;
    fileBox=Box2i(Vec2i(dim[1],dim[0]), Vec2i(dim[3],dim[2]));
    f << "bitmap[dimInFile]="<< fileBox << ",";
    shape.m_bitmapEntry.setBegin(input->tell());
    shape.m_bitmapEntry.setLength(fileBox.size()[1]*shape.m_numBytesByRow);
    if (fileBox.size()[1]<0 || shape.m_numBytesByRow<0 || !input->checkPosition(shape.m_bitmapEntry.end())) {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: can not compute the bitmap endPos\n"));
      f << "###";
      ok=false;
      break;
    }
    ascii().skipZone(shape.m_bitmapEntry.begin(), shape.m_bitmapEntry.end()-1);
    if (shape.m_numBytesByRow*8 < fileBox.size()[0] ||
        fileBox[0][0]>bitmapBox[0][0] || fileBox[0][1]>bitmapBox[0][1] ||
        fileBox[1][0]<bitmapBox[1][0] || fileBox[1][1]<bitmapBox[1][1]) {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: something look bad when reading a bitmap header\n"));
      f << "###";
      shape.m_bitmapEntry=MWAWEntry();
    }
    input->seek(shape.m_bitmapEntry.end(), librevenge::RVNG_SEEK_SET);
    break;
  }
  default:
    ok=false;
    break;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return ok ? int(shapeId) : -1;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MacDrawParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MacDrawParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->checkPosition(512))
    return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";
  input->seek(0, librevenge::RVNG_SEEK_SET);
  int val=(int) input->readULong(2);
  int vers=0;
  if (val==0x4452) {
    if (input->readULong(2)!=0x5747) return false;
    val=(int) input->readULong(2);
    if (val==0x4d44) vers=1;
    else {
      MWAW_DEBUG_MSG(("MacDrawParser::checkHeader: find unexpected header\n"));
      return false;
    }
    f << "version=" << vers << ",";
    f << "subVersion=" << input->readLong(2) << ",";
  }
  else if (val==0x4d44) {
    vers=0;
    val=(int) input->readLong(2); // find 4 for v0.9
    if (val!=4) f << "f0=" << val << ",";
    for (int i=0; i<2; ++i) { // always 0?
      val=(int) input->readLong(2);
      if (val) f << "f" << i+1 << "=" << val << ",";
    }
  }
  else
    return false;
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  if (strict && !readPrintInfo()) {
    input->seek(8, librevenge::RVNG_SEEK_SET);
    for (int i=0; i<10; ++i) // allow print info to be zero
      if (input->readLong(2)) return false;
  }

  if (strict && vers >= 1) {
    // we must check that this is not a basic pict file
    input->seek(512+2, librevenge::RVNG_SEEK_SET);
    int dim[4];
    for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
    val=(int) input->readLong(2);
    if (dim[0]<dim[2] && dim[1]<dim[3] && (val==0x1101 || (val==0x11 && input->readLong(2)==0x2ff))) {
      // posible
      input->seek(512, librevenge::RVNG_SEEK_SET);
      Box2f box;
      if (MWAWPictData::check(input, (int)(input->size()-512), box) != MWAWPict::MWAW_R_BAD)
        return false;
    }
  }
  if (strict) {
    // check also the beginning list of shape
    input->seek(512, librevenge::RVNG_SEEK_SET);
    for (int i=0; i<3; ++i) {
      if (input->isEnd()) break;
      if (readObject()<0) {
        MWAW_DEBUG_MSG(("MacDrawParser::checkHeader: problem reading some shape\n"));
        return false;
      }
    }
    m_state.reset(new MacDrawParserInternal::State());
  }
  setVersion(vers);
  m_state->m_version=vers;
  if (header)
    header->reset(MWAWDocument::MWAW_T_MACDRAW, vers, MWAWDocument::MWAW_K_DRAW);
  input->seek(512,librevenge::RVNG_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
// try to read the prefs zone
////////////////////////////////////////////////////////////
bool MacDrawParser::readPrefs()
{
  MWAWInputStreamPtr input = getInput();
  if (!input->checkPosition(512)) {
    MWAW_DEBUG_MSG(("MacDrawParser::readPrefs: the prefs zone seems too short\n"));
    ascii().addPos(14);
    ascii().addNote("Entries(Prefs):#");
    return false;
  }
  input->seek(8,librevenge::RVNG_SEEK_SET);
  if (!readPrintInfo()) {
    ascii().addPos(8);
    ascii().addNote("Entries(PrintInfo):#");
  }
  input->seek(8+120, librevenge::RVNG_SEEK_SET);
  // v2: cut in 128, 40, 3*40, remain
  long pos=input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Prefs):";
  for (int i=0; i<9; ++i) { // f0=1|2|7, f1=0|75|78|7c, f2=0|48, f4=0|48, f5=0|48, f6=0|48, f7=0|48
    int val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(0x100, librevenge::RVNG_SEEK_SET);
  pos=input->tell();
  input->seek(pos+40, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote("Prefs-A:");
  for (int i=0; i<5; ++i) {
    pos=input->tell();
    f.str("");
    f << "Prefs-B" << i << ":";
    input->seek(pos+40, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  pos=input->tell();
  ascii().addPos(pos);
  ascii().addNote("Prefs-end:");

  input->seek(512, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// try to read the print info zone
////////////////////////////////////////////////////////////
bool MacDrawParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  long endPos=pos+120;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("MacDrawParser::readPrintInfo: file seems too short\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(PrintInfo):";
  libmwaw::PrinterInfo info;
  if (!info.read(input)) {
    MWAW_DEBUG_MSG(("MacDrawParser::readPrintInfo: can not read print info\n"));
    return false;
  }
  f << info;
  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) {
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return true;
  }

  // define margin from print info
  Vec2i lTopMargin= -1 * info.paper().pos(0);
  Vec2i rBotMargin=info.paper().size() - info.page().size();

  // move margin left | top
  int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
  int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
  lTopMargin -= Vec2i(decalX, decalY);
  rBotMargin += Vec2i(decalX, decalY);

  // decrease right | bottom
  int rightMarg = rBotMargin.x() -50;
  if (rightMarg < 0) rightMarg=0;
  int botMarg = rBotMargin.y() -50;
  if (botMarg < 0) botMarg=0;

  getPageSpan().setMarginTop(lTopMargin.y()/72.0);
  getPageSpan().setMarginBottom(botMarg/72.0);
  getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
  getPageSpan().setMarginRight(rightMarg/72.0);
  getPageSpan().setFormLength(paperSize.y()/72.);
  getPageSpan().setFormWidth(paperSize.x()/72.);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//
// send data
//
////////////////////////////////////////////////////////////

bool MacDrawParser::send(MacDrawParserInternal::Shape const &shape)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDrawParser::send: can not find the listener\n"));
    return false;
  }
  shape.m_isSent=true;
  Box2f box=shape.getBdBox();
  MWAWPosition pos(box[0], box.size(), librevenge::RVNG_POINT);
  pos.m_anchorTo = MWAWPosition::Page;
  switch (shape.m_type) {
  case MacDrawParserInternal::Shape::Basic:
    listener->insertPicture(pos, shape.m_shape, shape.m_style);
    break;
  case MacDrawParserInternal::Shape::Bitmap:
    return sendBitmap(shape, pos);
  case MacDrawParserInternal::Shape::Group: {
    size_t numShapes=m_state->m_shapeList.size();
    if (!numShapes) break;
    listener->openLayer(pos);
    for (size_t i=0; i<shape.m_childList.size(); ++i) {
      if (shape.m_childList[i]>=numShapes) {
        MWAW_DEBUG_MSG(("MacDrawParser::send: can not find a child\n"));
        continue;
      }
      MacDrawParserInternal::Shape const &child=m_state->m_shapeList[shape.m_childList[i]];
      if (child.m_isSent) {
        MWAW_DEBUG_MSG(("MacDrawParser::send: the child is already sent\n"));
        continue;
      }
      send(child);
    }
    listener->closeLayer();
    break;
  }
  case MacDrawParserInternal::Shape::GroupEnd:
    break;
  case MacDrawParserInternal::Shape::Text: {
    shared_ptr<MWAWSubDocument> doc(new MacDrawParserInternal::SubDocument(*this, getInput(), shape.m_id));
    listener->insertTextBox(pos, doc, shape.m_style);
    return true;
  }
  case MacDrawParserInternal::Shape::Unknown:
  default:
    return false;
  }
  return true;
}

bool MacDrawParser::sendBitmap(MacDrawParserInternal::Shape const &shape, MWAWPosition const &position)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDrawParser::sendBitmap: can not find the listener\n"));
    return false;
  }
  if (!shape.m_bitmapEntry.valid()) return false;
  int const numBytesByRow=shape.m_numBytesByRow;
  if (shape.m_type!=MacDrawParserInternal::Shape::Bitmap || numBytesByRow<=0 ||
      numBytesByRow*shape.m_bitmapFileDim.size()[1]<shape.m_bitmapEntry.length() ||
      shape.m_bitmapDim[0][0]<0 || shape.m_bitmapDim[0][1]<0 ||
      shape.m_bitmapDim[0][0]<shape.m_bitmapFileDim[0][0] ||
      shape.m_bitmapFileDim.size()[0]<=0 || shape.m_bitmapFileDim.size()[1]<=0 ||
      8*numBytesByRow<shape.m_bitmapFileDim.size()[0]) {
    MWAW_DEBUG_MSG(("MacDrawParser::sendBitmap: the bitmap seems bad\n"));
    return false;
  }
  // change: implement indexed transparent color, replaced this code
  MWAWPictBitmapColor pict(shape.m_bitmapDim[1], true);
  MWAWColor transparent(255,255,255,0);
  MWAWColor black(MWAWColor::black());
  std::vector<MWAWColor> data(size_t(shape.m_bitmapDim[1][0]), transparent);
  // first set unseen row to zero (even if this must not appear)
  for (int r=shape.m_bitmapDim[0][1]; r<shape.m_bitmapFileDim[0][1]; ++r) pict.setRow(r, &data[0]);
  for (int r=shape.m_bitmapFileDim[1][1]; r<shape.m_bitmapDim[1][1]; ++r) pict.setRow(r, &data[0]);

  MWAWInputStreamPtr input=getInput();
  input->seek(shape.m_bitmapEntry.begin(), librevenge::RVNG_SEEK_SET);
  for (int r=shape.m_bitmapFileDim[0][1]; r<shape.m_bitmapFileDim[1][1]; ++r) {
    long pos=input->tell();
    if (r<shape.m_bitmapDim[0][1]||r>=shape.m_bitmapDim[1][1]) { // must not appear, but...
      input->seek(pos+numBytesByRow, librevenge::RVNG_SEEK_SET);
      continue;
    }
    int wPos=shape.m_bitmapFileDim[0][0];
    for (int col=shape.m_bitmapFileDim[0][0]; col<shape.m_bitmapFileDim[1][0]; ++col) {
      unsigned char c=(unsigned char) input->readULong(1);
      for (int j=0, bit=0x80; j<8 ; ++j, bit>>=1) {
        if (wPos>=shape.m_bitmapDim[1][0]) break;
        data[size_t(wPos++)]=(c&bit) ? black : transparent;
      }
    }
    pict.setRow(r, &data[0]);
    input->seek(pos+numBytesByRow, librevenge::RVNG_SEEK_SET);
  }

  librevenge::RVNGBinaryData binary;
  std::string type;
  if (!pict.getBinary(binary,type)) return false;
#ifdef DEBUG_WITH_FILES
  static int volatile pictName = 0;
  libmwaw::DebugStream f;
  f << "PICT-" << ++pictName << ".bmp";
  libmwaw::Debug::dumpFile(binary, f.str().c_str());
#endif

  listener->insertPicture(position, binary, type);

  return true;
}

bool MacDrawParser::sendText(int zId)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDrawParser::sendText: can not find the listener\n"));
    return false;
  }
  if (zId<0||zId>=(int) m_state->m_shapeList.size() ||
      m_state->m_shapeList[size_t(zId)].m_type != MacDrawParserInternal::Shape::Text) {
    MWAW_DEBUG_MSG(("MacDrawParser::sendText: can not find the text shape\n"));
    return false;
  }
  MacDrawParserInternal::Shape const &shape=m_state->m_shapeList[size_t(zId)];
  shape.m_isSent = true;
  if (!shape.m_textEntry.valid())
    return true;

  listener->setParagraph(shape.m_paragraph);
  listener->setFont(shape.m_font);

  MWAWInputStreamPtr input=getInput();
  input->seek(shape.m_textEntry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Object[text]:";
  long endPos=shape.m_textEntry.end();
  while (!input->isEnd()) {
    if (input->tell()>=shape.m_textEntry.end())
      break;
    char c = (char) input->readULong(1);
    if (c==0) {
      MWAW_DEBUG_MSG(("MacDrawParser::sendText: find char 0\n"));
      f << "#[0]";
      continue;
    }
    f << c;
    switch (c) {
    case 9:
      listener->insertTab();
      break;
    case 0xd:
      listener->insertEOL();
      break;
    default:
      listener->insertCharacter((unsigned char)c, input, endPos);
      break;
    }
  }
  ascii().addPos(shape.m_textEntry.begin());
  ascii().addNote(f.str().c_str());
  return true;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
