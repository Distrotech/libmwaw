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

#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>

#include <librevenge/librevenge.h>

#include "MWAWTextListener.hxx"
#include "MWAWDebug.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWOLEParser.hxx"
#include "MWAWPosition.hxx"
#include "MWAWRSRCParser.hxx"

#include "MsWksGraph.hxx"
#include "MsWks4Zone.hxx"

#include "MsWks4Parser.hxx"

/** Internal: the structures of a MsWks4Parser */
namespace MsWks4ParserInternal
{
//! Internal: the state of a MsWks4Parser
struct State {
  //! constructor
  State() : m_oleParser(), m_mn0Parser(), m_headerParser(), m_footerParser(), m_footnoteParser(), m_frameParserMap(), m_unparsedOlesName() { }

  /** the ole parser */
  shared_ptr<MWAWOLEParser> m_oleParser;
  shared_ptr<MsWks4Zone> m_mn0Parser /**parser of main text ole*/,
             m_headerParser /**parser of the header ole*/, m_footerParser /**parser of the footer ole*/,
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
MsWks4Parser::MsWks4Parser(MWAWInputStreamPtr inp, MWAWRSRCParserPtr rsrcParser, MWAWHeader *head) : MWAWTextParser(inp, rsrcParser, head), m_state()
{
  m_state.reset(new MsWks4ParserInternal::State);
  if (!getInput() || !getInput()->isStructured())
    return;
  MWAWInputStreamPtr mainOle = getInput()->getSubStreamByName("MN0");
  if (!mainOle) return;

  m_state->m_mn0Parser.reset(new MsWks4Zone(mainOle, getParserState(), *this, "MN0"));
  m_state->m_mn0Parser->m_newPage=static_cast<MsWksDocument::NewPage>(&MsWks4Parser::newPage);
}

MsWks4Parser::~MsWks4Parser()
{
}

////////////////////////////////////////////////////////////
// text positions
////////////////////////////////////////////////////////////
void MsWks4Parser::newPage(int number, bool soft)
{
  if (!m_state->m_mn0Parser) {
    MWAW_DEBUG_MSG(("MsWks4Parser::newPage: can not find the main zone\n"));
    return;
  }
  m_state->m_mn0Parser->newPage(number, soft);
}

////////////////////////////////////////////////////////////
// the main parse function
////////////////////////////////////////////////////////////
void MsWks4Parser::parse(librevenge::RVNGTextInterface *interface)
{
  assert(getInput().get() != 0);

  bool ok = true;
  try {
    ok = createStructures();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MsWks4Parser::parse: exception catched when parsing OLEs\n"));
    throw (libmwaw::ParseException());
  }

  if (!ok || m_state->m_mn0Parser.get() == 0) {
    MWAW_DEBUG_MSG(("MsWks4Parser::parse: does not find main ole MN0\n"));
    throw (libmwaw::ParseException());
  }

  // and the listener
  MWAWTextListenerPtr listener = m_state->m_mn0Parser->createListener(interface);
  if (!listener) {
    MWAW_DEBUG_MSG(("MsWks4Parser::parse: does not have listener\n"));
    throw (libmwaw::ParseException());
  }
  getParserState()->m_textListener=listener;
  listener->startDocument();
  m_state->m_mn0Parser->readContentZones(MWAWEntry(), true);

  try {
    flushExtra();
  }
  catch (...) { }

  if (listener) listener->endDocument();
  getTextListener().reset();
}

////////////////////////////////////////////////////////////
// create the ole structures
////////////////////////////////////////////////////////////
bool MsWks4Parser::createStructures()
{
  if (!checkHeader(getHeader()) || !m_state->m_mn0Parser || !m_state->m_mn0Parser->createZones(true))
    throw libmwaw::ParseException();
  if (getInput())
    m_state->m_mn0Parser->m_document->createOLEZones(getInput());
  return true;
}

////////////////////////////////////////////////////////////
// flush the extra data
////////////////////////////////////////////////////////////
void MsWks4Parser::flushExtra()
{
  MWAWTextListenerPtr listener=getTextListener();
  if (!listener) return;

  std::vector<std::string> unparsedOLEs=m_state->m_mn0Parser->m_document->getUnparsedOLEZones();
  size_t numUnparsed = unparsedOLEs.size();
  if (numUnparsed == 0) return;

  bool first = true;
  for (size_t i = 0; i < numUnparsed; i++) {
    std::string const &name = unparsedOLEs[i];
    MWAWInputStreamPtr ole = getInput()->getSubStreamByName(name.c_str());
    if (!ole.get()) {
      MWAW_DEBUG_MSG(("Works4: error: can not find OLE part: \"%s\"\n", name.c_str()));
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
    MWAW_DEBUG_MSG(("MsWks4Parser: error: can not parse OLE: \"%s\"\n", name.c_str()));
  }
}

////////////////////////////////////////////////////////////
// basic check header function
////////////////////////////////////////////////////////////
bool MsWks4Parser::checkHeader(MWAWHeader *header, bool /*strict*/)
{
  MWAWInputStreamPtr &input= getInput();
  if (!input || !input->hasDataFork() || !input->isStructured())
    return false;

  MWAWInputStreamPtr mmOle = input->getSubStreamByName("MM");
  if (!mmOle || mmOle->readULong(2) != 0x444e) return false;

  MWAWInputStreamPtr mainOle = input->getSubStreamByName("MN0");
  if (!mainOle || mainOle->readULong(4)!=0x43484e4b) // CHNKINK
    return false;
  MWAW_DEBUG_MSG(("MWAWHeader::checkHeader: find a Microsoft Works 4.0 file\n"));
  if (header)
    header->reset(MWAWDocument::MWAW_T_MICROSOFTWORKS, 104);
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
