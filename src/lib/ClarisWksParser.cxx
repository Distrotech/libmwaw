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

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWTextListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSection.hxx"

#include "ClarisWksDatabase.hxx"
#include "ClarisWksDocument.hxx"
#include "ClarisWksGraph.hxx"
#include "ClarisWksPresentation.hxx"
#include "ClarisWksSpreadsheet.hxx"
#include "ClarisWksStruct.hxx"
#include "ClarisWksStyleManager.hxx"
#include "ClarisWksTable.hxx"
#include "ClarisWksText.hxx"

#include "ClarisWksParser.hxx"

/** Internal: the structures of a ClarisWksParser */
namespace ClarisWksParserInternal
{

////////////////////////////////////////
//! Internal: the state of a ClarisWksParser
struct State {
  //! constructor
  State() : m_kind(MWAWDocument::MWAW_K_UNKNOWN), m_actPage(0), m_numPages(0)
  {
  }

  //! the document kind
  MWAWDocument::Kind m_kind;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;
};

////////////////////////////////////////
//! Internal: the subdocument of a ClarisWksParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(ClarisWksParser &pars, MWAWInputStreamPtr input, int zoneId, MWAWPosition const &pos=MWAWPosition()) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId), m_position(pos) {}

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
  //! the subdocument position if defined
  MWAWPosition m_position;
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("ClarisWksParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  if (m_id == -1) { // a number used to send linked frame
    listener->insertChar(' ');
    return;
  }
  if (m_id == 0) {
    MWAW_DEBUG_MSG(("ClarisWksParserInternal::SubDocument::parse: unknown zone\n"));
    return;
  }

  assert(m_parser);
  reinterpret_cast<ClarisWksParser *>(m_parser)->m_document->sendZone(m_id, false,m_position);
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
ClarisWksParser::ClarisWksParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWTextParser(input, rsrcParser, header), m_state(), m_document()
{
  init();
}

ClarisWksParser::~ClarisWksParser()
{
}

void ClarisWksParser::init()
{
  resetTextListener();
  setAsciiName("main-1");

  m_state.reset(new ClarisWksParserInternal::State);
  m_document.reset(new ClarisWksDocument(*this));
  m_document->m_newPage=reinterpret_cast<ClarisWksDocument::NewPage>(&ClarisWksParser::newPage);
  m_document->m_sendFootnote=reinterpret_cast<ClarisWksDocument::SendFootnote>(&ClarisWksParser::sendFootnote);
  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void ClarisWksParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!getTextListener() || m_state->m_actPage == 1)
      continue;
    getTextListener()->insertBreak(MWAWTextListener::PageBreak);
  }
}

