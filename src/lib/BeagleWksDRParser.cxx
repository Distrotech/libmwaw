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
#include <set>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWGraphicShape.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictData.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWGraphicListener.hxx"
#include "MWAWSubDocument.hxx"

#include "BeagleWksStructManager.hxx"

#include "BeagleWksDRParser.hxx"

/** Internal: the structures of a BeagleWksDRParser */
namespace BeagleWksDRParserInternal
{
//! Internal: the shape of BeagleWksDRParser
struct Shape {
  //! constructor
  Shape() : m_type(-1), m_box(), m_shape(), m_entry(), m_dataSize(0),
    m_style(), m_font(), m_justify(MWAWParagraph::JustificationLeft), m_interline(1), m_extra("")
  {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Shape const &shape);

  //! the shape type
  int m_type;
  //! the shape bdbox
  MWAWBox2f m_box;

  //! the graphic shape ( for line, rect, ...)
  MWAWGraphicShape m_shape;
  //! the textbox or picture entry
  MWAWEntry m_entry;
  //! the data size
  long m_dataSize;

  // style part

  //! the style
  MWAWGraphicStyle m_style;
  //! the textbox font
  MWAWFont m_font;
  //! the textbox justification
  MWAWParagraph::Justification m_justify;
  //! the interline in percent
  double m_interline;

  //! extra data
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, Shape const &shape)
{
  switch (shape.m_type) {
  case -1:
    break;
  case 1:
    o << "rect,";
    break;
  case 2:
    o << "circle,";
    break;
  case 3:
    o << "line,";
    break;
  case 4:
    o << "rectOval,";
    break;
  case 5:
    o << "arc,";
    break;
  case 6:
    o << "poly,";
    break;
  case 7:
    o << "textbox,";
    break;
  case 8:
    o << "group,";
    break;
  case 0xa:
    o << "poly[hand],";
    break;
  case 0xb:
    o << "picture,";
    break;
  default:
    o << "#type=" << shape.m_type << ",";
    break;
  }
  if (shape.m_box.size()[0]>0 || shape.m_box.size()[1]>0)
    o << "box=" << shape.m_box << ",";
  o << shape.m_style << ",";
  if (shape.m_dataSize) o << "size[data]=" << shape.m_dataSize << ",";
  o << shape.m_extra;
  return o;
}

////////////////////////////////////////
//! Internal: the state of a BeagleWksDRParser
struct State {
  //! constructor
  State() :  m_graphicBegin(-1), m_typeEntryMap(), m_colorList(), m_patternList(), m_shapeList(),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
    m_numShapes[0]=m_numShapes[1]=0;
    m_shapesBegin[0]=m_shapesBegin[1]=0;
  }

