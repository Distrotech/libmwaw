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
#include "MWAWSubDocument.hxx"

#include "ClarisDrawStyleManager.hxx"
#include "ClarisDrawGraph.hxx"
#include "ClarisDrawText.hxx"

#include "ClarisDrawParser.hxx"

/** Internal: the structures of a ClarisDrawParser */
namespace ClarisDrawParserInternal
{
// generic class used to defined a layer
struct Layer {
  //! constructor
  Layer() : m_groupId(0), m_isHidden(false), m_name("")
  {
  }
  //! the first group id
  int m_groupId;
  //! true if the layer is hidden
  bool m_isHidden;
  //! the layer name
  librevenge::RVNGString m_name;
};

////////////////////////////////////////////
//! Internal: the state of a ClarisDrawParser
struct State {
  //! constructor
  State() : m_version(0), m_isLibrary(false), m_numDSET(0), m_EOF(-1),
    m_actualLayer(1), m_numLayers(1),  m_createMasterPage(false), m_displayAsSlide(false), m_layerList(),
    m_pageSpanSet(false), m_headerId(0), m_footerId(0), m_pages(1,1), m_zonesMap(), m_zoneIdToFileTypeMap()
  {
  }
  //! the file version
  int m_version;
  //! flag to know if the file is a library or not
  bool m_isLibrary;
  //! the number of DSET+FNTM
  int m_numDSET;
  //! the last data zone size
  long m_EOF;
  //! the actual layer
  int m_actualLayer;
  //! the number of layer
  int m_numLayers;
  //! flag to know if we need or not to create a master
  bool m_createMasterPage;
  //! flag to know if we need to display data as slide
  bool m_displayAsSlide;
  //! the layer list
  std::vector<Layer> m_layerList;
  //! flag to know if the page has been set
  bool m_pageSpanSet;
  //! header id
  int m_headerId;
  //! footer id
  int m_footerId;
  //! the number of pages
  MWAWVec2i m_pages;
  /** the map of zone*/
  std::map<int, shared_ptr<ClarisWksStruct::DSET> > m_zonesMap;
  /** map zone id to file type */
  std::map<int, int> m_zoneIdToFileTypeMap;
};
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

int ClarisDrawParser::getFileType(int zoneId) const
{
  if (m_state->m_zoneIdToFileTypeMap.find(zoneId) == m_state->m_zoneIdToFileTypeMap.end()) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::getFileType: zone %d does not exists!!!!\n", zoneId));
    return -1;
  }
  return m_state->m_zoneIdToFileTypeMap.find(zoneId)->second;
}

bool ClarisDrawParser::sendTextZone(int number, int subZone)
{
  return m_textParser->sendZone(number, subZone);
}