////////////////////////////////////////////////////////////
// interface with the different parser
////////////////////////////////////////////////////////////
void ClarisWksParser::sendFootnote(int zoneId)
{
  if (!getTextListener()) return;

  MWAWSubDocumentPtr subdoc(new ClarisWksParserInternal::SubDocument(*this, getInput(), zoneId));
  getTextListener()->insertNote(MWAWNote(MWAWNote::FootNote), subdoc);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void ClarisWksParser::parse(librevenge::RVNGTextInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    // fixme: reset the real kind
    if (getHeader()) getHeader()->setKind(m_state->m_kind);
    ok = m_document->createZones();
    if (ok) {
      createDocument(docInterface);
      MWAWPosition pos;
      //pos.m_anchorTo=MWAWPosition::Page;
      int headerId, footerId;
      m_document->getHeaderFooterId(headerId,footerId);
      std::vector<int> const &mainZonesList=m_document->getMainZonesList();
      for (size_t i = 0; i < mainZonesList.size(); i++) {
        // can happens if mainZonesList is not fully reconstruct
        if (mainZonesList[i]==headerId ||
            mainZonesList[i]==footerId)
          continue;
        m_document->sendZone(mainZonesList[i], false, pos);
      }
      m_document->getPresentationParser()->flushExtra();
      m_document->getGraphParser()->flushExtra();
      m_document->getTableParser()->flushExtra();
      m_document->getTextParser()->flushExtra();
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("ClarisWksParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetTextListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void ClarisWksParser::createDocument(librevenge::RVNGTextInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getTextListener()) {
    MWAW_DEBUG_MSG(("ClarisWksParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  MWAWPageSpan ps(getPageSpan());

  // decrease right | bottom
  if (ps.getMarginRight()>50./72.)
    ps.setMarginRight(ps.getMarginRight()-50./72.);
  else
    ps.setMarginRight(0);
  if (ps.getMarginBottom()>50./72.)
    ps.setMarginBottom(ps.getMarginBottom()-50./72.);
  else
    ps.setMarginBottom(0);

  int numPage = m_document->getTextParser()->numPages();
  if (m_document->getDatabaseParser()->numPages() > numPage)
    numPage = m_document->getDatabaseParser()->numPages();
  if (m_document->getPresentationParser()->numPages() > numPage)
    numPage = m_document->getPresentationParser()->numPages();
  if (m_document->getGraphParser()->numPages() > numPage)
    numPage = m_document->getGraphParser()->numPages();
  if (m_document->getSpreadsheetParser()->numPages() > numPage)
    numPage = m_document->getSpreadsheetParser()->numPages();
  if (m_document->getTableParser()->numPages() > numPage)
    numPage = m_document->getTableParser()->numPages();
  m_state->m_numPages = numPage;

  int headerId, footerId;
  m_document->getHeaderFooterId(headerId,footerId);
  for (int i = 0; i < 2; i++) {
    int zoneId = i == 0 ? headerId : footerId;
    if (zoneId == 0)
      continue;
    MWAWHeaderFooter hF((i==0) ? MWAWHeaderFooter::HEADER : MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    hF.m_subDocument.reset(new ClarisWksParserInternal::SubDocument(*this, getInput(), zoneId));
    ps.setHeaderFooter(hF);
  }
  ps.setPageSpan(m_state->m_numPages);
  std::vector<MWAWPageSpan> pageList(1,ps);
  //
  MWAWTextListenerPtr listen(new MWAWTextListener(*getParserState(), pageList, documentInterface));
  setTextListener(listen);
  listen->startDocument();
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool ClarisWksParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = ClarisWksParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;
  libmwaw::DebugStream f;
  int const headerSize=8;
  input->seek(headerSize,librevenge::RVNG_SEEK_SET);
  if (int(input->tell()) != headerSize) {
    MWAW_DEBUG_MSG(("ClarisWksParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,librevenge::RVNG_SEEK_SET);
  f << "FileHeader:";
  int vers = (int) input->readLong(1);
  setVersion(vers);
  if (vers <=0 || vers > 6) {
    MWAW_DEBUG_MSG(("ClarisWksParser::checkHeader: unknown version: %d\n", vers));
    return false;
  }
  f << "vers=" << vers << ",";
  f << "unk=" << std::hex << input->readULong(2) << ",";
  int val = (int) input->readLong(1);
  if (val)
    f << "unkn1=" << val << ",";
  if (input->readULong(2) != 0x424f && input->readULong(2) != 0x424f)
    return false;

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  int typePos = 0;
  switch (vers) {
  case 1:
    typePos = 243;
    break;
  case 2:
  case 3:
    typePos = 249;
    break;
  case 4:
    typePos = 256;
    break;
  case 5:
    typePos = 268;
    break;
  case 6:
    typePos = 278;
    break;
  default:
    break;
  }
  input->seek(typePos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != typePos)
    return false;
  int type = (int) input->readULong(1);
  switch (type) {
  case 0:
    m_state->m_kind=MWAWDocument::MWAW_K_DRAW;
    break;
  case 1:
    m_state->m_kind=MWAWDocument::MWAW_K_TEXT;
    break;
  case 2:
    m_state->m_kind=MWAWDocument::MWAW_K_SPREADSHEET;
    break;
  case 3:
    m_state->m_kind=MWAWDocument::MWAW_K_DATABASE;
    break;
  case 4:
    m_state->m_kind=MWAWDocument::MWAW_K_PAINT;
    break;
  case 5:
    m_state->m_kind=MWAWDocument::MWAW_K_PRESENTATION;
    break;
  default:
    MWAW_DEBUG_MSG(("ClarisWksParser::checkHeader: unknown type=%d\n", type));
    m_state->m_kind=MWAWDocument::MWAW_K_UNKNOWN;
    break;
  }
  getParserState()->m_kind=m_state->m_kind;
  if (header) {
    header->reset(MWAWDocument::MWAW_T_CLARISWORKS, version());
    header->setKind(m_state->m_kind);
#ifdef DEBUG
    if (type >= 0 && type < 5)
      header->setKind(MWAWDocument::MWAW_K_TEXT);
#else
    if (type == 0 || type == 4)
      header->setKind(MWAWDocument::MWAW_K_TEXT);
#endif
  }

  if (strict && type > 5) return false;
#ifndef DEBUG
  if (type > 8) return false;
#endif
  input->seek(headerSize,librevenge::RVNG_SEEK_SET);

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
