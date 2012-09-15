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

#include <libwpd/WPXString.h>

#include "MWAWContentListener.hxx"
#include "MWAWDebug.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPageSpan.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "NSParser.hxx"
#include "NSStruct.hxx"

#include "NSText.hxx"

/** Internal: the structures of a NSText */
namespace NSTextInternal
{
/** Internal: the fonts and many other data*/
struct Font {
  //! the constructor
  Font(): m_font(-1,-1), m_pictureId(0), m_pictureWidth(0), m_markId(-1), m_variableId(0),
    m_format(0), m_format2(0), m_extra("") { }
  bool isVariable() const {
    return (m_format2&0x20);
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Font const &font);

  //! the font
  MWAWFont m_font;
  //! the picture id ( if this is for a picture )
  int m_pictureId;
  //! the picture width
  int m_pictureWidth;
  //! a mark id
  int m_markId;
  //! the variable id : in fact cst[unkn] + v_id
  int m_variableId;
  //! the main format ...
  int m_format;
  //! a series of flags
  int m_format2;
  //! two picture dim ( orig && file ?)
  Box2i m_pictureDim[2];
  //! extra data
  std::string m_extra;
};


std::ostream &operator<<(std::ostream &o, Font const &font)
{
  if (font.m_pictureId) o << "pictId=" << font.m_pictureId << ",";
  if (font.m_pictureWidth) o << "pictW=" << font.m_pictureWidth << ",";
  if (font.m_markId >= 0) o << "markId=" << font.m_markId << ",";
  if (font.m_variableId > 0) o << "variableId=" << font.m_variableId << ",";
  if (font.m_format2&0x4) o << "index,";
  if (font.m_format2&0x8) o << "TOC,";
  if (font.m_format2&0x10) o << "samePage,";
  if (font.m_format2&0x20) o << "variable,";
  if (font.m_format2&0x40) o << "hyphenate,";
  if (font.m_format2&0x83)
    o << "#format2=" << std::hex << (font.m_format2 &0x83) << std::dec << ",";

  if (font.m_format & 1) o << "noSpell,";
  if (font.m_format & 8) o << "REVERTED,"; // writing is Right->Left
  if (font.m_format & 0x10) o << "sameLine,";
  if (font.m_format & 0x40) o << "endOfPage,"; // checkme
  if (font.m_format & 0xA6)
    o << "#fl=" << std::hex << (font.m_format & 0xA6) << std::dec << ",";
  if (font.m_pictureDim[0].size()[0] || font.m_pictureDim[0].size()[1])
    o << "pictDim=" << font.m_pictureDim[0] << ",";
  if (font.m_pictureDim[0] != font.m_pictureDim[1] &&
      (font.m_pictureDim[1].size()[0] || font.m_pictureDim[1].size()[1]))
    o << "pictDim[crop]=" << font.m_pictureDim[1] << ",";
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

/** Internal structure: use to store a header */
struct HeaderFooter {
  //! Constructor
  HeaderFooter() : m_type(MWAWPageSpan::HEADER), m_occurence(MWAWPageSpan::NEVER),
    m_page(0), m_textParagraph(-1), m_unknown(0), m_parsed(false), m_extra("") {
    for (int i = 0; i < 2; i++) m_paragraph[i] = -1;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, HeaderFooter const &hf);
  //! the header type
  MWAWPageSpan::HeaderFooterType m_type;
  //! the header occurence
  MWAWPageSpan::HeaderFooterOccurence m_occurence;
  //! the page
  int m_page;
  //! the paragraph position in the header zone (first and last)
  long m_paragraph[2];
  //! the text position
  long m_textParagraph;
  //! a unknown value
  int m_unknown;
  //! a flag to know if the footnote is parsed
  mutable bool m_parsed;
  //! some extra debuging information
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, HeaderFooter const &hf)
{
  if (hf.m_type==MWAWPageSpan::HEADER) o << "header,";
  else o << "footer,";
  switch(hf.m_occurence) {
  case MWAWPageSpan::NEVER:
    o << "never,";
    break;
  case MWAWPageSpan::ODD:
    o << "odd,";
    break;
  case MWAWPageSpan::EVEN:
    o << "even,";
    break;
  case MWAWPageSpan::ALL:
    o << "all,";
    break;
  default:
    o << "#occurence=" << int(hf.m_occurence) << ",";
    break;
  }
  o << "pos=" << hf.m_paragraph[0] << "<->" << hf.m_paragraph[1] << ",";
  o << "pos[def]=" << hf.m_textParagraph << ",";
  if (hf.m_unknown) o << "unkn=" << std::hex << hf.m_unknown << std::dec << ",";
  o << hf.m_extra;
  return o;
}

/** Internal structure: use to store a footnote */
struct Footnote {
  //! Constructor
  Footnote() : m_number(0), m_textPosition(), m_textLabel(""), m_noteLabel(""),
    m_parsed(false), m_extra("") {
    for (int i = 0; i < 2; i++) m_paragraph[i] = -1;
  }

  //! returns a label corresponding to a note ( or nothing if we can use numbering note)
  std::string getTextLabel(int actId) const {
    if (m_textLabel.length() == 0 || m_textLabel=="1")
      return std::string("");
    std::stringstream s;
    for (size_t c = 0; c < m_textLabel.length(); c++) {
      if (m_textLabel[c]=='1')
        s << actId;
      else
        s << m_textLabel[c];
    }
    return s.str();
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Footnote const &ft);
  //! the note number
  int m_number;
  //! the paragraph position in the footnote zone (first and last)
  int m_paragraph[2];
  //! the text position
  NSStruct::Position m_textPosition;
  //! the label in the text
  std::string m_textLabel;
  //! the label in the note
  std::string m_noteLabel;
  //! a flag to know if the footnote is parsed
  mutable bool m_parsed;
  //! some extra debuging information
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, Footnote const &ft)
{
  o << "pos=" << ft.m_textPosition << ",";
  if (ft.m_paragraph[1] > ft.m_paragraph[0])
    o << "paragraph[inNote]=" << ft.m_paragraph[0] << "<->" << ft.m_paragraph[1] << ",";
  if (ft.m_number) o << "number=" << ft.m_number << ",";
  if (ft.m_textLabel.length())
    o << "textLabel=\"" << ft.m_textLabel << "\",";
  if (ft.m_noteLabel.length())
    o << "noteLabel=\"" << ft.m_noteLabel << "\",";
  if (ft.m_extra.length())
    o << ft.m_extra;
  return o;
}

//! Internal: the picture data ( PICD )
struct PicturePara {
  //! constructor
  PicturePara() : m_id(-1), m_paragraph(-1), m_position() {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, PicturePara const &pict);
  //! the picture id
  int m_id;
  //! the paragraph position
  int m_paragraph;
  //! the position
  Box2i m_position;
};

std::ostream &operator<<(std::ostream &o, PicturePara const &pict)
{
  if (pict.m_id > 0) o << "pictId=" << pict.m_id << ",";
  if (pict.m_paragraph >= 0) o << "paragraph=" << pict.m_paragraph << ",";
  if (pict.m_position.size()[0] || pict.m_position.size()[1])
    o << "pos=" << pict.m_position << ",";
  return o;
}

/** different types
 *
 * - Format: font properties
 * - Ruler: new ruler
 */
enum PLCType { P_Format=0, P_Ruler, P_Footnote, P_HeaderFooter, P_PicturePara, P_Unknown};

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
  case P_PicturePara:
    o << "Pict";
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

//! internal structure used to store zone data
struct Zone {
  typedef std::multimap<NSStruct::Position,DataPLC,NSStruct::Position::Compare> PLCMap;

  //! constructor
  Zone() : m_entry(), m_paragraphList(), m_pictureParaList(), m_plcMap() {
  }
  //! the position of text in the rsrc file
  MWAWEntry m_entry;
  //! the list of paragraph
  std::vector<Paragraph> m_paragraphList;
  //! the list of paragraph
  std::vector<PicturePara> m_pictureParaList;

  //! the map pos -> format id
  PLCMap m_plcMap;
};

////////////////////////////////////////
//! Internal: the state of a NSText
struct State {
  //! constructor
  State() : m_version(-1), m_fontList(), m_footnoteList(),
    m_numPages(-1), m_actualPage(0), m_hfList(), m_headersId(), m_footersId() {
  }

  //! the file version
  mutable int m_version;

  /** the font list */
  std::vector<Font> m_fontList;
  /** the list of footnote */
  std::vector<Footnote> m_footnoteList;
  /** the main zones : Main, Footnote, HeaderFooter */
  Zone m_zones[3];

  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
  /** the list of header footer */
  std::vector<HeaderFooter> m_hfList;
  /** the list of header id which corresponds to each page */
  std::vector<int> m_headersId;
  /** the list of footer id which corresponds to each page */
  std::vector<int> m_footersId;
};

////////////////////////////////////////
//! Internal: the subdocument of a NSText
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(NSText &pars, MWAWInputStreamPtr input, int id, libmwaw::SubDocumentType type) :
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
  NSText *m_textParser;
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
  NSContentListener *listen = dynamic_cast<NSContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
    return;
  }