MWAWVec2f ClarisDrawParser::getPageLeftTop()
{
  return MWAWVec2f(float(getParserState()->m_pageSpan.getMarginLeft()),
                   float(getParserState()->m_pageSpan.getMarginTop()));
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
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      MWAWVec2f leftTop=72.0f*getPageLeftTop();
      MWAWPosition pos(leftTop,MWAWVec2f(0,0),librevenge::RVNG_POINT);
      pos.setRelativePosition(MWAWPosition::Page);
      MWAWGraphicListenerPtr listener=getGraphicListener();

      if (m_state->m_isLibrary) {
        for (int i=0; i<(int) m_state->m_layerList.size(); ++i) {
          if (i>0)
            listener->insertBreak(MWAWListener::PageBreak);
          m_graphParser->sendMainGroupChild(i, pos);
        }
      }
      else if (m_state->m_displayAsSlide) {
        bool first=true;
        for (size_t i=1; i< m_state->m_layerList.size(); ++i) {
          if (m_graphParser->isEmptyGroup(m_state->m_layerList[i].m_groupId))
            continue;
          if (!first)
            listener->insertBreak(MWAWListener::PageBreak);
          first=false;
          m_graphParser->sendGroup(m_state->m_layerList[i].m_groupId, pos);
        }
      }
      else {
        for (size_t i=1; i< m_state->m_layerList.size(); ++i) {
          if (m_graphParser->isEmptyGroup(m_state->m_layerList[i].m_groupId))
            continue;
          bool openLayer=false;
          if (!m_state->m_layerList[i].m_name.empty())
            openLayer=listener->openLayer(m_state->m_layerList[i].m_name);
          m_graphParser->sendGroup(m_state->m_layerList[i].m_groupId, pos);
          if (openLayer)
            listener->closeLayer();
        }
      }
#ifdef DEBUG
      m_graphParser->flushExtra();
#endif
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

  m_graphParser->updateGroup(m_state->m_isLibrary);


  // create the page list
  int numPages=1;
  MWAWPageSpan ps(getPageSpan());
  ps.setPageSpan(1);
  std::vector<librevenge::RVNGString> listNames;
  if (m_state->m_isLibrary) {
    numPages=(int) m_state->m_layerList.size();
    if (numPages<1) numPages=1;
  }
  else {
    m_state->m_createMasterPage=!m_state->m_layerList.empty() &&
                                !m_graphParser->isEmptyGroup(m_state->m_layerList[0].m_groupId);
    if (m_state->m_createMasterPage)
      ps.setMasterPageName("Master");
    for (size_t i=1; i<m_state->m_layerList.size(); ++i) {
      if (!m_graphParser->isEmptyGroup(m_state->m_layerList[i].m_groupId))
        listNames.push_back(m_state->m_layerList[i].m_name);
    }
    if (m_state->m_displayAsSlide && !listNames.empty())
      numPages=(int) listNames.size();
  }
  std::vector<MWAWPageSpan> pageList;
  for (int i=0; i<numPages; ++i) {
    MWAWPageSpan page(ps);
    if (m_state->m_isLibrary && i<(int) m_state->m_layerList.size())
      page.setPageName(m_state->m_layerList[size_t(i)].m_name);
    else if (m_state->m_displayAsSlide && i<(int) listNames.size())
      page.setPageName(listNames[size_t(i)]);
    pageList.push_back(page);
  }
  MWAWGraphicListenerPtr listen(new MWAWGraphicListener(*getParserState(), pageList, documentInterface));
  setGraphicListener(listen);
  listen->startDocument();


  if (!m_state->m_createMasterPage)
    return;

  // we need to send the master page
  if (!listen->openMasterPage(ps)) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::createDocument: can not create the master page\n"));
    m_state->m_createMasterPage=false;
    return;
  }
  MWAWVec2f leftTop=72.0f*getPageLeftTop();
  MWAWPosition pos(leftTop,MWAWVec2f(0,0),librevenge::RVNG_POINT);
  pos.setRelativePosition(MWAWPosition::Page);
  m_graphParser->sendGroup(m_state->m_layerList[0].m_groupId, pos);
  listen->closeMasterPage();
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

  if (m_state->m_EOF>0)
    input->pushLimit(m_state->m_EOF);
  while (readZone());
  if (!input->isEnd()) {
    ascii().addPos(input->tell());
    ascii().addNote("Entries(BAD)");
  }
  while (!input->isEnd()) {
    long pos=input->tell();
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
  if (m_state->m_EOF>0)
    input->popLimit();
  return true;
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
    return readDSET().get()!=0;
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
    if (margin[i])
      f << margin[i] << ",";
    else
      f << "_,";
  }
  f << "],";
  // seems a good indication that slide mode is choosen
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
  f.str("");
  f << "DocHeader-2:";
  for (int i=0; i<5; ++i) {
    val=(int) input->readULong(2);
    static int const(expected[])= {0,1,2,2,0};
    if (val!=expected[i])
      f << "f" << i << "=" << val << ",";
  }
  for (int i=0; i<8; ++i) {
    val=(int) input->readULong(1);
    static int const(expected[])= {1/*or 16*/,0,0,0,0,1,1, 0};
    if (val!=expected[i])
      f << "f" << i+5 << "=" << val << ",";
  }
  for (int i=0; i<4; ++i) {
    val=(int) input->readULong(2);
    static int const(expected[])= {1,1,0x2d,0};
    if (val!=expected[i])
      f << "f" << i+13 << "=" << val << ",";
  }
  f << "fl=[";
  for (int i=0; i<13; ++i) {
    val=(int) input->readULong(1);
    if (val==1) f << "*,";
    else if (val) f << val << ",";
    else f << "_,";
  }
  f << "],";
  for (int i=0; i<16; ++i) { // always 0
    val=(int) input->readULong(2);
    if (val)
      f << "g" << i << "=" << val << ",";
  }
  for (int i=0; i<5; ++i) {
    val=(int) input->readULong(2);
    static int const(expected[])= {0,1,0,0x600,0x504};
    if (val!=expected[i])
      f << "h" << i << "=" << std::hex << val << std::dec << ",";
  }
  val=(int) input->readULong(1); // always 0
  if (val) f << "h5=" << val << ",";
  int num[3];
  for (int i=0; i<3; ++i) {
    num[i]=(int) input->readULong(2);
    if (!num[i])
      continue;
    static char const *(wh[])= {"color", "unkn1", "gradient"};
    f << "num[" << wh[i] << "]=" << num[i] << ",";
  }
  m_styleManager->setDefaultNumbers(num[0], num[2]);

  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+124, librevenge::RVNG_SEEK_SET);

  pos=input->tell();
  f.str("");
  f << "DocHeader(Col):";
  int numCols = (int) input->readLong(2);
  if (numCols < 1 || numCols > 9) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::readDocHeader: pb reading number of columns\n"));
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
      f << input->readULong(2);
      if (val) f << ":" << val << ",";
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
  f.str("");
  f << "DocHeader-8:";
  input->seek(pos+111, librevenge::RVNG_SEEK_SET);
  val=(int) input->readLong(1);
  if (val==1) {
    f << "slide,";
    m_state->m_displayAsSlide=true;
  }
  else if (val) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::readDocHeader: pb reading the viewer style\n"));
    f << "###slide=" << val << ",";
  }
  input->seek(pos+150, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

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
      MWAW_DEBUG_MSG(("ClarisDrawParser::readDocHeader: can not find zone %d\n", i+1));
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
    case 20:
      done = m_graphParser->readTransformations();
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
  if (!readLayouts()) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::readDocHeader: can not find layout zone\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return true;
  }
  pos=input->tell();
  input->seek(pos+4, librevenge::RVNG_SEEK_SET);
  if (input->readULong(2)!=0x100) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::readDocHeader: can not find UnknZone zone\n"));
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
      MWAW_DEBUG_MSG(("ClarisDrawParser::readDocHeader: can not find zone %d\n", i+1));
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
    default:
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
  // the style are coded after the DSET zones, let try to find them
  input->seek(-44, librevenge::RVNG_SEEK_END);
  while (input->tell()>=846) {
    pos=input->tell();
    int c=(int) input->readULong(1);
    bool find=false;
    switch (c) {
    case 0x44:
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      if (readDSET(pos==846))
        find=true;
      else
        input->seek(pos-4, librevenge::RVNG_SEEK_SET);
      break;
    case 0x53:
      input->seek(pos-1, librevenge::RVNG_SEEK_SET);
      break;
    case 0x45:
      input->seek(pos-2, librevenge::RVNG_SEEK_SET);
      break;
    case 0x54:
      input->seek(pos-3, librevenge::RVNG_SEEK_SET);
      break;
    default:
      input->seek(pos-4, librevenge::RVNG_SEEK_SET);
      break;
    }
    if (!find)
      continue;
    m_state->m_EOF=input->tell();
    // clean storage
    m_graphParser->resetState();
    m_textParser->resetState();
    m_state->m_zonesMap.clear();
    m_state->m_zoneIdToFileTypeMap.clear();
    while (!input->isEnd()) {
      pos=input->tell();
      if (input->readULong(4)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
      ascii().addPos(pos);
      ascii().addNote("_");
    }
    pos=input->tell();
    bool ok=true;
    if (!m_textParser->readParagraphs()) {
      MWAW_DEBUG_MSG(("ClarisDrawParser::readLibraryHeader: can not read the paragraph style\n"));
      ascii().addPos(pos);
      ascii().addNote("Entries(RULR):###");
      ok=false;
    }
    for (int i=0; ok && i<20; ++i) {
      pos=input->tell();
      long dSz=(long) input->readULong(4);
      if (!dSz) {
        ascii().addPos(pos);
        ascii().addNote("_");
        continue;
      }
      if (!input->checkPosition(pos+4+dSz)) {
        MWAW_DEBUG_MSG(("ClarisDrawParser::readLibraryHeader: can not find style zone %d\n", i+1));
        ascii().addPos(pos);
        ascii().addNote("###");
        break;
      }
      bool done=false;
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      switch (i+1) {
      case 1:
        done=m_styleManager->readArrows();
        break;
      case 2:
        done=m_styleManager->readDashs();
        break;
      case 3:
        done=m_styleManager->readPatternList();
        break;
      case 4:
        done=m_styleManager->readGradientList();
        break;
      case 5:
        done = m_graphParser->readTransformations();
        break;
      case 6:
        done = m_styleManager->readRulers();
        break;
      case 7: // two zones: pos and names
        done = readLibraryNames();
        break;
      case 8:
        done = m_styleManager->readColorList();
        break;
      case 9: // maybe zone 24
        done = ClarisWksStruct::readStructZone(*getParserState(), "Style10A", false);
        break;
      case 10: // maybe zone 25
        done = ClarisWksStruct::readStructZone(*getParserState(), "Style11A", false);
        break;
      case 11: // maybe readfontnames without header
        done = ClarisWksStruct::readStructZone(*getParserState(), "Style12A", false);
        break;
      default:
        break;
      }
      if (done) continue;
      f.str("");
      f << "Entries(Style" << i+1 << "A):";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      input->seek(pos+4+dSz, librevenge::RVNG_SEEK_SET);
    }
    break;
  }
  input->seek(846, librevenge::RVNG_SEEK_SET);
  if (!readDSET(true)) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::readLibraryHeader:can not read main group\n"));
    input->seek(846, librevenge::RVNG_SEEK_SET);
  }
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
    header->reset(MWAWDocument::MWAW_T_CLARISDRAW, vers, MWAWDocument::MWAW_K_DRAW);
  input->seek(8,librevenge::RVNG_SEEK_SET);
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// read the layout
////////////////////////////////////////////////////////////
bool ClarisDrawParser::readLayouts()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+8)) return false;
  libmwaw::DebugFile &ascFile = ascii();
  libmwaw::DebugStream f;
  f << "Entries(Layout):";
  long sz = (long) input->readULong(4);
  if (sz==0) {
    ascFile.addPos(pos);
    ascFile.addNote("_");
    return true;
  }
  long endPos=pos+4+sz;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::readLayouts: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }

  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  int type = (int) input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  int val = (int) input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) input->readULong(2);
  int hSz = (int) input->readULong(2);
  if (!fSz || N*fSz+hSz+12 != sz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisDrawParser::readLayouts: unexpected field/header size\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }

  if (long(input->tell()) != pos+4+12+hSz) {
    ascFile.addDelimiter(input->tell(), '|');
    input->seek(pos+4+12+hSz, librevenge::RVNG_SEEK_SET);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos=input->tell();
  if (fSz!=336) {
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisDrawParser::readLayouts: no sure how to read the data\n"));
    ascFile.addPos(pos);
    ascFile.addNote("Layout:###");
    return true;
  }
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    ClarisDrawParserInternal::Layer layer;
    f.str("");
    f << "Layout-A" << i << ":";
    int cSz=(int) input->readULong(1);
    librevenge::RVNGString name("");
    for (int c=0; c<cSz; ++c) {
      char ch=(char) input->readULong(1);
      if (!ch) continue;
      f << ch;
      int unicode= getParserState()->m_fontConverter->unicode(3, (unsigned char) ch);
      if (unicode==-1)
        name.append(ch);
      else
        libmwaw::appendUnicode((uint32_t) unicode, name);
    }
    f << ",";
    layer.m_name=name;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    input->seek(pos+256, librevenge::RVNG_SEEK_SET);
    pos=input->tell();
    f.str("");
    f << "Layout-B" << i << ":";
    val=(int) input->readULong(1); // always 40|80
    if (val&0x80) {
      f << "hidden,";
      layer.m_isHidden=true;
    }
    val &=0x7F;
    if (val!=0x40) f << "fl0=" << std::hex << val << std::dec << ",";
    val=(int) input->readULong(1); // 0|1|2|6c|d8|fc|ff
    if (val) f << "fl1=" << std::hex << val << std::dec << ",";
    for (int j=0; j<2; ++j) { // always 0
      val=(int) input->readULong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    layer.m_groupId=(int) input->readULong(2);
    f << "id[group]=" << layer.m_groupId << ",";
    val=(int) input->readLong(4);
    if (val==-1)
      f << "locked,";
    else if (val)
      f << "#lock=" << val << ",";
    for (int j=0; j<16; ++j) { // always 0
      val=(int) input->readULong(4);
      if (val) f << "g" << j << "=" << val << ",";
    }
    val=(int) input->readULong(4);
    if (val) f << "ID=" << std::hex << val << std::dec << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+80, librevenge::RVNG_SEEK_SET);
    m_state->m_layerList.push_back(layer);
  }
  return true;
}

