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

#include <librevenge/librevenge.h>

#include "MWAWTextListener.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWList.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "BeagleWksStructManager.hxx"
#include "BeagleWksText.hxx"

#include "BeagleWksParser.hxx"

/** Internal: the structures of a BeagleWksParser */
namespace BeagleWksParserInternal
{
////////////////////////////////////////
//! Internal: the state of a BeagleWksParser
struct State {
  //! constructor
  State() : m_textBegin(0), m_typeEntryMap(), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }

  /** the text begin position */
  long m_textBegin;
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
BeagleWksParser::BeagleWksParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWTextParser(input, rsrcParser, header), m_state(), m_structureManager(), m_textParser()
{
  init();
}

BeagleWksParser::~BeagleWksParser()
{
}

void BeagleWksParser::init()
{
  resetTextListener();
  setAsciiName("main-1");

  m_state.reset(new BeagleWksParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);

  m_structureManager.reset(new BeagleWksStructManager(getParserState()));
  m_textParser.reset(new BeagleWksText(*this));
}

MWAWInputStreamPtr BeagleWksParser::rsrcInput()
{
  return getRSRCParser()->getInput();
}

libmwaw::DebugFile &BeagleWksParser::rsrcAscii()
{
  return getRSRCParser()->ascii();
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
Vec2f BeagleWksParser::getPageLeftTop() const
{
  return Vec2f(float(getPageSpan().getMarginLeft()),
               float(getPageSpan().getMarginTop()+m_state->m_headerHeight/72.0));
}

////////////////////////////////////////////////////////////
// interface with the text parser
////////////////////////////////////////////////////////////
bool BeagleWksParser::sendFrame(int pId)
{
  BeagleWksStructManager::Frame frame;
  if (!m_structureManager->getFrame(pId, frame)) return false;
  if (!frame.m_charAnchor) {
    MWAW_DEBUG_MSG(("BeagleWksParser::sendFrame: the frame is bad for id=%d\n",pId));
    return false;
  }

  return sendFrame(frame);
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void BeagleWksParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!getTextListener() || m_state->m_actPage == 1)
      continue;
    getTextListener()->insertBreak(MWAWTextListener::PageBreak);
  }
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void BeagleWksParser::parse(librevenge::RVNGTextInterface *docInterface)
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
      m_textParser->sendMainText();
#ifdef DEBUG
      m_textParser->flushExtra();
#endif
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("BeagleWksParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetTextListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void BeagleWksParser::createDocument(librevenge::RVNGTextInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getTextListener()) {
    MWAW_DEBUG_MSG(("BeagleWksParser::createDocument: listener already exist\n"));
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
  shared_ptr<MWAWSubDocument> subDoc;
  for (int i = 0; i <= numPages;) {
    MWAWPageSpan ps(getPageSpan());
    int numSim[2]= {1,1};
    subDoc = m_textParser->getHeader(i, numSim[0]);
    if (subDoc) {
      MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
      header.m_subDocument=subDoc;
      ps.setHeaderFooter(header);
    }
    subDoc = m_textParser->getFooter(i, numSim[1]);
    if (subDoc) {
      MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
      footer.m_subDocument=subDoc;
      ps.setHeaderFooter(footer);
    }
    if (numSim[1] < numSim[0]) numSim[0]=numSim[1];
    if (numSim[0] < 1) numSim[0]=1;
    ps.setPageSpan(numSim[0]);
    i+=numSim[0];
    pageList.push_back(ps);
  }

  MWAWTextListenerPtr listen(new MWAWTextListener(*getParserState(), pageList, documentInterface));
  setTextListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool BeagleWksParser::createZones()
{
  readRSRCZones();
  MWAWInputStreamPtr input = getInput();
  if (input->seek(66, librevenge::RVNG_SEEK_SET) || !readPrintInfo())
    return false;
  long pos = input->tell();
  if (!input->checkPosition(pos+70)) {
    MWAW_DEBUG_MSG(("BeagleWksParser::createZones: the file can not contains Zones\n"));
    return false;
  }
  // first check the main entry
  MWAWEntry mainEntry;
  mainEntry.setBegin(m_state->m_textBegin);
  input->seek(mainEntry.begin(), librevenge::RVNG_SEEK_SET);
  mainEntry.setLength(input->readLong(4));
  if (!mainEntry.valid()||!input->checkPosition(mainEntry.end())) {
    MWAW_DEBUG_MSG(("BeagleWksParser::createZones: can not determine main zone size\n"));
    ascii().addPos(mainEntry.begin());
    ascii().addNote("Entries(Text):###");
    return false;
  }
  mainEntry.setType("Text");
  mainEntry.setId(0);

  // now read the list of zones
  libmwaw::DebugStream f;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(Zones):";
  for (int i=0; i<7; ++i) { // checkme: at least 2 zones, maybe 7
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
        MWAW_DEBUG_MSG(("BeagleWksParser::createZones: can not read the header/footer zone, stop\n"));
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        return false;
      }
      MWAW_DEBUG_MSG(("BeagleWksParser::createZones: can not zones entry %d\n",i));
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

  if (!m_textParser->createZones(mainEntry))
    return false;
  readLastZone();
  return true;
}

bool BeagleWksParser::readRSRCZones()
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
// read/send the list of frame
////////////////////////////////////////////////////////////
bool BeagleWksParser::sendPageFrames()
{
  std::map<int, BeagleWksStructManager::Frame> const &frameMap = m_structureManager->getIdFrameMap();
  std::map<int, BeagleWksStructManager::Frame>::const_iterator it;
  for (it=frameMap.begin(); it!=frameMap.end(); ++it) {
    BeagleWksStructManager::Frame const &frame=it->second;
    if (!frame.m_charAnchor)
      sendFrame(frame);
  }
  return true;
}

bool BeagleWksParser::sendFrame(BeagleWksStructManager::Frame const &frame)
{
  MWAWPosition fPos(Vec2f(0,0), frame.m_dim, librevenge::RVNG_POINT);
  librevenge::RVNGPropertyList extra;
  if (frame.m_charAnchor)
    fPos.setRelativePosition(MWAWPosition::Char);
  else {
    fPos.setPagePos(frame.m_page > 0 ? frame.m_page : 1, frame.m_origin);
    fPos.setRelativePosition(MWAWPosition::Page);

    fPos.m_wrapping = frame.m_wrap==0 ? MWAWPosition::WNone : MWAWPosition::WDynamic;
    if (!frame.m_border.isEmpty() &&
        frame.m_bordersSet==(libmwaw::LeftBit|libmwaw::RightBit|
                             libmwaw::TopBit|libmwaw::BottomBit))
      frame.m_border.addTo(extra,"");
    else if (!frame.m_border.isEmpty() && frame.m_bordersSet) {
      if (frame.m_bordersSet & libmwaw::LeftBit)
        frame.m_border.addTo(extra,"left");
      if (frame.m_bordersSet & libmwaw::RightBit)
        frame.m_border.addTo(extra,"right");
      if (frame.m_bordersSet & libmwaw::TopBit)
        frame.m_border.addTo(extra,"top");
      if (frame.m_bordersSet & libmwaw::BottomBit)
        frame.m_border.addTo(extra,"bottom");
    }
  }
  return sendPicture(frame.m_pictId, fPos, extra);
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool BeagleWksParser::readPrintInfo()
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
    MWAW_DEBUG_MSG(("BeagleWksParser::readPrintInfo: file is too short\n"));
    return false;
  }
  ascii().addPos(input->tell());

  return true;
}

////////////////////////////////////////////////////////////
// read the last zone ( mainly unknown )
////////////////////////////////////////////////////////////
bool BeagleWksParser::readLastZone()
{
  MWAWInputStreamPtr input = getInput();
  long beginPos = input->tell();
  if (input->seek(beginPos+568,librevenge::RVNG_SEEK_SET)||!input->isEnd()) {
    MWAW_DEBUG_MSG(("BeagleWksParser::readLastZone: the last zone seems odd\n"));
    ascii().addPos(beginPos);
    ascii().addNote("Entries(LastZone):###");
    return false;
  }

  libmwaw::DebugStream f;
  ascii().addPos(beginPos);
  ascii().addNote("Entries(LastZone)");

  input->seek(beginPos+4, librevenge::RVNG_SEEK_SET);
  long pos;
  for (int st=0; st<3; ++st) {
    pos=input->tell();
    f.str("");
    f << "LastZone-A" << st << ":";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+34, librevenge::RVNG_SEEK_SET);
  }

