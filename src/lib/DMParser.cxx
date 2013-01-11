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
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSubDocument.hxx"

#include "DMText.hxx"

#include "DMParser.hxx"

/** Internal: the structures of a DMParser */
namespace DMParserInternal
{
////////////////////////////////////////
//! Internal: the state of a DMParser
struct State {
  //! constructor
  State() : m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0) {
  }

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
DMParser::DMParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_listener(), m_convertissor(), m_state(),
  m_pageSpan(), m_textParser()
{
  init();
}

DMParser::~DMParser()
{
  if (m_listener.get()) m_listener->endDocument();
}

void DMParser::init()
{
  m_convertissor.reset(new MWAWFontConverter);
  m_listener.reset();

  m_state.reset(new DMParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  m_pageSpan.setMarginTop(0.1);
  m_pageSpan.setMarginBottom(0.1);
  m_pageSpan.setMarginLeft(0.1);
  m_pageSpan.setMarginRight(0.1);

  m_textParser.reset(new DMText(getInput(), *this, m_convertissor));
}

void DMParser::setListener(DMContentListenerPtr listen)
{
  m_listener = listen;
  m_textParser->setListener(listen);
}

MWAWInputStreamPtr DMParser::rsrcInput()
{
  return getRSRCParser()->getInput();
}

libmwaw::DebugFile &DMParser::rsrcAscii()
{
  return getRSRCParser()->ascii();
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
float DMParser::pageHeight() const
{
  return float(m_pageSpan.getFormLength()-m_pageSpan.getMarginTop()-m_pageSpan.getMarginBottom()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0);
}

float DMParser::pageWidth() const
{
  return float(m_pageSpan.getFormWidth()-m_pageSpan.getMarginLeft()-m_pageSpan.getMarginRight());
}

Vec2f DMParser::getPageLeftTop() const
{
  return Vec2f(float(m_pageSpan.getMarginLeft()),
               float(m_pageSpan.getMarginTop()+m_state->m_headerHeight/72.0));
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void DMParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!m_listener || m_state->m_actPage == 1)
      continue;
    m_listener->insertBreak(MWAW_PAGE_BREAK);
  }
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void DMParser::parse(WPXDocumentInterface *docInterface)
{
  assert(getInput().get() != 0 && getRSRCParser());

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = false;
  try {
    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      m_textParser->sendMainText();
#ifdef DEBUG
      m_textParser->flushExtra();
#endif
    }
    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("DMParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void DMParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (m_listener) {
    MWAW_DEBUG_MSG(("DMParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  int numPages = 1;
  if (m_textParser->numPages() > numPages)
    numPages = m_textParser->numPages();
  m_state->m_numPages = numPages;

  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan ps(m_pageSpan);
  for (int i = 0; i <= m_state->m_numPages; i++) pageList.push_back(ps);

  //
  DMContentListenerPtr listen(new DMContentListener(pageList, documentInterface));
  setListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool DMParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  std::string type, creator;
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;

  if (!m_textParser->createZones())
    return false;
  // the different pict zones
  it = entryMap.lower_bound("PICT");
  while (it != entryMap.end()) {
    if (it->first != "PICT")
      break;
    MWAWEntry const &entry = it++->second;
    WPXBinaryData data;
    rsrcParser->parsePICT(entry, data);
  }

  // entry 0: copyright
  it = entryMap.lower_bound("Dk@P");
  while (it != entryMap.end()) {
    if (it->first != "Dk@P")
      break;
    MWAWEntry const &entry = it++->second;
    std::string str;
    rsrcParser->parseSTR(entry, str);
  }
  // entry 128: ?
  it = entryMap.lower_bound("clut");
  while (it != entryMap.end()) {
    if (it->first != "clut")
      break;
    MWAWEntry const &entry = it++->second;
    std::vector<MWAWColor> cmap;
    rsrcParser->parseClut(entry, cmap);
  }
  it = entryMap.lower_bound("sTwD");
  while (it != entryMap.end()) {
    if (it->first != "sTwD")
      break;
    MWAWEntry const &entry = it++->second;
    readSTwD(entry);
  }
  it = entryMap.lower_bound("xtr2");
  while (it != entryMap.end()) {
    if (it->first != "xtr2")
      break;
    MWAWEntry const &entry = it++->second;
    readXtr2(entry);
  }
  // entry 128 and following: ?
  it = entryMap.lower_bound("Wndo");
  while (it != entryMap.end()) {
    if (it->first != "Wndo")
      break;
    MWAWEntry const &entry = it++->second;
    readWndo(entry);
  }
  //  1000:docname, 1001:footer name, 2001-... chapter name, others ...
  it = entryMap.lower_bound("STR ");
  while (it != entryMap.end()) {
    if (it->first != "STR ")
      break;
    MWAWEntry const &entry = it++->second;
    std::string str;
    rsrcParser->parseSTR(entry, str);
  }

#ifdef DEBUG_WITH_FILES
  // get rid of the default application resource
  libmwaw::DebugFile &ascFile = rsrcAscii();
  static char const *(appliRsrc[])= {
    // default
    "ALRT","BNDL","CNTL","CURS","CDEF", "DLOG","DITL","FREF","ICON","ICN#", "MENU",
    "crsr","dctb","icl4","icl8","ics4", "ics8","ics#","snd ",
    // local
    "mstr" /* menu string */
  };
  for (int r=0; r < 19+1; r++) {
    it = entryMap.lower_bound(appliRsrc[r]);
    while (it != entryMap.end()) {
      if (it->first != appliRsrc[r])
        break;
      MWAWEntry const &entry = it++->second;
      if (entry.isParsed()) continue;
      entry.setParsed(true);
      ascFile.skipZone(entry.begin()-4,entry.end()-1);
    }
  }
#endif
  return false;
}


////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read some unknown zone
////////////////////////////////////////////////////////////
bool DMParser::readSTwD(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<10) {
    MWAW_DEBUG_MSG(("DMText::readSTwD: the entry seems very short\n"));
    return false;
  }

  entry.setParsed(true);
  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(STwD)[" << entry.type() << "-" << entry.id() << "]:";
  int val;
  for (int i=0; i < 2; i++) { // f0=2, f1=1|2
    val =(int) input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  int flag =(int) input->readLong(2); // 320|7d0|1388 ?
  f << "fl=" << std::hex << flag << std::dec << ",";
  f << "dim=" << (int) input->readLong(2) << ","; // 0x1Fa|0x200
  for (int i=0; i < 2; i++) { // f0=1, f1=0|1
    val =(int) input->readLong(1);
    if (val)
      f << "f" << i+2 << "=" << val << ",";
  }
  f << "],";
  if (input->tell()!=entry.end())
    ascFile.addDelimiter(input->tell(),'|');
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool DMParser::readWndo(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<20) {
    MWAW_DEBUG_MSG(("DMText::readWndo: the entry seems very short\n"));
    return false;
  }

  entry.setParsed(true);
  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(Wndo)[" << entry.type() << "-" << entry.id() << "]:";
  int val=(int) input->readLong(2); // always 0?
  if (val) f << "unkn=" << val << ",";
  // f0=12|24, f1=4|7, f2=f0?, f3=e|36, f4=1d|36, f5=f1?, f6=f3?
  for (int i=0; i < 7; i++) {
    val =(int) input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  int flag =(int) input->readLong(2); //9|3e|6d|a8|13e|16d
  f << "fl=" << std::hex << flag << std::dec << ",";
  val = (int) input->readLong(2); // always 0?
  if (val) f << "unkn1=" << val << ",";

  if (input->tell()!=entry.end())
    ascFile.addDelimiter(input->tell(),'|');
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool DMParser::readXtr2(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<1) {
    MWAW_DEBUG_MSG(("DMText::readXtr2: the entry seems very short\n"));
    return false;
  }

  entry.setParsed(true);
  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(Xtr2)[" << entry.type() << "-" << entry.id() << "]:";
  int N=1;
  if (entry.length() != 1) {
    MWAW_DEBUG_MSG(("DMText::readXtr2: find more than one flag\n"));
    N = entry.length()>20 ? 20 : int(entry.length());
  }
  // f0=79|a8|b9|99
  int val;
  for (int i=0; i < N; i++) {
    val =(int) input->readULong(1);
    if (val)
      f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  if (input->tell()!=entry.end())
    ascFile.addDelimiter(input->tell(),'|');
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool DMParser::checkHeader(MWAWHeader *header, bool /*strict*/)
{
  *m_state = DMParserInternal::State();
  /** no data fork, may be ok, but this means
      that the file contains no text, so... */
  MWAWInputStreamPtr input = getInput();
  if (!input || !getRSRCParser())
    return false;
  if (input->hasDataFork()) {
    MWAW_DEBUG_MSG(("DMParser::checkHeader: find a datafork, odd!!!\n"));
  }
  MWAWRSRCParser::Version vers;
  // read the Docmaker version
  int docmakerVersion = -1;
  MWAWEntry entry = getRSRCParser()->getEntry("vers", 2);
  if (entry.valid() && getRSRCParser()->parseVers(entry, vers))
    docmakerVersion = vers.m_majorVersion;
  else if (docmakerVersion==-1) {
    MWAW_DEBUG_MSG(("DMParser::checkHeader: can not find the DocMaker version\n"));
  }
  setVersion(vers.m_majorVersion);
  if (header)
    header->reset(MWAWDocument::DM, version());

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
