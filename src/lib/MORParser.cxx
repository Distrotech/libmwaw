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
#include <set>
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "MORText.hxx"

#include "MORParser.hxx"

/** Internal: the structures of a MORParser */
namespace MORParserInternal
{
////////////////////////////////////////
//! Internal: the state of a MORParser
struct State {
  //! constructor
  State() : m_eof(-1), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0) {
  }

  //! end of file
  long m_eof;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MORParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MORParser &pars, MWAWInputStreamPtr input) :
    MWAWSubDocument(&pars, input, MWAWEntry()) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const {
    if (MWAWSubDocument::operator!=(doc)) return true;
    SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
    if (!sDoc) return true;
    return false;
  }

  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("MORParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_parser);
  //static_cast<MORParser *>(m_parser)->sendHeaderFooter();
}
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MORParser::MORParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_state(), m_textParser()
{
  init();
}

MORParser::~MORParser()
{
}

void MORParser::init()
{
  resetListener();
  setAsciiName("main-1");

  m_state.reset(new MORParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);

  m_textParser.reset(new MORText(*this));
}

MWAWInputStreamPtr MORParser::rsrcInput()
{
  return getRSRCParser()->getInput();
}

libmwaw::DebugFile &MORParser::rsrcAscii()
{
  return getRSRCParser()->ascii();
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
Vec2f MORParser::getPageLeftTop() const
{
  return Vec2f(float(getPageSpan().getMarginLeft()),
               float(getPageSpan().getMarginTop()+m_state->m_headerHeight/72.0));
}

bool MORParser::isFilePos(long pos)
{
  if (pos <= m_state->m_eof)
    return true;

  MWAWInputStreamPtr input = getInput();
  long actPos = input->tell();
  input->seek(pos, WPX_SEEK_SET);
  bool ok = long(input->tell()) == pos;
  if (ok) m_state->m_eof = pos;
  input->seek(actPos, WPX_SEEK_SET);
  return ok;
}

////////////////////////////////////////////////////////////
// interface with the text parser
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void MORParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!getListener() || m_state->m_actPage == 1)
      continue;
    getListener()->insertBreak(MWAWContentListener::PageBreak);
  }
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MORParser::parse(WPXDocumentInterface *docInterface)
{
  assert(getInput().get() != 0);
  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = false;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      m_textParser->sendMainText();
    }
    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("MORParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MORParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getListener()) {
    MWAW_DEBUG_MSG(("MORParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  int numPages = 1;
  if (m_textParser->numPages() > numPages)
    numPages = m_textParser->numPages();
  m_state->m_numPages = numPages;

  MWAWPageSpan ps(getPageSpan());
  ps.setPageSpan(m_state->m_numPages+1);

  std::vector<MWAWPageSpan> pageList(1,ps);
  MWAWContentListenerPtr listen(new MWAWContentListener(*getParserState(), pageList, documentInterface));
  setListener(listen);
  listen->startDocument();
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool MORParser::createZones()
{
  int vers=version();
  MWAWInputStreamPtr input = getInput();
  if (vers<2) {
    MWAW_DEBUG_MSG(("MORParser::createZones: do not know how to createZone for v1\n"));
    return false;
  }
  long pos=8;
  if (!isFilePos(0x80)) {
    MWAW_DEBUG_MSG(("MORParser::createZones: file is too short\n"));
    return false;
  }
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(Zones):";
  for (int i=0; i < 8; i++) {
    /* 0: printer
       4: list of pointer + ?
       6: list of pointer
     */
    MWAWEntry entry;
    entry.setBegin((long) input->readULong(4));
    entry.setLength((long) input->readULong(4));
    std::string name("");
    switch(i) {
    case 0:
      name="Printer";
      break;
    default: {
      std::stringstream s;
      s << "Unknown" << i;
      name=s.str();
    }
    break;
    }
    entry.setType(name);
    if (!entry.length())
      continue;
    f << name << "(" << std::hex << entry.begin() << "<->" << entry.end()
      << std::dec <<  "), ";
    if (!isFilePos(entry.end())) {
      MWAW_DEBUG_MSG(("MORParser::createZones: can not read entry %d\n", i));
      f << "###";
      continue;
    }

    libmwaw::DebugStream f2;
    f2 << "Entries(" << entry.type() << "):";
    ascii().addPos(entry.begin());
    ascii().addNote(f2.str().c_str());
    ascii().addPos(entry.end());
    ascii().addNote("_");
  }
  long unkn=(long) input->readULong(4);
  if (!isFilePos(unkn)) {
    MWAW_DEBUG_MSG(("MORParser::createZones: can not read unkn limit\n"));
    f << "###";
  }
  if (unkn) f << "unkn=" << std::hex << unkn << std::dec << ",";
  // checkme, only find UnknA1 and UnknA2 other 0 so?
  for (int i=0; i < 6; i++) {
    MWAWEntry entry;
    entry.setBegin((long) input->readULong(4));
    entry.setLength((long) input->readULong(4));
    std::string name("");
    std::stringstream s;
    s << "UnknA" << i;
    name=s.str();

    entry.setType(name);
    if (!entry.length())
      continue;
    f << name << "(" << std::hex << entry.begin() << "<->" << entry.end()
      << std::dec <<  "), ";
    if (!isFilePos(entry.end())) {
      MWAW_DEBUG_MSG(("MORParser::createZones: can not read entryA %d\n", i));
      f << "###";
      continue;
    }

    libmwaw::DebugStream f2;
    f2 << "Entries(" << entry.type() << "):";
    ascii().addPos(entry.begin());
    ascii().addNote(f2.str().c_str());
    ascii().addPos(entry.end());
    ascii().addNote("_");
  }
  unkn=(long) input->readULong(4); // always 0?
  if (unkn) f << "unkn2=" << std::hex << unkn << std::dec << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  ascii().addDelimiter(input->tell(),'|');
  return m_textParser->createZones();
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MORParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MORParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !isFilePos(0x80))
    return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";
  input->seek(0,WPX_SEEK_SET);
  int val=(int) input->readLong(2);
  int vers;
  switch (val) {
  case 3:
    vers=2;
    if (input->readULong(4)!=0x4d524949) // MRII
      return false;
    break;
  case 6:
    vers=3;
    if (input->readULong(4)!=0x4d4f5233) // MOR3
      return false;
    break;
  default:
    return -1;
  }
  setVersion(vers);
  val=(int) input->readLong(2);
  if (val!=0x80) {
    if (strict)
      return false;
    f << "f0=" << std::hex << val << std::dec << ",";
  }
  if (header)
    header->reset(MWAWDocument::MORE, vers);
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());
  return true;
}



// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