  /** the graphic begin position */
  long m_graphicBegin;
  /** the shape definitions and the unknown */
  long m_shapesBegin[2];
  /** the type entry map */
  std::multimap<std::string, MWAWEntry> m_typeEntryMap;
  /** the colors list */
  std::vector<MWAWColor> m_colorList;
  /** the pattern list */
  std::vector<MWAWGraphicStyle::Pattern> m_patternList;
  /** the number of shapes: positions and definitions */
  int m_numShapes[2];
  /** the list of shapes */
  std::vector<Shape> m_shapeList;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a BeagleWksDRParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(BeagleWksDRParser &pars, MWAWInputStreamPtr input, int zoneId) : MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

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
    MWAW_DEBUG_MSG(("BeagleWksDRParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  if (!m_parser) {
    MWAW_DEBUG_MSG(("BeagleWksDRParserInternal::SubDocument::parse: no parser\n"));
    return;
  }
  long pos = m_input->tell();
  static_cast<BeagleWksDRParser *>(m_parser)->sendText(m_id);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
BeagleWksDRParser::BeagleWksDRParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWGraphicParser(input, rsrcParser, header), m_state(), m_structureManager()
{
  init();
}

BeagleWksDRParser::~BeagleWksDRParser()
{
}

void BeagleWksDRParser::init()
{
  resetGraphicListener();
  setAsciiName("main-1");

  m_state.reset(new BeagleWksDRParserInternal::State);
  m_structureManager.reset(new BeagleWksStructManager(getParserState()));

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}

MWAWInputStreamPtr BeagleWksDRParser::rsrcInput()
{
  return getRSRCParser()->getInput();
}

libmwaw::DebugFile &BeagleWksDRParser::rsrcAscii()
{
  return getRSRCParser()->ascii();
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
Vec2f BeagleWksDRParser::getPageLeftTop() const
{
  return Vec2f(float(getPageSpan().getMarginLeft()),
               float(getPageSpan().getMarginTop()+m_state->m_headerHeight/72.0));
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void BeagleWksDRParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!getGraphicListener() || m_state->m_actPage == 1)
      continue;
    getGraphicListener()->insertBreak(MWAWGraphicListener::PageBreak);
  }
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void BeagleWksDRParser::parse(librevenge::RVNGDrawingInterface *docInterface)
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
      sendPageFrames();
      sendPictures();
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetGraphicListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void BeagleWksDRParser::createDocument(librevenge::RVNGDrawingInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getGraphicListener()) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  int numPages = 1;
  m_state->m_numPages = numPages;

  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan ps(getPageSpan());
  ps.setPageSpan(numPages);
  pageList.push_back(ps);

  MWAWGraphicListenerPtr listen(new MWAWGraphicListener(*getParserState(), pageList, documentInterface));
  setGraphicListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool BeagleWksDRParser::createZones()
{
  readRSRCZones();
  MWAWInputStreamPtr input = getInput();
  if (input->seek(66, librevenge::RVNG_SEEK_SET) || !readPrintInfo())
    return false;
  long pos = input->tell();
  if (!input->checkPosition(pos+70)) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::createZones: the file can not contains zones\n"));
    return false;
  }

  // now read the list of zones
  libmwaw::DebugStream f;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(Zones):";
  for (int i=0; i<7; ++i) { // checkme: at least 2 zones, i=2 is not a zone, maybe 7
    MWAWEntry entry;
    entry.setBegin(input->readLong(4));
    entry.setLength(input->readLong(4));
    entry.setId((int) input->readLong(2));
    if (entry.length()==0) continue;
    entry.setType(i==1?"Frame":"Unknown");
    f << entry.type() << "[" << entry.id() << "]="
      << std::hex << entry.begin() << "<->" << entry.end() << ",";
    if (!entry.valid() || !input->checkPosition(entry.end())) {
      f << "###";
      if (i<2) {
        MWAW_DEBUG_MSG(("BeagleWksDRParser::createZones: can not read the header zone, stop\n"));
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        return false;
      }
      if (i!=2) {
        MWAW_DEBUG_MSG(("BeagleWksDRParser::createZones: can not zones entry %d\n",i));
      }
      continue;
    }
    m_state->m_typeEntryMap.insert
    (std::multimap<std::string, MWAWEntry>::value_type(entry.type(),entry));
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // now parse the different zones
  std::multimap<std::string, MWAWEntry>::iterator it;
  it=m_state->m_typeEntryMap.find("FontNames");
  if (it!=m_state->m_typeEntryMap.end())
    m_structureManager->readFontNames(it->second);
  it=m_state->m_typeEntryMap.find("Frame");
  if (it!=m_state->m_typeEntryMap.end())
    m_structureManager->readFrame(it->second);

  // now parse the different zones
  for (it=m_state->m_typeEntryMap.begin(); it!=m_state->m_typeEntryMap.end(); ++it) {
    MWAWEntry const &entry=it->second;
    if (entry.isParsed())
      continue;
    f.str("");
    f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
    ascii().addPos(entry.end());
    ascii().addNote("_");
  }

  input->seek(m_state->m_graphicBegin, librevenge::RVNG_SEEK_SET);
  pos=input->tell();
  if (!readGraphicHeader()) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::createZones: can not read the graphic header\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(DrHeader):###");
    return false;
  }

  pos=input->tell();
  if (m_state->m_shapesBegin[0]<pos || !readShapeDefinitions()) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::createZones: can not find the shape definitions pointer\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(ShapeDef):###");
    return false;
  }
  if (pos!=m_state->m_shapesBegin[0]) {
    ascii().addPos(pos);
    ascii().addNote("_");
  }

  pos=input->tell();
  if (m_state->m_shapesBegin[1]<pos || !readShapeDatas()) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::createZones: can not find the shape data pointer\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(ShapeData):###");
    return true;
  }
  if (pos!=m_state->m_shapesBegin[1]) {
    ascii().addPos(pos);
    ascii().addNote("_");
  }

  if (!input->isEnd()) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::createZones: find some extra data\n"));
    ascii().addPos(input->tell());
    ascii().addNote("Entries(ZoneEnd)");
  }
  return true;
}