  assert(m_textParser);

  long pos = m_input->tell();
  if (m_type == libmwaw::DOC_NOTE)
    m_textParser->sendFootnote(m_id);
  else if (m_type == libmwaw::DOC_HEADER_FOOTER)
    m_textParser->sendHeaderFooter(m_id);
  else {
    MWAW_DEBUG_MSG(("SubDocument::parse: oops do not know how to send this kind of document\n"));
    return;
  }
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
NSText::NSText(MWAWInputStreamPtr ip, NSParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert),
  m_state(new NSTextInternal::State), m_mainParser(&parser)
{
}

NSText::~NSText()
{ }

int NSText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_mainParser->version();
  return m_state->m_version;
}

int NSText::numPages() const
{
  if (m_state->m_numPages >= 0)
    return m_state->m_numPages;
  const_cast<NSText *>(this)->computePositions();
  return m_state->m_numPages;
}

shared_ptr<MWAWSubDocument> NSText::getHeader(int page)
{
  shared_ptr<MWAWSubDocument> res;
  if (page < 1 || page-1 >= int(m_state->m_headersId.size())
      || m_state->m_headersId[size_t(page-1)] < 0)
    return res;
  res.reset(new NSTextInternal::SubDocument(*this, m_mainParser->rsrcInput(), m_state->m_headersId[size_t(page-1)], libmwaw::DOC_HEADER_FOOTER));

  return res;
}

shared_ptr<MWAWSubDocument> NSText::getFooter(int page)
{
  shared_ptr<MWAWSubDocument> res;
  if (page < 1 || page-1 >= int(m_state->m_footersId.size())
      || m_state->m_footersId[size_t(page-1)] < 0)
    return res;
  res.reset(new NSTextInternal::SubDocument(*this, m_mainParser->rsrcInput(), m_state->m_footersId[size_t(page-1)], libmwaw::DOC_HEADER_FOOTER));

  return res;
}

