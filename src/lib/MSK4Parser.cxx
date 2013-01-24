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

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWDebug.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWOLEParser.hxx"
#include "MWAWPosition.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "MSK4Zone.hxx"

#include "MSK4Parser.hxx"

/** Internal: the structures of a MSK4Parser */
namespace MSK4ParserInternal
{
//! Internal: the subdocument of a MSK4Parser
class SubDocument : public MWAWSubDocument
{
public:
  //! type of an entry stored in textId
  enum Type { Unknown, MN };
  //! constructor for a note with identificator \a ntId
  SubDocument(MSK4Zone *pars, MWAWInputStreamPtr input, int ntId)
    : MWAWSubDocument (pars, input, MWAWEntry()), m_noteId(ntId) { }
  //! constructor for a text/frame entry
  SubDocument(MSK4Zone *pars, MWAWInputStreamPtr input, MWAWEntry const &entry) :
    MWAWSubDocument (pars, input, entry), m_noteId(-1) {}
  //! destructor
  ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  //! the note identificator
  int m_noteId;
};

// implementation
bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_noteId != sDoc->m_noteId) return true;
  return false;
}

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("MSK4Parser::SubDocument::parse: no listener\n"));
    return;
  }
  // the foot note
  if (type == libmwaw::DOC_NOTE) {
    if (!m_parser) {
      listener->insertChar(' ');
      return;
    }
    MSK4Zone *mnParser = reinterpret_cast<MSK4Zone *>(m_parser);
    mnParser->createZones(false);
    mnParser->readFootNote(m_noteId);
    return;
  }

  if (!m_parser) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no parser\n"));
    listener->insertChar(' ');
    return;
  }

  if (m_zone.isParsed() && type != libmwaw::DOC_HEADER_FOOTER) {
    listener->insertChar(' ');
    MWAW_DEBUG_MSG(("SubDocument::parse: this zone is already parsed\n"));
    return;
  }
  m_zone.setParsed(true);
  if (m_zone.id() != MN) {
    listener->insertChar(' ');
    MWAW_DEBUG_MSG(("SubDocument::parse: send not MN entry is not implemented\n"));
    return;
  }
  MSK4Zone *mnParser = reinterpret_cast<MSK4Zone *>(m_parser);
  mnParser->createZones(false);
  mnParser->readContentZones(m_zone, false);
}

//! Internal: the state of a MSK4Parser
struct State {
  //! constructor
  State() : m_oleParser(), m_mn0Parser(), m_headerParser(), m_footerParser(), m_footnoteParser(), m_frameParserMap(), m_unparsedOlesName() { }

  /** the ole parser */
  shared_ptr<MWAWOLEParser> m_oleParser;
  shared_ptr<MSK4Zone> m_mn0Parser /**parser of main text ole*/,
             m_headerParser /**parser of the header ole*/, m_footerParser /**parser of the footer ole*/,
             m_footnoteParser /**parser of the footnote ole*/;
  /**the frame parsers: name-> parser*/
  std::map<std::string, shared_ptr<MSK4Zone> > m_frameParserMap;
  //! the list of unparsed OLEs
  std::vector<std::string> m_unparsedOlesName;
};
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MSK4Parser::MSK4Parser(MWAWInputStreamPtr inp, MWAWRSRCParserPtr rsrcParser, MWAWHeader *head) : m_input(inp), m_rsrcParser(rsrcParser), m_header(head), m_state(), m_listener(), m_convertissor()
{
  m_state.reset(new MSK4ParserInternal::State);
  m_convertissor.reset(new MWAWFontConverter);
}

MSK4Parser::~MSK4Parser()
{
  if (m_listener.get()) m_listener->endDocument();
}

void MSK4Parser::setListener(MWAWContentListenerPtr listen)
{
  m_listener = listen;
  m_state->m_mn0Parser->setListener(listen);
  if (m_state->m_headerParser.get())
    m_state->m_headerParser->setListener(listen);
  if (m_state->m_footerParser.get())
    m_state->m_footerParser->setListener(listen);
  std::map<std::string, shared_ptr<MSK4Zone> >::iterator frameIt;
  for (frameIt=m_state->m_frameParserMap.begin(); frameIt != m_state->m_frameParserMap.end(); frameIt++)
    frameIt->second->setListener(listen);
  if (m_state->m_footnoteParser.get())
    m_state->m_footnoteParser->setListener(listen);
}

