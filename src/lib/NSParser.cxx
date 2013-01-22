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

#include "NSGraph.hxx"
#include "NSStruct.hxx"
#include "NSText.hxx"

#include "NSParser.hxx"

/** Internal: the structures of a NSParser */
namespace NSParserInternal
{
/** Internal structure: use to store a numbering, a variable or a version */
struct Variable {
  //! Constructor
  Variable(NSStruct::VariableType type = NSStruct::V_None) :
    m_type(0), m_containerType(type), m_fieldType(-1), m_refId(-1),
    m_numberingType(libmwaw::NONE), m_startNumber(1), m_increment(1),
    m_prefix(""), m_suffix(""), m_dateFormat(0), m_sample(""), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Variable const &num);
  //! returns true if this is a date
  bool isDate() const {
    return m_fieldType == 1 || m_fieldType == 0xf;
  }
  //! returns the date format
  std::string getDateFormat() const {
    if (!isDate()) return "";
    switch(m_dateFormat) {
    case 0:
    case 0x20:
      return "%m/%d/%Y";
    case 0x40:
      return "%d/%m/%Y";
    case 1:
    case 0x21:
    case 2:
    case 0x22: // normally DDD, MMM d Y
      return "%A, %B %d %Y";
    case 0x41:
    case 0x42:
      return "%A, %d %B, %Y";
    case 0x81:
    case 0xa1:
    case 0x82:
    case 0xa2: // normally DDD, MMM d Y
      return "%B %d, %Y";
    case 0xc1:
    case 0xc2:
      return "%d %B, %Y";
    default:
      break;
    }
    return "";
  }
  //! returns true if this is a number field
  bool isNumbering() const {
    return m_type == 1 || m_type == 2;
  }
  //! the main type
  int m_type;
  //! the container type
  NSStruct::VariableType m_containerType;
  //! the variable type
  long m_fieldType;
  //! the reference id
  int m_refId;
  //! the numbering type
  libmwaw::NumberingType m_numberingType;
  //! the start number
  int m_startNumber;
  //! the increment
  int m_increment;
  //! the prefix
  std::string m_prefix;
  //! the suffix
  std::string m_suffix;
  //! the date format
  int m_dateFormat;
  //! a sample used in a dialog ?
  std::string m_sample;
  //! some extra debuging information
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, Variable const &num)
{
  switch(num.m_type) {
  case 1:
    o << "numbering,";
    break;
  case 2:
    o << "numbering[count],";
    break;
  case 3:
    o << "version,";
    break;
  case 4:
    o << "version[small],";
    break;
  case 5:
    o << "date/time,";
    break;
  case 6:
    o << "docTitle,";
    break;
  default:
    o << "#type=" << num.m_type << ",";
    break;
  }
  switch(num.m_containerType) {
  case NSStruct::V_Numbering:
    o << "number,";
    break;
  case NSStruct::V_Variable:
    o << "variable,";
    break;
  case NSStruct::V_Version:
    o << "version,";
    break;
  case NSStruct::V_None:
    break;
  default:
    o << "#type[container]=" << int(num.m_containerType) << ",";
    return o;
  }
  if (num.m_refId >= 0)
    o << "refId=" << num.m_refId << ",";
  switch(num.m_fieldType) {
  case -1:
    break;
  case 0x1:
    o << "date2,";
    break;
  case 0xe:
    o << "version,";
    break;
  case 0xf:
    o << "date,";
    break;
  case 0x10:
    o << "time,";
    break;
  case 0x11:
    o << "docTitle,";
    break;
  case 0x1c:
    o << "footnote,";
    break;
  case 0x1d:
    o << "reference?,";
    break;
    // alway in version variable ?
  case 0x7FFFFFFF:
    o << "none,";
    break;
    // in a variable find also 0xFFFF8014
  default:
    if ((num.m_fieldType>>16)==0x7FFF)
      o << "#fieldType=" << num.m_fieldType -0x7FFFFFFF-1  << ",";
    else if ((num.m_fieldType>>16)==0xFFFF)
      o << "#fieldType=X" << std::hex << num.m_fieldType << std::dec << ",";
    else
      o << "#fieldType=" << num.m_fieldType << ",";
    break;
  }
  std::string type = libmwaw::numberingTypeToString(num.m_numberingType);
  if (type.length())
    o << "type=" << type << ",";
  if (num.m_startNumber != 1) o << "start=" << num.m_startNumber << ",";
  if (num.m_increment != 1) o << "increment=" << num.m_increment << ",";
  static char const *(wh0[]) = { "unkn0", "prefix", "name", "comments" };
  if (num.m_prefix.length())
    o << wh0[num.m_containerType] << "=\"" << num.m_prefix << "\",";
  static char const *(wh2[]) = { "unkn2", "suffix", "suffix", "unkn2" };
  if (num.m_suffix.length())
    o << wh2[num.m_containerType] << "=\"" << num.m_suffix << "\",";
  static char const *(wh1[]) = { "unkn1", "sample", "sample", "author?" };
  if (num.m_sample.length())
    o << wh1[num.m_containerType] << "=\"" << num.m_sample << "\",";
  if (num.m_dateFormat) {
    switch(num.m_dateFormat & 0x9F) {
    case 1:
      o << "format=Day, Month D YYYY,";
      break;
    case 2:
      o << "format=Day, Mon D YYYY,";
      break;
    case 0x81:
      o << "format=Month D, YYYY,";
      break;
    case 0x82:
      o << "format=Mon D, YYYY,";
      break;
    default:
      o << "#format=" << std::hex << (num.m_dateFormat&0x9F) << std::dec << ",";
      break;
    }
    if (num.m_dateFormat & 0x20) o << "[english]";
    if (num.m_dateFormat & 0x40) o << "[european]";
    o << ",";
  }
  if (num.m_extra.length())
    o << num.m_extra;
  return o;
}

/** Internal structure: use to store a mark (reference) */
struct Reference {
  //! constructor
  Reference() : m_id(-1), m_textPosition(), m_text("") {
  }

  // the mark id ?
  int m_id;
  // the text position
  MWAWEntry m_textPosition;
  // the mark text
  std::string m_text;
};

//! internal structure used to stored some zone data
struct Zone {
  //! constructor
  Zone() : m_referenceList(), m_numberingResetList(), m_variableList(), m_versionList() {
  }
  /** the list of reference */
  std::vector<Reference> m_referenceList;
  //! the list of numbering reset id
  std::vector<int> m_numberingResetList;
  /** the list of variable */
  std::vector<Variable> m_variableList;
  /** the list of versions */
  std::vector<Variable> m_versionList;
};

////////////////////////////////////////
//! Internal: the state of a NSParser
struct State {
  //! constructor
  State() : m_numberingList(),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0),
    m_numColumns(1), m_columnSep(0.5f), m_footnoteInfo(), m_isTextDocument() {
  }

  /** the list of numbering */
  std::vector<Variable> m_numberingList;
  /** the main zones : Main, Footnote, HeaderFooter */
  Zone m_zones[3];
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
  int m_numColumns /** the number of columns */;
  float m_columnSep /** the columns separator */;
  /** the footnote placement */
  NSStruct::FootnoteInfo m_footnoteInfo;
  /** a bool to know if we examine a text document or another thing glossary ... */
  bool m_isTextDocument;
};
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
NSParser::NSParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_listener(), m_convertissor(), m_state(),
  m_pageSpan(), m_graphParser(), m_textParser()
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

  m_graphParser.reset(new NSGraph(getInput(), *this, m_convertissor));
  m_textParser.reset(new NSText(getInput(), *this, m_convertissor));
}

