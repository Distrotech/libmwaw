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

#include "SuperPaintParser.hxx"

/** Internal: the structures of a SuperPaintParser */
namespace SuperPaintParserInternal
{
////////////////////////////////////////
//! Internal: the shape of a SuperPaintParser
struct Shape {
  //! the type
  enum Type { GraphicShape, Group, Picture, TextBox };
  //! constructor
  Shape(Type type, Box2f const &box) : m_type(type), m_box(box), m_entry(), m_shape(), m_style(), m_font(), m_justify(MWAWParagraph::JustificationLeft), m_interline(1)
  {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Shape const &shape);
  //! the shape type
  Type m_type;
  //! the bdbox
  Box2f m_box;
  //! the picture/textbox entry
  MWAWEntry m_entry;
  //! the graphic shape
  MWAWGraphicShape m_shape;
  //! the graphic style
  MWAWGraphicStyle m_style;
  //! the textbox font
  MWAWFont m_font;
  //! the textbox justification
  MWAWParagraph::Justification m_justify;
  //! the interline in percent
  double m_interline;
};

std::ostream &operator<<(std::ostream &o, Shape const &shape)
{
  switch (shape.m_type) {
  case Shape::GraphicShape:
    o << "shape," << shape.m_shape << ",";
    break;
  case Shape::Group:
    o << "group,box=" << shape.m_box << ",";
    break;
  case Shape::Picture:
    o << "picture,box=" << shape.m_box << ",";
    break;
  case Shape::TextBox:
    o << "textbox,box=" << shape.m_box << ",";
    break;
  default:
    MWAW_DEBUG_MSG(("SuperPaintParserInternal::Shape::operator<<: find unknown type"));
    break;
  }
  o << shape.m_style;
  return o;
}

////////////////////////////////////////
//! Internal: the state of a SuperPaintParser
struct State {
  //! constructor
  State() : m_kind(MWAWDocument::MWAW_K_DRAW), m_bitmap(), m_shapeList()
  {
  }
  //! try to return the color corresponding to an id
  static bool getColor(int id, MWAWColor &color);
  //! the file type
  MWAWDocument::Kind m_kind;
  /// the bitmap (v1)
  shared_ptr<MWAWPict> m_bitmap;
  //! the list of shapes
  std::vector<Shape> m_shapeList;
};

bool State::getColor(int id, MWAWColor &color)
{
  switch (id) {
  case 0:
    color=MWAWColor(0,0,0);
    break;
  case 1:
    color=MWAWColor(255,255,255);
    break;
  case 2:
    color=MWAWColor(255,0,0);
    break;
  case 3:
    color=MWAWColor(0,255,0);
    break;
  case 4:
    color=MWAWColor(0,0,255);
    break;
  case 5: // orange
    color=MWAWColor(255, 165, 0);
    break;
  case 6: // purple
    color=MWAWColor(128,0,128);
    break;
  case 7: // yellow
    color=MWAWColor(255,255,0);
    break;
  default:
    MWAW_DEBUG_MSG(("SuperPaintParserInternal::State::getColor: can not determine color for id=%d\n", id));
    return false;
  }
  return true;
}

////////////////////////////////////////
//! Internal: the subdocument of a SuperPaintParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(SuperPaintParser &pars, MWAWInputStreamPtr input, int zoneId) : MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

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
    MWAW_DEBUG_MSG(("SuperPaintParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  if (!m_parser) {
    MWAW_DEBUG_MSG(("SuperPaintParserInternal::SubDocument::parse: no parser\n"));
    return;
  }
  long pos = m_input->tell();
  static_cast<SuperPaintParser *>(m_parser)->sendText(m_id);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}


}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
SuperPaintParser::SuperPaintParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWGraphicParser(input, rsrcParser, header), m_state()
{
  init();
}

SuperPaintParser::~SuperPaintParser()
{
}