bool BeagleWksDRParser::readRSRCZones()
{
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  if (!rsrcParser)
    return true;

  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;
  // the 1 zone
  char const *(zNames[]) = {"wPos", "DMPF" };
  for (int z = 0; z < 2; ++z) {
    it = entryMap.lower_bound(zNames[z]);
    while (it != entryMap.end()) {
      if (it->first != zNames[z])
        break;
      MWAWEntry const &entry = it++->second;
      switch (z) {
      case 0: // 1001
        m_structureManager->readwPos(entry);
        break;
      case 1: // find in one file with id=4661 6a1f 4057
        m_structureManager->readFontStyle(entry);
        break;
      /* find also
         - edpt: see sendPicture
         - DMPP: the paragraph style
         - sect and alis: position?, alis=filesystem alias(dir, filename, path...)
      */
      default:
        break;
      }
    }
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the graphic data
////////////////////////////////////////////////////////////
bool BeagleWksDRParser::readGraphicHeader()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (!input->checkPosition(pos+112))
    return false;
  libmwaw::DebugStream f;
  f << "Entries(DrHeader):";
  int val=(int) input->readLong(2);
  if (val) f << "f0=" << val << ",";
  val=(int) input->readLong(2);
  if (val!=4) f << "f1=" << val << ",";
  m_state->m_numShapes[0]=(int) input->readULong(2);
  f << "num[shapesPos]=" << m_state->m_numShapes[0] << ",";
  for (int i=0; i<8; ++i) {
    static int const expected[]= {0, 0x1144, 0, 7, 0, 0x5a8, 0, 0xfcc };
    val=(int) input->readLong(2);
    if (val != expected[i]) f << "f" << i+2 << "=" << val << ",";
  }
  m_state->m_numShapes[1]=(int) input->readULong(2);
  f << "num[shapesDef]=" << m_state->m_numShapes[1] << ",";
  for (int i=0; i<2; ++i) {
    m_state->m_shapesBegin[i]= pos+input->readLong(4);
    f << "ptr" << i << "=" << std::hex << m_state->m_shapesBegin[i] << std::dec << ",";
    if (!input->checkPosition(m_state->m_shapesBegin[i])) {
      MWAW_DEBUG_MSG(("BeagleWksDRParser::readGraphicHeader: the shapes pointers seems bad\n"));
      f << "###";
      m_state->m_shapesBegin[i]=0;
    }
  }
  for (int i=0; i<2; ++i) {
    val=(int) input->readLong(2);
    if (val!=2*i) f << "g" << i << "=" << val << ",";
  }
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int) input->readULong(2);
  f << "dim=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
  // checkme: followed by some flag?
  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+62, librevenge::RVNG_SEEK_SET);

  // DOME: the actual format, must be read with the same code than readShapeDefinitions
  pos=input->tell();
  f.str("");
  f << "DrHeader-style:";
  BeagleWksDRParserInternal::Shape shape;
  if (!readStyle(shape)) f << "###";
  f << shape;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+50, librevenge::RVNG_SEEK_SET);

  if (!readPatterns() || !readColors() || !readArrows() || !readShapePositions()) return false;
  return true;
}