void NSParser::setListener(MWAWContentListenerPtr listen)
{
  m_listener = listen;
  m_textParser->setListener(listen);
  m_graphParser->setListener(listen);
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

Vec2f NSParser::getPageLeftTop() const
{
  return Vec2f(float(m_pageSpan.getMarginLeft()),
               float(m_pageSpan.getMarginTop()+m_state->m_headerHeight/72.0));
}

void NSParser::getColumnInfo(int &numColumns, float &colSep) const
{
  numColumns = m_state->m_numColumns;
  colSep = m_state->m_columnSep;
}

void NSParser::getFootnoteInfo(NSStruct::FootnoteInfo &fInfo) const
{
  fInfo = m_state->m_footnoteInfo;
}

////////////////////////////////////////////////////////////
// interface with the graph parser
////////////////////////////////////////////////////////////
bool NSParser::sendPicture(int pictId, MWAWPosition const &pictPos, WPXPropertyList extras)
{
  if (!rsrcInput()) return false;
  long pos = rsrcInput()->tell();
  bool ok=m_graphParser->sendPicture(pictId, true, pictPos, extras);
  rsrcInput()->seek(pos, WPX_SEEK_SET);
  return ok;
}

////////////////////////////////////////////////////////////
// access to variable date
////////////////////////////////////////////////////////////
std::string NSParser::getDateFormat(NSStruct::ZoneType zoneId, int vId) const
{
  if (zoneId < 0 || zoneId >= 3) {
    MWAW_DEBUG_MSG(("NSParser::getDateFormat: bad zone %d\n", zoneId));
    return "";
  }
  NSParserInternal::Zone const &zone = m_state->m_zones[zoneId];
  if (vId < 0 || vId >= int(zone.m_variableList.size()) ||
      !zone.m_variableList[size_t(vId)].isDate()) {
    // some version 3 files do not contain any variables, so returns the default value...
    if (version()==3 && zone.m_variableList.size()==0)
      return "%m/%d/%Y";
    MWAW_DEBUG_MSG(("NSParser::getDateFormat: can not find the variable %d\n", vId));
    return "";
  }
  return zone.m_variableList[size_t(vId)].getDateFormat();
}

bool NSParser::getReferenceData
(NSStruct::ZoneType zoneId, int vId,
 MWAWContentListener::FieldType &fType, std::string &content, std::vector<int> &values) const
{
  fType = MWAWContentListener::None;
  content = "";
  if (zoneId < 0 || zoneId >= 3) {
    MWAW_DEBUG_MSG(("NSParser::getReferenceData: bad zone %d\n", zoneId));
    return false;
  }
  NSParserInternal::Zone const &zone = m_state->m_zones[zoneId];
  if (vId < 0 || vId >= int(zone.m_variableList.size())) {
    MWAW_DEBUG_MSG(("NSParser::getReferenceData: can not find the variable %d\n", vId));
    return false;
  }
  NSParserInternal::Variable const &var=zone.m_variableList[size_t(vId)];
  if ((var.m_type != 1 && var.m_type != 2) || var.m_refId<=0) {
    MWAW_DEBUG_MSG(("NSParser::getReferenceData: find a variable with bad type %d\n", vId));
    return false;
  }
  // first special case
  if (var.m_type == 1 && var.m_refId == 14) {
    fType = MWAWContentListener::PageNumber;
    return true;
  }
  if (var.m_type == 2 && var.m_refId == 15) {
    fType = MWAWContentListener::PageCount;
    return true;
  }
  size_t numVar = m_state->m_numberingList.size();
  if (var.m_refId-1 >= int(numVar)) {
    MWAW_DEBUG_MSG(("NSParser::getReferenceData: can not find numbering variable for %d\n", vId));
    return false;
  }
  // resize values if needed
  for (size_t p = values.size(); p < numVar; p++)
    values.push_back(m_state->m_numberingList[p].m_startNumber
                     -m_state->m_numberingList[p].m_increment);
  NSParserInternal::Variable const &ref=m_state->m_numberingList[size_t(var.m_refId-1)];
  values[size_t(var.m_refId-1)] += ref.m_increment;

  size_t numReset = zone.m_numberingResetList.size();
  if (numReset < numVar+1)
    numReset = numVar+1;
  if (size_t(var.m_refId) < numReset) {
    std::vector<bool> doneValues;
    std::vector<int> toDoValues;
    doneValues.resize(numReset, false);
    doneValues[size_t(var.m_refId)]=true;
    toDoValues.push_back(int(var.m_refId));
    while(toDoValues.size()) {
      int modId = (int) toDoValues.back();
      toDoValues.pop_back();
      for (size_t r = 0; r < numReset; r++) {
        if (zone.m_numberingResetList[r] != modId)
          continue;
        if (r == 0 || doneValues[r]) continue;
        doneValues[r] = true;
        values[r-1] = m_state->m_numberingList[r-1].m_startNumber
                      -m_state->m_numberingList[r-1].m_increment;
        toDoValues.push_back(int(r));
      }
    }
  }
  std::stringstream s;
  std::string str = ref.m_prefix + ref.m_suffix;
  for (size_t p = 0; p < str.length(); p++) {
    unsigned char c = (unsigned char) str[p];
    if (c==0 || (c < 0x20 && c > numVar)) {
      MWAW_DEBUG_MSG(("NSParser::getReferenceData: find unknown variable\n"));
#ifdef DEBUG
      s << "###[" << int(c) << "]";
#endif
    } else if (c < 0x20)
      s << libmwaw::numberingValueToString
        (m_state->m_numberingList[size_t(c-1)].m_numberingType,
         values[size_t(c-1)]);
    else s << (char)c;
  }
  content=s.str();
  return true;
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
    if (ok) {
      createDocument(docInterface);
      m_graphParser->sendPageGraphics();
      m_textParser->sendMainText();
#ifdef DEBUG
      m_graphParser->flushExtra();
      m_textParser->flushExtra();
#endif
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
  int numPages = 1;
  if (m_graphParser->numPages() > numPages)
    numPages = m_graphParser->numPages();
  if (m_textParser->numPages() > numPages)
    numPages = m_textParser->numPages();
  m_state->m_numPages = numPages;

  shared_ptr<MWAWSubDocument> subDoc;
  for (int i = 0; i <= numPages; i++) {
    MWAWPageSpan ps(m_pageSpan);
    subDoc = m_textParser->getHeader(i+1);
    if (subDoc)
      ps.setHeaderFooter(MWAWPageSpan::HEADER, MWAWPageSpan::ALL, subDoc);
    subDoc = m_textParser->getFooter(i+1);
    if (subDoc)
      ps.setHeaderFooter(MWAWPageSpan::FOOTER, MWAWPageSpan::ALL, subDoc);
    pageList.push_back(ps);
  }

  //
  MWAWContentListenerPtr listen(new MWAWContentListener(m_convertissor, pageList, documentInterface));
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
  MWAWInputStreamPtr input = getInput();
  std::string type, creator;
  if (input && input->getFinderInfo(type, creator) && type != "TEXT")
    m_state->m_isTextDocument = false;
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
    readPageLimit(entry);
  }
  it = entryMap.lower_bound("INFO");
  while (it != entryMap.end()) {
    if (it->first != "INFO")
      break;
    MWAWEntry &entry = it++->second;
    readINFO(entry);
  }
  it = entryMap.lower_bound("ABBR");
  while (it != entryMap.end()) {
    if (it->first != "ABBR")
      break;
    MWAWEntry &entry = it++->second;
    readABBR(entry);
  }
  it = entryMap.lower_bound("FTA2");
  while (it != entryMap.end()) {
    if (it->first != "FTA2")
      break;
    MWAWEntry &entry = it++->second;
    readFTA2(entry);
  }
  it = entryMap.lower_bound("FnSc");
  while (it != entryMap.end()) {
    if (it->first != "FnSc")
      break;
    MWAWEntry &entry = it++->second;
    readFnSc(entry);
  }
  /** find also the resources :
      - alis: seems to contains data and some strings
      - OPEN: seems to contains one string: author? (size 0x20)
      - INF2: seems to contain some colors and some data (size 0x222)
      - sect: in two files: 010[1|a]0000acc0015f0000000[1|6]000... (size 0x24)
  */

  if (!m_textParser->createZones())
    return false;
  m_graphParser->createZones();

  // numbering, mark, variable, version, ...
  it = entryMap.lower_bound("CNAM"); // numbering name added in v5, v6
  while (it != entryMap.end()) {
    if (it->first != "CNAM")
      break;
    MWAWEntry &entry = it++->second;
    std::vector<std::string> list;
    readStringsList(entry, list, true);
  }

  it = entryMap.lower_bound("DSPL");
  while (it != entryMap.end()) {
    if (it->first != "DSPL")
      break;
    MWAWEntry &entry = it++->second;
    entry.setName("NumberingDef");
    NSStruct::RecursifData data(NSStruct::Z_Main, NSStruct::V_Numbering);
    data.read(*this, entry);
    readVariable(data);
  }
  char const *(variableNames[]) = { "VARI", "FVAR", "HVAR" };
  for (int z = 0; z < 3; z++) {
    it = entryMap.lower_bound(variableNames[z]);
    while (it != entryMap.end()) {
      if (it->first != variableNames[z])
        break;
      MWAWEntry &entry = it++->second;
      entry.setName("Variable");
      NSStruct::RecursifData data(NSStruct::ZoneType(z), NSStruct::V_Variable);
      data.read(*this, entry);
      readVariable(data);
    }
  }

  char const *(cntrNames[]) = { "CNTR", "FCNT", "HCNT" };
  for (int z = 0; z < 3; z++) {
    it = entryMap.lower_bound(cntrNames[z]);
    while (it != entryMap.end()) {
      if (it->first != cntrNames[z])
        break;
      MWAWEntry &entry = it++->second;
      readCNTR(entry, NSStruct::ZoneType(z));
    }
  }
  char const *(numbResetNames[]) = { "DPND", "FDPN", "HDPN" };
  for (int z = 0; z < 3; z++) {
    it = entryMap.lower_bound(numbResetNames[z]);
    while (it != entryMap.end()) {
      if (it->first != numbResetNames[z])
        break;
      MWAWEntry &entry = it++->second;
      readNumberingReset(entry, NSStruct::ZoneType(z));
    }
  }
  char const *(versionNames[]) = { "VRS ", "FVRS", "HVRS" };
  for (int z = 0; z < 3; z++) {
    it = entryMap.lower_bound(versionNames[z]);
    while (it != entryMap.end()) {
      if (it->first != versionNames[z])
        break;
      MWAWEntry &entry = it++->second;
      entry.setName("VariabS");
      NSStruct::RecursifData data(NSStruct::ZoneType(z), NSStruct::V_Version);
      data.read(*this, entry);
      readVariable(data);
    }
  }

  char const *(markNames[]) = { "MRK7", "FMRK", "HMRK" };
  for (int z = 0; z < 3; z++) {
    it = entryMap.lower_bound(markNames[z]);
    while (it != entryMap.end()) {
      if (it->first != markNames[z])
        break;
      MWAWEntry &entry = it++->second;
      entry.setName("Reference");
      NSStruct::RecursifData data = NSStruct::RecursifData(NSStruct::ZoneType(z));
      data.read(*this, entry);
      readReference(data);
    }
  }
  it = entryMap.lower_bound("XMRK"); // related to mark7 ?
  while (it != entryMap.end()) {
    if (it->first != "XMRK")
      break;
    MWAWEntry &entry = it++->second;
    entry.setName("XMRK");
    NSStruct::RecursifData data(NSStruct::Z_Main);
    data.read(*this, entry);
  }
  // unknown ?
  it = entryMap.lower_bound("SGP1");
  while (it != entryMap.end()) {
    if (it->first != "SGP1")
      break;
    MWAWEntry &entry = it++->second;
    entry.setName("SGP1");
    NSStruct::RecursifData data(NSStruct::Z_Main);
    data.read(*this, entry);
    readSGP1(data);
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
  /** no data fork, may be ok, but this means
      that the file contains no text, so... */
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !getRSRCParser())
    return false;
  MWAWRSRCParser::Version vers;
  // read the Nisus version
  int nisusVersion = -1;
  MWAWEntry entry = getRSRCParser()->getEntry("vers", 2002);
  if (!entry.valid()) entry = getRSRCParser()->getEntry("vers", 2);
  if (entry.valid() && getRSRCParser()->parseVers(entry, vers))
    nisusVersion = vers.m_majorVersion;
  else if (nisusVersion==-1) {
    MWAW_DEBUG_MSG(("NSParser::checkHeader: can not find the Nisus version\n"));
  }

  // read the file format version
  entry = getRSRCParser()->getEntry("vers", 1);
  if (!entry.valid() || !getRSRCParser()->parseVers(entry, vers)) {
    MWAW_DEBUG_MSG(("NSParser::checkHeader: can not find the Nisus file format version\n"));
    return false;
  }
  switch(vers.m_majorVersion) {
  case 3:
  case 4:
    MWAW_DEBUG_MSG(("NSParser::checkHeader: find Nisus %d file with file format version %d\n", nisusVersion, vers.m_majorVersion));
    break;
  default:
    MWAW_DEBUG_MSG(("NSParser::checkHeader: find Nisus %d file with unknown file format version %d\n", nisusVersion, vers.m_majorVersion));
    return false;
  }

  setVersion(vers.m_majorVersion);
  if (header)
    header->reset(MWAWDocument::NISUSW, version());

  return true;
}

////////////////////////////////////////////////////////////
// read a list of string
////////////////////////////////////////////////////////////
bool NSParser::readStringsList(MWAWEntry const &entry, std::vector<std::string> &list, bool simpleList)
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
    long endPos = entry.end();
    f.str("");
    f << entry.type() << list.size() << ":";

    if (!simpleList) {
      if (pos+2 > entry.end()) {
        f << "###";
        rsrcAscii().addPos(pos);
        rsrcAscii().addNote(f.str().c_str());

        MWAW_DEBUG_MSG(("NSParser::readStringsList: can not read strings\n"));
        return false;
      }
      int sz = (int)input->readULong(2);
      endPos = pos+2+sz;
      if (pos+2+sz > entry.end()) {
        f.str("");
        f << "###";
        rsrcAscii().addPos(pos);
        rsrcAscii().addNote(f.str().c_str());

        MWAW_DEBUG_MSG(("NSParser::readStringsList: zone size is bad\n"));
        return false;
      }
    }

    /* checkme: in STNM we can have a list of string, it is general or
       do we need to create a new function NSParser::readStringsListList
     */
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
      if (simpleList) continue;
      if ((pSz%2)==0) input->seek(1,WPX_SEEK_CUR);
    }
    list.push_back(str);
    rsrcAscii().addPos(pos);
    rsrcAscii().addNote(f.str().c_str());
    if (simpleList) break;
  }
  return true;
}