void NSText::computePositions()
{
  // first compute the number of page and the number of paragraph by pages
  int nPages = 1;
  MWAWInputStreamPtr input = m_mainParser->getInput();
  input->seek(0, WPX_SEEK_SET);
  int paragraph=0;
  std::vector<int> firstParagraphInPage;
  firstParagraphInPage.push_back(0);
  while (!input->atEOS()) {
    char c = (char) input->readULong(1);
    if (c==0xd)
      paragraph++;
    else if (c==0xc) {
      nPages++;
      firstParagraphInPage.push_back(paragraph);
    }
  }
  m_state->m_actualPage = 1;
  m_state->m_numPages = nPages;

  // update the main page zone
  m_state->m_zones[NSStruct::Z_Main].m_entry.setBegin(0);
  m_state->m_zones[NSStruct::Z_Main].m_entry.setEnd(input->tell());
  m_state->m_zones[NSStruct::Z_Main].m_entry.setId(NSStruct::Z_Main);
  // compute the header/footer pages
  int actPage = 1;
  Vec2i headerId(-1,-1), footerId(-1,-1);
  m_state->m_headersId.resize(nPages, -1);
  m_state->m_footersId.resize(nPages, -1);
  for (size_t i = 0; i < m_state->m_hfList.size(); i++) {
    NSTextInternal::HeaderFooter &hf = m_state->m_hfList[i];
    int page = 1;
    long textPos = hf.m_textParagraph;
    if (hf.m_type == MWAWPageSpan::FOOTER && textPos) textPos--;
    for (size_t j = 0; j < firstParagraphInPage.size(); j++) {
      if (textPos < firstParagraphInPage[j])
        break;
      page = int(j)+1;
    }
    //std::cerr << "find : page=" << page << " for hf" << int(i) << "\n";
    for (int p = actPage;  p < page; p++) {
      m_state->m_headersId[p-1] = (p%2) ? headerId[0] : headerId[1];
      m_state->m_footersId[p-1] = (p%2) ? footerId[0] : footerId[1];
    }
    actPage = hf.m_page = page;
    Vec2i &wh = hf.m_type == MWAWPageSpan::HEADER ? headerId : footerId;
    switch(hf.m_occurence) {
    case MWAWPageSpan::ODD:
      wh[0] = int(i);
      break;
    case MWAWPageSpan::EVEN:
      wh[1] = int(i);
      break;
    case MWAWPageSpan::ALL:
      wh[0] = wh[1] = int(i);
      break;
    case MWAWPageSpan::NEVER:
      wh[0] = wh[1] = -1;
      break;
    default:
      break;
    }
  }
  for (int p = actPage;  p <= nPages; p++) {
    m_state->m_headersId[p-1] = (p%2) ? headerId[0] : headerId[1];
    m_state->m_footersId[p-1] = (p%2) ? footerId[0] : footerId[1];
  }
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

// find the different zones
bool NSText::createZones()
{
  if (!m_mainParser->getRSRCParser()) {
    MWAW_DEBUG_MSG(("NSText::createZones: can not find the entry map\n"));
    return false;
  }
  std::multimap<std::string, MWAWEntry> &entryMap
    = m_mainParser->getRSRCParser()->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;

  // footnote and headerFooter main zone
  it = entryMap.lower_bound("HF  ");
  while (it != entryMap.end()) {
    if (it->first != "HF  ")
      break;
    MWAWEntry &entry = it++->second;
    readHeaderFooter(entry);
  }
  it = entryMap.lower_bound("FOOT");
  while (it != entryMap.end()) {
    if (it->first != "FOOT")
      break;
    MWAWEntry &entry = it++->second;
    readFootnotes(entry);
  }

  // fonts
  it = entryMap.lower_bound("FLST");
  while (it != entryMap.end()) {
    if (it->first != "FLST")
      break;
    MWAWEntry &entry = it++->second;
    readFontsList(entry);
  }
  it = entryMap.lower_bound("STYL");
  while (it != entryMap.end()) {
    if (it->first != "STYL")
      break;
    MWAWEntry &entry = it++->second;
    readFonts(entry);
  }
  it = entryMap.lower_bound("FTAB");
  while (it != entryMap.end()) {
    if (it->first != "FTAB")
      break;
    MWAWEntry &entry = it++->second;
    readFonts(entry);
  }

  // the font change
  char const *(posFontNames[]) = { "FRMT", "FFRM", "HFRM" };
  for (int z = 0; z < 3; z++) {
    it = entryMap.lower_bound(posFontNames[z]);
    while (it != entryMap.end()) {
      if (it->first != posFontNames[z])
        break;
      MWAWEntry &entry = it++->second;
      readPosToFont(entry, NSStruct::ZoneType(z));
    }
  }

  // the paragraph: in mainZone 1004 means style paragraph
  char const *(paragNames[]) = { "RULE", "FRUL", "HRUL" };
  for (int z = 0; z < 3; z++) {
    it = entryMap.lower_bound(paragNames[z]);
    while (it != entryMap.end()) {
      if (it->first != paragNames[z])
        break;
      MWAWEntry &entry = it++->second;
      readParagraphs(entry, NSStruct::ZoneType(z));
    }
  }
  // the picture associated to the paragraph
  char const *(pictDNames[]) = { "PICD", "FPIC", "HPIC" };
  for (int z = 0; z < 3; z++) {
    it = entryMap.lower_bound(pictDNames[z]);
    while (it != entryMap.end()) {
      if (it->first != pictDNames[z])
        break;
      MWAWEntry &entry = it++->second;
      readPICD(entry, NSStruct::ZoneType(z));
    }
  }

  // End of the style zone

  // style name ( can also contains some flags... )
  it = entryMap.lower_bound("STNM");
  while (it != entryMap.end()) {
    if (it->first != "STNM")
      break;
    MWAWEntry &entry = it++->second;
    std::vector<std::string> list;
    m_mainParser->readStringsList(entry, list);
  }
  // style link to paragraph name
  it = entryMap.lower_bound("STRL");
  while (it != entryMap.end()) {
    if (it->first != "STRL")
      break;
    MWAWEntry &entry = it++->second;
    std::vector<std::string> list;
    m_mainParser->readStringsList(entry, list);
  }
  // style next name
  it = entryMap.lower_bound("STNX");
  while (it != entryMap.end()) {
    if (it->first != "STNX")
      break;
    MWAWEntry &entry = it++->second;
    std::vector<std::string> list;
    m_mainParser->readStringsList(entry, list);
  }

  // the text zone
  char const *(textNames[]) = { "", "FNTX", "HFTX" };
  for (int z = 1; z < 3; z++) {
    it = entryMap.lower_bound(textNames[z]);
    while (it != entryMap.end()) {
      if (it->first != textNames[z])
        break;
      m_state->m_zones[z].m_entry = it++->second;
      m_state->m_zones[z].m_entry.setId(z);
    }
  }
  // now update the different data
  computePositions();
  return true;
}

////////////////////////////////////////////////////////////
//     Fonts
////////////////////////////////////////////////////////////

// send the font to the listener
void NSText::setProperty(MWAWFont const &font, MWAWFont &previousFont)
{
  if (!m_listener) return;
  font.sendTo(m_listener.get(), m_convertissor, previousFont);
}

// read a list of fonts
bool NSText::readFontsList(MWAWEntry const &entry)
{
  if (!entry.valid() && entry.length()!=0) {
    MWAW_DEBUG_MSG(("NSText::readFontsList: the entry is bad\n"));
    return false;
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &asciiFile = m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(FontNames)[" << entry.id() << "]:";
  asciiFile.addPos(pos-4);
  asciiFile.addNote(f.str().c_str());

  int num=0;
  while(!input->atEOS()) {
    pos = input->tell();
    if (pos == entry.end()) break;
    if (pos+4 > entry.end()) {
      asciiFile.addPos(pos);
      asciiFile.addNote("FontNames###");

      MWAW_DEBUG_MSG(("NSText::readFontsList: can not read flst\n"));
      return false;
    }
    int fId = (int)input->readULong(2);
    f.str("");
    f << "FontNames" << num++ << ":fId=" << std::hex << fId << std::dec << ",";
    int pSz = (int)input->readULong(1);

    if (pSz+1+pos+2 > entry.end()) {
      f << "###";
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("NSText::readFontsList: can not read pSize\n"));
      return false;
    }
    std::string name("");
    for (int c=0; c < pSz; c++)
      name += (char) input->readULong(1);
    m_convertissor->setCorrespondance(fId, name);
    f << name;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    if ((pSz%2)==0) input->seek(1,WPX_SEEK_CUR);
  }
  return true;
}

// read the FTAB/STYL resource: a font format ?
bool NSText::readFonts(MWAWEntry const &entry)
{
  bool isStyle = entry.type()=="STYL";
  int const fSize = isStyle ? 58 : 98;
  std::string name(isStyle ? "Style" : "Fonts");
  if ((!entry.valid()&&entry.length()) || (entry.length()%fSize)) {
    MWAW_DEBUG_MSG(("NSText::readFonts: the entry is bad\n"));
    return false;
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &asciiFile = m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int numElt = int(entry.length()/fSize);
  libmwaw::DebugStream f;
  f << "Entries(" << name << ")[" << entry.id() << "]:N=" << numElt;
  asciiFile.addPos(pos-4);
  asciiFile.addNote(f.str().c_str());

  long val;
  for (int i = 0; i < numElt; i++) {
    NSTextInternal::Font font;
    pos = input->tell();
    f.str("");
    if (!isStyle)
      font.m_pictureId = (int)input->readLong(2);

    if (font.m_pictureId) {
      // the two value seems to differ slightly for a picture
      val = (long)input->readULong(2);
      if (val != 0xFF01) f << "#pictFlags0=" << std::hex << val << ",";
      font.m_pictureWidth = (int)input->readLong(2);
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
      // f0=0|1 and if f0=1 then f1 is a small number between 1 and 20
      for (int j = 0; j < 20; j++) {
        val = (int) input->readULong(2);
        if (val) f << "g" << j << "=" << val << ",";
      }
      // 0, -1 or a small number related to the variable id : probably unknowncst+vId
      font.m_variableId = (int)input->readLong(4);
      // the remaining seems to be 0 excepted for picture
      for (int j = 0; j < 4; j++) { // find 0,0,[0|d5|f5|f8|ba], big number
        val = (int) input->readULong(2);
        if (val) f << "h" << j << "=" << std::hex << val << std::dec << ",";
      }
      // two dim ?
      int dim[4];
      for (int st = 0; st < 2; st++) {
        for (int j = 0; j < 4; j++)
          dim[j] = (int) input->readLong(2);
        font.m_pictureDim[st]=Box2i(Vec2i(dim[1],dim[0]),Vec2i(dim[3],dim[2]));
      }
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
    if (input->tell() != pos+fSize)
      asciiFile.addDelimiter(input->tell(),'|');
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+fSize, WPX_SEEK_SET);
  }
  return true;
}

// read the FRMT resource: filepos -> fonts
bool NSText::readPosToFont(MWAWEntry const &entry, NSStruct::ZoneType zoneId)
{
  if (!entry.valid() || (entry.length()%10)) {
    MWAW_DEBUG_MSG(("NSText::readPosToFont: the entry is bad\n"));
    return false;
  }
  if (zoneId < 0 || zoneId >= 3) {
    MWAW_DEBUG_MSG(("NSText::readPosToFont: find unexpected zoneId: %d\n", zoneId));
    return false;
  }
  NSTextInternal::Zone &zone = m_state->m_zones[zoneId];
  entry.setParsed(true);
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &asciiFile = m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int numElt = int(entry.length()/10);
  libmwaw::DebugStream f;
  f << "Entries(PosToFont)[" << zoneId << "]:N=" << numElt;
  asciiFile.addPos(pos-4);
  asciiFile.addNote(f.str().c_str());

  NSStruct::Position position;
  NSTextInternal::DataPLC plc;
  plc.m_type = NSTextInternal::P_Format;
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();
    f.str("");
    f << "PosToFont" << i << "[" << zoneId << "]:";
    position.m_paragraph = (int) input->readULong(4); // checkme or ??? m_paragraph
    position.m_word = (int) input->readULong(2);
    position.m_char = (int) input->readULong(2);
    f << "pos=" << position << ",";
    int id = (int) input->readLong(2);
    f << "F" << id << ",";
    plc.m_id = id;
    zone.m_plcMap.insert(NSTextInternal::Zone::PLCMap::value_type(position, plc));
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+10, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// the paragraphs
////////////////////////////////////////////////////////////

// send the paragraph to the listener
void NSText::setProperty(NSTextInternal::Paragraph const &para, int width)
{
  if (!m_listener) return;
  double origRMargin = para.m_margins[2].get();
  double rMargin=double(width)/72.-origRMargin;
  if (rMargin < 0.0) rMargin = 0;
  const_cast<NSTextInternal::Paragraph &>(para).m_margins[2] = rMargin;
  para.send(m_listener);
  const_cast<NSTextInternal::Paragraph &>(para).m_margins[2] = origRMargin;
}

// read the RULE resource: a list of rulers
bool NSText::readParagraphs(MWAWEntry const &entry, NSStruct::ZoneType zoneId)
{
  if (!entry.valid() && entry.length() != 0) {
    MWAW_DEBUG_MSG(("NSText::readParagraphs: the entry is bad\n"));
    return false;
  }
  if (zoneId < 0 || zoneId >= 3) {
    MWAW_DEBUG_MSG(("NSText::readParagraphs: find unexpected zoneId: %d\n", zoneId));
    return false;
  }
  NSTextInternal::Zone &zone = m_state->m_zones[zoneId];

  entry.setParsed(true);
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &asciiFile = m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int numElt = int(entry.length()/98);
  libmwaw::DebugStream f, f2;
  f << "Entries(RULE)[" << entry.type() << entry.id() << "]";
  if (entry.id()==1004) f << "[Styl]";
  else if (entry.id() != 1003) {
    MWAW_DEBUG_MSG(("NSText::readParagraphs: find unexpected entryId: %d\n", entry.id()));
    f << "###";
  }
  f << ":N=" << numElt;
  asciiFile.addPos(pos-4);
  asciiFile.addNote(f.str().c_str());

  NSTextInternal::DataPLC plc;
  plc.m_type = NSTextInternal::P_Ruler;

  long val;
  while (input->tell() != entry.end()) {
    int num = (entry.id() == 1003) ? (int)zone.m_paragraphList.size() : -1;
    pos = input->tell();
    f.str("");
    if (pos+8 > entry.end() || input->atEOS()) {
      f << "RULE" << num << "[" << zoneId << "]:###";
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("NSText::readParagraphs: can not read end of zone\n"));
      return false;
    }

    long nPara = (long) input->readULong(4);
    if (nPara == 0x7FFFFFFF) {
      input->seek(-4, WPX_SEEK_CUR);
      break;
    }
    NSStruct::Position textPosition;
    textPosition.m_paragraph = (int) nPara;  // checkme or ???? + para

    long sz = (long) input->readULong(4);
    if (sz < 0x42 || pos+sz > entry.end()) {
      f << "RULE" << num << "[" << zoneId << "]:###";
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("NSText::readParagraphs: can not read the size zone\n"));
      return false;
    }
    NSTextInternal::Paragraph para;
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
    para.m_margins[2] = float(input->readLong(4))/65536.f/72.f;

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
      // before spacing is also in %, try to correct it
      para.m_spacings[1] = para.m_spacings[1].get()*12.0;
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
      MWAW_DEBUG_MSG(("NSText::readParagraphs: can not read the string\n"));
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
        MWAW_DEBUG_MSG(("NSText::readParagraphs: can not read the ruler name\n"));
        asciiFile.addDelimiter(input->tell()-1,'#');
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
      zone.m_plcMap.insert(NSTextInternal::Zone::PLCMap::value_type(textPosition, plc));
    }

    f.str("");
    f << "RULE" << num << "[" << zoneId << "]:";
    f << "paragraph=" << nPara << "," << para;

    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+sz, WPX_SEEK_SET);
  }
  pos = input->tell();
  f.str("");
  f << "RULE[" << zoneId << "](II):";
  if (pos+66 != entry.end() || input->readULong(4) != 0x7FFFFFFF) {
    f << "###";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());

    MWAW_DEBUG_MSG(("NSText::readParagraphs: find odd end\n"));
    return true;
  }
  for (int i = 0; i < 31; i++) { // only find 0 expected f12=0|100
    val = (long) input->readLong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
//     the zones header
////////////////////////////////////////////////////////////

// read the header/footer main entry
bool NSText::readHeaderFooter(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%32)) {
    MWAW_DEBUG_MSG(("NSText::readHeaderFooter: the entry is bad\n"));
    return false;
  }
  NSTextInternal::Zone &zone = m_state->m_zones[NSStruct::Z_HeaderFooter];
  entry.setParsed(true);
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &asciiFile = m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int numElt = int(entry.length()/32);
  libmwaw::DebugStream f;
  f << "Entries(HeaderFooter)[" << entry.id() << "]:N=" << numElt;
  asciiFile.addPos(pos-4);
  asciiFile.addNote(f.str().c_str());

  NSTextInternal::DataPLC plc;
  plc.m_type = NSTextInternal::P_HeaderFooter;
  long val;
  long lastPara = 0;
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();
    f.str("");
    NSTextInternal::HeaderFooter hf;
    hf.m_textParagraph = input->readLong(4);
    hf.m_paragraph[0] = lastPara;
    hf.m_paragraph[1] = lastPara = input->readLong(4);
    int what = (int) input->readULong(2);
    switch((what>>2)&0x3) {
    case 1:
      hf.m_type = MWAWPageSpan::HEADER;
      break;
    case 2:
      hf.m_type = MWAWPageSpan::FOOTER;
      break;
    default:
      f << "#what=" << ((what>>2)&0x3);
      break;
    }
    switch(what&0x3) {
    case 1:
      hf.m_occurence = MWAWPageSpan::ODD;
      break;
    case 2:
      hf.m_occurence = MWAWPageSpan::EVEN;
      break;
    case 3:
      hf.m_occurence = MWAWPageSpan::ALL;
      break;
    default:
      f << "[#page],";
      break;
    }
    if (what&0xFFF0) f << "#flags=" << std::hex << (what&0xFFF0) << ",";
    // find 0|0x10|0x18|0x20|0x36|0x3a|0x40|0x4c|0x66 here
    hf.m_unknown = (int) input->readLong(2);
    for (int j = 0; j < 10; j++) { // always 0 ?
      val = (long) input->readLong(2);
      if (val) f << "g" << j << "=" << val << ",";
    }
    hf.m_extra = f.str();
    f.str("");
    f << "HeaderFooter" << i << ":" << hf;

    m_state->m_hfList.push_back(hf);
    plc.m_id = i+1;
    NSStruct::Position hfPosition;
    hfPosition.m_paragraph=int(lastPara);
    zone.m_plcMap.insert(NSTextInternal::Zone::PLCMap::value_type(hfPosition, plc));
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+32, WPX_SEEK_SET);
  }
  return true;
}