bool ClarisDrawParser::readLibraryNames()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+8)) return false;
  libmwaw::DebugFile &ascFile = ascii();
  libmwaw::DebugStream f;
  f << "Entries(LibraryName):";
  long sz = (long) input->readULong(4);
  if (sz==0) {
    ascFile.addPos(pos);
    ascFile.addNote("_");
    pos=input->tell();
    sz = (long) input->readULong(4);
    if (!input->checkPosition(pos+4+sz)) {
      MWAW_DEBUG_MSG(("ClarisDrawParser::readLibraryNames: the name size seems bad\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      return false;
    }
    if (!sz) {
      ascFile.addPos(pos);
      ascFile.addNote("_");
      return true;
    }
    MWAW_DEBUG_MSG(("ClarisDrawParser::readLibraryNames: find a string but no position\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
    return true;
  }

  long endPos=pos+4+sz;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::readLibraryNames: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }

  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  int type = (int) input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  int val = (int) input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) input->readULong(2);
  int hSz = (int) input->readULong(2);
  if (!fSz || N*fSz+hSz+12 != sz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisDrawParser::readLibraryNames: unexpected field/header size\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }

  if (long(input->tell()) != pos+4+12+hSz) {
    ascFile.addDelimiter(input->tell(), '|');
    input->seek(pos+4+12+hSz, librevenge::RVNG_SEEK_SET);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos=input->tell();
  if (fSz!=4) {
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisDrawParser::readLibraryNames: no sure how to read the position\n"));
    ascFile.addPos(pos);
    ascFile.addNote("LibraryName:###");
    return true;
  }

  pos=input->tell();
  f.str("");
  f << "LibraryName-pos:";
  std::vector<int> positions, lengths;
  for (int i=0; i<N; ++i) {
    int sSz=(int) input->readULong(2);
    int position=(int) input->readULong(2);
    f << position << ":" << sSz << ",";
    positions.push_back(position);
    lengths.push_back(sSz);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "LibraryName-name:";
  sz = (long) input->readULong(4);
  if (sz==0 || !input->checkPosition(pos+4+sz)) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::readLibraryNames: can not find the name\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }

  for (size_t i=0; i<positions.size(); ++i) {
    ClarisDrawParserInternal::Layer layer;
    if (positions[i]+lengths[i]>sz) {
      MWAW_DEBUG_MSG(("ClarisDrawParser::readLibraryNames: the name %d seems bad\n", int(i)));
      f << "###,";
      m_state->m_layerList.push_back(layer);
      continue;
    }
    input->seek(pos+4+positions[i], librevenge::RVNG_SEEK_SET);
    librevenge::RVNGString name("");
    for (int c=0; c<lengths[i]; ++c) {
      char ch=(char) input->readULong(1);
      if (!ch) continue;
      f << ch;
      int unicode= getParserState()->m_fontConverter->unicode(3, (unsigned char) ch);
      if (unicode==-1)
        name.append(ch);
      else
        libmwaw::appendUnicode((uint32_t) unicode, name);
    }
    f << ",";
    layer.m_name = name;
    m_state->m_layerList.push_back(layer);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
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
shared_ptr<ClarisWksStruct::DSET> ClarisDrawParser::readDSET(bool isLibHeader)
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
  shared_ptr<ClarisWksStruct::DSET> zone;
  switch (dset.m_fileType)  {
  case 0:
    zone=m_graphParser->readGroupZone(dset, entry, isLibHeader);
    break;
  case 1:
    zone=m_textParser->readDSETZone(dset, entry);
    break;
  case 4:
    zone=m_graphParser->readBitmapZone(dset, entry);
    break;
  default:
    break;
  }
  if (!zone) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::readDSET: find unexpected type\n"));
    zone.reset(new ClarisWksStruct::DSET(dset));
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
  }

  if (!zone)
    return zone;
  if (m_state->m_zonesMap.find(zone->m_id) != m_state->m_zonesMap.end()) {
    MWAW_DEBUG_MSG(("ClarisDrawParser::readDSET: zone %d already exists!!!!\n",
                    zone->m_id));
  }
  else {
    m_state->m_zonesMap[zone->m_id] = zone;
    m_state->m_zoneIdToFileTypeMap[zone->m_id] = dset.m_fileType;
  }
  return zone;
}

////////////////////////////////////////////////////////////
//
// send data
//
////////////////////////////////////////////////////////////


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
