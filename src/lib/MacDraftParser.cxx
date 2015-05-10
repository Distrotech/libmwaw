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

#include "MacDraftParser.hxx"

/** Internal: the structures of a MacDraftParser */
namespace MacDraftParserInternal
{
//! low level: bitmap file position in a MacDraft file
struct BitmapFileData {
  //! constructor
  BitmapFileData() : m_id(0), m_rowSize(0), m_dimension(), m_entry()
  {
  }
  //! check is the entry is coherent
  bool ok() const
  {
    return m_dimension.size()[0]>=0 && m_dimension.size()[1]>=0 &&
           m_rowSize>=0 && m_rowSize*8>=m_dimension.size()[1] &&
           m_rowSize*m_dimension.size()[1]<=m_entry.length();
  }
  //! the bitmap id
  unsigned long m_id;
  //! the row size
  int m_rowSize;
  //! the bitmap dimension
  MWAWBox2i m_dimension;
  //! the data position
  MWAWEntry m_entry;
};

//! generic class used to store shape in MWAWDraftParser
struct Shape {
  //! the different shape
  enum Type { Basic, Bitmap, Group, Label, Text, Unknown };

  //! constructor
  Shape() : m_type(Unknown), m_box(), m_origin(), m_style(), m_patternId(-1), m_shape(), m_isLine(false), m_id(-1), m_nextId(-1),
    m_font(), m_paragraph(), m_textEntry(), m_labelWidth(0), m_childList(),
    m_bitmapIdList(), m_bitmapDimensionList(), m_isSent(false)
  {
  }

  //! return the shape bdbox
  MWAWBox2f getBdBox() const
  {
    return m_type==Basic ? m_shape.getBdBox() : m_box;
  }
  //! translate a shape
  void translate(MWAWVec2f const &dir)
  {
    if (m_type==Basic)
      m_shape.translate(dir);
    m_box=MWAWBox2f(m_box[0]+dir, m_box[1]+dir);
    m_origin=m_origin+dir;
  }
  //! transform a shape
  void transform(float rotate, bool flipX, MWAWVec2f const &center)
  {
    if (rotate<0 || rotate>0) {
      if (m_type==Basic) {
        m_shape=m_shape.rotate(rotate, center);
        m_box=m_shape.getBdBox();
      }
      else {
        if (m_type==Text) // the textbox is the rotated textbox, so first compute the normal box
          m_box=rotateBox(m_box, -m_style.m_rotate*float(M_PI/180.), m_box.center());
        if (m_type!=Label) // the label seems always horizontal
          m_style.m_rotate+=rotate;
        float angle=rotate*float(M_PI/180.);
        m_box=rotateBox(m_box, angle, center);
        MWAWVec2f decal=center-MWAWVec2f(std::cos(angle)*center[0]-std::sin(angle)*center[1],
                                         std::sin(angle)*center[0]+std::cos(angle)*center[1]);
        m_origin=MWAWVec2f(std::cos(angle)*m_origin[0]-std::sin(angle)*m_origin[1],
                           std::sin(angle)*m_origin[0]+std::cos(angle)*m_origin[1])+decal;
        if (m_type==Text) // now, we can compute the final rotated textbox
          m_box=rotateBox(m_box, m_style.m_rotate*float(M_PI/180.), m_box.center());
      }
    }
    if (flipX) {
      if (m_type==Basic) {
        m_shape.scale(MWAWVec2f(-1,1));
        m_shape.translate(MWAWVec2f(2*center[0],0));
      }
      m_style.m_flip[0]=!m_style.m_flip[0];
      m_box=MWAWBox2f(MWAWVec2f(2*center[0]-m_box[1][0],m_box[0][1]), MWAWVec2f(2*center[0]-m_box[0][0],m_box[1][1]));
      m_origin=MWAWVec2f(2*center[0]-m_origin[0],m_origin[1]);
    }
  }
  //! returns the rotation of a box
  static MWAWBox2f rotateBox(MWAWBox2f const &box, float angle, MWAWVec2f const &center)
  {
    MWAWVec2f decal=center-MWAWVec2f(std::cos(angle)*center[0]-std::sin(angle)*center[1],
                                     std::sin(angle)*center[0]+std::cos(angle)*center[1]);
    MWAWBox2f fBox;
    for (int i=0; i < 4; ++i) {
      MWAWVec2f pt=MWAWVec2f(box[i%2][0],box[i/2][1]);
      pt = MWAWVec2f(std::cos(angle)*pt[0]-std::sin(angle)*pt[1],
                     std::sin(angle)*pt[0]+std::cos(angle)*pt[1])+decal;
      if (i==0) fBox=MWAWBox2f(pt,pt);
      else fBox=fBox.getUnion(MWAWBox2f(pt,pt));
    }

    return fBox;
  }
  //! the graphic type
  Type m_type;
  //! the shape bdbox
  MWAWBox2f m_box;
  //! the shape origin
  MWAWVec2f m_origin;
  //! the graphic style
  MWAWGraphicStyle m_style;
  //! the pattern id
  int m_patternId;
  //! the graphic shape ( for basic geometric form )
  MWAWGraphicShape m_shape;
  //! flag to know if the shape is a line
  bool m_isLine;
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
  //! the 1D label width in point
  float m_labelWidth;
  //! the child list ( for a group )
  std::vector<size_t> m_childList;
  //! the list of bitmap id ( for a bitmap)
  std::vector<unsigned long> m_bitmapIdList;
  //! the list of bitmap dimension ( for a bitmap)
  std::vector<MWAWBox2i> m_bitmapDimensionList;
  //! a flag used to know if the object is sent to the listener or not
  mutable bool m_isSent;
};

////////////////////////////////////////
//! Internal: the state of a MacDraftParser
struct State {
  //! constructor
  State() : m_version(0), m_patternList(), m_shapeList(), m_idToBitmapMap()
  {
  }
  //! returns a pattern if posible
  bool getPattern(int id, MWAWGraphicStyle::Pattern &pat)
  {
    if (m_patternList.empty()) initPatterns();
    if (id<=0 || id>=int(m_patternList.size())) {
      MWAW_DEBUG_MSG(("MacDraftParserInternal::getPattern: can not find pattern %d\n", id));
      return false;
    }
    pat=m_patternList[size_t(id)];
    return true;
  }

