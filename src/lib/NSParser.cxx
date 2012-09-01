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
#include <sstream>

#include <libwpd/WPXString.h>

#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"

#include "NSParser.hxx"

/** Internal: the structures of a NSParser */
namespace NSParserInternal
{
/** Internal: the fonts and many other data*/
struct Font {
  //! the constructor
  Font(): m_font(-1,-1), m_pictureId(0), m_pictureHeight(0), m_markId(-1),
    m_format(0), m_format2(0), m_extra("") { }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Font const &font);

  //! the font
  MWAWFont m_font;
  //! the picture id ( if this is for a picture )
  int m_pictureId;
  //! the picture height
  int m_pictureHeight;
  //! a mark id
  int m_markId;
  //! the main format ...
  int m_format;
  //! a series of flags
  int m_format2;
  //! extra data
  std::string m_extra;
};


std::ostream &operator<<(std::ostream &o, Font const &font)
{
  if (font.m_pictureId) o << "pictId=" << font.m_pictureId << ",";
  if (font.m_pictureHeight) o << "pictH=" << font.m_pictureHeight << ",";
  if (font.m_markId >= 0) o << "markId=" << font.m_markId << ",";
  if (font.m_format2&0x4) o << "index,";
  if (font.m_format2&0x8) o << "TOC,";
  if (font.m_format2&0x10) o << "samePage,";
  if (font.m_format2&0x20) o << "field,";
  if (font.m_format2&0x40) o << "hyphenate,";
  if (font.m_format2&0x83)
    o << "#format2=" << std::hex << (font.m_format2 &0x83) << std::dec << ",";

  if (font.m_format & 1) o << "noSpell,";
  if (font.m_format & 8) o << "REVERTED,"; // writing is Right->Left
  if (font.m_format & 0x10) o << "sameLine,";
  if (font.m_format & 0x40) o << "endOfPage,"; // checkme
  if (font.m_format & 0xA6)
    o << "#fl=" << std::hex << (font.m_format & 0xA6) << std::dec << ",";
  if (font.m_extra.length())
    o << font.m_extra << ",";
  return o;
}

/** Internal: class to store the paragraph properties */
struct Paragraph : public MWAWParagraph {
  //! Constructor
  Paragraph() : MWAWParagraph(), m_name("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Paragraph const &ind) {
    o << reinterpret_cast<MWAWParagraph const &>(ind);
    if (ind.m_name.length()) o << "name=" << ind.m_name << ",";
    return o;
  }
  //! the paragraph name
  std::string m_name;
};

/** different types
 *
 * - Format: font properties
 * - Ruler: new ruler
 */
enum PLCType { P_Format=0, P_Ruler, P_Footnote, P_HeaderFooter, P_Unknown};

/** Internal: class to store the PLC: Pointer List Content ? */
struct DataPLC {
  DataPLC() : m_type(P_Format), m_id(-1), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, DataPLC const &plc);
  //! PLC type
  PLCType m_type;
  //! the id
  int m_id;
  //! an extra data to store message ( if needed )
  std::string m_extra;
};
//! operator<< for DataPLC
std::ostream &operator<<(std::ostream &o, DataPLC const &plc)
{
  switch(plc.m_type) {
  case P_Format:
    o << "F";
    break;
  case P_Ruler:
    o << "R";
    break;
  case P_Footnote:
    o << "Fn";
    break;
  case P_HeaderFooter:
    o << "HF";
    break;
  case P_Unknown:
  default:
    o << "#type=" << int(plc.m_type) << ",";
  }
  if (plc.m_id >= 0) o << plc.m_id << ",";
  else o << "_";
  if (plc.m_extra.length()) o << plc.m_extra;
  return o;
}

enum ZoneType { Main=0, Footnote, HeaderFooter };

struct Zone {
  typedef std::multimap<NSPosition,DataPLC,NSPosition::Compare> PLCMap;

  //! constructor
  Zone() : m_entry(), m_paragraphList(), m_plcMap() {
  }
  //! the position of text in the rsrc file
  MWAWEntry m_entry;
  //! the list of paragraph
  std::vector<Paragraph> m_paragraphList;
  //! the map pos -> format id
  PLCMap m_plcMap;
};

////////////////////////////////////////
//! Internal: low level a structure helping to read recursifList
struct RecursifData {
  RecursifData(ZoneType zone) : m_zoneId(zone), m_actualPos(), m_positionList(), m_entryList() {
  }
  //! the zone id
  ZoneType m_zoneId;
  //! the actual position
  Vec3i m_actualPos;
  //! the list of read position
  std::vector<Vec3i> m_positionList;
  //! the list of data entry
  std::vector<MWAWEntry> m_entryList;
};

////////////////////////////////////////
//! Internal: the state of a NSParser
struct State {
  //! constructor
  State() : m_fontList(),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)

  {
  }

  /** the font list */
  std::vector<Font> m_fontList;
  /** the main zones : Main, Footnote, HeaderFooter */
  Zone m_zones[3];

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a NSParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(NSParser &pars, MWAWInputStreamPtr input, int zoneId) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! returns the subdocument \a id
  int getId() const {
    return m_id;
  }
  //! sets the subdocument \a id
  void setId(int vid) {
    m_id = vid;
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  //! the subdocument id
  int m_id;
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  NSContentListener *listen = dynamic_cast<NSContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
    return;
  }
  if (m_id != 1 && m_id != 2) {
    MWAW_DEBUG_MSG(("SubDocument::parse: unknown zone\n"));
    return;
  }

  assert(m_parser);

  long pos = m_input->tell();
  // reinterpret_cast<NSParser *>(m_parser)->sendWindow(m_id);
  m_input->seek(pos, WPX_SEEK_SET);
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
NSParser::NSParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_listener(), m_convertissor(), m_state(),
  m_pageSpan()
{
  init();
}

NSParser::~NSParser()
{
  if (m_listener.get()) m_listener->endDocument();
}