//  read the colors
bool BeagleWksDRParser::readColors()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Color):";
  if (!input->checkPosition(pos+16)) {
    f << "###";
    MWAW_DEBUG_MSG(("BeagleWksDRParser::readColors: the header size seems too short\n"));
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  /* checkme: {N0,N1,N2} is probably {num defined, max defined, first
     free} but in which order? */
  int maxN=(int)input->readULong(2);
  f << "N0=" << maxN << ",";
  int val=(int)input->readULong(2);
  if (val!=maxN) f << "N1=" << val << ",";
  if (val>maxN) maxN=val;
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  val=(int)input->readULong(2);
  if (val!=6) f << "f0=" << val << ",";
  val=(int)input->readULong(2);
  if (val!=maxN) f << "N2=" << val << ",";
  if (val>maxN) maxN=val;
  int fSz=(int) input->readULong(2);
  f << "fSz=" << fSz << ",";
  long dSz=(long) input->readULong(4);
  if (!input->checkPosition(pos+16+dSz) || fSz<10 || dSz!=N*fSz) {
    f << "###";
    MWAW_DEBUG_MSG(("BeagleWksDRParser::readColors: the color size seems bad\n"));
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  m_state->m_colorList.resize(size_t(maxN));
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    if (i>=maxN) {
      ascii().addPos(pos);
      ascii().addNote("_");
      input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
      continue;
    }
    f.str("");
    f << "Color-" << i << ":";
    for (int j=0; j<2; ++j) { // f0=0, f1=small number: rsrcId ?
      val=(int) input->readLong(2);
      if (val) f<<"f" << j << "=" << val << ",";
    }
    unsigned char color[3];
    for (int c=0; c < 3; c++) color[c] = (unsigned char)(input->readULong(2)/256);
    m_state->m_colorList[size_t(i)]=MWAWColor(color[0], color[1],color[2]);
    f << "col=" << m_state->m_colorList[size_t(i)] << ",";

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

//  read the patterns
bool BeagleWksDRParser::readPatterns()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Pattern):";
  if (!input->checkPosition(pos+16)) {
    f << "###";
    MWAW_DEBUG_MSG(("BeagleWksDRParser::readPatterns: the header size seems too short\n"));
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int maxN=(int)input->readULong(2);
  f << "N0=" << maxN << ",";
  int val=(int)input->readULong(2);
  if (val!=maxN) f << "N1=" << val << ",";
  if (val>maxN) maxN=val;
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  val=(int)input->readULong(2);
  if (val!=6) f << "f0=" << val << ",";
  val=(int)input->readULong(2);
  if (val!=maxN) f << "N2=" << val << ",";
  if (val>maxN) maxN=val;
  int fSz=(int) input->readULong(2);
  f << "fSz=" << fSz << ",";
  long dSz=(long) input->readULong(4);
  if (!input->checkPosition(pos+16+dSz) || fSz<10 || dSz!=N*fSz) {
    f << "###";
    MWAW_DEBUG_MSG(("BeagleWksDRParser::readPatterns: the pattern size seems bad\n"));
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  m_state->m_patternList.resize(size_t(maxN));
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    if (i>=maxN) {
      ascii().addPos(pos);
      ascii().addNote("_");
      input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
      continue;
    }
    f.str("");
    f << "Pattern-" << i << ":";
    val=(int) input->readLong(2); // always 0?
    if (val) f << "f0=" << val << ",";
    MWAWGraphicStyle::Pattern pat;
    pat.m_dim=Vec2i(8,8);
    pat.m_data.resize(8);
    for (size_t j=0; j<8; ++j) pat.m_data[j]=(unsigned char) input->readULong(1);
    m_state->m_patternList[size_t(i)]=pat;
    f << "pat=[" << pat << "],";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

//  read a unknown zone: maybe the arrow definition
bool BeagleWksDRParser::readArrows()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Arrows):";
  if (!input->checkPosition(pos+16)) {
    f << "###";
    MWAW_DEBUG_MSG(("BeagleWksDRParser::readArrows: the header size seems too short\n"));
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int maxN=(int)input->readULong(2);
  f << "N0=" << maxN << ",";
  int val=(int)input->readULong(2);
  if (val!=maxN) f << "N1=" << val << ",";
  if (val>maxN) maxN=val;
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  val=(int)input->readULong(2);
  if (val!=6) f << "f0=" << val << ",";
  val=(int)input->readULong(2);
  if (val!=maxN) f << "N2=" << val << ",";
  if (val>maxN) maxN=val;
  int fSz=(int) input->readULong(2);
  f << "fSz=" << fSz << ",";
  long dSz=(long) input->readULong(4);
  if (!input->checkPosition(pos+16+dSz) || fSz<0x3c || dSz!=fSz*N) {
    f << "###";
    MWAW_DEBUG_MSG(("BeagleWksDRParser::readArrows: the pattern size seems bad\n"));
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    if (i>=maxN) {
      ascii().addPos(pos);
      ascii().addNote("_");
      input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
      continue;
    }
    f.str("");
    f << "Arrows-" << i << ":";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

//  read the shape positions
bool BeagleWksDRParser::readShapePositions()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;
  if (m_state->m_numShapes[0]<0 || !input->checkPosition(pos+m_state->m_numShapes[0]*20)) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::readShapePositions: can not read the shape positions\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(ShapePos):###");
    return false;
  }
  m_state->m_shapeList.resize(size_t(m_state->m_numShapes[0]));
  for (int i=0; i<m_state->m_numShapes[0]; ++i) {
    BeagleWksDRParserInternal::Shape &shape= m_state->m_shapeList[size_t(i)];
    pos=input->tell();
    f.str("");
    f << "Entries(ShapePos)[" << i << "]:";
    int val=(int) input->readULong(2);
    if (val!=i) f << "#id=" << val << ",";
    val=(int) input->readULong(1);
    if (val&0x8) {
      f << "locked,";
      val &= 0xF7;
    }
    if (val!=0x10) f << "flags=" << std::hex << val << std::dec << ",";
    val=(int) input->readULong(1); // a small number 3|7
    f << "f0=" << val << ",";
    float dim[4];
    for (int j=0; j<4; ++j) dim[j]=float(input->readLong(4))/65536.f;
    shape.m_box=MWAWBox2f(Vec2f(dim[1],dim[0]),Vec2f(dim[3],dim[2]));
    f << "pos=" << shape.m_box << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+20, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

//  read the shape definitions
bool BeagleWksDRParser::readShapeDefinitions()
{
  MWAWInputStreamPtr input = getInput();
  if (!input->checkPosition(m_state->m_shapesBegin[0]+m_state->m_numShapes[1]*56))
    return false;
  input->seek(m_state->m_shapesBegin[0], librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  if (m_state->m_numShapes[1]>int(m_state->m_shapeList.size())) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::readShapeDefinitions: the number of shapes definitions seems too big\n"));
    m_state->m_shapeList.resize(size_t(m_state->m_numShapes[1]));
  }
  for (int i=0; i<m_state->m_numShapes[1]; ++i) {
    BeagleWksDRParserInternal::Shape &shape=m_state->m_shapeList[size_t(i)];
    long pos=input->tell();
    f.str("");
    shape.m_type=(int) input->readULong(2);
    int val=(int)input->readLong(2); // always 0 ?
    if (val) f << "f0=" << val << ",";
    int lineFlags=0;
    val=(int)input->readULong(2);
    if (shape.m_type==3) {
      lineFlags=val;
      if (lineFlags&1) {
        shape.m_style.m_arrows[1]=true;
        f << "arrow[end],";
      }
      if (lineFlags&2) {
        shape.m_style.m_arrows[0]=true;
        f << "arrow[start],";
      }
      val &= 0xFFCC;
    }
    if (val & 0x10) {
      shape.m_style.m_flip[0] = true;
      f << "flip[hori],";
    }
    if (val & 0x20) {
      shape.m_style.m_flip[1] = true;
      f << "flip[verti],";
    }
    val &= 0xFFCF;
    if (val) f << "fl=" << std::hex << val << std::dec << ",";
    switch (shape.m_type) {
    case 1:
      shape.m_shape = MWAWGraphicShape::rectangle(shape.m_box);
      break;
    case 2:
      shape.m_shape = MWAWGraphicShape::circle(shape.m_box);
      break;
    case 3:
      switch ((lineFlags>>4)&3) {
      case 1:
        shape.m_shape = MWAWGraphicShape::line(shape.m_box[0], shape.m_box[1]);
        break;
      case 2:
        shape.m_shape = MWAWGraphicShape::line(shape.m_box[1], shape.m_box[0]);
        break;
      default:
      case 0:
        shape.m_shape = MWAWGraphicShape::line(Vec2f(shape.m_box[1][0],shape.m_box[0][1]), Vec2f(shape.m_box[0][0],shape.m_box[1][1]));
        break;
      case 3:
        shape.m_shape = MWAWGraphicShape::line(Vec2f(shape.m_box[0][0],shape.m_box[1][1]), Vec2f(shape.m_box[1][0],shape.m_box[0][1]));
        break;
      }
      break;
    case 4:
      shape.m_shape = MWAWGraphicShape::rectangle(shape.m_box, Vec2f(25,25));
      break;
    case 6:
    case 10:
      shape.m_shape = MWAWGraphicShape::polygon(shape.m_box);
      break;
    default:
      break;
    }
    // now the style
    if (!readStyle(shape))
      f << "##";
    shape.m_extra+=f.str();
    f.str("");
    f << "Entries(ShapeDef)[" << i << "]:" << shape;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+56, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

bool BeagleWksDRParser::readShapeDatas()
{
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  for (size_t i=0; i<m_state->m_shapeList.size(); ++i) {
    BeagleWksDRParserInternal::Shape &shape=m_state->m_shapeList[i];
    long dataSize=shape.m_dataSize;
    if (!dataSize)
      continue;
    long pos = input->tell();
    f.str("");
    f << "Entries(ShapeData)[" << i << "]:type=" << shape.m_type << ",";
    bool ok=false;
    switch (shape.m_type) {
    case 6:
    case 10: {
      if (dataSize<2)
        break;
      int N=(int) input->readULong(2);
      if (dataSize!=2+N*8) break;
      f << "pts=[";
      ok=true;
      std::vector<Vec2f> &vertices=shape.m_shape.m_vertices;
      vertices.resize(size_t(N));
      for (int pt=0; pt<N; ++pt) {
        float position[2];
        for (int j=0; j<2; ++j) position[j]=float(input->readLong(4))/65536.f;
        Vec2f point(position[1],position[0]);
        vertices[size_t(pt)]=point;
        f << point << ",";
      }
      f << "],";
      break;
    }
    case 7: {
      if (dataSize<10) break;
      int N=(int) input->readULong(2);
      if (10+N*2!=dataSize) break;
      f << "begPos=[";
      for (int line=0; line<N; ++line)
        f << input->readULong(2) << ",";
      f << "],";
      f << "height=" << input->readLong(2) << ","; // checkme
      f << "unkn=" << std::hex << input->readULong(2) << std::dec << ",";
      int val = (int) input->readULong(2);
      if (val) f<< "f0=" << val << ","; // checkme: dataSize can also be a 32int
      int len=(int) input->readULong(2);
      dataSize+=len;
      if (!input->checkPosition(pos+dataSize)) break;
      ok=true;
      shape.m_entry.setBegin(input->tell());
      shape.m_entry.setLength(len);
      for (int c=0; c<len; ++c) {
        char ch=(char) input->readULong(1);
        f << ch;
      }
      break;
    }
    case 11:
      if (dataSize<2) break;
      ok=long(input->readULong(2))==dataSize;
      shape.m_entry.setBegin(pos);
      shape.m_entry.setLength(dataSize);
      ascii().skipZone(pos,pos+dataSize-1);
      break;
    default:
      MWAW_DEBUG_MSG(("BeagleWksDRParser::readShapeDatas: find unexpected type\n"));
    }
    if (!ok || !input->checkPosition(pos+dataSize)) {
      MWAW_DEBUG_MSG(("BeagleWksDRParser::readShapeDatas: the data seems bad\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return false;
    }
    input->seek(pos+dataSize, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool BeagleWksDRParser::readStyle(BeagleWksDRParserInternal::Shape &shape)
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (!input->checkPosition(pos+50)) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::readStyle: the style zone seems too short\n"));
    return false;
  }
  libmwaw::DebugStream f;
  int val=(int)input->readLong(2); // always 8 ?
  if (val!=8) f << "f0=" << val << ",";
  int penSize[2];
  for (int p=0; p<2; ++p) {
    penSize[p]=(int) input->readLong(2);
    val=(int)input->readLong(2); // always 0 ?
    if (val) f << "f" << p+1 << "=" << val << ",";
  }
  if (penSize[0]!=penSize[1]) f << "penSize=" << penSize[1] << "x" << penSize[0] << ",";
  shape.m_style.m_lineWidth=float(penSize[0]+penSize[1])/2.f;
  int patternIds[2], colorIds[4];
  for (int p=0; p<2; ++p) patternIds[p]=(int) input->readLong(2);
  for (int p=0; p<4; ++p) colorIds[p]=(int) input->readLong(2);
  // set some default value
  if (patternIds[1]) shape.m_style.setSurfaceColor(MWAWColor::white());
  for (int i=0; i<2; ++i) {
    if (shape.m_type==-1) {
      // data are not yet initialised, so...
      if (i==0) f << "line=";
      else f << "surf=";
      f << "[pat=" << patternIds[i] << ",col0=" << colorIds[2*i] << ",col0=" << colorIds[2*i] << "],";
      continue;
    }
    if (patternIds[i]<0||patternIds[i]>=int(m_state->m_patternList.size())) {
      MWAW_DEBUG_MSG(("BeagleWksDRParser::readStyle: can not find a pattern\n"));
      f << "#pattern[" << i << "]=" << patternIds[i] << ",";
      continue;
    }
    if (patternIds[i]==0) { // no pattern
      if (i==0)
        shape.m_style.m_lineWidth=0;
      else
        shape.m_style.setSurfaceColor(MWAWColor::white(), 0);
      continue;
    }
    MWAWGraphicStyle::Pattern pattern=m_state->m_patternList[size_t(patternIds[i])];
    for (int j=0; j<2; ++j) {
      if (colorIds[2*i+j]<0||colorIds[2*i+j]>int(m_state->m_colorList.size())) {
        MWAW_DEBUG_MSG(("BeagleWksDRParser::readStyle: can not find a pattern\n"));
        f << "#color[" << i << "," << j << "]=" << colorIds[2*i+j] << ",";
        continue;
      }
      pattern.m_colors[1-j]=m_state->m_colorList[size_t(colorIds[2*i+j])];
    }
    MWAWColor color;
    if (i==0) {
      if (pattern.getAverageColor(color))
        shape.m_style.m_lineColor=color;
    }
    else if (pattern.getUniqueColor(color))
      shape.m_style.setSurfaceColor(color);
    else
      shape.m_style.m_pattern=pattern;
  }
  shape.m_style.m_rotate=(float)-input->readLong(2);
  int dim[2];
  for (int p=0; p<2; ++p) dim[p]=(int) input->readLong(2);
  bool defaultDim=dim[0]==25 && dim[1]==25;
  switch (shape.m_type) {
  case 4:
    shape.m_shape.m_cornerWidth=Vec2f(float(dim[1]), float(dim[0]));
    if (!defaultDim) f << "corner=" << dim[1] << "x" << dim[0] << ",";
    break;
  case 5: {
    f << "angle=" << dim[0] << "x" << dim[0]+dim[1] << ",";
    int angle[2] = { 90-dim[0]-dim[1], 90-dim[0] };
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
    MWAWBox2f box=shape.m_box;
    Vec2f center = box.center();
    Vec2f axis = 0.5*Vec2f(box.size());
    // we must compute the real bd box
    float minVal[2] = { 0, 0 }, maxVal[2] = { 0, 0 };
    int limitAngle[2];
    for (int j = 0; j < 2; j++)
      limitAngle[j] = (angle[j] < 0) ? int(angle[j]/90)-1 : int(angle[j]/90);
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
    MWAWBox2f realBox(Vec2f(center[0]+minVal[0],center[1]+minVal[1]),
                      Vec2f(center[0]+maxVal[0],center[1]+maxVal[1]));
    shape.m_box=realBox;
    shape.m_shape = MWAWGraphicShape::pie(realBox, box, Vec2f(float(angle[0]),float(angle[1])));
    break;
  }
  default:
    if (!defaultDim) f << "#dim=" << dim[1] << "x" << dim[0] << ",";
    break;
  }
  if (shape.m_type==3 || shape.m_type==5) { // g0:find 0-1 for arc
    for (int j=0; j<5; ++j) {
      val=(int) input->readLong(2);
      if (!val) continue;
      if (j==1 && shape.m_type==3) f << "arrowId=" << val << ",";
      else f << "g" << j << "=" << val << ",";
    }
  }
  else {
    MWAWFont &font=shape.m_font;
    if (shape.m_type==-1) // not initialized
      f << "fId=" << input->readULong(2) << ",";
    else
      font.setId(m_structureManager->getFontId((int) input->readULong(2)));
    font.setSize((float) input->readULong(2));
    int flag=(int) input->readULong(2);
    uint32_t flags=0;
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0x8) flags |= MWAWFont::embossBit;
    if (flag&0x10) flags |= MWAWFont::shadowBit;
    font.setFlags(flags);
    f << "font=[" << font.getDebugString(getParserState()->m_fontConverter) << "],";
    if (flag&0xFFE0) f << "fFlags=" << std::hex << (flag&0xFFE0) << std::dec << ",";
    val=(int) input->readULong(2); // 0
    if (val) f << "g0=" << val << ",";
    val=(int) input->readULong(2); // 0
    if (shape.m_type==7) {
      switch (val) { // what is the logic ?
      case 0x1e:
        font.setColor(MWAWColor(255,255,255));
        break;
      case 0x21:
        break;
      case 0x199:
        font.setColor(MWAWColor(0,0,255));
        break;
      case 0x155:
        font.setColor(MWAWColor(0,255,0));
        break;
      case 0xcd:
        font.setColor(MWAWColor(255,0,0));
        break;
      case 0x111:
        font.setColor(MWAWColor(0,255,255));
        break;
      case 0x89:
        font.setColor(MWAWColor(255,0,255));
        break;
      case 0x45:
        font.setColor(MWAWColor(255,255,0));
        break;
      default:
        MWAW_DEBUG_MSG(("BeagleWksDRParser::readStyle: find unknown font color\n"));
        f << "#fColor=" << std::hex << val << std::dec << ",";
      }
    }
    else if (val) f << "g1=" << val << ",";
  }
  val=(int) input->readLong(2);
  switch (val) {
  case 0:
    break;
  case 1:
    shape.m_justify=MWAWParagraph::JustificationCenter;
    f << "center,";
    break;
  case -1:
    shape.m_justify=MWAWParagraph::JustificationRight;
    f << "right,";
    break;
  default:
    f << "#align=" << val << ",";
  }
  val=(int) input->readLong(2);
  switch (val) {
  case -1:
    break;
  case -2:
    shape.m_interline=2;
    f << "spacing=200%,";
    break;
  case -3:
    shape.m_interline=1.5;
    f << "spacing=150%,";
    break;
  default:
    f << "#spacing=" << val << ",";
  }
  val=(int) input->readLong(2);
  if (val) f << "h0=" << val << ",";
  shape.m_dataSize=(long) input->readULong(2);
  shape.m_extra=f.str();
  ascii().addDelimiter(input->tell(),'|');
  input->seek(pos+50, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool BeagleWksDRParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (!input->checkPosition(pos+0x70))
    return false;

  libmwaw::DebugStream f;
  // print info
  libmwaw::PrinterInfo info;
  if (!info.read(input)) return false;
  f << "Entries(PrintInfo):"<< info;

  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  // define margin from print info
  Vec2i lTopMargin= -1 * info.paper().pos(0);
  Vec2i rBotMargin=info.paper().pos(1) - info.page().pos(1);

  // move margin left | top
  int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
  int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
  lTopMargin -= Vec2i(decalX, decalY);
  rBotMargin += Vec2i(decalX, decalY);

  // decrease right | bottom
  int rightMarg = rBotMargin.x() -10;
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
  input->seek(pos+0x78, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != pos+0x78) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::readPrintInfo: file is too short\n"));
    return false;
  }
  ascii().addPos(input->tell());

  return true;
}

////////////////////////////////////////////////////////////
// send shapes: vector graphic document
////////////////////////////////////////////////////////////
bool BeagleWksDRParser::sendPictures()
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::sendPictures: can not find the listener\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();

  for (size_t i=0; i<m_state->m_shapeList.size(); i++) {
    BeagleWksDRParserInternal::Shape const &shape=m_state->m_shapeList[i];
    MWAWBox2f box=shape.m_box;
    /* if m_rotate!=0, the box is the rotate box, so we need to rotate
       it back Normally, ok because the angle is a multiple of 90,
       but, in pratical, the new center is not good...
     */
    if (shape.m_style.m_rotate<0 || shape.m_style.m_rotate>0)
      box=libmwaw::rotateBoxFromCenter(box, -shape.m_style.m_rotate);

    MWAWPosition pos(box[0], box.size(), librevenge::RVNG_POINT);
    pos.setPage(1);
    pos.m_anchorTo=MWAWPosition::Page;
    switch (shape.m_type) {
    case 8: // group
      break;
    case 7: {
      shared_ptr<MWAWSubDocument> doc(new BeagleWksDRParserInternal::SubDocument(*this, input, (int) i));
      MWAWGraphicStyle style=shape.m_style;
      style.m_lineWidth=0;
      listener->insertTextBox(pos, doc, style);
      break;
    }
    case 0xb:
      if (!shape.m_entry.valid()) {
        MWAW_DEBUG_MSG(("BeagleWksDRParser::sendPictures: the picture entry seems bad\n"));
        break;
      }
      else {
        input->seek(shape.m_entry.begin(), librevenge::RVNG_SEEK_SET);
        shared_ptr<MWAWPict> thePict(MWAWPictData::get(input, (int)shape.m_entry.length()));
        librevenge::RVNGBinaryData data;
        std::string type;
        if (thePict && thePict->getBinary(data,type)) {
          MWAWGraphicStyle style;
          style.m_lineWidth=0;
          style.setSurfaceColor(MWAWColor::white());
          listener->insertPicture(pos, data, type, style);
        }
        else {
          MWAW_DEBUG_MSG(("BeagleWksDRParser::sendPictures: can not check the picture data\n"));
          break;
        }
      }
      break;
    default:
      if (shape.m_type >= 1 && shape.m_type<=10 && shape.m_type != 9)
        listener->insertPicture(pos, shape.m_shape, shape.m_style);
      else {
        MWAW_DEBUG_MSG(("BeagleWksDRParser::sendPictures: find unknown shape type\n"));
      }
      break;
    }
  }
  return true;
}

bool BeagleWksDRParser::sendText(int id)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener || !listener->canWriteText()) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::sendText: can not find the listener\n"));
    return false;
  }
  if (id<0 || id>=(int)m_state->m_shapeList.size()) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::sendText: can not find the textbox\n"));
    return false;
  }
  BeagleWksDRParserInternal::Shape const &shape=m_state->m_shapeList[size_t(id)];
  if (!shape.m_entry.valid() || shape.m_type != 7) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::sendText: the textbox seems invalid\n"));
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
  for (long i = 0; i < shape.m_entry.length(); i++) {
    unsigned char c = (unsigned char) input->readULong(1);
    switch (c) {
    case 0x9:
      listener->insertTab();
      break;
    case 0xd:
      if (i+1==shape.m_entry.length()) break;
      // this is marks the end of a paragraph
      listener->insertEOL();
      break;
    default:
      listener->insertCharacter(c);
      break;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////
// send other data
////////////////////////////////////////////////////////////
bool BeagleWksDRParser::sendPageFrames()
{
  std::map<int, BeagleWksStructManager::Frame> const &frameMap = m_structureManager->getIdFrameMap();
  std::map<int, BeagleWksStructManager::Frame>::const_iterator it;
  for (it=frameMap.begin(); it!=frameMap.end(); ++it)
    sendFrame(it->second);
  return true;
}

bool BeagleWksDRParser::sendFrame(BeagleWksStructManager::Frame const &frame)
{
  MWAWPosition fPos(Vec2f(0,0), frame.m_dim, librevenge::RVNG_POINT);

  fPos.setPagePos(frame.m_page > 0 ? frame.m_page : 1, frame.m_origin);
  fPos.setRelativePosition(MWAWPosition::Page);
  fPos.m_wrapping = frame.m_wrap==0 ? MWAWPosition::WNone : MWAWPosition::WDynamic;

  MWAWGraphicStyle style=MWAWGraphicStyle::emptyStyle();
  style.setBorders(frame.m_bordersSet, frame.m_border);
  return sendPicture(frame.m_pictId, fPos, style);
}

// read/send picture (edtp resource)
bool BeagleWksDRParser::sendPicture
(int pId, MWAWPosition const &pictPos, MWAWGraphicStyle const &style)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::sendPicture: can not find the listener\n"));
    return false;
  }
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  if (!rsrcParser) {
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("BeagleWksDRParser::sendPicture: need access to resource fork to retrieve picture content\n"));
      first=false;
    }
    return true;
  }

  librevenge::RVNGBinaryData data;
  if (!m_structureManager->readPicture(pId, data))
    return false;

  listener->insertPicture(pictPos, data, "image/pict", style);
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool BeagleWksDRParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = BeagleWksDRParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->checkPosition(66))
    return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";

  input->seek(0, librevenge::RVNG_SEEK_SET);
  if (input->readLong(2)!=0x4257 || input->readLong(2)!=0x6b73 ||
      input->readLong(2)!=0x4257 || input->readLong(2)!=0x6472 ||
      input->readLong(2)!=0x4257 || input->readLong(2)!=0x6472)
    return false;
  for (int i=0; i < 9; ++i) { // f2=f6=1 other 0
    int val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  setVersion(1);

  if (header)
    header->reset(MWAWDocument::MWAW_T_BEAGLEWORKS, 1, MWAWDocument::MWAW_K_DRAW);

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  long pos=input->tell();
  f.str("");
  f << "FileHeader-II:";
  m_state->m_graphicBegin=input->readLong(4);
  if (!input->checkPosition(m_state->m_graphicBegin)) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::checkHeader: can not read the graphic position\n"));
    return false;
  }
  f << "graphic[ptr]=" << std::hex << m_state->m_graphicBegin << std::dec << ",";
  for (int i=0; i < 11; ++i) { // f2=0x50c|58c|5ac f3=f5=9
    long val=input->readLong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  MWAWEntry entry;
  entry.setBegin(input->readLong(4));
  entry.setLength(input->readLong(4));
  entry.setId((int) input->readLong(2)); // in fact nFonts
  entry.setType("FontNames");
  f << "fontNames[ptr]=" << std::hex << entry.begin() << "<->" << entry.end()
    << std::dec << ",nFonts=" << entry.id() << ",";
  if (entry.length() && (!entry.valid() || !input->checkPosition(entry.end()))) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::checkHeader: can not read the font names position\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }

  m_state->m_typeEntryMap.insert
  (std::multimap<std::string, MWAWEntry>::value_type(entry.type(),entry));
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  if (strict && !readPrintInfo())
    return false;
  ascii().addPos(66);
  ascii().addNote("_");

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
