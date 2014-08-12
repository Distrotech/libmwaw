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

#include "MacDrawProParser.hxx"

/** Internal: the structures of a MacDrawProParser */
namespace MacDrawProParserInternal
{
// generic class used to store shape in MWAWDrawParser
struct Shape {
  //! the different shape
  enum Type { Basic, Bitmap, Group, GroupEnd, Text, Unknown };

  //! constructor
  Shape() : m_type(Unknown), m_box(), m_style(), m_shape(), m_id(-1), m_nextId(-1), m_font(), m_paragraph(), m_textEntry(), m_childList(), m_numBytesByRow(0), m_bitmapDim(), m_bitmapFileDim(), m_bitmapEntry(), m_isSent(false)
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
//! Internal: the state of a MacDrawProParser
struct State {
  //! constructor
  State() : m_version(0), m_documentSize(), m_numLayers(1), m_numShapes(0), m_numColors(8), m_numBWPatterns(0), m_numColorPatterns(0), m_numPatternsInTool(0),
    m_penSizeList(), m_dashList(), m_fontList(), m_colorList(), m_BWPatternList(), m_colorPatternList(),
    m_shapeList()
  {
    for (int i=0; i<5; ++i) m_numZones[i]=0;
    for (int i=0; i<5; ++i) m_sizeHeaderZones[i]=0;
    for (int i=0; i<2; ++i) m_sizeLayerZones[i]=0;
    for (int i=0; i<2; ++i) m_sizeLibraryZones[i]=0;
    for (int i=0; i<4; ++i) m_sizeFZones[i]=0;
  }
  //! returns a pattern if posible
  bool getPattern(int id, MWAWGraphicStyle::Pattern &pat)
  {
    if (m_BWPatternList.empty()) initBWPatterns();
    if (id<=0 || id>int(m_BWPatternList.size())) {
      MWAW_DEBUG_MSG(("MacDrawProParserInternal::getPattern: can not find pattern %d\n", id));
      return false;
    }
    pat=m_BWPatternList[size_t(id-1)];
    return true;
  }
  //! returns a color if posible
  bool getColor(int id, MWAWColor &col)
  {
    if (m_colorList.empty()) initColors();
    if (id<=0 || id>int(m_colorList.size())) {
      MWAW_DEBUG_MSG(("MacDrawProParserInternal::getColor: can not find color %d\n", id));
      return false;
    }
    col=m_colorList[size_t(id-1)];
    return true;
  }
  //! init the black and white patterns list
  void initBWPatterns();
  //! init the colors list
  void initColors();
  //! init the dashs list
  void initDashs();
  //! init the pens list
  void initPens();
  //! the file version
  int m_version;
  //! the document size (in point)
  Vec2f m_documentSize;
  //! the number of layer
  int m_numLayers;
  //! the total number of shapes
  int m_numShapes;
  //! the number of zones
  int m_numZones[5];
  //! the number of color
  int m_numColors;
  //! the number of BW pattern
  int m_numBWPatterns;
  //! the number of color pattern
  int m_numColorPatterns;
  //! the number of pattern in tool list
  int m_numPatternsInTool;
  //! the size of the header zones
  long m_sizeHeaderZones[5];
  //! the size of the layer zones
  long m_sizeLayerZones[2];
  //! the size of the library zones(checkme)
  long m_sizeLibraryZones[2];
  //! the size of the zoneF
  long m_sizeFZones[4];
  //! the list of pen size
  std::vector<float> m_penSizeList;
  //! the list of dash
  std::vector< std::vector<float> > m_dashList;
  //! the list of font
  std::vector<MWAWFont> m_fontList;
  //! the color list
  std::vector<MWAWColor> m_colorList;
  //! the patterns list
  std::vector<MWAWGraphicStyle::Pattern> m_BWPatternList;
  //! the color patterns list
  std::vector<MWAWGraphicStyle::Pattern> m_colorPatternList;
  //! the shape list
  std::vector<Shape> m_shapeList;
};

void State::initColors()
{
  if (!m_colorList.empty()) return;
  m_colorList.push_back(MWAWColor::white());
  m_colorList.push_back(MWAWColor::black());
  m_colorList.push_back(MWAWColor(0xDD,0x8,0x6));
  m_colorList.push_back(MWAWColor(0,0x80,0x11));
  m_colorList.push_back(MWAWColor(0,0,0xd4));
  m_colorList.push_back(MWAWColor(0xFC,0xF3,0x5));
  m_colorList.push_back(MWAWColor(0x2,0xAB,0xEB));
  m_colorList.push_back(MWAWColor(0xF2,0x8,0x85));
}

void State::initDashs()
{
  if (!m_dashList.empty()) return;
  std::vector<float> dash;
  // 1: 9x9
  dash.push_back(9);
  dash.push_back(9);
  m_dashList.push_back(dash);
  // 2: 27x9
  dash[0]=27;
  m_dashList.push_back(dash);
  // 3: 18x18
  dash[0]=dash[1]=18;
  m_dashList.push_back(dash);
  // 4: 54x18
  dash[0]=54;
  m_dashList.push_back(dash);
  // 5: 72x9, 9x9
  dash.resize(4,9);
  dash[0]=72;
  dash[1]=9;
  m_dashList.push_back(dash);
  // 6: 72x9, 9x9, 9x9
  dash.resize(6,9);
}

void State::initPens()
{
  if (!m_penSizeList.empty()) return;
  static float const(values[])= {1,2,4,6,8,10};
  for (int i=0; i<6; ++i) m_penSizeList.push_back(values[i]);
}

void State::initBWPatterns()
{
  if (!m_BWPatternList.empty()) return;
  for (int i=0; i<59; ++i) {
    static uint16_t const(patterns[]) = {
      0xf0f0,0x3c3c,0x0f0f,0xc3c3, 0x1111,0x4444,0x1111,0x4444,
      0x7777,0xbbbb,0x7777,0xbbbb, 0x9999,0xcccc,0x9999,0xcccc,
      0x0000,0x0000,0x0000,0x0000, 0xffff,0xffff,0xffff,0xffff,
      0x7f7f,0xffff,0xf7f7,0xffff, 0x7f7f,0xf7f7,0x7f7f,0xf7f7,
      0x7777,0xdddd,0x7777,0xdddd, 0x7777,0x7777,0x7777,0x7777,
      0xaaaa,0xaaaa,0xaaaa,0xaaaa, 0x8888,0x8888,0x8888,0x8888,
      0x8888,0x2222,0x8888,0x2222, 0x8080,0x0808,0x8080,0x0808,
      0x8080,0x0000,0x0808,0x0000, 0x8080,0x4141,0x0808,0x1414,
      0xffff,0x8080,0xffff,0x0808, 0x8181,0x2424,0x8181,0x2424,
      0x8080,0x2020,0x0808,0x0202, 0xe0e0,0x3838,0x0e0e,0x8383,
      0x7777,0xdddd,0x7777,0xdddd, 0x8888,0x2222,0x8888,0x2222,
      0x9999,0x6666,0x9999,0x6666, 0x2020,0x8080,0x0808,0x0202,
      0xffff,0xffff,0xffff,0xffff, 0xffff,0x0000,0xffff,0x0000,
      0xcccc,0x0000,0x3333,0x0000, 0xf0f0,0xf0f0,0x0f0f,0x0f0f,
      0xffff,0x8888,0xffff,0x8888, 0xaaaa,0xaaaa,0xaaaa,0xaaaa,
      0x0101,0x0404,0x1010,0x4040, 0x8383,0x0e0e,0x3838,0xe0e0,
      0xeeee,0xbbbb,0xeeee,0xbbbb, 0x1111,0x4444,0x1111,0x4444,
      0x3333,0xcccc,0x3333,0xcccc, 0x4040,0x0000,0x0404,0x0000,
      0xaaaa,0xaaaa,0xaaaa,0xaaaa, 0x8888,0x8888,0x8888,0x8888,
      0x0101,0x1010,0x0101,0x1010, 0x0000,0x1414,0x5555,0x1414,
      0xffff,0x8080,0x8080,0x8080, 0x8282,0x2828,0x2828,0x8282,
      0x8080,0x0000,0x0000,0x0000, 0x8080,0x0202,0x0101,0x4040,
      0xaaaa,0xaaaa,0xaaaa,0xaaaa, 0x5555,0x5555,0x5555,0x5555,
      0xdddd,0x7777,0xdddd,0x7777, 0xaaaa,0x8080,0x8888,0x8080,
      0x0808,0x2222,0x8080,0x0202, 0xb1b1,0x0303,0xd8d8,0x0c0c,
      0x8888,0x2222,0x8888,0xaaaa, 0x8282,0x3939,0x8282,0x0101,
      0x0303,0x4848,0x0c0c,0x0101, 0x5555,0x4040,0x5555,0x0404,
      0x1010,0x5454,0xffff,0x0404, 0x2020,0x8888,0x8888,0x0505,
      0xbfbf,0xbfbf,0xb0b0,0xb0b0, 0xf8f8,0x2222,0x8f8f,0x2222,
      0x7777,0x8f8f,0x7777,0xf8f8,
    };
    MWAWGraphicStyle::Pattern pat;
    pat.m_dim=Vec2i(8,8);
    pat.m_data.resize(8);
    pat.m_colors[0]=MWAWColor::white();
    pat.m_colors[1]=MWAWColor::black();

    uint16_t const *patPtr=&patterns[4*i];
    for (size_t j=0; j<8; j+=2, ++patPtr) {
      pat.m_data[j]=uint8_t((*patPtr)>>8);
      pat.m_data[j+1]=uint8_t((*patPtr)&0xFF);
    }
    if (i==0) m_BWPatternList.push_back(pat); // none pattern
    m_BWPatternList.push_back(pat);
  }
}

////////////////////////////////////////
//! Internal: the subdocument of a MacDrawProParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MacDrawProParser &pars, MWAWInputStreamPtr input, int zoneId) : MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

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
    MWAW_DEBUG_MSG(("MacDrawProParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  MacDrawProParser *parser=dynamic_cast<MacDrawProParser *>(m_parser);
  if (!m_parser) {
    MWAW_DEBUG_MSG(("MacDrawProParserInternal::SubDocument::parse: no parser\n"));
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
MacDrawProParser::MacDrawProParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWGraphicParser(input, rsrcParser, header), m_state()
{
  init();
}

MacDrawProParser::~MacDrawProParser()
{
}

void MacDrawProParser::init()
{
  resetGraphicListener();
  setAsciiName("main-1");

  m_state.reset(new MacDrawProParserInternal::State);

  getPageSpan().setMargins(0.1);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MacDrawProParser::parse(librevenge::RVNGDrawingInterface *docInterface)
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
        MacDrawProParserInternal::Shape const &shape=m_state->m_shapeList[i];
        if (shape.m_isSent) continue;
        send(shape);
        if (shape.m_nextId>0 && shape.m_nextId>int(i))
          i=size_t(shape.m_nextId-1);
      }
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MacDrawProParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetGraphicListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MacDrawProParser::createDocument(librevenge::RVNGDrawingInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getGraphicListener()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::createDocument: listener already exist\n"));
    return;
  }

  // create the page list
  MWAWPageSpan ps(getPageSpan());
  ps.setPageSpan(1);
  /* FIXME: update the page size using m_state->m_documentSize or by
     finding the right/bottom shape */
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
bool MacDrawProParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  readHeaderInfo();

  if (getRSRCParser()) readRSRCZones();

  input->seek(0x1f4,librevenge::RVNG_SEEK_SET);
  if (!readStyles() || !readLayersInfo()) return false;
  for (int i=0; i<3; ++i) { // never seems zoneF1 and zoneF2, so there may have a different form
    if (!readUnknownMainZones(i)) return false;
    if (i==0 && !readLibrariesInfo()) return false;
  }
  long pos;
  libmwaw::DebugStream f;
  if (m_state->m_sizeFZones[3]) {
    pos=input->tell();
    long endPos=pos+m_state->m_sizeFZones[3];
    f.str("");
    f << "Entries(ZoneF5):";
    if (m_state->m_sizeFZones[3]<0 || !input->checkPosition(endPos)) {
      MWAW_DEBUG_MSG(("MacDrawProParser::createZones: can not read ZoneF5 size\n"));
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return false;
    }
    /** m_state->m_sizeFZones[3] is often a multiple of 8 ( and look like a list of struct of size 8)
        but I also find m_state->m_sizeFZones[3]=0x16 ( an uncomplete struct ? )
     */
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
  }
  pos=input->tell();
  if (m_state->m_numShapes<0 || !input->checkPosition(pos+32*m_state->m_numShapes)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::createZones: can not read the object information\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Object):###");
    return false;
  }
  for (int i=0; i<m_state->m_numShapes; ++i) {
    if (!readObject()) break;
  }

  // maybe junk
  pos=input->tell();
  ascii().addPos(pos);
  ascii().addNote("Entries(ZoneG):");
  pos+=512;
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  pos=input->tell();
  pos=256*(pos/256)+256;
  while (input->checkPosition(pos)) {
    ascii().addPos(pos);
    ascii().addNote("ZoneG-##");
    pos+=256;
  }
  MWAW_DEBUG_MSG(("MacDrawProParser::createZones: oops is not implemented\n"));
  return false;
}

bool MacDrawProParser::readStyles()
{
  MWAWInputStreamPtr input = getInput();
  long pos;
  for (int i=0; i<5; ++i) {
    if (!m_state->m_sizeHeaderZones[i])
      continue;
    pos=input->tell();

    bool done=false;
    MWAWEntry entry;
    entry.setBegin(pos);
    entry.setLength(m_state->m_sizeHeaderZones[i]);
    switch (i) {
    case 0:
      done=readRulers(entry);
      break;
    case 1:
      done=readPens(entry);
      break;
    case 2:
      done=readDashs(entry);
      break;
    case 3:
      done=readArrows(entry);
      break;
    case 4:
      done=readFontStyles(entry);
      break;
    default:
      break;
    }
    if (done) continue;
    MWAW_DEBUG_MSG(("MacDrawProParser::readStyles: can not read zone %d\n", i));
    ascii().addPos(entry.end());
    ascii().addNote("Entries(Styles):###");
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  }

  return true;
}

bool MacDrawProParser::readLayersInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  long begNamePos=pos+m_state->m_sizeLayerZones[0];
  long endPos=pos+m_state->m_sizeLayerZones[0]+m_state->m_sizeLayerZones[1];
  if (input->isEnd() || m_state->m_numLayers*0x80>m_state->m_sizeLayerZones[0] || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readLayersInfo: problem with the layer dimensions\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Layer):###");
    return false;
  }