////////////////////////////////////////////////////////////
// read DSPL or the VRS zone: numbering definition or version
////////////////////////////////////////////////////////////
bool NSParser::readVariable(NSStruct::RecursifData const &data)
{
  if (!data.m_info || data.m_info->m_zoneType < 0 || data.m_info->m_zoneType >= 3) {
    MWAW_DEBUG_MSG(("NSParser::readVariable: find unexpected zoneType\n"));
    return false;
  }
  if (!data.m_childList.size())
    return true;

  if (data.m_childList.size() > 1) {
    MWAW_DEBUG_MSG(("NSParser::readVariable: level 0 node contains more than 1 node\n"));
  }
  if (data.m_childList[0].isLeaf()) {
    MWAW_DEBUG_MSG(("NSParser::readVariable: level 1 node is a leaf\n"));
    return false;
  }
  NSStruct::RecursifData const &mainData = *data.m_childList[0].m_data;
  size_t numData = mainData.m_childList.size();
  NSParserInternal::Zone &zone = m_state->m_zones[int(data.m_info->m_zoneType)];
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugStream f;
  long val;

  std::vector<NSParserInternal::Variable> *result = 0;
  int lastMaxId = 0;
  switch(data.m_info->m_variableType) {
  case NSStruct::V_Numbering:
    lastMaxId = 11;
    result = &m_state->m_numberingList;
    break;
  case NSStruct::V_Version:
    lastMaxId = 8;
    result = &zone.m_versionList;
    break;
  case NSStruct::V_Variable:
    lastMaxId = 12;
    result = &zone.m_variableList;
    break;
  case NSStruct::V_None:
  default:
    MWAW_DEBUG_MSG(("NSParser::readVariable: find unexpected dataType\n"));
    return false;
  }
  for (size_t n = 0; n < numData; n++) {
    static int const minExpectedSz[] = { 1, 4, 1, 2, 2, 2, 4, 4, 1, 4, 1 };
    NSStruct::RecursifData::Node const &node = mainData.m_childList[n];
    NSParserInternal::Variable num(data.m_info->m_variableType);
    num.m_type = node.m_type;

    if (node.isLeaf()) {
      MWAW_DEBUG_MSG(("NSParser::readVariable: level 2 node is a leaf\n"));
      continue;
    }
    NSStruct::RecursifData const &dt = *node.m_data;
    f.str("");
    int lastId = lastMaxId;
    switch(num.m_type) {
    case 1: // [2..12] with v12 is a string
      lastId = 12;
      f << "numbering,";
      break;
    case 2: // idem
      lastId = 12;
      f << "numbering[count],";
      break;
    case 3: // type = [2, 3.. 8]
      lastId = 8;
      f << "version,";
      break;
    case 4: // type = [2,3,4]
      lastId = 4;
      f << "version[small],";
      break;
    case 5: //type=[2,3,4,4]  second id=4: a small int : Date format/time
      lastId = 4;
      f << "date/time,";
      break;
    case 6:  // type=[2,3,4]
      lastId = 4;
      f << "docTitle,";
      break;
    default:
      f << "#";
      break;
    }
    rsrcAscii().addPos(node.m_entry.begin()-16);
    rsrcAscii().addNote(f.str().c_str());

    for (size_t nDt = 0; nDt < dt.m_childList.size(); nDt++) {
      if (!dt.m_childList[nDt].isLeaf()) {
        MWAW_DEBUG_MSG(("NSParser::readVariable: level 3 node is not a leaf\n"));
        continue;
      }
      MWAWEntry const &entry = dt.m_childList[nDt].m_entry;
      f.str("");
      input->seek(entry.begin(), WPX_SEEK_SET);
      int id = dt.m_childList[nDt].m_type;
      bool checkId = false;
      if (id != int(nDt)+2) {
        if (id == 4 && mainData.m_childList[n].m_type == 5 && entry.length() >= 2) {
          checkId = true;
          id = -4;
        } else
          f << "#id=" << id << ",";
      }

      if (!checkId && (id < 2 || id > lastId || entry.length() < minExpectedSz[id-2])) {
        MWAW_DEBUG_MSG(("NSParser::readVariable: find unexpected size for data %d\n", int(nDt)));
        f << "###";
        rsrcAscii().addPos(entry.begin()-12);
        rsrcAscii().addNote(f.str().c_str());
        continue;
      }
      switch(id) {
      case 2:
      case 4: // display example
      case 10:
      case 12: { // postfix : list of fields
        int mSz = (int) input->readULong(1);
        if (mSz+1 > entry.length()) {
          MWAW_DEBUG_MSG(("NSParser::readVariable: the dpsl seems to short\n"));
          f << "###Text";
          break;
        }
        std::string text("");
        for (int i = 0; i < mSz; i++)
          text+= (char) input->readULong(1);
        static char const *(wh0[]) = { "unkn0", "prefix", "name", "comments" };
        static char const *(wh1[]) = { "unkn1", "sample", "sample", "author?" };
        static char const *(wh2[]) = { "unkn2", "suffix", "suffix", "unkn2" };
        switch(id) {
        case 2:
          num.m_prefix = text;
          f << wh0[num.m_containerType];
          break;
        case 4:
          num.m_sample = text;
          f << wh1[num.m_containerType];
          break;
        case 10:
          num.m_suffix = text;
          f << wh2[num.m_containerType];
          break;
        case 12:
          f << "f12";
          break;
        default:
          f << "###id[" << id << "]";
        }
        f << "=\"" << text << "\"";
        break;
      }
      case -4: { // date format
        num.m_dateFormat = (int) input->readULong(2);
        if (!num.m_dateFormat) break; // default
        std::string format("");
        format = num.getDateFormat();
        if (format.length()) f << "format=" << format << ",";
        else f << "#format=" << std::hex << (num.m_dateFormat&0x9F) << std::dec << ",";
        break;
      }
      case 5: // style : 0=arabic, 1=roman, upperroman, abgadhawaz(arabic), alpha, upperalpha, hebrew
        val = (long) input->readULong(2);
        f << "type=";
        num.m_numberingType = libmwaw::ARABIC;
        switch(val) {
        case 0:
          f << "arabic,";
          break;
        case 1:
          num.m_numberingType = libmwaw::LOWERCASE_ROMAN;
          f << "roman,";
          break;
        case 2:
          num.m_numberingType = libmwaw::UPPERCASE_ROMAN;
          f << "upperroman,";
          break;
        case 3:
          f << "abgadhawaz,";
          break;
        case 4:
          num.m_numberingType = libmwaw::LOWERCASE;
          f << "alpha,";
          break;
        case 5:
          num.m_numberingType = libmwaw::UPPERCASE;
          f << "upperalpha,";
          break;
        case 6:
          f << "hebrew,";
          break;
        default:
          f << "#" << val << ",";
        }
        break;
      case 6: // start number
        num.m_startNumber = (int) input->readLong(2);
        f << "start=" << num.m_startNumber <<",";
        break;
      case 7: // increment
        num.m_increment = (int) input->readLong(2);
        f << "increment=" << num.m_increment <<",";
        break;
      case 9: // id ?
        num.m_refId = (int) input->readULong(4);
        f << "refId=" << num.m_refId << ",";
        break;
      case 3:
        num.m_fieldType = (long) input->readULong(4);
        switch (num.m_fieldType) {
          // 1(type 5) date ?
        case 0xe:
          f << "version,";
          break;
        case 0xf:
          f << "date,";
          break;
        case 0x10:
          f << "time,";
          break;
        case 0x11:
          f << "docTitle,";
          break;
        case 0x14: // find with 0xFFFF8014
          f << "mark,";
          break;
        case 0x1c:
          f << "footnote,";
          break;
        case 0x1d:
          f << "reference?,";
          break;
          // alway in version variable ?
        case 0x7FFFFFFF:
          f << "none,";
          break;
        default:
          if ((num.m_fieldType>>16)==0x7FFF)
            f << "#fieldType=" << num.m_fieldType -0x7FFFFFFF-1  << ",";
          else if ((num.m_fieldType>>16)==0xFFFF)
            f << "#fieldType=X" << std::hex << num.m_fieldType << std::dec << ",";
          else
            f << "#fieldType=" << num.m_fieldType << ",";
          break;
        }
        break;
      case 8: // always 0 ?
        val = (long) input->readULong(4);
        f << "g8=" << val << ",";
        break;
      case 11: // find 0-2: if numbering g11=0 means add default data, if not num data?
        val = (long) input->readULong(4);
        if (val==0) {
          f << "auto,";
          if (num.m_suffix.length())
            f << "###";
          else
            num.m_suffix += char(num.m_refId);
        } else
          f << "numVar?=" << val << ",";
        break;
      default:
        break;
      }

      if (f.str().length()) {
        rsrcAscii().addPos(entry.begin()-12);
        rsrcAscii().addNote(f.str().c_str());
      }
    }
    result->push_back(num);
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
  NSParserInternal::Zone &zone = m_state->m_zones[zoneId];
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  int sz = (int) input->readULong(2);
  if (sz+2 != entry.length() || sz%2) {
    MWAW_DEBUG_MSG(("NSParser::readNumberingReset: entry size seems odd\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(NumberingReset)[" << zoneId << "]:";
  size_t numElt = size_t(sz/2);
  zone.m_numberingResetList.resize(numElt, 0);
  for (size_t i = 0; i < numElt; i++) {
    int val = int(input->readULong(2));
    zone.m_numberingResetList[i] = val;
    if (val) f << "reset" << int(i) << "=" << val << ",";
  }
  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read mark zone ( ie the reference structure )
////////////////////////////////////////////////////////////
bool NSParser::readReference(NSStruct::RecursifData const &data)
{
  if (!data.m_info || data.m_info->m_zoneType < 0 || data.m_info->m_zoneType >= 3) {
    MWAW_DEBUG_MSG(("NSParser::readReference: find unexpected zoneType\n"));
    return false;
  }

  if (!data.m_childList.size())
    return true;

  if (data.m_childList.size() > 1) {
    MWAW_DEBUG_MSG(("NSParser::readReference: level 0 node contains more than 1 node\n"));
  }
  if (data.m_childList[0].isLeaf()) {
    MWAW_DEBUG_MSG(("NSParser::readReference: level 1 node is a leaf\n"));
    return false;
  }
  NSStruct::RecursifData const &mainData = *data.m_childList[0].m_data;
  size_t numData = mainData.m_childList.size();
  NSParserInternal::Zone &zone = m_state->m_zones[(int) data.m_info->m_zoneType];
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugStream f, f2;

  size_t n = 0;
  bool pbFound = false;
  while (n < numData) {
    if (n+1 >= numData) {
      MWAW_DEBUG_MSG(("NSParser::readReference: find an odd number of data\n"));
      break;
    }

    // ----- First the position -----
    NSStruct::RecursifData::Node const &nd=mainData.m_childList[n++];
    if (nd.isLeaf() || nd.m_type != 0x7FFFFFFF) {
      if (!pbFound) {
        MWAW_DEBUG_MSG(("NSParser::readReference: oops find bad type for the filePos node\n"));
        pbFound = true;
      }
      continue;
    }
    NSParserInternal::Reference ref;
    NSStruct::RecursifData const &dt=*nd.m_data;
    if (dt.m_childList.size() != 1 || !dt.m_childList[0].isLeaf()) {
      if (!pbFound) {
        MWAW_DEBUG_MSG(("NSParser::readReference: the filePos node contain unexpected data\n"));
        pbFound = true;
      }
      zone.m_referenceList.push_back(ref);
      continue;
    }
    MWAWEntry entry = dt.m_childList[0].m_entry;
    if (entry.length() < 8) {
      if (!pbFound) {
        MWAW_DEBUG_MSG(("NSParser::readReference: the filePos size seem bad\n"));
        pbFound = true;
      }
      zone.m_referenceList.push_back(ref);
      rsrcAscii().addPos(entry.begin()-12);
      rsrcAscii().addNote("###");
      continue;
    }

    // the file position in the text part
    long pos = entry.begin();
    input->seek(pos, WPX_SEEK_SET);
    f.str("");
    f << "Position:";
    ref.m_textPosition.setBegin((long)input->readULong(4));
    ref.m_textPosition.setEnd((long)input->readULong(4));
    f << "filePos=" << std::hex
      << ref.m_textPosition.begin() << "<->" << ref.m_textPosition.end() << std::dec << ",";
    rsrcAscii().addPos(pos-12);
    rsrcAscii().addNote(f.str().c_str());

    // ----- Second the data node -----
    NSStruct::RecursifData::Node const &nd1=mainData.m_childList[n++];

    if (nd1.isLeaf() || nd1.m_type == 0x7FFFFFFF) {
      if (!pbFound) {
        MWAW_DEBUG_MSG(("NSParser::readReference: the date node contain unexpected data\n"));
        pbFound = true;
      }
      zone.m_referenceList.push_back(ref);
      n--;
      continue;
    }

    f.str("");
    switch (nd1.m_type) {
    case 1:
      break; // type=1:id, type=100:text
    case 2:
      f << "unknown,";
      break; // type=1:id, type=-4:?, type=220: ?, type=300: list of chain?
    default:
      f << "#type=" << nd1.m_type << ",";
      break;
    }
    if (f.str().length()) {
      rsrcAscii().addPos(nd1.m_entry.begin()-16);
      rsrcAscii().addNote(f.str().c_str());
    }
    long val;
    NSStruct::RecursifData const &dt1=*nd1.m_data;
    for (size_t c=0; c < dt1.m_childList.size(); c++) {
      if (!dt1.m_childList[c].isLeaf()) {
        if (!pbFound) {
          MWAW_DEBUG_MSG(("NSParser::readReference: find some level 2 data array nodes\n"));
          pbFound = true;
        }
        continue;
      }
      NSStruct::RecursifData::Node const &childNode = dt1.m_childList[c];
      entry = childNode.m_entry;
      pos = entry.begin();
      input->seek(pos, WPX_SEEK_SET);
      f.str("");
      switch(childNode.m_type) {
      case 1:
        f << "II:";
        if (entry.length() < 4) {
          f << "###";
          break;
        }
        ref.m_id = (int) input->readLong(4); // some kind of id ?
        f << "id?=" << ref.m_id << ",";
        break;
      case 0x7ffffffc: // find one time with 0
        f << "III:";
        if (entry.length() < 4) {
          f << "###";
          break;
        }
        val = input->readLong(4);
        f << "unkn=" << val << ",";
        break;
      case 100: {
        f << "Text:";
        if (entry.length() < 1) {
          f << "###";
          break;
        }
        int mSz = (int) input->readULong(1);
        if (mSz+1 > entry.length()) {
          f << "###";
          break;
        }
        std::string mark("");
        for (int i = 0; i < mSz; i++)
          mark+=(char) input->readULong(1);
        ref.m_text = mark;
        f << mark;
        break;
      }
      case 220: {
        if (entry.length()==0) break;
        if (entry.length()%12) {
          MWAW_DEBUG_MSG(("NSParser::readReference: unexpected length for type 220\n"));
          f << "###sz";
          break;
        }
        int N=int(entry.length()/12);
        for (int j = 0; j < N ; j++) {
          long actPos = input->tell();
          f2.str("");
          f2 << "Reference2[" << j << "]:type=220,";
          val = long(input->readULong(4)); // always 0?
          if (val) f << "unkn=" << val << ",";
          // unknown, maybe some dim...
          f2 << "dim?=" << float(input->readLong(4))/65536.f << "<->";
          f2 << float(input->readLong(4))/65536.f << ",";
          rsrcAscii().addPos(actPos);
          rsrcAscii().addNote(f2.str().c_str());
          input->seek(actPos+12, WPX_SEEK_SET);
        }
        break;
      }
      case 300: // find with size 0x28
        break;
      default:
        f << "#type";
        break;
      }
      if (f.str().length()) {
        rsrcAscii().addPos(pos-12);
        rsrcAscii().addNote(f.str().c_str());
      }
    }
    zone.m_referenceList.push_back(ref);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read SGP1 zone: a unknown zone (a type, an id/anchor type? and a bdbox )
////////////////////////////////////////////////////////////
bool NSParser::readSGP1(NSStruct::RecursifData const &data)
{
  if (!data.m_info || data.m_info->m_zoneType < 0 || data.m_info->m_zoneType >= 3) {
    MWAW_DEBUG_MSG(("NSParser::readSGP1: find unexpected zoneType\n"));
    return false;
  }

  if (!data.m_childList.size())
    return true;

  if (data.m_childList.size() > 1) {
    MWAW_DEBUG_MSG(("NSParser::readSGP1: level 0 node contains more than 1 node\n"));
  }
  if (data.m_childList[0].isLeaf()) {
    MWAW_DEBUG_MSG(("NSParser::readSGP1: level 1 node is a leaf\n"));
    return false;
  }
  NSStruct::RecursifData const &mainData = *data.m_childList[0].m_data;
  size_t numData = mainData.m_childList.size();
  //  NSParserInternal::Zone &zone = m_state->m_zones[(int) data.m_info->m_zoneType];
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugStream f;

  long val;
  for (size_t n = 0 ; n < numData; n++) {
    if (mainData.m_childList[n].isLeaf()) {
      MWAW_DEBUG_MSG(("NSParser::readSGP1: oops some level 2 node are leaf\n"));
      continue;
    }
    NSStruct::RecursifData const &dt=*mainData.m_childList[n].m_data;
    for (size_t i = 0; i < dt.m_childList.size(); i++) {
      NSStruct::RecursifData::Node const &child= dt.m_childList[i];
      if (!child.isLeaf()) {
        MWAW_DEBUG_MSG(("NSParser::readSGP1: find an odd level 3 leaf\n"));
        continue;
      }

      long pos = child.m_entry.begin();
      input->seek(pos, WPX_SEEK_SET);
      f.str("");
      switch(child.m_type) {
      case 110: {
        if (child.m_entry.length()==0) break;
        if (child.m_entry.length()%4) {
          MWAW_DEBUG_MSG(("NSParser::readSGP1: unexpected length for type 110\n"));
          f << "###sz";
          break;
        }
        // find some increasing sequence here
        int N=int(child.m_entry.length()/4);
        f << "unkn=[";
        for (int j = 0; j < N; j++) {
          val = input->readLong(4);
          f << val << ",";
        }
        f << "]";
        break;
      }
      case 120:
      case 200: { /* checkme: always find mSz=0 here */
        if (child.m_entry.length()==0) {
          MWAW_DEBUG_MSG(("NSParser::readSGP1: unexpected length for type %d\n", child.m_type));
          f << "###sz";
          break;
        }
        int mSz = (int) input->readULong(1);
        if (mSz+1 > child.m_entry.length()) {
          MWAW_DEBUG_MSG(("NSParser::readSGP1: the %d text seems to short\n", child.m_type));
          f << "###sz";
          break;
        }
        std::string text("");
        for (int c = 0; c < mSz; c++)
          text+=(char) input->readULong(1);
        f << "text=" << text << ",";
        break;
      }
      case 100: {
        if (child.m_entry.length()!=0x24) {
          MWAW_DEBUG_MSG(("NSParser::readSGP1: unexpected length for type 100\n"));
          f << "###sz";
          break;
        }
        // find f1=1, f3=[0|1], f5=0x28, other always 0...
        for (int j = 0; j < 18; j++) {
          val = input->readLong(2);
          if (val) f << "f" << j << "=" << val << ",";
        }
        break;
      }
      case 300: {
        if (child.m_entry.length()!=0x5c) {
          MWAW_DEBUG_MSG(("NSParser::readSGP1: unexpected length for type 300\n"));
          f << "###sz";
          break;
        }
        val = (long) input->readULong(2); // always 0x8000 ?
        if (val != 0x8000) f << "f0=" << std::hex << val << std::dec << ",";
        for (int j = 0; j < 2; j++) { // find [0,-2]|0
          val = input->readLong(2);
          if (val) f << "f" << j+1 << "=" << val << ",";
        }
        val = (long) input->readULong(2); // find 0 and 1 time 0x7101
        if (val) f << "f3=" << std::hex << val << std::dec << ",";
        int mSz = (int) input->readULong(1);
        if (mSz >= 32) {
          f << "###textSz=" << mSz << ",";
          mSz = 0;
        }
        std::string text("");
        for (int j = 0; j < mSz; j++)
          text += (char) input->readULong(1);
        if (text.length()) f << "\"" << text << "\",";
        // checkme: where does the data begins again
        input->seek(child.m_entry.begin()+40, WPX_SEEK_SET);
        for (int j = 0; j < 17; j++) { // find always 0 here
          val = input->readLong(2);
          if (val) f << "g" << j << "=" << val << ",";
        }
        val = (long) input->readULong(2); // 1|b
        f << "unkn=" << val << ",";
        for (int j = 0; j < 8; j++) { // find always 0 here
          val = input->readLong(2);
          if (val) f << "h" << j << "=" << val << ",";
        }
        break;
      }
      default:
        f << "type=" << child.m_type << ",";
        break;
      }
      rsrcAscii().addPos(pos-12);
      rsrcAscii().addNote(f.str().c_str());
    }
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
    f << "Entries(PrintInfo)[#" << entry.id() << "]:" << info;
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
    MWAW_DEBUG_MSG(("NSParser::readPrintInfo: file is too short\n"));
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the PGLY zone ( page limit )
////////////////////////////////////////////////////////////
bool NSParser::readPageLimit(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() < 0xa2) {
    MWAW_DEBUG_MSG(("NSParser::readPageLimit: the entry is bad\n"));
    return false;
  }
  if (entry.id() != 128) {
    MWAW_DEBUG_MSG(("NSParser::readPageLimit: the entry id %d is odd\n", entry.id()));
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  if (entry.id() != 128)
    f << "Entries(Page)[#" << entry.id() << "]:";
  else
    f << "Entries(Page):";

  long val = input->readLong(2); // always 0x88 ?
  if (val != 0x88) f << "f0=" << val << ",";
  val = input->readLong(2); // always 0x901 or two flags ?
  if (val != 0x901) f << "f1=" << val << ",";
  val = input->readLong(2); // always 0x0
  if (val) f << "f2=" << val << ",";

  Box2i boxes[3];
  for (int i = 0; i < 3; i++) {
    int dim[4];
    for (int d = 0; d < 4; d++)
      dim[d] = (int) input->readLong(2);
    boxes[i] = Box2i(Vec2i(dim[1],dim[0]),Vec2i(dim[3],dim[2]));
  }
  f << "page=" << boxes[0] << ",";
  f << "page[text]=" << boxes[1] << ",";
  f << "page[columns]=" << boxes[2] << ",";
  Vec2i pageDim = boxes[0].size();
  Vec2i LT=boxes[1][0]-boxes[0][0];
  Vec2i RB=boxes[0][1]-boxes[1][1];
  bool dimOk=pageDim[0] > 0 && pageDim[1] > 0 &&
             LT[0] >= 0 && LT[1] >= 0 && RB[0] >= 0 && RB[1] >= 0;
  if (!dimOk && m_state->m_isTextDocument) {
    // can be ok in a glossary: in this case, all values can be 0
    MWAW_DEBUG_MSG(("NSParser::readPageLimit: the page margins seems odd\n"));
    f << "###dim,";
  }
  // all zero expected some time g0=2
  for (int i = 0; i < 3; i++) {
    val = input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  int numColumns = (int) input->readLong(2)+1;
  int colSeps = (int) input->readLong(2);
  if (!dimOk) ;
  else if (numColumns <= 0 || numColumns > 8 || colSeps < 0 || colSeps*numColumns >= pageDim[0]) {
    MWAW_DEBUG_MSG(("NSParser::readPageLimit: the columns definition seems odd\n"));
    f << "###";
  } else {
    m_state->m_numColumns = numColumns;
    m_state->m_columnSep = float(colSeps)/72.f;
  }
  if (numColumns != 1) f << "nCols=" << numColumns << ",";
  if (colSeps != 36) f << "colSeps=" << colSeps << "pt,";
  static int const expectedValues[]= { 0, 3, 1};
  // find also, g3=0x10, g4=g5=0 (in v3 file)
  for (int i = 0; i < 3; i++) {
    val = input->readLong(2);
    if (int(val) != expectedValues[i])
      f << "g" << i+3 << "=" << val << ",";
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
  f << "Page[A]:";
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
  int dim[4];
  for (int d = 0; d < 4; d++)
    dim[d] = (int) input->readLong(2);
  Box2i realPage = Box2i(Vec2i(dim[1],dim[0]), Vec2i(dim[3],dim[2]));
  if (dimOk && realPage.size()[0] >= pageDim[0] && realPage.size()[1] >= pageDim[1]) {
    LT -= realPage[0];
    RB += (realPage[1]-boxes[0][1]);
    pageDim = realPage.size();
  } else if (dimOk) {
    MWAW_DEBUG_MSG(("NSParser::readPageLimit: realPage is smaller than page\n"));
    f << "###";
    dimOk = false;
  }
  f << "page[complete]=" << realPage << ",";
  val = input->readLong(1);
  bool portrait = true;
  switch(val) {
  case 0:
    break;
  case 1:
    portrait = false;
    f << "landscape,";
    break;
  default:
    f << "#pageOrientation=" << val << ",";
    break;
  }
  for (int i = 0; i < 5; i++) {
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

  if (!dimOk) return true;
  m_pageSpan.setMarginTop(double(LT[1])/72.0);
  m_pageSpan.setMarginBottom(double(RB[1])/72.0);
  m_pageSpan.setMarginLeft(double(LT[0])/72.0);
  m_pageSpan.setMarginRight(double(RB[0])/72.0);
  m_pageSpan.setFormLength(double(pageDim[1])/72.);
  m_pageSpan.setFormWidth(double(pageDim[0])/72.);
  if (!portrait)
    m_pageSpan.setFormOrientation(MWAWPageSpan::LANDSCAPE);

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

  /** find only 0...
      except one time f0=1[id?], f4=1900 f6=206c [2pos?]
   */
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
// read the CNTR zone ( a list of related to VRS ?  )
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
  f << "Entries(VariabCntr)[" << zoneId << "]:N=" << numElt;
  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());
  for (int i = 0; i < numElt; i++) {
    // unkn[16] * 3, 0[16], refId, 0[16],  unkn[16]
    pos = input->tell();
    f.str("");
    f << "VariabCntr" << i << ":";
    rsrcAscii().addPos(pos);
    rsrcAscii().addNote(f.str().c_str());
    input->seek(pos+12, WPX_SEEK_SET);
  }

  f.str("");
  f << "VariabCntr(II)";
  rsrcAscii().addPos(input->tell());
  rsrcAscii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
//! read the ABBR resource: a list of abreviation ?
////////////////////////////////////////////////////////////
bool NSParser::readABBR(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()%32) {
    MWAW_DEBUG_MSG(("NSParser::readABBR: the entry is bad\n"));
    return false;
  }
  if (entry.id() != 1003) {
    MWAW_DEBUG_MSG(("NSParser::readABBR: the entry id %d is odd\n", entry.id()));
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  int numElt = int(entry.length()/32);
  for (int n = 0; n < numElt; n++) {
    pos = input->tell();
    f.str("");
    if (n==0) {
      if (entry.id() != 1003)
        f << "Entries(ABBR)[#" << entry.id() << "]";
      else
        f << "Entries(ABBR)";
    } else
      f << "ABBR";
    f << "[" << n << "]:";
    long id = input->readLong(4);
    if (id != long(n))
      f << "#id=" << id << ",";
    int sSz = int(input->readULong(1));
    if (sSz > 27) {
      MWAW_DEBUG_MSG(("NSParser::readABBR: the string size is bad\n"));
      f << "##";
    } else {
      std::string str("");
      for (int i = 0; i < sSz; i++)
        str += char(input->readULong(1));
      f << "\"" << str << "\",";
    }
    rsrcAscii().addPos(n==0 ? pos-4 : pos);
    rsrcAscii().addNote(f.str().c_str());
    input->seek(pos+32, WPX_SEEK_SET);
  }

  return true;
}

////////////////////////////////////////////////////////////
//! read the FTA2 resource: a list of  ?
////////////////////////////////////////////////////////////
bool NSParser::readFTA2(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()%12) {
    MWAW_DEBUG_MSG(("NSParser::readFTA2: the entry is bad\n"));
    return false;
  }
  if (entry.id() != 1003) {
    MWAW_DEBUG_MSG(("NSParser::readFTA2: the entry id %d is odd\n", entry.id()));
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  int numElt = int(entry.length()/12);
  int val;
  for (int n = 0; n < numElt; n++) {
    pos = input->tell();
    f.str("");
    if (n==0) {
      if (entry.id() != 1003)
        f << "Entries(FTA2)[#" << entry.id() << "]";
      else
        f << "Entries(FTA2)";
    } else
      f << "FTA2";
    f << "[" << n << "]:";
    val = int(input->readLong(1)); // 0|ff
    if (val==-1) f << "f0,";
    else if (val) f << "f0=" << val << ",";
    val = int(input->readLong(1)); // 0|6|7|f ( maybe f1=0 if f0=ff )
    if (val)
      f << "f1=" << std::hex << val << std::dec << ",";
    // always 0 excepted f3=0|ff9f|ffc1
    for (int i = 0; i < 5; i++) {
      val = int(input->readLong(2));
      if (val) f << "f" << i+2 << "=" << val << ",";
    }
    rsrcAscii().addPos(n==0 ? pos-4 : pos);
    rsrcAscii().addNote(f.str().c_str());
    input->seek(pos+12, WPX_SEEK_SET);
  }

  return true;
}

////////////////////////////////////////////////////////////
//! read the FnSc resource: a list of  ?
////////////////////////////////////////////////////////////
bool NSParser::readFnSc(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() != 0x42) {
    MWAW_DEBUG_MSG(("NSParser::readFnSc: the entry is bad\n"));
    return false;
  }
  if (entry.id() != 1003) {
    MWAW_DEBUG_MSG(("NSParser::readFnSc: the entry id %d is odd\n", entry.id()));
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  if (entry.id() != 1003)
    f << "Entries(FnSc)[#" << entry.id() << "]:";
  else
    f << "Entries(FnSc):";
  long val;
  val = input->readLong(2); // find 0|3|10|14
  if (val) f << "f0=" << val << ",";
  val = (long) input->readULong(2); // find 0|4000|4034
  if (val) f << "f1=" << std::hex << val << std::dec << ",";
  for (int i = 0; i < 31; i++) { // always 0?
    val = input->readLong(2);
    if (val) f << "f" << i+2 << "=" << val << ",";
  }
  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
//! read the INFO resource: a unknown zone
////////////////////////////////////////////////////////////
bool NSParser::readINFO(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() < 0x23a) {
    MWAW_DEBUG_MSG(("NSParser::readINFO: the entry is bad\n"));
    return false;
  }
  if (entry.id() != 1003) {
    MWAW_DEBUG_MSG(("NSParser::readINFO: the entry id %d is odd\n", entry.id()));
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  if (entry.id() != 1003)
    f << "Entries(INFO)[#" << entry.id() << "]:";
  else
    f << "Entries(INFO):";
  long val = input->readLong(2);
  if (val != 1) f << "f0=" << val << ",";
  int select[2]; // checkme
  for (int i = 0; i < 2; i++)
    select[i] = (int) input->readLong(4);
  if (select[0] || select[1]) {
    f << "select=" << select[0];
    if (select[1] != select[0]) f << "x" << select[1];
    f << ",";
  }
  val =  input->readLong(2);
  if (val) f << "f1=" << val << ",";
  // a dim or 2 position ?
  int dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = (int) input->readLong(2);
  if (dim[0] || dim[1] || dim[2] || dim[3])
    f << "dim|pos?=" << dim[0] << "x" << dim[1]
      << "<->" << dim[2] << "x" << dim[3] << ",";
  int sz[2];
  for (int i = 0; i < 2; i++)
    sz[i] = (int) input->readLong(2);
  if (sz[0] || sz[1])
    f << "sz=" << sz[0] << "x" << sz[1] << ",";
  for (int i = 0; i < 2; i++) { // always 1|1
    val = input->readLong(1);
    if (val) f << "fl" << i << "=" << val << ",";
  }
  for (int i = 0; i < 8; i++) { // always 0
    val = input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());

  pos = input->tell();
  f.str("");
  f << "INFOA0:";
  for (int i = 0; i < 4; i++) { // find 0-0-0-0 or 1-1-1-1 or 1-1-1-4
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  // 4 times the same val: 0, aa55, ffff
  int unkn = (int) input->readULong(2);
  if (unkn) f << "unknA0=" << std::hex << unkn << std::dec << ",";
  for (int i = 1; i < 4; i++) {
    val = (long) input->readULong(2);
    if (val != unkn)
      f << "unknA" << i << "=" << std::hex << val << std::dec << ",";
  }
  // 4 times the same val: 0, ffff
  unkn = (int) input->readULong(2);
  if (unkn) f << "unknB0=" << std::hex << unkn << std::dec << ",";
  for (int i = 1; i < 4; i++) {
    val = (long) input->readULong(2);
    if (val != unkn)
      f << "unknB" << i << "=" << std::hex << val << std::dec << ",";
  }
  rsrcAscii().addDelimiter(input->tell(),'|');
  rsrcAscii().addPos(pos);
  rsrcAscii().addNote(f.str().c_str());

  input->seek(pos+0x2c, WPX_SEEK_SET);
  pos = input->tell();
  f.str("");
  f << "INFOB:";
  rsrcAscii().addPos(pos);
  rsrcAscii().addNote(f.str().c_str());

  input->seek(pos+0x112, WPX_SEEK_SET);
  pos = input->tell();
  f.str("");
  f << "INFOA1:";
  rsrcAscii().addPos(pos);
  rsrcAscii().addNote(f.str().c_str());

  input->seek(entry.begin() + 0x194, WPX_SEEK_SET);
  pos = input->tell();
  f.str("");
  f << "INFOC:";

  NSStruct::FootnoteInfo &ftInfo = m_state->m_footnoteInfo;
  ftInfo.m_flags = (int)input->readULong(2);
  ftInfo.m_unknown = (int)input->readLong(2);
  ftInfo.m_distToDocument = (int)input->readLong(2);
  ftInfo.m_distSeparator = (int)input->readLong(2);
  ftInfo.m_separatorLength = (int)input->readLong(2);
  f << "footnote=[" << ftInfo << "],";
  rsrcAscii().addDelimiter(input->tell(), '|');
  rsrcAscii().addPos(pos);
  rsrcAscii().addNote(f.str().c_str());

  return true;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