  //! init the patterns list
  void initPatterns();
  //! the file version
  int m_version;
  //! the patterns list
  std::vector<MWAWGraphicStyle::Pattern> m_patternList;
  //! the shapes list
  std::vector<Shape> m_shapeList;
  //! map id to bitmap file data
  std::map<unsigned long, BitmapFileData> m_idToBitmapMap;
};

void State::initPatterns()
{
  if (!m_patternList.empty()) return;
  for (int i=0; i<64; ++i) {
    static uint16_t const(patterns[]) = {
      0x0, 0x0, 0x0, 0x0, 0x40, 0x400, 0x10, 0x100, 0x8040, 0x2010, 0x804, 0x201, 0x102, 0x408, 0x1020, 0x4080,
      0x0, 0x0, 0x0, 0x0, 0x842, 0x90, 0x440, 0x1001, 0xe070, 0x381c, 0xe07, 0x83c1, 0x8307, 0xe1c, 0x3870, 0xe0c1,
      0x8000, 0x0, 0x800, 0x0, 0x42a, 0x4025, 0x251, 0x2442, 0x4422, 0x88, 0x4422, 0x88, 0x1122, 0x4400, 0x1122, 0x4400,
      0x8000, 0x800, 0x8000, 0x800, 0x4aa4, 0x8852, 0x843a, 0x4411, 0x8844, 0x2211, 0x8844, 0x2211, 0x1122, 0x4488, 0x1122, 0x4488,
      0x8800, 0x2200, 0x8800, 0x2200, 0x4cd2, 0x532d, 0x9659, 0x46b3, 0x99cc, 0x6633, 0x99cc, 0x6633, 0x3366, 0xcc99, 0x3366, 0xcc99,
      0x8822, 0x8822, 0x8822, 0x8822, 0xdbbe, 0xedbb, 0xfeab, 0xbeeb, 0xcc00, 0x0, 0x3300, 0x0, 0x101, 0x1010, 0x101, 0x1010,
      0xaa55, 0xaa55, 0xaa55, 0xaa55, 0xf7bd, 0xff6f, 0xfbbf, 0xeffe, 0x2040, 0x8000, 0x804, 0x200, 0x40a0, 0x0, 0x40a, 0x0,
      0x77dd, 0x77dd, 0x77dd, 0x77dd, 0x8244, 0x3944, 0x8201, 0x101, 0xff00, 0x0, 0xff00, 0x0, 0x8888, 0x8888, 0x8888, 0x8888,
      0xffff, 0xffff, 0xffff, 0xffff, 0x8142, 0x3c18, 0x183c, 0x4281, 0xb130, 0x31b, 0xb8c0, 0xc8d, 0x6c92, 0x8282, 0x4428, 0x1000,
      0xff80, 0x8080, 0xff80, 0x8080, 0x8142, 0x2418, 0x1020, 0x4080, 0xff80, 0x8080, 0xff08, 0x808, 0x8080, 0x413e, 0x808, 0x14e3,
      0xff88, 0x8888, 0xff88, 0x8888, 0xff80, 0x8080, 0x8080, 0x8080, 0xbf00, 0xbfbf, 0xb0b0, 0xb0b0, 0xaa00, 0x8000, 0x8800, 0x8000,
      0xaa44, 0xaa11, 0xaa44, 0xaa11, 0x8244, 0x2810, 0x2844, 0x8201, 0x8, 0x142a, 0x552a, 0x1408, 0x1038, 0x7cfe, 0x7c38, 0x1000,
      0x1020, 0x54aa, 0xff02, 0x408, 0x8080, 0x8080, 0x8094, 0xaa55, 0x804, 0x2a55, 0xff40, 0x2010, 0x7789, 0x8f8f, 0x7798, 0xf8f8,
      0x8814, 0x2241, 0x8800, 0xaa00, 0x77eb, 0xddbe, 0x77ff, 0x55ff, 0x1022, 0x408a, 0x4022, 0x108a, 0xefdd, 0xbf75, 0xbfdd, 0xef75,
      0x9f90, 0x909f, 0xf909, 0x9f9, 0xf078, 0x2442, 0x870f, 0x1221, 0xfe82, 0xfeee, 0xef28, 0xefee, 0xf9fc, 0x664f, 0x9f3f, 0x66f3,
      0xaf5f, 0xaf5f, 0xd0b, 0xd0b, 0xa011, 0xa1c, 0x2844, 0x82c1, 0xf0f0, 0xf0f0, 0xf0f, 0xf0f, 0xc864, 0x3219, 0x9923, 0x468c
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
////////////////////////////////////////
//! Internal: the subdocument of a MacDraftParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MacDraftParser &pars, MWAWInputStreamPtr input, int zoneId) : MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

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
    MWAW_DEBUG_MSG(("MacDraftParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  MacDraftParser *parser=dynamic_cast<MacDraftParser *>(m_parser);
  if (!parser) {
    MWAW_DEBUG_MSG(("MacDraftParserInternal::SubDocument::parse: no parser\n"));
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
MacDraftParser::MacDraftParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWGraphicParser(input, rsrcParser, header), m_state()
{
  init();
}

MacDraftParser::~MacDraftParser()
{
}

void MacDraftParser::init()
{
  resetGraphicListener();
  setAsciiName("main-1");

  m_state.reset(new MacDraftParserInternal::State);

  getPageSpan().setMargins(0.1);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MacDraftParser::parse(librevenge::RVNGDrawingInterface *docInterface)
{
  if (!getInput().get() || !checkHeader(0L))  throw(libmwaw::ParseException());
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
        MacDraftParserInternal::Shape const &shape=m_state->m_shapeList[i];
        if (shape.m_isSent) continue;
        send(shape);
        if (shape.m_nextId>0 && shape.m_nextId>int(i))
          i=size_t(shape.m_nextId-1);
      }
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MacDraftParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetGraphicListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MacDraftParser::createDocument(librevenge::RVNGDrawingInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getGraphicListener()) {
    MWAW_DEBUG_MSG(("MacDraftParser::createDocument: listener already exist\n"));
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
bool MacDraftParser::createZones()
{
  if (!readDocHeader())
    return false;
  MWAWInputStreamPtr input = getInput();

  // first the list of patterns (often empty)
  while (readPattern()) {
  }
  long pos=input->tell();
  long fSz=(long) input->readULong(2);
  if (fSz==0) {
    ascii().addPos(pos);
    ascii().addNote("Patterns[end]:");
  }
  else {
    MWAW_DEBUG_MSG(("MacDraftParser::createZones: can not find end patterns zone\n"));
    ascii().addPos(pos);
    ascii().addNote("###");
    input->seek(pos,librevenge::RVNG_SEEK_SET);
  }
  // bitmap data
  if (m_state->m_version==1) {
    while (readBitmapData()) {
    }
    pos=input->tell();
    fSz=(long) input->readULong(2);
    if (fSz==0) {
      ascii().addPos(pos);
      ascii().addNote("BitmapData[end]:");
    }
    else {
      MWAW_DEBUG_MSG(("MacDraftParser::createZones: can not find end bitmap data zone\n"));
      ascii().addPos(pos);
      ascii().addNote("###");
      input->seek(pos,librevenge::RVNG_SEEK_SET);
    }
  }

  // second the list of objects (can be empty)
  while (readObject()) {
  }

  pos=input->tell();
  fSz=(long) input->readULong(2);
  if (fSz==0) {
    ascii().addPos(pos);
    ascii().addNote("Object[end]:");
  }
  else {
    MWAW_DEBUG_MSG(("MacDraftParser::createZones: can not find end object zone\n"));
    ascii().addPos(pos);
    ascii().addNote("###");
    input->seek(pos,librevenge::RVNG_SEEK_SET);
  }

  // never seems
  for (int i=0; i<2; ++i) {
    while (!input->isEnd()) {
      pos=input->tell();
      fSz=(long) input->readULong(2);
      if (!input->checkPosition(pos+2+fSz)) {
        input->seek(pos,librevenge::RVNG_SEEK_SET);
        break;
      }
      if (fSz==0) {
        ascii().addPos(pos);
        ascii().addNote("_");
        break;
      }
      else {
        MWAW_DEBUG_MSG(("MacDraftParser::createZones: find some unknown zone\n"));
        ascii().addPos(pos);
        ascii().addNote(i==0 ? "Entries(ZoneA):" : "Entries(ZoneB):");
        input->seek(pos+2+fSz,librevenge::RVNG_SEEK_SET);
      }
    }
  }
  if (m_state->m_version==1) {
    if (input->isEnd())
      return true;
    MWAW_DEBUG_MSG(("MacDraftParser::createZones: find some extra zone\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Extra):###");
    return !m_state->m_shapeList.empty();
  }
  // bitmap data
  while (readBitmapData()) {
  }
  pos=input->tell();
  fSz=(long) input->readULong(2);
  if (fSz==0) {
    ascii().addPos(pos);
    ascii().addNote("BitmapData[end]:");
  }
  else {
    MWAW_DEBUG_MSG(("MacDraftParser::createZones: can not find end bitmap data zone\n"));
    ascii().addPos(pos);
    ascii().addNote("###");
    input->seek(pos,librevenge::RVNG_SEEK_SET);
  }

  // modified patterns list
  while (readPattern()) {
  }
  pos=input->tell();
  fSz=(long) input->readULong(2);
  if (fSz==0) {
    ascii().addPos(pos);
    ascii().addNote("Patterns[end]:");
  }
  else {
    MWAW_DEBUG_MSG(("MacDraftParser::createZones: can not find end of pattern zone\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Extra):###");
    return !m_state->m_shapeList.empty();
  }

  if (input->isEnd())
    return !m_state->m_shapeList.empty();
  pos=input->tell();
  fSz=(long) input->readULong(2);
  input->seek(pos,librevenge::RVNG_SEEK_SET);
  if (fSz==0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  else if (fSz==0x78) {
    if (!readPrintInfo()) {
      ascii().addPos(pos);
      ascii().addNote("Entries(PrintInfo):#");
    }
    input->seek(pos+2+fSz,librevenge::RVNG_SEEK_SET);
  }

  if (!input->isEnd()) {
    MWAW_DEBUG_MSG(("MacDraftParser::createZones: find some extra zone\n"));
    ascii().addPos(input->tell());
    ascii().addNote("Entries(Extra):###");
  }
  return true;
}

bool MacDraftParser::readZone()
{
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd())
    return false;
  long pos=input->tell();
  long fSz=(long) input->readULong(2);
  if (fSz==0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  input->seek(pos,librevenge::RVNG_SEEK_SET);
  if (fSz==0x1e)
    return readPattern();
  if (fSz==0x78 && readPrintInfo())
    return true;
  if (!input->checkPosition(pos+2+fSz)) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote("Entries(UnknZone):");
  input->seek(pos+2+fSz,librevenge::RVNG_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
// read an object
////////////////////////////////////////////////////////////
bool MacDraftParser::readObject()
{
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd())
    return false;
  long pos=input->tell();
  long fSz=(long) input->readULong(2);
  if (fSz==0x1a) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return readLabel();
  }
  long endPos=pos+2+fSz;
  if (fSz<0x20 || !input->checkPosition(endPos)) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(Object):";
  int val;
  for (int i=0; i<2; ++i) {
    val=(int) input->readULong(4);
    if (val) f << "id" << i << "=" << std::hex << val << std::dec << ",";
  }
  size_t shapeId= m_state->m_shapeList.size();
  MacDraftParserInternal::Shape shape;
  shape.m_id=(int) shapeId;
  shape.m_nextId=shape.m_id+1; // default value
  unsigned long flag=input->readULong(4);
  switch ((flag>>3)&7) {
  case 0:
    break;
  case 1:
    shape.m_style.m_arrows[1]=true;
    f << "->,";
    break;
  case 2:
    shape.m_style.m_arrows[0]=true;
    f << "<-,";
    break;
  case 3:
    shape.m_style.m_arrows[0]=shape.m_style.m_arrows[1]=true;
    f << "<->,";
    break;
  case 4:
    shape.m_style.m_arrows[0]=shape.m_style.m_arrows[1]=true;
    f << "<->+length,";
    break;
  default:
    f << "##arrow=" << ((flag>>3)&7) << ",";
  }
  switch ((flag>>9)&0x3) {
  case 0:
    f << "line[inside],";
    break;
  case 1: // centered
    break;
  case 2:
    f << "line[outside],";
    break;
  default:
    f << "line[pos]=##3,";
  }
  shape.m_patternId=(flag>>11)&0x3f;
  if (shape.m_patternId)
    f << "pat=" << shape.m_patternId << ",";
  else
    shape.m_style.m_surfaceOpacity=0;
  if (flag&0x40) {
    shape.m_style.m_lineWidth=0.; // or maybe dotted
    f << "hairline,";
  }
  else {
    shape.m_style.m_lineWidth=float((flag>>23)&7)+1;
    f << "width=" << ((flag>>23)&7)+1 << ",";
  }
  if (flag&0x80000000)
    f << "select,";
  flag &= 0x7c7e0187;

  if (flag>>16) f << "fl0[h]=" << std::hex << (flag>>16) << std::dec << ",";
  if (flag&0xFFFF) f << "fl0[l]=" << std::hex << (flag&0xFFFF) << std::dec << ",";

  val=(int) input->readULong(2);
  if (val&0x8000) f << "lock,";
  val &= 0x7fff;
  if (val) f << "fl1=" << std::hex << val << std::dec << ",";

  val=(int) input->readULong(2);
  bool flipX=false;
  float rotate=0;
  if (val&0xfc00)
    f << "rot[h]=" << std::hex << (val>>10) << std::dec << ",";
  if (val&0x200) {
    flipX=true;
    f << "flipX,";
  }
  val &=0x1ff;
  if (val) {
    rotate=float(val);
    f << "rot=" << val << ",";
  }
  float dim[4];
  for (int i=0; i<4; ++i) dim[i]=float(input->readLong(2))/8.f;
  shape.m_box=MWAWBox2f(MWAWVec2f(dim[1],dim[0]), MWAWVec2f(dim[3],dim[2]));
  f << "dim=" << shape.m_box << ",";
  for (int i=0; i<2; ++i) dim[i]=float(input->readLong(2))/8.f;
  MWAWVec2f origin=MWAWVec2f(dim[1],dim[0]);
  f << "orig=" << origin << ",";
  switch (fSz) {
  case 0x28:
  case 0x2c:
  case 0x30:
  case 0x38: {
    shape.m_type=MacDraftParserInternal::Shape::Basic;
    val=(int) input->readULong(4);
    if (val) f << "id?=" << std::hex << val << std::dec << ",";
    for (int i=0; i<4; ++i) dim[i]=float(input->readLong(2))/8.f;
    MWAWBox2f box(MWAWVec2f(dim[1],dim[0]), MWAWVec2f(dim[3],dim[2]));
    f << "dim1=" << box << ",";
    if (fSz==0x28) {
      if (flag&0x8000000) {
        shape.m_shape=MWAWGraphicShape::circle(box);
        f << "ellipse,";
      }
      else {
        shape.m_shape=MWAWGraphicShape::rectangle(box);
        f << "rect,";
      }
      break;
    }
    else if (fSz==0x2c) {
      shape.m_shape=MWAWGraphicShape::rectangle(box);
      f << "rectOval,";
    }
    else if (fSz==0x30) {
      if ((flag&0x28000000)==0x28000000) {
        shape.m_shape=MWAWGraphicShape::circle(box);
        f << "circle,";
      }
      else if ((flag&0x8000000)==0x8000000) {
        shape.m_type=MacDraftParserInternal::Shape::Bitmap;
        f << "bitmap,";
      }
      else {
        shape.m_type=MacDraftParserInternal::Shape::Group;
        f << "group,";
      }
    }
    else if (fSz==0x38)
      f << "pie,";
    else {
      static bool first=true;
      if (first) {
        MWAW_DEBUG_MSG(("MacDraftParser::readObject: find some bitmap\n"));
        first=false;
      }
      f << "Bitmap###,";
    }
    if (fSz==0x30) {
      val=(int) input->readULong(4);
      if (val)
        f << "id2=" << std::hex << val << std::dec << ",";
    }
    else if (fSz==0x38) {
      int fileAngle[2];
      for (int i=0; i<2; ++i) fileAngle[i]=(int) input->readLong(2);
      f << "angle=" << fileAngle[0] << "x" << fileAngle[0]+fileAngle[1] << ",";
      if (fileAngle[1]<0) {
        fileAngle[0]+=fileAngle[1];
        fileAngle[1]*=-1;
      }
      int angle[2] = { 90-fileAngle[0]-fileAngle[1], 90-fileAngle[0] };
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
      MWAWVec2f axis = 0.5*box.size();
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
      MWAWVec2f center = box.center();
      MWAWBox2f realBox(MWAWVec2f(center[0]+minVal[0],center[1]+minVal[1]),
                        MWAWVec2f(center[0]+maxVal[0],center[1]+maxVal[1]));
      shape.m_box=MWAWBox2f(MWAWVec2f(shape.m_box[0])+realBox[0],MWAWVec2f(shape.m_box[0])+realBox[1]);
      if (shape.m_patternId!=0)
        shape.m_shape = MWAWGraphicShape::pie(realBox, box, MWAWVec2f(float(angle[0]),float(angle[1])));
      else
        shape.m_shape = MWAWGraphicShape::arc(realBox, box, MWAWVec2f(float(angle[0]),float(angle[1])));

      for (int i=0; i<2; ++i) { // always 0
        val=(int) input->readULong(2);
        if (val) f << "f" << i+2 << "=" << std::hex << val << std::dec << ",";
      }
      for (int i=0; i<2; ++i) dim[i]=float(input->readLong(2))/8.f;
      f << "sz=" << MWAWVec2f(dim[1],dim[0]) << ",";
    }
    for (int i=0; i<2; ++i) dim[i]=float(input->readLong(2))/8.f;
    if (fSz==0x2c)
      shape.m_shape.m_cornerWidth=MWAWVec2f(dim[1]/2.f,dim[0]/2.f);
    f << "corner=" << MWAWVec2f(dim[1],dim[0]) << ",";
    if (shape.m_type!=MacDraftParserInternal::Shape::Group && shape.m_type!=MacDraftParserInternal::Shape::Bitmap)
      break;
    if (input->tell()!=endPos)
      ascii().addDelimiter(input->tell(),'|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(endPos,librevenge::RVNG_SEEK_SET);

    if (shape.m_type==MacDraftParserInternal::Shape::Bitmap) {
      while (!input->isEnd()) {
        pos=input->tell();
        if (input->readULong(2)==0) {
          ascii().addPos(pos);
          ascii().addNote("Bitmap[end]");
          break;
        }
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        if (!readBitmapDefinition(shape)) {
          MWAW_DEBUG_MSG(("MacDraftParser::readObject: can not find end of bitmap\n"));
          input->seek(pos, librevenge::RVNG_SEEK_SET);
          ascii().addPos(pos);
          ascii().addNote("Bitmap###");
          break;
        }
      }
      m_state->m_shapeList.push_back(shape);
      return true;
    }

    m_state->m_shapeList.push_back(shape);
    while (!input->isEnd()) {
      pos=input->tell();
      if (input->readULong(2)==0) {
        ascii().addPos(pos);
        ascii().addNote("Object[endGroup]");
        break;
      }
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      size_t cId=m_state->m_shapeList.size();
      if (!readObject()) {
        MWAW_DEBUG_MSG(("MacDraftParser::readObject: can not find end of group\n"));
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
      m_state->m_shapeList[shapeId].m_childList.push_back(size_t(cId));
    }
    m_state->m_shapeList[shapeId].m_nextId=(int) m_state->m_shapeList.size();
    // times to apply transformation on child
    if (origin!=MWAWVec2f(0,0)) {
      for (int i=m_state->m_shapeList[shapeId].m_id+1; i<m_state->m_shapeList[shapeId].m_nextId; ++i)
        m_state->m_shapeList[size_t(i)].translate(origin);
    }
    if (rotate<0 || rotate>0 || flipX) {
      MWAWVec2f center=origin+box.center();
      for (int i=m_state->m_shapeList[shapeId].m_id+1; i<m_state->m_shapeList[shapeId].m_nextId; ++i)
        m_state->m_shapeList[size_t(i)].transform(rotate,flipX,center);
    }
    return true;
  }
  case 0x32: {
    f << "line,";
    val=(int) input->readULong(4);
    if (val) f << "pat[id]=" << std::hex << val << ",";
    for (int i=0; i<2; ++i) { // 0
      val=(int) input->readULong(2);
      if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
    }
    for (int i=0; i<2; ++i) dim[i]=float(input->readLong(2))/8.f;
    if (dim[0]<0 || dim[0]>0 || dim[1]<0 || dim[1]>0)
      f << "center=" << MWAWVec2f(dim[1],dim[0]) << ",";
    val=(int) input->readLong(2); // always 2
    if (val != 1)
      f << "f2=" << val << ",";
    for (int i=0; i<4; ++i) dim[i]=float(input->readLong(2))/8.f;
    MWAWBox2f box(MWAWVec2f(dim[1],dim[0]), MWAWVec2f(dim[3],dim[2]));
    f << "line=" << box << ",";
    shape.m_type=MacDraftParserInternal::Shape::Basic;
    shape.m_shape=MWAWGraphicShape::line(box[0], box[1]);
    shape.m_isLine=true;
    break;
  }
  default: {
    int type=(flag>>26)&0x1f;
    if (type==3) {
      f << "textbox,";
      shape.m_type=MacDraftParserInternal::Shape::Text;
      shape.m_style.m_lineWidth=0;
      for (int i=0; i<2; ++i) {
        val=(int) input->readLong(2);
        if (val)
          f << "f" << i << "=" << val << ",";
      }
      for (int i=0; i<4; ++i) dim[i]=float(input->readLong(2))/8.f;
      MWAWBox2f box(MWAWVec2f(dim[1],dim[0]), MWAWVec2f(dim[3],dim[2]));
      shape.m_box=box;
      f << "box=" << box << ",";
      f << "unkn=[";
      for (int i=0; i<2; ++i) {
        val=(int) input->readULong(2);
        if (val)
          f << std::hex << val << std::dec << ",";
        else
          f << "_,";
      }
      f << "],";
      MWAWFont &font=shape.m_font;
      val=(int) input->readULong(1);
      uint32_t flags=0;
      if (val&0x1) flags |= MWAWFont::boldBit;
      if (val&0x2) flags |= MWAWFont::italicBit;
      if (val&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
      if (val&0x8) flags |= MWAWFont::embossBit;
      if (val&0x10) flags |= MWAWFont::shadowBit;
      if (val>>5) f << "#flag[font]=" << (val>>5) << ",";
      val=(int) input->readULong(1);
      if (val) f << "flag1[font]=" << val << ",";
      font.setId((int) input->readULong(1));
      val=(int) input->readULong(1);
      if (val)
        font.setSize((float) val);
      font.setFlags(flags);
      f << "font=[" << font.getDebugString(getParserState()->m_fontConverter) << "],";
      val=(int) input->readLong(1);
      switch (val) {
      case -1:
        shape.m_paragraph.m_justify = MWAWParagraph::JustificationRight;
        f << "right,";
        break;
      case 0:
        break;
      case 1:
        shape.m_paragraph.m_justify = MWAWParagraph::JustificationCenter;
        f << "center,";
        break;
      default:
        MWAW_DEBUG_MSG(("MacDraftParser::readObject: find unknown align\n"));
        f << "##align=" << val << ",";
        break;
      }
      val=(int) input->readLong(1); // find 2
      if (val) f << "f4=" << val << ",";
      val=(int) input->readLong(1);
      switch (val) {
      case 0:
        break;
      case 1:
        shape.m_paragraph.setInterline(1.5, librevenge::RVNG_PERCENT);
        f << "interline=150%,";
        break;
      case 2:
        shape.m_paragraph.setInterline(2, librevenge::RVNG_PERCENT);
        f << "interline=200%,";
        break;
      default:
        MWAW_DEBUG_MSG(("MacDraftParser::readObject: find unknown interline\n"));
        f << "##interline=" << val << ",";
        break;
      }
      val=(int) input->readLong(1);
      switch (val) { // the string must already be in lowercase, ..., so
      case 0:
        break;
      case 1:
        f << "upper,";
        break;
      case 2:
        f << "lower,";
        break;
      case 3:
        f << "title,";
        break;
      default:
        MWAW_DEBUG_MSG(("MacDraftParser::readObject: find unknown position\n"));
        f << "##position=" << val << ",";
        break;
      }
      val=(int) input->readLong(2);
      if (val!=1) // find 1 or 3
        f << "f5=" << val << ",";
      int N=(int) input->readULong(2);
      if (input->tell()+4+N>endPos) {
        MWAW_DEBUG_MSG(("MacDraftParser::readObject: can not read the number of char \n"));
        f << "##N=" << N << ",";
        break;
      }
      val=(int) input->readLong(2);
      if (val!=1) // find always 1
        f << "f6=" << val << ",";
      std::string text("");
      shape.m_textEntry.setBegin(input->tell());
      shape.m_textEntry.setLength(N);
      for (int i=0; i<N; ++i) text+=(char) input->readULong(1);
      f << text << ",";
      val=(int) input->readULong(2);
      if (val)
        f << "fl3=" << std::hex << val << std::dec << ",";
      break;
    }
    val=(int) input->readULong(4);
    if (val) f << "pat[id]=" << std::hex << val << ",";
    for (int i=0; i<4; ++i) dim[i]=float(input->readLong(2))/8.f;
    int N=(int) input->readULong(2);
    if (N*4+46==fSz) {
      f << "poly, box=" << MWAWBox2f(MWAWVec2f(dim[1],dim[0]), MWAWVec2f(dim[3],dim[2])) << ",";
      shape.m_type=MacDraftParserInternal::Shape::Basic;
      shape.m_shape.m_type = MWAWGraphicShape::Polygon;
      std::vector<MWAWVec2f> &vertices=shape.m_shape.m_vertices;
      MWAWBox2f box;
      f << "pts=[";
      for (int i=0; i<N; ++i) {
        for (int j=0; j<2; ++j) dim[j]=float(input->readLong(2))/8.f;
        MWAWVec2f point=MWAWVec2f(dim[1],dim[0]);
        vertices.push_back(point);
        if (i==0)
          box=MWAWBox2f(point, point);
        else
          box=box.getUnion(MWAWBox2f(point, point));
        f << point << ",";
      }
      shape.m_shape.m_bdBox=shape.m_shape.m_formBox=box;
      f << "],";
      break;
    }
    input->seek(-10, librevenge::RVNG_SEEK_CUR);
    // happens in a converted file: a quite horrible file
    MWAW_DEBUG_MSG(("MacDraftParser::readObject: find unknown data\n"));
    f << "UNKN###,";
    break;
  }
  }
  shape.transform(rotate, flipX, shape.getBdBox().center());
  shape.translate(origin);
  m_state->m_shapeList.push_back(shape);
  if (input->tell()!=endPos)
    ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read a bitmap
////////////////////////////////////////////////////////////

bool MacDraftParser::readBitmapData()
{
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd())
    return false;
  long pos=input->tell();
  long fSz=(long) input->readULong(2);
  long endPos=pos+2+fSz;
  if (fSz<28 || !input->checkPosition(endPos)) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return false;
  }
  MacDraftParserInternal::BitmapFileData data;
  libmwaw::DebugStream f;
  f << "Entries(BitmapData):";
  long val=(long) input->readULong(4);
  if (val)
    f << "id[next]=" << std::hex << val << std::dec << ",";
  val=(long) input->readULong(2);
  if (val) f << "fl="  << std::hex << val << std::dec << ",";
  data.m_id=input->readULong(4);
  f << "id=" << std::hex << data.m_id << std::dec << ",";
  val=input->readLong(2); // 1 or 2
  if (val) f << "f0=" << val << ",";
  val=input->readLong(2); // 0
  if (val)  f << "f1=" << val << ",";
  f << "id2=" << std::hex << input->readULong(4) << std::dec << ","; // 80000000+id
  data.m_rowSize=(int) input->readULong(2); // 2|4|72 : number of bytes by row
  if (data.m_rowSize) f << "row[size]=" << data.m_rowSize << ",";
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
  data.m_dimension=MWAWBox2i(MWAWVec2i(dim[1],dim[0]),MWAWVec2i(dim[3],dim[2]));
  f << "dim=" << data.m_dimension << ",";
  data.m_entry.setBegin(input->tell());
  data.m_entry.setEnd(endPos);
  if (!data.ok()) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return false;
  }
  if (data.m_rowSize*data.m_dimension.size()[1]+28!=fSz) {
    MWAW_DEBUG_MSG(("MacDraftParser::readBitmapData: the bitmap size seems odd\n"));
    f << "###sz,";
  }
  if (m_state->m_idToBitmapMap.find(data.m_id)!=m_state->m_idToBitmapMap.end()) {
    MWAW_DEBUG_MSG(("MacDraftParser::readBitmapData: the id is already defined\n"));
    f << "###id,";
  }
  else
    m_state->m_idToBitmapMap[data.m_id]=data;
  if (data.m_entry.length())
    ascii().skipZone(data.m_entry.begin(), data.m_entry.end()-1);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  return true;
}

bool MacDraftParser::readBitmapDefinition(MacDraftParserInternal::Shape &bitmap)
{
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd())
    return false;
  long pos=input->tell();
  long fSz=(long) input->readULong(2);
  long endPos=pos+2+fSz;
  if (fSz!=0x3e || !input->checkPosition(endPos)) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(BitmapDef):";
  int val;
  for (int i=0; i<2; ++i) {
    val=(int) input->readULong(4);
    if (val) f << "id" << i << "=" << std::hex << val << std::dec << ",";
  }
  unsigned long flag=input->readULong(4);
  if (flag>>16) f << "fl0[h]=" << std::hex << (flag>>16) << std::dec << ",";
  if (flag&0xFFFF) f << "fl0[l]=" << std::hex << (flag&0xFFFF) << std::dec << ",";

  val=(int) input->readULong(2);
  if (val&0x8000) f << "lock,";
  val &= 0x7fff;
  if (val) f << "fl1=" << std::hex << val << std::dec << ",";

  val=(int) input->readULong(2);
  if (val&0xfc00)
    f << "rot[h]=" << std::hex << (val>>10) << std::dec << ",";
  if (val&0x200) {
    f << "flipX,";
  }
  val &=0x1ff;
  if (val)
    f << "rot=" << float(val) << ",";
  float dim[4];
  for (int i=0; i<4; ++i) dim[i]=float(input->readLong(2))/8.f;
  f << "dim=" << MWAWBox2f(MWAWVec2f(dim[1],dim[0]), MWAWVec2f(dim[3],dim[2])) << ",";
  for (int i=0; i<2; ++i) dim[i]=float(input->readLong(2))/8.f;
  f << "orig=" << MWAWVec2f(dim[1],dim[0]) << ",";

  val=(int) input->readULong(4);
  if (val) f << "id?=" << std::hex << val << std::dec << ",";
  for (int i=0; i<4; ++i) dim[i]=float(input->readLong(2))/8.f;
  f << "dim1=" << MWAWBox2f(MWAWVec2f(dim[1],dim[0]), MWAWVec2f(dim[3],dim[2])) << ",";
  unsigned long id=input->readULong(4);
  bitmap.m_bitmapIdList.push_back(id);
  f << "id[data]=" << std::hex << id << std::dec << ",";
  val=(int) input->readLong(2); // 0|1
  if (val) f << "g0=" << val << ",";
  for (int i=0; i<4; ++i) dim[i]=float(input->readLong(2));
  MWAWBox2f box(MWAWVec2f(dim[1],dim[0]), MWAWVec2f(dim[3],dim[2]));
  bitmap.m_bitmapDimensionList.push_back(box);
  f << "dim2=" << box << ",";
  for (int i=0; i<4; ++i) dim[i]=float(input->readLong(2));
  f << "dim3=" << MWAWBox2f(MWAWVec2f(dim[1],dim[0]), MWAWVec2f(dim[3],dim[2])) << ",";

  if (input->tell()!=endPos)
    ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read a pattern
////////////////////////////////////////////////////////////
bool MacDraftParser::readPattern()
{
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd())
    return false;
  long pos=input->tell();
  long fSz=(long) input->readULong(2);
  long endPos=pos+2+fSz;
  if (fSz!=0x1e || !input->checkPosition(endPos)) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(Pattern):";
  long val=(long) input->readULong(4);
  if (val) f << "id=" << std::hex << val << std::dec << ",";
  int id=(int) input->readULong(2);
  if (id==0xf0)
    f << "default,";
  else if (id<64)
    f << "id=" << id << ",";
  else {
    MWAW_DEBUG_MSG(("MacDraftParser::readPattern: find odd id\n"));
    f << "##id=" << id << ",";
  }
  val=(long) input->readULong(4);
  if (val) f << "id[prev?]=" << std::hex << val << std::dec << ",";
  for (int i=0; i<2; ++i) { //3,3 | 4,4 | 5,5
    val=input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  MWAWGraphicStyle::Pattern pat;
  pat.m_dim=MWAWVec2i(8,8);
  pat.m_data.resize(8);
  pat.m_colors[0]=MWAWColor::white();
  pat.m_colors[1]=MWAWColor::black();

  for (size_t i=0; i<8; ++i) pat.m_data[i]=uint8_t(input->readULong(1));
  f << "orig=" << pat << ",";
  for (size_t i=0; i<8; ++i) pat.m_data[i]=uint8_t(input->readULong(1));
  f << "final=" << pat << ",";
  if (id>=0 && id<64) {
    m_state->initPatterns();
    if (id<(int) m_state->m_patternList.size())
      m_state->m_patternList[size_t(id)]=pat;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  return true;
}
////////////////////////////////////////////////////////////
// read an label
////////////////////////////////////////////////////////////
bool MacDraftParser::readLabel()
{
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd())
    return false;
  long pos=input->tell();
  long fSz=(long) input->readULong(2);
  long endPos=pos+2+fSz;
  if (fSz!=0x1a || !input->checkPosition(endPos)) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return false;
  }
  size_t shapeId= m_state->m_shapeList.size();
  MacDraftParserInternal::Shape shape;
  shape.m_id=(int) shapeId;
  shape.m_nextId=shape.m_id+1;
  shape.m_type=MacDraftParserInternal::Shape::Label;
  shape.m_style.m_lineWidth=0;
  shape.m_style.setSurfaceColor(MWAWColor::white());
  libmwaw::DebugStream f;
  f << "Entries(Label):";
  int val;
  for (int i=0; i<2; ++i) {
    val=(int) input->readULong(4);
    if (val) f << "id" << i << "=" << std::hex << val << std::dec << ",";
  }
  unsigned long flag=input->readULong(4);
  MWAWFont &font=shape.m_font;
  if ((flag & 0x7F))
    font.setSize(float(flag & 0x7F));
  font.setId((flag>>8) & 0xFF);
  uint32_t flags=0;
  if (flag&0x1000000) flags |= MWAWFont::boldBit;
  if (flag&0x2000000) flags |= MWAWFont::italicBit;
  if (flag&0x4000000) font.setUnderlineStyle(MWAWFont::Line::Simple);
  if (flag&0x8000000) flags |= MWAWFont::embossBit;
  if (flag&0x10000000) flags |= MWAWFont::shadowBit;
  if (flag&(unsigned long)(0x80000000)) f << "select,";
  flag &= (unsigned long)(0x60FF0080);
  if (flag>>16) f << "fl0[h]=" << std::hex << (flag>>16) << std::dec << ",";
  if (flag&0xFFFF) f << "fl0[l]=" << std::hex << (flag&0xFFFF) << std::dec << ",";

  flag=input->readULong(4);
  switch (flag&3) {
  case 1:
    flags |= MWAWFont::allCapsBit;
    break;
  case 2:
    flags |= MWAWFont::lowercaseBit;
    break;
  case 3:
    f << "title,";
    flags |= MWAWFont::smallCapsBit;
    break;
  case 0:
  default:
    break;
  }
  font.setFlags(flags);
  f << "font=[" << font.getDebugString(getParserState()->m_fontConverter) << "],";
  switch ((flag>>8)&3) {
  case 0:
    break;
  case 1:
    shape.m_paragraph.setInterline(1.5, librevenge::RVNG_PERCENT);
    f << "interline=150%,";
    break;
  case 2:
    shape.m_paragraph.setInterline(200, librevenge::RVNG_PERCENT);
    f << "interline=200%,";
    break;
  default:
    MWAW_DEBUG_MSG(("MacDraftParser::readObject: find unknown interline\n"));
    f << "##interline=3,";
    break;
  }
  switch ((flag>>24)) {
  case 0:
    break;
  case 0xff:
    shape.m_paragraph.m_justify = MWAWParagraph::JustificationRight;
    f << "right,";
    break;
  case 1:
    shape.m_paragraph.m_justify = MWAWParagraph::JustificationCenter;
    f << "center,";
    break;
  default:
    MWAW_DEBUG_MSG(("MacDraftParser::readObject: find unknown align\n"));
    f << "##align=" << (flag>>24) << ",";
    break;
  }

  flag &= 0xFFFCFC;
  if (flag>>16) f << "fl1[h]=" << std::hex << (flag>>16) << std::dec << ",";
  if (flag&0xFFFF) f << "fl1[l]=" << std::hex << (flag&0xFFFF) << std::dec << ",";

  float dim[4];
  for (int i=0; i<4; ++i) dim[i]=float(input->readLong(2))/8.f;
  shape.m_box=MWAWBox2f(MWAWVec2f(dim[1],dim[0]), MWAWVec2f(dim[3],dim[2]));
  f << shape.m_box << ",";
  val=(int) input->readULong(2);
  if (val!=0x204)
    f << "fl2=" << std::hex << val << std::dec << ",";
  m_state->m_shapeList.push_back(shape);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MacDraftParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MacDraftParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->checkPosition(0x270))
    return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";
  input->seek(0, librevenge::RVNG_SEEK_SET);
  if (input->readULong(2)!=2 || input->readULong(2)!=0 || input->readULong(2)!=2 ||
      input->readULong(2)!=0x262 || input->readULong(2)!=0x262)
    return false;
  int const vers=1;
  int val=(int) input->readULong(2);
  if (val)
    f << "f0=" << val << ",";
  f << "id=" << std::hex << input->readULong(4) << std::dec << ",";
  for (int i=0; i<2; ++i) {// f0=fc[4c]0, f1=[09]200
    val=(int) input->readULong(2);
    if (val) f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  f << "id1=" << std::hex << input->readULong(4) << std::dec << ",";
  val=(int) input->readLong(2); // always 0
  if (val)
    f << "f1=" << val << ",";
  f << "ids=[";
  for (int i=0; i<5; ++i) { // maybe junk
    val=(int) input->readULong(4);
    if (val)
      f << std::hex << val << std::dec << ",";
    else
      f << "_,";
  }
  f << "],";
  val=(int) input->readLong(2); // 0|1
  if (val)
    f << "f2=" << val << ",";
  int sSz=(int) input->readULong(1);
  if (sSz>31) {
    if (strict)
      return false;
    MWAW_DEBUG_MSG(("MacDraftParser::checkHeader: string size seems bad\n"));
    f << "##sSz=" << sSz << ",";
  }
  else {
    std::string name("");
    for (int i=0; i<sSz; ++i) name+=(char) input->readULong(1);
    f << name << ",";
  }
  input->seek(80, librevenge::RVNG_SEEK_SET);
  val=(int) input->readULong(4);
  if (val)
    f << "id2=" << std::hex << val << std::dec << ",";

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());
  if (strict) {
    input->seek(0x270,librevenge::RVNG_SEEK_SET);
    for (int i=0; i<10; ++i) {
      if (input->isEnd())
        break;
      long pos=input->tell();
      long fSz=(int) input->readULong(2);
      if (!input->checkPosition(pos+2+fSz))
        return false;
      input->seek(pos+2+fSz, librevenge::RVNG_SEEK_SET);
    }
    input->seek(84, librevenge::RVNG_SEEK_SET);
  }
  setVersion(vers);
  m_state->m_version=vers;
  if (header)
    header->reset(MWAWDocument::MWAW_T_MACDRAFT, vers, MWAWDocument::MWAW_K_DRAW);

  return true;
}

////////////////////////////////////////////////////////////
// read the document header
////////////////////////////////////////////////////////////
bool MacDraftParser::readDocHeader()
{
  MWAWInputStreamPtr input = getInput();
  if (!input->checkPosition(0x262)) {
    MWAW_DEBUG_MSG(("MacDraftParser::readDocHeader: the file seems too short\n"));
    return false;
  }
  input->seek(84,librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(DocHeader):";
  int val;
  for (int i=0; i<16; ++i) {
    val=(int) input->readULong(2);
    static int const(expected[])= {0,1,0,0,0xdfff,0xffff,0,0, 0xeed7, 0xffff, 0, 0,
                                   0x9dd5, 0xbfff,0x4088,0x8000
                                  };
    if (val!=expected[i])
      f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  for (int i=0; i<7; ++i) {
    f << "unkn" << i << "=[";
    for (int j=0; j<4; ++j) {
      val=(int) input->readULong(2);
      if (val)
        f << std::hex << val << std::dec << ",";
      else
        f << "_,";
    }
    f << "],";
  }
  for (int i=0; i<14; ++i) { // always 0
    val=(int) input->readLong(2);
    if (val)
      f << "g" << i << "=" << val << ",";
  }
  ascii().addPos(84);
  ascii().addNote(f.str().c_str());

  long pos=input->tell();
  f.str("");
  f << "DocHeader-1:";
  int N=(int) input->readULong(2);
  if (N>=20) {
    MWAW_DEBUG_MSG(("MacDraftParser::readDocHeader: N seems bad\n"));
    f << "###";
    N=0;
  }
  // FIXME: find a better way to determine v1.0 and/or v1.2 file
  if (N==4)
    m_state->m_version=1;
  else
    m_state->m_version=2;
  f << "N=" << N << ",unk=[";
  for (int i=0; i<=N; ++i) {
    val=(int) input->readULong(2);
    f << input->readULong(2) << ":" << std::hex << val << std::dec << ",";
  }
  f << "],";
  input->seek(pos+42, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<6; ++i) {
    val=(int) input->readULong(2);
    static int const(expected[])= {0x144, 0xa, 0x152, 0xc, 0, 0 };
    if (val!=expected[i])
      f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  f << "unkn1=[";
  for (int i=0; i<2; ++i) { // 0,0 or 99cc6633,99cc6633
    long lVal=(long) input->readULong(4);
    if (lVal)
      f << std::hex << lVal << std::dec << ",";
    else
      f << "_,";
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "DocHeader-2:";
  input->seek(pos+132,librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "DocHeader-3:";
  input->seek(pos+66,librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int st=0; st<2; ++st) {
    pos=input->tell();
    f.str("");
    f << "DocHeader-4" << (st==0 ? "a" : "b") << ":";
    input->seek(pos+40,librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  pos=input->tell();
  f.str("");
  f << "DocHeader-5:";
  for (int i=0; i<2; ++i) { // fl0=0|1, fl1=1
    val=(int) input->readLong(1);
    if (val==1)
      f << "fl" << i << ",";
    else if (val)
      f << "fl" << i << "=" << val << ",";
  }
  for (int i=0; i<2; ++i) { // f0=3, f1=32|100
    val=(int) input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  for (int i=0; i<4; ++i) { // fl2=1, fl4=0|1
    val=(int) input->readLong(1);
    if (val==1)
      f << "fl" << i+2 << ",";
    else if (val)
      f << "fl" << i+2 << "=" << val << ",";
  }
  f << "unkn=[";
  for (int i=0; i<6; ++i) { // small number
    val=(int) input->readLong(2);
    if (val)
      f << val << ",";
    else
      f << "_,";
  }
  f << "],";
  float dim[2];
  for (int i=0; i<2; ++i) dim[i]=float(input->readLong(2))/8.f;
  f << "dim?=" << MWAWVec2f(dim[1],dim[0]) << ",";
  for (int i=0; i<7; ++i) { // f2=0|256, f5=256
    val=(int) input->readLong(2);
    if (val)
      f << "f" << i+2 << "=" << val << ",";
  }
  int numPages[2];
  for (int i=0; i<2; ++i) numPages[i]=(int) input->readLong(2);
  if (numPages[0]!=1 || numPages[1]!=1)
    f << "numPages=" << MWAWVec2i(numPages[0], numPages[1]) << ",";
  if (numPages[0]<=0 || numPages[0]>50) {
    MWAW_DEBUG_MSG(("MacDraftParser::readDocHeader: num horizontal pages seems bad, reset it to 1\n"));
    numPages[0]=1;
    f << "###hori,";
  }
  if (numPages[1]<=0 || numPages[1]>100) {
    MWAW_DEBUG_MSG(("MacDraftParser::readDocHeader: num vertical pages seems bad, reset it to 1\n"));
    numPages[1]=1;
    f << "###verti,";
  }
  int pageDim[2];
  for (int i=0; i<2; ++i) pageDim[i]=(int) input->readLong(2);
  f << "page[dim]=" << MWAWVec2i(pageDim[1],pageDim[0]) << ",";
  if (pageDim[0]>=50 && pageDim[0]<=2000)
    getPageSpan().setFormLength(double(pageDim[0]*numPages[1])/72.);
  else {
    MWAW_DEBUG_MSG(("MacDraftParser::readDocHeader: page height seems bad, use default\n"));
  }
  if (pageDim[1]>=50 && pageDim[1]<=2000)
    getPageSpan().setFormWidth(double(pageDim[1]*numPages[0])/72.);
  else {
    MWAW_DEBUG_MSG(("MacDraftParser::readDocHeader: page width seems bad, use default\n"));
  }

  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(0x270,librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// try to read the print info zone
////////////////////////////////////////////////////////////
bool MacDraftParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  long fSz=(long) input->readULong(2);
  long endPos=pos+2+120;
  if (fSz!=120 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("MacDraftParser::readPrintInfo: file seems too short\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(PrintInfo):";
  libmwaw::PrinterInfo info;
  if (!info.read(input)) {
    MWAW_DEBUG_MSG(("MacDraftParser::readPrintInfo: can not read print info\n"));
    return false;
  }
  f << info;
  MWAWVec2i paperSize = info.paper().size();
  MWAWVec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) {
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return true;
  }

  // define margin from print info
  MWAWVec2i lTopMargin= -1 * info.paper().pos(0);
  MWAWVec2i rBotMargin=info.paper().size() - info.page().size();

  // move margin left | top
  int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
  int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
  lTopMargin -= MWAWVec2i(decalX, decalY);
  rBotMargin += MWAWVec2i(decalX, decalY);

  // decrease right | bottom
  int rightMarg = rBotMargin.x() -50;
  if (rightMarg < 0) rightMarg=0;
  int botMarg = rBotMargin.y() -50;
  if (botMarg < 0) botMarg=0;

  getPageSpan().setMarginTop(lTopMargin.y()/72.0);
  getPageSpan().setMarginBottom(botMarg/72.0);
  getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
  getPageSpan().setMarginRight(rightMarg/72.0);
  // do not set page height/width as this is done by readDocHeader

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

bool MacDraftParser::send(MacDraftParserInternal::Shape const &shape)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDraftParser::send: can not find the listener\n"));
    return false;
  }
  shape.m_isSent=true;
  MWAWBox2f box=shape.getBdBox();
  MWAWPosition pos(box[0], box.size(), librevenge::RVNG_POINT);
  pos.m_anchorTo = MWAWPosition::Page;

  MWAWGraphicStyle style=shape.m_style;
  if (shape.m_patternId>0) {
    MWAWGraphicStyle::Pattern pat;
    if (m_state->getPattern(shape.m_patternId, pat)) {
      MWAWColor col;
      if (pat.getUniqueColor(col))
        style.setSurfaceColor(col);
      else
        style.m_pattern=pat;
    }
    else {
      MWAW_DEBUG_MSG(("MacDraftParser::send: can not find pattern %d\n", shape.m_patternId));
    }
  }
  switch (shape.m_type) {
  case MacDraftParserInternal::Shape::Basic:
    listener->insertPicture(pos, shape.m_shape, style);
    break;
  case MacDraftParserInternal::Shape::Group: {
    size_t numShapes=m_state->m_shapeList.size();
    if (!numShapes) break;
    listener->openGroup(pos);
    for (size_t i=0; i<shape.m_childList.size(); ++i) {
      if (shape.m_childList[i]>=numShapes) {
        MWAW_DEBUG_MSG(("MacDraftParser::send: can not find a child\n"));
        continue;
      }
      MacDraftParserInternal::Shape const &child=m_state->m_shapeList[shape.m_childList[i]];
      if (child.m_isSent) {
        MWAW_DEBUG_MSG(("MacDraftParser::send: the child is already sent\n"));
        continue;
      }
      send(child);
    }
    listener->closeGroup();
    break;
  }
  case MacDraftParserInternal::Shape::Label: {
    /* either a 1d label or a 2d label, so check if we can print it,
       ie. if the previous object is a line
     */
    if (shape.m_id<=0 || shape.m_id>=(int) m_state->m_shapeList.size()) {
      MWAW_DEBUG_MSG(("MacDraftParser::send: problem finding the shape\n"));
      break;
    }
    MacDraftParserInternal::Shape const &obj=m_state->m_shapeList[size_t(shape.m_id-1)];
    if (obj.m_type!=MacDraftParserInternal::Shape::Basic || !obj.m_isLine) {
      MWAW_DEBUG_MSG(("MacDraftParser::send: can only retrieve line label\n"));
      break;
    }
    // update the label width and recenter the label box
    MWAWVec2f lineSz=obj.m_shape.getBdBox().size();
    MacDraftParserInternal::Shape &label=m_state->m_shapeList[size_t(shape.m_id)];
    label.m_labelWidth=std::sqrt(lineSz[0]*lineSz[0]+lineSz[1]*lineSz[1]);
    MWAWVec2f decal=obj.m_shape.getBdBox().center()-box.center();
    pos.setOrigin(box[0]+decal);
    shared_ptr<MWAWSubDocument> doc(new MacDraftParserInternal::SubDocument(*this, getInput(), shape.m_id));
    listener->insertTextBox(pos, doc, style);
    return true;
  }
  case MacDraftParserInternal::Shape::Text: {
    shared_ptr<MWAWSubDocument> doc(new MacDraftParserInternal::SubDocument(*this, getInput(), shape.m_id));
    listener->insertTextBox(pos, doc, style);
    return true;
  }
  case MacDraftParserInternal::Shape::Bitmap:
    sendBitmap(shape, pos);
    break;
  case MacDraftParserInternal::Shape::Unknown:
  default:
    return false;
  }
  return true;
}

bool MacDraftParser::sendBitmap(MacDraftParserInternal::Shape const &bitmap, MWAWPosition const &position)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDraftParser::sendBitmap: can not find the listener\n"));
    return false;
  }
  bitmap.m_isSent=true;

  if (bitmap.m_bitmapIdList.empty() || bitmap.m_bitmapDimensionList.size()!=bitmap.m_bitmapIdList.size()) {
    MWAW_DEBUG_MSG(("MacDraftParser::sendBitmap: the bitmap seems bad\n"));
    return false;
  }
  MWAWBox2i bitmapBox=bitmap.m_bitmapDimensionList[0];
  for (size_t i=1; i<bitmap.m_bitmapDimensionList.size(); ++i)
    bitmapBox=bitmapBox.getUnion(bitmap.m_bitmapDimensionList[i]);
  MWAWVec2i bitmapDim=bitmapBox.size();
  if (bitmapDim[0]<=0||bitmapDim[1]<=0||bitmapDim[0]*bitmapDim[1]>10000000) {
    MWAW_DEBUG_MSG(("MacDraftParser::sendBitmap: can not compute the bitmap bdbox\n"));
    return false;
  }

  MWAWInputStreamPtr input = getInput();
  MWAWPictBitmapColor pict(bitmapDim, true);
  MWAWColor transparent(255,255,255,0);
  MWAWColor black(MWAWColor::black());
  std::vector<MWAWColor> colors;
  colors.resize(size_t(bitmapDim[0]), transparent);
  for (int r=0; r<bitmapDim[1]; ++r)  pict.setRow(r, &colors[0]);

  for (size_t i=0; i<bitmap.m_bitmapIdList.size(); ++i) {
    if (m_state->m_idToBitmapMap.find(bitmap.m_bitmapIdList[i])==m_state->m_idToBitmapMap.end()) {
      MWAW_DEBUG_MSG(("MacDraftParser::sendBitmap: can not find bitmap data %lx\n", bitmap.m_bitmapIdList[i]));
      continue;
    }
    MWAWBox2i dataBox=bitmap.m_bitmapDimensionList[i];
    MacDraftParserInternal::BitmapFileData const &data=m_state->m_idToBitmapMap.find(bitmap.m_bitmapIdList[i])->second;
    if (!data.ok() || data.m_dimension[0]!=dataBox[0] || data.m_dimension.size()[0]<dataBox.size()[0] ||
        data.m_dimension.size()[1]<dataBox.size()[1]) {
      MWAW_DEBUG_MSG(("MacDraftParser::sendBitmap: bitmap data %lx seems bad\n", bitmap.m_bitmapIdList[i]));
      continue;
    }

    long pos=input->tell();
    input->seek(data.m_entry.begin(), librevenge::RVNG_SEEK_SET);
    for (int r=dataBox[0][1]; r<dataBox[1][1]; ++r) {
      long debRow=input->tell();
      if (r>=bitmapBox[1][1])
        break;
      int row=r-bitmapBox[0][1];
      int wPos=dataBox[0][0]-bitmapBox[0][0];
      for (int col=dataBox[0][0]; col<dataBox[1][0];) {
        unsigned char c=(unsigned char) input->readULong(1);
        for (int j=0, bit=0x80; j<8 ; ++j, bit>>=1) {
          if (wPos>=bitmapDim[0] || ++col>dataBox[1][0]) break;
          pict.set(wPos++, row, (c&bit) ? black : transparent);
        }
      }
      input->seek(debRow+data.m_rowSize, librevenge::RVNG_SEEK_SET);
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
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

bool MacDraftParser::sendText(int zId)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDraftParser::sendText: can not find the listener\n"));
    return false;
  }
  if (zId<0||zId>=(int) m_state->m_shapeList.size()) {
    MWAW_DEBUG_MSG(("MacDraftParser::sendText: can not find the text shape\n"));
    return false;
  }
  MacDraftParserInternal::Shape const &shape=m_state->m_shapeList[size_t(zId)];
  shape.m_isSent = true;

  listener->setParagraph(shape.m_paragraph);
  listener->setFont(shape.m_font);
  if (shape.m_type == MacDraftParserInternal::Shape::Label) {
    std::stringstream s;
    s << std::setprecision(0) << std::fixed << shape.m_labelWidth << " pt";
    listener->insertUnicodeString(s.str().c_str());
    return true;
  }
  if (shape.m_type != MacDraftParserInternal::Shape::Text) {
    MWAW_DEBUG_MSG(("MacDraftParser::sendText: can not find the text shape\n"));
    return false;
  }
  if (!shape.m_textEntry.valid())
    return true;
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
      MWAW_DEBUG_MSG(("MacDraftParser::sendText: find char 0\n"));
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
