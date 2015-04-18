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


#include "MWAWTextListener.hxx"
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

#include "MsWksParser.hxx"

/** Internal: the structures of a MsWksParser */
namespace MsWksParserInternal
{
////////////////////////////////////////
//! Internal: the state of a MsWksParser
struct State {
  //! constructor
  State() : m_mn0Parser(), m_actPage(0), m_numPages(0)
  {
  }

  shared_ptr<MsWks4Zone> m_mn0Parser /**parser of main text ole (v4 document)*/;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;
};
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MsWksParser::MsWksParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWTextParser(input, rsrcParser, header), m_state(new MsWksParserInternal::State), m_listZones(), m_document()
{
  if (input && input->isStructured()) { // potential v4 file
    MWAWInputStreamPtr mainOle = getInput()->getSubStreamByName("MN0");
    if (!mainOle) return;

    m_state->m_mn0Parser.reset(new MsWks4Zone(mainOle, getParserState(), *this, "MN0"));
    m_document=m_state->m_mn0Parser->m_document;
  }
  else
    m_document.reset(new MsWksDocument(input, *this));
  m_document->m_newPage=static_cast<MsWksDocument::NewPage>(&MsWksParser::newPage);
  init();
}

MsWksParser::~MsWksParser()
{
}

void MsWksParser::init()
{
  resetTextListener();
  setAsciiName("main-1");

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void MsWksParser::newPage(int number, bool softBreak)
{
  if (m_state->m_mn0Parser) // v4 document
    return m_state->m_mn0Parser->newPage(number, softBreak);

  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!getTextListener() || m_state->m_actPage == 1)
      continue;
    if (softBreak)
      getTextListener()->insertBreak(MWAWTextListener::SoftPageBreak);
    else
      getTextListener()->insertBreak(MWAWTextListener::PageBreak);
  }
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MsWksParser::parse(librevenge::RVNGTextInterface *docInterface)
{
  if (!checkHeader(0L) || !m_document || !m_document->getInput())  throw(libmwaw::ParseException());

  int const vers=version();
  bool ok = true;
  try {
    // create the asciiFile for v1-v3 document
    if (vers<=3)
      m_document->initAsciiFile(asciiName());

    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      if (vers<=3)
        m_document->sendZone(MsWksDocument::Z_MAIN);
      else
        m_state->m_mn0Parser->readContentZones(MWAWEntry(), true);
      flushExtra();
    }
    m_document->ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MsWksParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetTextListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// flush the extra data
////////////////////////////////////////////////////////////
void MsWksParser::flushExtra()
{
  MWAWTextListenerPtr listener=getTextListener();
  if (!listener) return;

  if (version()<=3) {
    m_document->getTextParser3()->flushExtra();
    m_document->getGraphParser()->flushExtra();
    return;
  }

  // send the final data
  std::vector<std::string> unparsedOLEs=m_document->getUnparsedOLEZones();
  size_t numUnparsed = unparsedOLEs.size();
  if (numUnparsed == 0) return;

  bool first = true;
  for (size_t i = 0; i < numUnparsed; i++) {
    std::string const &name = unparsedOLEs[i];
    MWAWInputStreamPtr ole = getInput()->getSubStreamByName(name.c_str());
    if (!ole.get()) {
      MWAW_DEBUG_MSG(("MsWksParser::flushExtra: error: can not find OLE part: \"%s\"\n", name.c_str()));
      continue;
    }

    shared_ptr<MsWks4Zone> newParser(new MsWks4Zone(ole, getParserState(), *this, name));
    bool ok = true;
    try {
      ok = newParser->createZones(false);
      if (ok) {
        // FIXME: add a message here
        if (first) {
          first = false;
          listener->setFont(MWAWFont(20,20));
          librevenge::RVNGString message = "--------- The original document has some extra ole: -------- ";
          listener->insertUnicodeString(message);
          listener->insertEOL();
        }
        newParser->readContentZones(MWAWEntry(), false);
      }
    }
    catch (...) {
      ok = false;
    }

    if (ok) continue;
    MWAW_DEBUG_MSG(("MsWksParser::flushExtra: error: can not parse OLE: \"%s\"\n", name.c_str()));
  }
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MsWksParser::createDocument(librevenge::RVNGTextInterface *documentInterface)
{
  if (!documentInterface || getTextListener()) {
    MWAW_DEBUG_MSG(("MsWksParser::createDocument: listener already exist\n"));
    throw (libmwaw::ParseException());
  }

  if (m_state->m_mn0Parser) {
    MWAWTextListenerPtr listener = m_state->m_mn0Parser->createListener(documentInterface);
    if (!listener) {
      MWAW_DEBUG_MSG(("MsWksParser::parse: can not create a listener\n"));
      throw (libmwaw::ParseException());
    }
    setTextListener(listener);
    listener->startDocument();
    return;
  }

  std::vector<MWAWPageSpan> pageList;
  m_state->m_actPage = 0;
  m_document->getPageSpanList(pageList, m_state->m_numPages);
  MWAWTextListenerPtr listen(new MWAWTextListener(*getParserState(), pageList, documentInterface));
  setTextListener(listen);
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
bool MsWksParser::createZones()
{
  MWAWInputStreamPtr input = m_document->getInput();
  if (!input) return false;

  int const vers=version();
  if (vers==4) {
    if (!m_state->m_mn0Parser || !m_state->m_mn0Parser->createZones(true))
      return false;
    m_state->m_mn0Parser->m_document->createOLEZones(getInput());
    return true;
  }

  long pos = input->tell();
  if (vers>=3) {
    bool ok = true;
    if (m_document->hasHeader())
      ok = m_document->readGroupHeaderFooter(true,99);
    if (ok) pos = input->tell();
    else input->seek(pos, librevenge::RVNG_SEEK_SET);
    if (ok && m_document->hasFooter())
      ok = m_document->readGroupHeaderFooter(false,99);
    if (!ok) input->seek(pos, librevenge::RVNG_SEEK_SET);
  }

  std::multimap<int, MsWksDocument::Zone> &typeZoneMap=m_document->getTypeZoneMap();
  int mainId=int(typeZoneMap.size()); // FIXME: use m_document->getNewZoneId();
  typeZoneMap.insert(std::multimap<int,MsWksDocument::Zone>::value_type
                     (MsWksDocument::Z_MAIN,MsWksDocument::Zone(MsWksDocument::Z_MAIN, mainId)));
  MsWksDocument::Zone &mainZone = typeZoneMap.find(MsWksDocument::Z_MAIN)->second;

  libmwaw::DebugFile &ascFile = m_document->ascii();
  /* now normally
     in v1: a list of pictures, document info zone, ..., without any way to know the limit
     in v2: a group zone (which regroup the graphic) and a document info zone
     in v3: a document info zone and a group zone (the graphic zone)
   */
  while (!input->isEnd()) {
    pos = input->tell();
    if (!m_document->readZone(mainZone)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
  }

  mainZone.m_textId = m_document->getTextParser3()->createZones(-1, true);

  pos = input->tell();

  if (!input->isEnd()) {
    ascFile.addPos(pos);
    ascFile.addNote("Entries(End)");
    ascFile.addPos(pos+100);
    ascFile.addNote("_");
  }

  // ok, prepare the data
  m_state->m_numPages = 1;
  std::vector<int> linesH, pagesH;
  if (m_document->getTextParser3()->getLinesPagesHeight(mainZone.m_textId, linesH, pagesH))
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
bool MsWksParser::checkHeader(MWAWHeader *header, bool strict)
{
  MWAWInputStreamPtr &input= getInput();
  if (!input || !input->hasDataFork() || !m_document)
    return false;

  if (!input->isStructured()) {
    // first check v1-v3 file
    if (!m_document->checkHeader3(header, strict)) return false;
    if (m_document->getKind() != MWAWDocument::MWAW_K_TEXT)
      return false;
    if (version() < 1 || version() > 3)
      return false;
    return true;
  }

  // now check for version 4 file
  MWAWInputStreamPtr mmOle = input->getSubStreamByName("MM");
  if (!mmOle) return false;
  mmOle->seek(0, librevenge::RVNG_SEEK_SET);
  if (mmOle->readULong(2) != 0x444e) return false;

  MWAWInputStreamPtr mainOle = m_document->getInput();
  if (!mainOle) return false;
  mainOle->seek(0, librevenge::RVNG_SEEK_SET);
  if (mainOle->readULong(4)!=0x43484e4b) // CHNKINK
    return false;
  setVersion(4);
  if (header)
    header->reset(MWAWDocument::MWAW_T_MICROSOFTWORKS, 4);
  return true;

}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