  libmwaw::DebugStream f;
  int numShapes=0;
  for (int i=0; i<m_state->m_numLayers; ++i) {
    pos=input->tell();
    f.str("");
    f << "Entries(Layer)[" << i << "]:";
    long val=(long) input->readULong(2); // always 0?
    if (val) f << "f0=" << val << ",";
    f << "id=" << input->readULong(2) << ",";
    val=(long) input->readULong(4); // 0 excepted if we have many layers
    if (val) f << "f1=" << val << ",";
    val=(long) input->readULong(4);
    numShapes+=(int) val;
    if (val) f << "N[shapes]=" << val << ",";

    ascii().addDelimiter(input->tell(),'|');
    input->seek(pos+128, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  m_state->m_numShapes=numShapes;
  input->seek(begNamePos, librevenge::RVNG_SEEK_SET);

  pos=input->tell();
  f.str("");
  f << "Layer[name]:";
  for (int i=0; i<m_state->m_numLayers; ++i) {
    int fSz=(int)input->readULong(1);
    if (input->tell()+fSz>endPos) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readLayersInfo: oops the layer name size seems bad\n"));
      f << "###";
      break;
    }
    std::string name("");
    for (int c=0; c<fSz; ++c) name+=(char) input->readULong(1);
    f << name << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}


bool MacDrawProParser::readLibrariesInfo()
{
  if (!m_state->m_sizeLibraryZones[0]||!m_state->m_sizeLibraryZones[1])
    return false;
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  long begNamePos=pos+m_state->m_sizeLibraryZones[0];
  long endPos=pos+m_state->m_sizeLibraryZones[0]+m_state->m_sizeLibraryZones[1];
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readLibrariesInfo: problem with the library dimensions\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Library):###");
    return false;
  }
  if ((m_state->m_sizeLibraryZones[0]%8) || !m_state->m_sizeLibraryZones[0]||!m_state->m_sizeLibraryZones[1]) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readLibrariesInfo: problem with the size zone(II)\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Library):###");
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return true;
  }

  libmwaw::DebugStream f;
  f << "Entries(Library):";
  std::vector<long> posList;
  int N=int(m_state->m_sizeLibraryZones[0]/8);
  for (int i=0; i<N; ++i) {
    long cPos=(long) input->readULong(4);
    if (cPos<0 || cPos>m_state->m_sizeLibraryZones[1]) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readLayersInfo: oops the library name positions seems bad\n"));
      f << "###";
      posList.push_back(-1);
    }
    else
      posList.push_back(cPos);
    f << std::hex << cPos << std::dec;
    int val=(int) input->readLong(4);
    if (val) f << ":" << val << ",";
    else f << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (size_t i=0; i<posList.size(); ++i) {
    if (posList[i]<0) continue;

    pos=begNamePos+posList[i];
    f.str("");
    f << "Library-name[" << i << "]:";
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    int fSz=(int)input->readULong(1);
    if (input->tell()+fSz>endPos) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readLayersInfo: oops the library name size seems bad\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      continue;
    }
    std::string name("");
    for (int c=0; c<fSz; ++c) name+=(char) input->readULong(1);
    f << name << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}


