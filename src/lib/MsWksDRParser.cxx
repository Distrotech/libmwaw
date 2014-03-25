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

#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include <librevenge/librevenge.h>


#include "MWAWGraphicListener.hxx"
#include "MWAWHeader.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWOLEParser.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSubDocument.hxx"

#include "MsWksGraph.hxx"
#include "MsWksDocument.hxx"
#include "MsWks3Text.hxx"
#include "MsWks4Zone.hxx"

#include "MsWksDRParser.hxx"

/** Internal: the structures of a MsWksDRParser */
namespace MsWksDRParserInternal
{
////////////////////////////////////////
//! Internal: the state of a MsWksDRParser
struct State {
  //! constructor
  State() : m_actPage(0), m_numPages(0), m_headerParser(), m_footerParser(), m_footnoteParser(), m_frameParserMap(), m_unparsedOlesName()
  {
  }

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  /** the ole parser */
  shared_ptr<MWAWOLEParser> m_oleParser;
  shared_ptr<MsWks4Zone> m_headerParser /**parser of the header ole*/, m_footerParser /**parser of the footer ole*/,
             m_footnoteParser /**parser of the footnote ole*/;
  /**the frame parsers: name-> parser*/
  std::map<std::string, shared_ptr<MsWks4Zone> > m_frameParserMap;
  //! the list of unparsed OLEs
  std::vector<std::string> m_unparsedOlesName;
};
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MsWksDRParser::MsWksDRParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWGraphicParser(input, rsrcParser, header), m_state(), m_listZones(), m_document()
{
  MWAWInputStreamPtr mainInput=input;
  if (input->isStructured()) {
    MWAWInputStreamPtr mainOle = input->getSubStreamByName("MN0");
    if (mainOle)
      mainInput=mainOle;
  }
  m_document.reset(new MsWksDocument(mainInput, *this));
  init();
}

MsWksDRParser::~MsWksDRParser()
{
}

void MsWksDRParser::init()
{
  resetGraphicListener();
  setAsciiName("main-1");

  m_state.reset(new MsWksDRParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);

  m_document->m_newPage=static_cast<MsWksDocument::NewPage>(&MsWksDRParser::newPage);
  m_document->m_sendOLE=static_cast<MsWksDocument::SendOLE>(&MsWksDRParser::sendOLE);
  m_document->m_sendTextbox=static_cast<MsWksDocument::SendTextbox>(&MsWksDRParser::sendTextbox);
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void MsWksDRParser::newPage(int number, bool softBreak)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  long pos = m_document->getInput()->tell();
  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!getGraphicListener() || m_state->m_actPage == 1)
      continue;
    if (softBreak)
      getGraphicListener()->insertBreak(MWAWGraphicListener::SoftPageBreak);
    else
      getGraphicListener()->insertBreak(MWAWGraphicListener::PageBreak);

