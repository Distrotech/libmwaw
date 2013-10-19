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
#include <set>
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictData.hxx"
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
  State() : m_typeEntryMap(), m_backgroundColor(MWAWColor::white()), m_colorList(), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0) {
  }
  //! set the default color map
  void setDefaultColorList(int version);

  //! a map type -> entry
  std::multimap<std::string, MWAWEntry> m_typeEntryMap;
  //! the organization back page color
  MWAWColor m_backgroundColor;
  //! a list colorId -> color
  std::vector<MWAWColor> m_colorList;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

void State::setDefaultColorList(int version)
{
  if (m_colorList.size()) return;
  if (version==3) {
    uint32_t const defCol[32] = {
      0x000000,0x333333,0x555555,0x7f7f7f,0x999999,0xbbbbbb,0xdddddd,0xffffff,
      0xfcf305,0xf20884,0xdd0806,0x02abea,0x008011,0x0000d4,0x7f007f,0x7f3f00,
      0xffff80,0xff80ff,0xff8080,0x80ffff,0x80ff80,0x8080ff,0x008080,0x006699,
      0xffcccc,0xcccccc,0xcc9999,0xcc9966,0xcc6633,0xcccc99,0x999966,0x666633
    };
    m_colorList.resize(32);
    for (size_t i = 0; i < 32; i++)
      m_colorList[i] = defCol[i];
    return;
  }
}

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

bool MORParser::checkAndStore(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.begin() < 0x80 || !getInput()->checkPosition(entry.end()))
    return false;
  if (entry.type().empty()) {
    MWAW_DEBUG_MSG(("MORParser::checkAndStore: entry type is not set\n"));
    return false;
  }

  m_state->m_typeEntryMap.insert
  (std::multimap<std::string, MWAWEntry>::value_type(entry.type(),entry));
  return true;
}