// read the footnote main entry
bool NSText::readFootnotes(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%36)) {
    MWAW_DEBUG_MSG(("NSText::readFootnotes: the entry is bad\n"));
    return false;
  }
  NSTextInternal::Zone &mainZone = m_state->m_zones[NSStruct::Z_Main];
  NSTextInternal::Zone &zone = m_state->m_zones[NSStruct::Z_Footnote];
  entry.setParsed(true);
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &asciiFile = m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int numElt = int(entry.length()/36);
  libmwaw::DebugStream f;
  f << "Entries(Footnotes)[" << entry.id() << "]:N=" << numElt;
  asciiFile.addPos(pos-4);
  asciiFile.addNote(f.str().c_str());

  NSTextInternal::DataPLC plc;
  plc.m_type = NSTextInternal::P_Footnote;
  int lastParagraph = 0;
  long val;
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();
    f.str("");
    NSTextInternal::Footnote footnote;
    footnote.m_textPosition.m_paragraph = (int) input->readULong(4); // checkme or ??? m_paragraph
    footnote.m_textPosition.m_word = (int) input->readULong(2);
    footnote.m_textPosition.m_char = (int) input->readULong(2);
    footnote.m_paragraph[0] = lastParagraph;
    lastParagraph = (int) input->readULong(4); // checkme or ??? m_paragraph
    footnote.m_paragraph[1] = lastParagraph;
    // find f0=0|55|6f, f1=15|16|26|27|39|3a
    for (int j = 0; j < 2; j++) {
      val = input->readLong(2);
      if (val) f << "f" << j << "=" << std::hex << val << std::dec << ",";
    }
    footnote.m_number = (int) input->readLong(2);
    for (int j = 0; j < 3; j++) { // always 0 ?
      val = input->readLong(2);
      if (val) f << "g" << j << "=" << val << ",";
    }
    for (int wh = 0; wh < 2; wh++) {
      input->seek(pos+24+wh*6, WPX_SEEK_SET);
      std::string label("");
      for (int c = 0; c < 6; c++) {
        char ch = (char) input->readULong(1);
        if (ch == 0)
          break;
        label += ch;
      }
      if (wh==0) footnote.m_noteLabel = label;
      else footnote.m_textLabel = label;
    }
    footnote.m_extra = f.str();
    f.str("");
    f << "Footnotes" << i << ":" << footnote;

    m_state->m_footnoteList.push_back(footnote);
    plc.m_id = i;
    mainZone.m_plcMap.insert(NSTextInternal::Zone::PLCMap::value_type(footnote.m_textPosition, plc));
    NSStruct::Position notePosition;
    notePosition.m_paragraph= footnote.m_paragraph[0];
    zone.m_plcMap.insert(NSTextInternal::Zone::PLCMap::value_type(notePosition, plc));

    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+36, WPX_SEEK_SET);
  }
  return true;
}