void SuperPaintParser::init()
{
  resetGraphicListener();
  setAsciiName("main-1");

  m_state.reset(new SuperPaintParserInternal::State);

  getPageSpan().setMargins(0.1);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void SuperPaintParser::parse(librevenge::RVNGDrawingInterface *docInterface)
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
      if (m_state->m_kind==MWAWDocument::MWAW_K_PAINT)
        sendBitmap();
      else
        sendPictures();
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("SuperPaintParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetGraphicListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void SuperPaintParser::createDocument(librevenge::RVNGDrawingInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getGraphicListener()) {
    MWAW_DEBUG_MSG(("SuperPaintParser::createDocument: listener already exist\n"));
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
bool SuperPaintParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  readHeader();
  input->seek(512,librevenge::RVNG_SEEK_SET);
  bool ok=true;
  if (m_state->m_kind==MWAWDocument::MWAW_K_DRAW)
    ok = readPictures();
  else
    ok = readBitmap();

  long pos=input->tell();
  if (input->size()==pos+2 && input->readLong(2)==0) {
    ascii().addPos(pos);
    ascii().addNote("_");
  }
  else if (input->size()!=pos) {
    MWAW_DEBUG_MSG(("SuperPaintParser::createZones: find some extra data\n"));
    ascii().addPos(input->tell());
    ascii().addNote("Entries(End):###");
  }
  return ok;
}

////////////////////////////////////////////////////////////
// send shapes: vector graphic document
////////////////////////////////////////////////////////////
bool SuperPaintParser::sendPictures()
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("SuperPaintParser::sendPictures: can not find the listener\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();

  for (size_t i=m_state->m_shapeList.size(); i>0;) {
    SuperPaintParserInternal::Shape const &shape=m_state->m_shapeList[--i];
    MWAWPosition pos(shape.m_box[0], shape.m_box.size(), librevenge::RVNG_POINT);
    pos.setPage(1);
    pos.m_anchorTo=MWAWPosition::Page;
    switch (shape.m_type) {
    case SuperPaintParserInternal::Shape::Group:
      break;
    case SuperPaintParserInternal::Shape::TextBox: {
      shared_ptr<MWAWSubDocument> doc(new SuperPaintParserInternal::SubDocument(*this, input, (int) i));
      listener->insertTextBox(pos, doc, MWAWGraphicStyle::emptyStyle());
      break;
    }
    case SuperPaintParserInternal::Shape::Picture:
      if (!shape.m_entry.valid()) {
        MWAW_DEBUG_MSG(("SuperPaintParser::sendPictures: the picture entry seems bad\n"));
        break;
      }
      else {
        input->seek(shape.m_entry.begin(), librevenge::RVNG_SEEK_SET);
        shared_ptr<MWAWPict> thePict(MWAWPictData::get(input, (int)shape.m_entry.length()));
        librevenge::RVNGBinaryData data;
        std::string type;
        if (thePict && thePict->getBinary(data,type))
          listener->insertPicture(pos, data, type);
        else {
          MWAW_DEBUG_MSG(("SuperPaintParser::sendPictures: can not check the picture data\n"));
          break;
        }
      }
      break;
    case SuperPaintParserInternal::Shape::GraphicShape:
      listener->insertPicture(pos, shape.m_shape, shape.m_style);
      break;
    default:
      MWAW_DEBUG_MSG(("SuperPaintParser::sendPictures: find unknown shape type\n"));
      break;
    }
  }
  return true;
}

bool SuperPaintParser::sendText(int id)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener || !listener->canWriteText()) {
    MWAW_DEBUG_MSG(("SuperPaintParser::sendText: can not find the listener\n"));
    return false;
  }
  if (id<0 || id>=(int)m_state->m_shapeList.size()) {
    MWAW_DEBUG_MSG(("SuperPaintParser::sendText: can not find the textbox\n"));
    return false;
  }
  SuperPaintParserInternal::Shape const &shape=m_state->m_shapeList[size_t(id)];
  if (!shape.m_entry.valid() || shape.m_type != SuperPaintParserInternal::Shape::TextBox) {
    MWAW_DEBUG_MSG(("SuperPaintParser::sendText: the textbox seems invalid\n"));
    return false;
  }
  MWAWParagraph para;
  para.setInterline(shape.m_interline, librevenge::RVNG_PERCENT);
  para.m_justify=shape.m_justify;
  listener->setParagraph(para);
  MWAWFont font=shape.m_font;
  listener->setFont(font);

  MWAWInputStreamPtr input = getInput();
  input->seek(shape.m_entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Shape-Text:";
  for (long i = 0; i < shape.m_entry.length(); i++) {
    unsigned char c = (unsigned char) input->readULong(1);
    f << c;
    switch (c) {
    case 0x9:
      listener->insertTab();
      break;
    case 0xd:
      // this is marks the end of a paragraph
      listener->insertEOL();
      break;
    default:
      listener->insertCharacter(c);
      break;
    }
  }
  ascii().addPos(shape.m_entry.begin());
  ascii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// read shapes: vector graphic document
////////////////////////////////////////////////////////////
bool SuperPaintParser::readPictures()
{
  MWAWInputStreamPtr input = getInput();
  input->seek(512, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  while (!input->isEnd()) {
    long pos=input->tell();
    if (readShape()) continue;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << "Entries(Picture):";
    int dSz=(int) input->readULong(2);
    if (!input->checkPosition(pos+2+dSz)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    input->seek(pos+2+dSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return !m_state->m_shapeList.empty();
}

bool SuperPaintParser::readShape()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();

  libmwaw::DebugStream f;
  f << "Entries(Shape):";
  int dSz=(int) input->readULong(2);
  if (dSz==0xFFFF) {
    f << "endGroup";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return true;
  }
  if (dSz<50 || !input->checkPosition(pos+2+dSz)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
  Box2f box(Vec2f((float)dim[1],(float)dim[0]),Vec2f((float)dim[3],(float)dim[2]));
  f << "box=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
  int val;
  for (int i=0; i<4; ++i) { // always 0?
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  val=(int) input->readLong(1); // always 0?
  if (val) f << "f4=" << val << ",";
  int type=(int) input->readLong(1);
  MWAWGraphicStyle style;
  MWAWColor fontColor(MWAWColor::black());
  if (type) {
    val=(int) input->readULong(1);
    bool hasSurfaceColor=false;
    if (val==1)
      hasSurfaceColor=true;
    else if (val)
      f << "#hasColor=" << val << ",";
    // small number: pattern?
    val=(int) input->readULong(1);
    if (val) f << "g0=" << val << ",";
    val=(int) input->readULong(1);
    if (val==1) f << "select,";
    else if (val) f << "#select=val,";
    val=(int) input->readULong(1); // always 0
    if (val) f << "g1=" << val << ",";
    val=(int) input->readULong(2); // g3=a big number
    if (val) f << "g2=" << std::hex << val << std::dec << ",";
    int penSize[2];
    for (int i=0; i<2; ++i) penSize[i]=(int) input->readULong(2);
    if (penSize[0]!=1||penSize[1]!=1) f << "pen[size]=" << penSize[0] << "x" << penSize[1] << ",";
    style.m_lineWidth=0.5f*float(penSize[0]+penSize[1]);
    MWAWGraphicStyle::Pattern pat[2];
    for (int p=0; p<2; ++p) {
      pat[p].m_dim=Vec2i(8,8);
      pat[p].m_data.resize(8);
      for (size_t i=0; i < 8; i++) pat[p].m_data[i]=(uint8_t) input->readULong(1);
    }
    f << "line[pattern]=";
    MWAWColor color;
    if (pat[0].getUniqueColor(color))
      f << color << ",";
    else
      f << "[" << pat[0] << "],";
    f << "surface[pattern]=";
    if (pat[1].getUniqueColor(color))
      f << color << ",";
    else
      f << "[" << pat[1] << "],";
    val=(int) input->readULong(2);
    if (m_state->getColor((val>>13)&7, color)) {
      pat[0].m_colors[1]=color;
      if (!color.isBlack())
        f << "line0[col]=" << color << ",";
    }
    if (m_state->getColor((val>>10)&7, color)) {
      pat[0].m_colors[0]=color;
      if (!color.isWhite())
        f << "line1[col]=" << color << ",";
    }
    if (m_state->getColor((val>>7)&7, color)) {
      pat[1].m_colors[1]=fontColor=color;
      if (!color.isBlack())
        f << "surf0[col]=" << color << ",";
    }
    if (m_state->getColor(val&7, color)) {
      pat[1].m_colors[0]=color;
      if (!color.isWhite())
        f << "surf1[col]=" << color << ",";
    }
    if (pat[0].getAverageColor(color))
      style.m_lineColor=color;
    if (hasSurfaceColor) {
      if (pat[1].getUniqueColor(color))
        style.setSurfaceColor(color);
      else
        style.setPattern(pat[1]);
    }
    if (val&0x78) f << "g4=" << ((val&0x78)>>3) << ",";
  }
  int numDatas=0;
  SuperPaintParserInternal::Shape::Type shapeType=SuperPaintParserInternal::Shape::GraphicShape;
  switch (type) {
  case 0:
    shapeType=SuperPaintParserInternal::Shape::Group;
    m_state->m_shapeList.push_back(SuperPaintParserInternal::Shape(shapeType, box));
    f << "group,";
    break;
  case 0x2:
    shapeType=SuperPaintParserInternal::Shape::Picture;
    m_state->m_shapeList.push_back(SuperPaintParserInternal::Shape(shapeType, box));
    f << "pict,";
    numDatas=2;
    break;
  case 0x17: {
    f << "textbox,";
    val=(int) input->readULong(4);
    if (val) f << "id?=" << std::hex << val << ",";
    shapeType=SuperPaintParserInternal::Shape::TextBox;
    m_state->m_shapeList.push_back(SuperPaintParserInternal::Shape(shapeType, box));
    numDatas=2;
    if (dSz<0x4a) {
      MWAW_DEBUG_MSG(("SuperPaintParser::readShape: textbox size seems to short\n"));
      f << "###";
      break;
    }
    MWAWFont font;
    font.setId((int)input->readULong(2));
    font.setSize((float)input->readULong(2));
    font.setColor(fontColor);
    val=(int) input->readULong(2);
    switch (val&3) {
    case 0: // left
      break;
    case 1:
      m_state->m_shapeList.back().m_justify=MWAWParagraph::JustificationCenter;
      f << "center,";
      break;
    case 2:
      m_state->m_shapeList.back().m_justify=MWAWParagraph::JustificationRight;
      f << "right,";
      break;
    default:
      f << "#align=3,";
      break;
    }
    if ((val&0xFFFC)!=8) f << "#fl0=" << std::hex << (val&0xFFFC) << std::dec << ",";
    f << "height[line]?=" << input->readLong(2) << ","; // checkme
    val=(int) input->readULong(2);
    switch (val&3) {
    case 0: // basic interline
      break;
    case 1:
      m_state->m_shapeList.back().m_interline=1.5;
      f << "interline=150%,";
      break;
    case 2:
      m_state->m_shapeList.back().m_interline=2;
      f << "interline=200%,";
      break;
    default:
      break;
    }
    if ((val&0xFFFC)!=0xc) f << "#fl1=" << std::hex << (val&0xFFFC) << std::dec << ",";
    val=(int) input->readULong(2); // no sure about this one, seems sometimes related to the next font flags...
    if (val) f << "fl2="<< std::hex << val << std::dec << ",";
    int flag=(int) input->readULong(1);
    uint32_t flags=0;
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0x8) flags |= MWAWFont::embossBit;
    if (flag&0x10) flags |= MWAWFont::shadowBit;
    if (flag&0xE0) f << "#fFlag=" << std::hex << (flag&0xE0) << std::dec << ",";
    font.setFlags(flags);

    m_state->m_shapeList.back().m_font=font;
    f << "font=[" << font.getDebugString(getParserState()->m_fontConverter) << "],";
    break;
  }
  case 0x6: // axis line
  case 0x18: { // normal
    if (type==6)
      f << "line[axis],";
    else
      f << "line,";
    m_state->m_shapeList.push_back(SuperPaintParserInternal::Shape(shapeType, box));
    m_state->m_shapeList.back().m_shape=MWAWGraphicShape::line(box[0],box[1]);
    // now check the box size
    m_state->m_shapeList.back().m_box=m_state->m_shapeList.back().m_shape.getBdBox();
    break;
  }
  case 0x19:
    f << "rect,";
    m_state->m_shapeList.push_back(SuperPaintParserInternal::Shape(shapeType, box));
    m_state->m_shapeList.back().m_shape=MWAWGraphicShape::rectangle(box);
    break;
  case 0x1a:
    f << "roundRect,";
    m_state->m_shapeList.push_back(SuperPaintParserInternal::Shape(shapeType, box));
    m_state->m_shapeList.back().m_shape=MWAWGraphicShape::rectangle(box, Vec2f(5,5));
    break;
  case 0x1b:
  case 0x20:
    if (type==0x20)
      f << "circle[ortho],";
    else
      f << "circle,";
    m_state->m_shapeList.push_back(SuperPaintParserInternal::Shape(shapeType, box));
    m_state->m_shapeList.back().m_shape=MWAWGraphicShape::circle(box);
    break;
  case 0x1c: {
    f << "arc,";
    int fileAngle[2];
    for (int i = 0; i < 2; i++)
      fileAngle[i] = (int) input->readLong(2);
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
    m_state->m_shapeList.push_back(SuperPaintParserInternal::Shape(shapeType, realBox));
    m_state->m_shapeList.back().m_shape = MWAWGraphicShape::pie(realBox, box, Vec2f(float(angle[0]),float(angle[1])));
    break;
  }
  case 0x1d:
  case 0x1e:
    if (type==0x1d)
      f << "poly,";
    else
      f << "poly[hand],";
    m_state->m_shapeList.push_back(SuperPaintParserInternal::Shape(shapeType, box));
    m_state->m_shapeList.back().m_shape.m_type=MWAWGraphicShape::Polygon;
    numDatas=2;
    break;
  default:
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  if (m_state->m_shapeList.empty() || m_state->m_shapeList.back().m_type!=shapeType) {
    MWAW_DEBUG_MSG(("SuperPaintParser::readShape: the shape list seems bad\n"));
    f.str("");
    f << "Shape:type=" << type << ",###";
    input->seek(pos+2+dSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return true;
  }
  if (input->tell()!=pos+2+dSz) {
    ascii().addDelimiter(input->tell(),'|');
    input->seek(pos+2+dSz, librevenge::RVNG_SEEK_SET);
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  SuperPaintParserInternal::Shape &shape=m_state->m_shapeList.back();
  shape.m_style=style;
  if (!numDatas) return true;

  // first zone always 0 (or it is a 4 bytes pointer ) ?
  pos=input->tell();
  dSz=(int) input->readULong(2);
  if (!input->checkPosition(pos+2+dSz)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  if (dSz==0) {
    ascii().addPos(pos);
    ascii().addNote("_");
  }
  else {
    MWAW_DEBUG_MSG(("SuperPaintParser::readShape: find some data for type %d\n", type));
    f.str("");
    f << "Shape-Data0:type=" << type << ",###";
    input->seek(pos+2+dSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  if (numDatas<=1) return true;

  // now the real data
  pos=input->tell();
  dSz=(int) input->readULong(2);
  if (!input->checkPosition(pos+2+dSz)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  switch (shapeType) {
  case SuperPaintParserInternal::Shape::Picture:
    shape.m_entry.setBegin(pos+2);
    shape.m_entry.setLength(dSz);
    ascii().skipZone(pos+2,pos+1+dSz);
    break;
  case SuperPaintParserInternal::Shape::TextBox:
    shape.m_entry.setBegin(pos+2);
    shape.m_entry.setLength(dSz);
    break;
  case SuperPaintParserInternal::Shape::GraphicShape: {
    f.str("");
    f << "Shape-Point:";
    if (shape.m_shape.m_type==MWAWGraphicShape::Polygon)
      f << "poly,";
    else {
      m_state->m_shapeList.pop_back();
      MWAW_DEBUG_MSG(("SuperPaintParser::readShape: unexpected type\n"));
      f << "###";
      input->seek(pos+2+dSz, librevenge::RVNG_SEEK_SET);
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return true;
    }
    if (dSz<6 || (dSz%4)!=2) {
      m_state->m_shapeList.pop_back();
      MWAW_DEBUG_MSG(("SuperPaintParser::readShape: can not compute the number of polygon points\n"));
      f << "###";
      input->seek(pos+2+dSz, librevenge::RVNG_SEEK_SET);
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return true;
    }
    val=(int) input->readLong(2);
    if (val!=dSz) f << "#sz=" << val << ",";
    f << "bdbox=[";
    for (int i=0; i< 4; ++i) f << input->readLong(2) << ",";
    f << "],";
    int N=(dSz-6)/4;
    f << "N=" << N << ",pts=[";
    std::vector<Vec2f> vertices((size_t)N);
    for (int i=0; i<N; ++i) {
      int coord[2];
      for (int p=0; p<2; ++p) coord[p]=(int) input->readLong(2);
      f << Vec2i(coord[1],coord[0]) << ",";
      vertices[size_t(i)]=Vec2f((float)coord[1],(float)coord[0])-box[0];
    }
    f << "],";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    // the last vertices is sometimes bad, so...
    if (!vertices.empty())
      vertices.pop_back();
    shape.m_shape.m_vertices=vertices;
    break;
  }
  case SuperPaintParserInternal::Shape::Group:
  default:
    MWAW_DEBUG_MSG(("SuperPaintParser::readShape: call with unexpeced type\n"));
    f.str("");
    f << "Shape-Data1:type=" << type << ",###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  input->seek(pos+2+dSz, librevenge::RVNG_SEEK_SET);

  if (numDatas<=2) return true;
  // must not appear
  MWAW_DEBUG_MSG(("SuperPaintParser::readShape: find numData=%d\n", numDatas));
  for (int i=2; i<numDatas; ++i) {
    pos=input->tell();
    f.str("");
    f << "Shape-Data" << i << ":##";
    dSz=(int) input->readULong(2);
    if (!input->checkPosition(pos+2+dSz)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    input->seek(pos+2+dSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// read/send a bitmap ( paint document)
////////////////////////////////////////////////////////////
bool SuperPaintParser::sendBitmap()
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("SuperPaintParser::sendBitmap: can not find the listener\n"));
    return false;
  }

  librevenge::RVNGBinaryData data;
  std::string type;
  if (!m_state->m_bitmap || !m_state->m_bitmap->getBinary(data,type)) return false;

  MWAWPageSpan const &page=getPageSpan();
  MWAWPosition pos(Vec2f((float)page.getMarginLeft(),(float)page.getMarginRight()),
                   Vec2f((float)page.getPageWidth(),(float)page.getPageLength()), librevenge::RVNG_INCH);
  pos.setRelativePosition(MWAWPosition::Page);
  pos.m_wrapping = MWAWPosition::WNone;
  listener->insertPicture(pos, data, "image/pict");
  return true;
}

bool SuperPaintParser::readBitmap(bool onlyCheck)
{
  MWAWInputStreamPtr input = getInput();
  input->seek(512, librevenge::RVNG_SEEK_SET);
  long pos=input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Bitmap):";
  if (!input->checkPosition(pos+48)) {
    MWAW_DEBUG_MSG(("SuperPaintParser::readBitmap: the bitmap header seems too short\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int val;
  for (int i=0; i<11; ++i) {
    static int const expected[]= {0x11,1,0xa0,0x62,0xc,0xa0,0,0x8e,1,0,0xa};
    val=(int) input->readULong(1);
    if (val!=expected[i])
      f << "f" << i << "=" << val << ",";
  }
  int pictDim[4];
  for (int i=0; i<4; ++i) pictDim[i]=(int) input->readLong(2);
  f << "dim=" << pictDim[1] << "x" << pictDim[0] << "<->" << pictDim[3] << "x" << pictDim[2] << ",";
  if (pictDim[0]<0 || pictDim[1]<0 || pictDim[0]>pictDim[2] || pictDim[1]>pictDim[3]) {
    MWAW_DEBUG_MSG(("SuperPaintParser::readBitmap: the picture dimension seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+19, librevenge::RVNG_SEEK_SET);

  shared_ptr<MWAWPictBitmapIndexed> pict;
  if (!onlyCheck) {
    pict.reset(new MWAWPictBitmapIndexed(Vec2i(pictDim[3],pictDim[2])));
    std::vector<MWAWColor> colors(2);
    colors[0]=MWAWColor::white();
    colors[1]=MWAWColor::black();
    pict->setColors(colors);
  }

  while (!input->isEnd()) {
    pos=input->tell();
    f.str("");
    f << "Bitmap-Row:";
    val=(int) input->readULong(1);
    if (val!=0x98||!input->checkPosition(pos+29)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    int numColByRow=(int) input->readLong(2); // 20|2c|2a
    f << "numCol[byRow]=" << numColByRow << ",";
    int dim[4];
    for (int i=0; i<4; ++i) dim[i]=(int) input->readULong(2);
    f << "dim?=" << dim[1] << "x" << dim[0] << "<->" <<  dim[3] << "x" << dim[2] << ",";
    if (dim[2]<dim[0] || numColByRow*8 < dim[3]-dim[1] || dim[1]<0 || dim[3]>pictDim[3]+8) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    for (int j=0; j<2; ++j) {
      // normally the real picture position (before alignment to 8)
      int dim2[4];
      for (int i=0; i<4; ++i) dim2[i]=(int) input->readULong(2);
      if (dim[0]!=dim2[0]||dim[2]!=dim2[2]||(dim[1]>dim2[1]||dim[1]+8<dim2[1])||
          (dim[3]<dim2[3]||dim[3]>dim2[3]+8))
        f << "dim" << j << "?=" << dim2[1] << "x" << dim2[0] << "<->" <<  dim2[3] << "x" << dim2[2] << ",";
    }
    val=(int) input->readLong(2);
    if (val!=1) f << "f1=" << val << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    for (int r=dim[0]; r<dim[2]; ++r) {
      pos=input->tell();
      int len=(int) input->readULong(1);
      long rEndPos=pos+1+len;
      if (!len || !input->checkPosition(rEndPos)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
      f.str("");
      f << "Bitmap-R" << r << ":";
      int col=dim[1];
      while (input->tell()<rEndPos) {
        int wh=(int) input->readULong(1);
        if (wh>=0x81) {
          int color=(int) input->readULong(1);
          if (onlyCheck) {
            col+=8*(0x101-wh);
          }
          else {
            for (int j=0; j < 0x101-wh; ++j) {
              for (int b=7; b>=0; --b) {
                if (col<pictDim[3])
                  pict->set(col, r, (color>>b)&1);
                ++col;
              }
            }
          }
        }
        else {
          if (input->tell()+wh+1>rEndPos) {
            MWAW_DEBUG_MSG(("SuperPaintParser::readBitmap: can not read row %d\n", r));
            f << "###";
            ascii().addPos(pos);
            ascii().addNote(f.str().c_str());
            return false;
          }
          if (onlyCheck) {
            col+=8*(wh+1);
            input->seek(wh+1, librevenge::RVNG_SEEK_CUR);
          }
          else {
            for (int j=0; j < wh+1; ++j) {
              int color=(int) input->readULong(1);
              for (int b=7; b>=0; --b) {
                if (col<pictDim[3])
                  pict->set(col, r, (color>>b)&1);
                ++col;
              }
            }
          }
        }
        if (col>pictDim[3]+8) {
          MWAW_DEBUG_MSG(("SuperPaintParser::readBitmap: can not read row %d\n", r));
          f << "###";
          ascii().addPos(pos);
          ascii().addNote(f.str().c_str());
          return false;
        }
      }
      f << "col=" << col << ",";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      input->seek(rEndPos, librevenge::RVNG_SEEK_SET);
    }
  }
  pos=input->tell();
  f.str("");
  f << "Bitmap-End:";
  if (!input->checkPosition(pos+6) || input->readULong(1)!=0xa0) {
    MWAW_DEBUG_MSG(("SuperPaintParser::readBitmap: problem when reading the bitmap\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  if (!onlyCheck)
    m_state->m_bitmap=pict;
  for (int i=0; i<3; ++i) {
    int const expected[]= {0,0x8F,0xFF};
    val=(int) input->readULong(1);
    if (val!=expected[i])
      f << "f" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  if (!onlyCheck)
    m_state->m_bitmap=pict;
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool SuperPaintParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = SuperPaintParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->checkPosition(512))
    return false;

  int const vers=1;
  libmwaw::DebugStream f;
  f << "FileHeader:";
  input->seek(0, librevenge::RVNG_SEEK_SET);
  if (input->readULong(2)!=0x1000) return false;
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
  if (dim[0]||dim[1]||dim[2]||dim[3])
    f << "bitmap[content]=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
  int val=(int) input->readLong(2);
  switch (val) {
  case 1:
    m_state->m_kind=MWAWDocument::MWAW_K_PAINT;
    f << "paint,";
    break;
  case 2: // not yet implemented
    f << "draw,";
    break;
  default:
    return false;
  }
  val=(int) input->readLong(1);
  if (val==1) f << "hasPrintInfo?,";
  else if (val) f << "#f0=" << val << ",";
  val=(int) input->readLong(1);
  if (val==8) f << "color?,";
  else if (val) f << "#f1=" << val << ",";
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  if (strict) {
    if (m_state->m_kind==MWAWDocument::MWAW_K_PAINT) {
      if (!readBitmap(true))
        return false;
    }
    else {
      input->seek(512,librevenge::RVNG_SEEK_SET);
      for (int i=0; i<4; ++i) {
        if (input->isEnd()) break;
        long pos=input->tell();
        long dSz=(long) input->readULong(2);
        if (!input->checkPosition(pos+2+dSz)) return false;
        input->seek(pos+2+dSz,librevenge::RVNG_SEEK_SET);
      }
    }
  }

  setVersion(vers);
  if (header)
    header->reset(MWAWDocument::MWAW_T_SUPERPAINT, vers, m_state->m_kind);
  input->seek(512,librevenge::RVNG_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
// try to read the header zone
////////////////////////////////////////////////////////////
bool SuperPaintParser::readHeader()
{
  MWAWInputStreamPtr input = getInput();
  if (!input->checkPosition(512)) {
    MWAW_DEBUG_MSG(("SuperPaintParser::readHeader: the header zone seems too short\n"));
    ascii().addPos(14);
    ascii().addNote("Entries(Header):#");
    return false;
  }
  input->seek(14,librevenge::RVNG_SEEK_SET);
  if (!readPrintInfo()) {
    ascii().addPos(14);
    ascii().addNote("Entries(PrintInfo):#");
    input->seek(14+120, librevenge::RVNG_SEEK_SET);
  }
  long pos=input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Header):";
  int val;
  for (int i=0; i<6; ++i) {
    int const expected[]= {0x24,1,0,0/*or3*/,0,0x24 /* or 32*/};
    val=(int) input->readLong(1);
    if (val != expected[i]) f << "f" << i << "=" << val << ",";
  }
  for (int i=0; i<60; ++i) { // always 0?
    val=(int) input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "Header-II:";
  for (int i=0; i<126; ++i) { // always 0?
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// try to read the print info zone
////////////////////////////////////////////////////////////
bool SuperPaintParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  long endPos=pos+120;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("SuperPaintParser::readPrintInfo: file seems too short\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(PrintInfo):";
  libmwaw::PrinterInfo info;
  if (!info.read(input)) {
    MWAW_DEBUG_MSG(("SuperPaintParser::readPrintInfo: can not read print info\n"));
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

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