bool MacDrawProParser::readFontStyles(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readFontStyles: the entry is bad\n"));
    return false;
  }

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  entry.setParsed(true);
  long pos=entry.begin();
  if ((entry.length()%18)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readFontStyles: the data size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  int N=int(entry.length()/18);
  if (N!=m_state->m_numZones[2]) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readFontStyles: oops the number of fonts seems odd\n"));
  }

  int val;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int j=0; j<N; ++j) {
    pos=input->tell();
    f.str("");
    val=(int) input->readLong(2);
    if (val!=1) f<<"numUsed=" << val << ",";
    f << "h[total]=" << input->readLong(2) << ",";
    int sz=(int) input->readULong(2);
    MWAWFont font;
    font.setId((int) input->readULong(2));
    uint32_t flags = 0;
    int flag=(int) input->readULong(1);
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0x8) flags |= MWAWFont::embossBit;
    if (flag&0x10) flags |= MWAWFont::shadowBit;
    if (flag&0xE0) f << "#fl0=" << std::hex << (flag&0xE0) << std::dec << ",";
    font.setFlags(flags);
    val=(int) input->readULong(1);
    if (val)
      f << "fl2=" << val << ",";
    val=(int) input->readULong(2);
    font.setSize((float) val);
    if (sz!=val)
      f << "sz2=" << sz << ",";
    unsigned char col[3];
    for (int k=0; k < 3; k++)
      col[k] = (unsigned char)(input->readULong(2)>>8);
    if (col[0] || col[1] || col[2])
      font.setColor(MWAWColor(col[0],col[1],col[2]));
    font.m_extra=f.str();
    m_state->m_fontList.push_back(font);

    f.str("");
    f << "Entries(FontStyle):F" << j+1 << ":";
    f << font.getDebugString(getParserState()->m_fontConverter) << font.m_extra;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProParser::readUnknownMainZones(int zoneId)
{
  if (!m_state->m_sizeFZones[zoneId]) return true;
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  long endPos=pos+m_state->m_sizeFZones[zoneId];

  libmwaw::DebugStream f;
  if (zoneId==0)
    f << "Entries(LayToLib):";
  else
    f << "Entries(ZoneF" << zoneId << "):";
  if (m_state->m_sizeFZones[zoneId]<0 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readUnknownMainZones: can not read ZoneF%d size\n", zoneId));
    f.str("");
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  long sz=(long) input->readULong(4);
  long fFree=(int) input->readULong(4);
  f << "fFree=" << fFree << ",";
  if (sz<4 ||  pos+sz>endPos) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readUnknownMainZones: can not read ZoneF%d-ptr size\n", zoneId));
    f << "ptr###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return true;
  }
  if (fFree!=sz)
    ascii().addDelimiter(pos+fFree,'|');
  int numDatas=int((sz-8)/4);
  if (numDatas>m_state->m_numShapes)
    numDatas=m_state->m_numShapes;
  std::map<int, long> idToDecal;
  f << "ptrs=[";
  for (int i=0; i<numDatas; ++i) {
    long ptr=(long) input->readULong(4);
    if (!ptr) continue;
    idToDecal[i]=ptr;
    f << std::hex << ptr << std::dec << ":" << i << ",";
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+sz,  librevenge::RVNG_SEEK_SET);

  pos=input->tell();
  f.str("");
  if (zoneId==0)
    f << "LayToLib[data]:";
  else
    f << "ZoneF" << zoneId << "[data]:";
  sz=(long) input->readULong(4);
  if (sz<4 || pos+sz>endPos) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readUnknownMainZones: can not read ZoneF%d-data size\n", zoneId));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return true;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (std::map<int, long>::const_iterator it=idToDecal.begin(); it!=idToDecal.end(); ++it) {
    int id=it->first;
    long decal=it->second;
    if (decal<0 || decal+8>sz) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readUnknownMainZones: can not read ZoneF%d-D%d position\n", zoneId, id));
      continue;
    }
    f.str("");
    if (zoneId==0)
      f << "LayToLib-D" << id << ":";
    else
      f << "ZoneF" << zoneId << "-D" << id << ":";
    long begPos=pos+decal;
    input->seek(begPos, librevenge::RVNG_SEEK_SET);
    int val=(int) input->readLong(2); // always 0?
    if (val) f << "f0=" << val << ",";
    val=(int) input->readLong(2); // 1|3
    if (val!=1) f << "type=" << val << ",";
    long dataSz=(long) input->readULong(4);
    if (dataSz<8 || begPos+dataSz>endPos) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readUnknownMainZones: can not read ZoneF%d-D%d data\n", zoneId, id));
      f << "###";
      ascii().addPos(begPos);
      ascii().addNote(f.str().c_str());
      continue;
    }
    if (dataSz!=8)
      ascii().addDelimiter(input->tell(),'|');
    ascii().addPos(begPos+dataSz);
    ascii().addNote("_");
    ascii().addPos(begPos);
    ascii().addNote(f.str().c_str());
  }
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

