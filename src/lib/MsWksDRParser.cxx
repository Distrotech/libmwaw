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
  State() : m_actPage(0), m_numPages(0)
  {
  }

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;
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
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void MsWksDRParser::newPage(int number, bool softBreak)
{
  if (!getGraphicListener() || number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  long pos = m_document->getInput()->tell();
  while (m_state->m_actPage < number) {
    if (++m_state->m_actPage!=1) {
      if (softBreak)
        getGraphicListener()->insertBreak(MWAWGraphicListener::SoftPageBreak);
      else
        getGraphicListener()->insertBreak(MWAWGraphicListener::PageBreak);
    }

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
      for (int i=0; i< m_state->m_numPages; ++i)
        newPage(i);
#if defined(DEBUG)
      if (version()<=3)
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
    m_document->createOLEZones(getInput());
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

  libmwaw::DebugFile &ascFile = m_document->ascii();
  std::multimap<int, MsWksDocument::Zone> &typeZoneMap=m_document->getTypeZoneMap();
  MWAWEntry group;

  // now the main group of draw shape
  int mainId=MsWksDocument::Z_MAIN; // fixme: use m_document->getNewZoneId() here
  typeZoneMap.insert(std::multimap<int,MsWksDocument::Zone>::value_type
                     (MsWksDocument::Z_MAIN,MsWksDocument::Zone(MsWksDocument::Z_MAIN, mainId)));
  if (version()==4) {
    pos=input->tell();
    int id=m_document->getNewZoneId();
    typeZoneMap.insert(std::multimap<int,MsWksDocument::Zone>::value_type
                       (MsWksDocument::Z_NONE,MsWksDocument::Zone(MsWksDocument::Z_NONE, id)));
    group.setId(mainId);
    group.setName("RBIL");
    if (!m_document->m_graphParser->readRB(input,group,1)) {
      MWAW_DEBUG_MSG(("MsWksDRParser::createZones: can not read RBIL group\n"));
      ascFile.addPos(pos);
      ascFile.addNote("Entries(RBIL):###");
      return false;
    }
  }

  pos=input->tell();
  group.setId(mainId);
  group.setName("RBDR");
  if (!m_document->m_graphParser->readRB(input,group,1)) {
    MWAW_DEBUG_MSG(("MsWksDRParser::createZones: can not read RBDR group\n"));
    ascFile.addPos(pos);
    ascFile.addNote("Entries(RBDR):###");
    return false;
  }

  // normally, the file is now parsed, let check for potential remaining zones
  if (!input->isEnd()) {
    MWAW_DEBUG_MSG(("MsWksDRParser::createZones: find some extra data\n"));
    while (!input->isEnd()) {
      pos = input->tell();
      MsWksDocument::Zone unknown;
      if (!m_document->readZone(unknown)) {
        ascFile.addPos(pos);
        ascFile.addNote("Entries(End)");
        ascFile.addPos(pos+100);
        ascFile.addNote("_");
        break;
      }
    }
  }

  std::vector<int> linesH, pagesH;
  m_document->getGraphParser()->computePositions(mainId, linesH, pagesH);

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
