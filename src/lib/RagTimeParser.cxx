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
#include "MWAWFont.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWGraphicShape.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictBitmap.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "RagTimeSpreadsheet.hxx"
#include "RagTimeStruct.hxx"
#include "RagTimeText.hxx"

#include "RagTimeParser.hxx"

/** Internal: the structures of a RagTimeParser */
namespace RagTimeParserInternal
{
////////////////////////////////////////
//! Internal: the pattern of a RagTimeManager
struct Pattern : public MWAWGraphicStyle::Pattern {
  //! constructor ( 4 int by patterns )
  Pattern(uint16_t const *pat=0) : MWAWGraphicStyle::Pattern(), m_percent(0)
  {
    if (!pat) return;
    m_colors[0]=MWAWColor::white();
    m_colors[1]=MWAWColor::black();
    m_dim=Vec2i(8,8);
    m_data.resize(8);
    for (size_t i=0; i < 4; ++i) {
      uint16_t val=pat[i];
      m_data[2*i]=(unsigned char)(val>>8);
      m_data[2*i+1]=(unsigned char)(val&0xFF);
    }
    int numOnes=0;
    for (size_t j=0; j < 8; ++j) {
      uint8_t val=(uint8_t) m_data[j];
      for (int b=0; b < 8; b++) {
        if (val&1) ++numOnes;
        val = uint8_t(val>>1);
      }
    }
    m_percent=float(numOnes)/64.f;
  }
  //! the percentage
  float m_percent;
};


////////////////////////////////////////
//! Internal: a picture of a RagTimeParser
struct Picture {
  //! constructor
  Picture() : m_type(0), m_pos(), m_dim(), m_headerPos(0), m_isSent(false)
  {
  }
  //! the picture type(unsure)
  int m_type;
  //! the data position
  MWAWEntry m_pos;
  //! the dimension
  Box2f m_dim;
  //! the beginning of the header(for debugging)
  long m_headerPos;
  //! a flag to know if the picture is sent
  mutable bool m_isSent;
};

////////////////////////////////////////
//! Internal: a zone of a RagTimeParser
struct Zone {
  //! the zone type
  enum Type { Text, Page, Picture, Line, Spreadsheet, Chart, Unknown };
  //! constructor
  Zone(): m_type(Unknown), m_subType(0), m_read32Size(false), m_dimension(), m_page(0), m_rotation(0),
    m_style(MWAWGraphicStyle::emptyStyle()), m_fontColor(MWAWColor::black()), m_arrowFlags(0), m_sharedWith(0), m_isSent(false), m_extra("")
  {
    for (int i=0; i<5; ++i) m_linkZones[i]=0;
  }
  //! returns the bounding box
  Box2f getBoundingBox() const
  {
    Vec2f minPt=m_dimension[0], maxPt=m_dimension[1];
    for (int i=0; i<2; ++i) {
      if (m_dimension[0][i]<=m_dimension[1][i])
        continue;
      minPt[i]=m_dimension[1][i];
      maxPt[i]=m_dimension[0][i];
    }
    return Box2f(minPt,maxPt);
  }
  //! returns a zone name
  std::string getTypeString() const
  {
    switch (m_type) {
    case Chart:
      return "Chart";
    case Spreadsheet:
      return "SheetZone";
    case Page:
      return "Page";
    case Picture:
      return "PictZone";
    case Text:
      return "TextZone";
    case Line:
      return "Line";
    case Unknown:
    default:
      break;
    }
    std::stringstream s;
    if (m_subType&0xf)
      s << "Undef" << m_subType << "A";
    else
      s << "Zone" << m_subType;
    return s.str();
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Zone const &z);
  //! the zone type
  Type m_type;
  //! the zone sub type
  int m_subType;
  //! flag to know if the datasize in uint16 or uint32
  bool m_read32Size;
  //! the dimension
  Box2f m_dimension;
  //! the page
  int m_page;
  //! the rotation
  int m_rotation;
  //! the style
  MWAWGraphicStyle m_style;
  //! the font color (for text)
  MWAWColor m_fontColor;
  //! arrow flag 1:begin, 2:end
  int m_arrowFlags;
  //! the link zones ( parent, prev, next, child, linked)
  int m_linkZones[5];
  //! the zone which contains the content
  int m_sharedWith;
  //! a flag to know if the picture is sent
  mutable bool m_isSent;
  //! extra data
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, Zone const &z)
{
  switch (z.m_type) {
  case Zone::Chart:
    o << "chart,";
    break;
  case Zone::Line:
    o << "line,";
    break;
  case Zone::Page:
    o << "page,";
    break;
  case Zone::Picture:
    o << "pict,";
    break;
  case Zone::Text:
    o << "text,";
    break;
  case Zone::Spreadsheet:
    o << "spreadsheet,";
    break;
  case Zone::Unknown:
  default:
    o << "zone" << z.m_subType << ",";
    break;
  }
  if (z.m_read32Size) o << "32[dataSize],";
  o << "dim=" << z.m_dimension << ",";
  if (z.m_page>0) o << "page=" << z.m_page << ",";
  if (z.m_rotation) o << "rot=" << z.m_rotation << ",";
  o << "style=[" << z.m_style << "],";
  if (!z.m_fontColor.isBlack())
    o << "color[font]=" << z.m_fontColor << ",";
  if (z.m_arrowFlags&1)
    o << "arrows[beg],";
  if (z.m_arrowFlags&2)
    o << "arrows[end],";
  o << "ids=[";
  for (int i=0; i<5; ++i) {
    static char const *(wh[])= {"parent", "prev", "next", "child", "linked"};
    if (z.m_linkZones[i])
      o <<  wh[i] << "=Z" << z.m_linkZones[i] << ",";
  }
  o << "],";
  if (z.m_sharedWith)
    o << "#shared=Z" << z.m_sharedWith << ",";
  o << z.m_extra << ",";
  return o;
}

////////////////////////////////////////
//! Internal: the state of a RagTimeParser
struct State {
  //! constructor
  State() : m_numDataZone(0), m_dataZoneMap(), m_RSRCZoneMap(), m_idColorsMap(), m_patternList(),
    m_actualZoneId(-1), m_idZoneMap(), m_pageZonesIdMap(), m_idPictureMap(), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }
  //! init the pattern to default
  void initDefaultPatterns(int vers);
  //! the number of data zone
  int m_numDataZone;
  //! a map: type->entry (datafork)
  std::multimap<std::string, MWAWEntry> m_dataZoneMap;
  //! a map: type->entry (resource fork)
  std::multimap<std::string, MWAWEntry> m_RSRCZoneMap;
  //! the color map
  std::map<int, std::vector<MWAWColor> > m_idColorsMap;
  //! a list patternId -> pattern
  std::vector<Pattern> m_patternList;
  //! the actual zone id
  int m_actualZoneId;
  //! a map: zoneId->zone (datafork)
  std::map<int, Zone> m_idZoneMap;
  //! a map: page->main zone id
  std::map<int, std::vector<int> > m_pageZonesIdMap;
  //! a map: zoneId->picture (datafork)
  std::map<int, Picture> m_idPictureMap;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

void State::initDefaultPatterns(int vers)
{
  if (!m_patternList.empty()) return;
  if (vers <= 2) {
    static uint16_t const(s_pattern[4*40]) = {
      0x0, 0x0, 0x0, 0x0, 0x8000, 0x800, 0x8000, 0x800, 0x8800, 0x2200, 0x8800, 0x2200, 0x8822, 0x8822, 0x8822, 0x8822,
      0xaa55, 0xaa55, 0xaa55, 0xaa55, 0xdd77, 0xdd77, 0xdd77, 0xdd77, 0xddff, 0x77ff, 0xddff, 0x77ff, 0xffff, 0xffff, 0xffff, 0xffff,
      0x8888, 0x8888, 0x8888, 0x8888, 0xff00, 0x0, 0xff00, 0x0, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa, 0xff00, 0xff00, 0xff00, 0xff00,
      0xeedd, 0xbb77, 0xeedd, 0xbb77, 0x1122, 0x4488, 0x1122, 0x4488, 0x102, 0x408, 0x1020, 0x4080, 0x102, 0x0, 0x0, 0x4080,
      0x77bb, 0xddee, 0x77bb, 0xddee, 0x8844, 0x2211, 0x8844, 0x2211, 0x8040, 0x2010, 0x804, 0x201, 0x8040, 0x0, 0x0, 0x201,
      0x55ff, 0x55ff, 0x55ff, 0x55ff, 0xaa00, 0xaa00, 0xaa00, 0xaa00, 0x8010, 0x220, 0x108, 0x4004, 0xb130, 0x31b, 0xd8c0, 0xc8d,
      0xff88, 0x8888, 0xff88, 0x8888, 0xaa00, 0x8000, 0x8800, 0x8000, 0xff80, 0x8080, 0x8080, 0x8080, 0xf0f0, 0xf0f0, 0xf0f, 0xf0f,
      0xc300, 0x0, 0x3c00, 0x0, 0x808, 0x8080, 0x8080, 0x808, 0x8040, 0x2000, 0x204, 0x800, 0x2, 0x588, 0x5020, 0x0,
      0x8000, 0x0, 0x0, 0x0, 0x40a0, 0x0, 0x40a, 0x0, 0x8142, 0x2418, 0x1824, 0x4281, 0x2050, 0x8888, 0x8888, 0x502,
      0xff80, 0x8080, 0xff08, 0x808, 0x44, 0x2810, 0x2844, 0x0, 0x103c, 0x5038, 0x1478, 0x1000, 0x8, 0x142a, 0x552a, 0x1408
    };
    m_patternList.resize(40);
    for (size_t i = 0; i < 40; i++)
      m_patternList[i]=Pattern(&s_pattern[i*4]);
  }
}

////////////////////////////////////////
//! Internal: the subdocument of a RagTimeParser
class SubDocument : public MWAWSubDocument
{
public:
  // constructor
  SubDocument(RagTimeParser &pars, MWAWInputStreamPtr input, int zoneId) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const
  {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  //! the subdocument id
  int m_id;
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("RagTimeParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_parser);

  long pos = m_input->tell();
  static_cast<RagTimeParser *>(m_parser)->sendText(m_id, listener);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_id != sDoc->m_id) return true;
  return false;
}
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
RagTimeParser::RagTimeParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWTextParser(input, rsrcParser, header), m_state(), m_spreadsheetParser(), m_textParser()
{
  init();
}

RagTimeParser::~RagTimeParser()
{
}

void RagTimeParser::init()
{
  resetTextListener();
  setAsciiName("main-1");

  m_state.reset(new RagTimeParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);

  m_spreadsheetParser.reset(new RagTimeSpreadsheet(*this));
  m_textParser.reset(new RagTimeText(*this));
}

int RagTimeParser::getFontId(int localId) const
{
  return m_textParser->getFontId(localId);
}

bool RagTimeParser::getCharStyle(int charId, MWAWFont &font) const
{
  return m_textParser->getCharStyle(charId, font);
}

bool RagTimeParser::getColor(int colId, MWAWColor &color, int listId) const
{
  if (listId==-1) listId=version()>=2 ? 1 : 0;
  if (m_state->m_idColorsMap.find(listId)==m_state->m_idColorsMap.end()) {
    MWAW_DEBUG_MSG(("RagTimeParser::getColor: can not find the color list %d\n", listId));
    return false;
  }
  std::vector<MWAWColor> const &colors=m_state->m_idColorsMap.find(listId)->second;
  if (colId<0 || colId>=(int)colors.size()) {
    MWAW_DEBUG_MSG(("RagTimeParser::getColor: can not find color %d\n", colId));
    return false;
  }
  color=colors[size_t(colId)];
  return true;
}

int RagTimeParser::getZoneDataFieldSize(int zId) const
{
  if (m_state->m_idZoneMap.find(zId)==m_state->m_idZoneMap.end()) {
    MWAW_DEBUG_MSG(("RagTimeParser::getZoneDataSize: can not find the zone %d\n", zId));
    return 2;
  }
  return m_state->m_idZoneMap.find(zId)->second.m_read32Size ? 4 : 2;
}

int RagTimeParser::getNewZoneId()
{
  return ++m_state->m_actualZoneId;
}

bool RagTimeParser::readTextZone(MWAWEntry &entry, int width, MWAWColor const &fontColor)
{
  return m_textParser->readTextZone(entry, width, fontColor);
}

bool RagTimeParser::getDateTimeFormat(int dtId, std::string &dtFormat) const
{
  return m_spreadsheetParser->getDateTimeFormat(dtId,dtFormat);
}

bool RagTimeParser::sendText(int zId, MWAWListenerPtr listener)
{
  return m_textParser->send(zId, listener);
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void RagTimeParser::newPage(int number)
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
void RagTimeParser::parse(librevenge::RVNGTextInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());
    checkHeader(0L);
    ok=createZones();
    if (ok) {
      createDocument(docInterface);
      sendZones();
#ifdef DEBUG
      m_textParser->flushExtra();
      m_spreadsheetParser->flushExtra();
      flushExtra();
#endif
    }

    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("RagTimeParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetTextListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void RagTimeParser::createDocument(librevenge::RVNGTextInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getTextListener()) {
    MWAW_DEBUG_MSG(("RagTimeParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  int numPages=1;
  std::map<int, RagTimeParserInternal::Zone>::const_iterator it;
  for (it=m_state->m_idZoneMap.begin(); it!=m_state->m_idZoneMap.end(); ++it) {
    RagTimeParserInternal::Zone const &z=it->second;
    if (z.m_type!=RagTimeParserInternal::Zone::Page || numPages>z.m_page)
      continue;
    numPages=z.m_page;
  }
  m_state->m_actPage = 0;
  m_state->m_numPages=numPages;

  // create the page list
  MWAWPageSpan ps(getPageSpan());
  std::vector<MWAWPageSpan> pageList;
  ps.setPageSpan(m_state->m_numPages);
  pageList.push_back(ps);
  //
  MWAWTextListenerPtr listen(new MWAWTextListener(*getParserState(), pageList, documentInterface));
  setTextListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////

bool RagTimeParser::createZones()
{
  int const vers=version();
  if (!findDataZones())
    return false;

  libmwaw::DebugStream f;
  std::multimap<std::string,MWAWEntry>::iterator it;

  if (vers<=1) {
    MWAWRSRCParserPtr rsrcParser = getRSRCParser();
    if (rsrcParser) {
      std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
      it=entryMap.find("PREC");
      if (it!=entryMap.end())
        readPrintInfo(it->second, true);
    }
  }
  else if (vers==2) {
    // print info
    it=m_state->m_RSRCZoneMap.lower_bound("rsrcPREC");
    while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcPREC")
      readPrintInfo(it++->second);
    // the font
    it=m_state->m_RSRCZoneMap.lower_bound("FontName");
    while (it!=m_state->m_RSRCZoneMap.end() && it->first=="FontName")
      m_textParser->readFontNames(it++->second);
    // the character properties
    it=m_state->m_RSRCZoneMap.lower_bound("CharProp");
    while (it!=m_state->m_RSRCZoneMap.end() && it->first=="CharProp")
      m_textParser->readCharProperties(it++->second);
    // the numeric and spreadsheet formats
    readFormatsMap();
    // the file link
    it=m_state->m_RSRCZoneMap.lower_bound("Link");
    while (it!=m_state->m_RSRCZoneMap.end() && it->first=="Link")
      readLinks(it++->second);
    // puce, ...
    it=m_state->m_RSRCZoneMap.lower_bound("Macro");
    while (it!=m_state->m_RSRCZoneMap.end() && it->first=="Macro")
      readMacroFormats(it++->second);
  }
  it=m_state->m_dataZoneMap.lower_bound("TextZone");
  while (it!=m_state->m_dataZoneMap.end() && it->first=="TextZone") {
    MWAWEntry entry=it++->second;
    int width;
    MWAWColor fontColor;
    if (m_state->m_idZoneMap.find(entry.id())!=m_state->m_idZoneMap.end()) {
      RagTimeParserInternal::Zone const &zone=m_state->m_idZoneMap.find(entry.id())->second;
      if (zone.m_rotation==90 || zone.m_rotation==270)
        width=int(zone.m_dimension.size()[1]);
      else
        width=int(zone.m_dimension.size()[0]);
      fontColor=zone.m_fontColor;
    }
    else {
      MWAW_DEBUG_MSG(("RagTimeParser::createZones: can not find text zone with id=%d\n", entry.id()));
      width=int(72.*getPageWidth());
      fontColor=MWAWColor::black();
    }
    m_textParser->readTextZone(entry, width, fontColor);
  }
  it=m_state->m_dataZoneMap.lower_bound("PictZone");
  while (it!=m_state->m_dataZoneMap.end() && it->first=="PictZone") {
    if (vers==1)
      readPictZoneV2(it++->second);
    else
      readPictZone(it++->second);
  }

  // now unknown zone
  it=m_state->m_dataZoneMap.lower_bound("PageZone");
  while (it!=m_state->m_dataZoneMap.end() && it->first=="PageZone")
    readPageZone(it++->second);

  it=m_state->m_dataZoneMap.lower_bound("SheetZone");
  while (it!=m_state->m_dataZoneMap.end() && it->first=="SheetZone") {
    if (vers==1)
      m_spreadsheetParser->readSpreadsheetV2(it++->second);
    else
      m_spreadsheetParser->readSpreadsheet(it++->second);
  }
  it=m_state->m_dataZoneMap.lower_bound("Zone6");
  while (it!=m_state->m_dataZoneMap.end() && it->first=="Zone6")
    readZone6(it++->second);

  it=m_state->m_RSRCZoneMap.lower_bound("rsrcfppr");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcfppr")
    readRsrcfppr(it++->second);
  it=m_state->m_RSRCZoneMap.lower_bound("rsrcBeDc");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcBeDc")
    readRsrcBeDc(it++->second);
  it=m_state->m_RSRCZoneMap.lower_bound("rsrcBtch");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcBtch")
    readRsrcBtch(it++->second);
  it=m_state->m_RSRCZoneMap.lower_bound("rsrcCalc");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcCalc")
    readRsrcCalc(it++->second);

  it=m_state->m_RSRCZoneMap.lower_bound("rsrcFHwl");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcFHwl")
    readRsrcFHwl(it++->second);
  for (int i=0; i<6; ++i) {
    /* gray [2bytes]:color,0,26
       color [2bytes]*3:color
       res_ [2bytes]*3: sometimes string?
       BuSl: [small 2bytes int]*3
       BuGr : structure with many data of size 0x1a
       Unamed: sometimes structured
     */
    static char const *(what[])= {
      "rsrcgray", "rsrccolr", "rsrcres_",
      "rsrcBuSl", "rsrcBuGr", "rsrcUnamed"
    };
    it=m_state->m_RSRCZoneMap.lower_bound(what[i]);
    while (it!=m_state->m_RSRCZoneMap.end() && it->first==what[i])
      readRsrcStructured(it++->second);
  }

  for (it=m_state->m_dataZoneMap.begin(); it!=m_state->m_dataZoneMap.end(); ++it) {
    MWAWEntry const &entry=it->second;
    if (entry.isParsed()) continue;
    f.str("");
    f << "Entries(" << entry.type() << "):";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
  }
  for (it=m_state->m_RSRCZoneMap.begin(); it!=m_state->m_RSRCZoneMap.end(); ++it) {
    MWAWEntry const &entry=it->second;
    if (entry.isParsed()) continue;
    f.str("");
    f << "Entries(" << entry.type() << ")";
    if (entry.id()) f << "[" << entry.id() << "]";
    f << ":";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
  }

  // time to sort the zones by page
  findPagesZones();
  return true;
}

////////////////////////////////////////////////////////////
// find the different zones
////////////////////////////////////////////////////////////

bool RagTimeParser::findDataZones()
{
  MWAWInputStreamPtr input = getInput();
  int const vers=version();
  int const headerSize=vers>=2 ? 156 : 196;
  int const zoneLength=vers>=2 ? 54 : 40;
  m_state->initDefaultPatterns(vers);

  libmwaw::DebugStream f;
  f << "Entries(Zones)[header]:";
  long pos = input->tell();
  input->seek(pos+(vers>=2 ? 48 : 72), librevenge::RVNG_SEEK_SET);
  ascii().addDelimiter(input->tell(),'|');
  m_state->m_numDataZone=(int) input->readULong(2) ;
  long endPos=pos+headerSize+m_state->m_numDataZone*zoneLength;
  f << "num[zones]=" << m_state->m_numDataZone << ",";
  if (m_state->m_numDataZone==0 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::findDataZones: can not find the number of zones\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addDelimiter(input->tell(),'|');
  // we must first read the color map as the frame header can use them
  if (vers==1) {
    input->seek(pos+186, librevenge::RVNG_SEEK_SET);
    MWAWEntry entry;
    entry.setBegin((long) input->readULong(2));
    entry.setType("ColorMap");
    readColorMapV2(entry);
  }
  else if (vers>=2) {
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    if (findRsrcZones())
      readColorsMap();
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+headerSize, librevenge::RVNG_SEEK_SET);
  for (int n=0; n<m_state->m_numDataZone; ++n) {
    pos=input->tell();
    if (!readDataZoneHeader(n+1, endPos)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
  }
  ascii().addPos(endPos);
  ascii().addNote("_");

  return true;
}

bool RagTimeParser::findPagesZones()
{
  std::map<int,RagTimeParserInternal::Zone>::iterator it;
  for (it=m_state->m_idZoneMap.begin(); it!=m_state->m_idZoneMap.end(); ++it) {
    RagTimeParserInternal::Zone const &main=it->second;
    if (main.m_type!=RagTimeParserInternal::Zone::Page)
      continue;
    int zId=main.m_linkZones[3];
    int page=main.m_page;
    std::set<int> seens;
    std::vector<int> lists;
    while (zId) {
      if (seens.find(zId)!=seens.end()) {
        MWAW_DEBUG_MSG(("RagTimeParser::findPagesZones: already find zone %d\n", zId));
        break;
      }
      seens.insert(zId);
      if (m_state->m_idZoneMap.find(zId)==m_state->m_idZoneMap.end()) {
        MWAW_DEBUG_MSG(("RagTimeParser::findPagesZones: %d does not correspond to a zone\n", zId));
        break;
      }
      RagTimeParserInternal::Zone &zone=m_state->m_idZoneMap.find(zId)->second;
      if (zone.m_type==RagTimeParserInternal::Zone::Page) {
        MWAW_DEBUG_MSG(("RagTimeParser::findPagesZones: oops find a page zone %d\n", zId));
        break;
      }
      zone.m_page=page;
      lists.push_back(zId);
      zId=zone.m_linkZones[2];
    }
    if (lists.empty()) continue;
    if (m_state->m_pageZonesIdMap.find(page)!=m_state->m_pageZonesIdMap.end()) {
      MWAW_DEBUG_MSG(("RagTimeParser::findPagesZones: oops already find main zone for %d\n", page));
    }
    else
      m_state->m_pageZonesIdMap[page]=lists;
  }
  return true;
}

bool RagTimeParser::readDataZoneHeader(int id, long endPos)
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  int const vers=version();
  int const zoneLength=vers>=2 ? 54 : 40;
  libmwaw::DebugStream f;
  if (!input->checkPosition(pos+zoneLength)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readDataZoneHeader: the zone seems too short\n"));
    return false;
  }
  RagTimeParserInternal::Zone zone;
  zone.m_subType=(int) input->readULong(1);
  bool hasPointer=true;
  switch (zone.m_subType) {
  case 0: // checkme
    zone.m_type=RagTimeParserInternal::Zone::Page;
    break;
  case 2:
    zone.m_type=RagTimeParserInternal::Zone::Text;
    break;
  case 3:
    zone.m_type=RagTimeParserInternal::Zone::Spreadsheet;
    break;
  case 4:
    zone.m_type=RagTimeParserInternal::Zone::Picture;
    break;
  case 5:
    zone.m_type=RagTimeParserInternal::Zone::Line;
    hasPointer=false;
    break;
  case 6:
    zone.m_type=RagTimeParserInternal::Zone::Chart;
    break;
  default:
    if ((zone.m_subType&0xf)==0xF)
      hasPointer=false;
    break;
  }
  MWAWEntry entry;
  entry.setType(zone.getTypeString());
  entry.setId(id);
  if (hasPointer) {
    input->seek(pos+zoneLength-4, librevenge::RVNG_SEEK_SET);
    long filePos=(long) input->readULong(4);
    if (filePos==0) {
      // checkme: is it normal or not ?
    }
    else if (filePos>0 && filePos<=long(m_state->m_numDataZone)) {
      /* TODO: rare but we must manage that, this seems easy for
         shared text zone, more difficult for other shared zones */
      zone.m_sharedWith=int(filePos);
      static bool first=true;
      if (zone.m_sharedWith && first) {
        first=false;
        MWAW_DEBUG_MSG(("RagTimeParser::readDataZoneHeader: find potential shared zones, not implemented\n"));
      }
    }
    else if (filePos<endPos || !input->checkPosition(filePos)) {
      f << "###";
      MWAW_DEBUG_MSG(("RagTimeParser::readDataZoneHeader: find an odd zone\n"));
    }
    else if (filePos) {
      // checkme: does vers==1 has some flags which explains +4
      entry.setBegin(filePos+(vers==1 ? 4 : 0));
      m_state->m_dataZoneMap.insert
      (std::multimap<std::string,MWAWEntry>::value_type(entry.type(),entry));
    }
  }
  input->seek(pos+1, librevenge::RVNG_SEEK_SET);

  MWAWGraphicStyle &style=zone.m_style;
  style.m_lineWidth=float(input->readULong(1))/8.f;
  int val;
  if (vers>=2) {
    val=(int) input->readLong(2);
    if (val) f << "d0=" << val << ",";
  }
  for (int i=0; i< 4; ++i) {
    // fl0=0|80|81|83|a0|b0|c0,fl1=0|2|80,fl2=1|3|5|9|b|19|1d, fl3=0|1|28|2a|38|...|f8
    val=(int) input->readULong(1);
    switch (i) {
    case 0:
      style.m_rotate=float(zone.m_rotation=((val>>2)&3)*90);
      val&=0xF3;
      break;
    case 1:
      if (val&4) f << "round,";
      if (val&2) {
        style.m_shadowOffset=Vec2f(5,5);
        style.setShadowColor(MWAWColor(128,128,128));
      }
      val&=0xF9;
      break;
    case 2:
      if (val&2)
        f << "selected,";
      if (val&0x80)
        zone.m_read32Size=true;
      val &= 0x7D;
      break;
    default:
      break;
    }

    if (val) f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  float dim[4];
  for (int i=0; i<4; ++i) {
    if (vers >= 2)
      dim[i]=float(input->readLong(4))/65536.f;
    else
      dim[i]=(float) input->readLong(2);
  }
  zone.m_dimension=Box2f(Vec2f(dim[1],dim[0]), Vec2f(dim[3],dim[2]));

  int numZones=m_state->m_numDataZone;
  val=(int) input->readLong(2); // 0, find also 6 for a line ?
  if (val) f << "f0=" << val << ",";
  for (int i=0; i<2; ++i) {
    val=(int) input->readLong(2);
    if (val < 0 || val > numZones) {
      MWAW_DEBUG_MSG(("RagTimeParser::readDataZoneHeader: find unexpected link zone\n"));
      f << "##id[" << i+1 << "]=" << val << ",";
      continue;
    }
    zone.m_linkZones[i==0 ? 2 : 1]=val;
  }
  if (zone.m_type==RagTimeParserInternal::Zone::Page)
    // todo: data probably differs from here
    zone.m_linkZones[3]=(int) input->readLong(2);
  else {
    val=(int) input->readULong(2); //f1=(-20)-14|8001
    if (val==0x8001) // use to indicated the last main zone
      f << "linked=last,";
    else if (val >=0 && val <= numZones)
      zone.m_linkZones[4]=val;
    else
      f << "#linked=" << val << ",";
  }
  for (int i=0; i<(vers>=2 ? 4 : 2); ++i) { // fl5=0|1|2|3|10, fl6=0|1
    val=(int) input->readULong(1);
    if (val) f << "fl" << i+5 << "=" << std::hex << val << std::dec << ",";
  }
  if (zone.m_type==RagTimeParserInternal::Zone::Page) {
    zone.m_page=(int) input->readLong(2);
    f << "pg=" << zone.m_page << ",";
    // todo: data probably differs before
  }
  else {
    val=(int) input->readLong(2);
    if (val) f << "f2=" << val << ",";
    val=(int) input->readLong(2);
    if (val < 0 || val > numZones) {
      MWAW_DEBUG_MSG(("RagTimeParser::readDataZoneHeader: find unexpected parent zone\n"));
      f << "##id[parent]=" << val << ",";
    }
    else
      zone.m_linkZones[0]=val;
    val=(int) input->readULong(1);
    if (val) f << "fl7=" << std::hex << val << std::dec << ",";
    if (vers>=2) {
      val=(int) input->readULong(2);
      if (val) f << "f3=" << val << ",";
    }
    int pattern=(int) input->readLong(1);
    if (pattern!=7) f << "pattern=" << pattern << ",";
    int percentValues[2]= {100,100};
    MWAWColor colors[2]= {MWAWColor::black(), MWAWColor::white()};
    bool hasSurfaceColor=true;
    bool const isLine=zone.m_type==RagTimeParserInternal::Zone::Line;
    int const numColorsInFile=isLine ? 1 : 2;
    for (int i=0; i<numColorsInFile; ++i) {
      static char const *(wh[])= {"line", "surf"};
      int col;
      if (vers<2) {
        percentValues[i]=(int) input->readLong(1);
        col=(int) input->readULong(1);
        if (col==255) col=-1;
      }
      else
        col=(int) input->readLong(2)-1;
      // 255 means default, ...
      if (percentValues[i]<=0 || percentValues[i]>=100) percentValues[i]=100;
      else if (i) percentValues[i]=100-percentValues[i];
      if (percentValues[i]!=100) f << "gray[" << wh[i] << "]=" << percentValues[i] << "%,";

      if (i==1 && col==-1) { // none ?
        hasSurfaceColor=false;
        f << "noColors,";
        continue;
      }
      if (col!=-1 && !getColor(col, colors[i]))
        f << "##color[" << wh[i] << "]=" << col << ",";
      else
        f << "color[" << wh[i] << "]=" << colors[i] << ",";

      colors[i]=MWAWColor::barycenter(float(percentValues[i])/100.f,colors[i],
                                      1.f-float(percentValues[i])/100.f, i==0 ? MWAWColor::white() : MWAWColor::black());
    }

    if (pattern >=0 && pattern<int(m_state->m_patternList.size())) {
      RagTimeParserInternal::Pattern pat=m_state->m_patternList[size_t(pattern)];
      if (isLine)
        style.m_lineColor=MWAWColor::barycenter(1.f-pat.m_percent/100.f,colors[0],
                                                pat.m_percent/100.f, MWAWColor::white());
      else {
        style.m_lineColor=colors[0];
        pat.m_colors[0]=colors[1];
        pat.m_colors[1]=colors[0];
        MWAWColor col;
        if (style.m_pattern.getUniqueColor(col)) {
          if (!col.isWhite())
            style.setBackgroundColor(col);
        }
        else {
          style.m_pattern=pat;
          if (style.m_pattern.getAverageColor(col) && !col.isWhite())
            style.setBackgroundColor(col);
        }
      }
    }
    else {
      MWAW_DEBUG_MSG(("RagTimeParser::readDataZoneHeader: can not find pattern %d\n", pattern));
      style.m_lineColor=colors[0];
      if (!isLine && hasSurfaceColor)
        style.setBackgroundColor(colors[1]);
    }
    if (isLine) {
      if (vers<2) {
        for (int i=0; i<2; ++i) {
          val=(int) input->readULong(1);
          if (val) f << "g" << i << "=" << std::hex << val << std::dec << ",";
        }
      }
      val=(int) input->readULong(1);
      zone.m_arrowFlags=((val>>3)&3);
      if (val&4) {
        f << "mirrorX,";
        float tmp=zone.m_dimension[1][1];
        zone.m_dimension.setMax(Vec2f(zone.m_dimension[1][0], zone.m_dimension[0][1]));
        zone.m_dimension.setMin(Vec2f(zone.m_dimension[0][0], tmp));
      }
      val &=0xE3;
      if (val) f << "flA4=" << std::hex << val << std::dec << ",";
      // not sure are to read the end but as I find 00c0000000: vertical and 0000004000: horizontal, let try
      val=(int) input->readULong(1);
      if (val) f << "g2=" << val << ",";
      int unkn[2];
      for (int i=0; i<2; ++i) unkn[i]=(int) input->readULong(2);
      if (unkn[0]==0xc000 && unkn[1]==0) {
        f << "vertical,";
        zone.m_dimension.setMax(Vec2f(zone.m_dimension[0][0], zone.m_dimension[1][1]));
      }
      else if (unkn[0]==0 && unkn[1]==0x4000) {
        f << "horizontal,";
        zone.m_dimension.setMax(Vec2f(zone.m_dimension[1][0], zone.m_dimension[0][1]));
      }
    }
    else {
      val=(int) input->readULong(1);
      if (val!=100) f << "gray[text]=" << val << "%,";
      val=(int) input->readULong(1);
      if (val!=255  && !getColor(val, zone.m_fontColor))
        f << "##color[text]=" << val << ",";
    }
  }
  zone.m_extra=f.str();
  m_state->m_idZoneMap[id]=zone;
  if (id>m_state->m_actualZoneId) m_state->m_actualZoneId=id;
  f.str("");
  f << "Zones-Z" << id << ":" << zone;

  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+zoneLength, librevenge::RVNG_SEEK_SET);
  return true;
}

bool RagTimeParser::findRsrcZones()
{
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  long pos=input->tell();
  int dSz=(int) input->readULong(2);
  f << "Entries(RsrcMap):";
  if ((dSz%10)!=2 || !input->checkPosition(pos+2+dSz)) {
    MWAW_DEBUG_MSG(("RagTimeParser::findRsrcZones: find odd size\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  // the resource map
  int N=dSz/10;
  f << "N=" << N << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  std::vector<MWAWEntry> entriesList;
  std::set<std::string> seensSet;
  for (int i=0; i<N; ++i) {
    long fPos=input->tell();
    f.str("");
    f << "RsrcMap[" << i << "]:";
    MWAWEntry entry;
    long depl=(long) input->readULong(4);
    entry.setBegin(pos+depl+2);
    std::string what("rsrc");
    for (int c=0; c<4; ++c) {
      char ch=(char) input->readULong(1);
      if (ch==' ') what += '_';
      else what+=ch;
    }
    if (what=="rsrcCHTa") what="Color";
    else if (what=="rsrcFHfl") what="FontName";
    else if (what=="rsrcFHsl") what="CharProp";
    else if (what=="rsrcFLin") what="Link";
    else if (what=="rsrcFoTa") what="NumFormat";
    else if (what=="rsrcRTml") what="Macro";
    /* not really sure, find in v3.2 with value:
      id=2:0011000200000000000c00010647656e6576610000
      id=4:000e00042e0000060000000c000000000000
      id=5:000a00050000000d000100000000
      id=6:00060006000000000000
      id=7:00060007000000030000
      id=8:00060008000000020000
     */
    else if (what=="rsrcRtPr") what="Reserved";
    entry.setType(what);
    entry.setId((int) input->readLong(2));
    f << what << "[" << entry.id() << "],";
    // no data rsrc seems to exists, so...
    if (depl) {
      f << std::hex << entry.begin() << std::dec << ",";
      if (!entry.begin() || !input->checkPosition(entry.begin())) {
        f << "###";
        MWAW_DEBUG_MSG(("RagTimeParser::findRsrcZones: the entry position seems bad\n"));
        entry=MWAWEntry();
      }
    }
    else
      entry=MWAWEntry();
    entriesList.push_back(entry);
    if (!entry.type().empty())
      seensSet.insert(entry.type());
    input->seek(fPos+10, librevenge::RVNG_SEEK_SET);
    ascii().addPos(fPos);
    ascii().addNote(f.str().c_str());
  }
  /* FIXME: some rsrc does not have a name, ie. have "    " as name,
     let use an heuristic to name them...
   */
  for (size_t i=0; i<entriesList.size(); ++i) {
    MWAWEntry entry=entriesList[i];
    if (entry.begin()<=0) continue;
    if (entry.type() != "rsrc____") {
      m_state->m_RSRCZoneMap.insert(std::multimap<std::string,MWAWEntry>::value_type(entry.type(),entry));
      continue;
    }
    entry.setType("rsrcUnamed");
    RagTimeStruct::ResourceList zone;
    if (zone.read(input, entry) && zone.m_type!=RagTimeStruct::ResourceList::Undef) {
      std::string newType("rsrc");
      newType+=RagTimeStruct::ResourceList::getName(zone.m_type);
      if (seensSet.find(newType)==seensSet.end()) {
        MWAW_DEBUG_MSG(("RagTimeParser::findRsrcZones: set the rsrc zone %d to %s\n", int(i), newType.c_str()));
        entry.setType(newType);
      }
    }
    entry.setParsed(false);
    m_state->m_RSRCZoneMap.insert(std::multimap<std::string,MWAWEntry>::value_type(entry.type(),entry));
  }
  return true;
}
////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// picture zone
////////////////////////////////////////////////////////////
bool RagTimeParser::readPictZone(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  int dataFieldSize=getZoneDataFieldSize(entry.id());
  if (pos<=0 || !input->checkPosition(pos+0x48+dataFieldSize)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZone: the position seems bad\n"));
    return false;
  }
  if (version()<2) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZone: must not be called for v1-2... file\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(PictZone):";
  int dSz=(int) input->readULong(dataFieldSize);
  long endPos=pos+dataFieldSize+dSz;
  if (dSz<76 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZone: the header size seems bad\n"));
    f << "###dSz=" << dSz << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int headerSz=(int) input->readULong(2);
  if ((headerSz&0x8000)) // default
    headerSz&=0x7FFF;
  else
    f << "noHeaderflags,";
  if (headerSz>dSz) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZone: the header size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }

  RagTimeParserInternal::Picture pict;
  pict.m_headerPos=entry.begin();

  int val;
  for (int i=0; i<6; ++i) { // f1=0|1|9|10|20
    val=(int) input->readULong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  int bitmapDim[4];
  for (int i=0; i<4; ++i) bitmapDim[i]=(int) input->readLong(2);
  if (bitmapDim[0]||bitmapDim[1]||bitmapDim[2]||bitmapDim[3])
    f << "dim[bitmap?]=" << bitmapDim[1] << "x" << bitmapDim[0] << "<->"
      << bitmapDim[3] << "x" << bitmapDim[2] << ",";

  float dim[4];
  for (int i=0; i<4; ++i) dim[i]=float(input->readULong(4))/65536.f;
  pict.m_dim=Box2f(Vec2f(dim[1],dim[0]),Vec2f(dim[3],dim[2]));
  f << "dim=" << pict.m_dim << ",";
  for (int i=0; i<2; ++i) {
    float dim2[2];
    for (int j=0; j<2; ++j) dim2[j]=float(input->readULong(4))/65536.f;
    if (dim2[0]<dim[2]-dim[0] || dim2[0]>dim[2]-dim[0] ||
        dim2[1]<dim[3]-dim[1] || dim2[1]>dim[3]-dim[1])
      f << "dim" << i+1 << "=" << dim2[1] << "x" << dim2[0] << ",";
  }
  /* checkme: no sure what the different type of pict can be
     - pict[bitmap]:  f4=1-4, f5=1
     - pict: f5=1|3
   */
  for (int i=0; i<7; ++i) {
    val=(int) input->readULong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  pict.m_type=(int) input->readULong(1);
  switch (pict.m_type) {
  case 2:
    f << "pict,";
    break;
  case 3:
    f << "pict+label,";
    break;
  case 6:
    f << "pict[bitmap],";
    break;
  default:
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZone: find unexpected type\n"));
    f << "#type=" << pict.m_type << ",";
    break;
  }

  ascii().addDelimiter(input->tell(),'|');

  input->seek(pos+72+dataFieldSize, librevenge::RVNG_SEEK_SET);
  ascii().addDelimiter(input->tell(),'|');
  long pictSz=(long) input->readULong(2);
  if (headerSz+pictSz>dSz) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZone: the pict size seems bad\n"));
    f << "###pictSz=" << std::hex << pictSz << std::dec << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(pos+dataFieldSize+headerSz, librevenge::RVNG_SEEK_SET);
  pos=input->tell();

  pict.m_pos.setBegin(pos);
  pict.m_pos.setLength(pictSz);
  m_state->m_idPictureMap[entry.id()]=pict;

  input->seek(pos+pictSz, librevenge::RVNG_SEEK_SET);
  pos=input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZone: find some extra data\n"));
    ascii().addPos(pos);
    ascii().addNote("PictZone[end]");
  }
  return true;
}

bool RagTimeParser::readPictZoneV2(MWAWEntry &entry)
{
  if (version()>=2) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZoneV2: must not be called for v3... file\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZoneV2: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(PictZone):";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<0x24 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZoneV2: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(endPos);
  ascii().addNote("_");
  int headerSz=(int) input->readULong(2);
  long endHeader=pos+2+headerSz;
  if (headerSz<0x24 || !input->checkPosition(endHeader)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZoneV2: the header size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }

  RagTimeParserInternal::Picture pict;
  pict.m_headerPos=entry.begin();
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int) input->readULong(2);
  pict.m_dim=Box2f(Vec2f((float)dim[1],(float)dim[0]),Vec2f((float)dim[3],(float)dim[2]));
  f << "dim=" << pict.m_dim << ",";
  for (int i=0; i<2; ++i) {
    int dim2[2];
    for (int j=0; j<2; ++j) dim2[j]=(int) input->readULong(2);
    if (dim2[0]!=dim[2]-dim[0] || dim2[1]!=dim[3]-dim[1])
      f << "dim" << i+1 << "=" << dim2[1] << "x" << dim2[0] << ",";
  }
  /* find for
     - bitmap: headerSz=24, f2=1, f7=1
     - pict simple: headerSz=24, f2=f7=2
     - pict+link: headerSz=78, f2=4, f4=80, f7=3
   */
  for (int i=0; i<6; ++i) {
    int val=(int) input->readULong(2);
    if (val) f << "f" << i+1 << "=" << val << ",";
  }
  pict.m_type=(int) input->readLong(2);
  switch (pict.m_type) {
  case 1:
    f << "bitmap,";
    break;
  case 2:
    f << "pict,";
    break;
  case 3:
    f << "pict+eps,";
    break;
  default:
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZoneV2: find unexpected type\n"));
    f << "#type=" << pict.m_type << ",";
    break;
  }
  f << "id?=" << std::hex << input->readULong(4) << std::dec << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  if (headerSz>0x24) {
    f.str("");
    f << "PictZone[Link]:";
    if (headerSz>0x24+34) {
      f << "id2=" << std::hex << input->readULong(4) << std::dec << ",";
      int val;
      for (int i=0; i<14; ++i) {
        static int const(expected[])= {0,0x1f22,0xffe8,0xff80,0x2c,0x3f,0x2d0,0x2d0,2,0x2d,0x3c,0,0x16,0x9dba};
        val=(int) input->readULong(2);
        if (val!=expected[i])
          f << "f" << i << "=" << std::hex << val << std::dec << ",";
      }
      val=(int) input->readULong(1);
      if (val!=0x5d) f << "f14=" << std::hex << val << std::dec << ",";
      int sSz=(int) input->readULong(1);
      if (input->tell()+sSz<=endHeader) {
        std::string text("");
        for (int i=0; i<sSz; ++i) text+=(char) input->readULong(1);
        f << "file=" << text << ",";
      }
      else {
        MWAW_DEBUG_MSG(("RagTimeParser::readPictZoneV2: can not read the file name\n"));
        f << "#sz=" << sSz << ",";
      }
    }
    ascii().addPos(pos+2+0x24);
    ascii().addNote(f.str().c_str());
  }
  input->seek(endHeader, librevenge::RVNG_SEEK_SET);

  pos=input->tell();
  pict.m_pos.setBegin(pos);
  pict.m_pos.setEnd(endPos);
  m_state->m_idPictureMap[entry.id()]=pict;

  return true;
}

////////////////////////////////////////////////////////////
// color map
////////////////////////////////////////////////////////////
bool RagTimeParser::readColorsMap()
{
  // we must read the colors map in order: first the 0th color map, ...
  for (int i=0; i<3; ++i) {
    std::multimap<std::string,MWAWEntry>::iterator it;
    it=m_state->m_RSRCZoneMap.lower_bound("Color");
    while (it!=m_state->m_RSRCZoneMap.end() && it->first=="Color") {
      MWAWEntry &entry=it++->second;
      if ((i<2 && i==entry.id()) || (i>2 && !entry.isParsed()))
        readColorTable(entry);
    }
  }
  return true;
}

bool RagTimeParser::readColorMapV2(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+8)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readColorMapV2: the position seems bad\n"));
    return false;
  }
  if (version()>=2) {
    MWAW_DEBUG_MSG(("RagTimeParser::readColorMapV2: must only be called for v1-2... file\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(ColorMap):";
  int val=(int) input->readLong(2); // always 0?
  if (val) f << "f0=" << val << ",";
  int dSz=(int) input->readULong(2);
  int N[3];
  for (int i=0; i<3; ++i)
    N[i]=(int) input->readLong(2);
  f << "numColors=" << N[1] << ",";
  if (N[0]+1!=N[1]) {
    MWAW_DEBUG_MSG(("RagTimeParser::readColorMapV2: arghhs N[0]+1!=N[1]\n"));
    f << "###num?=" << N[0] << ",";
  }
  if (N[2]>=0) f << "maxptr=" << N[2] << ",";
  long endPos=pos+4+dSz;
  if (dSz<6+10*N[1]+(N[2]+1)*4+N[1] || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readColorMapV2: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  std::vector<MWAWColor> colorList;
  for (int i=0; i<=N[0]; ++i) {
    //-2: white, -1: black, ...
    pos=input->tell();
    f.str("");
    f << "ColorMap-" << i << ":";
    unsigned char col[3];
    for (int j=0; j < 3; j++)
      col[j] = (unsigned char)(input->readULong(2)>>8);
    MWAWColor color(col[0],col[1],col[2]);
    colorList.push_back(color);
    f << "col=" << color << ",";
    val=(int) input->readULong(2); // 1|2|3|4|22
    if (val) f << "used=" << val << ",";
    f << "id?=" << input->readLong(2) << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  m_state->m_idColorsMap[0]=colorList;
  for (int i=0; i<=N[2]; ++i) {
    pos=input->tell();
    f.str("");
    f << "ColorMap-A" << i << ":";
    val=(int) input->readULong(2); // the index in the text?
    f << "text[pos]?=" << std::hex << val << std::dec << ",";
    val=(int) input->readULong(2); // 1|2|4 color used?
    f << "id?=" << val << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  for (int i=0; i< N[1]; ++i) { // the color name
    pos=input->tell();
    f.str("");
    f << "ColorMap-" << i << ":name=";
    int fSz=(int) input->readULong(1);
    if (pos+1+fSz>endPos) {
      MWAW_DEBUG_MSG(("RagTimeParser::readColorMapV2: can not read a string\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return false;
    }
    std::string text("");
    for (int c=0; c<fSz; ++c) text+=(char) input->readULong(1);
    f << text;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  pos=input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("RagTimeParser::readColorMapV2: find some extra data\n"));
    ascii().addPos(pos);
    ascii().addNote("ColorMap-end:###");
  }
  return true;
}

////////////////////////////////////////////////////////////
// print info
////////////////////////////////////////////////////////////
bool RagTimeParser::readPrintInfo(MWAWEntry &entry, bool inRSRCFork)
{
  MWAWInputStreamPtr input = inRSRCFork ? getRSRCParser()->getInput() : getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+120)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPrintInfo: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile=inRSRCFork ? getRSRCParser()->ascii() : ascii();
  libmwaw::DebugStream f;
  f << "Entries(rsrcPREC):";
  long dSz=inRSRCFork ? entry.length() : (int) input->readULong(2);
  long endPos=input->tell()+dSz;
  libmwaw::PrinterInfo info;
  if (dSz<120 || !input->checkPosition(endPos) || !info.read(input)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPrintInfo: the zone seems too short\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  f << info;

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
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos=input->tell();
  if (pos==endPos) return true;
  // TODO read the last part 0x20(size?) + page dim + some flags
  f.str("");
  f << "rsrcPREC[data]:";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// unknown zone
////////////////////////////////////////////////////////////
bool RagTimeParser::readZone6(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+0x66)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readZone6: the position seems bad\n"));
    return false;
  }
  if (version()<2) {
    MWAW_DEBUG_MSG(("RagTimeParser::readZone6: must not be called for v1-2... file\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(Zone6):";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<0x62 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readZone6: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  std::set<long> beginZones;
  long zonesList[4]= {0,0,0,0};
  int val;
  beginZones.insert(endPos);
  long ptr=(long) input->readULong(4);
  val=(int) input->readULong(2);
  if (ptr) {
    f << "zone0=" << std::hex << pos+2+ptr << std::dec << "[" << val << "],";
    if (pos+2+ptr>endPos) {
      MWAW_DEBUG_MSG(("RagTimeParser::readZone6: the zone 0 seems bad\n"));
      f << "###";
    }
    else {
      beginZones.insert(pos+2+ptr);
      zonesList[0]=pos+2+ptr;
    }
  }
  for (int i=0; i<2; ++i) {
    val=(int) input->readULong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }

  for (int i=0; i<3; ++i) {
    ptr=(long) input->readULong(4);
    val=(int) input->readULong(2);
    if (!ptr) continue;
    f << "zone" << i+1 << "=" << std::hex << pos+2+ptr << std::dec << "[" << val << "],";
    if (pos+2+ptr>endPos) {
      MWAW_DEBUG_MSG(("RagTimeParser::readZone6: the zone %d seems bad\n",i+1));
      f << "###";
      continue;
    }
    beginZones.insert(pos+2+ptr);
    zonesList[i+1]=pos+2+ptr;
  }
  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i=0; i<4; ++i) {
    pos=zonesList[i];
    if (!pos || pos==endPos) continue;
    f.str("");
    f << "Zone6-" << i << ":";
    std::set<long>::const_iterator it=beginZones.find(pos);
    if (it==beginZones.end() || ++it==beginZones.end()) {
      MWAW_DEBUG_MSG(("RagTimeParser::readZone6: can not find the end of zone\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      continue;
    }
    long zEndPos=*it;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    while (!input->isEnd()) {
      pos=input->tell();
      if (pos+4>zEndPos) break;
      dSz=(int) input->readULong(4);
      if (pos+4+dSz>zEndPos) {
        MWAW_DEBUG_MSG(("RagTimeParser::readZone6: can not determine the end of sub zone\n"));
        f << "###";
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        break;
      }
      input->seek(pos+4+dSz, librevenge::RVNG_SEEK_SET);
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
    }
  }
  return true;
}

bool RagTimeParser::readPageZone(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+22)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPageZone: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(PageZone):";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<20 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPageZone: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int val;
  for (int i=0; i<4; i++) { // always 0?
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  int fl=(int) input->readULong(4); // 71606|e2c60|e2c68|e6658
  f << "flag?=" << std::hex << fl << std::dec << ",";
  val=(int) input->readULong(4); // always equal to fl?
  if (val!=fl)
    f << "flag1?=" << std::hex << val << std::dec << ",";
  val=(int) input->readLong(2); // 4|5
  if (val!=4) f << "f4=" << val << ",";
  val=(int) input->readLong(2); // always 0
  if (val) f << "f5=" << val << ",";
  if (input->tell()!=endPos)
    ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// rsrc zone: link
////////////////////////////////////////////////////////////
bool RagTimeParser::readLinks(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x26)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readLinks: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  int headerSz=(int) input->readULong(2);
  int fSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (headerSz<0x20 || fSz<0x10 || dSz<headerSz+(N+1)*fSz || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readLinks: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  std::set<long> posSet;
  posSet.insert(endPos);
  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << entry.type() << "-" << i << ":";
    int val=(int) input->readLong(2); // small number
    if (val) f << "f0=" << val << ",";
    val=(int) input->readLong(2); // always 0?
    if (val) f << "f1=" << val << ",";
    int fPos=(int) input->readULong(2);
    f << "pos[name]=" << std::hex << entry.begin()+2+fPos << std::dec << ",";
    posSet.insert(entry.begin()+2+fPos);
    val=(int) input->readLong(2);
    if (val) f << "fId=" << val << ",";
    ascii().addDelimiter(input->tell(),'|');
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  for (std::set<long>::const_iterator it=posSet.begin(); it!=posSet.end();) {
    pos=*(it++);
    if (pos>=endPos) break;
    long nextPos=it==posSet.end() ? endPos : *it;
    f.str("");
    f << entry.type() << "[name]:";
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    std::string name("");
    while (!input->isEnd() && input->tell()<nextPos) {
      char c=(char) input->readULong(1);
      if (c=='\0') break;
      name+=c;
    }
    f << name;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// rsrc zone: numeric format
////////////////////////////////////////////////////////////
bool RagTimeParser::readFormatsMap()
{
  std::multimap<std::string,MWAWEntry>::iterator it;
  for (it=m_state->m_RSRCZoneMap.begin(); it!=m_state->m_RSRCZoneMap.end(); ++it) {
    std::string const &type=it->first;
    if (type=="NumFormat")
      m_spreadsheetParser->readNumericFormat(it->second);
    else if (type.length()>=7 && type.compare(0,6,"rsrcSp")==0)
      m_spreadsheetParser->readResource(it->second);
  }
  return true;
}

bool RagTimeParser::readColorTable(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x26)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readColorTable: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  std::string what(entry.id()==0 ? "ColorMain" : "ColorList");
  if (entry.id()<0 || entry.id()>1) {
    MWAW_DEBUG_MSG(("RagTimeParser::readColorTable: find unknown color map\n"));
    std::stringstream s;
    s << "ColorTable" << entry.id();
    what=s.str();
  }
  f << "Entries(" << what << "):";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  int headerSz=(int) input->readULong(2);
  int fSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (headerSz<0x20 || fSz<0x8 || dSz<headerSz+(N+1)*fSz || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readColorTable: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  bool okSize=(entry.id()==0&&fSz==12)||(entry.id()==1&&fSz==8);
  if (!okSize) {
    MWAW_DEBUG_MSG(("RagTimeParser::readColorTable: unexpected size for table %d\n", entry.id()));
    f << "###";
  }
  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  std::set<long> posSet;
  posSet.insert(endPos);
  std::vector<MWAWColor> colorList;
  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << what << "-" << i << ":";
    int val=(int) input->readLong(2); // 0 (except last)
    if (val) f << "f0=" << val << ",";
    if (i==N) {
    }
    else if (!okSize)
      f << "###";
    else if (entry.id()==0) {
      int fPos=(int) input->readULong(2);
      if (fPos) {
        f << "pos[def]=" << std::hex << entry.begin()+2+fPos << std::dec << ",";
        posSet.insert(entry.begin()+2+fPos);
      }
      unsigned char col[3];
      for (int j=0; j < 3; j++)
        col[j] = (unsigned char)(input->readULong(2)>>8);
      MWAWColor color(col[0],col[1],col[2]);
      colorList.push_back(color);
      f << "col=" << color << ",";
      f << "id?=" << input->readLong(2) << ",";
    }
    else if (entry.id()==1) {
      val=(int) input->readLong(2);
      if (val) f << "used?=" << val << ",";
      int colId=(int) input->readULong(2)-1;
      MWAWColor col=MWAWColor::white();
      if (!getColor(colId,col,0) && val) { // ie. if the color is not used, this can be normal
        MWAW_DEBUG_MSG(("RagTimeParser::readColorTable: unexpected color id=%d\n", colId));
        f << "###";
      }
      float percent=float(input->readLong(2))/1000.f;
      col=MWAWColor::barycenter(percent,col,1.f-percent, MWAWColor::white());
      colorList.push_back(col);
      f << col << ",";
    }
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  m_state->m_idColorsMap[entry.id()]=colorList;
  for (std::set<long>::const_iterator it=posSet.begin(); it!=posSet.end();) {
    pos=*(it++);
    if (pos>=endPos) break;
    long nextPos=it==posSet.end() ? endPos : *it;
    f.str("");
    f << what << "[name]:";
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    std::string name("");
    while (!input->isEnd() && input->tell()<nextPos) {
      char c=(char) input->readULong(1);
      if (c=='\0') break;
      name+=c;
    }
    f << name;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeParser::readMacroFormats(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x26)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readMacroFormats: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  int headerSz=(int) input->readULong(2);
  int fSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (headerSz<0x20 || fSz<0x10 || dSz<headerSz+(N+1)*fSz || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readMacroFormats: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  std::set<long> posSet;
  posSet.insert(endPos);
  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << entry.type() << "-" << i << ":";
    int fPos=(int) input->readULong(2);
    f << "pos[name]=" << std::hex << entry.begin()+2+fPos << std::dec << ",";
    posSet.insert(entry.begin()+2+fPos);
    ascii().addDelimiter(input->tell(),'|');
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  for (std::set<long>::const_iterator it=posSet.begin(); it!=posSet.end();) {
    pos=*(it++);
    if (pos>=endPos) break;
    long nextPos=it==posSet.end() ? endPos : *it;
    f.str("");
    f << entry.type() << "[name]:";
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    std::string name("");
    while (!input->isEnd() && input->tell()<nextPos) {
      char c=(char) input->readULong(1);
      if (c=='\0') break;
      name+=c;
    }
    f << name;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// other unknown rsrc zone
////////////////////////////////////////////////////////////
bool RagTimeParser::readRsrcCalc(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x26)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcCalc: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(rsrcCalc)[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  int val=(int) input->readLong(2); // always 0
  if (val) f << "f0=" << val << ",";
  val=(int) input->readLong(2); // a small number
  if (val) f << "f1=" << val << ",";
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  long endPos=pos+2+dSz;
  if (dSz!=24+N*26 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcCalc: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  input->seek(pos+2+24, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "rsrcCalc-" << i << ":";

    input->seek(pos+26, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeParser::readRsrcFHwl(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x26)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcFHwl: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(rsrcFHwl)[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  int headerSz=(int) input->readULong(2);
  int fSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (headerSz<0x26 || dSz!=headerSz+(N+1)*fSz || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcFHwl: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  for (int i=0; i<8; ++i) {
    int val=(int) input->readLong(2);
    static int const expected[]= {0x200, 2, 0, 0, 0, 0, 5, 0/* small number*/};
    if (val!=expected[i])
      f << "f" << i+2 << "=" << val << ",";
  }
  for (int i=0; i<8; ++i) { // g4=1, g6=17|21|2b|7b, g7=0x208
    int val=(int) input->readLong(2);
    f << "g" << i << "=" << val << ",";
  }
  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << "rsrcFHwl-" << i << ":";
    /* a small number, except for the last value
       so, maybe the interesting data are at the end of the bloc, in pos+0x210-4 ?
       or the block are subdivided differently
     */
    f << "N=" << input->readLong(2) << ",";
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeParser::readRsrcBeDc(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+52)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcBeDc: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(rsrcBeDc)[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<52 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcBeDc: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int val=(int) input->readLong(2); // alway 1?
  if (val!=1) f << "f0=" << val << ",";
  val=(int) input->readLong(2); // alway 0?
  if (val) f << "f1=" << val << ",";
  for (int i=0; i<2; ++i) { // f2=0|1|11a0|349c|5c74|a250|e09c, f3=0|1|a|17|..2014
    val=(int) input->readULong(2);
    if (val) f << "f" << i+2 << "=" << std::hex << val << std::dec << ",";
  }
  f << "id?=" << std::hex << input->readULong(4) << std::dec << ","; // a really big number
  for (int i=0; i<7; ++i) { // all 0, or g2=1,g3=157,g4=13,g5=1f5|275: (g2,g3,g4,g5) a dim?
    val=(int) input->readULong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  for (int i=0; i<13; ++i) { // begin by small int, h2,h5: maybe two bool,
    val=(int) input->readLong(2);
    if (val) f << "h" << i << "=" << val << ",";
  }
  if (input->tell()!=endPos)
    ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

bool RagTimeParser::readRsrcBtch(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+6)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcBtch: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(rsrcBtch)[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<6 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcBtch: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int val=(int) input->readLong(2); // an id?
  f << "id=" << val << ",";
  for (int i=0; i<2; ++i) { // alway 0?
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

bool RagTimeParser::readRsrcfppr(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x4a)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcfppr: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(rsrcfppr)[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<0x1a || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcfppr: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  for (int i=0; i<5; ++i) {
    int const expected[]= {2, 0x2d, 0x3c, 0, 0x64 };
    int val=(int) input->readLong(2);
    if (val!=expected[i]) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  for (int i=0; i<4; ++i) { // g0-g2: find small number of form 10*k+3 and often g1=g0+10, g2=g1+10 ; g3=0
    int val = (int) input->readLong(4);
    if (val)
      f << "g" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

bool RagTimeParser::readRsrcSele(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+4)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcSele: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(rsrcSele)[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  // checkme: what are the posible size value, I find either 4 or 18, 26, 74
  if ((dSz!=4 && (dSz%8)!=2) || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcSele: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int val=(int) input->readLong(2);
  f << "f0=" << val << ",";
  if (dSz==4) {
    val=(int) input->readLong(2); // maybe 2 bool
    f << "f1=" << val << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return true;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  int N=(dSz/8);
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "rsrcSele-" << i << ":";
    input->seek(pos+8, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeParser::readRsrcStructured(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcStructured: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
  RagTimeStruct::ResourceList zone;
  if (!zone.read(input, entry)) {
    static bool first=true;
    if (first) {
      first=false;
      MWAW_DEBUG_MSG(("RagTimeParser::readRsrcStructured: find some no list field\n"));
    }
    f << "#";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  f << zone;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(zone.m_dataPos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<=zone.m_dataNumber; ++i) {
    pos=input->tell();
    f.str("");
    f << entry.type() << "-" << i << ":";
    input->seek(pos+zone.m_dataSize, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  if (input->tell()!=zone.m_endPos) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcStructured: find some extra data\n"));
    f.str("");
    f << entry.type() << "-end:";
    ascii().addPos(input->tell());
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool RagTimeParser::checkHeader(MWAWHeader *header, bool /*strict*/)
{
  *m_state = RagTimeParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";
  int headerSize=48;
  if (!input->checkPosition(headerSize)) {
    MWAW_DEBUG_MSG(("RagTimeParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,librevenge::RVNG_SEEK_SET);
  /* the document dimension, but find also a spreadsheet size which was remained opened */
  int dim[2];
  dim[0]=(int) input->readULong(4);
  dim[1]=(int) input->readULong(2);
  f << "dim=" << dim[1] << "x" << dim[0] << ",";
  int val=(int) input->readLong(2); // related to page?
  if (val!=1) f << "f0=" << val << ",";
  for (int i=0; i< 2; ++i) {
    f << "unkn" << i << "[";
    for (int j=0; j<7; ++j) {
      val=(int) input->readLong(2);
      static int const expected[]= {0,0,0,0,0x688f,0x688f,0x688f};
      // unkn0: always as expected
      // unkn1: find f0=-13:602, f1=0|...|431, f2=0|228|296
      if (val!=expected[j])
        f << "f" << j << "=" << val << ",";
    }
    f << "],";
  }
  for (int i=0; i<3; ++i) { // f0=0|4|9|a|14|23|29, other 0
    val=(int) input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  // finally a very big number, which seems a multiple of 4 as in 0xb713cfc
  f << "unkn2=" << std::hex << input->readULong(4) << std::dec << ",";
  int vers=(int) input->readULong(1);
  val=(int) input->readULong(1);
  switch (vers) {
  case 0:
    setVersion(1);
    if (val==0x64 || val==0x65)
      f << "v2.1[classic],";
    else // or v1?
      f << "v2[" << std::hex << val << std::dec << "],";
    break;
  case 1:
    setVersion(2);
    if (val==0x2c)
      f << "v3.0,";
    else if (val==0x90) f << "v3.[12],";
    else
      f << "v3[" << std::hex << val << std::dec << "],";
    break;
  default:
    MWAW_DEBUG_MSG(("RagTimeParser::checkHeader: unknown version=%d\n", vers));
    return false;
  }
  input->seek(headerSize, librevenge::RVNG_SEEK_SET);

  // ok, we can finish initialization
  if (header)
    header->reset(MWAWDocument::MWAW_T_RAGTIME, version());

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());
  ascii().addPos(headerSize);

  return true;
}

////////////////////////////////////////////////////////////
// send data to the listener
////////////////////////////////////////////////////////////
bool RagTimeParser::sendPicture(int zId, MWAWPosition const &position)
{
  MWAWListenerPtr listener=getMainListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("RagTimeParser::sendPicture: can not find the listener\n"));
    return false;
  }
  if (m_state->m_idPictureMap.find(zId)==m_state->m_idPictureMap.end()) {
    // can be an empty textbox
    if (m_state->m_idZoneMap.find(zId)==m_state->m_idZoneMap.end()) {
      MWAW_DEBUG_MSG(("RagTimeParser::sendPicture: can not find the picture and zone %d\n", zId));
      return false;
    }
    MWAWGraphicStyle style=m_state->m_idZoneMap.find(zId)->second.m_style;
    if (style.hasLine()) {
      MWAWBorder border;
      border.m_width=style.m_lineWidth;
      border.m_color=style.m_lineColor;
      style.setBorders(0xf, border);
    }
    listener->insertTextBox(position, MWAWSubDocumentPtr(), style);
    return true;
  }
  RagTimeParserInternal::Picture const &pict=m_state->m_idPictureMap.find(zId)->second;
  pict.m_isSent=true;
  if (!pict.m_pos.valid()) return false;

  MWAWInputStreamPtr input = getInput();
  Box2f box;
  input->seek(pict.m_pos.begin(), librevenge::RVNG_SEEK_SET);
  if (pict.m_type==2 || pict.m_type==3 || pict.m_type==6) {
    // first read the picture
    int pictSize=(int) pict.m_pos.length();
    if (pict.m_type==3) {
      pictSize=(int) input->readULong(2);
      input->seek(pict.m_pos.begin(), librevenge::RVNG_SEEK_SET);
    }
    if (!pictSize || pictSize>pict.m_pos.length() ||
        MWAWPictData::check(input, pictSize, box) == MWAWPict::MWAW_R_BAD) {
      MWAW_DEBUG_MSG(("RagTimeParser::sendPicture: can not find picture\n"));
      ascii().addPos(pict.m_pos.begin());
      ascii().addNote("PictZone-data:###");
      return false;
    }
    input->seek(pict.m_pos.begin(), librevenge::RVNG_SEEK_SET);
    shared_ptr<MWAWPict> thePict(MWAWPictData::get(input, pictSize));
    bool ok=bool(thePict);
    if (ok) {
      librevenge::RVNGBinaryData data;
      std::string type;
      if (thePict->getBinary(data,type))
        listener->insertPicture(position, data, type);
    }
#ifdef DEBUG_WITH_FILES
    ascii().skipZone(pict.m_pos.begin(), pict.m_pos.begin()+pictSize-1);
    input->seek(pict.m_pos.begin(), librevenge::RVNG_SEEK_SET);
    librevenge::RVNGBinaryData file;
    input->seek(pict.m_pos.begin(), librevenge::RVNG_SEEK_SET);
    input->readDataBlock(pictSize, file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f;
    f << "PICT-" << ++pictName;
    libmwaw::Debug::dumpFile(file, f.str().c_str());
    if (pict.m_type!=3 || pictSize>=(int) pict.m_pos.length()) return ok;

    long pos=pict.m_pos.begin()+pictSize;
    if (version()>=2) {
      // FIXME: read the label which is a text zone
      MWAW_DEBUG_MSG(("RagTimeParser::sendPicture: sending a label is not implemented\n"));
      ascii().addPos(pos);
      ascii().addNote("PictZone[label]");
      return ok;
    }
    // ok, now a eps file
    ascii().skipZone(pos, pict.m_pos.end()-1);
    file.clear();
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    input->readDataBlock(pict.m_pos.end()-pos, file);
    f.str("");
    f << "PICT-" << pictName << ".eps";
    libmwaw::Debug::dumpFile(file, f.str().c_str());
#endif

    return ok;
  }
  else if (pict.m_type==1)
    return sendBitmap(pict, position);

  MWAW_DEBUG_MSG(("RagTimeParser::sendPicture: do not known how to read the picture\n"));
  ascii().addPos(pict.m_pos.begin());
  ascii().addNote("PictZone-data:###");
  return false;
}

bool RagTimeParser::sendBitmap(RagTimeParserInternal::Picture const &pict, MWAWPosition const &position)
{
  MWAWListenerPtr listener=getMainListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("RagTimeParser::sendBitmap: can not find the listener\n"));
    return false;
  }
  if (pict.m_pos.length()<14) {
    MWAW_DEBUG_MSG(("RagTimeParser::sendBitmap: the picture size seems bad\n"));
    return false;
  }
  int const vers=version();
  MWAWInputStreamPtr input = getInput();
  input->seek(pict.m_pos.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "PictZone-data:";
  // a big number: 804289ce
  f << "f0=" << std::hex << input->readULong(4) << std::dec << ",";
  int rowSize=(int) input->readULong(2);
  for (int i=0; i<2; ++i) { // always 0 or is it some min dimension ?
    int val=(int) input->readULong(2);
    if (val) f << "f" << i+1 << "=" << val << ",";
  }
  int dim[2];
  for (int i=0; i<2; ++i) dim[i]=(int) input->readULong(2);
  f << "dim=" << dim[1] << "x" << dim[0] << ",";
  if (14+dim[0]*rowSize!=pict.m_pos.length() || dim[1]>8*rowSize) {
    MWAW_DEBUG_MSG(("RagTimeParser::sendBitmap: can not read a bitmap\n"));
    f << "###";
    ascii().addPos(pict.m_pos.begin());
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pict.m_pos.begin());
  ascii().addNote(f.str().c_str());
  if (vers>=2) {
    MWAW_DEBUG_MSG(("RagTimeParser::sendBitmap: send bitmap is not implemented\n"));
    return false;
  }
  ascii().skipZone(input->tell(), pict.m_pos.end()-1);
  MWAWPictBitmapBW bitmap(Vec2i(dim[1],dim[0]));
  for (int r=0; r<dim[0]; ++r) {
    long pos=input->tell();
    unsigned long numReads;
    uint8_t const *values=input->read(size_t(rowSize), numReads);
    if (!values || numReads!=(unsigned long) rowSize) {
      MWAW_DEBUG_MSG(("RagTimeParser::sendBitmap: can not read row %d\n", r));
      return false;
    }
    bitmap.setRowPacked(r, (unsigned char const *) values);
    input->seek(pos+rowSize, librevenge::RVNG_SEEK_SET);
  }
  librevenge::RVNGBinaryData data;
  std::string type;
  if (bitmap.getBinary(data,type))
    listener->insertPicture(position, data, type);
  return true;
}

bool RagTimeParser::sendBasicPicture(int zId, MWAWPosition const &position)
{
  MWAWListenerPtr listener=getMainListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("RagTimeParser::sendBasicPicture: can not find the listener\n"));
    return false;
  }
  if (m_state->m_idZoneMap.find(zId)==m_state->m_idZoneMap.end()) {
    MWAW_DEBUG_MSG(("RagTimeParser::sendBasicPicture: can not find the zone %d\n", zId));
    return false;
  }
  RagTimeParserInternal::Zone const &zone=m_state->m_idZoneMap.find(zId)->second;
  zone.m_isSent=true;
  if (zone.m_type!=RagTimeParserInternal::Zone::Line) {
    MWAW_DEBUG_MSG(("RagTimeParser::sendBasicPicture: find unexpected type for zone %d\n", zId));
    return false;
  }
  MWAWGraphicShape shape= MWAWGraphicShape::line(Vec2f(zone.m_dimension[0][0],zone.m_dimension[0][1])-position.origin(),
                          Vec2f(zone.m_dimension[1][0],zone.m_dimension[1][1])-position.origin());
  MWAWGraphicStyle style(zone.m_style);
  if (zone.m_arrowFlags&1) style.m_arrows[0]=true;
  if (zone.m_arrowFlags&2) style.m_arrows[1]=true;
  listener->insertPicture(position, shape, style);
  return true;
}

bool RagTimeParser::sendPageZone(int page)
{
  if (m_state->m_pageZonesIdMap.find(page+1)==m_state->m_pageZonesIdMap.end())
    return true;
  std::vector<int> const &list=m_state->m_pageZonesIdMap.find(page+1)->second;
  for (size_t i=0; i<list.size(); ++i)
    send(list[i]);
  return true;
}

bool RagTimeParser::sendZones()
{
  for (int pg=0; pg<m_state->m_numPages; ++pg)
    sendPageZone(pg);
  return true;
}

bool RagTimeParser::send(int zId)
{
  MWAWListenerPtr listener=getMainListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("RagTimeParser::send: can not find the listener\n"));
    return false;
  }
  if (m_state->m_idZoneMap.find(zId)==m_state->m_idZoneMap.end()) {
    MWAW_DEBUG_MSG(("RagTimeParser::send: can not find the zone %d\n", zId));
    return false;
  }
  RagTimeParserInternal::Zone const &zone=m_state->m_idZoneMap.find(zId)->second;
  Box2i box=zone.getBoundingBox();
  MWAWPosition pos(box[0], box.size(), librevenge::RVNG_POINT);
  pos.m_anchorTo=MWAWPosition::Page;
  if (zone.m_page>0)
    pos.setPage(zone.m_page);
  pos.m_wrapping=MWAWPosition::WRunThrough;
  switch (zone.m_type) {
  case RagTimeParserInternal::Zone::Chart: {
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("RagTimeParser::send: this file contains some charts which will be ignored\n"));
      first=false;
    }
    break;
  }
  case RagTimeParserInternal::Zone::Line:
    return sendBasicPicture(zId, pos);
  case RagTimeParserInternal::Zone::Picture:
    return sendPicture(zId, pos);
  case RagTimeParserInternal::Zone::Spreadsheet:
    return m_spreadsheetParser->send(zId, pos);
  case RagTimeParserInternal::Zone::Text: {
    pos.m_wrapping=MWAWPosition::WBackground;
    MWAWSubDocumentPtr doc(new RagTimeParserInternal::SubDocument(*this, getInput(), zId));
    MWAWGraphicStyle style=zone.m_style;
    if (style.hasLine()) {
      MWAWBorder border;
      border.m_width=style.m_lineWidth;
      border.m_color=style.m_lineColor;
      style.setBorders(0xf, border);
    }
    pos.setSize(Vec2f((float)box.size()[0],(float)-box.size()[1]));
    if (zone.m_rotation) {
      static bool first=true;
      if (first) {
        MWAW_DEBUG_MSG(("RagTimeParser::send: some rotation will be ignored\n"));
        first=false;
      }
    }
    listener->insertTextBox(pos, doc, style);
    return true;
  }
  case RagTimeParserInternal::Zone::Unknown:
  case RagTimeParserInternal::Zone::Page:
  default:
    break;
  }
  return false;
}

void RagTimeParser::flushExtra()
{
  MWAWListenerPtr listener=getMainListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("RagTimeParser::flushExtra: can not find the listener\n"));
    return;
  }
  std::map<int, RagTimeParserInternal::Picture >::const_iterator pIt;
  for (pIt=m_state->m_idPictureMap.begin(); pIt!=m_state->m_idPictureMap.end(); ++pIt) {
    RagTimeParserInternal::Picture const &pict=pIt->second;
    if (pict.m_isSent) continue;
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("RagTimeParser::flushExtra: find some unsent picture zone\n"));
      first=false;
    }
    MWAWPosition pos(Vec2f(0,0), Vec2f(200,200), librevenge::RVNG_POINT);
    pos.m_anchorTo=MWAWPosition::Char;
    sendPicture(pIt->first, pos);
    listener->insertEOL();
  }
  std::map<int, RagTimeParserInternal::Zone >::const_iterator zIt;
  for (zIt=m_state->m_idZoneMap.begin(); zIt!=m_state->m_idZoneMap.end(); ++zIt) {
    RagTimeParserInternal::Zone const &zone=zIt->second;
    if (zone.m_isSent || zone.m_type != RagTimeParserInternal::Zone::Line) continue;
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("RagTimeParser::flushExtra: find some unsent line zone\n"));
      first=false;
    }
    MWAWPosition pos(Vec2f(0,0), Vec2f(50,50), librevenge::RVNG_POINT);
    pos.m_anchorTo=MWAWPosition::Char;
    sendBasicPicture(zIt->first, pos);
    listener->insertEOL();
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
