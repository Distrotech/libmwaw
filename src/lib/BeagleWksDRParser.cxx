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

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWHeader.hxx"
#include "MWAWPictBitmap.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWGraphicListener.hxx"
#include "MWAWSubDocument.hxx"

#include "BeagleWksStructManager.hxx"

#include "BeagleWksDRParser.hxx"

/** Internal: the structures of a BeagleWksDRParser */
namespace BeagleWksDRParserInternal
{
////////////////////////////////////////
//! Internal: a bitmap of a BeagleWksDRParser
struct Bitmap {
  //! the constructor
  Bitmap() : m_colorBytes(1), m_dim(0,0), m_colorList()
  {
  }
  //! the number of color bytes (1: means black/white, 8: means color)
  int m_colorBytes;
  //! the bitmap dimension
  Vec2i m_dim;
  //! the color list
  std::vector<MWAWColor> m_colorList;
};

////////////////////////////////////////
//! Internal: the state of a BeagleWksDRParser
struct State {
  //! constructor
  State() :  m_graphicBegin(-1), m_typeEntryMap(),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }

  /** the graphic begin position */
  long m_graphicBegin;
  /** the type entry map */
  std::multimap<std::string, MWAWEntry> m_typeEntryMap;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
BeagleWksDRParser::BeagleWksDRParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWGraphicParser(input, rsrcParser, header), m_state(), m_structureManager()
{
  init();
}

BeagleWksDRParser::~BeagleWksDRParser()
{
}

void BeagleWksDRParser::init()
{
  resetGraphicListener();
  setAsciiName("main-1");

  m_state.reset(new BeagleWksDRParserInternal::State);
  m_structureManager.reset(new BeagleWksStructManager(getParserState()));

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}

MWAWInputStreamPtr BeagleWksDRParser::rsrcInput()
{
  return getRSRCParser()->getInput();
}

libmwaw::DebugFile &BeagleWksDRParser::rsrcAscii()
{
  return getRSRCParser()->ascii();
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
Vec2f BeagleWksDRParser::getPageLeftTop() const
{
  return Vec2f(float(getPageSpan().getMarginLeft()),
               float(getPageSpan().getMarginTop()+m_state->m_headerHeight/72.0));
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void BeagleWksDRParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!getGraphicListener() || m_state->m_actPage == 1)
      continue;
    getGraphicListener()->insertBreak(MWAWGraphicListener::PageBreak);
  }
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void BeagleWksDRParser::parse(librevenge::RVNGDrawingInterface *docInterface)
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
      sendPageFrames();
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetGraphicListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void BeagleWksDRParser::createDocument(librevenge::RVNGDrawingInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getGraphicListener()) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  int numPages = 1;
  m_state->m_numPages = numPages;

  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan ps(getPageSpan());
  ps.setPageSpan(numPages);
  pageList.push_back(ps);

  MWAWGraphicListenerPtr listen(new MWAWGraphicListener(*getParserState(), pageList, documentInterface));
  setGraphicListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool BeagleWksDRParser::createZones()
{
  readRSRCZones();
  MWAWInputStreamPtr input = getInput();
  if (input->seek(66, librevenge::RVNG_SEEK_SET) || !readPrintInfo())
    return false;
  long pos = input->tell();
  if (!input->checkPosition(pos+70)) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::createZones: the file can not contains zones\n"));
    return false;
  }

