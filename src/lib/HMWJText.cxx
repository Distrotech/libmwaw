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
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "HMWJParser.hxx"

#include "HMWJText.hxx"

/** Internal: the structures of a HMWJText */
namespace HMWJTextInternal
{
////////////////////////////////////////
//! Internal: the state of a HMWJText
struct State {
  //! constructor
  State() : m_version(-1), m_numPages(-1), m_actualPage(0) {
  }

  //! the file version
  mutable int m_version;
  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
};

////////////////////////////////////////
//! Internal: the subdocument of a HMWJText
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(HMWJText &pars, MWAWInputStreamPtr input, int id, libmwaw::SubDocumentType type) :
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
  HMWJText *m_textParser;
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
HMWJText::HMWJText(HMWJParser &parser) :
  m_parserState(parser.getParserState()), m_state(new HMWJTextInternal::State), m_mainParser(&parser)
{
}

HMWJText::~HMWJText()
{
}

int HMWJText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int HMWJText::numPages() const
{
  return m_state->m_numPages;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//     Text
////////////////////////////////////////////////////////////
bool HMWJText::sendText(long /*id*/, long /*subId*/)
{
  return false;
}

////////////////////////////////////////////////////////////
//     Fonts
////////////////////////////////////////////////////////////


// the list of fonts
bool HMWJText::readFontNames(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("HMWJText::readFontNames: called without any entry\n"));
    return false;
  }
  if (entry.length() < 28) {
    MWAW_DEBUG_MSG(("HMWJText::readFontNames: the entry seems too short\n"));
    return false;
  }
  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  entry.setParsed(true);

  f << entry.name() << "[data]:";

  long pos = entry.begin()+8; // skip header
  input->seek(pos, WPX_SEEK_SET);
  int N, val;
  long readDataSz = (long) input->readULong(4);
  if (readDataSz+12 != entry.length()) {
    MWAW_DEBUG_MSG(("HMWJText::readFontNames: the data size seems odd\n"));
    f << "##dataSz=" << readDataSz << ",";
  }
  N = (int) input->readLong(2);
  f << "N=" << N << ",";
  long fieldSz =  (long) input->readULong(4);
  if (fieldSz != 68) {
    MWAW_DEBUG_MSG(("HMWJText::readFontNames: the field size seems odd\n"));
    f << "##fieldSz=" << fieldSz << ",";
  }
  for (int i = 0; i < 5; i++) { //f1=f2=1
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }

  long expectedSz = N*68+28;
  if (expectedSz != entry.length() && expectedSz+1 != entry.length()) {
    MWAW_DEBUG_MSG(("HMWJText::readFontNames: the entry size seems odd\n"));
    return false;
  }
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << entry.name() << "-" << i << ":";
    int fId = (int) input->readLong(2);
    f << "fId=" << fId << ",";
    val = (int) input->readLong(2);
    if (val != fId)
      f << "#fId2=" << val << ",";
    int fSz = (int) input->readULong(1);
    if (fSz+5 > 68) {
      f << "###fSz";
      MWAW_DEBUG_MSG(("HMWJText::readFontNames: can not read a font\n"));
    } else {
      std::string name("");
      for (int c = 0; c < fSz; c++)
        name += (char) input->readULong(1);
      f << name;
      m_parserState->m_fontConverter->setCorrespondance(fId, name);
    }
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+68, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
//     Style
////////////////////////////////////////////////////////////
bool HMWJText::readStyles(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("HMWJText::readStyles: called without any zone\n"));
    return false;
  }

  long dataSz = entry.length();
  if (dataSz < 4) {
    MWAW_DEBUG_MSG(("HMWJText::readStyles: the zone seems too short\n"));
    return false;
  }
  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  entry.setParsed(true);

  f << entry.name() << "[header]:";

  long pos = entry.begin()+8; // skip header
  long endPos = entry.end();
  input->seek(pos, WPX_SEEK_SET);
  int N, val;

  long headerSz = (long) input->readULong(4);
  if (headerSz < 0 || headerSz+4 > dataSz) {
    MWAW_DEBUG_MSG(("HMWJText::readStyles: the header size seems odd\n"));
    return false;
  }
  N = (int) input->readULong(2);
  if (headerSz+4+4*N > dataSz || headerSz < 4*N+16) {
    MWAW_DEBUG_MSG(("HMWJText::readStyles: the number of style seems odd\n"));
    if (headerSz+4+4*N > dataSz)
      return false;
  }
  for (int i = 0; i < 7; i++) {
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  f << "unkn=[";
  for (int i = 0; i < N; i++) { /* (1d:*)* */
    f << input->readLong(2) << ":";
    f << std::hex << input->readULong(2) << std::dec << ",";
  }
  f << "],";
  input->seek(pos+4+headerSz, WPX_SEEK_SET);
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    f.str("");
    f << entry.name() << "-" << i << ":";
    pos = input->tell();
    long fieldSz = (long) input->readULong(4)+4;
    if (fieldSz < 0x1bc || pos+fieldSz > endPos) {
      f << "###";
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("HMWJText::readStyles: can not read field %d\n", i));
      return true;
    }
    val = (int) input->readULong(1);
    if (val != i) f << "#id=" << val << ",";

    // f0=c2|c6, f2=0|44, f3=1|14|15|16: fontId?
    for (int j=0; j < 5; j++) {
      val = (int) input->readULong(1);
      if (val)
        f << "f" << j << "=" << std::hex << val << std::dec << ",";
    }
    /* g1=9|a|c|e|12|18: size ?, g5=1, g8=0|1, g25=0|18|30|48, g31=1, g35=0|1 */
    for (int j=0; j < 33; j++) {
      val = (int) input->readULong(2);
      if (val)
        f << "g" << j << "=" << val  << ",";
    }
    for (int j=0; j < 4; j++) { // b,b,b,0
      val = (int) input->readULong(1);
      if ((j < 3 && val != 0xb) || (j==3 && val))
        f << "h" << j << "=" << val  << ",";
    }

    for (int j=0; j < 17; j++) { // always 0
      val = (int) input->readULong(2);
      if (val)
        f << "l" << j << "=" << val  << ",";
    }
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    long pos2 = input->tell();
    f.str("");
    f << entry.name() << "-" << i << "[B]:";
    // checkme probably f15=numTabs  ..
    for (int j = 0; j < 50; j++) {
      val = (int) input->readULong(2);
      if ((j < 5 && val != 1) || (j >= 5 && val))
        f << "f" << j << "=" << val  << ",";
    }
    for (int j = 0; j < 50; j++) {
      val = (int) input->readULong(2);
      if (val)
        f << "g" << j << "=" << val  << ",";
    }
    for (int j = 0; j < 43; j++) {
      val = (int) input->readULong(2);
      if (val)
        f << "h" << j << "=" << val  << ",";
    }
    asciiFile.addPos(pos2);
    asciiFile.addNote(f.str().c_str());

    pos2 = input->tell();
    f.str("");
    f << entry.name() << "-" << i << "[C]:";
    val = (int) input->readLong(2);
    if (val != -1) f << "unkn=" << val << ",";
    val  = (int) input->readLong(2);
    if (val != i) f << "#id" << val << ",";
    for (int j = 0; j < 4; j++) {
      val = (int) input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    int fSz = (int) input->readULong(1);
    if (input->tell()+fSz > pos+fieldSz) {
      MWAW_DEBUG_MSG(("HMWJText::readStyles: can not read styleName\n"));
      f << "###";
    } else {
      std::string name("");
      for (int j = 0; j < fSz; j++)
        name +=(char) input->readULong(1);
      f << name;
    }
    asciiFile.addPos(pos2);
    asciiFile.addNote(f.str().c_str());
    if (input->tell() != pos+fieldSz)
      asciiFile.addDelimiter(input->tell(),'|');

    input->seek(pos+fieldSz, WPX_SEEK_SET);
  }

  if (!input->atEOS()) {
    asciiFile.addPos(input->tell());
    asciiFile.addNote("_");
  }
  return true;
}

////////////////////////////////////////////////////////////
//     Paragraph
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//     the token
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//     the sections
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

//! send data to the listener

void HMWJText::flushExtra()
{
  if (!m_parserState->m_listener) return;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
