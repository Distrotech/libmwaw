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
  State() : m_actPage(0), m_numPages(0)
  {
  }

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
  static_cast<ClarisWksParser *>(m_parser)->m_document->sendZone(m_id, listener, m_position);
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
  m_document->m_newPage=static_cast<ClarisWksDocument::NewPage>(&ClarisWksParser::newPage);
  m_document->m_sendFootnote=static_cast<ClarisWksDocument::SendFootnote>(&ClarisWksParser::sendFootnote);
  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void ClarisWksParser::newPage(int number, bool soft)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!getTextListener() || m_state->m_actPage == 1)
      continue;
    if (soft)
      getTextListener()->insertBreak(MWAWTextListener::SoftPageBreak);
    else
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

bool ClarisWksParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = ClarisWksParserInternal::State();
  if (!m_document->checkHeader(header, strict))
    return false;
  // remove me when a presentation parser will be implemented
  if (getParserState()->m_kind==MWAWDocument::MWAW_K_PRESENTATION && header)
    header->setKind(MWAWDocument::MWAW_K_TEXT);
  return getParserState()->m_kind==MWAWDocument::MWAW_K_TEXT ||
         getParserState()->m_kind==MWAWDocument::MWAW_K_DRAW ||
         getParserState()->m_kind==MWAWDocument::MWAW_K_PRESENTATION;
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
    ok = m_document->createZones();
    if (ok) {
      createDocument(docInterface);
      std::vector<int> const &mainZonesList=m_document->getMainZonesList();
      if (getParserState()->m_kind!=MWAWDocument::MWAW_K_PRESENTATION) {
        for (size_t i = 0; i < mainZonesList.size(); i++)
          m_document->getGraphParser()->sendPageGraphics(mainZonesList[i]);
        if (getParserState()->m_kind==MWAWDocument::MWAW_K_TEXT) {
          shared_ptr<ClarisWksStruct::DSET> mainMap = m_document->getZone(1);
          if (mainMap && mainMap->m_fileType==1 && !mainMap->m_parsed)
            m_document->sendZone(1, MWAWListenerPtr(), MWAWPosition());
          else {
            MWAW_DEBUG_MSG(("ClarisWksParser::parse: find a problem with the main text zones\n"));
          }
        }
        newPage(m_state->m_numPages, false);
      }
      else {
        MWAWPosition pos;
        for (size_t i = 0; i < mainZonesList.size(); i++)
          m_document->sendZone(mainZonesList[i], MWAWListenerPtr(), MWAWPosition());
      }
      m_document->getPresentationParser()->flushExtra();
#ifdef DEBUG
      m_document->getGraphParser()->flushExtra();
#endif
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
  m_state->m_numPages = m_document->numPages();

  // create the page list
  std::vector<MWAWPageSpan> pageList;
  m_document->updatePageSpanList(pageList);
  //
  MWAWTextListenerPtr listen(new MWAWTextListener(*getParserState(), pageList, documentInterface));
  setTextListener(listen);
  listen->startDocument();
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