  // now read the list of zones
  libmwaw::DebugStream f;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(Zones):";
  for (int i=0; i<7; ++i) { // checkme: at least 2 zones, i=2 is not a zone, maybe 7
    MWAWEntry entry;
    entry.setBegin(input->readLong(4));
    entry.setLength(input->readLong(4));
    entry.setId((int) input->readLong(2));
    if (entry.length()==0) continue;
    entry.setType(i==1?"Frame":"Unknown");
    f << entry.type() << "[" << entry.id() << "]="
      << std::hex << entry.begin() << "<->" << entry.end() << ",";
    if (!entry.valid() || !input->checkPosition(entry.end())) {
      f << "###";
      if (i<2) {
        MWAW_DEBUG_MSG(("BeagleWksDRParser::createZones: can not read the header zone, stop\n"));
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        return false;
      }
      if (i!=2) {
        MWAW_DEBUG_MSG(("BeagleWksDRParser::createZones: can not zones entry %d\n",i));
      }
      continue;
    }
    m_state->m_typeEntryMap.insert
    (std::multimap<std::string, MWAWEntry>::value_type(entry.type(),entry));
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // now parse the different zones
  std::multimap<std::string, MWAWEntry>::iterator it;
  it=m_state->m_typeEntryMap.find("FontNames");
  if (it!=m_state->m_typeEntryMap.end())
    m_structureManager->readFontNames(it->second);
  it=m_state->m_typeEntryMap.find("Frame");
  if (it!=m_state->m_typeEntryMap.end())
    m_structureManager->readFrame(it->second);

  // now parse the different zones
  for (it=m_state->m_typeEntryMap.begin(); it!=m_state->m_typeEntryMap.end(); ++it) {
    MWAWEntry const &entry=it->second;
    if (entry.isParsed())
      continue;
    f.str("");
    f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
    ascii().addPos(entry.end());
    ascii().addNote("_");
  }

  input->seek(m_state->m_graphicBegin, librevenge::RVNG_SEEK_SET);
  pos=input->tell();
  if (!readGraphicHeader()) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::createZones: can not read the graphic header\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(DrHeader):###");
    return false;
  }
  // DOME now read the shape
  if (!input->isEnd()) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::createZones: find some extra data\n"));
    ascii().addPos(input->tell());
    ascii().addNote("Entries(ZoneEnd)");
  }
  return false;
}

bool BeagleWksDRParser::readRSRCZones()
{
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  if (!rsrcParser)
    return true;

  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;
  // the 1 zone
  char const *(zNames[]) = {"wPos", "DMPF" };
  for (int z = 0; z < 2; ++z) {
    it = entryMap.lower_bound(zNames[z]);
    while (it != entryMap.end()) {
      if (it->first != zNames[z])
        break;
      MWAWEntry const &entry = it++->second;
      switch (z) {
      case 0: // 1001
        m_structureManager->readwPos(entry);
        break;
      case 1: // find in one file with id=4661 6a1f 4057
        m_structureManager->readFontStyle(entry);
        break;
      /* find also
         - edpt: see sendPicture
         - DMPP: the paragraph style
         - sect and alis: position?, alis=filesystem alias(dir, filename, path...)
      */
      default:
        break;
      }
    }
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the graphic data
////////////////////////////////////////////////////////////
bool BeagleWksDRParser::readGraphicHeader()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (!input->checkPosition(pos+112))
    return false;
  libmwaw::DebugStream f;
  f << "Entries(DrHeader):";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+112, librevenge::RVNG_SEEK_SET);

  if (!readPatterns() || !readColors() || !readZoneA()) return false;
  return true;
}

