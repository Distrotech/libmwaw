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

#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPresentationListener.hxx"
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

#include "ClarisWksPRParser.hxx"

/** Internal: the structures of a ClarisWksPRParser */
namespace ClarisWksPRParserInternal
{

////////////////////////////////////////
//! Internal: the state of a ClarisWksPRParser
struct State {
  //! constructor
  State() : m_actPage(0), m_numPages(0)
  {
  }

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;
};

////////////////////////////////////////
//! Internal: the subdocument of a ClarisWksPRParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(ClarisWksPRParser &pars, MWAWInputStreamPtr input, int zoneId, MWAWPosition const &pos=MWAWPosition()) :
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
    MWAW_DEBUG_MSG(("ClarisWksPRParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  if (m_id == -1) { // a number used to send linked frame
    listener->insertChar(' ');
    return;
  }
  if (m_id == 0) {
    MWAW_DEBUG_MSG(("ClarisWksPRParserInternal::SubDocument::parse: unknown zone\n"));
    return;
  }
  ClarisWksPRParser *parser=dynamic_cast<ClarisWksPRParser *>(m_parser);
  if (!parser) {
    MWAW_DEBUG_MSG(("ClarisWksPRParserInternal::SubDocument::parse: can not find parser\n"));
    return;
  }
  parser->m_document->sendZone(m_id, listener, m_position);
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
ClarisWksPRParser::ClarisWksPRParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWPresentationParser(input, rsrcParser, header), m_state(), m_document()
{
  init();
}

ClarisWksPRParser::~ClarisWksPRParser()
{
}

void ClarisWksPRParser::init()
{
  resetPresentationListener();
  setAsciiName("main-1");

  m_state.reset(new ClarisWksPRParserInternal::State);
  m_document.reset(new ClarisWksDocument(*this));
  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}


////////////////////////////////////////////////////////////
// interface with the different parser
////////////////////////////////////////////////////////////
bool ClarisWksPRParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = ClarisWksPRParserInternal::State();
  if (!m_document->checkHeader(header, strict))
    return false;
  return getParserState()->m_kind==MWAWDocument::MWAW_K_PRESENTATION;
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void ClarisWksPRParser::parse(librevenge::RVNGPresentationInterface *docInterface)
{
  if (!getInput().get() || !checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    ok = m_document->createZones();
    // check that we have at least read the main zone
    if (ok) {
      shared_ptr<ClarisWksStruct::DSET> zMap = m_document->getZone(1);
      ok = zMap && zMap->m_fileType==5;
    }
    if (ok) {
      createDocument(docInterface);

      m_document->sendZone(1, getPresentationListener(), MWAWPosition());
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("ClarisWksPRParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetPresentationListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void ClarisWksPRParser::createDocument(librevenge::RVNGPresentationInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getPresentationListener()) {
    MWAW_DEBUG_MSG(("ClarisWksPRParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;
  m_state->m_numPages = m_document->numPages();

  // create the page list
  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan master;
  m_document->updatePageSpanList(pageList, master);

  //
  MWAWPresentationListenerPtr listen(new MWAWPresentationListener(*getParserState(), pageList, documentInterface));
  setPresentationListener(listen);
  listen->startDocument();

  // time to send the master
  listen->openMasterPage(master);
  m_document->getPresentationParser()->sendMaster();
  listen->closeMasterPage();
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