// read the PICD zone ( a list of picture ? )
bool NSText::readPICD(MWAWEntry const &entry, NSStruct::ZoneType zoneId)
{
  if ((!entry.valid()&&entry.length()) || (entry.length()%14)) {
    MWAW_DEBUG_MSG(("NSText::readPICD: the entry is bad\n"));
    return false;
  }
  if (zoneId < 0 || zoneId >= 3) {
    MWAW_DEBUG_MSG(("NSText::readPICD: find unexpected zoneId: %d\n", zoneId));
    return false;
  }
  NSTextInternal::Zone &zone = m_state->m_zones[zoneId];
  entry.setParsed(true);
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int numElt = int(entry.length()/14);
  libmwaw::DebugStream f;
  f << "Entries(PICD)[" << zoneId << "]:N=" << numElt;
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  NSTextInternal::DataPLC plc;
  plc.m_type = NSTextInternal::P_PicturePara;
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();
    f.str("");
    NSTextInternal::PicturePara pict;
    pict.m_paragraph = (int) input->readLong(4);
    int dim[4];
    for (int j = 0; j < 4; j++)
      dim[j] = (int) input->readLong(2);
    pict.m_position = Box2i(Vec2i(dim[1],dim[0]), Vec2i(dim[3],dim[2]));
    pict.m_id = (int) input->readULong(2);
    zone.m_pictureParaList.push_back(pict);

    NSStruct::Position pictPosition;
    pictPosition.m_paragraph= pict.m_paragraph;
    plc.m_id = i;
    zone.m_plcMap.insert(NSTextInternal::Zone::PLCMap::value_type(pictPosition, plc));

    f << "PICD" << i << ":" << pict;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+14, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
long NSText::findFilePos(NSStruct::ZoneType zoneId, NSStruct::Position const &pos)
{
  if (zoneId < 0 || zoneId >= 3) {
    MWAW_DEBUG_MSG(("NSText::findFilePos: find unexpected zoneId: %d\n", zoneId));
    return -1;
  }
  NSTextInternal::Zone &zone = m_state->m_zones[zoneId];
  MWAWEntry entry = zone.m_entry;
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("NSText::findFilePos: the entry is bad\n"));
    return -1;
  }

  bool isMain = zoneId == NSStruct::Z_Main;
  MWAWInputStreamPtr input = isMain ?
                             m_mainParser->getInput() : m_mainParser->rsrcInput();
  input->seek(entry.begin(), WPX_SEEK_SET);

  NSStruct::Position actPos;
  for (int i = 0; i < entry.length(); i++) {
    if (input->atEOS())
      break;
    if (pos == actPos)
      return input->tell();
    unsigned char c = (unsigned char) input->readULong(1);
    // update the position
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
  if (pos == actPos)
    return input->tell();
  MWAW_DEBUG_MSG(("NSText::findFilePos: can not find the position\n"));
  return -1;
}