////////////////////////////////////////////////////////////
// the main parse function
////////////////////////////////////////////////////////////
void MSK4Parser::parse(WPXDocumentInterface *interface)
{
  assert(m_input.get() != 0);

  bool ok = true;
  try {
    ok = createStructures();
  } catch (...) {
    MWAW_DEBUG_MSG(("MSK4Parser::parse: exception catched when parsing OLEs\n"));
    throw(libmwaw::ParseException());
  }

  if (!ok || m_state->m_mn0Parser.get() == 0) {
    MWAW_DEBUG_MSG(("MSK4Parser::parse: does not find main ole MN0\n"));
    throw(libmwaw::ParseException());
  }

  // time to create the header, ...
  MWAWEntry empty;
  empty.setId(MSK4ParserInternal::SubDocument::MN);
  MWAWSubDocumentPtr header, footer;
  if (m_state->m_headerParser.get() != 0)
    header.reset(new MSK4ParserInternal::SubDocument(m_state->m_headerParser.get(), m_state->m_headerParser->getInput(), empty));
  if (m_state->m_footerParser.get() != 0)
    footer.reset(new MSK4ParserInternal::SubDocument(m_state->m_footerParser.get(), m_state->m_footerParser->getInput(), empty));

  // and the listener
  MWAWContentListenerPtr listener
    = m_state->m_mn0Parser->createListener(interface, header, footer);
  if (!listener) {
    MWAW_DEBUG_MSG(("MSK4Parser::parse: does not have listener\n"));
    throw(libmwaw::ParseException());
  }
  setListener(listener);
  m_listener->startDocument();
  m_state->m_mn0Parser->readContentZones(MWAWEntry(), true);

  try {
    flushExtra();
  } catch (...) { }
}

////////////////////////////////////////////////////////////
// create the ole structures
////////////////////////////////////////////////////////////
bool MSK4Parser::createStructures()
{
  assert(m_input.get());
  if (!checkHeader(m_header))
    throw libmwaw::ParseException();

  m_state->m_oleParser.reset(new MWAWOLEParser("MN0"));

  if (!m_state->m_oleParser->parse(m_input)) return false;

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

    bool ok = (dir == "" && base == "MN0"), mainOle = true;
    bool isFrame = false;
    if (!ok && dir == "MacWorks") {
      ok = (base == "QHdr" || base == "QFtr" || base == "QFootnotes");
      if (!ok && strncmp(base.c_str(),"QFrm",4)==0)
        ok = isFrame = true;
      mainOle = false;
    }
    if (!ok) {
      m_state->m_unparsedOlesName.push_back(name);
      continue;
    }

    MWAWInputStreamPtr ole = m_input->getDocumentOLEStream(name.c_str());
    if (!ole.get()) {
      MWAW_DEBUG_MSG(("Works4: error: can not find OLE part: \"%s\"\n", name.c_str()));
      continue;
    }

    shared_ptr<MSK4Zone> newParser(new MSK4Zone(ole, m_rsrcParser, m_header, *this, m_convertissor, name));
    try {
      ok = newParser->createZones(mainOle);
    } catch (...) {
      ok = false;
    }

    if (!ok) {
      MWAW_DEBUG_MSG(("MSK4Parser: error: can not parse OLE: \"%s\"\n", name.c_str()));
      continue;
    }

    if (mainOle) m_state->m_mn0Parser = newParser;
    else if (base == "QHdr") m_state->m_headerParser = newParser;
    else if (base == "QFtr") m_state->m_footerParser = newParser;
    else if (isFrame) {
      std::map<std::string, shared_ptr<MSK4Zone> >::iterator frameIt =
        m_state->m_frameParserMap.find(base);
      if (frameIt != m_state->m_frameParserMap.end()) {
        MWAW_DEBUG_MSG(("MSK4Parser: error: oops, I already find a frame zone %s\n", base.c_str()));
      } else
        m_state->m_frameParserMap[base] = newParser;
    } else if (base == "QFootnotes") m_state->m_footnoteParser = newParser;
  }

  return (m_state->m_mn0Parser.get() != 0);
}