bool MORParser::checkAndFindSize(MWAWEntry &entry)
{
  MWAWInputStreamPtr &input= getInput();
  if (entry.begin()<0 || !input->checkPosition(entry.begin()+4))
    return false;
  long actPos=input->tell();
  input->seek(entry.begin(), WPX_SEEK_SET);
  entry.setLength(4+(long) input->readULong(4));
  input->seek(actPos,WPX_SEEK_SET);
  return input->checkPosition(entry.end());
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

bool MORParser::getColor(int id, MWAWColor &col) const
{
  int numColor = (int) m_state->m_colorList.size();
  if (!numColor) {
    m_state->setDefaultColorList(version());
    numColor = int(m_state->m_colorList.size());
  }
  if (id < 0 || id >= numColor)
    return false;
  col = m_state->m_colorList[size_t(id)];
  return true;
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
  ps.setBackgroundColor(m_state->m_backgroundColor);
  MWAWSubDocumentPtr doc=m_textParser->getHeaderFooter(true);
  if (doc) {
    MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
    header.m_subDocument=doc;
    ps.setHeaderFooter(header);
  }
  doc=m_textParser->getHeaderFooter(false);
  if (doc) {
    MWAWHeaderFooter header(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    header.m_subDocument=doc;
    ps.setHeaderFooter(header);
  }

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
  if (!readZonesList())
    return false;

  std::multimap<std::string, MWAWEntry>::const_iterator it;
  it = m_state->m_typeEntryMap.find("PrintInfo");
  if (it != m_state->m_typeEntryMap.end())
    readPrintInfo(it->second);

  it = m_state->m_typeEntryMap.find("DocInfo");
  if (it != m_state->m_typeEntryMap.end())
    readDocumentInfo(it->second);

  it = m_state->m_typeEntryMap.find("Fonts");
  if (it != m_state->m_typeEntryMap.end())
    m_textParser->readFonts(it->second);

  bool ok=false;
  it = m_state->m_typeEntryMap.find("Topic");
  if (it != m_state->m_typeEntryMap.end())
    ok=m_textParser->readTopic(it->second);
  if (!ok) // no need to continue if we can not read the text position
    return false;

  it = m_state->m_typeEntryMap.find("Comment");
  if (it != m_state->m_typeEntryMap.end())
    m_textParser->readComment(it->second);

  it = m_state->m_typeEntryMap.find("SpeakerNote");
  if (it != m_state->m_typeEntryMap.end())
    m_textParser->readSpeakerNote(it->second);

  it = m_state->m_typeEntryMap.find("Slide");
  if (it != m_state->m_typeEntryMap.end())
    readSlideList(it->second);

  it = m_state->m_typeEntryMap.find("Outline");
  if (it != m_state->m_typeEntryMap.end())
    m_textParser->readOutlineList(it->second);

  it = m_state->m_typeEntryMap.find("FreePos");
  if (it != m_state->m_typeEntryMap.end())
    readFreePos(it->second);

  it = m_state->m_typeEntryMap.find("Unknown9");
  if (it != m_state->m_typeEntryMap.end())
    readUnknown9(it->second);

  it = m_state->m_typeEntryMap.begin();
  while (it != m_state->m_typeEntryMap.end()) {
    MWAWEntry const &entry=(it++)->second;
    if (entry.isParsed())
      continue;
    libmwaw::DebugStream f;
    f << "Entries(" << entry.type() << "):";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
    ascii().addPos(entry.end());
    ascii().addNote("_");
  }

  return m_textParser->createZones();
}

bool MORParser::readZonesList()
{
  int vers=version();
  if (vers<2)
    return false;
  MWAWInputStreamPtr input = getInput();
  if (!input->checkPosition(0x80)) {
    MWAW_DEBUG_MSG(("MORParser::readZonesList: file is too short\n"));
    return false;
  }
  long pos=8;
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(Zones):";
  for (int i=0; i < 9; i++) {
    MWAWEntry entry;
    entry.setBegin((long) input->readULong(4));
    entry.setLength((long) input->readULong(4));
    static char const *(names[])= {
      "PrintInfo", "DocInfo", "Unknown2", "Topic",
      "Comment", "Slide", "Outline", "FreePos", "SpeakerNote"
    };
    entry.setType(names[i]);
    if (!entry.length())
      continue;
    f << names[i] << "(" << std::hex << entry.begin() << "<->" << entry.end()
      << std::dec <<  "), ";
    if (!checkAndStore(entry)) {
      MWAW_DEBUG_MSG(("MORParser::readZonesList: can not read entry %d\n", i));
      f << "###";
    }
  }
  long unkn=(long) input->readULong(4);
  if (unkn) f << "unkn=" << unkn << ",";
  /* checkme: another list begins here, but I am not sure of its length :-~ */
  for (int i=0; i < 5; i++) {
    static char const *(names[])=
    { "Unknown9", "Fonts", "UnknownB","UnknownC", "UnknownD" };
    MWAWEntry entry;
    entry.setBegin((long) input->readULong(4));
    entry.setLength((long) input->readULong(4));
    entry.setType(names[i]);
    if (!entry.length())
      continue;
    f << names[i] << "(" << std::hex << entry.begin() << "<->" << entry.end()
      << std::dec <<  "), ";
    if (!checkAndStore(entry)) {
      MWAW_DEBUG_MSG(("MORParser::readZonesList: can not read entry %d\n", i));
      f << "###";
    }
  }
  unkn=(long) input->readULong(4); // always 0?
  if (unkn) f << "unkn2=" << std::hex << unkn << std::dec << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return !m_state->m_typeEntryMap.empty();
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool MORParser::readPrintInfo(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() != 120) {
    MWAW_DEBUG_MSG(("MORParser::readPrintInfo: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  libmwaw::PrinterInfo info;
  if (!info.read(input)) return false;
  f << "Entries(PrintInfo):"<< info;
  entry.setParsed(true);

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
  return true;
}

////////////////////////////////////////////////////////////
// read the document info
////////////////////////////////////////////////////////////
bool MORParser::readDocumentInfo(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() != 436) {
    MWAW_DEBUG_MSG(("MORParser::readDocumentInfo: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  f << "Entries(DocInfo):";
  entry.setParsed(true);
  double margins[4]; // LR TB
  for (int i=0; i < 4; i++)
    margins[i]=double(input->readULong(2))/1440.;
  f << "margins=" << margins[0] << "x" << margins[2]
    << "<->" << margins[1] << "x" << margins[3] << ",";
  int val;
  for (int i=0; i < 2; i++) { // always 1: related to header/footer?
    val=(int) input->readLong(1);
    if (val!=1) f << "fl" << i << "=" << val << ",";
  }
  double dim[3];
  for (int i=0; i < 3; i++)
    dim[i]=double(input->readULong(2))/72.;
  f << "dim=" << dim[0] << "x" << dim[1];
  if (dim[1]<dim[2]||dim[1]>dim[2])
    f << "[" << dim[2] << "],";
  else
    f << ",";
  if (dim[0]>0 && dim[1]>0 &&
      margins[0]>=0 && margins[1]>=0 && margins[2]>=0 && margins[3]>=0 &&
      2.*(margins[0]+margins[1])<dim[0] && 2.*(margins[2]+margins[3])<dim[1]) {
    getPageSpan().setMarginLeft(margins[0]);
    getPageSpan().setMarginRight(margins[1]);
    getPageSpan().setMarginTop(margins[2]);
    getPageSpan().setMarginBottom(margins[3]);
    // has we do not know how to retrieve the page orientation
    if ((dim[0]>=dim[1]) ==
        (getPageSpan().getFormWidth()>=getPageSpan().getFormLength())) {
      getPageSpan().setFormWidth(dim[0]);
      getPageSpan().setFormLength(dim[1]);
    }
  } else {
    MWAW_DEBUG_MSG(("MORParser::readDocumentInfo: can not read the page dimension\n"));
    f << "###";
  }
  static int const expectedVal[4]= {0,3,1,0}; // unknown
  for (int i=0; i < 4; i++) {
    val=(int) input->readLong(2);
    if (val!=expectedVal[i])
      f << "f" << i << "=" << val << ",";
  }
  val=(int) input->readLong(2);
  if (val!=3) f << "fId?=" << val << ",";
  val=(int) input->readLong(2);
  if (val!=12) f << "fSz?=" << val << ",";
  for (int i=0; i < 2; i++) { // always 1: related to font flag
    val=(int) input->readLong(1);
    if (val!=1) f << "fl" << i+2 << "=" << val << ",";
  }

  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=entry.begin()+160;
  input->seek(pos,WPX_SEEK_SET);
  f.str("");
  f << "DocInfo-II:";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=entry.begin()+268;
  input->seek(pos,WPX_SEEK_SET);
  f.str("");
  f << "DocInfo-III:";
  for (int st=0; st <7; st++) {
    // normal white,white,black,white,black,background=white,white
    unsigned char color[3];
    for (int i=0; i < 3; i++)
      color[i]=(unsigned char) (input->readULong(2)>>8);
    MWAWColor col(color[0], color[1], color[2]);
    if (st==2 || st==4) {
      if (col.isBlack())
        continue;
    } else if (col.isWhite())
      continue;
    if (st==5) {
      m_state->m_backgroundColor=col;
      f << "backColor=" << col << ",";
    } else
      f << "color" << st << "?=" << col << ",";
  }
  for (int i=0; i < 60; i++) { // always 0 excepted f57=0|1 ?
    val=(int) input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  for (int i=0; i < 3; i++) { // always 5,5,-1 ?
    val=(int) input->readLong(2);
    int expVal=i==2?-1:5;
    if (val!=expVal)
      f << "g" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the list of free position
////////////////////////////////////////////////////////////
bool MORParser::readFreePos(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<4) {
    MWAW_DEBUG_MSG(("MORParser::readFreePos: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr &input= getInput();
  libmwaw::DebugStream f;
  entry.setParsed(true);

  input->seek(pos, WPX_SEEK_SET);
  int N=(int) input->readULong(4);
  f << "Entries(FreePos):N=" << N;
  if (4+N*8 > entry.length()) {
    MWAW_DEBUG_MSG(("MORParser::readFreePos: the number of entry seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  int val;
  std::vector<MWAWEntry> filePositions;
  for (int i=0; i < N; i++) {
    pos = input->tell();
    long fPos = input->readLong(4);
    f.str("");
    f << "FreePos-" << i << ":";
    f << std::hex << fPos << std::dec << ",";
    MWAWEntry tEntry;
    tEntry.setBegin(fPos);
    int what=(int) input->readULong(2);
    if (what==0) {
      tEntry.setLength((int) input->readULong(2));
      f << "length=" << tEntry.length() << ",";
    } else {
      if (what!=0x7FFF) // 0x7FFF: last entry, fPos correspond to eof position
        f << "#wh=" << std::hex << what << std::dec << ",";
      val = (int) input->readULong(2); // probably junk
      if (val) f << "f0=" << std::hex << val << std::dec << ",";
    }
    if (tEntry.valid()) {
      if (!input->checkPosition(tEntry.end())) {
        MWAW_DEBUG_MSG(("MORParser::readFreePos: the entry does not seems valid\n"));
        f << "###";
      } else
        filePositions.push_back(tEntry);
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  if (input->tell()!=entry.end()) { // end can be junk field
    ascii().addPos(input->tell());
    ascii().addNote("FreePos-#");
  }
  ascii().addPos(entry.end());
  ascii().addNote("_");

  for (size_t i=0; i < filePositions.size(); i++) {
    MWAWEntry const &tEntry=filePositions[i];
    ascii().addPos(tEntry.begin());
    ascii().addNote("FreePos-data:");
    ascii().addPos(tEntry.end());
    ascii().addNote("_");
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MORParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MORParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->checkPosition(0x80))
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
  if (strict) {
    for (int i=0; i < 8; i++) {
      MWAWEntry entry;
      entry.setBegin((long) input->readULong(4));
      entry.setLength((long) input->readULong(4));
      if (!entry.length())
        continue;
      if (!input->checkPosition(entry.end()-1))
        return false;
    }
  }
  if (header)
    header->reset(MWAWDocument::MWAW_T_MORE, vers);
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());
  return true;
}

//////////////////////////////////////////////
// slide
//////////////////////////////////////////////
bool MORParser::readSlideList(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%8)) {
    MWAW_DEBUG_MSG(("MORParser::readSlideList: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr &input= getInput();
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  entry.setParsed(true);

  ascii().addPos(pos);
  ascii().addNote("Entries(Slide)");

  int N=int(entry.length()/8);
  std::vector<MWAWEntry> filePositions;
  for (int i=0; i < N; i++) {
    pos=input->tell();

    f.str("");
    f << "Slide-" << i << ":";
    long fPos = input->readLong(4);
    f << "pos=" << std::hex << fPos << std::dec << ",";
    MWAWEntry tEntry;
    tEntry.setBegin(fPos);
    if (fPos==0x50) // checkme: default or related to filePosition 0x50 ?
      ;
    else if (!checkAndFindSize(tEntry)) {
      MWAW_DEBUG_MSG(("MORParser::readSlideList: can not read a file position\n"));
      f << "###";
    } else
      filePositions.push_back(tEntry);
    int val = (int) input->readLong(2); // always -1 ?
    if (val != -1)
      f << "f0=" << val << ",";
    val = (int) input->readLong(2); // always 0 ?
    if (val)
      f << "f1=" << val << ",";

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+8, WPX_SEEK_SET);
  }
  for (size_t i=0; i < filePositions.size(); i++) {
    MWAWEntry const &tEntry=filePositions[i];
    if (readSlide(tEntry))
      continue;
    f.str("");
    f << "Slide-###" << i << "[data]:";
    ascii().addPos(tEntry.begin());
    ascii().addNote(f.str().c_str());
    ascii().addPos(tEntry.end());
    ascii().addNote("_");
  }
  return true;
}

bool MORParser::readSlide(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<16) {
    MWAW_DEBUG_MSG(("MORParser::readSlide: the entry is bad\n"));
    return false;
  }
  long pos = entry.begin();
  long endPos = entry.end();
  MWAWInputStreamPtr &input= getInput();
  libmwaw::DebugStream f;

  input->seek(pos+4, WPX_SEEK_SET); // skip size
  entry.setParsed(true);

  f << "Slide[data]:";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+16, WPX_SEEK_SET);

  int n=0;
  while(1) {
    pos = input->tell();
    if (pos+2 > endPos)
      break;
    int type=(int) input->readLong(2);
    int dataSz=0;
    if (type & 0x1)
      dataSz=4;
    else {
      switch(type) {
      case 0x66: // group: arg num group
      case 0x68: // group of num 6a,70* ?
      case 0x72: // group of num 74* ?
      case 0x74: // [ id : val ]
        dataSz=4;
        break;
      case 0x6a: // pattern?, text, id, ...
      case 0x70: // size=0x4a ?
        dataSz=4+(int) input->readULong(4);
        break;
      default:
        MWAW_DEBUG_MSG(("MORParser::readSlide: argh... find unexpected type %d\n", type));
        break;
      }
    }
    if (!dataSz || pos+2+dataSz > endPos) {
      input->seek(pos, WPX_SEEK_SET);
      break;
    }

    f.str("");
    f << "Slide-" << n++ << "[data]:";
    f << "type=" << std::hex << (type&0xFFFE) << std::dec;
    if (type&1) f << "*";
    f << ",";
    if (dataSz==4)
      f << "N=" << input->readLong(4) << ",";
    if (type==0x6a) {
      MWAWEntry dEntry;
      dEntry.setBegin(pos+2+4);
      dEntry.setLength(dataSz-4);
      // can also be some text and ?
      if (m_textParser->parseUnknown(dEntry,-6))
        ;
      else if (readGraphic(dEntry))
        f << "graphic,";
      else
        f << "#";
    }
    input->seek(pos+2+dataSz, WPX_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  pos = input->tell();
  if (pos!=endPos) {
    ascii().addPos(pos);
    ascii().addNote("Slide-###[data]:");
  }

  ascii().addPos(endPos);
  ascii().addNote("_");

  return true;
}

bool MORParser::readGraphic(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<0xd)
    return false;

  long pos = entry.begin();
  MWAWInputStreamPtr input = getInput();
  input->seek(pos, WPX_SEEK_SET);

  // first check
  int readSize = int(input->readULong(2));
  input->seek(8, WPX_SEEK_CUR); // skip dim
  long lastFlag = input->readLong(2);
  switch(lastFlag) {
  case 0x1101: {
    if (readSize+2 != entry.length() && readSize+3 != entry.length())
      return false;
    break;
  }
  case 0x0011: {
    if (entry.length() < 42) return false;
    if (input->readULong(2) != 0x2ff) return false;
    if (input->readULong(2) != 0xC00) return false;
    break;
  }
  default:
    return false;
  }

  input->seek(pos, WPX_SEEK_SET);

  Box2f box;
  if (MWAWPictData::check(input, (int)entry.length(), box)==MWAWPict::MWAW_R_BAD)
    return false;
#ifdef DEBUG_WITH_FILES
  if (1) {
    WPXBinaryData file;
    input->seek(pos, WPX_SEEK_SET);
    input->readDataBlock(entry.length(), file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f;
    f << "Pict-" << ++pictName << ".pct";
    libmwaw::Debug::dumpFile(file, f.str().c_str());
  }
#endif
  ascii().skipZone(pos, entry.end()-1);
  return true;
}

////////////////////////////////////////////////////////////
// read some unknow zone
////////////////////////////////////////////////////////////

// checkme: not sure...
bool MORParser::readUnknown9(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() < 26) {
    MWAW_DEBUG_MSG(("MORParser::readUnknown9: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  long endPos = entry.end();
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  entry.setParsed(true);

  input->seek(pos, WPX_SEEK_SET);
  f << "Entries(Unknown9):";
  int N=(int) input->readLong(4);
  f << "N=" << N << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  for (int n=0; n<N; n++) {
    pos=input->tell();
    if (pos+6>endPos)
      break;
    if (n==0) {
      if (readColors(endPos))
        continue;
      input->seek(pos, WPX_SEEK_SET);
    }
    int type=(int) input->readULong(2); // find 1: color 2:?
    if (type > 10) break;
    long dataSz = (long) input->readULong(4);
    if (dataSz<= 0 || pos+6+dataSz > endPos) {
      input->seek(pos, WPX_SEEK_SET);
      break;
    }

    bool ok=false;
    long endFPos = pos+6+dataSz;

    f.str("");
    f << "Unknown9-" << n << ":type=" << type << ",";
    if (type==2) {
      MORStruct::Pattern pattern;
      ok=readPattern(endFPos, pattern);
      if (ok)
        f << pattern << ",";
      if (!ok) {
        std::string mess("");
        input->seek(pos+6, WPX_SEEK_SET);
        ok = readBackside(endFPos, mess);
        if (ok)
          f << "backside," << mess;
      }
      if (!ok) {
        input->seek(pos+6, WPX_SEEK_SET);
        ok = readUnkn9Sub(endFPos);
        if (ok)
          f << "Unkn9A,";
      }
    }
    if (!ok) {
      MWAW_DEBUG_MSG(("MORParser::readUnknown9: find some unknown structure\n"));
      f << "###";
    } else if (endFPos!=input->tell()) {
      MWAW_DEBUG_MSG(("MORParser::readUnknown9: find some extra data\n"));
      f << "###";
      ascii().addDelimiter(input->tell(),'|');
    }
    input->seek(endFPos, WPX_SEEK_SET);

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    ascii().addPos(endFPos);
    ascii().addNote("_");
  }
  pos=input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("MORParser::readUnknown9: the parsing stopped before end\n"));
    ascii().addPos(pos);
    ascii().addNote("Unknown9(II)");
  }
  return true;
}

// a list of colors ( the first zone of block9)
bool MORParser::readColors(long endPos)
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (pos+22 > endPos)
    return false;
  if (input->readLong(2)!=1)
    return false;

  libmwaw::DebugStream f;
  f << "Entries(ColorL):";
  long dataSz=(long) input->readULong(4);
  if (pos+6+dataSz > endPos)
    return false;
  long val= input->readLong(4); // 3ff or 413 a size ?
  if (val) f << "f0=" << val << ",";
  val= input->readLong(2); // always 0
  if (val) f << "f1=" << val << ",";
  int maxCols=(int)input->readLong(2);
  f << "nCol=" << maxCols << ",";
  if (maxCols<0 || 16+8*maxCols != dataSz)
    return false;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i=0; i <= maxCols; i++) {
    pos=input->tell();
    f.str("");
    f << "ColorL" << i << ",";
    int id=(int) input->readLong(2);
    if (id!=i) f << "#id=" << id << ",";
    unsigned char rgb[3];
    for (int c=0; c<3; c++)
      rgb[c]=(unsigned char)(input->readULong(2)>>8);
    MWAWColor col(rgb[0], rgb[1], rgb[2]);
    f << "col=" << col << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

// a backside definition? ( the last zones of block9)
bool MORParser::readBackside(long endPos, std::string &extra)
{
  extra="";

  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (pos+0x2e > endPos)
    return false;

  std::string name("");
  for (int i=0; i < 8; i++)
    name += char(input->readULong(1));
  if (name != "BACKSIDE")
    return false;

  libmwaw::DebugStream f;
  int val=(int) input->readULong(1); // small number between 1 and 8
  f << "f0=" << val << ",";
  val=(int) input->readLong(1); // always 0 ?
  if (val) f << "f1=" << val << ",";
  for (int i=0; i < 4; i++) { // always 0?
    val = (int) input->readLong(2);
    if (val) f << "f" << i+2 << "=" << val << ",";
  }
  int center[2]; // checkme: xy
  for (int i=0; i < 2; i++)
    center[i]=(int) input->readLong(2);
  if (center[0]!=500 || center[1]!=500)
    f << "center=" << center[0] << "x" << center[1] << ",";
  int dim[4];
  for (int i=0; i < 4; i++)
    dim[i]=(int) input->readLong(2);
  if (dim[0] || dim[1] || dim[2]!=1000 || dim[3]!=1000)
    f << "dim=" << dim[0] << "x" << dim[1] << "<->" << dim[2] << "x" << dim[3] << ",";
  for (int i=0; i < 2; i++) { // g0: small number between 1 and a, g1:16*small number
    val = (int) input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  unsigned char rgb[3];
  for (int c=0; c<3; c++)
    rgb[c]=(unsigned char)(input->readULong(2)>>8);
  f << "col0=" << MWAWColor(rgb[0], rgb[1], rgb[2]) << ",";
  for (int c=0; c<3; c++)
    rgb[c]=(unsigned char)(input->readULong(2)>>8);
  f << "col1=" << MWAWColor(rgb[0], rgb[1], rgb[2]) << ",";
  extra=f.str();
  return true;
}

// a pattern ( the zones of block9 which follow color)
bool MORParser::readPattern(long endPos, MORStruct::Pattern &pattern)
{
  pattern = MORStruct::Pattern();
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (pos+0x1c > endPos)
    return false;

  std::string name("");
  for (int i=0; i < 8; i++)
    name += char(input->readULong(1));
  if (name != "BACKPTRN")
    return false;

  for (int i=0; i < 8; i++)
    pattern.m_pattern[i] = (unsigned char) input->readULong(1);

  // checkme: in general frontColor=backColor, but not always...
  unsigned char rgb[3];
  for (int c=0; c<3; c++)
    rgb[c]=(unsigned char)(input->readULong(2)>>8);
  pattern.m_frontColor=MWAWColor(rgb[0], rgb[1], rgb[2]);
  for (int c=0; c<3; c++)
    rgb[c]=(unsigned char)(input->readULong(2)>>8);
  pattern.m_backColor=MWAWColor(rgb[0], rgb[1], rgb[2]);
  return true;
}

/* a ? ( the middle zone of block9). checkme: structure */
bool MORParser::readUnkn9Sub(long endPos)
{
  MWAWInputStreamPtr input = getInput();
  long debPos = input->tell();
  if (debPos+118 > endPos)
    return false;

  long pos = debPos;
  libmwaw::DebugStream f;
  f << "Entries(Unkn9A):";
  long val=input->readLong(2); // always 1?
  if (val!=1) f << "f0=" << val << ",";
  val=input->readLong(4); // always 1c
  if (val!=0x1c) f << "f1=" << val << ",";
  val=input->readLong(4); // always 4e
  if (val!=0x4e) f << "f2=" << val << ",";
  for (int i=0; i < 5; i++) { // 0 excepted f5=-1
    val=input->readLong(2);
    if (val) f << "f" << i+3 << "=" << val << ",";
  }
  /* find [0,2,2,2,2,2,7e,0] or [0,0,0,0,0,0,ff,0,] or [db,6d,b6,db,6d,b6,db,6d,]:
     maybe a pattern */
  f << "pattern?=[";
  for (int i=0; i < 8; i++)
    f << std::hex << input->readULong(1) << std::dec << ",";
  f << "],";

  static int const expectedVal[]= {0, 0, 0x8004, 0, 0, 8, 8 };
  for (int i=0; i < 7; i++) {
    val=(long) input->readULong(2);
    if (val!=expectedVal[i]) f << "g" << i << "=" << val << ",";
  }
  for (int i=0; i < 9; i++) {
    val=input->readLong(2);
    int expected=(i==4||i==6) ? 0x48:0;
    if (val != expected) f << "h" << i << "=" << val << ",";
  }

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=debPos+60;
  input->seek(pos, WPX_SEEK_SET);
  f.str("");
  f << "Unkn9A-II:";
  for (int i=0; i < 9; i++) {
    val=input->readLong(2);
    int expected= i==1 ? 1 : i<3 ? 4 : i==6 ? 0x6e : 0;
    if (val != expected) f << "f" << i << "=" << val << ",";
  }
  /* now 8 uint32_t zones:
     Z1,Z2,..,Z5: always similar, Z7: often 0
     note: I only find in 0..3 in the 8 uint4_t which compose a uint32_t
   */
  f << "unkn=[";
  for (int i=0; i < 8; i++) {
    val=(long) input->readULong(4);
    if (val)
      f << std::hex << val << std::dec << ",";
    else
      f << "_,";
  }
  f << "],";
  for (int i=0; i < 3; i++) { // always 0?
    val=input->readLong(2);
    if (val)
      f << "g=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=debPos+116;
  input->seek(pos, WPX_SEEK_SET);
  int N=(int) input->readLong(2);
  f.str("");
  f << "Unkn9A-III:N=" << N << ",";
  if (pos+2+(N+1)*8 > endPos) {
    MWAW_DEBUG_MSG(("MORParser::readUnkn9Sub: can not read end of zone\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int n=0; n <= N; n++) {
    pos = input->tell();
    f.str("");
    f << "Unkn9A-III[" << n << "]:";
    val = input->readLong(2);
    if (int(val) != n) f << "#id=" << val << ",";

    ascii().addDelimiter(input->tell(),'|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+8, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// MORStruct implementation
////////////////////////////////////////////////////////////

namespace MORStruct
{
std::ostream &operator<<(std::ostream &o, Pattern const &pat)
{
  o << "pat=[" << std::hex;
  for (int i=0; i<8; i++)
    o << int(pat.m_pattern[i]) << ",";
  o << std::dec << "],";
  if (!pat.m_frontColor.isBlack())
    o << "frontColor=" << pat.m_frontColor << ",";
  if (!pat.m_backColor.isWhite())
    o << "backColor=" << pat.m_backColor << ",";
  return o;
}
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