////////////////////////////////////////////////////////////
bool NSText::sendHeaderFooter(int hfId)
{
  if (!m_listener) {
    MWAW_DEBUG_MSG(("NSText::sendHeaderFooter: can not find the listener\n"));
    return false;
  }
  if (hfId >= int(m_state->m_hfList.size())) {
    MWAW_DEBUG_MSG(("NSText::sendHeaderFooter: can not find the headerFooter list\n"));
    return false;
  }
  if (hfId < 0)
    return true;
  NSTextInternal::HeaderFooter const &hf = m_state->m_hfList[size_t(hfId)];
  hf.m_parsed = true;

  MWAWEntry entry;
  entry.setId(NSStruct::Z_HeaderFooter);
  NSStruct::Position pos;
  pos.m_paragraph = (int) hf.m_paragraph[0];
  entry.setBegin(findFilePos(NSStruct::Z_HeaderFooter, pos));
  pos.m_paragraph = (int) hf.m_paragraph[1];
  entry.setEnd(findFilePos(NSStruct::Z_HeaderFooter, pos));
  if (entry.begin() < 0 || entry.length() < 0) {
    MWAW_DEBUG_MSG(("NSText::sendHeaderFooter: can not compute the headerFooter entry\n"));
    return false;
  }
  pos.m_paragraph = (int) hf.m_paragraph[0];
  sendText(entry, pos);
  return true;
}

