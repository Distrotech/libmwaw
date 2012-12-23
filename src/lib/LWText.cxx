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
#include <map>
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWDebug.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPageSpan.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "LWParser.hxx"

#include "LWText.hxx"

/** Internal: the structures of a LWText */
namespace LWTextInternal
{
////////////////////////////////////////
//! Internal: the state of a LWText
struct State {
  //! constructor
  State() : m_version(-1), m_numPages(-1), m_actualPage(0) {
  }

  //! the file version
  mutable int m_version;

  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
};

////////////////////////////////////////
//! Internal: the subdocument of a LWText
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(LWText &pars, MWAWInputStreamPtr input, int id, libmwaw::SubDocumentType type) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_textParser(&pars), m_id(id), m_type(type) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  /** the text parser */
  LWText *m_textParser;
  //! the subdocument id
  int m_id;
  //! the subdocument type
  libmwaw::SubDocumentType m_type;
private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  LWContentListener *listen = dynamic_cast<LWContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
    return;
  }

  assert(m_textParser);

  long pos = m_input->tell();
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_textParser != sDoc->m_textParser) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_type != sDoc->m_type) return true;
  return false;
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
LWText::LWText(MWAWInputStreamPtr ip, LWParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert),
  m_state(new LWTextInternal::State), m_mainParser(&parser)
{
}

LWText::~LWText()
{ }

int LWText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_mainParser->version();
  return m_state->m_version;
}

int LWText::numPages() const
{
  if (m_state->m_numPages >= 0)
    return m_state->m_numPages;
  const_cast<LWText *>(this)->computePositions();
  return m_state->m_numPages;
}


void LWText::computePositions()
{
  int nPages = 1;
  m_state->m_actualPage = 1;
  m_state->m_numPages = nPages;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

// find the different zones
bool LWText::createZones()
{
  MWAWRSRCParserPtr rsrcParser = m_mainParser->getRSRCParser();
  if (!rsrcParser) {
    MWAW_DEBUG_MSG(("LWText::createZones: can not find the entry map\n"));
    return false;
  }
  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;

  // the different zones
  it = entryMap.lower_bound("styl");
  while (it != entryMap.end()) {
    if (it->first != "styl")
      break;

    MWAWEntry const &entry = it++->second;
    readFonts(entry);
  }

  it = entryMap.lower_bound("styu");
  while (it != entryMap.end()) {
    if (it->first != "styu")
      break;

    MWAWEntry const &entry = it++->second;
    readUnknownStyle(entry);
  }
  it = entryMap.lower_bound("styw");
  while (it != entryMap.end()) {
    if (it->first != "styw")
      break;

    MWAWEntry const &entry = it++->second;
    readUnknownStyle(entry);
  }
  it = entryMap.lower_bound("styx");
  while (it != entryMap.end()) {
    if (it->first != "styx")
      break;

    MWAWEntry const &entry = it++->second;
    readUnknownStyle(entry);
  }
  it = entryMap.lower_bound("styy");
  while (it != entryMap.end()) {
    if (it->first != "styy")
      break;

    MWAWEntry const &entry = it++->second;
    readUnknownStyle(entry);
  }

  // now update the different data
  computePositions();
  return true;
}

bool LWText::readFonts(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() < 2) {
    MWAW_DEBUG_MSG(("LWText::readFonts: the entry is bad\n"));
    return false;
  }
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(Fonts)[" << entry.id() << "]:";
  entry.setParsed(true);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  int fSz = N ? int((entry.length()-2)/N) : 0;
  if (long(N*fSz+2) != entry.length()) {
    MWAW_DEBUG_MSG(("LWText::readFonts: the number of entry seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << "Fonts-" << i << ":";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    input->seek(pos+fSz, WPX_SEEK_SET);
  }
  return true;
}

bool LWText::readUnknownStyle(MWAWEntry const &entry)
{
  int const numSz = (entry.type()=="styu" || entry.type()=="styy") ? 4 : 2;
  if (!entry.valid() || entry.length() < numSz) {
    MWAW_DEBUG_MSG(("LWText::readUnknownStyle: the entry is bad\n"));
    return false;
  }
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
  entry.setParsed(true);
  int N=(int) input->readULong(numSz);
  f << "N=" << N << ",";
  int fSz = N ? int((entry.length()-numSz)/N) : 0;
  if (long(N*fSz+numSz) != entry.length()) {
    MWAW_DEBUG_MSG(("LWText::readUnknownStyle: the number of entry seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << entry.type() << "-" << i << ":";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    input->seek(pos+fSz, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
//     Fonts
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//     Paragraphs
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

//! send data to the listener
bool LWText::sendMainText()
{
  if (!m_listener) return true;
  return true;
}


void LWText::flushExtra()
{
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