    MsWksGraph::SendData sendData;
    sendData.m_type = MsWksGraph::SendData::RBDR;
    sendData.m_anchor =  MWAWPosition::Page;
    sendData.m_page = m_state->m_actPage;
    m_document->getGraphParser()->sendObjects(sendData);
  }
  m_document->getInput()->seek(pos, librevenge::RVNG_SEEK_SET);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MsWksDRParser::parse(librevenge::RVNGDrawingInterface *docInterface)
{
  assert(m_document && m_document->getInput());

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    m_document->initAsciiFile(asciiName());

    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      m_document->sendZone(MsWksDocument::Z_MAIN);
#ifdef DEBUG
      m_document->getTextParser3()->flushExtra();
      m_document->getGraphParser()->flushExtra();
#endif
    }
    m_document->ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MsWksDRParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetGraphicListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MsWksDRParser::createDocument(librevenge::RVNGDrawingInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getGraphicListener()) {
    MWAW_DEBUG_MSG(("MsWksDRParser::createDocument: listener already exist\n"));
    return;
  }

  std::vector<MWAWPageSpan> pageList;
  m_state->m_actPage = 0;
  m_document->getPageSpanList(pageList, m_state->m_numPages);
  MWAWGraphicListenerPtr listen(new MWAWGraphicListener(*getParserState(), pageList, documentInterface));
  setGraphicListener(listen);
  listen->startDocument();
  // time to send page information the graph parser and the text parser
  m_document->getGraphParser()->setPageLeftTop
  (Vec2f(72.f*float(getPageSpan().getMarginLeft()),
         72.f*float(getPageSpan().getMarginTop())+m_document->getHeaderFooterHeight(true)));
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool MsWksDRParser::createZones()
{
  if (getInput()->isStructured())
    createOLEZones();
  MWAWInputStreamPtr input = m_document->getInput();
  long pos = input->tell();
  if (!m_document->readDocumentInfo(0x9a))
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (m_document->hasHeader() && !m_document->readGroupHeaderFooter(true,99))
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  pos = input->tell();
  if (m_document->hasFooter() && !m_document->readGroupHeaderFooter(false,99))
    input->seek(pos, librevenge::RVNG_SEEK_SET);

  if (!readDrawHeader()) return false;

  if (version()==4) {
    MWAWEntry group;
    pos = input->tell();
    MsWksDocument::Zone unknownZone(MsWksDocument::Z_NONE, -1);
    if (!m_document->readGroup(unknownZone, group, 2))
      input->seek(pos, librevenge::RVNG_SEEK_SET);
  }

  // now the main group of draw shape
  std::map<int, MsWksDocument::Zone> &typeZoneMap=m_document->getTypeZoneMap();
  MsWksDocument::ZoneType const type = MsWksDocument::Z_MAIN;
  typeZoneMap.insert(std::map<int,MsWksDocument::Zone>::value_type
                     (int(type),MsWksDocument::Zone(type, int(typeZoneMap.size()))));
  MsWksDocument::Zone &mainZone = typeZoneMap.find(int(type))->second;
  MWAWEntry group;
  pos = input->tell();
  if (!m_document->readGroup(mainZone, group, 2))
    input->seek(pos, librevenge::RVNG_SEEK_SET);

  if (!input->isEnd()) {
    MWAW_DEBUG_MSG(("MsWksDRParser::createZones: find some extra data\n"));
    while (!input->isEnd()) {
      pos = input->tell();
      if (!m_document->readZone(mainZone)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
    }
  }
  if (!input->isEnd()) {
    pos = input->tell();
    m_document->ascii().addPos(pos);
    m_document->ascii().addNote("Entries(End)");
    m_document->ascii().addPos(pos+100);
    m_document->ascii().addNote("_");
  }

  std::vector<int> linesH, pagesH;
  m_document->getGraphParser()->computePositions(mainZone.m_zoneId, linesH, pagesH);

  return true;
}

////////////////////////////////////////////////////////////
// create the ole structures
////////////////////////////////////////////////////////////
bool MsWksDRParser::createOLEZones()
{
  MWAWInputStreamPtr &input= getInput();
  assert(input.get());
  if (!checkHeader(getHeader()))
    throw libmwaw::ParseException();

  m_state->m_oleParser.reset(new MWAWOLEParser("MN0"));

  if (!m_state->m_oleParser->parse(input)) return false;

  // normally,
  // MacWorks/QHdr, MacWorks/QFtr, MacWorks/QFootnotes, MacWorks/QFrm<number>
  // MN0 (the main header)
  std::vector<std::string> unparsed = m_state->m_oleParser->getNotParse();

  size_t numUnparsed = unparsed.size();
  unparsed.push_back("MN0");

  for (size_t i = 0; i <= numUnparsed; i++) {
    std::string const &name = unparsed[i];

    // separated the directory and the name
    //    MatOST/MatadorObject1/Ole10Native
    //      -> dir="MatOST/MatadorObject1", base="Ole10Native"
    std::string::size_type pos = name.find_last_of('/');
    std::string dir, base;
    if (pos == std::string::npos) base = name;
    else if (pos == 0) base = name.substr(1);
    else {
      dir = name.substr(0,pos);
      base = name.substr(pos+1);
    }

    if (dir == "" && base == "MN0") continue;
    bool ok = false;
    bool isFrame = false;
    if (!ok && dir == "MacWorks") {
      ok = (base == "QHdr" || base == "QFtr" || base == "QFootnotes");
      if (!ok && strncmp(base.c_str(),"QFrm",4)==0)
        ok = isFrame = true;
    }
    if (!ok) {
      m_state->m_unparsedOlesName.push_back(name);
      continue;
    }

    MWAWInputStreamPtr ole = input->getSubStreamByName(name.c_str());
    if (!ole.get()) {
      MWAW_DEBUG_MSG(("MsWksDrParser::createOLEZones: error: can not find OLE part: \"%s\"\n", name.c_str()));
      continue;
    }

    shared_ptr<MsWks4Zone> newParser(new MsWks4Zone(ole, getParserState(), *this, name));
    try {
      ok = newParser->createZones(false);
    }
    catch (...) {
      ok = false;
    }

    if (!ok) {
      MWAW_DEBUG_MSG(("MsWksDrParser::createOLEZones: error: can not parse OLE: \"%s\"\n", name.c_str()));
      continue;
    }

    if (base == "QHdr") m_state->m_headerParser = newParser;
    else if (base == "QFtr") m_state->m_footerParser = newParser;
    else if (isFrame) {
      std::map<std::string, shared_ptr<MsWks4Zone> >::iterator frameIt =
        m_state->m_frameParserMap.find(base);
      if (frameIt != m_state->m_frameParserMap.end()) {
        MWAW_DEBUG_MSG(("MsWksDrParser::createOLEZones: error: oops, I already find a frame zone %s\n", base.c_str()));
      }
      else
        m_state->m_frameParserMap[base] = newParser;
    }
    else if (base == "QFootnotes") m_state->m_footnoteParser = newParser;
  }

  return true;
}

void MsWksDRParser::sendTextbox(MWAWEntry const &entry, std::string const &frame)
{
  MWAWListenerPtr listener=getMainListener();
  if (!listener) return;

  if (entry.length()==0) {
    listener->insertChar(' ');
    return;
  }

  MsWks4Zone *parser = 0;
  std::map<std::string, shared_ptr<MsWks4Zone> >::iterator frameIt =
    m_state->m_frameParserMap.find(frame);
  if (frameIt != m_state->m_frameParserMap.end())
    parser = frameIt->second.get();
  if (!parser || parser->getTextPosition().length() < entry.end()) {
    MWAW_DEBUG_MSG(("MsWksDRParser::sendTextbox: can not find frame ole: %s\n", frame.c_str()));
    listener->insertChar(' ');
    return;
  }

  // ok, create the entry
  MWAWEntry ent(entry);
  ent.setBegin(entry.begin()+parser->getTextPosition().begin());
  parser->createZones(false);
  parser->readContentZones(ent, false);
}

void MsWksDRParser::sendOLE(int id, MWAWPosition const &pictPos, MWAWGraphicStyle const &style)
{
  if (!getMainListener()) return;

  librevenge::RVNGBinaryData data;
  MWAWPosition pos;
  std::string type;
  if (!m_state->m_oleParser->getObject(id, data, pos, type)) {
    MWAW_DEBUG_MSG(("MsWksDRParser::sendOLE: can not find OLE%d\n", id));
    return;
  }
  getMainListener()->insertPicture(pictPos, data, type, style);
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MsWksDRParser::readDrawHeader()
{
  MWAWInputStreamPtr input=m_document->getInput();

  int const vers=version();
  long pos = input->tell();
  int N = (int) input->readULong(2);
  int headerSize = vers == 3 ? 4 : 88;
  int dataSize = vers == 3 ? 4 : 51;
  libmwaw::DebugStream f;
  f << "FileHeader(A)";
  if (!input->checkPosition(pos+headerSize+dataSize*N)) {
    f << "###";
    MWAW_DEBUG_MSG(("MsWksDRParser::readDrawHeader: Unknown header find N=%d\n", N));
    m_document->ascii().addPos(pos);
    m_document->ascii().addNote(f.str().c_str());
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  f << "N = " << N << ", v0 = " << input->readLong(2);
  // version 4
  // begin always by
  // v0=1, v1=[2|6|9], v2=180, v3=1, v6=0x300, v7=0x400, v8=1, v11=0x300, v12=0x400, v14=255
  // followed by
  // v16=4003, v20=8, v31=100
  // or v15=6, v16=-1, v17=-1, v19=0x300, v20=8, v21=1, v24=44, v25=6, v31=100
  if (vers == 4) {
    for (int i = 1; i < 35; i++) {
      int val = (int) input->readLong(2);
      if (val) f << ", v" << i << "=" << val;
    }
    //    [0,0,1,1,0,0,0,0,1,1,0,1,1,1,1,0,]
    // or [0,0,3,1,0,0,0,0,1,1,0,1,1,1,1,0,]
    f << ",fl=[";
    for (int i = 0; i < 16; i++) {
      f << input->readLong(1) << ",";
    }
    f << "]";
  }
  m_document->ascii().addPos(pos);
  m_document->ascii().addNote(f.str().c_str());

  input->seek(pos+headerSize,librevenge::RVNG_SEEK_SET);
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << "FileHeader(A)[" << i << "]:";
    int v = (int) input->readULong(2);    // normally 0xe or 0x800e
    int id = (int) input->readLong(2);
    f << std::hex << v << std::dec;

    if (id != i+1) {
      MWAW_DEBUG_MSG(("MsWksDRParser::readDrawHeader: bad data %i\n", id));
      f << "###";
      m_document->ascii().addPos(pos);
      m_document->ascii().addNote(f.str().c_str());
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }

    if (vers==4) {
      // always v0=255, v2=4003, v6=8, v16=-1, w2=3, w4=4
      for (int j = 0; j < 20; j++) {
        int val = (int) input->readLong(2);
        if (val) f << ", v" << j << "=" << val;
      }
      for (int j = 0; j < 7; j++) {
        int val = (int) input->readLong(1);
        if (val) f << ", w" << j << "=" << val;
      }
    }

    m_document->ascii().addPos(pos);
    m_document->ascii().addNote(f.str().c_str());
    input->seek(pos+dataSize, librevenge::RVNG_SEEK_SET);
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MsWksDRParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MsWksDRParserInternal::State();
  if (!m_document->checkHeader3(header, strict)) return false;
  if (m_document->getKind() != MWAWDocument::MWAW_K_DRAW)
    return false;
  if (version() < 2 || version() > 4)
    return false;
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
