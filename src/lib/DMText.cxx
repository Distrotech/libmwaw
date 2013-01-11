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

#include "DMParser.hxx"

#include "DMText.hxx"

/** Internal: the structures of a DMText */
namespace DMTextInternal
{
////////////////////////////////////////
//! Internal: the state of a DMText
struct State {
  //! constructor
  State() : m_version(-1), m_numPages(-1), m_actualPage(0) {
  }

  //! the file version
  mutable int m_version;

  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
};

////////////////////////////////////////
//! Internal: the subdocument of a DMText
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(DMText &pars, MWAWInputStreamPtr input, int id, libmwaw::SubDocumentType type) :
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
  DMText *m_textParser;
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
  DMContentListener *listen = dynamic_cast<DMContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
    return;
  }

  assert(m_textParser);

  long pos = m_input->tell();
  MWAW_DEBUG_MSG(("SubDocument::parse: oops do not know how to send this kind of document\n"));
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
DMText::DMText(MWAWInputStreamPtr ip, DMParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert),
  m_state(new DMTextInternal::State), m_mainParser(&parser)
{
}

DMText::~DMText()
{ }

int DMText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_mainParser->version();
  return m_state->m_version;
}

int DMText::numPages() const
{
  if (m_state->m_numPages >= 0)
    return m_state->m_numPages;
  const_cast<DMText *>(this)->computePositions();
  return m_state->m_numPages;
}


void DMText::computePositions()
{
  // first compute the number of page and the number of paragraph by pages
  int nPages = 1;
  m_state->m_actualPage = 1;
  m_state->m_numPages = nPages;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

// find the different zones
bool DMText::createZones()
{
  if (!m_mainParser->getRSRCParser()) {
    MWAW_DEBUG_MSG(("DMText::createZones: can not find the entry map\n"));
    return false;
  }
  std::multimap<std::string, MWAWEntry> &entryMap
    = m_mainParser->getRSRCParser()->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;

  // entry 128: font name and size
  it = entryMap.lower_bound("rQDF");
  while (it != entryMap.end()) {
    if (it->first != "rQDF")
      break;
    MWAWEntry const &entry = it++->second;
    std::string str;
    readFontNames(entry);
  }
  it = entryMap.lower_bound("foot");
  while (it != entryMap.end()) {
    if (it->first != "foot")
      break;
    MWAWEntry const &entry = it++->second;
    std::string str;
    readFooter(entry);
  }

  // now update the different data
  computePositions();
  return true;
}

////////////////////////////////////////////////////////////
//     Fonts
////////////////////////////////////////////////////////////
bool DMText::readFontNames(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<2) {
    MWAW_DEBUG_MSG(("DMText::readFontNames: the entry is bad\n"));
    return false;
  }
  entry.setParsed(true);
  long pos = entry.begin();
  long endPos = entry.end();
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  int N=(int) input->readULong(2);
  f << "Entries(FontName)[" << entry.type() << "-" << entry.id() << "]:N="<<N;
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  int val;
  for (int i = 0; i < N; i++) {
    f.str("");
    f << "FontName-" << i << ":";
    pos = input->tell();
    if (pos+1 > endPos) {
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("DMText::readFontNames: can not read fontname %d\n", i));
      return false;
    }
    int sz = (int)input->readULong(1);
    if (pos+1+sz+2 > endPos) {
      f.str("");
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("DMText::readFontNames: fontname size %d is bad\n", i));
      return false;
    }

    std::string str("");
    for (int c=0; c < sz; c++)
      str += (char) input->readULong(1);
    f << str << ",";

    val=(int) input->readULong(1);
    if (val) f << "unkn=" << val << ",";
    int N1=(int) input->readULong(1);
    if (pos+1+sz+2+N1 > endPos) {
      f.str("");
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("DMText::readFontNames: fontname size %d is bad\n", i));
      return false;
    }
    f << "fontSz=[";
    for (int j = 0; j < N1; j++)
      f << input->readULong(1) << ",";
    f << "],";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// the paragraphs
////////////////////////////////////////////////////////////

//     Footer
////////////////////////////////////////////////////////////
bool DMText::readFooter(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<22) {
    MWAW_DEBUG_MSG(("DMText::readFooter: the entry seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  long pos = entry.begin();
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(Footer)[" << entry.type() << "-" << entry.id() << "]:";
  int val=0;
  f << "items=["; // L TCR, B TCR item
  for (int i=0; i < 6; i++) {
    val = (int) input->readLong(2);
    if (val)
      f << val << ",";
    else
      f << "_,";
  }
  f << "],";
  for (int  i=0; i < 4; i++) { // f0=0|1, f1=f3=0, f2=1
    val = (int) input->readLong(1);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  for (int i = 0; i < 2; i++) { // fl0=0, fl1=3|14|7d7
    val = (int) input->readLong(2); // always 0
    if (val) f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  val = (int) input->readLong(2); // 9|a
  if (val) f << "f4=" << val << ",";
  if (input->tell()!=entry.end())
    ascFile.addDelimiter(input->tell(),'|');
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

//! send data to the listener
bool DMText::sendMainText()
{
  if (!m_listener) return true;
  return true;
}


void DMText::flushExtra()
{
  if (!m_listener) return;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