bool MacDrawProParser::readObject()
{
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd()) return false;
  long pos=input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Object):";

  if (!input->checkPosition(pos+32)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readObject: the zone seems to small\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  size_t shapeId= m_state->m_shapeList.size();
  m_state->m_shapeList.push_back(MacDrawProParserInternal::Shape());
  MacDrawProParserInternal::Shape &shape=m_state->m_shapeList.back();
  shape.m_id=int(shapeId);
  f << "id=" << shapeId << ",";
  float dim[4];
  for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
  shape.m_box=Box2f(Vec2f(dim[1],dim[0]),Vec2f(dim[3],dim[2]));
  f << shape.m_box << ",";
  int val=(int) input->readULong(1);
  if (val & 0x8) f << "select,";
  if (val & 0x20) f << "locked,";
  if (val & 0x80) f << "rotation,";
  val &=0x57;
  if (val) f << "fl0=" << std::hex << val << std::dec << ",";
  val=(int) input->readULong(1);
  // checkme
  if (val&0x20) f << "flipX,";
  if (val&0x40) f << "flipY,";
  if (val&0x90) f << "fl1=" << std::hex << ((val>>4)&9) << std::dec << ",";
  val &= 0xF;
  switch (val) {
  case 1:
    f << "text,";
    break;
  case 2:
    f << "line,";
    break;
  case 3:
    f << "rect,";
    break;
  case 4:
    f << "rectOval,";
    break;
  case 5:
    f << "circle,";
    break;
  case 6:
    f << "arc,";
    break;
  case 7:
    f << "spline,";
    break;
  case 8:
    f << "poly,";
    break;
  case 9:
    f << "bitmap,";
    break;
  case 0xa:
    f << "group,";
    break;
  case 0xb:
    f << "endGroup,";
    break;
  default:
    MWAW_DEBUG_MSG(("MacDrawProParser::readObject: find unknown type %d\n", val));
    f << "###type=" << std::hex << val << std::dec << ",";
  }
  val=(int) input->readLong(1); // always 1
  if (val!=1) f << "f0=" << val << ",";
  val=(int) input->readULong(1);
  if (val) {
    m_state->initPens();
    if (val<=0 || val>int(m_state->m_penSizeList.size())) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readObject: find unknown pen: %d\n", val));
      f << "###";
    }
    f << "P" << val << ",";
  }
  val=(int) input->readLong(2); // often 6
  if (val!=6) f << "f1=" << val << ",";
  val=(int) input->readULong(2); // often 6
  if (val&0x8000) {
    val &= 0x7FFF;
    if (val<=0 || val>int(m_state->m_colorPatternList.size())) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readObject: find unknown color pattern: %d\n", val));
      f << "###";
    }
    f << "surf[pat]=C" << val << ",";
  }
  else if (val)  {
    m_state->initBWPatterns();
    if (val<=0 || val>int(m_state->m_BWPatternList.size())) {
      static bool first=true;
      if (first) {
        MWAW_DEBUG_MSG(("MacDrawProParser::readObject: find unknown BW pattern: %d\n", val));
        first=false;
      }
      f << "###";
    }
    f << "surf[pat]=" << val << ",";
  }
  val=(int) input->readULong(1);
  if (val) {
    m_state->initDashs();
    if (val<=0 || val>int(m_state->m_dashList.size())) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readObject: find unknown dash: %d\n", val));
      f << "###";
    }
    f << "D" << val << ",";
  }

  ascii().addDelimiter(input->tell(),'|');
  input->seek(pos+32, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MacDrawProParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MacDrawProParserInternal::State();
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
  }
  else if (val==0x5354) { // template
    if (input->readULong(2)!=0x4154) return false;
  }
  else return false;

  val=(int) input->readULong(2);
  if (val==0x4432) vers=0;
  else {
    MWAW_DEBUG_MSG(("MacDrawProParser::checkHeader: find unexpected header\n"));
    return false;
  }
  f << "version=" << vers << ",";
  f << "subVersion=" << input->readLong(2) << ",";
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  if (strict && !readPrintInfo()) {
    input->seek(8, librevenge::RVNG_SEEK_SET);
    for (int i=0; i<10; ++i) // allow print info to be zero
      if (input->readLong(2)) return false;
  }

  if (strict) {
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

  setVersion(vers);
  m_state->m_version=vers;
  if (header)
    header->reset(MWAWDocument::MWAW_T_MACDRAWPRO, vers, MWAWDocument::MWAW_K_DRAW);
  input->seek(512,librevenge::RVNG_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
// try to read the prefs zone
////////////////////////////////////////////////////////////
bool MacDrawProParser::readHeaderInfo()
{
  MWAWInputStreamPtr input = getInput();
  if (!input->checkPosition(512)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readHeaderInfo: the prefs zone seems too short\n"));
    ascii().addPos(14);
    ascii().addNote("Entries(HeaderInfo):#");
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
  f << "Entries(HeaderInfo):";
  for (int i=0; i<9; ++i) { // f0=1|2|7, f1=0|75|78|7c, f2=0|48, f4=0|48, f5=0|48, f6=0|48, f7=0|48
    int val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(0x100, librevenge::RVNG_SEEK_SET);
  pos=input->tell();
  f.str("");
  f << "HeaderInfo-A:";
  int val=(int) input->readULong(2); // 0 or 8
  if (val) f << "f0=" << val << ",";
  m_state->m_numLayers=(int) input->readULong(2); // 1|2|9
  if (m_state->m_numLayers!=1) f << "num[layers]=" << m_state->m_numLayers << ",";
  f << "NZones=[";
  for (int i=0; i<5; ++i) {
    m_state->m_numZones[i]=(int)input->readULong(2);
    f << m_state->m_numZones[i] << ",";
  }
  f << "],";
  for (int i=0; i<7; ++i) {
    val=(int) input->readULong(2);
    if (!val) continue;
    switch (i) {
    case 0:
      m_state->m_numPatternsInTool=val;
      f << "num[PatternsTool]=" << val << ",";
      break;
    case 1:
      m_state->m_numBWPatterns=val;
      f << "num[BWPatterns]=" << val << ",";
      break;
    case 2:
      m_state->m_numColors=val;
      if (val!=8) f << "numColors=" << val << ",";
      break;
    case 3:
      m_state->m_numColorPatterns=val;
      f << "num[colPatterns]=" << val << ",";
      break;
    default:
      f << "g" << i << "=" << val << ",";
      break;
    }
  }
  for (int i=0; i<2; ++i) { // h0=1|2|9 ( maybe related to layer), h1=1|2|40
    val=(int) input->readULong(2);
    if (val!=1) f << "h" << i << "=" << val << ",";
  }
  val=(int) input->readULong(2); // 0|45
  if (val) f << "h2=" << val << ",";
  val=(int) input->readULong(2); // actual shape ?
  if (val!=1) f << "shape[actual]?=" << val << ",";
  for (int i=0; i<2; ++i) { // h3=0, h4=2|3|7|a
    val=(int) input->readULong(2);
    if (val) f << "h" << i+3 << "=" << val << ",";
  }

  input->seek(pos+40, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i=0; i<3; ++i) {
    pos=input->tell();
    f.str("");
    f << "HeaderInfo-B" << i << ":";
    input->seek(pos+40, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  pos=input->tell();
  f.str("");
  f << "HeaderInfo-C:";
  for (int i=0; i<5; ++i) {
    m_state->m_sizeHeaderZones[i]=(long) input->readULong(4);
    if (!m_state->m_sizeHeaderZones[i]) continue;
    f << "sz[HeaderZ" << i << "]=" << std::hex << m_state->m_sizeHeaderZones[i] << std::dec << ",";
  }
  for (int i=0;  i<5; ++i) {
    long lVal=(long) input->readULong(4);
    if (!lVal) continue;
    MWAW_DEBUG_MSG(("MacDrawProParser::readHeaderInfo: find some unexpected value in C zone, we may have a problem\n"));
    f << "##f" << i << "=" << std::hex << lVal << std::dec << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "HeaderInfo-C(II):";
  for (int i=0; i<2; ++i)
    m_state->m_sizeLayerZones[i]=(long) input->readULong(4);
  f << "sz[layers]=" << std::hex << m_state->m_sizeLayerZones[0] << "+" << m_state->m_sizeLayerZones[1] << std::dec << ",";
  m_state->m_sizeFZones[0]=(long) input->readULong(4);
  if (m_state->m_sizeFZones[0])
    f << "sz[ZoneF0]=" << std::hex << m_state->m_sizeFZones[0] << std::dec << ",";
  for (int i=0; i<2; ++i)
    m_state->m_sizeLibraryZones[i]=(long) input->readULong(4);
  if (m_state->m_sizeLibraryZones[0] || m_state->m_sizeLibraryZones[1])
    f << "sz[libraries]=" << std::hex << m_state->m_sizeLibraryZones[0] << "+" << m_state->m_sizeLibraryZones[1] << std::dec << ",";
  for (int i=1; i<4; ++i) {
    m_state->m_sizeFZones[i]=(long) input->readULong(4);
    if (!m_state->m_sizeFZones[i]) continue;
    f << "sz[ZoneF" << i << "]=" << std::hex << m_state->m_sizeFZones[i] << std::dec << ",";
  }
  for (int i=0; i<3; ++i) { // always 0?
    long lVal=(long) input->readULong(4);
    if (!lVal) continue;
    MWAW_DEBUG_MSG(("MacDrawProParser::readHeaderInfo: find some unexpected value in C(II) zone, we may have a problem\n"));
    f << "##f" << i << "=" << lVal << ",";
  }

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(0x1f4, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// try to read the print info zone
////////////////////////////////////////////////////////////
bool MacDrawProParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  long endPos=pos+120;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readPrintInfo: file seems too short\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(PrintInfo):";
  libmwaw::PrinterInfo info;
  if (!info.read(input)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readPrintInfo: can not read print info\n"));
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
// styles/settings present in data/rsrc fork
////////////////////////////////////////////////////////////

bool MacDrawProParser::readArrows(MWAWEntry const &entry, bool inRsrc)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readArrows: the entry is bad\n"));
    return false;
  }

  if (inRsrc && entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readArrows: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = inRsrc ? getRSRCParser()->getInput() : getInput();
  libmwaw::DebugFile &ascFile = inRsrc ? getRSRCParser()->ascii() : ascii();
  libmwaw::DebugStream f;
  f << "Entries(Arrow)";
  if (inRsrc)
    f << "[" << entry.id() << "]:";
  else
    f << ":";
  entry.setParsed(true);
  long pos=entry.begin();
  int const expectedSz=inRsrc ? 10 : 14;
  if ((entry.length()%expectedSz)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readArrows: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(inRsrc ? pos-4 : pos);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  if (inRsrc) {
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
  }
  int N=int(entry.length()/expectedSz);
  if (!inRsrc && N!=m_state->m_numZones[3]) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readArrows: oops the number of arrows seems odd\n"));
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "Entries(Arrow):A" << i+1 << ",";
    if (!inRsrc) {
      int val=(int) input->readLong(2); // always 0
      if (val) f<<"f0=" << val << ",";
      val=(int) input->readLong(2);
      if (val!=1) f<<"numUsed=" << val << ",";
      val=(int) input->readLong(2);
      if (val!=i+1) f << "id=" << val << ",";
    }
    float pt[2]; // position of the symetric point in the arrow head
    for (int k=0; k<2; ++k) pt[k]=float(input->readULong(4))/65536.f;
    f << "pt=" << pt[1] << "x" << pt[0] << ",";
    if (inRsrc) {
      int val=(int) input->readLong(2);
      if (val!=i+1) f << "id=" << val << ",";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProParser::readDashs(MWAWEntry const &entry, bool inRsrc)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readDashs: the entry is bad\n"));
    return false;
  }

  if (inRsrc && entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readDashs: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = inRsrc ? getRSRCParser()->getInput() : getInput();
  libmwaw::DebugFile &ascFile = inRsrc ? getRSRCParser()->ascii() : ascii();
  libmwaw::DebugStream f;
  f << "Entries(Dash)";
  if (inRsrc)
    f << "[" << entry.id() << "]:";
  else
    f << ":";
  entry.setParsed(true);
  long pos=entry.begin();
  int const expectedSz=inRsrc ? 26 : 28;
  if ((entry.length()%expectedSz)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readDashs: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(inRsrc ? pos-4 : pos);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  m_state->m_dashList.clear();
  if (inRsrc) {
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
  }
  int N=int(entry.length()/expectedSz);
  if (!inRsrc && N!=m_state->m_numZones[4]) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readDashs: oops the number of dashs seems odd\n"));
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "Entries(Dash):D" << i+1 << ",";
    if (!inRsrc) {
      int val=(int) input->readLong(2); // always 0
      if (val) f<<"f0=" << val << ",";
      val=(int) input->readLong(2);
      if (val!=1) f<<"numUsed=" << val << ",";
    }
    f << "dash=[";
    std::vector<float> dash;
    for (int k=0; k<3; ++k) {
      long solid=(long) input->readULong(4);
      long empty=(long) input->readULong(4);
      if (!solid && !empty) continue;
      dash.push_back(float(solid)/65536.f);
      dash.push_back(float(empty)/65536.f);
      f << double(solid)/65536. << "x" << double(empty)/65536. << ",";
    }
    m_state->m_dashList.push_back(dash);
    f << "],";
    if (inRsrc) {
      int val=(int) input->readLong(2);
      if (val!=i+1) f << "id=" << val << ",";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProParser::readPens(MWAWEntry const &entry, bool inRsrc)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readPens: the entry is bad\n"));
    return false;
  }

  if (inRsrc && entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readPens: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = inRsrc ? getRSRCParser()->getInput() : getInput();
  libmwaw::DebugFile &ascFile = inRsrc ? getRSRCParser()->ascii() : ascii();
  libmwaw::DebugStream f;
  f << "Entries(Pen)";
  if (inRsrc)
    f << "[" << entry.id() << "]:";
  else
    f << ":";
  entry.setParsed(true);
  long pos=entry.begin();
  int const expectedSz=inRsrc ? 8 : 12;
  if ((entry.length()%expectedSz)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readPens: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(inRsrc ? pos-4 : pos);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  m_state->m_penSizeList.clear();
  if (inRsrc) {
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
  }
  int N=int(entry.length()/expectedSz);
  if (!inRsrc && N!=m_state->m_numZones[1]) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readPens: oops the number of pens seems odd\n"));
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "Entries(Pen):P" << i+1 << ",";
    if (!inRsrc) {
      int val=(int) input->readLong(2); // always 0
      if (val) f<<"f0=" << val << ",";
      val=(int) input->readLong(2);
      if (val!=1) f<<"numUsed=" << val << ",";
    }
    long penSize=(long) input->readULong(4);
    if (penSize!=0x10000) f << "pen[size]=" << float(penSize)/65536.f << ",";
    if (!inRsrc) {
      int dim[2];
      for (int j=0; j<2; ++j) dim[j]=(int) input->readULong(2);
      f << "pen=" << Vec2i(dim[1],dim[0]) << ",";
      m_state->m_penSizeList.push_back(float(dim[0]+dim[1])/2.f);
    }
    else {
      m_state->m_penSizeList.push_back(float(penSize)/65536.f);
      int val=(int) input->readLong(2); // unit ?
      if (val) f << "fl=" << std::hex << val << std::dec << ",";
      val=(int) input->readLong(2);
      if (val!=i+1) f << "id=" << val << ",";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProParser::readRulers(MWAWEntry const &entry, bool inRsrc)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readRulers: the entry is bad\n"));
    return false;
  }

  if (inRsrc && entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readRulers: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = inRsrc ? getRSRCParser()->getInput() : getInput();
  libmwaw::DebugFile &ascFile = inRsrc ? getRSRCParser()->ascii() : ascii();
  libmwaw::DebugStream f;
  f << "Entries(RulerStyle)";
  if (inRsrc)
    f << "[" << entry.id() << "]:";
  else
    f << ":";
  entry.setParsed(true);

  long pos=entry.begin();
  if ((entry.length()%24)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readRulers: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(inRsrc ? pos-4 : pos);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  if (inRsrc) {
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
  }
  int N=int(entry.length()/24);
  if (!inRsrc && N!=m_state->m_numZones[0]) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readRulers: oops the number of rulers seems odd\n"));
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "Entries(RulerStyle):R" << i+1 << ":";
    int numUsed=(int) input->readULong(4);
    if (numUsed!=1) f << "numUsed=" << numUsed << ",";
    for (int j=0; j<2; ++j) {
      double value;
      bool isNAN;
      if (!input->readDouble8(value, isNAN))
        f << "###,";
      else
        f << value << ",";
    }
    f << "num[subd]=" << input->readULong(2) << ",";
    f << "fl=" << std::hex << input->readULong(2) << std::dec << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// RSRC zones
////////////////////////////////////////////////////////////
bool MacDrawProParser::readRSRCZones()
{
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  if (!rsrcParser) return false;

  readFontNames();

  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;
  // the 256 zones
  char const *(zNames[]) = {
    "Dinf", "Pref",
    "Ctbl", "bawP", "colP", "patR",
    "Aset", "Dset", "DRul", "Pset", "Rset", "Rst2", "Dvws",
    "Dstl"
  };
  /* find also
     pPrf: with 0001000001000002000000000000
     sPrf: spelling preference ? ie find in one file and contains strings "Main/User dictionary"...
     xPrf: with 0000000100000001
   */
  for (int z = 0; z < 14; z++) {
    it = entryMap.lower_bound(zNames[z]);
    while (it != entryMap.end()) {
      if (it->first != zNames[z])
        break;
      MWAWEntry const &entry = it++->second;
      switch (z) {
      case 0: // document info
        readDocumentInfo(entry);
        break;
      case 1: // preferences
        readPreferences(entry);
        break;
      case 2: // color table
        readColors(entry);
        break;
      case 3: // BW patterns
        readBWPatterns(entry);
        break;
      case 4: // color patterns
        readColorPatterns(entry);
        break;
      case 5: // list of patterns in tools
        readPatternsToolList(entry);
        break;
      case 6: // arrow definitions
        readArrows(entry, true);
        break;
      case 7: // dashes
        readDashs(entry, true);
        break;
      case 8: // ruler
        readRulers(entry, true);
        break;
      case 9: // pen size
        readPens(entry, true);
        break;
      case 10: // ruler settings
      case 11:
        readRulerSettings(entry);
        break;
      case 12: // views positions
        readViews(entry);
        break;
      case 13: // unknown
        readRSRCDstl(entry);
        break;
      default:
        MWAW_DEBUG_MSG(("MacDrawProParser::readRSRCZones: called with unexpected settings %d\n", z));
        break;
      }
    }
  }
  // Ctbl: list of 16*data: numUsed, id, color, pos?
  return true;
}

bool MacDrawProParser::readPreferences(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readPreferences: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readPreferences: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = getRSRCParser()->getInput();
  libmwaw::DebugFile &ascFile = getRSRCParser()->ascii();
  libmwaw::DebugStream f;
  f << "Entries(Preferences)[" << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  if (entry.length()!=0x1a) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readPreferences: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int val=(int) input->readLong(2); // always 0?
  if (val) f << "f0=" << val << ",";
  f << "fl=[";
  for (int i=0; i<4; ++i) { // fl0=20|30, fl2=1|2, fl3=2|3
    val=(int) input->readULong(1);
    f << std::hex << val << std::dec << ",";
  }
  f << "],";
  for (int i=0; i<5; ++i) { // f1=12|1b, f2=f4=f5=0, f3=1e
    val=(int) input->readLong(2);
    if (val)
      f << "f" << i+1 << "=" << val << ",";
  }
  val=(int) input->readULong(2);
  if (val!=0xC000) f << "g0=" << std::hex << val << std::dec << ",";
  for (int i=0; i<4; ++i) { // g1=0, g2=0|8, g3=0|4, g5=0|3
    val=(int) input->readLong(2);
    if (val)
      f << "g" << i+1 << "=" << val << ",";
  }

  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool MacDrawProParser::readDocumentInfo(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readDocumentInfo: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readDocumentInfo: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = getRSRCParser()->getInput();
  libmwaw::DebugFile &ascFile = getRSRCParser()->ascii();
  libmwaw::DebugStream f;
  f << "Entries(DocumentInfo)[" << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  if (entry.length()!=0x58) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readDocumentInfo: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int val;
  f << "fl=[";
  for (int i=0; i<4; ++i) { // fl0=24|64 (version?), fl1=d|f|1d, f2=2, f3=8|9|10
    val=(int) input->readULong(1);
    f << std::hex << val << std::dec << ",";
  }
  f << "],";
  f << "windows[dim]?=[";
  for (int i=0; i<4; ++i) { // 29|2a,3,d2|1b0, 110|250
    val=(int) input->readLong(2);
    f << val << ",";
  }
  f << "],";
  f << "num?=[";
  for (int i=0; i<14; ++i) { // series of small number
    val=(int) input->readLong(2);
    if (val) f << val << ",";
    else f << "_,";
  }
  f << "],";
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "DocumentInfo-II:";
  for (int i=0; i<5; ++i) { // f3=2, f4=0|20, other 0
    val=(int) input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  int dim[2];
  for (int i=0; i<2; ++i) dim[i]=(int) input->readULong(2);
  if (dim[0]||dim[1]) f << "pos0?=" << dim[0] << "x" << dim[1] << ",";
  float fDim[2];
  for (int i=0; i<2; ++i) fDim[i]=float(input->readLong(4))/65536.f;
  m_state->m_documentSize=Vec2f(fDim[0],fDim[1]);
  f << "document[size]=" << m_state->m_documentSize << ",";
  val=(int) input->readLong(2); // 0|1
  if (val) f << "f5=" << val << ",";
  for (int i=0; i<2; ++i) fDim[i]=float(input->readLong(4))/65536.f;
  f << "pos1?=" << Vec2f(fDim[0],fDim[1]) << ",";
  for (int i=0; i<4; ++i) { // always 0?
    val=(int) input->readLong(2);
    if (val)
      f << "g" << i << "=" << val << ",";
  }
  for (int i=0; i<2; ++i) fDim[i]=float(input->readLong(4))/65536.f;
  f << "page[size]=" << Vec2f(fDim[1],fDim[0]) << ",";

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool MacDrawProParser::readFontNames()
{
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  if (!rsrcParser) return false;
  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  MWAWEntry idPosEntry, nameEntry;
  if (entryMap.find("Fnms")!=entryMap.end())
    nameEntry=entryMap.find("Fnms")->second;
  if (entryMap.find("Fmtx")!=entryMap.end())
    idPosEntry=entryMap.find("Fmtx")->second;
  if (!nameEntry.valid() && !idPosEntry.valid()) return true;
  if (!nameEntry.valid() || !idPosEntry.valid() || (idPosEntry.length()%8)!=0 ||
      nameEntry.id()!=256 || idPosEntry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readFontNames: the entries seems incoherent\n"));
    return false;
  }

  entryMap.find("Fnms")->second.setParsed(true);
  entryMap.find("Fmtx")->second.setParsed(true);

  MWAWInputStreamPtr input = getRSRCParser()->getInput();
  libmwaw::DebugFile &ascFile = getRSRCParser()->ascii();
  libmwaw::DebugStream f;

  // first read the id and the string delta pos
  long pos=idPosEntry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(FontName)[pos]:";
  int N=int(idPosEntry.length()/8);
  std::map<int, long> idPosMap;
  for (int i=0; i<N; ++i) {
    int id=(int) input->readULong(4);
    long dPos=(long) input->readULong(4);
    f << std::hex << dPos << std::dec << "[id=" << id << "],";
    idPosMap[id]=dPos;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  // now read the name
  pos=nameEntry.begin();
  long endPos=nameEntry.end();
  f.str("");
  f << "Entries(FontName):";
  for (std::map<int, long>::const_iterator it=idPosMap.begin(); it!=idPosMap.end(); ++it) {
    long dPos=it->second;
    if (dPos<0 || pos+dPos+1>endPos) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readFontNames: the name positions seems bad\n"));
      f << "###[id=" << it->first << "],";
      continue;
    }
    input->seek(pos+dPos, librevenge::RVNG_SEEK_SET);
    int sSz=(int) input->readULong(1);
    if (sSz==0 || pos+dPos+1+sSz>endPos) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readFontNames: the name size seems bad\n"));
      f << "###[id=" << it->first << "],";
      continue;
    }
    std::string name("");
    for (int i=0; i<sSz; ++i) name+=(char) input->readULong(1);
    f << name << "[id=" << it->first << "],";
    getParserState()->m_fontConverter->setCorrespondance(it->first, name);
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  return true;
}

bool MacDrawProParser::readColors(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readColors: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readColors: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = getRSRCParser()->getInput();
  libmwaw::DebugFile &ascFile = getRSRCParser()->ascii();
  libmwaw::DebugStream f;
  f << "Entries(ColorMap)";
  f << "[" << entry.id() << "]:";
  entry.setParsed(true);
  long pos=entry.begin();
  if ((entry.length()%16)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readColors: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  m_state->m_colorList.clear();
  int N=int(entry.length()/16);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "ColorMap-" << i << ":";
    int val=(int) input->readULong(4);
    if (val!=1) f << "num[used]=" << val << ",";
    val =(int) input->readULong(2); // index of ?
    f << "id0=" << val << ",";
    unsigned char col[3];
    for (int k=0; k < 3; k++)
      col[k] = (unsigned char)(input->readULong(2)>>8);
    MWAWColor color(col[0],col[1],col[2]);
    f << color << ",";
    m_state->m_colorList.push_back(color);
    val=(int) input->readULong(2); // always 0?
    if (val) f << "f0=" << val << ",";
    val =(int) input->readULong(2); // index of ?
    f << "id1=" << val << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProParser::readBWPatterns(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readBWPatterns: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readBWPatterns: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = getRSRCParser()->getInput();
  libmwaw::DebugFile &ascFile = getRSRCParser()->ascii();
  libmwaw::DebugStream f;
  f << "Entries(BWPattern)";
  f << "[" << entry.id() << "]:";
  entry.setParsed(true);
  long pos=entry.begin();
  if ((entry.length()%12)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readBWPatterns: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  m_state->m_BWPatternList.clear();
  int N=int(entry.length()/12);
  if (N!=m_state->m_numBWPatterns) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readBWPatterns: the number of patterns seems bad\n"));
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  MWAWGraphicStyle::Pattern pat;
  pat.m_dim=Vec2i(8,8);
  pat.m_data.resize(8);
  pat.m_colors[0]=MWAWColor::white();
  pat.m_colors[1]=MWAWColor::black();
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "BWPattern-" << i+1 << ":";
    int val=(int) input->readULong(4);
    if (val!=1) f << "num[used]=" << val << ",";
    for (size_t j=0; j<8; ++j)
      pat.m_data[j]=uint8_t(input->readULong(1));
    f << pat;
    m_state->m_BWPatternList.push_back(pat);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProParser::readColorPatterns(MWAWEntry const &entry)
{
  if (!entry.length()) { // ok not color patterns
    entry.setParsed(true);
    return true;
  }
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readColorPatterns: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readColorPatterns: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = getRSRCParser()->getInput();
  libmwaw::DebugFile &ascFile = getRSRCParser()->ascii();
  libmwaw::DebugStream f;
  f << "Entries(ColorPattern)";
  f << "[" << entry.id() << "]:";
  entry.setParsed(true);
  long pos=entry.begin();
  if ((entry.length()%70)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readColorPatterns: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  m_state->m_colorPatternList.clear();

  int N=int(entry.length()/70);
  if (N!=m_state->m_numColorPatterns) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readColorPatterns: the number of patterns seems bad\n"));
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (m_state->m_colorList.empty()) m_state->initColors();

  int numColors=(int)m_state->m_colorList.size();
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    MWAWPictBitmapIndexed bitmap(Vec2i(8,8));
    bitmap.setColors(m_state->m_colorList);

    f.str("");
    f << "ColorPattern-" << i+1 << ":";
    int val=(int) input->readULong(4);
    if (val!=1) f << "num[used]=" << val << ",";
    // the final pattern id
    f << "id=" << input->readLong(2) << ",";
    int indices[8];
    int sumColors[3]= {0,0,0};
    for (int r=0; r<8; ++r) {
      for (size_t c=0; c<8; ++c) {
        int id=(int) input->readULong(1);
        if (id<0 || id>=numColors) {
          static bool first=true;
          if (first) {
            MWAW_DEBUG_MSG(("MacDrawProParser::readColorPatterns: find some bad indices\n"));
            f << "#id=" << id << ",";
            first=false;
          }
          indices[c]=0;
          continue;
        }
        indices[c]=id;
        MWAWColor const &col=m_state->m_colorList[size_t(id)];
        sumColors[0]+=(int)col.getRed();
        sumColors[1]+=(int)col.getGreen();
        sumColors[2]+=(int)col.getBlue();
      }
      bitmap.setRow(r, indices);
    }
    MWAWGraphicStyle::Pattern pat;
    librevenge::RVNGBinaryData binary;
    std::string type;
    if (bitmap.getBinary(binary,type)) {
      pat=MWAWGraphicStyle::Pattern
          (Vec2i(8,8), binary, type,MWAWColor((unsigned char)(sumColors[0]/64),(unsigned char)(sumColors[1]/64),(unsigned char)(sumColors[2]/64)));
    }
    else {
      MWAW_DEBUG_MSG(("MacDrawProParser::readColorPatterns: oops can not create the binary data\n"));
      pat.m_dim=Vec2i(8,8);
      pat.m_data.resize(8, 0);
      pat.m_colors[0]=MWAWColor::white();
      pat.m_colors[1]=MWAWColor::black();
    }
    m_state->m_colorPatternList.push_back(pat);
    f << pat;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProParser::readPatternsToolList(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readPatternsToolList: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readPatternsToolList: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = getRSRCParser()->getInput();
  libmwaw::DebugFile &ascFile = getRSRCParser()->ascii();
  libmwaw::DebugStream f;
  f << "Entries(PatternTool)[" << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  if ((entry.length()%4)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readPatternsToolList: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  int N=int(entry.length()/4);
  if (N!=m_state->m_numPatternsInTool) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readPatternsToolList: the number of patterns seems bad\n"));
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "pats=[";
  for (int i=0; i<N; ++i) {
    int val=(int) input->readULong(2);
    if (val&0x8000) f << "C" << (val&0x7FFF);
    else if (val) f << "BW" << val;
    else f << "_";
    val=(int) input->readULong(2);
    if (val&0x8000) f << ":C" << (val&0x7FFF);
    else if (val) f << ":BW" << val;
    f << ",";
  }
  f << "],";
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool MacDrawProParser::readRulerSettings(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readRulerSettings: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readRulerSettings: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = getRSRCParser()->getInput();
  libmwaw::DebugFile &ascFile = getRSRCParser()->ascii();
  libmwaw::DebugStream f;
  f << "Entries(RulerSetting)[" << entry.type() << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  if ((entry.length()%24)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readRulerSettings: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  int N=int(entry.length()/24);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "RulerSetting[R" << i+1 << "]:";
    for (int j=0; j<2; ++j)
      f << "dim" << j << "=" << float(input->readULong(4))/65536.f << ",";
    int val;
    for (int j=0; j<2; ++j) { // seems related to RulerStyle flags
      val=(int) input->readULong(2);
      if (val) f << "fl" << j << "=" << std::hex << val << std::dec << ",";
    }
    for (int j=0; j<4; ++j) { // always 0
      val=(int) input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    val=(int) input->readLong(2);
    if (val) f << "numSub=" << val << ",";
    val=(int) input->readLong(2);
    if (val!=i+1) f << "id=" << val << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+24, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

bool MacDrawProParser::readViews(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readViews: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readViews: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = getRSRCParser()->getInput();
  libmwaw::DebugFile &ascFile = getRSRCParser()->ascii();
  libmwaw::DebugStream f;
  f << "Entries(View)[" << entry.type() << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  if ((entry.length()%8)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readViews: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  int N=int(entry.length()/8);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "View[V" << i+1 << "]:";
    int val=(int) input->readULong(2);
    if (!val) {
      ascFile.addPos(pos);
      ascFile.addNote("_");
      input->seek(pos+8, librevenge::RVNG_SEEK_SET);
      continue;
    }
    if (val!=1) f << "numUsed?=" << val << ",";
    val=(int) input->readULong(2); // 0 or 1
    if (val) f << "f0=" << val << ",";
    f << "pos=" << input->readLong(2) << "x" << input->readLong(2) << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

// unknown rsrc data
bool MacDrawProParser::readRSRCDstl(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readRSRCDstls: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readRSRCDstls: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = getRSRCParser()->getInput();
  libmwaw::DebugFile &ascFile = getRSRCParser()->ascii();
  libmwaw::DebugStream f;
  f << "Entries(UnknDstl)[" << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  if (entry.length()!=18) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readRSRCDstls: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<9; ++i) { // always -1?
    int val=(int) input->readLong(2);
    if (val!=-1) f << "f" << i << "=" << val << ",";
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}


////////////////////////////////////////////////////////////
//
// send data
//
////////////////////////////////////////////////////////////

bool MacDrawProParser::send(MacDrawProParserInternal::Shape const &shape)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDrawProParser::send: can not find the listener\n"));
    return false;
  }
  shape.m_isSent=true;
  Box2f box=shape.getBdBox();
  MWAWPosition pos(box[0], box.size(), librevenge::RVNG_POINT);
  pos.m_anchorTo = MWAWPosition::Page;
  switch (shape.m_type) {
  case MacDrawProParserInternal::Shape::Basic:
    listener->insertPicture(pos, shape.m_shape, shape.m_style);
    break;
  case MacDrawProParserInternal::Shape::Bitmap:
    return sendBitmap(shape, pos);
  case MacDrawProParserInternal::Shape::Group: {
    size_t numShapes=m_state->m_shapeList.size();
    if (!numShapes) break;
    listener->openLayer(pos);
    for (size_t i=0; i<shape.m_childList.size(); ++i) {
      if (shape.m_childList[i]>=numShapes) {
        MWAW_DEBUG_MSG(("MacDrawProParser::send: can not find a child\n"));
        continue;
      }
      MacDrawProParserInternal::Shape const &child=m_state->m_shapeList[shape.m_childList[i]];
      if (child.m_isSent) {
        MWAW_DEBUG_MSG(("MacDrawProParser::send: the child is already sent\n"));
        continue;
      }
      send(child);
    }
    listener->closeLayer();
    break;
  }
  case MacDrawProParserInternal::Shape::GroupEnd:
    break;
  case MacDrawProParserInternal::Shape::Text: {
    shared_ptr<MWAWSubDocument> doc(new MacDrawProParserInternal::SubDocument(*this, getInput(), shape.m_id));
    listener->insertTextBox(pos, doc, shape.m_style);
    return true;
  }
  case MacDrawProParserInternal::Shape::Unknown:
  default:
    return false;
  }
  return true;
}

bool MacDrawProParser::sendBitmap(MacDrawProParserInternal::Shape const &shape, MWAWPosition const &position)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDrawProParser::sendBitmap: can not find the listener\n"));
    return false;
  }
  if (!shape.m_bitmapEntry.valid()) return false;
  int const numBytesByRow=shape.m_numBytesByRow;
  if (shape.m_type!=MacDrawProParserInternal::Shape::Bitmap || numBytesByRow<=0 ||
      numBytesByRow*shape.m_bitmapFileDim.size()[1]<shape.m_bitmapEntry.length() ||
      shape.m_bitmapDim[0][0]<0 || shape.m_bitmapDim[0][1]<0 ||
      shape.m_bitmapDim[0][0]<shape.m_bitmapFileDim[0][0] ||
      shape.m_bitmapFileDim.size()[0]<=0 || shape.m_bitmapFileDim.size()[1]<=0 ||
      8*numBytesByRow<shape.m_bitmapFileDim.size()[0]) {
    MWAW_DEBUG_MSG(("MacDrawProParser::sendBitmap: the bitmap seems bad\n"));
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

bool MacDrawProParser::sendText(int zId)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDrawProParser::sendText: can not find the listener\n"));
    return false;
  }
  if (zId<0||zId>=(int) m_state->m_shapeList.size() ||
      m_state->m_shapeList[size_t(zId)].m_type != MacDrawProParserInternal::Shape::Text) {
    MWAW_DEBUG_MSG(("MacDrawProParser::sendText: can not find the text shape\n"));
    return false;
  }
  MacDrawProParserInternal::Shape const &shape=m_state->m_shapeList[size_t(zId)];
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
      MWAW_DEBUG_MSG(("MacDrawProParser::sendText: find char 0\n"));
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
