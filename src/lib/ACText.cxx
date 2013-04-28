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

#include "ACParser.hxx"

#include "ACText.hxx"

/** Internal: the structures of a ACText */
namespace ACTextInternal
{
////////////////////////////////////////
//! Internal: the state of a ACText
struct State {
  //! constructor
  State() : m_version(-1), m_numPages(-1), m_actualPage(1) {
  }

  //! the file version
  mutable int m_version;
  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
};

////////////////////////////////////////
//! Internal: the subdocument of a ACText
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(ACText &pars, MWAWInputStreamPtr input, int id, MWAWEntry entry) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_textParser(&pars), m_id(id), m_pos(entry) {}

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
  ACText *m_textParser;
  //! the section id
  int m_id;
  //! the file pos
  MWAWEntry m_pos;
private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_textParser != sDoc->m_textParser) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_pos != sDoc->m_pos) return true;
  return false;
}

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_textParser);

  long pos = m_input->tell();
  m_input->seek(pos, WPX_SEEK_SET);
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
ACText::ACText(ACParser &parser) :
  m_parserState(parser.getParserState()), m_state(new ACTextInternal::State), m_mainParser(&parser)
{
}

ACText::~ACText()
{ }

int ACText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int ACText::numPages() const
{
  if (m_state->m_numPages >= 0)
    return m_state->m_numPages;
  // FIXME: compute the positions here
  return m_state->m_numPages;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

// find the different zones
bool ACText::createZones()
{
  MWAWRSRCParserPtr rsrcParser = m_mainParser->getRSRCParser();
  if (!rsrcParser) {
    MWAW_DEBUG_MSG(("ACText::createZones: can not find the entry map\n"));
    return false;
  }
  return true;
}

bool ACText::readLine(int id)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  int vers=version();
  long pos = input->tell();
  if (!m_mainParser->isFilePos(pos+18+4+((vers>=3) ? 4 : 0)))
    return false;
  f << "Entries(Header)[" << id << "]:";
  int depth=(int) input->readLong(2); // checkme
  int type=(int) input->readLong(2);
  if (depth <= 0 || type < 1 || type > 2) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  f << "depth=" << depth << ",";
  if (type==2) f << "graphic,";
  ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  input->seek(pos+18, WPX_SEEK_SET);

  bool ok = true;
  char const *(wh[3]) = { "Text", "Data1", "Data2" };
  for (int i=0; i < 3; i++) {
    pos = input->tell();
    long sz=long(input->readULong(4));
    if (sz < 0 || !m_mainParser->isFilePos(pos+4+sz)) {
      ok=false;
      break;
    }
    f.str("");
    f << "Entries(" << wh[i] << "):";
    input->seek(pos+4+sz, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    if (vers < 3 && i==0)
      break;
    if (type==2 && i==1)
      break;
  }
  if (!ok)
    input->seek(pos, WPX_SEEK_SET);
  return ok;
}

//////////////////////////////////////////////
// Fonts
//////////////////////////////////////////////

//////////////////////////////////////////////
// Styles
//////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the header/footer zone
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////
bool ACText::sendMainText()
{
  return false;
}

void ACText::flushExtra()
{
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