////////////////////////////////////////////////////////////
bool NSText::sendFootnote(int footnoteId)
{
  if (!m_listener) {
    MWAW_DEBUG_MSG(("NSText::sendFootnote: can not find the listener\n"));
    return false;
  }
  if (footnoteId >= int(m_state->m_footnoteList.size())) {
    MWAW_DEBUG_MSG(("NSText::sendFootnote: can not find the footnote list\n"));
    return false;
  }
  if (footnoteId < 0)
    return true;
  NSTextInternal::Footnote const &fnote =
    m_state->m_footnoteList[size_t(footnoteId)];
  fnote.m_parsed = true;

  MWAWEntry entry;
  entry.setId(NSStruct::Z_Footnote);
  NSStruct::Position pos;
  pos.m_paragraph = fnote.m_paragraph[0];
  entry.setBegin(findFilePos(NSStruct::Z_Footnote, pos));
  pos.m_paragraph = fnote.m_paragraph[1];
  entry.setEnd(findFilePos(NSStruct::Z_Footnote, pos));
  if (entry.begin() < 0 || entry.length() < 0) {
    MWAW_DEBUG_MSG(("NSText::sendFootnote: can not compute the footnote entry\n"));
    return false;
  }
  pos.m_paragraph = fnote.m_paragraph[0];
  sendText(entry, pos);
  return true;
}