  pos=input->tell();
  ascii().addPos(pos);
  ascii().addNote("LastZone-B:");
  input->seek(pos+100, librevenge::RVNG_SEEK_SET);
  pos=input->tell();
  f.str("");
  f << "LastZone-DocInfo:";
  double margins[4]; // TBRL
  for (int i=0; i<4; ++i) {
    margins[i]=double(input->readLong(4))/65536./72.;
    if (i<2) input->seek(2, librevenge::RVNG_SEEK_CUR); // skip margins in point
  }
  f << "margins=[" << margins[0] << "," << margins[1] << "," << margins[2] << "," << margins[3] << "],";
  if (margins[0]>=0&&margins[1]>=0&&margins[2]>=0&&margins[3]>=0&&
      margins[0]+margins[1]<0.5*getFormLength() &&
      margins[2]+margins[3]<0.5*getFormWidth()) {
    getPageSpan().setMarginTop(margins[0]);
    getPageSpan().setMarginBottom(margins[1]);
    getPageSpan().setMarginLeft(margins[3]);
    getPageSpan().setMarginRight(margins[2]);
  }
  else {
    MWAW_DEBUG_MSG(("BeagleWksParser::readLastZone: the page margins seem bad\n"));
    f << "###";
  }
  int val=(int) input->readLong(2);
  if (val!=1) f << "firstPage=" << val << ",";
  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());


  input->seek(pos+76, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<20; ++i) {
    pos=input->tell();
    f.str("");
    f << "LastZone-C" << i << ":";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+6, librevenge::RVNG_SEEK_SET);
  }

  pos=input->tell();
  f.str("");
  f << "LastZone-D:";
  for (int i=0; i < 2; ++i) { // always 0 excepted one time f0=0x263,f1=-7
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  for (int i=0; i < 140; ++i) { // a list of flags ? all 1 expected f3=-1
    val=(int) input->readLong(1);
    if (val!=1) f << "g" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "LastZone-E:";
  val=(int)input->readLong(4); // a big number, a dim?
  if (val) f << "dim?=" << double(val)/65536. << ",";
  val=(int) input->readLong(2); // 1|2|3|4
  if (val) f << "f0=" << val << ",";
  for (int i=0; i<4; ++i) {
    static int const expectedVal[]= {0x6c, 0xc, 0, 0x21};
    val=(int) input->readLong(2);
    if (val != expectedVal[i])
      f << "f" << i+1 << "=" << val << ",";
  }
  // then some flags ?
  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// resource fork data
////////////////////////////////////////////////////////////

// read/send picture (edtp resource)
bool BeagleWksParser::sendPicture
(int pId, MWAWPosition const &pictPos, librevenge::RVNGPropertyList frameExtras)
{
  MWAWTextListenerPtr listener=getTextListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("BeagleWksParser::sendPicture: can not find the listener\n"));
    return false;
  }
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  if (!rsrcParser) {
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("BeagleWksParser::sendPicture: need access to resource fork to retrieve picture content\n"));
      first=false;
    }
    return true;
  }

  librevenge::RVNGBinaryData data;
  if (!m_structureManager->readPicture(pId, data))
    return false;

  listener->insertPicture(pictPos, data, "image/pict", frameExtras);
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
bool BeagleWksParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = BeagleWksParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->checkPosition(66))
    return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";

  input->seek(0, librevenge::RVNG_SEEK_SET);
  if (input->readLong(2)!=0x4257 || input->readLong(2)!=0x6b73 ||
      input->readLong(2)!=0x4257 || input->readLong(2)!=0x7770 ||
      input->readLong(2)!=0x4257 || input->readLong(2)!=0x7770) {
    return false;
  }
  for (int i=0; i<9; ++i) { // f2=f6=1 other 0
    int val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  setVersion(1);

  if (header)
    header->reset(MWAWDocument::MWAW_T_BEAGLEWORKS, 1);

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  long pos=input->tell();
  f.str("");
  f << "FileHeader-II:";
  m_state->m_textBegin=input->readLong(4);
  if (!input->checkPosition(m_state->m_textBegin)) {
    MWAW_DEBUG_MSG(("BeagleWksParser::checkHeader: can not read the text position\n"));
    return false;
  }
  f << "text[ptr]=" << std::hex << m_state->m_textBegin << std::dec << ",";
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
    MWAW_DEBUG_MSG(("BeagleWksParser::checkHeader: can not read the font names position\n"));
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
