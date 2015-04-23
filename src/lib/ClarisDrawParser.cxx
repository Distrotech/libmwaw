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

#include "ClarisDrawStyleManager.hxx"
#include "ClarisDrawGraph.hxx"
#include "ClarisDrawText.hxx"

#include "ClarisDrawParser.hxx"

/** Internal: the structures of a ClarisDrawParser */
namespace ClarisDrawParserInternal
{
////////////////////////////////////////////
//! Internal: the state of a ClarisDrawParser
struct State {
  //! constructor
  State() : m_version(0), m_isLibrary(false), m_numDSET(0), m_pageSpanSet(false), m_headerId(0), m_footerId(0), m_pages(1,1)
  {
  }
  //! the file version
  int m_version;
  //! flag to know if the file is a library or not
  bool m_isLibrary;
  //! the number of DSET+FNTM
  int m_numDSET;
  //! flag to know if the page has been set
  bool m_pageSpanSet;
  //! header id
  int m_headerId;
  //! footer id
  int m_footerId;
  //! the number of pages
  MWAWVec2i m_pages;
};

////////////////////////////////////////
//! Internal: the subdocument of a ClarisDrawParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(ClarisDrawParser &pars, MWAWInputStreamPtr input, int zoneId) : MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

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
    MWAW_DEBUG_MSG(("ClarisDrawParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  ClarisDrawParser *parser=dynamic_cast<ClarisDrawParser *>(m_parser);
  if (!parser) {
    MWAW_DEBUG_MSG(("ClarisDrawParserInternal::SubDocument::parse: no parser\n"));
    return;
  }
  long pos = m_input->tell();
  // TODO
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
ClarisDrawParser::ClarisDrawParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWGraphicParser(input, rsrcParser, header), m_state(), m_styleManager(), m_graphParser(), m_textParser()
{
  init();
}

ClarisDrawParser::~ClarisDrawParser()
{
}

void ClarisDrawParser::init()
{
  resetGraphicListener();
  setAsciiName("main-1");
  m_state.reset(new ClarisDrawParserInternal::State);
  m_styleManager.reset(new ClarisDrawStyleManager(*this));

  m_graphParser.reset(new ClarisDrawGraph(*this));
  m_textParser.reset(new ClarisDrawText(*this));

  getPageSpan().setMargins(0.1);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void ClarisDrawParser::parse(librevenge::RVNGDrawingInterface *docInterface)
{
  if (!getInput().get() || !checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = false;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());
    checkHeader(0L);
    ok = createZones() && false;
    if (ok) {
      createDocument(docInterface);
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetGraphicListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void ClarisDrawParser::createDocument(librevenge::RVNGDrawingInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getGraphicListener()) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::createDocument: listener already exist\n"));
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
bool ClarisDrawParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  if ((m_state->m_isLibrary && !readLibraryHeader()) ||
      (!m_state->m_isLibrary && !readDocHeader()))
    return false;

  while (readZone());
  if (!input->isEnd()) {
    ascii().addPos(input->tell());
    ascii().addNote("Entries(BAD)");
  }
  long pos;
  while (!input->isEnd()) {
    pos=input->tell();
    if (input->readULong(4)!=0x44534554) {
      input->seek(pos+1, librevenge::RVNG_SEEK_SET);
      continue;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    long actPos=input->tell();
    while (readZone())
      actPos=input->tell();
    if (actPos==pos) {
      input->seek(actPos+4, librevenge::RVNG_SEEK_SET);
      continue;
    }
    input->seek(actPos, librevenge::RVNG_SEEK_SET);
    if (input->isEnd()) break;
    MWAW_DEBUG_MSG(("ClarisDrawParser::createZones: oops loose tracks of zones\n"));
    ascii().addPos(input->tell());
    ascii().addNote("Entries(BAD):###");
  }
  return false;
}

bool ClarisDrawParser::readZone()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+4))
    return false;
  long fSz=(long) input->readULong(4);
  std::string what("Unknown");
  if (fSz==0x44534554) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return readDSET();
  }
  else if (fSz==0x464e544d) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return m_styleManager->readFontNames();
  }
  else if ((fSz>>16)>16) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  else if ((fSz>>16)!=0) {
    ascii().addPos(pos);
    ascii().addNote("Entries(Short)");
    input->seek(pos+2, librevenge::RVNG_SEEK_SET);
    return true;
  }
  long endPos=input->tell()+fSz;
  if (fSz<0 || ((fSz&1) && (fSz==0x4453 || fSz==0x464e)) || !input->checkPosition(endPos)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  libmwaw::DebugStream f;
  if (fSz==0)
    f << "_";
  else
    f << "Entries(" << what << ")";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

bool ClarisDrawParser::readDocHeader()
{
  MWAWInputStreamPtr input = getInput();
  if (!input->checkPosition(0x80A)) return false;

  input->seek(8, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(DocHeader):";
  int val = (int) input->readLong(2); // always find 1
  if (val != 1)
    f << "#unkn=" << std::hex << val << std::dec << ",";
  for (int i = 0; i < 4; i++) {
    val = (int) input->readULong(2);
    if (val)
      f << std::hex << "f" << i << "="  << std::hex << val << std::dec << ",";
  }
  int dim[2];
  for (int i = 0; i < 2; i++)
    dim[i] = (int) input->readLong(2);
  f << "dim?=" << dim[1] << "x" << dim[0] << ",";
  int margin[6];
  f << "margin?=[";
  for (int i = 0; i < 6; i++) {
    margin[i] = (int) input->readLong(2);
    f << margin[i] << ",";
  }
  f << "],";
  if (dim[0] > 0 && dim[1] > 0 &&
      margin[0] >= 0 && margin[1] >= 0 && margin[2] >= 0 && margin[3] >= 0 &&
      dim[0] > margin[0]+margin[2] && dim[1] > margin[1]+margin[3]) {

    MWAWVec2i paperSize(dim[1],dim[0]);
    MWAWVec2i lTopMargin(margin[1], margin[0]);
    MWAWVec2i rBotMargin(margin[3], margin[2]);

    getPageSpan().setMarginTop(lTopMargin.y()/72.0);
    getPageSpan().setMarginBottom(rBotMargin.y()/72.0);
    getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
    getPageSpan().setMarginRight(rBotMargin.x()/72.0);
    getPageSpan().setFormLength(paperSize.y()/72.);
    getPageSpan().setFormWidth(paperSize.x()/72.);
    m_state->m_pageSpanSet = true;
  }
  int dim2[2];
  for (int i = 0; i < 2; i++)
    dim2[i] = (int) input->readLong(2);
  f << "dim2?=" << dim2[1] << "x" << dim2[0] << ",";
  int fl[4];
  f << "fl?=[";
  for (int i = 0; i < 4; i++) {
    fl[i] = (int) input->readULong(1);
    if (fl[i])
      f << fl[i] << ",";
    else
      f << "_,";
  }
  f << "],";
  for (int i = 0; i < 9; i++) {
    val = (int) input->readLong(2);
    if (val)
      f << "g" << i << "="  << val << ",";
  }

  ascii().addDelimiter(input->tell(), '|');
  ascii().addPos(8);
  ascii().addNote(f.str().c_str());

  input->seek(8+132, librevenge::RVNG_SEEK_SET);

  /* zone 1 actual font, actul pos, .. */
  if (!m_textParser->readParagraph())
    return false;
  long pos = input->tell();
  f.str("");
  f << "DocHeader:zone?=" << input->readULong(2) << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  MWAWFont font;
  int posChar;
  if (!m_textParser->readFont(-1, posChar, font))
    return false;

  /* zone 2, type, unknown */
  pos = input->tell();
  f.str("");
  f << "DocHeader-1:";
  for (int i = 0; i < 6; i++) {
    val = (int) input->readULong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addDelimiter(input->tell(), '|');
  input->seek(pos+20, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // the document font ?
  if (!m_textParser->readFont(-1, posChar, font))
    return false;

  pos=input->tell();
  input->seek(pos+124, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote("DocHeader-2");

  pos=input->tell();
  f.str("");
  f << "DocHeader(Col):";
  int numCols = (int) input->readLong(2);
  if (numCols < 1 || numCols > 9) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: pb reading number of columns\n"));
    f << "###numCols=" << numCols;
    numCols = 1;
  }
  if (numCols != 1)
    f << "numCols=" << numCols << ",";
  f << "colsW=[";
  for (int i = 0; i < numCols; i++) {
    val = (int) input->readULong(2);
    f << val << ",";
  }
  f << "],";
  input->seek(pos+20, librevenge::RVNG_SEEK_SET);
  if (numCols > 1) {
    f << "colsS=[";
    for (int i = 0; i < numCols-1; i++) {
      val = (int) input->readULong(2);
      f << input->readULong(2) << ",";
    }
    f << "],";
  }
  input->seek(pos+36, librevenge::RVNG_SEEK_SET);
  val = (int) input->readLong(2);
  if (val) f << "unkn=" << val << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  input->seek(pos+214, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote("DocHeader-3");

  pos=input->tell(); // only 0?
  input->seek(pos+256, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote("DocHeader-4");

  pos=input->tell();
  input->seek(pos+190, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote("DocHeader-5");

  pos=input->tell(); // only 0?
  input->seek(pos+256, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote("DocHeader-6");

  pos=input->tell();
  input->seek(pos+256+96, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote("DocHeader-7");

  pos=input->tell();
  input->seek(pos+150, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote("DocHeader-8");

  pos=input->tell();
  input->seek(pos+130, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote("DocHeader-9");

  pos=input->tell();
  ascii().addPos(pos);
  ascii().addNote("DocHeader-A");

  input->seek(0x80A, librevenge::RVNG_SEEK_SET);
  // now 25 zones
  for (int i=0; i<25; ++i) {
    if (input->isEnd())
      return false;
    pos=input->tell();
    long fSz=(long) input->readULong(4);
    if (!fSz) {
      ascii().addPos(pos);
      ascii().addNote("_");
      continue;
    }
    if (!input->checkPosition(pos+4+fSz)) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: can not find zone %d\n", i+1));
      ascii().addPos(pos);
      ascii().addNote("###");
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return true;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    bool done=false;
    switch (i+1) {
    case 1:
      done=m_textParser->readParagraphs();
      break;
    case 2:
      done=readPrintInfo();
      break;
    case 3:
      done=m_styleManager->readPatternList();
      break;
    case 4:
      done=m_styleManager->readGradientList();
      break;
    case 6:
      done=m_styleManager->readFontStyles();
      break;
    case 8: // never seens data: fieldData=4, headerSize=14 (always 1,5,0,100,101,0,101,0,1,12710?)
      done = ClarisWksStruct::readStructZone(*getParserState(), "Zone8A", false);
      break;
    case 9: // never seens data: fieldData=12, headerSize=4 (1,id)
      done = ClarisWksStruct::readStructZone(*getParserState(), "Zone9A", false);
      break;
    case 10: // never seens data: fieldData=4, headerSize=4 (1,id)
      done = ClarisWksStruct::readStructZone(*getParserState(), "Zone10A", false);
      break;
    // CHECKME
    case 16: // Arrow
      done=m_styleManager->readArrows();
      break;
    case 17: // Dash
      done=m_styleManager->readDashs();
      break;
    case 18: // Ruler style
      done=m_styleManager->readRulers();
      break;
    case 20: // fieldData=1e, link to transformation ?
      done = ClarisWksStruct::readStructZone(*getParserState(), "Zone20A", false);
      break;
    case 21: // fieldData=36
      done = ClarisWksStruct::readStructZone(*getParserState(), "Zone21A", false);
      break;
    case 22: // fieldData=4, headerSize=4 (1, consecutive id)
      done = ClarisWksStruct::readStructZone(*getParserState(), "Zone22A", false);
      break;
    default:
      break;
    }
    if (done) continue;
    f.str("");
    f << "Entries(Zone" << i+1 << "A):";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+4+fSz, librevenge::RVNG_SEEK_SET);
  }

  pos=input->tell();
  if (!readDocInfo()) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return true;
  }
  pos=input->tell();
  if (!ClarisWksStruct::readStructZone(*getParserState(), "Layout", false)) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: can not find layout zone\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return true;
  }
  pos=input->tell();
  input->seek(pos+4, librevenge::RVNG_SEEK_SET);
  if (input->readULong(2)!=0x100) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: can not find UnknZone zone\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return true;
  }
  ascii().addPos(pos);
  ascii().addNote("Entries(UnknZone0)");
  input->seek(pos+832, librevenge::RVNG_SEEK_SET);

  // now 12 zones
  for (int i=0; i<12; ++i) {
    if (input->isEnd())
      return false;
    pos=input->tell();
    long fSz=(long) input->readULong(4);
    if (!fSz) {
      ascii().addPos(pos);
      ascii().addNote("_");
      continue;
    }
    if (!input->checkPosition(pos+4+fSz)) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: can not find zone %d\n", i+1));
      ascii().addPos(pos);
      ascii().addNote("###");
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return true;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    bool done=false;
    switch (i+1) {
    case 1: // never seens data: fieldData=4, headerSize=4 (1,id)
      done = ClarisWksStruct::readStructZone(*getParserState(), "ZoneB1A", false);
      break;
    case 3: // never seens data: fieldData=6, headerSize=0
      done = ClarisWksStruct::readStructZone(*getParserState(), "ZoneB3A", false);
      break;
    case 4:// never seens data: fieldData=14, headerSize=0
      done = ClarisWksStruct::readStructZone(*getParserState(), "ZoneB4A", false);
      break;
    case 5:// never seens data: fieldData=14, headerSize=0
      done = ClarisWksStruct::readStructZone(*getParserState(), "ZoneB5A", false);
      break;
    case 6:// never seens data: fieldData=8, headerSize=80
      done = ClarisWksStruct::readStructZone(*getParserState(), "ZoneB6A", false);
      break;
    case 7:// never seens data: fieldData=28, headerSize=80
      done = ClarisWksStruct::readStructZone(*getParserState(), "ZoneB7A", false);
      break;
    case 8:// never seens data: fieldData=1e, headerSize=0
      done = ClarisWksStruct::readStructZone(*getParserState(), "ZoneB8A", false);
      break;
    case 9:// TODO: fieldData=1c (2*double+), headerSize=0
      done = ClarisWksStruct::readStructZone(*getParserState(), "ZoneB9A", false);
      break;
    case 10:
      done = m_styleManager->readColorList();
      break;
    case 11:// never seens data: fieldData=c, headerSize=0
      done = ClarisWksStruct::readStructZone(*getParserState(), "ZoneB11A", false);
      break;
    case 12:// never seens data: fieldData=32, headerSize=0
      done = ClarisWksStruct::readStructZone(*getParserState(), "ZoneB12A", false);
      break;
    }
    if (done) continue;
    f.str("");
    f << "Entries(ZoneB" << i+1 << "A):";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+4+fSz, librevenge::RVNG_SEEK_SET);
  }
  pos=input->tell();
  f.str("");
  f << "Entries(NDSET):";
  m_state->m_numDSET=(int) input->readLong(2);
  if (m_state->m_numDSET) f << "num=" << m_state->m_numDSET << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return true;
}

bool ClarisDrawParser::readLibraryHeader()
{
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  f << "Entries(LibHeader):";
  if (!input->checkPosition(846)) return false;
  input->seek(8, librevenge::RVNG_SEEK_SET);
  long val=input->readLong(4); // always 0
  if (val) f << "f0=" << val << ",";
  f << "ID=" << std::hex << input->readULong(4) << std::dec << ","; // a big number
  val=input->readLong(2); // always 0x100
  if (val!=0x100) f << "f1=" << val << ",";
  for (int i=0; i<5; ++i) { // always 0
    val=input->readLong(2);
    if (val) f << "f" << i+2 << "=" << val << ",";
  }
  int N=(int) input->readLong(2); // checkme: the number of graphics ?
  if (N) f << "N=" << N << ",";
  for (long i=0; i<2; ++i) { // always 0,1
    val=input->readLong(2);
    if (val!=i) f << "f" << i+7 << "=" << val << ",";
  }
  f << "ID1=[";
  for (int i=0; i<2; ++i) { // two big number
    f << std::hex << input->readULong(4) << std::dec << ",";
  }
  f << "],";
  val=input->readLong(2); // always 0x100
  if (val!=0x100) f << "f9=" << val << ",";
  val=input->readLong(2); // always 0
  if (val) f << "f10=" << val << ",";
  f << "ID2=" << std::hex << input->readULong(4) << std::dec << ","; // another big number
  for (int i=0; i<56; ++i) { // always 0
    val=input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  ascii().addPos(8);
  ascii().addNote(f.str().c_str());

  long pos=input->tell();
  f.str("");
  f << "LibHeader-A:";
  for (int i=0; i<2; ++i) { // f1=54,5c|5d
    val=input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  f << "IDs=[";
  for (int i=0; i<9; ++i) { // 7 or 9 big number
    val=(long) input->readULong(4);
    if (val)
      f << std::hex << val << std::dec << ",";
    else
      f << "_,";
  }
  f << "],";
  for (int i=0; i<6; ++i) { // always 0
    val=input->readLong(2);
    if (val) f << "f" << i+2 << "=" << val << ",";
  }
  for (int i=0; i<2; ++i) {
    f << "unkn" << i << "=[";
    for (int j=0; j<2; ++j) { // _, small number
      val=input->readLong(2);
      if (val)
        f << val << ",";
      else
        f << "_,";
    }
    f << std::hex << input->readULong(4) << std::dec << ","; // big number or 0
    f << "],";
  }
  for (int i=0; i<26; ++i) { // always 0
    val=input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "LibHeader-B:";
  int fSz=(int) input->readULong(1);
  if (fSz>63) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::readLibraryHeader: string size seems bad\n"));
    f << "##fSz=" << fSz << ",";
    fSz=0;
  }
  std::string text("");
  for (int i=0; i<fSz; ++i) text+=(char) input->readULong(1);
  f << "filename=" << text << ",";
  input->seek(pos+64, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<96; ++i) { // always 0
    val=input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  val=(int) input->readULong(1); // always filename size ?
  if (val!=fSz) f << "#fSz1=" << fSz << ",";
  val=(int) input->readULong(1); // always 0?
  if (val) f << "f96=" << val << ",";
  f << "ID=" << std::hex << input->readULong(4) << std::dec << ","; // big number
  val=(int) input->readLong(2); // 0 | 0fcc
  if (val) f << "g0=" << val << ",";
  val=(int) input->readULong(2); //  80[89]3
  if (val) f << "fl=" << std::hex << val << std::dec << ",";
  val=(int) input->readLong(2); // 0
  if (val) f << "g1=" << val << ",";
  val=(int) input->readULong(1); // always filename size ?
  if (val!=fSz) f << "#fSz2=" << fSz << ",";
  val=(int) input->readULong(1); // always 0?
  if (val) f << "g2=" << val << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "LibHeader-C:";
  fSz=(int) input->readULong(1);
  if (fSz>63) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::readLibraryHeader: string size seems bad\n"));
    f << "##fSz=" << fSz << ",";
    fSz=0;
  }
  text="";
  for (int i=0; i<fSz; ++i) text+=(char) input->readULong(1);
  f << "author?=" << text << ",";
  input->seek(pos+64, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<96; ++i) { // always 0
    val=input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "LibHeader-D:";
  for (int i=0; i<16; ++i) {
    val=(int) input->readLong(2);
    static int const expected[]= {4,2,0/*35 or 3f*/,48, 9, 0/*78 or 8x*/, 0x42, 18,
                                  3, 0xd4, 0/*7b or 8e*/, 0x57, 3, 0xc4, 0/*7b or 8f*/, 0
                                 };
    if (val!=expected[i])
      f << "f" << i << "=" << val << ",";
  }
  int dim[2];
  for (int i=0; i<2; ++i) dim[i]=(int) input->readLong(2);
  f << "dim=" << MWAWVec2i(dim[0],dim[1]) << ","; // 64x64 or 72x72
  m_state->m_numDSET=(int) input->readLong(2);
  if (m_state->m_numDSET) f << "num[DSET]=" << m_state->m_numDSET << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(846, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool ClarisDrawParser::checkHeader(MWAWHeader *header, bool /*strict*/)
{
  *m_state = ClarisDrawParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->checkPosition(846))
    return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";
  input->seek(0, librevenge::RVNG_SEEK_SET);
  int val=(int) input->readULong(1);
  if (val==0x2) {
    m_state->m_isLibrary=true;
    f << "lib,";
  }
  else if (val==0x1) {
    if (!input->checkPosition(0x80A))
      return false;
  }
  else
    return false;
  val=(int) input->readULong(2); // a3-b1
  if (val) f << "f0=" << std::hex << val << std::dec << ",";
  if (input->readULong(1) || input->readULong(4) != 0x45585057) return false;
  int vers=1;
  setVersion(vers);
  m_state->m_version=vers;
  if (header)
    header->reset(MWAWDocument::MWAW_T_RESERVED1, vers, MWAWDocument::MWAW_K_DRAW);
  input->seek(8,librevenge::RVNG_SEEK_SET);
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// read the document information
////////////////////////////////////////////////////////////
bool ClarisDrawParser::readDocInfo()
{
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  f << "Entries(DocInfo):";
  long pos = input->tell();
  long endPos=pos+428;
  if (!input->checkPosition(endPos)) return false;
  f << "ptr=" << std::hex << input->readULong(4) << std::dec << ",";
  int val;
  for (int i = 0; i < 6; i++) {
    val = (int) input->readULong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  m_state->m_headerId = (int) input->readLong(2);
  if (m_state->m_headerId) f << "headerId=" << m_state->m_headerId << ",";
  val = (int) input->readLong(2);
  if (val) f << "unkn=" << val << ",";
  m_state->m_footerId = (int) input->readLong(2);
  if (m_state->m_footerId) f << "footerId=" << m_state->m_footerId << ",";
  for (int i=0; i < 4; ++i) {
    val = (int) input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  int pages[2];
  for (int i=0; i < 2; ++i)
    pages[i]=(int) input->readLong(2);
  if (pages[1]>=1 && pages[1] < 1000 && pages[0]>=1 && pages[0]<100)
    m_state->m_pages=MWAWVec2i(pages[0],pages[1]);
  if (pages[0]!=1 || pages[1]!=1)
    f << "pages[num]=" << pages[0] << "x" << pages[1] << ",";
  ascii().addDelimiter(input->tell(), '|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  ascii().addPos(pos+100);
  ascii().addNote("DocInfo-2");
  ascii().addPos(pos+200);
  ascii().addNote("DocInfo-3");
  ascii().addPos(pos+300);
  ascii().addNote("DocInfo-4");
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// try to read the print info zone
////////////////////////////////////////////////////////////
bool ClarisDrawParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  long endPos=pos+124;
  if (input->readULong(4)!=120 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::readPrintInfo: file seems too short\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(PrintInfo):";
  libmwaw::PrinterInfo info;
  if (!info.read(input)) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::readPrintInfo: can not read print info\n"));
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
  getPageSpan().setFormLength(paperSize.y()/72.);
  getPageSpan().setFormWidth(paperSize.x()/72.);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read the document main part
////////////////////////////////////////////////////////////
shared_ptr<ClarisWksStruct::DSET> ClarisDrawParser::readDSET()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=ascii();
  if (input->readULong(4) != 0x44534554L)
    return shared_ptr<ClarisWksStruct::DSET>();
  long sz = (long) input->readULong(4);
  MWAWEntry entry;
  entry.setBegin(pos);
  entry.setLength(sz+8);

  long endPos = entry.end();
  if (sz<16 || !input->checkPosition(entry.end())) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::readDSET: file is too short\n"));
    return shared_ptr<ClarisWksStruct::DSET>();
  }

  ClarisWksStruct::DSET dset;
  dset.m_size = sz;
  dset.m_numData = (int) input->readULong(2);

  input->seek(10, librevenge::RVNG_SEEK_CUR);
  dset.m_fileType = (int) input->readULong(1);
  input->seek(-11, librevenge::RVNG_SEEK_CUR);
  int nFlags = 0;
  switch (dset.m_fileType) {
  case 1: // text
    dset.m_beginSelection = (int) input->readLong(4);
    dset.m_endSelection = (int) input->readLong(4);
    dset.m_textType = (int) input->readULong(1);
    dset.m_flags[nFlags++] = (int) input->readLong(1);
    dset.m_headerSz = 44;
    dset.m_dataSz = 28;
    break;
  default:
    dset.m_flags[nFlags++] = (int) input->readLong(2); // normally -1
    dset.m_flags[nFlags++] = (int) input->readLong(2); // the 0
    dset.m_dataSz = (int) input->readULong(2);
    dset.m_headerSz = (int) input->readULong(2);
    dset.m_flags[nFlags++] = (int) input->readLong(2);
    break;
  }
  dset.m_flags[nFlags++] = (int) input->readLong(2);
  dset.m_id = (int) input->readULong(2) ;
  switch (dset.m_fileType)  {
  case 0:
    return m_graphParser->readGroupZone(dset, entry);
  case 1:
    return m_textParser->readDSETZone(dset, entry);
  case 4:
    return m_graphParser->readBitmapZone(dset, entry);
  default:
    break;
  }
  MWAW_DEBUG_MSG(("ClarisDrawParser::readDSET: find unexpected type\n"));
  shared_ptr<ClarisWksStruct::DSET> zone(new ClarisWksStruct::DSET(dset));
  f << "Entries(DSETU): " << *zone;

  int data0Length = (int) zone->m_dataSz;
  int N = (int) zone->m_numData;

  ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  if (sz-12 != data0Length*N + zone->m_headerSz) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::readDSET: unexpected size for zone definition, try to continue\n"));
    ascFile.addPos(pos);
    ascFile.addNote("###");
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return zone;
  }

  long debPos = endPos-N*data0Length;
  for (int i = 0; i < zone->m_numData; i++) {
    input->seek(debPos, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << "DSETU-" << i << ":";

    long actPos = input->tell();
    if (actPos != debPos && actPos != debPos+data0Length)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(debPos);
    ascFile.addNote(f.str().c_str());
    debPos += data0Length;
  }

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return zone;
}

////////////////////////////////////////////////////////////
//
// send data
//
////////////////////////////////////////////////////////////


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