//  read the colors
bool BeagleWksDRParser::readColors()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Color):";
  if (!input->checkPosition(pos+16)) {
    f << "###";
    MWAW_DEBUG_MSG(("BeagleWksDRParser::readColors: the header size seems too short\n"));
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int maxN=(int)input->readULong(2);
  f << "N0=" << maxN << ",";
  int val=(int)input->readULong(2);
  if (val!=maxN) f << "N1=" << val << ",";
  if (val>maxN) maxN=val;
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  val=(int)input->readULong(2);
  if (val!=6) f << "f0=" << val << ",";
  val=(int)input->readULong(2);
  if (val!=maxN) f << "N2=" << val << ",";
  if (val>maxN) maxN=val;
  int fSz=(int) input->readULong(2);
  f << "fSz=" << fSz << ",";
  long dSz=(long) input->readULong(4);
  if (!input->checkPosition(pos+16+dSz) || fSz<10 || dSz!=N*fSz) {
    f << "###";
    MWAW_DEBUG_MSG(("BeagleWksDRParser::readColors: the color size seems bad\n"));
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    if (i>=maxN) {
      ascii().addPos(pos);
      ascii().addNote("_");
      input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
      continue;
    }
    f.str("");
    f << "Color-" << i << ":";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

//  read the patterns
bool BeagleWksDRParser::readPatterns()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Pattern):";
  if (!input->checkPosition(pos+16)) {
    f << "###";
    MWAW_DEBUG_MSG(("BeagleWksDRParser::readPatterns: the header size seems too short\n"));
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int maxN=(int)input->readULong(2);
  f << "N0=" << maxN << ",";
  int val=(int)input->readULong(2);
  if (val!=maxN) f << "N1=" << val << ",";
  if (val>maxN) maxN=val;
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  val=(int)input->readULong(2);
  if (val!=6) f << "f0=" << val << ",";
  val=(int)input->readULong(2);
  if (val!=maxN) f << "N2=" << val << ",";
  if (val>maxN) maxN=val;
  int fSz=(int) input->readULong(2);
  f << "fSz=" << fSz << ",";
  long dSz=(long) input->readULong(4);
  if (!input->checkPosition(pos+16+dSz) || fSz<10 || dSz!=N*fSz) {
    f << "###";
    MWAW_DEBUG_MSG(("BeagleWksDRParser::readPatterns: the pattern size seems bad\n"));
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    if (i>=maxN) {
      ascii().addPos(pos);
      ascii().addNote("_");
      input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
      continue;
    }
    f.str("");
    f << "Pattern-" << i << ":";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

//  read a unknown zone
bool BeagleWksDRParser::readZoneA()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;
  f << "Entries(ZoneA):";
  if (!input->checkPosition(pos+16)) {
    f << "###";
    MWAW_DEBUG_MSG(("BeagleWksDRParser::readZoneA: the header size seems too short\n"));
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int maxN=(int)input->readULong(2);
  f << "N0=" << maxN << ",";
  int val=(int)input->readULong(2);
  if (val!=maxN) f << "N1=" << val << ",";
  if (val>maxN) maxN=val;
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  val=(int)input->readULong(2);
  if (val!=6) f << "f0=" << val << ",";
  val=(int)input->readULong(2);
  if (val!=maxN) f << "N2=" << val << ",";
  if (val>maxN) maxN=val;
  int fSz=(int) input->readULong(2);
  f << "fSz=" << fSz << ",";
  long dSz=(long) input->readULong(4);
  if (!input->checkPosition(pos+16+dSz) || fSz<0x3c || dSz!=fSz*N) {
    f << "###";
    MWAW_DEBUG_MSG(("BeagleWksDRParser::readZoneA: the pattern size seems bad\n"));
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    if (i>=maxN) {
      ascii().addPos(pos);
      ascii().addNote("_");
      input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
      continue;
    }
    f.str("");
    f << "ZoneA-" << i << ":";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool BeagleWksDRParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (!input->checkPosition(pos+0x70))
    return false;

  libmwaw::DebugStream f;
  // print info
  libmwaw::PrinterInfo info;
  if (!info.read(input)) return false;
  f << "Entries(PrintInfo):"<< info;

  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  // define margin from print info
  Vec2i lTopMargin= -1 * info.paper().pos(0);
  Vec2i rBotMargin=info.paper().pos(1) - info.page().pos(1);

  // move margin left | top
  int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
  int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
  lTopMargin -= Vec2i(decalX, decalY);
  rBotMargin += Vec2i(decalX, decalY);

  // decrease right | bottom
  int rightMarg = rBotMargin.x() -10;
  if (rightMarg < 0) rightMarg=0;
  int botMarg = rBotMargin.y() -50;
  if (botMarg < 0) botMarg=0;

  getPageSpan().setMarginTop(lTopMargin.y()/72.0);
  getPageSpan().setMarginBottom(botMarg/72.0);
  getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
  getPageSpan().setMarginRight(rightMarg/72.0);
  getPageSpan().setFormLength(paperSize.y()/72.);
  getPageSpan().setFormWidth(paperSize.x()/72.);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+0x78, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != pos+0x78) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::readPrintInfo: file is too short\n"));
    return false;
  }
  ascii().addPos(input->tell());

  return true;
}

////////////////////////////////////////////////////////////
// send data
////////////////////////////////////////////////////////////
bool BeagleWksDRParser::sendPageFrames()
{
  std::map<int, BeagleWksStructManager::Frame> const &frameMap = m_structureManager->getIdFrameMap();
  std::map<int, BeagleWksStructManager::Frame>::const_iterator it;
  for (it=frameMap.begin(); it!=frameMap.end(); ++it)
    sendFrame(it->second);
  return true;
}

bool BeagleWksDRParser::sendFrame(BeagleWksStructManager::Frame const &frame)
{
  MWAWPosition fPos(Vec2f(0,0), frame.m_dim, librevenge::RVNG_POINT);

  fPos.setPagePos(frame.m_page > 0 ? frame.m_page : 1, frame.m_origin);
  fPos.setRelativePosition(MWAWPosition::Page);
  fPos.m_wrapping = frame.m_wrap==0 ? MWAWPosition::WNone : MWAWPosition::WDynamic;

  MWAWGraphicStyle style=MWAWGraphicStyle::emptyStyle();
  style.setBorders(frame.m_bordersSet, frame.m_border);
  return sendPicture(frame.m_pictId, fPos, style);
}

// read/send picture (edtp resource)
bool BeagleWksDRParser::sendPicture
(int pId, MWAWPosition const &pictPos, MWAWGraphicStyle const &style)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::sendPicture: can not find the listener\n"));
    return false;
  }
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  if (!rsrcParser) {
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("BeagleWksDRParser::sendPicture: need access to resource fork to retrieve picture content\n"));
      first=false;
    }
    return true;
  }

  librevenge::RVNGBinaryData data;
  if (!m_structureManager->readPicture(pId, data))
    return false;

  listener->insertPicture(pictPos, data, "image/pict", style);
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
bool BeagleWksDRParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = BeagleWksDRParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->checkPosition(66))
    return false;
  if (strict) return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";

  input->seek(0, librevenge::RVNG_SEEK_SET);
  if (input->readLong(2)!=0x4257 || input->readLong(2)!=0x6b73 ||
      input->readLong(2)!=0x4257 || input->readLong(2)!=0x6472 ||
      input->readLong(2)!=0x4257 || input->readLong(2)!=0x6472)
    return false;
  for (int i=0; i < 9; ++i) { // f2=f6=1 other 0
    int val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  setVersion(1);

  if (header)
    header->reset(MWAWDocument::MWAW_T_BEAGLEWORKS, 1, MWAWDocument::MWAW_K_DRAW);

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  long pos=input->tell();
  f.str("");
  f << "FileHeader-II:";
  m_state->m_graphicBegin=input->readLong(4);
  if (!input->checkPosition(m_state->m_graphicBegin)) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::checkHeader: can not read the graphic position\n"));
    return false;
  }
  f << "graphic[ptr]=" << std::hex << m_state->m_graphicBegin << std::dec << ",";
  for (int i=0; i < 11; ++i) { // f2=0x50c|58c|5ac f3=f5=9
    long val=input->readLong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  MWAWEntry entry;
  entry.setBegin(input->readLong(4));
  entry.setLength(input->readLong(4));
  entry.setId((int) input->readLong(2)); // in fact nFonts
  entry.setType("FontNames");
  f << "fontNames[ptr]=" << std::hex << entry.begin() << "<->" << entry.end()
    << std::dec << ",nFonts=" << entry.id() << ",";
  if (entry.length() && (!entry.valid() || !input->checkPosition(entry.end()))) {
    MWAW_DEBUG_MSG(("BeagleWksDRParser::checkHeader: can not read the font names position\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }

  m_state->m_typeEntryMap.insert
  (std::multimap<std::string, MWAWEntry>::value_type(entry.type(),entry));
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  if (strict && !readPrintInfo())
    return false;
  ascii().addPos(66);
  ascii().addNote("_");

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