void NSParser::init()
{
  m_convertissor.reset(new MWAWFontConverter);
  m_listener.reset();
  setAsciiName("main-1");

  m_state.reset(new NSParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  m_pageSpan.setMarginTop(0.1);
  m_pageSpan.setMarginBottom(0.1);
  m_pageSpan.setMarginLeft(0.1);
  m_pageSpan.setMarginRight(0.1);
}

void NSParser::setListener(NSContentListenerPtr listen)
{
  m_listener = listen;
}

MWAWInputStreamPtr NSParser::rsrcInput()
{
  return getRSRCParser()->getInput();
}

libmwaw::DebugFile &NSParser::rsrcAscii()
{
  return getRSRCParser()->ascii();
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
float NSParser::pageHeight() const
{
  return float(m_pageSpan.getFormLength()-m_pageSpan.getMarginTop()-m_pageSpan.getMarginBottom()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0);
}

float NSParser::pageWidth() const
{
  return float(m_pageSpan.getFormWidth()-m_pageSpan.getMarginLeft()-m_pageSpan.getMarginRight());
}


////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void NSParser::newPage(int number)
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
void NSParser::parse(WPXDocumentInterface *docInterface)
{
  assert(getInput().get() != 0 && getRSRCParser());

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = false;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());
    checkHeader(0L);
    ok = createZones();
    ok = false;
    if (ok) {
      createDocument(docInterface);
    }

    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("NSParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void NSParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (m_listener) {
    MWAW_DEBUG_MSG(("NSParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan ps(m_pageSpan);
  m_state->m_numPages = 1;
  for (int i = 0; i <= m_state->m_numPages; i++) pageList.push_back(ps);

  //
  NSContentListenerPtr listen(new NSContentListener(pageList, documentInterface));
  setListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool NSParser::createZones()
{
  std::multimap<std::string, MWAWEntry> &entryMap
    = getRSRCParser()->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;

  // the 128 id
  it = entryMap.lower_bound("PREC");
  while (it != entryMap.end()) {
    if (it->first != "PREC")
      break;
    MWAWEntry &entry = it++->second;
    readPrintInfo(entry);
  }
  it = entryMap.lower_bound("CPRC");
  while (it != entryMap.end()) {
    if (it->first != "CPRC")
      break;
    MWAWEntry &entry = it++->second;
    readCPRC(entry);
  }
  it = entryMap.lower_bound("PGLY");
  while (it != entryMap.end()) {
    if (it->first != "PGLY")
      break;
    MWAWEntry &entry = it++->second;
    readPGLY(entry);
  }
  // 20000
  it = entryMap.lower_bound("PGRA");
  while (it != entryMap.end()) {
    if (it->first != "PGRA")
      break;
    MWAWEntry &entry = it++->second;
    readPGRA(entry);
  }

  // the 100* id
  it = entryMap.lower_bound("FLST");
  while (it != entryMap.end()) {
    if (it->first != "FLST")
      break;
    MWAWEntry &entry = it++->second;
    readFontsList(entry);
  }

  // the different pict zones
  it = entryMap.lower_bound("PICT");
  while (it != entryMap.end()) {
    if (it->first != "PICT")
      break;
    MWAWEntry &entry = it++->second;
    WPXBinaryData data;
    getRSRCParser()->parsePICT(entry, data);
  }
  it = entryMap.lower_bound("RSSO");
  while (it != entryMap.end()) {
    if (it->first != "RSSO")
      break;
    MWAWEntry &entry = it++->second;
    WPXBinaryData data;
    getRSRCParser()->parsePICT(entry, data);
  }

  // The style zone

  // style name ( can also contains some flags... )
  it = entryMap.lower_bound("STNM");
  while (it != entryMap.end()) {
    if (it->first != "STNM")
      break;
    MWAWEntry &entry = it++->second;
    std::vector<std::string> list;
    readStringsList(entry, list);
  }
  // style link to paragraph name
  it = entryMap.lower_bound("STRL");
  while (it != entryMap.end()) {
    if (it->first != "STRL")
      break;
    MWAWEntry &entry = it++->second;
    std::vector<std::string> list;
    readStringsList(entry, list);
  }
  // style next name
  it = entryMap.lower_bound("STNX");
  while (it != entryMap.end()) {
    if (it->first != "STNX")
      break;
    MWAWEntry &entry = it++->second;
    std::vector<std::string> list;
    readStringsList(entry, list);
  }

  it = entryMap.lower_bound("STYL");
  while (it != entryMap.end()) {
    if (it->first != "STYL")
      break;
    MWAWEntry &entry = it++->second;
    readFonts(entry);
  }

  // the fonts (global)
  it = entryMap.lower_bound("FTAB");
  while (it != entryMap.end()) {
    if (it->first != "FTAB")
      break;
    MWAWEntry &entry = it++->second;
    readFonts(entry);
  }
  // the main zone paragraph (id 1003) + name paragraph (id 1004)
  it = entryMap.lower_bound("RULE");
  while (it != entryMap.end()) {
    if (it->first != "RULE")
      break;
    MWAWEntry &entry = it++->second;
    readParagraphs(entry, NSParserInternal::Main);
  }
  // the main zone changing of font position
  it = entryMap.lower_bound("FRMT");
  while (it != entryMap.end()) {
    if (it->first != "FRMT")
      break;
    MWAWEntry &entry = it++->second;
    readFRMT(entry, NSParserInternal::Main);
  }
  it = entryMap.lower_bound("CNTR");
  while (it != entryMap.end()) {
    if (it->first != "CNTR")
      break;
    MWAWEntry &entry = it++->second;
    readCNTR(entry, NSParserInternal::Main);
  }
  it = entryMap.lower_bound("DPND");
  while (it != entryMap.end()) {
    if (it->first != "DPND")
      break;
    MWAWEntry &entry = it++->second;
    readNumberingReset(entry, NSParserInternal::Main);
  }
  it = entryMap.lower_bound("PICD");
  while (it != entryMap.end()) {
    if (it->first != "PICD")
      break;
    MWAWEntry &entry = it++->second;
    readPICD(entry, NSParserInternal::Main);
  }
  it = entryMap.lower_bound("PLAC");
  while (it != entryMap.end()) {
    if (it->first != "PLAC")
      break;
    MWAWEntry &entry = it++->second;
    readPLAC(entry);
  }
  it = entryMap.lower_bound("DSPL");
  while (it != entryMap.end()) {
    if (it->first != "DSPL")
      break;
    MWAWEntry &entry = it++->second;
    entry.setName("NumbDef");
    NSParserInternal::RecursifData data(NSParserInternal::Main);
    readRecursifList(entry, data);
    readNumberingDef(data);
  }
  it = entryMap.lower_bound("MRK7");
  while (it != entryMap.end()) {
    if (it->first != "MRK7")
      break;
    MWAWEntry &entry = it++->second;
    entry.setName("Mark");
    NSParserInternal::RecursifData data(NSParserInternal::Main);
    readRecursifList(entry, data);
    readMark(data);
  }
  it = entryMap.lower_bound("PLDT");
  while (it != entryMap.end()) {
    if (it->first != "PLDT")
      break;
    MWAWEntry &entry = it++->second;
    entry.setName("PLDT");
    NSParserInternal::RecursifData data(NSParserInternal::Main);
    readRecursifList(entry, data);
  }
  it = entryMap.lower_bound("SGP1");
  while (it != entryMap.end()) {
    if (it->first != "SGP1")
      break;
    MWAWEntry &entry = it++->second;
    entry.setName("SGP1");
    NSParserInternal::RecursifData data(NSParserInternal::Main);
    readRecursifList(entry, data);
  }
  it = entryMap.lower_bound("VARI");
  while (it != entryMap.end()) {
    if (it->first != "VARI")
      break;
    MWAWEntry &entry = it++->second;
    entry.setName("VARI");
    NSParserInternal::RecursifData data(NSParserInternal::Main);
    readRecursifList(entry, data);
  }
  it = entryMap.lower_bound("VRS ");
  while (it != entryMap.end()) {
    if (it->first != "VRS ")
      break;
    MWAWEntry &entry = it++->second;
    entry.setName("VRS");
    NSParserInternal::RecursifData data(NSParserInternal::Main);
    readRecursifList(entry, data);
  }
  it = entryMap.lower_bound("XMRK"); // related to mark7 ?
  while (it != entryMap.end()) {
    if (it->first != "XMRK")
      break;
    MWAWEntry &entry = it++->second;
    entry.setName("XMRK");
    NSParserInternal::RecursifData data(NSParserInternal::Main);
    readRecursifList(entry, data);
  }

  // header zone :
  it = entryMap.lower_bound("HF  "); // header
  while (it != entryMap.end()) {
    if (it->first != "HF  ")
      break;
    MWAWEntry &entry = it++->second;
    readHFHeader(entry);
  }
  it = entryMap.lower_bound("HRUL"); // ruler
  while (it != entryMap.end()) {
    if (it->first != "HRUL")
      break;
    MWAWEntry &entry = it++->second;
    readParagraphs(entry, NSParserInternal::HeaderFooter);
  }
  it = entryMap.lower_bound("HFRM");
  while (it != entryMap.end()) {
    if (it->first != "HFRM")
      break;
    MWAWEntry &entry = it++->second;
    readFRMT(entry, NSParserInternal::HeaderFooter);
  }
  it = entryMap.lower_bound("HCNT");
  while (it != entryMap.end()) {
    if (it->first != "HCNT")
      break;
    MWAWEntry &entry = it++->second;
    readCNTR(entry, NSParserInternal::HeaderFooter);
  }
  it = entryMap.lower_bound("HDPN");
  while (it != entryMap.end()) {
    if (it->first != "HDPN")
      break;
    MWAWEntry &entry = it++->second;
    readNumberingReset(entry, NSParserInternal::HeaderFooter);
  }
  it = entryMap.lower_bound("HPIC");
  while (it != entryMap.end()) {
    if (it->first != "HPIC")
      break;
    MWAWEntry &entry = it++->second;
    readPICD(entry, NSParserInternal::HeaderFooter);
  }
  it = entryMap.lower_bound("HMRK");
  while (it != entryMap.end()) {
    if (it->first != "HMRK")
      break;
    MWAWEntry &entry = it++->second;
    entry.setName("Mark");
    NSParserInternal::RecursifData data(NSParserInternal::HeaderFooter);
    readRecursifList(entry, data);
    readMark(data);
  }
  it = entryMap.lower_bound("HVAR");
  while (it != entryMap.end()) {
    if (it->first != "HVAR")
      break;
    MWAWEntry &entry = it++->second;
    entry.setName("VARI");
    NSParserInternal::RecursifData data(NSParserInternal::HeaderFooter);
    readRecursifList(entry, data);
  }
  it = entryMap.lower_bound("HVRS");
  while (it != entryMap.end()) {
    if (it->first != "HVRS")
      break;
    MWAWEntry &entry = it++->second;
    entry.setName("VRS");
    NSParserInternal::RecursifData data(NSParserInternal::HeaderFooter);
    readRecursifList(entry, data);
  }
  it = entryMap.lower_bound("HFTX"); // text
  while (it != entryMap.end()) {
    if (it->first != "HFTX")
      break;
    m_state->m_zones[NSParserInternal::HeaderFooter].m_entry = it++->second;
  }

  // footnote zone :
  it = entryMap.lower_bound("FOOT"); // header
  while (it != entryMap.end()) {
    if (it->first != "FOOT")
      break;
    MWAWEntry &entry = it++->second;
    readFootnoteHeader(entry);
  }
  it = entryMap.lower_bound("FRUL"); // ruler
  while (it != entryMap.end()) {
    if (it->first != "FRUL")
      break;
    MWAWEntry &entry = it++->second;
    readParagraphs(entry, NSParserInternal::Footnote);
  }
  it = entryMap.lower_bound("FFRM");
  while (it != entryMap.end()) {
    if (it->first != "FFRM")
      break;
    MWAWEntry &entry = it++->second;
    readFRMT(entry, NSParserInternal::Footnote);
  }
  it = entryMap.lower_bound("FCNT"); // never seens but probably exists
  while (it != entryMap.end()) {
    if (it->first != "FCNT")
      break;
    MWAWEntry &entry = it++->second;
    readCNTR(entry, NSParserInternal::Footnote);
  }
  it = entryMap.lower_bound("FDPN"); // never seens but probably exists
  while (it != entryMap.end()) {
    if (it->first != "FDPN")
      break;
    MWAWEntry &entry = it++->second;
    readNumberingReset(entry, NSParserInternal::Footnote);
  }
  it = entryMap.lower_bound("FPIC"); // never seens but probably exists
  while (it != entryMap.end()) {
    if (it->first != "FPIC")
      break;
    MWAWEntry &entry = it++->second;
    readPICD(entry, NSParserInternal::Footnote);
  }
  it = entryMap.lower_bound("FMRK");
  while (it != entryMap.end()) {
    if (it->first != "FMRK")
      break;
    MWAWEntry &entry = it++->second;
    entry.setName("Mark");
    NSParserInternal::RecursifData data(NSParserInternal::Footnote);
    readRecursifList(entry, data);
    readMark(data);
  }
  it = entryMap.lower_bound("FVAR");
  while (it != entryMap.end()) {
    if (it->first != "FVAR")
      break;
    MWAWEntry &entry = it++->second;
    entry.setName("VARI");
    NSParserInternal::RecursifData data(NSParserInternal::Footnote);
    readRecursifList(entry, data);
  }
  it = entryMap.lower_bound("FVRS"); // never seens but probably exists
  while (it != entryMap.end()) {
    if (it->first != "FVRS")
      break;
    MWAWEntry &entry = it++->second;
    entry.setName("VRS");
    NSParserInternal::RecursifData data(NSParserInternal::Footnote);
    readRecursifList(entry, data);
  }
  it = entryMap.lower_bound("FNTX"); // text
  while (it != entryMap.end()) {
    if (it->first != "FNTX")
      break;
    m_state->m_zones[NSParserInternal::Footnote].m_entry = it++->second;
  }
  // fixme
  m_state->m_zones[NSParserInternal::Main].m_entry.setBegin(0);
  m_state->m_zones[NSParserInternal::Main].m_entry.setEnd(0xFFFFFF);
  for (int i = 0; i < 3; i++) {
    if (m_state->m_zones[i].m_entry.valid())
      sendText(i);
  }
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
bool NSParser::checkHeader(MWAWHeader *header, bool /*strict*/)
{
  *m_state = NSParserInternal::State();
  if (!getRSRCParser())
    return false;
  MWAWRSRCParser::Version vers;
  MWAWEntry entry = getRSRCParser()->getEntry("vers", 1);
  if (!entry.valid() || !getRSRCParser()->parseVers(entry, vers)) {
    MWAW_DEBUG_MSG(("NSParser::checkHeader: can not find the version\n"));
    return false;
  }
  switch(vers.m_majorVersion) {
  case 3:
  case 4:
    MWAW_DEBUG_MSG(("NSParser::checkHeader: find Nisus file version %d\n", vers.m_majorVersion));
    break;
  default:
    MWAW_DEBUG_MSG(("NSParser::checkHeader: find Nisus file with unknown version %d\n", vers.m_majorVersion));
    return false;
  }
  setVersion(vers.m_majorVersion);
  if (header)
    header->reset(MWAWDocument::NISUSW, version());

  return true;
}

////////////////////////////////////////////////////////////
// read a  of string
////////////////////////////////////////////////////////////
bool NSParser::sendText(int zoneId)
{
  if (zoneId < 0 || zoneId >= 3) {
    MWAW_DEBUG_MSG(("NSParser::sendText: find unexpected zoneId: %d\n", zoneId));
    return false;
  }
  NSParserInternal::Zone &zone = m_state->m_zones[zoneId];
  MWAWEntry entry = zone.m_entry;
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("NSParser::sendText: the entry is bad\n"));
    return false;
  }

  entry.setParsed(true);
  MWAWInputStreamPtr input = zoneId == 0 ? getInput() : rsrcInput();
  libmwaw::DebugFile &ascFile = zoneId == 0 ? ascii() : rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(TEXT)[" << zoneId << "]:";
  std::string str("");
  NSPosition actPos;
  NSParserInternal::Zone::PLCMap::iterator it = zone.m_plcMap.begin();
  // fixme: treat here the first char
  for (int i = 0; i < entry.length(); i++) {
    if (input->atEOS())
      break;
    char c = (char) input->readULong(1);
    while (it != zone.m_plcMap.end() && it->first.cmp(actPos) <= 0) {
      NSPosition const &plcPos = it->first;
      NSParserInternal::DataPLC const &plc = it++->second;
      f << str;
      str="";
      if (plcPos.cmp(actPos) < 0) {
        MWAW_DEBUG_MSG(("NSParser::sendText: oops find unexpected position\n"));
        f << "###[" << plc << "]";
        continue;
      }
      f << "[" << plc << "]";
    }

    // 0xc: page break
    // 0xd: line break
    // 0xf: date
    // 0x10: time
    // 0x1d: chapter
    str+=(char) c;
    if (c==0xd) {
      f << str;
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      str = "";
      pos = input->tell();
      f.str("");
      f << "TEXT" << zoneId << ":";
    }

    switch(c) {
    case 0xd:
      actPos.m_paragraph++;
      actPos.m_word = actPos.m_char = 0;
      break;
    case '\t':
    case ' ':
      actPos.m_word++;
      actPos.m_char = 0;
      break;
    default:
      actPos.m_char++;
      break;
    }
  }
  f << str;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the header/footer main entry
////////////////////////////////////////////////////////////
bool NSParser::readHFHeader(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%32)) {
    MWAW_DEBUG_MSG(("NSParser::readHFHeader: the entry is bad\n"));
    return false;
  }
  NSParserInternal::Zone &zone = m_state->m_zones[NSParserInternal::HeaderFooter];
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int numElt = int(entry.length()/32);
  libmwaw::DebugStream f;
  f << "Entries(HFHeader)[" << entry.id() << "]:N=" << numElt;
  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());

  NSParserInternal::DataPLC plc;
  plc.m_type = NSParserInternal::P_HeaderFooter;
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();
    f.str("");
    f << "HFHeader" << i << ":";
    long textPara = (long) input->readULong(4); // checkme or ??? m_paragraph
    f << "textParag[def]=" << textPara << ",";
    NSPosition headerPosition;
    headerPosition.m_paragraph = (int) input->readULong(4); // checkme or ??? m_paragraph
    f << "lastParag=" << headerPosition.m_paragraph << ",";
    plc.m_id = i+1;
    zone.m_plcMap.insert(NSParserInternal::Zone::PLCMap::value_type(headerPosition, plc));

    rsrcAscii().addDelimiter(input->tell(),'|');
    rsrcAscii().addPos(pos);
    rsrcAscii().addNote(f.str().c_str());
    input->seek(pos+32, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the footnote main entry
////////////////////////////////////////////////////////////
bool NSParser::readFootnoteHeader(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%36)) {
    MWAW_DEBUG_MSG(("NSParser::readFootnoteHeader: the entry is bad\n"));
    return false;
  }
  NSParserInternal::Zone &mainZone = m_state->m_zones[NSParserInternal::Main];
  NSParserInternal::Zone &zone = m_state->m_zones[NSParserInternal::Footnote];
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int numElt = int(entry.length()/36);
  libmwaw::DebugStream f;
  f << "Entries(FootnoteHeader)[" << entry.id() << "]:N=" << numElt;
  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());

  NSParserInternal::DataPLC plc;
  plc.m_type = NSParserInternal::P_Footnote;
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();
    f.str("");
    f << "FootnoteHeader" << i << ":";
    NSPosition textPosition;
    textPosition.m_paragraph = (int) input->readULong(4); // checkme or ??? m_paragraph
    textPosition.m_word = (int) input->readULong(2);
    textPosition.m_char = (int) input->readULong(2);
    f << "pos=" << textPosition << ",";
    NSPosition notePosition;
    notePosition.m_paragraph = (int) input->readULong(4); // checkme or ??? m_paragraph
    f << "lastParag[inNote]=" << notePosition.m_paragraph << ",";
    rsrcAscii().addDelimiter(input->tell(),'|');
    rsrcAscii().addDelimiter(pos+24,'|');
    for (int wh = 0; wh < 2; wh++) {
      input->seek(pos+24+wh*6, WPX_SEEK_SET);
      std::string label("");
      for (int c = 0; c < 6; c++) {
        char ch = (char) input->readULong(1);
        if (ch == 0)
          break;
        label += ch;
      }
      if (wh==0) f << "label[note]=" << label << ",";
      else f << "label[text]=" << label << ",";
    }

    plc.m_id = i;
    mainZone.m_plcMap.insert(NSParserInternal::Zone::PLCMap::value_type(textPosition, plc));
    plc.m_id = i+1;
    zone.m_plcMap.insert(NSParserInternal::Zone::PLCMap::value_type(notePosition, plc));

    rsrcAscii().addPos(pos);
    rsrcAscii().addNote(f.str().c_str());
    input->seek(pos+36, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read a list of string
////////////////////////////////////////////////////////////
bool NSParser::readStringsList(MWAWEntry const &entry, std::vector<std::string> &list)
{
  list.resize(0);
  if (!entry.valid() && entry.length()!=0) {
    MWAW_DEBUG_MSG(("NSParser::readStringsList: the entry is bad\n"));
    return false;
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());

  while(!input->atEOS()) {
    pos = input->tell();
    if (pos == entry.end()) break;
    if (pos+2 > entry.end()) {
      f.str("");
      f << entry.type() << "###";
      rsrcAscii().addPos(pos);
      rsrcAscii().addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("NSParser::readStringsList: can not read strings\n"));
      return false;
    }
    int sz = (int)input->readULong(2);
    if (pos+2+sz > entry.end()) {
      f.str("");
      f << entry.type() << "###";
      rsrcAscii().addPos(pos);
      rsrcAscii().addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("NSParser::readStringsList: zone size is bad\n"));
      return false;
    }
    f.str("");
    f << entry.type() << list.size() << ":";

    /* checkme: in STNM we can have a list of string, it is general or
       do we need to create a new functionNSParser::readStringsListList
     */
    long endPos = pos+2+sz;
    std::string str("");
    while (input->tell() < endPos-1) {
      int pSz = (int)input->readULong(1);
      if (pSz == 0xFF) {
        f << "_";
        pSz = 0;
      }
      if (input->tell()+pSz > endPos || input->atEOS()) {
        f << "###";
        rsrcAscii().addPos(pos);
        rsrcAscii().addNote(f.str().c_str());
        MWAW_DEBUG_MSG(("NSParser::readStringsList: string size is too big\n"));
        return false;
      }
      std::string str1("");
      for (int c=0; c < pSz; c++)
        str1 += (char) input->readULong(1);
      f << str1 << ",";
      str += str1;
      if ((pSz%2)==0) input->seek(1,WPX_SEEK_CUR);
    }
    list.push_back(str);
    rsrcAscii().addPos(pos);
    rsrcAscii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// read a list of fonts
////////////////////////////////////////////////////////////
bool NSParser::readFontsList(MWAWEntry const &entry)
{
  if (!entry.valid() && entry.length()!=0) {
    MWAW_DEBUG_MSG(("NSParser::readFontsList: the entry is bad\n"));
    return false;
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(FontNames)[" << entry.id() << "]:";
  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());

  int num=0;
  while(!input->atEOS()) {
    pos = input->tell();
    if (pos == entry.end()) break;
    if (pos+4 > entry.end()) {
      rsrcAscii().addPos(pos);
      rsrcAscii().addNote("FontNames###");

      MWAW_DEBUG_MSG(("NSParser::readFontsList: can not read flst\n"));
      return false;
    }
    int fId = (int)input->readULong(2);
    f.str("");
    f << "FontNames" << num++ << ":fId=" << std::hex << fId << std::dec << ",";
    int pSz = (int)input->readULong(1);

    if (pSz+1+pos+2 > entry.end()) {
      f << "###";
      rsrcAscii().addPos(pos);
      rsrcAscii().addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("NSParser::readFontsList: can not read pSize\n"));
      return false;
    }
    std::string name("");
    for (int c=0; c < pSz; c++)
      name += (char) input->readULong(1);
    m_convertissor->setCorrespondance(fId, name);
    f << name;
    rsrcAscii().addPos(pos);
    rsrcAscii().addNote(f.str().c_str());
    if ((pSz%2)==0) input->seek(1,WPX_SEEK_CUR);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the FTAB/STYL resource: a font format ?
////////////////////////////////////////////////////////////
bool NSParser::readFonts(MWAWEntry const &entry)
{
  bool isStyle = entry.type()=="STYL";
  int const fSize = isStyle ? 58 : 98;
  std::string name(isStyle ? "Style" : "Fonts");
  if ((!entry.valid()&&entry.length()) || (entry.length()%fSize)) {
    MWAW_DEBUG_MSG(("NSParser::readFonts: the entry is bad\n"));
    return false;
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int numElt = int(entry.length()/fSize);
  libmwaw::DebugStream f;
  f << "Entries(" << name << ")[" << entry.id() << "]:N=" << numElt;
  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());

  long val;
  for (int i = 0; i < numElt; i++) {
    NSParserInternal::Font font;
    pos = input->tell();
    f.str("");
    if (!isStyle)
      font.m_pictureId = (int)input->readLong(2);

    if (font.m_pictureId) {
      // the two value seems to differ slightly for a picture
      val = (long)input->readULong(2);
      if (val != 0xFF01) f << "#pictFlags0=" << std::hex << val << ",";
      font.m_pictureHeight = (int)input->readLong(2);
    } else {
      val = (long)input->readULong(2);
      if (val != 0xFF00)
        font.m_font.setId(int(val));
      val = (long)input->readULong(2);
      if (val != 0xFF00)
        font.m_font.setSize(int(val));
    }

    uint32_t flags=0;
    int flag = (int) input->readULong(2);

    if (flag&0x1) flags |= MWAW_BOLD_BIT;
    if (flag&0x2) flags |= MWAW_ITALICS_BIT;
    if (flag&0x4) flags |= MWAW_UNDERLINE_BIT;
    if (flag&0x8) flags |= MWAW_EMBOSS_BIT;
    if (flag&0x10) flags |= MWAW_SHADOW_BIT;
    if (flag&0x20) f << "condensed,";
    if (flag&0x40) f << "extended,";
    if (flag &0xFF80)
      f << "#flags0=" << std::hex << (flag &0xFF80) << std::dec << ",";
    flag = (int) input->readULong(2);
    if (flag & 1) {
      flags |= MWAW_UNDERLINE_BIT;
      f << "underline[lower],";
    }
    if (flag & 2) {
      flags |= MWAW_UNDERLINE_BIT;
      f << "underline[dot],";
    }
    if (flag & 4) {
      flags |= MWAW_UNDERLINE_BIT;
      f << "underline[word],";
    }
    if (flag & 0x8) flags |= MWAW_SUPERSCRIPT_BIT;
    if (flag & 0x10) flags |= MWAW_SUBSCRIPT_BIT;
    if (flag & 0x20) flags |= MWAW_STRIKEOUT_BIT;
    if (flag & 0x40) flags |= MWAW_OVERLINE_BIT;
    if (flag & 0x80) flags |= MWAW_SMALL_CAPS_BIT;
    if (flag & 0x100) flags |= MWAW_ALL_CAPS_BIT;
    if (flag & 0x200) // checkme: possible ?
      f << "boxed,";
    if (flag & 0x400) flags |= MWAW_HIDDEN_BIT;
    if (flag & 0x1000) {
      flags |= MWAW_SUPERSCRIPT_BIT;
      f << "superscript2,";
    }
    if (flag & 0x2000) {
      flags |= MWAW_SUBSCRIPT_BIT;
      f << "subscript2,";
    }
    if (flag & 0x4000) // fixme
      f << "invert,";
    if (flag & 0x8800)
      f << "#flags1=" << std::hex << (flag & 0x8800) << std::dec << ",";
    val = input->readLong(2);
    if (val) f << "#f0=" << std::hex << val << ",";
    font.m_format = (int) input->readULong(1);
    font.m_format2 = (int) input->readULong(1);
    font.m_font.setFlags(flags);

    int color = 0;
    // now data differs
    if (isStyle) {
      val = (int) input->readULong(2); // find [0-3] here
      if (val) f << "unkn0=" << val << ",";
      for (int j = 0; j < 6; j++) { // find s0=67, s1=a728
        val = (int) input->readULong(2);
        if (val) f << "f" << j << "=" << std::hex << val << std::dec << ",";
      }
      color = (int) input->readULong(2);
    } else {
      color = (int) input->readULong(2);
      for (int j = 1; j < 6; j++) { // find always 0 here...
        val = (int) input->readULong(2);
        if (val) f << "#f" << j << "=" << val << ",";
      }
      bool hasMark = false;
      val = (int) input->readULong(2);
      if (val == 1) hasMark = true;
      else if (val) f << "#hasMark=" << val << ",";
      val = (int) input->readULong(2);
      if (hasMark) font.m_markId = int(val);
      else if (val) f << "#markId=" << val << ",";
    }

    static const uint32_t colors[] =
    { 0, 0xFF0000, 0x00FF00, 0x0000FF, 0x00FFFF, 0xFF00FF, 0xFFFF00, 0xFFFFFF };
    if (color < 8) {
      uint32_t col = colors[color];
      font.m_font.setColor
      (Vec3uc((unsigned char)(col>>16),(unsigned char)((col>>8)&0xFF), (unsigned char)(col&0xFF)));
    } else if (color != 0xFF00)
      f << "#color=" << color << ",";
    font.m_extra = f.str();
    if (!isStyle)
      m_state->m_fontList.push_back(font);

    f.str("");
    f << name << i << ":" << font.m_font.getDebugString(m_convertissor)
      << font;
    rsrcAscii().addDelimiter(input->tell(),'|');
    rsrcAscii().addPos(pos);
    rsrcAscii().addNote(f.str().c_str());
    input->seek(pos+fSize, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the FRMT resource: a list of ?
////////////////////////////////////////////////////////////
bool NSParser::readFRMT(MWAWEntry const &entry, int zoneId)
{
  if (!entry.valid() || (entry.length()%10)) {
    MWAW_DEBUG_MSG(("NSParser::readFRMT: the entry is bad\n"));
    return false;
  }
  if (zoneId < 0 || zoneId >= 3) {
    MWAW_DEBUG_MSG(("NSParser::readFRMT: find unexpected zoneId: %d\n", zoneId));
    return false;
  }
  NSParserInternal::Zone &zone = m_state->m_zones[zoneId];
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int numElt = int(entry.length()/10);
  libmwaw::DebugStream f;
  f << "Entries(FRMT)[" << zoneId << "]:N=" << numElt;
  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());

  NSPosition position;
  NSParserInternal::DataPLC plc;
  plc.m_type = NSParserInternal::P_Format;
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();
    f.str("");
    f << "FRMT" << i << "[" << zoneId << "]:";
    position.m_paragraph = (int) input->readULong(4); // checkme or ??? m_paragraph
    position.m_word = (int) input->readULong(2);
    position.m_char = (int) input->readULong(2);
    f << "pos=" << position << ",";
    int id = (int) input->readLong(2);
    f << "F" << id << ",";
    plc.m_id = id;
    zone.m_plcMap.insert(NSParserInternal::Zone::PLCMap::value_type(position, plc));
    rsrcAscii().addPos(pos);
    rsrcAscii().addNote(f.str().c_str());
    input->seek(pos+10, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the RULE resource: a list of ruler ?
////////////////////////////////////////////////////////////
bool NSParser::readParagraphs(MWAWEntry const &entry, int zoneId)
{
  if (!entry.valid() && entry.length() != 0) {
    MWAW_DEBUG_MSG(("NSParser::readParagraphs: the entry is bad\n"));
    return false;
  }
  if (zoneId < 0 || zoneId >= 3) {
    MWAW_DEBUG_MSG(("NSParser::readParagraphs: find unexpected zoneId: %d\n", zoneId));
    return false;
  }
  NSParserInternal::Zone &zone = m_state->m_zones[zoneId];

  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int numElt = int(entry.length()/98);
  libmwaw::DebugStream f, f2;
  f << "Entries(RULE)[" << entry.type() << entry.id() << "]";
  if (entry.id()==1004) f << "[Styl]";
  else if (entry.id() != 1003) {
    MWAW_DEBUG_MSG(("NSParser::readParagraphs: find unexpected entryId: %d\n", entry.id()));
    f << "###";
  }
  f << ":N=" << numElt;
  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());

  NSParserInternal::DataPLC plc;
  plc.m_type = NSParserInternal::P_Ruler;

  long val;
  while (input->tell() != entry.end()) {
    int num = (entry.id() == 1003) ? (int)zone.m_paragraphList.size() : -1;
    pos = input->tell();
    f.str("");
    if (pos+8 > entry.end() || input->atEOS()) {
      f << "RULE" << num << "[" << zoneId << "]:###";
      rsrcAscii().addPos(pos);
      rsrcAscii().addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("NSParser::readParagraphs: can not read end of zone\n"));
      return false;
    }

    long nPara = (long) input->readULong(4);
    if (nPara == 0x7FFFFFFF) {
      input->seek(-4, WPX_SEEK_CUR);
      break;
    }
    NSPosition textPosition;
    textPosition.m_paragraph = (int) nPara;  // checkme or ???? + para

    long sz = (long) input->readULong(4);
    if (sz < 0x42 || pos+sz > entry.end()) {
      f << "RULE" << num << "[" << zoneId << "]:###";
      rsrcAscii().addPos(pos);
      rsrcAscii().addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("NSParser::readParagraphs: can not read the size zone\n"));
      return false;
    }
    NSParserInternal::Paragraph para;
    para.m_spacingsInterlineUnit = WPX_POINT; // set default
    para.m_spacingsInterlineType = libmwaw::AtLeast;
    para.m_spacings[0] = float(input->readLong(4))/65536.f;
    para.m_spacings[1] = float(input->readLong(4))/65536.f/72.f;
    int wh = int(input->readLong(2));
    switch(wh) {
    case 0:
      break; // left
    case 1:
      para.m_justify = libmwaw::JustificationCenter;
      break;
    case 2:
      para.m_justify = libmwaw::JustificationRight;
      break;
    case 3:
      para.m_justify = libmwaw::JustificationFull;
      break;
    default:
      f << "#align=" << wh << ",";
      break;
    }
    val = input->readLong(2);
    if (val) f << "#f0=" << val << ",";

    para.m_marginsUnit = WPX_INCH;
    para.m_margins[1] = float(input->readLong(4))/65536.f/72.f;
    para.m_margins[0] = float(input->readLong(4))/65536.f/72.f;
    para.m_margins[2] = pageWidth()-float(input->readLong(4))/65536.f/72.f;

    wh = int(input->readULong(1));
    switch(wh) {
    case 0:
      para.m_spacings[0]=0;
      break; // auto
    case 1:
      para.m_spacingsInterlineType = libmwaw::Fixed;
      break;
    case 2:
      para.m_spacingsInterlineUnit = WPX_PERCENT;
      para.m_spacingsInterlineType = libmwaw::Fixed;
      break;
    default:
      f << "#interline=" << (val&0xFC) << ",";
      para.m_spacings[0]=0;
      break;
    }
    val = input->readLong(1);
    if (val) f << "#f1=" << val << ",";
    for (int i = 0; i < 14; i++) {
      val = input->readLong(2);
      if (val) f << "g" << i << "=" << val << ",";
    }
    input->seek(pos+0x3E, WPX_SEEK_SET);
    long numTabs = input->readLong(2);
    bool ok = true;
    if (0x40+8*numTabs+2 > sz) {
      f << "###";
      MWAW_DEBUG_MSG(("NSParser::readParagraphs: can not read the string\n"));
      ok = false;
      numTabs = 0;
    }
    for (int i = 0; i < numTabs; i++) {
      long tabPos = input->tell();
      MWAWTabStop tab;

      f2.str("");
      tab.m_position = float(input->readLong(4))/72.f/65536.; // from left pos
      val = (long) input->readULong(1);
      switch(val) {
      case 1:
        break;
      case 2:
        tab.m_alignment = MWAWTabStop::CENTER;
        break;
      case 3:
        tab.m_alignment = MWAWTabStop::RIGHT;
        break;
      case 4:
        tab.m_alignment = MWAWTabStop::DECIMAL;
        break;
      case 6:
        f2 << "justify,";
        break;
      default:
        f2 << "#type=" << val << ",";
        break;
      }
      tab.m_leaderCharacter = (unsigned short)input->readULong(1);
      val = (long) input->readLong(2); // unused ?
      if (val) f2 << "#unkn0=" << val << ",";
      para.m_tabs->push_back(tab);
      if (f2.str().length())
        f << "tab" << i << "=[" << f2.str() << "],";
      input->seek(tabPos+8, WPX_SEEK_SET);
    }

    // ruler name
    long pSz = ok ? (long) input->readULong(1) : 0;
    if (pSz) {
      if (input->tell()+pSz != pos+sz && input->tell()+pSz+1 != pos+sz) {
        f << "name###";
        MWAW_DEBUG_MSG(("NSParser::readParagraphs: can not read the ruler name\n"));
        rsrcAscii().addDelimiter(input->tell()-1,'#');
      } else {
        std::string str("");
        for (int i = 0; i < pSz; i++)
          str += (char) input->readULong(1);
        para.m_name = str;
      }
    }
    plc.m_id = num;
    para.m_extra=f.str();
    if (entry.id() == 1003) {
      zone.m_paragraphList.push_back(para);
      zone.m_plcMap.insert(NSParserInternal::Zone::PLCMap::value_type(textPosition, plc));
    }

    f.str("");
    f << "RULE" << num << "[" << zoneId << "]:";
    f << "paragraph=" << nPara << "," << para;

    rsrcAscii().addPos(pos);
    rsrcAscii().addNote(f.str().c_str());
    input->seek(pos+sz, WPX_SEEK_SET);
  }
  pos = input->tell();
  f.str("");
  f << "RULE[" << zoneId << "](II):";
  if (pos+66 != entry.end() || input->readULong(4) != 0x7FFFFFFF) {
    f << "###";
    rsrcAscii().addPos(pos);
    rsrcAscii().addNote(f.str().c_str());

    MWAW_DEBUG_MSG(("NSParser::readParagraphs: find odd end\n"));
    return true;
  }
  for (int i = 0; i < 31; i++) { // only find 0 expected f12=0|100
    val = (long) input->readLong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  rsrcAscii().addPos(pos);
  rsrcAscii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// read DSPL zone: numbering definition
////////////////////////////////////////////////////////////
bool NSParser::readNumberingDef(NSParserInternal::RecursifData const &data)
{
  size_t numData = data.m_positionList.size();
  if (data.m_entryList.size() != numData) {
    MWAW_DEBUG_MSG(("NSParser::readNumberingDef: the position and the entry do not coincide\n"));
    return false;
  }
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugStream f;
  size_t n = 0;
  long val;
  while (n < numData) {
    Vec3i const &wh = data.m_positionList[n];

    static int const minExpectedSz[] = { 5, 8, 5, 6, 6, 6, 8, 8, 5, 8 };
    while (n < numData) {
      Vec3i actWh = data.m_positionList[n];
      Vec3i diffWh = actWh-wh;
      if (diffWh[0] || diffWh[1]) break;
      MWAWEntry const &entry = data.m_entryList[n++];
      f.str("");
      input->seek(entry.begin(), WPX_SEEK_SET);
      val = input->readLong(2);
      if (val) f << "#f0=" << val << ",";
      int id = (int) input->readLong(2);
      if (id != diffWh[2]+2) f << "#id=" << id << ",";

      if (actWh[2] < 0 || actWh[2] > 9 || id != diffWh[2]+2 ||
          entry.length() < minExpectedSz[actWh[2]]) {
        MWAW_DEBUG_MSG(("NSParser::readNumberingDef: find unexpected size for data %d\n", actWh[2]));
        f << "###";
        rsrcAscii().addPos(entry.begin()-8);
        rsrcAscii().addNote(f.str().c_str());
        continue;
      }

      switch(actWh[2]) {
      case 0: // main text
      case 2: // display example
      case 8: { // postfix : list of fields
        int mSz = (int) input->readULong(1);
        if (mSz+1+4 > entry.length()) {
          MWAW_DEBUG_MSG(("NSParser::readNumberingDef: the dpsl seems to short\n"));
          f << "###Text";
          break;
        }
        std::string text("");
        for (int i = 0; i < mSz; i++)
          text+= (char) input->readULong(1);
        f << "g" << actWh[2] << "=\"" << text << "\"";
        break;
      }
      case 3: // style : 0=arabic, 1=roman, upperroman, abgadhawaz(arabic), alpha, upperalpha, hebrew
      case 4: // start number
      case 5: // increment
        val = (long) input->readULong(2);
        f << "g" << actWh[2] << "=" << std::hex << val << std::dec << ",";
        break;
      case 1: // find f or 1d
      case 6: // always 0 ?
      case 7: // main id
      case 9: // find 0-2
        val = (long) input->readULong(4);
        f << "g" << actWh[2] << "=" << std::hex << val << std::dec << ",";
        break;
      default:
        break;
      }

      if (f.str().length()) {
        rsrcAscii().addPos(entry.begin()-8);
        rsrcAscii().addNote(f.str().c_str());
      }
    }
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the DPND zone ( the numbering reset zone )
////////////////////////////////////////////////////////////
bool NSParser::readNumberingReset(MWAWEntry const &entry, int zoneId)
{
  // find to 2 times with entry.length()=0x22
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("NSParser::readNumberingReset: the entry is bad\n"));
    return false;
  }
  if (zoneId < 0 || zoneId >= 3) {
    MWAW_DEBUG_MSG(("NSParser::readNumberingReset: find unexpected zoneId: %d\n", zoneId));
    return false;
  }
  //  NSParserInternal::Zone &zone = m_state->m_zones[zoneId];
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(NumberingReset)[" << zoneId << "]:";
  /* sz, list of reset ? */
  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read mark zone ( ie the reference structure )
////////////////////////////////////////////////////////////
bool NSParser::readMark(NSParserInternal::RecursifData const &data)
{
  size_t numData = data.m_positionList.size();
  if (data.m_entryList.size() != numData) {
    MWAW_DEBUG_MSG(("NSParser::readMark: the position and the entry do not coincide\n"));
    return false;
  }
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugStream f;

  size_t n = 0;
  bool pbFound = false;
  while (n < numData) {
    Vec3i const &wh = data.m_positionList[n];
    MWAWEntry const &entry = data.m_entryList[n++];
    if ((wh[1]%2) || wh[2] || n+1 >= numData ||
        data.m_positionList[n+1][1] <= wh[1] || entry.length() < 12) {
      if (!pbFound) {
        MWAW_DEBUG_MSG(("NSParser::readMark: the data order seems bads\n"));
        pbFound = true;
      }
      rsrcAscii().addPos(entry.begin()-8);
      rsrcAscii().addNote("###");
      continue;
    }

    // the mark position in the text part
    long pos = entry.begin();
    input->seek(pos, WPX_SEEK_SET);
    f.str("");
    f << "[Position]:";
    long val = (long) input->readULong(2);
    if (val != 0x7fff) f << "#f0=" << val << ",";
    val = input->readLong(2);
    if (val != -1) f << "#f1=" << val << ",";
    f << "filePos=" << std::hex << input->readULong(4) << "<->";
    f << input->readULong(4) << std::dec << ",";
    rsrcAscii().addPos(pos-8);
    rsrcAscii().addNote(f.str().c_str());

    // the mark data
    size_t numMarkData = 0;
    while (n+numMarkData < numData) {
      Vec3i diffWh = data.m_positionList[n+numMarkData]-wh;
      if (diffWh[0] || diffWh[1]!=1)
        break;
      numMarkData++;
    }
    if (numMarkData != 2 || data.m_entryList[n].length() < 8 ||
        data.m_entryList[n+1].length() < 9) {
      if (!pbFound) {
        MWAW_DEBUG_MSG(("NSParser::readMark: the mark data seems odds\n"));
        pbFound = true;
      }
      for (size_t d=0; d < numMarkData; d++) {
        rsrcAscii().addPos(data.m_entryList[n+d].begin()-8);
        rsrcAscii().addNote("Data###");
      }
      n+=numMarkData;
      continue;
    }

    pos = data.m_entryList[n++].begin();
    input->seek(pos, WPX_SEEK_SET);
    f.str("");
    f << "[II]:";
    val = input->readLong(4); // always 1 ?
    if (val != 1) f << "#f0=" << val << ",";
    val = input->readLong(4); // some kind of id ?
    f << "id?=" << val << ",";
    rsrcAscii().addPos(pos-8);
    rsrcAscii().addNote(f.str().c_str());

    MWAWEntry const &textEntry = data.m_entryList[n++];
    pos = textEntry.begin();
    input->seek(pos, WPX_SEEK_SET);
    f.str("");
    f << "[Text]:";
    val = input->readLong(2); // always 0 ?
    if (val) f << "#f0=" << val << ",";
    val = input->readLong(2); // always 0x64 ?
    if (val != 100) f << "#f1=" << val << ",";
    int mSz = (int) input->readULong(1);
    if (mSz+1+4 > textEntry.length()) {
      MWAW_DEBUG_MSG(("NSParser::readMark: the mark text seems to short\n"));
      rsrcAscii().addPos(pos-8);
      rsrcAscii().addNote("###Text");
      continue;
    }
    std::string mark("");
    for (int i = 0; i < mSz; i++)
      mark+=(char) input->readULong(1);
    f << mark;
    rsrcAscii().addPos(pos-8);
    rsrcAscii().addNote(f.str().c_str());
    // fixme: save here the mark wh[1]/2
  }
  return true;
}

////////////////////////////////////////////////////////////
// read a recursive zone of ? Only in v4 ?
////////////////////////////////////////////////////////////
bool NSParser::readRecursifList
(MWAWEntry const &entry, NSParserInternal::RecursifData &data, int level)
{
  if (!entry.valid() || entry.length() < 8) {
    MWAW_DEBUG_MSG(("NSParser::readRecursifList: the entry is bad\n"));
    return false;
  }
  if (level < 0 || level >= 3) {
    MWAW_DEBUG_MSG(("NSParser::readRecursifList: find unexpected level: %d\n", level));
    return false;
  }

  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  if (level == 0) {
    f << "Entries(" << entry.name() << "):";
    rsrcAscii().addPos(pos);
    rsrcAscii().addNote(f.str().c_str());
  }
  int num = 0;
  while (input->tell() != entry.end()) {
    pos = input->tell();
    f.str("");
    if (level==0) data.m_actualPos[level]=num++;
    f << entry.name();
    for (int i = 0; i <= level; i++) {
      f << data.m_actualPos[i];
      if (i!=level) f << "-";
    }
    if (data.m_zoneId) f << "[" << data.m_zoneId << "]";
    f << ":";
    int depth = (int) input->readLong(2);
    if (depth != level+1) {
      f << "###";
      rsrcAscii().addPos(pos);
      rsrcAscii().addNote(f.str().c_str());
      return false;
    }
    int val = (int) input->readLong(2);
    f << "unkn=" << val << ","; // always 10 ?
    long sz = input->readLong(4);
    long minSize = 16;
    if (level == 2) {
      sz += 13;
      if (sz%2) sz++;
      minSize = 14;
    }
    long endPos = pos+sz;
    if (sz < minSize || endPos > entry.end()) {
      MWAW_DEBUG_MSG(("NSParser::readRecursifList: can not read entry %d\n", num));
      f << "###";
      rsrcAscii().addPos(pos);
      rsrcAscii().addNote(f.str().c_str());
      return false;
    }

    rsrcAscii().addPos(pos);
    rsrcAscii().addNote(f.str().c_str());
    if (level == 2) {
      MWAWEntry child(entry);
      child.setBegin(input->tell());
      child.setEnd(endPos);
      data.m_positionList.push_back(data.m_actualPos);
      data.m_entryList.push_back(child);
      input->seek(endPos, WPX_SEEK_SET);
      break;
    }
    input->seek(8, WPX_SEEK_CUR);

    int childNum = 0;
    while (input->tell() != endPos) {
      data.m_actualPos[level+1] = childNum++;
      pos = input->tell();
      MWAWEntry child(entry);
      child.setBegin(pos);
      child.setEnd(endPos);
      if (input->atEOS() || pos >= endPos) {
        MWAW_DEBUG_MSG(("NSParser::readRecursifList: bad position for child entry\n"));
        f.str("");
        f << entry.name() << "###";
        rsrcAscii().addPos(pos);
        rsrcAscii().addNote(f.str().c_str());
        return false;
      }

      if (!readRecursifList(child, data, level+1)) {
        MWAW_DEBUG_MSG(("NSParser::readRecursifList: can not read child entry\n"));
        input->seek(pos, WPX_SEEK_SET);
        if ((int) input->readLong(2)==level+1) {
          // seems to happens sometimes in XMRK zone :-~
          input->seek(pos, WPX_SEEK_SET);
          break;
        }

        f.str("");
        f << entry.name() << "###";
        rsrcAscii().addPos(pos);
        rsrcAscii().addNote(f.str().c_str());
        input->seek(endPos, WPX_SEEK_SET);
        break;
      }
    }
    if (level)
      break;
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool NSParser::readPrintInfo(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() < 0x78) {
    MWAW_DEBUG_MSG(("NSParser::readPrintInfo: the entry is bad\n"));
    return false;
  }
  if (entry.id() != 128) {
    MWAW_DEBUG_MSG(("NSParser::readPrintInfo: the entry id %d is odd\n", entry.id()));
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  // print info
  libmwaw::PrinterInfo info;
  if (!info.read(input)) return false;
  if (entry.id() != 128)
    f << "Entries(PrintInfo)[#" << entry.id() << "]:";
  else
    f << "Entries(PrintInfo):" << info;

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

  m_pageSpan.setMarginTop(lTopMargin.y()/72.0);
  m_pageSpan.setMarginBottom(botMarg/72.0);
  m_pageSpan.setMarginLeft(lTopMargin.x()/72.0);
  m_pageSpan.setMarginRight(rightMarg/72.0);
  m_pageSpan.setFormLength(paperSize.y()/72.);
  m_pageSpan.setFormWidth(paperSize.x()/72.);

  if (entry.length() != 0x78)
    f << "###size=" << entry.length() << ",";
  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());
  input->seek(pos+0x78, WPX_SEEK_SET);
  if (long(input->tell()) != pos+0x78) {
    MWAW_DEBUG_MSG(("MWParser::readPrintInfo: file is too short\n"));
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the PGLY zone ( unknown )
////////////////////////////////////////////////////////////
bool NSParser::readPGLY(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() < 0xa2) {
    MWAW_DEBUG_MSG(("NSParser::readPGLY: the entry is bad\n"));
    return false;
  }
  if (entry.id() != 128) {
    MWAW_DEBUG_MSG(("NSParser::readPGLY: the entry id %d is odd\n", entry.id()));
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  if (entry.id() != 128)
    f << "Entries(PGLY)[#" << entry.id() << "]:";
  else
    f << "Entries(PGLY):";

  long val = input->readLong(2); // always 0x88 ?
  if (val != 0x88) f << "f0=" << val << ",";
  val = input->readLong(2); // always 0x901 or two flags ?
  if (val != 0x901) f << "f1=" << val << ",";
  val = input->readLong(2); // always 0x0
  if (val) f << "f2=" << val << ",";

  /** checkme
      bdbox0=page,
      bdbox1=page less margin
      bdbox2=column ?
  */
  int dim[4];
  for (int i = 0; i < 3; i++) {
    for (int d = 0; d < 4; d++)
      dim[d] = (int) input->readLong(2);
    f << "bdbox" << i << "=[" << dim[1] << "x" << dim[0]
      << "<->" << dim[3] << "x" << dim[2] << "],";
  }
  static int const expectedValues[]= {0,0,0,0, 0x24, 0, 3, 1};
  // find also, g0=2, g3=1 [ncol?], g5=0x10, g6=g7=0 (in v3 file)
  for (int i = 0; i < 8; i++) {
    val = input->readLong(2);
    if (int(val) != expectedValues[i])
      f << "g" << i << "=" << val << ",";
  }
  // find only 0 expected h2=[0|1], h6=[0|-1]
  for (int i = 0; i < 7; i++) {
    val = input->readLong(2);
    if (val)
      f << "h" << i << "=" << val << ",";
  }
  // a series of flags 0 or 1, find frequently fl0=1, and sometimes fl4=fl5=1
  for (int i = 0; i < 8; i++) {
    val = input->readLong(1);
    if (val)
      f << "fl" << i << "=" << val << ",";
  }
  // always 0
  for (int i = 0; i < 3; i++) {
    val = input->readLong(2);
    if (val)
      f << "l" << i << "=" << val << ",";
  }
  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());

  pos = input->tell();
  f.str("");
  f << "PGLY[A]:";
  // find 0,3ea0f83e|3ea19dbd|3ea2433b|3ec00000|3ec0a57f
  val = (long) input->readULong(4);
  if (val) f << "f0=" << std::hex << val << std::dec << ",";
  // find f3=[0|1|-1]
  for (int i = 1; i < 4; i++) {
    val = input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  // 2 times 0x48 ?
  for (int i = 4; i < 6; i++) {
    val = input->readLong(2);
    if (val != 0x48)
      f << "f" << i << "=" << val << ",";
  }
  for (int d = 0; d < 4; d++)
    dim[d] = (int) input->readLong(2);
  f << "bdbox=[" << dim[1] << "x" << dim[0]
    << "<->" << dim[3] << "x" << dim[2] << "],";
  // 6 flags, 0|1|-1
  for (int i = 0; i < 6; i++) {
    val = input->readLong(1);
    if (val)
      f << "fl" << i << "=" << val << ",";
  }
  // a size ? often 114,16d
  int sz[2];
  for (int i = 0; i < 2; i++)
    sz[i]=(int)input->readLong(2);
  if (sz[0]||sz[1])
    f << "sz=" << sz[1] << "x" << sz[0] << ",";
  // v3: 0,0 or v4: 0x29 0x3 ?
  for (int i = 0; i < 2; i++) {
    val = input->readLong(2);
    if (val)
      f << "g" << i << "=" << val << ",";
  }
  // another sz ? ~153,~1fc
  for (int i = 0; i < 2; i++)
    sz[i]=(int)input->readLong(2);
  if (sz[0]||sz[1])
    f << "sz2=" << sz[1] << "x" << sz[0] << ",";
  // 0|-1, find also g4=0xa
  for (int i = 2; i < 8; i++) {
    val = input->readLong(2);
    if (val)
      f << "g" << i << "=" << val << ",";
  }
  // 0 expect h0=[0|1|22], h1=[0|1|22], h0=h1 ( almost always ? )
  for (int i = 0; i < 7; i++) {
    val = input->readLong(2);
    if (val)
      f << "h" << i << "=" << val << ",";
  }
  // a dim
  for (int i = 0; i < 2; i++)
    sz[i]=(int)input->readLong(2);
  if (sz[0]||sz[1])
    f << "dim=" << sz[1] << "x" << sz[0] << ",";
  // all 0 expected k1=[0|1], k2=[0|1] k4=1
  for (int i = 0; i < 9; i++) {
    val = input->readLong(2);
    if (val)
      f << "k" << i << "=" << val << ",";
  }
  if (entry.length()!=0xa2)
    f << "###size=" << entry.length() << ",";
  rsrcAscii().addPos(pos);
  rsrcAscii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// read the CPRC zone ( unknown )
////////////////////////////////////////////////////////////
bool NSParser::readCPRC(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() < 0xe) {
    MWAW_DEBUG_MSG(("NSParser::readCPRC: the entry is bad\n"));
    return false;
  }
  if (entry.id() != 128) {
    MWAW_DEBUG_MSG(("NSParser::readCPRC: the entry id %d is odd\n", entry.id()));
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  if (entry.id() != 128)
    f << "Entries(CPRC)[#" << entry.id() << "]:";
  else
    f << "Entries(CPRC):";

  // find only 0...
  for (int i = 0; i < int(entry.length())/2; i++) {
    int val = (int) input->readULong(2);
    if (val) f << "#f" << i << "=" << std::hex << val << std::dec << ",";
  }

  if (entry.length()!=0xe)
    f << "###size=" << entry.length() << ",";
  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the CNTR zone ( a list of  ? )
////////////////////////////////////////////////////////////
bool NSParser::readCNTR(MWAWEntry const &entry, int zoneId)
{
  if (!entry.valid() || entry.length()<20 || (entry.length()%12) != 8 ) {
    MWAW_DEBUG_MSG(("NSParser::readCNTR: the entry is bad\n"));
    return false;
  }
  if (zoneId < 0 || zoneId >= 3) {
    MWAW_DEBUG_MSG(("NSParser::readCNTR: find unexpected zoneId: %d\n", zoneId));
    return false;
  }
  //  NSParserInternal::Zone &zone = m_state->m_zones[zoneId];
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int numElt = int(entry.length()/12)-1;
  libmwaw::DebugStream f;
  f << "Entries(CNTR)[" << zoneId << "]:N=" << numElt;
  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();
    f.str("");
    f << "CNTR" << i << ":";
    rsrcAscii().addPos(pos);
    rsrcAscii().addNote(f.str().c_str());
    input->seek(pos+12, WPX_SEEK_SET);
  }

  f.str("");
  f << "CNTR(II)";
  rsrcAscii().addPos(input->tell());
  rsrcAscii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// read the PICD zone ( a list of picture ? )
////////////////////////////////////////////////////////////
bool NSParser::readPICD(MWAWEntry const &entry, int zoneId)
{
  if ((!entry.valid()&&entry.length()) || (entry.length()%14)) {
    MWAW_DEBUG_MSG(("NSParser::readPICD: the entry is bad\n"));
    return false;
  }
  if (zoneId < 0 || zoneId >= 3) {
    MWAW_DEBUG_MSG(("NSParser::readPICD: find unexpected zoneId: %d\n", zoneId));
    return false;
  }
  //  NSParserInternal::Zone &zone = m_state->m_zones[zoneId];
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int numElt = int(entry.length()/14);
  libmwaw::DebugStream f;
  f << "Entries(PICD)[" << zoneId << "]:N=" << numElt;
  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());

  for (int i = 0; i < numElt; i++) {
    pos = input->tell();
    f.str("");
    f << "PICD" << i << ":";
    rsrcAscii().addPos(pos);
    rsrcAscii().addNote(f.str().c_str());
    input->seek(pos+14, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the PLAC zone ( a list of picture placement ? )
////////////////////////////////////////////////////////////
bool NSParser::readPLAC(MWAWEntry const &entry)
{
  if ((!entry.valid()&&entry.length()) || (entry.length()%202)) {
    MWAW_DEBUG_MSG(("NSParser::readPLAC: the entry is bad\n"));
    return false;
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int numElt = int(entry.length()/202);
  libmwaw::DebugStream f;
  f << "Entries(PLAC)[" << entry.id() << "]:N=" << numElt;
  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());

  long val;
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();
    f.str("");
    f << "PLAC" << i << ":";
    val = (int) input->readULong(2);
    f << "pictId=" << val;
    rsrcAscii().addDelimiter(input->tell(),'|');
    rsrcAscii().addPos(pos);
    rsrcAscii().addNote(f.str().c_str());
    input->seek(pos+202, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
//! read the PGRA resource: a unknown number (id 20000)
////////////////////////////////////////////////////////////
bool NSParser::readPGRA(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() < 2) {
    MWAW_DEBUG_MSG(("NSParser::readPGRA: the entry is bad\n"));
    return false;
  }
  if (entry.id() != 20000) {
    MWAW_DEBUG_MSG(("NSParser::readPGRA: the entry id %d is odd\n", entry.id()));
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  if (entry.id() != 20000)
    f << "Entries(PGRA)[#" << entry.id() << "]:";
  else
    f << "Entries(PGRA):";
  f << "N=" << input->readLong(2) << ","; // a number between 0 and 2
  if (entry.length()!=2)
    f << "###size=" << entry.length() << ",";

  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
std::ostream &operator<< (std::ostream &o, NSPosition const &pos)
{
  o << pos.m_paragraph << "x";
  if (pos.m_word) o << pos.m_word << "x";
  else o << "_x";
  if (pos.m_char) o << pos.m_char;
  else o << "_";

  return o;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