////////////////////////////////////////////////////////////
// flush the extra data
////////////////////////////////////////////////////////////
void MSK4Parser::flushExtra()
{
  if (!m_listener) return;

  size_t numUnparsed = m_state->m_unparsedOlesName.size();
  if (numUnparsed == 0) return;

  bool first = true;
  for (size_t i = 0; i < numUnparsed; i++) {
    std::string const &name = m_state->m_unparsedOlesName[i];
    MWAWInputStreamPtr ole = m_input->getDocumentOLEStream(name.c_str());
    if (!ole.get()) {
      MWAW_DEBUG_MSG(("Works4: error: can not find OLE part: \"%s\"\n", name.c_str()));
      continue;
    }

    shared_ptr<MSK4Zone> newParser(new MSK4Zone(ole, m_rsrcParser, m_header, *this, m_convertissor, name));
    bool ok = true;
    try {
      ok = newParser->createZones(false);
      if (ok) {
        newParser->setListener(m_listener);
        // FIXME: add a message here
        if (first) {
          first = false;
          m_listener->setFont(MWAWFont(20,20));
          WPXString message = "--------- The original document has some extra ole: -------- ";
          m_listener->insertUnicodeString(message);
          m_listener->insertEOL();
        }
        newParser->readContentZones(MWAWEntry(), false);
      }
    } catch (...) {
      ok = false;
    }

    if (ok) continue;
    MWAW_DEBUG_MSG(("MSK4Parser: error: can not parse OLE: \"%s\"\n", name.c_str()));
  }
}

////////////////////////////////////////////////////////////
// basic check header function
////////////////////////////////////////////////////////////
bool MSK4Parser::checkHeader(MWAWHeader *header, bool /*strict*/)
{
  if (!m_input || !m_input->hasDataFork() || !m_input->isOLEStream())
    return false;

  MWAWInputStreamPtr mmOle = m_input->getDocumentOLEStream("MM");
  if (!mmOle || mmOle->readULong(2) != 0x444e) return false;

  MWAWInputStreamPtr mainOle = m_input->getDocumentOLEStream("MN0");
  if (!mainOle)
    return false;
  MWAW_DEBUG_MSG(("MWAWHeader::checkHeader: find a Microsoft Works 4.0 file\n"));
  if (header)
    header->reset(MWAWDocument::MSWORKS, 104);
  return true;
}

////////////////////////////////////////////////////////////
// subdocument helper
////////////////////////////////////////////////////////////
void MSK4Parser::sendFootNote(int id)
{
  if (!m_listener) return;

  MSK4Zone *parser = m_state->m_footnoteParser.get();

  if (!parser) {
    MWAW_DEBUG_MSG(("MSK4Parser::sendFootNote: can not find footnote ole\n"));
    MWAWSubDocumentPtr subdoc(new MSK4ParserInternal::SubDocument(0L, MWAWInputStreamPtr(), -1));
    m_listener->insertNote(MWAWContentListener::FOOTNOTE, subdoc);
    return;
  }

  MWAWSubDocumentPtr subdoc(new MSK4ParserInternal::SubDocument(parser, parser->getInput(), id));
  m_listener->insertNote(MWAWContentListener::FOOTNOTE, subdoc);
}

void MSK4Parser::sendFrameText(MWAWEntry const &entry, std::string const &frame)
{
  if (!m_listener) return;

  if (entry.length()==0) {
    m_listener->insertChar(' ');
    return;
  }

  MSK4Zone *parser = 0;
  std::map<std::string, shared_ptr<MSK4Zone> >::iterator frameIt =
    m_state->m_frameParserMap.find(frame);
  if (frameIt != m_state->m_frameParserMap.end())
    parser = frameIt->second.get();
  if (!parser || parser->getTextPosition().length() < entry.end()) {
    MWAW_DEBUG_MSG(("MSK4Parser::sendFrameText: can not find frame ole: %s\n", frame.c_str()));
    m_listener->insertChar(' ');
    return;
  }

  // ok, create the entry
  MWAWEntry ent(entry);
  ent.setBegin(entry.begin()+parser->getTextPosition().begin());
  parser->createZones(false);
  parser->readContentZones(ent, false);
}

void MSK4Parser::sendOLE(int id, MWAWPosition const &pictPos, WPXPropertyList extras)
{
  if (!m_listener) return;

  WPXBinaryData data;
  MWAWPosition pos;
  std::string type;
  if (!m_state->m_oleParser->getObject(id, data, pos, type)) {
    MWAW_DEBUG_MSG(("MSK4Parser::sendOLE: can not find OLE%d\n", id));
    return;
  }
  m_listener->insertPicture(pictPos, data, type, extras);
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
