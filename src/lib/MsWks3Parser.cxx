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

#include "MsWks3Parser.hxx"

/** Internal: the structures of a MsWks3Parser */
namespace MsWks3ParserInternal
{
////////////////////////////////////////
//! Internal: the state of a MsWks3Parser
struct State {
  //! constructor
  State() : m_actPage(0), m_numPages(0)
  {
  }

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MsWks3Parser
class SubDocument : public MWAWSubDocument
{
public:
  enum Type { Zone, Text };
  SubDocument(MsWks3Parser &pars, MWAWInputStreamPtr input, Type type,
              int zoneId, int noteId=-1) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_type(type), m_id(zoneId), m_noteId(noteId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const
  {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  /** the type */
  Type m_type;
  /** the subdocument id*/
  int m_id;
  /** the note id */
  int m_noteId;
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("MsWks3Parser::SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_parser);

  long pos = m_input->tell();
  MsWks3Parser *parser = static_cast<MsWks3Parser *>(m_parser);
  switch (m_type) {
  case Text:
    parser->sendText(m_id, m_noteId);
    break;
  case Zone:
    parser->sendZone(m_id);
    break;
  default:
    MWAW_DEBUG_MSG(("MsWks3Parser::SubDocument::parse: unexpected zone type\n"));
    break;
  }
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_type != sDoc->m_type) return true;
  if (m_noteId != sDoc->m_noteId) return true;
  return false;
}
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MsWks3Parser::MsWks3Parser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWTextParser(input, rsrcParser, header), m_state(), m_listZones(), m_document()
{
  m_document.reset(new MsWksDocument(input, *this));
  init();
}

MsWks3Parser::~MsWks3Parser()
{
}

void MsWks3Parser::init()
{
  resetTextListener();
  setAsciiName("main-1");

  m_state.reset(new MsWks3ParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);

  m_document->m_newPage=static_cast<MsWksDocument::NewPage>(&MsWks3Parser::newPage);
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void MsWks3Parser::newPage(int number, bool softBreak)
{
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
void MsWks3Parser::parse(librevenge::RVNGTextInterface *docInterface)
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
      sendZone(MsWksDocument::Z_MAIN);
      m_document->getTextParser3()->flushExtra();
      m_document->getGraphParser()->flushExtra();
    }
    m_document->ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MsWks3Parser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetTextListener();
  if (!ok) throw(libmwaw::ParseException());
}

void MsWks3Parser::sendText(int id, int noteId)
{
  if (noteId < 0)
    m_document->getTextParser3()->sendZone(id);
  else
    m_document->getTextParser3()->sendNote(id, noteId);
}

void MsWks3Parser::sendZone(int zoneType)
{
  if (!getTextListener()) return;
  MsWksDocument::Zone zone=m_document->getZone(MsWksDocument::ZoneType(zoneType));
  if (zone.m_zoneId >= 0)
    m_document->getGraphParser()->sendAll(zone.m_zoneId, zoneType==MsWksDocument::Z_MAIN);
  if (zone.m_textId >= 0)
    m_document->getTextParser3()->sendZone(zone.m_textId);
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MsWks3Parser::createDocument(librevenge::RVNGTextInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getTextListener()) {
    MWAW_DEBUG_MSG(("MsWks3Parser::createDocument: listener already exist\n"));
    return;
  }

  MsWksDocument::Zone mainZone=m_document->getZone(MsWksDocument::Z_MAIN);
  // update the page
  int numPage = 1;
  if (mainZone.m_textId >= 0 && m_document->getTextParser3()->numPages(mainZone.m_textId) > numPage)
    numPage = m_document->getTextParser3()->numPages(mainZone.m_textId);
  if (mainZone.m_zoneId >= 0 && m_document->getGraphParser()->numPages(mainZone.m_zoneId) > numPage)
    numPage = m_document->getGraphParser()->numPages(mainZone.m_zoneId);
  m_state->m_numPages = numPage;
  m_state->m_actPage = 0;

  // create the page list
  MWAWPageSpan ps(getPageSpan());
  int id = m_document->getTextParser3()->getHeader();
  if (id >= 0) {
    MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
    header.m_subDocument.reset
    (new MsWks3ParserInternal::SubDocument
     (*this, m_document->getInput(), MsWks3ParserInternal::SubDocument::Text, id));
    ps.setHeaderFooter(header);
  }
  else if (m_document->getZone(MsWksDocument::Z_HEADER).m_zoneId >= 0) {
    MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
    header.m_subDocument.reset
    (new MsWks3ParserInternal::SubDocument
     (*this, m_document->getInput(), MsWks3ParserInternal::SubDocument::Zone, int(MsWksDocument::Z_HEADER)));
    ps.setHeaderFooter(header);
  }
  id = m_document->getTextParser3()->getFooter();
  if (id >= 0) {
    MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    footer.m_subDocument.reset
    (new MsWks3ParserInternal::SubDocument
     (*this, m_document->getInput(), MsWks3ParserInternal::SubDocument::Text, id));
    ps.setHeaderFooter(footer);
  }
  else if (m_document->getZone(MsWksDocument::Z_FOOTER).m_zoneId >= 0) {
    MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    footer.m_subDocument.reset
    (new MsWks3ParserInternal::SubDocument
     (*this, m_document->getInput(), MsWks3ParserInternal::SubDocument::Zone, int(MsWksDocument::Z_FOOTER)));
    ps.setHeaderFooter(footer);
  }
  ps.setPageSpan(m_state->m_numPages+1);
  std::vector<MWAWPageSpan> pageList(1,ps);
  //
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
bool MsWks3Parser::createZones()
{
  MWAWInputStreamPtr input = m_document->getInput();
  long pos = input->tell();

  if (version()>=3) {
    bool ok = true;
    if (m_document->hasHeader())
      ok = m_document->readGroupHeaderFooter(true,99);
    if (ok) pos = input->tell();
    else input->seek(pos, librevenge::RVNG_SEEK_SET);
    if (ok && m_document->hasFooter())
      ok = m_document->readGroupHeaderFooter(false,99);
    if (ok) pos = input->tell();
    else input->seek(pos, librevenge::RVNG_SEEK_SET);
  }

  std::map<int, MsWksDocument::Zone> &typeZoneMap=m_document->getTypeZoneMap();
  MsWksDocument::ZoneType const type = MsWksDocument::Z_MAIN;
  typeZoneMap.insert(std::map<int,MsWksDocument::Zone>::value_type
                     (int(type),MsWksDocument::Zone(type, int(typeZoneMap.size()))));
  MsWksDocument::Zone &mainZone = typeZoneMap.find(int(type))->second;
  while (!input->isEnd()) {
    pos = input->tell();
    if (!m_document->readZone(mainZone)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
  }

  mainZone.m_textId = m_document->getTextParser3()->createZones(-1, true);

  pos = input->tell();

  if (!input->isEnd())
    m_document->ascii().addPos(input->tell());
  m_document->ascii().addNote("Entries(End)");
  m_document->ascii().addPos(pos+100);
  m_document->ascii().addNote("_");

  // ok, prepare the data
  m_state->m_numPages = 1;
  std::vector<int> linesH, pagesH;
  if (m_document->getTextParser3()->getLinesPagesHeight(mainZone.m_textId, linesH, pagesH))
    m_document->getGraphParser()->computePositions(mainZone.m_zoneId, linesH, pagesH);

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
bool MsWks3Parser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MsWks3ParserInternal::State();
  if (!m_document->checkHeader3(header, strict)) return false;
  if (m_document->getKind() != MWAWDocument::MWAW_K_TEXT)
    return false;
  if (version() < 1 || version() > 3)
    return false;
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