////////////////////////////////////////////////////////////
bool NSText::sendText(MWAWEntry entry, NSStruct::Position firstPos)
{
  if (!m_listener) {
    MWAW_DEBUG_MSG(("NSText::sendText: can not find the listener\n"));
    return false;
  }
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("NSText::sendText: the entry is bad\n"));
    return false;
  }

  NSStruct::ZoneType zoneId = (NSStruct::ZoneType) entry.id();
  if (zoneId < 0 || zoneId >= 3) {
    MWAW_DEBUG_MSG(("NSText::sendText: find unexpected zoneId: %d\n", zoneId));
    return false;
  }
  NSTextInternal::Zone &zone = m_state->m_zones[zoneId];
  bool isMain = zoneId == NSStruct::Z_Main;
  int nCol = 1;
  int width = int(72.0*m_mainParser->pageWidth());
  if (isMain || zoneId == NSStruct::Z_Footnote) {
    float colSep = 0.5f;
    m_mainParser->getColumnInfo(nCol, colSep);
    if (nCol > 1)
      width /= nCol;
    if (isMain && nCol > 1) {
      std::vector<int> colSize;
      colSize.resize((size_t) nCol, width);
      if (m_listener->isSectionOpened())
        m_listener->closeSection();
      m_listener->openSection(colSize, WPX_POINT);
    }
  }
  MWAWInputStreamPtr input
    = isMain ? m_mainParser->getInput() : m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile =
    isMain ? m_mainParser->ascii() : m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(TEXT)[" << zoneId << "]:";
  std::string str("");
  NSStruct::Position actPos(firstPos);
  NSTextInternal::Zone::PLCMap::iterator it = zone.m_plcMap.begin();
  while (it != zone.m_plcMap.end() && it->first.cmp(actPos) < 0) {
    it++;
    continue;
  }

  NSTextInternal::Font actFont;
  actFont.m_font = MWAWFont(3,12);
  setProperty(actFont.m_font, actFont.m_font);
  NSStruct::FootnoteInfo ftInfo;
  m_mainParser->getFootnoteInfo(ftInfo);
  bool lastCharFootnote = false;
  int noteId = 0, numVar=0, actVar=-1;
  std::vector<int> varValues;
  std::map<int,int> fontIdToVarIdMap;
  for (int i = 0; i <= entry.length(); i++) {
    if (i==entry.length() && !lastCharFootnote)
      break;
    while (it != zone.m_plcMap.end() && it->first.cmp(actPos) <= 0) {
      NSStruct::Position const &plcPos = it->first;
      NSTextInternal::DataPLC const &plc = it++->second;
      f << str;
      str="";
      if (plcPos.cmp(actPos) < 0) {
        MWAW_DEBUG_MSG(("NSText::sendText: oops find unexpected position\n"));
        f << "###[" << plc << "]";
        continue;
      }
      f << "[" << plc << "]";
      switch (plc.m_type) {
      case NSTextInternal::P_Format: {
        if (plc.m_id < 0 || plc.m_id >= int(m_state->m_fontList.size())) {
          MWAW_DEBUG_MSG(("NSText::sendText: oops can not find the actual font\n"));
          f << "###";
          break;
        }
        NSTextInternal::Font const &font = m_state->m_fontList[size_t(plc.m_id)];
        actFont = font;
        if (font.m_pictureId <= 0)
          setProperty(font.m_font, actFont.m_font);
        if (!font.isVariable())
          break;
        if (fontIdToVarIdMap.find(plc.m_id) != fontIdToVarIdMap.end())
          actVar = fontIdToVarIdMap.find(plc.m_id)->second;
        else {
          actVar = numVar++;
          fontIdToVarIdMap[plc.m_id]=actVar;
        }
        break;
      }
      case NSTextInternal::P_Ruler: {
        if (plc.m_id < 0 || plc.m_id >= int(zone.m_paragraphList.size())) {
          MWAW_DEBUG_MSG(("NSText::sendText: oops can not find the actual ruler\n"));
          f << "###";
          break;
        }
        NSTextInternal::Paragraph const &para = zone.m_paragraphList[size_t(plc.m_id)];
        setProperty(para, width);
        break;
      }
      case NSTextInternal::P_Footnote: {
        if (!isMain) break;
        if (!lastCharFootnote) {
          MWAW_DEBUG_MSG(("NSText::sendText: oops do not find the footnote symbol\n"));
          break;
        }
        if (plc.m_id < 0 || plc.m_id >= int(m_state->m_footnoteList.size())) {
          MWAW_DEBUG_MSG(("NSText::sendText: can not find the footnote\n"));
          MWAWSubDocumentPtr subdoc(new NSTextInternal::SubDocument(*this, input, -1, libmwaw::DOC_NOTE));
          m_listener->insertNote(MWAWContentListener::FOOTNOTE, subdoc);
          break;
        }
        MWAWSubDocumentPtr subdoc(new NSTextInternal::SubDocument(*this, input, plc.m_id, libmwaw::DOC_NOTE));
        NSTextInternal::Footnote const &fnote = m_state->m_footnoteList[plc.m_id];
        noteId++;
        if (fnote.m_number && noteId != fnote.m_number)
          noteId = fnote.m_number;
        MWAWContentListener::NoteType noteType
          = ftInfo.endNotes() ? MWAWContentListener::ENDNOTE :
            MWAWContentListener::FOOTNOTE;
        m_listener->resetNoteNumber(noteType, noteId);
        std::string label = fnote.getTextLabel(noteId);
        if (label.length())
          m_listener->insertLabelNote(noteType, label.c_str(), subdoc);
        else
          m_listener->insertNote(noteType, subdoc);

        break;
      }
      case NSTextInternal::P_PicturePara: {
        if (plc.m_id < 0 || plc.m_id >= int(zone.m_pictureParaList.size())) {
          MWAW_DEBUG_MSG(("NSText::sendText: can not find the paragraph picture\n"));
          break;
        }
        NSTextInternal::PicturePara &pict = zone.m_pictureParaList[int(plc.m_id)];
        MWAWPosition pictPos(pict.m_position.min(), pict.m_position.size(), WPX_POINT);
        pictPos.setRelativePosition(MWAWPosition::Paragraph);
        pictPos.m_wrapping = MWAWPosition::WRunThrough;
        m_mainParser->sendPicture(pict.m_id, pictPos);
        break;
      }
      case NSTextInternal::P_HeaderFooter:
        break;
      case NSTextInternal::P_Unknown:
      default:
        MWAW_DEBUG_MSG(("NSText::sendText: oops can not find unknown plc type\n"));
        f << "###";
        break;
      }
    }

    if (input->atEOS())
      break;
    if (i==entry.length())
      break;
    unsigned char c = (unsigned char) input->readULong(1);
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

    // update the position
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

    // send char
    lastCharFootnote = false;
    switch(c) {
    case 0x1: {
      if (actFont.m_pictureId <= 0) {
        MWAW_DEBUG_MSG(("NSText::sendText: can not find pictureId for char 1\n"));
        f << "#";
        break;
      }
      MWAWPosition pictPos(actFont.m_pictureDim[0].min(), actFont.m_pictureDim[0].size(), WPX_POINT);
      pictPos.setRelativePosition(MWAWPosition::CharBaseLine);
      pictPos.setClippingPosition
      (actFont.m_pictureDim[1].min()-actFont.m_pictureDim[0].min(),
       actFont.m_pictureDim[0].max()-actFont.m_pictureDim[1].max());
      m_mainParser->sendPicture(actFont.m_pictureId, pictPos);
      break;
    }
    case 0x9:
      m_listener->insertTab();
      break;
    case 0xc:
      if (!isMain) break;
      m_mainParser->newPage(++m_state->m_actualPage);
      if (ftInfo.resetNumberOnNewPage()) noteId = 0;
      break;
    case 0xd:
      m_listener->insertEOL();
      break;
    case 0xf: {
      std::string format(m_mainParser->getDateFormat(zoneId, actVar));
      if (format.length())
        m_listener->insertDateTimeField(format.c_str());
      else {
        f << "#";
        m_listener->insertField(MWAWContentListener::Date);
      }
      break;
    }
    case 0x10:
      m_listener->insertDateTimeField("%H:%M");
      break;
    case 0x11:
      m_listener->insertField(MWAWContentListener::Title);
      break;
    case 0x14: // mark separator ( ok to ignore )
      break;
    case 0x1c:
      if (isMain)
        lastCharFootnote = true;
      break;
    case 0x1d: {
      MWAWContentListener::FieldType fType;
      std::string content;
      if (!m_mainParser->getReferenceData(zoneId, actVar, fType, content, varValues)) {
        m_listener->insertCharacter(' ');
        f << "#";
        break;
      }
      if (fType != MWAWContentListener::None)
        m_listener->insertField(fType);
      else if (content.length())
        m_listener->insertUnicodeString(content.c_str());
      else
        f << "#";
      break;
    }
    default: {
      int unicode = m_convertissor->unicode (actFont.m_font.id(),c);
      if (unicode == -1) {
        if (c < 32) {
          MWAW_DEBUG_MSG(("NSText::send: Find odd char %x\n", int(c)));
          f << "#";
        } else
          m_listener->insertCharacter(c);
      } else
        m_listener->insertUnicode((uint32_t) unicode);
      break;
    }
    }
  }
  f << str;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

//! send data to the listener
bool NSText::sendMainText()
{
  if (!m_listener) return true;

  if (!m_state->m_zones[0].m_entry.valid()) {
    MWAW_DEBUG_MSG(("NSText::sendMainText: can not find the main text\n"));
    return false;
  }
  m_state->m_zones[0].m_entry.setParsed(true);
  sendText(m_state->m_zones[0].m_entry);
  return true;
}


void NSText::flushExtra()
{
  if (!m_listener) return;
  for (size_t f = 0; f < m_state->m_footnoteList.size(); f++) {
    if (m_state->m_footnoteList[f].m_parsed)
      continue;
    sendFootnote(int(f));
  }
  m_listener->insertCharacter(' ');
  for (size_t hf = 0; hf < m_state->m_hfList.size(); hf++) {
    if (m_state->m_hfList[hf].m_parsed)
      continue;
    sendHeaderFooter(int(hf));
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
