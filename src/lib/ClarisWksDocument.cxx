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

#include <string.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"
#include "MWAWHeader.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWListener.hxx"
#include "MWAWParser.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSection.hxx"

#include "ClarisWksDatabase.hxx"
#include "ClarisWksGraph.hxx"
#include "ClarisWksPresentation.hxx"
#include "ClarisWksSpreadsheet.hxx"
#include "ClarisWksStyleManager.hxx"
#include "ClarisWksStruct.hxx"
#include "ClarisWksTable.hxx"
#include "ClarisWksText.hxx"

#include "ClarisWksDocument.hxx"

/** Internal: the structures of a ClarisWksDocument */
namespace ClarisWksDocumentInternal
{
////////////////////////////////////////
//! Internal: the state of a ClarisWksDocument
struct State {
  //! constructor
  State() : m_pageSpanSet(false), m_numPages(0), m_pages(0,0), m_pagesVerified(false), m_headerId(0), m_footerId(0),  m_headerHeight(0), m_footerHeight(0),
    m_columns(1), m_columnsWidth(), m_columnsSep(),
    m_zonesMap(), m_zonesTypeMap(), m_rootZonesList(), m_hFZonesList()
  {
  }
  //! returns a zone's type
  ClarisWksStruct::DSET::Position getZoneType(int id) const
  {
    if (m_zonesTypeMap.find(id)==m_zonesTypeMap.end())
      return ClarisWksStruct::DSET::P_Unknown;
    return m_zonesTypeMap.find(id)->second;
  }
  //! a flag to know if pageSpan is filled
  bool m_pageSpanSet;

  //! the number of pages (computed)
  int m_numPages;
  //! the number of pages find in the header ( if known )
  Vec2i m_pages;
  //! true if the number pages of pages has been verified
  bool m_pagesVerified;
  int m_headerId /** the header zone if known */,
      m_footerId /** the footer zone if known */;
  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;

  /** the number of columns */
  int m_columns;
  /** the columns witdh in Points*/
  std::vector<int> m_columnsWidth;
  /** the columns separator in Points*/
  std::vector<int> m_columnsSep;

  /** the map of zone*/
  std::map<int, shared_ptr<ClarisWksStruct::DSET> > m_zonesMap;
  /** the type of each zone*/
  std::map<int, ClarisWksStruct::DSET::Position> m_zonesTypeMap;
  /** the list of root zones */
  std::vector<int> m_rootZonesList;
  /** the list of header/footer group */
  std::vector<int> m_hFZonesList;
};

////////////////////////////////////////
//! Internal: the subdocument of a ClarisWksDocument
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(ClarisWksDocument &doc, MWAWInputStreamPtr input, int zoneId) :
    MWAWSubDocument(0, input, MWAWEntry()), m_document(doc), m_id(zoneId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const
  {
    if (MWAWSubDocument::operator!=(doc)) return true;
    SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
    if (!sDoc) return true;
    if (&m_document != &sDoc->m_document) return true;
    if (m_id != sDoc->m_id) return true;
    return false;
  }

  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const
  {
    return !operator!=(doc);
  }
  //! the parser function
  void parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  //! the document manager
  ClarisWksDocument &m_document;
  //! the subdocument id
  int m_id;
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("ClarisWksDocumentInternal::SubDocument::parse: no listener\n"));
    return;
  }
  if (m_id == -1) { // a number used to send linked frame
    listener->insertChar(' ');
    return;
  }
  if (m_id == 0) {
    MWAW_DEBUG_MSG(("ClarisWksDocumentInternal::SubDocument::parse: unknown zone\n"));
    return;
  }

  m_document.sendZone(m_id, listener);
}
}

ClarisWksDocument::ClarisWksDocument(MWAWParser &parser) :
  m_state(new ClarisWksDocumentInternal::State), m_parserState(parser.getParserState()),
  m_parser(&parser), m_styleManager(),
  m_databaseParser(), m_graphParser(), m_presentationParser(), m_spreadsheetParser(), m_tableParser(), m_textParser(),
  m_newPage(0), m_sendFootnote(0)
{
  m_styleManager.reset(new ClarisWksStyleManager(*this));

  m_databaseParser.reset(new ClarisWksDatabase(*this));
  m_graphParser.reset(new ClarisWksGraph(*this));
  m_presentationParser.reset(new ClarisWksPresentation(*this));
  m_spreadsheetParser.reset(new ClarisWksSpreadsheet(*this));
  m_tableParser.reset(new ClarisWksTable(*this));
  m_textParser.reset(new ClarisWksText(*this));
}

ClarisWksDocument::~ClarisWksDocument()
{
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
Vec2i ClarisWksDocument::getDocumentPages()
{
  if (!m_state->m_pagesVerified && m_parserState->m_kind==MWAWDocument::MWAW_K_DRAW) {
    int numHPages=m_state->m_pages[0];
    float textWidth=72.0f*(float)m_parser->getPageWidth();
    std::map<int, shared_ptr<ClarisWksStruct::DSET> >::iterator iter;
    for (iter=m_state->m_zonesMap.begin() ; iter != m_state->m_zonesMap.end() ; ++iter) {
      shared_ptr<ClarisWksStruct::DSET> group = iter->second;
      if (!group || group->m_position != ClarisWksStruct::DSET::P_Main)
        continue;
      int page=int(float(group->getUnionChildBox()[1][0])/textWidth-0.2f)+1;
      if (page > numHPages && page < numHPages+10) {
        MWAW_DEBUG_MSG(("ClarisWksGraph::computePositions: increase num page accross to %d\n", page));
        numHPages = page;
      }
    }
    m_state->m_pages[0]=numHPages;
  }
  else if (m_state->m_pages[0]<=0)
    m_state->m_pages[0]=1;
  m_state->m_pagesVerified=true;
  return m_state->m_pages;
}

int ClarisWksDocument::numPages() const
{
  if (m_state->m_numPages>0) return m_state->m_numPages;
  int numPage = m_textParser->numPages();
  if (m_databaseParser->numPages() > numPage)
    numPage = m_databaseParser->numPages();
  if (m_presentationParser->numPages() > numPage)
    numPage = m_presentationParser->numPages();
  if (m_graphParser->numPages() > numPage)
    numPage = m_graphParser->numPages();
  if (m_spreadsheetParser->numPages() > numPage)
    numPage = m_spreadsheetParser->numPages();
  if (m_tableParser->numPages() > numPage)
    numPage = m_tableParser->numPages();
  m_state->m_numPages=numPage;
  return numPage;
}

void ClarisWksDocument::updatePageSpanList(std::vector<MWAWPageSpan> &pageList)
{
  MWAWPageSpan ps=getFinalPageSpan();
  if (m_textParser->updatePageSpanList(ps, pageList))
    return;
  pageList.resize(0);
  for (int i = 0; i < 2; i++) {
    int zoneId = i==0 ? m_state->m_headerId : m_state->m_footerId;
    if (zoneId == 0)
      continue;
    MWAWHeaderFooter hF((i==0) ? MWAWHeaderFooter::HEADER : MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    hF.m_subDocument.reset(new ClarisWksDocumentInternal::SubDocument(*this, m_parserState->m_input, zoneId));
    ps.setHeaderFooter(hF);
  }
  ps.setPageSpan(numPages());
  pageList=std::vector<MWAWPageSpan>(1,ps);
}

void ClarisWksDocument::updatePageSpanList(std::vector<MWAWPageSpan> &pageList, MWAWPageSpan &master)
{
  MWAWPageSpan ps=getFinalPageSpan();
  ps.setMasterPageName("Master");
  master=ps;
  if (m_presentationParser->updatePageSpanList(ps, pageList))
    return;
  ps.setPageSpan(numPages());
  pageList=std::vector<MWAWPageSpan>(1,ps);
}

MWAWPageSpan ClarisWksDocument::getFinalPageSpan() const
{
  MWAWPageSpan ps(m_parserState->m_pageSpan);
  // decrease right | bottom
  if (ps.getMarginRight()>50./72.)
    ps.setMarginRight(ps.getMarginRight()-50./72.);
  else
    ps.setMarginRight(0);
  if (ps.getMarginBottom()>50./72.)
    ps.setMarginBottom(ps.getMarginBottom()-50./72.);
  else
    ps.setMarginBottom(0);
  return ps;
}

double ClarisWksDocument::getTextHeight() const
{
  return m_parserState->m_pageSpan.getPageLength()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0;
}

Vec2f ClarisWksDocument::getPageLeftTop() const
{
  return Vec2f(float(m_parserState->m_pageSpan.getMarginLeft()),
               float(m_parserState->m_pageSpan.getMarginTop()+m_state->m_headerHeight/72.0));
}

////////////////////////////////////////////////////////////
// interface via callback
////////////////////////////////////////////////////////////
void ClarisWksDocument::newPage(int page, bool softBreak)
{
  if (!m_newPage) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::newPage: can not find newPage callback\n"));
    return;
  }
  (m_parser->*m_newPage)(page, softBreak);
}

MWAWSection ClarisWksDocument::getMainSection() const
{
  MWAWSection sec;
  if (m_state->m_columns <= 1)
    return sec;
  size_t numCols = size_t(m_state->m_columns);
  bool hasSep = m_state->m_columnsSep.size()+1==numCols;
  bool hasWidth = m_state->m_columnsWidth.size()==numCols;
  double width=0.0;
  if (!hasWidth) {
    double totalWidth = 72.0*m_parserState->m_pageSpan.getPageWidth();
    for (size_t c=0; c+1 < numCols; c++)
      totalWidth -= double(m_state->m_columnsSep[c]);
    width = totalWidth/double(numCols);
  }
  sec.m_columns.resize(numCols);
  for (size_t c=0; c < numCols; c++) {
    sec.m_columns[c].m_width =
      hasWidth ? double(m_state->m_columnsWidth[c]) : width;
    sec.m_columns[c].m_widthUnit = librevenge::RVNG_POINT;
    if (!hasSep)
      continue;
    if (c)
      sec.m_columns[c].m_margins[libmwaw::Left]=
        double(m_state->m_columnsSep[c-1])/72./2.;
    if (c+1!=numCols)
      sec.m_columns[c].m_margins[libmwaw::Right]=
        double(m_state->m_columnsSep[c])/72./2.;
  }
  return sec;
}

void ClarisWksDocument::sendFootnote(int zoneId)
{
  if (!m_sendFootnote) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::sendFootnote: can not find sendFootnote callback\n"));
    return;
  }
  (m_parser->*m_sendFootnote)(zoneId);
}

void ClarisWksDocument::checkOrdering(std::vector<int16_t> &vec16, std::vector<int32_t> &vec32) const
{
  if (!m_parserState || m_parserState->m_version < 4) return;
  int numSmallEndian = 0, numBigEndian = 0;
  unsigned long val;
  for (size_t i = 0; i < vec16.size(); i++) {
    val = (unsigned long)(uint16_t) vec16[i];
    if ((val & 0xFF00) && !(val & 0xFF))
      numSmallEndian++;
    else if ((val&0xFF) && !(val&0xFF00))
      numBigEndian++;
  }
  for (size_t i = 0; i < vec32.size(); i++) {
    val = (unsigned long)(uint32_t) vec32[i];
    if ((val & 0xFFFF0000) && !(val & 0xFFFF))
      numSmallEndian++;
    else if ((val&0xFFFF) && !(val&0xFFFF0000))
      numBigEndian++;
  }
  if (numBigEndian >= numSmallEndian)
    return;
  for (size_t i = 0; i < vec16.size(); i++) {
    val = (unsigned long)(uint16_t) vec16[i];
    vec16[i] = (int16_t)((val>>8) & ((val&0xFF)<<8));
  }
  for (size_t i = 0; i < vec32.size(); i++) {
    val = (unsigned long)(uint32_t) vec32[i];
    vec32[i] = (int32_t)((val>>16) & ((val&0xFFFF)<<16));
  }
}

////////////////////////////////////////////////////////////
// zone function
////////////////////////////////////////////////////////////
std::vector<int> const &ClarisWksDocument::getMainZonesList() const
{
  return m_state->m_rootZonesList;
}

shared_ptr<ClarisWksStruct::DSET> ClarisWksDocument::getZone(int zId) const
{
  std::map<int, shared_ptr<ClarisWksStruct::DSET> >::const_iterator iter = m_state->m_zonesMap.find(zId);
  if (iter != m_state->m_zonesMap.end())
    return iter->second;
  return shared_ptr<ClarisWksStruct::DSET>();
}

bool ClarisWksDocument::canSendZoneAsGraphic(int zoneId) const
{
  shared_ptr<ClarisWksStruct::DSET> zMap = getZone(zoneId);
  if (!zMap) return false;
  switch (zMap->m_fileType) {
  case 0:
    return m_graphParser->canSendGroupAsGraphic(zoneId);
  case 1:
    return m_textParser->canSendTextAsGraphic(zoneId);
  case 2:
    return m_spreadsheetParser->canSendSpreadsheetAsGraphic(zoneId);
  case 3:
    return m_databaseParser->canSendDatabaseAsGraphic(zoneId);
  case 4:
    return m_graphParser->canSendBitmapAsGraphic(zoneId);
  default:
    break;
  }
  return false;
}

bool ClarisWksDocument::sendZone(int zoneId, MWAWListenerPtr listener, MWAWPosition position)
{
  shared_ptr<ClarisWksStruct::DSET> zMap = getZone(zoneId);
  if (!zMap) return false;
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell();
  bool res = false;
  switch (zMap->m_fileType) {
  case 0:
    res = getGraphParser()->sendGroup(zoneId, listener, position);
    break;
  case 1:
    res = getTextParser()->sendZone(zoneId, listener);
    break;
  case 4:
    res = getGraphParser()->sendBitmap(zoneId, listener, position);
    break;
  case 5: {
    if (!listener) listener=m_parserState->getMainListener();
    if (listener && listener->getType()==MWAWListener::Presentation)
      res = getPresentationParser()->sendZone(zoneId);
    else {
      MWAW_DEBUG_MSG(("ClarisWksDocument::sendZone: sorry, not possible to send a presentation zone %d in a not presentation document\n", zoneId));
    }
    break;
  }
  case 6:
    res = getTableParser()->sendZone(zoneId);
    break;
  case 2:
    res = getSpreadsheetParser()->sendSpreadsheet(zoneId, listener);
    break;
  case 3:
    res = getDatabaseParser()->sendDatabase(zoneId, listener);
    break;
  default:
    MWAW_DEBUG_MSG(("ClarisWksDocument::sendZone: can not send zone: %d\n", zoneId));
    break;
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  zMap->m_parsed = true;
  return res;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool ClarisWksDocument::createZones()
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  int const vers=m_parserState->m_version;
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;

  long eof=-1;
  if (readDocHeader() && readDocInfo()) {
    long pos = input->tell();
    // time to read the styles
    if (vers > 1) {
      readEndTable(eof);
      if (eof > 0)
        input->pushLimit(eof);
      input->seek(pos, librevenge::RVNG_SEEK_SET);
    }

    pos = input->tell();
    while (!input->isEnd()) {
      if (!readZone()) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
      pos = input->tell();
    }
  }

  if (!input->isEnd()) {
    ascFile.addPos(input->tell());
    f.str("");
    f << "Entries(Loose): vers=" << vers;
    ascFile.addNote(f.str().c_str());
  }
  // look for graphic
  while (!input->isEnd()) {
    long pos = input->tell();
    int val = (int) input->readULong(2);
    if (input->isEnd()) break;
    bool ok = false;
    if (val == 0x4453) {
      if (input->readULong(2) == 0x4554) {
        ok = true;
        input->seek(-4, librevenge::RVNG_SEEK_CUR);
      }
    }
    if (!ok && (val == 0x1101 || val == 0x1102)) {
      long debPos = (val == 0x1102) ? pos-15 : pos-14;
      input->seek(debPos, librevenge::RVNG_SEEK_SET);
      if (input->readULong(2) == 0) {
        int sz = (int) input->readULong(2);
        int fSz  = (int) input->readULong(2);
        if (sz >= 0x10 && (val == 0x1102 || sz == fSz)) {
          ok = true;
          input->seek(-6, librevenge::RVNG_SEEK_CUR);
        }
      }
    }
    if (!ok) {
      input->seek(pos+1, librevenge::RVNG_SEEK_SET);
      continue;
    }

    if (input->isEnd()) break;

    long prevPos = pos;
    ok = false;
    while (!input->isEnd()) {
      if (!readZone()) {
        input->seek(pos+1, librevenge::RVNG_SEEK_SET);
        break;
      }
      pos = input->tell();
      if (pos <= prevPos)
        break;
      ok = true;
    }
    if (!ok || pos <= prevPos) {
      input->seek(prevPos+1, librevenge::RVNG_SEEK_SET);
      continue;
    }
    if (input->isEnd()) break;

    ascFile.addPos(pos);
    ascFile.addNote("Entries(End)");
  }
  if (eof > 0)
    input->popLimit();
  exploreZonesGraph();
  typeMainZones();
  cleanZonesGraph();
  updateChildPositions();

#ifdef DEBUG
  // let check if all is ok
  size_t numMain=m_state->m_rootZonesList.size();
  if (1 && numMain == 1)
    return !getMainZonesList().empty();
  MWAW_DEBUG_MSG(("ClarisWksDocument::createZones: find %d main zones\n", int(numMain)));
  // we have do not have find the root note : probably a database...
  std::cerr << "--------------------------------------------------------\n";
  std::cerr << "List of potential main zones : ";
  for (size_t i = 0; i < numMain; i++)
    std::cerr << m_state->m_rootZonesList[i] << ",";
  std::cerr << "\n";
  std::map<int, shared_ptr<ClarisWksStruct::DSET> >::const_iterator iter = m_state->m_zonesMap.begin();
  for (; iter != m_state->m_zonesMap.end(); ++iter) {
    shared_ptr<ClarisWksStruct::DSET> zone = iter->second;
    std::cerr << *zone << "\n";
  }
  std::cerr << "--------------------------------------------------------\n";
#endif
  return !getMainZonesList().empty();
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool ClarisWksDocument::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = ClarisWksDocumentInternal::State();

  MWAWInputStreamPtr input = m_parserState->m_input;
  if (!input || !input->hasDataFork())
    return false;
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;

  int const headerSize=8;
  if (!input->checkPosition(headerSize)) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,librevenge::RVNG_SEEK_SET);
  f << "FileHeader:";
  int vers = (int) input->readLong(1);
  m_parserState->m_version = vers;
  if (vers <=0 || vers > 6) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::checkHeader: unknown version: %d\n", vers));
    return false;
  }
  f << "vers=" << vers << ",";
  f << "unk=" << std::hex << input->readULong(2) << ",";
  int val = (int) input->readLong(1);
  if (val)
    f << "unkn1=" << val << ",";
  if (input->readULong(2) != 0x424f && input->readULong(2) != 0x424f)
    return false;

  ascFile.addPos(0);
  ascFile.addNote(f.str().c_str());

  int typePos = 0;
  switch (vers) {
  case 1:
    typePos = 242;
    break;
  case 2:
  case 3:
    typePos = 248;
    break;
  case 4:
    typePos = 256;
    break;
  case 5:
    typePos = 268;
    break;
  case 6:
    typePos = 278;
    break;
  default:
    break;
  }
  input->seek(typePos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != typePos)
    return false;
  int type = (int) input->readULong(1);

  MWAWDocument::Kind kind=MWAWDocument::MWAW_K_UNKNOWN;
  switch (type) {
  case 0:
    kind=MWAWDocument::MWAW_K_DRAW;
    break;
  case 1:
    kind=MWAWDocument::MWAW_K_TEXT;
    break;
  case 2:
    kind=MWAWDocument::MWAW_K_SPREADSHEET;
    break;
  case 3:
    kind=MWAWDocument::MWAW_K_DATABASE;
    break;
  case 4:
    kind=MWAWDocument::MWAW_K_PAINT;
    break;
  case 5:
    kind=MWAWDocument::MWAW_K_PRESENTATION;
    break;
  default:
    MWAW_DEBUG_MSG(("ClarisWksDocument::checkHeader: unknown type=%d\n", type));
    break;
  }
  m_parserState->m_kind=kind;
  if (header) {
    header->reset(MWAWDocument::MWAW_T_CLARISWORKS, vers, kind);
    // changeme: draw files are actually converted in text document
    if (type == 0)
      header->setKind(MWAWDocument::MWAW_K_TEXT);
  }

  if (strict && type > 5) return false;
#ifndef DEBUG
  if (type > 8) return false;
#endif
  input->seek(headerSize,librevenge::RVNG_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
// read the zone
////////////////////////////////////////////////////////////
bool ClarisWksDocument::readZone()
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell();
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;

  std::string name("");
  char c = (char) input->readULong(1);
  if (!c)
    input->seek(-1, librevenge::RVNG_SEEK_CUR);
  else {
    if (c >= ' ' && c <= 'z')
      name += c;
    else
      return false;
    for (int i = 0; i < 3; i++) {
      c= (char) input->readULong(1);
      if (c >= ' ' && c <= 'z')
        name += c;
      else
        return false;
    }
  }
  long sz = 0;
  if (name == "QTIM")
    sz = 4;
  else {
    long debPos = input->tell();
    sz = (long) input->readULong(4);
    if (long(input->tell()) != debPos+4) return false;
  }

  if (sz == 0) {
    f << "Entries(Nop):" << name;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  MWAWEntry entry;
  entry.setBegin(pos);
  entry.setLength(4+sz);

  long actPos = input->tell();
  if (!input->checkPosition(entry.end())) return false;
  bool parsed = false;
  if (name.length()) {
    if (name == "DSET") {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      bool complete;
      if (readDSET(complete))
        return true;
    }
    if (name == "FNTM") {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      if (m_styleManager->readFontNames())
        return true;
    }
    if (name == "HDNI" && m_parserState->m_version <= 4)
      sz = 2;
    f << "Entries(" << name << ")";
  }
  else {
    int firstOffset = (int) input->readULong(2);
    if (sz >= 16) { // check for a picture .pct
      input->seek(8, librevenge::RVNG_SEEK_CUR);
      int val = (int) input->readULong(2);
      if (val == 0x1101  && firstOffset == sz)
        parsed = true;
      else if (val == 0x11 && input->readULong(1)==0x2)
        parsed = true;

      if (parsed) {
#ifdef DEBUG_WITH_FILES
        librevenge::RVNGBinaryData file;
        input->seek(actPos, librevenge::RVNG_SEEK_SET);
        input->readDataBlock(sz, file);

        libmwaw::DebugStream f2;
        static int volatile pictName = 0;
        f2 << "Parser" << ++pictName << ".pct";
        libmwaw::Debug::dumpFile(file, f2.str().c_str());
        ascFile.skipZone(actPos, actPos+sz-1);
#endif
        f << "Entries(PICT)";
      }
    }
    if (!parsed)
      f << "Entries(UnknownA" << sz << "A)";
  }

  if (!parsed)
    ascFile.addDelimiter(actPos, '|');

  input->seek(actPos+sz, librevenge::RVNG_SEEK_SET);

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  ascFile.addPos(input->tell());
  ascFile.addNote("_");

  return true;
}

////////////////////////////////////////////////////////////
// read the document main part
////////////////////////////////////////////////////////////
shared_ptr<ClarisWksStruct::DSET> ClarisWksDocument::readDSET(bool &complete)
{
  complete = false;
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell();
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  if (input->readULong(4) != 0x44534554L)
    return shared_ptr<ClarisWksStruct::DSET>();
  long sz = (long) input->readULong(4);
  MWAWEntry entry;
  entry.setBegin(pos);
  entry.setLength(sz+8);

  if (sz < 16) return shared_ptr<ClarisWksStruct::DSET>();
  long endPos = entry.end();
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readDSET: file is too short\n"));
    return shared_ptr<ClarisWksStruct::DSET>();
  }

  ClarisWksStruct::DSET dset;
  input->seek(pos+8, librevenge::RVNG_SEEK_SET);
  dset.m_size = sz;
  dset.m_numData = (int) input->readULong(2);

  input->seek(10, librevenge::RVNG_SEEK_CUR);
  dset.m_fileType = (int) input->readULong(1);
  input->seek(-11, librevenge::RVNG_SEEK_CUR);
  int nFlags = 0;
  switch (dset.m_fileType) {
  case 1: // text
    dset.m_beginSelection = (int) input->readLong(4);
    dset.m_endSelection = (int) input->readLong(4);
    dset.m_textType = (int) input->readULong(1);
    dset.m_flags[nFlags++] = (int) input->readLong(1);
    break;
  default:
    dset.m_flags[nFlags++] = (int) input->readLong(2); // normally -1
    dset.m_flags[nFlags++] = (int) input->readLong(2); // the 0
    dset.m_dataSz = (int) input->readULong(2);
    dset.m_headerSz = (int) input->readULong(2);
    dset.m_flags[nFlags++] = (int) input->readLong(2);
    break;
  }
  dset.m_flags[nFlags++] = (int) input->readLong(2);
  dset.m_id = (int) input->readULong(2) ;

  bool parsed = true;
  shared_ptr<ClarisWksStruct::DSET> res;
  switch (dset.m_fileType) {
  case 0:
    res = getGraphParser()->readGroupZone(dset, entry, complete);
    break;
  case 1:
    res = getTextParser()->readDSETZone(dset, entry, complete);
    break;
  case 2:
    res = getSpreadsheetParser()->readSpreadsheetZone(dset, entry, complete);
    break;
  case 3:
    res = getDatabaseParser()->readDatabaseZone(dset, entry, complete);
    break;
  case 4:
    res = getGraphParser()->readBitmapZone(dset, entry, complete);
    break;
  case 5:
    res = getPresentationParser()->readPresentationZone(dset, entry, complete);
    break;
  case 6:
    res = getTableParser()->readTableZone(dset, entry, complete);
    break;
  default:
    parsed = false;
    break;
  }

  if (parsed) {
    if (!res)
      return shared_ptr<ClarisWksStruct::DSET>();
    if (m_state->m_zonesMap.find(res->m_id) != m_state->m_zonesMap.end()) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readDSET: zone %d already exists!!!!\n",
                      res->m_id));
    }
    else
      m_state->m_zonesMap[res->m_id] = res;
    return res;
  }

  shared_ptr<ClarisWksStruct::DSET> zone(new ClarisWksStruct::DSET(dset));
  f << "Entries(DSETU): " << *zone;

  int data0Length = (int) zone->m_dataSz;
  int N = (int) zone->m_numData;

  ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  if (sz-12 != data0Length*N + zone->m_headerSz) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readDSET: unexpected size for zone definition, try to continue\n"));
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return zone;
  }

  long debPos = endPos-N*data0Length;
  for (int i = 0; i < zone->m_numData; i++) {
    input->seek(debPos, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << "DSETU-" << i << ":";

    long actPos = input->tell();
    if (actPos != debPos && actPos != debPos+data0Length)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(debPos);
    ascFile.addNote(f.str().c_str());
    debPos += data0Length;
  }

  // in general, such a zone is followed by a small zone ( a container)
  zone->m_otherChilds.push_back(zone->m_id+1);
  if (m_state->m_zonesMap.find(zone->m_id) != m_state->m_zonesMap.end()) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readDSET: zone %d already exists!!!!\n",
                    zone->m_id));
  }
  else
    m_state->m_zonesMap[zone->m_id] = zone;

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return zone;
}

////////////////////////////////////////////////////////////
// read the document information
////////////////////////////////////////////////////////////
bool ClarisWksDocument::readDocInfo()
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  int const vers=m_parserState->m_version;
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  f << "Entries(DocInfo):";
  long expectedSize=vers==1 ? 352 : vers < 6 ? 372 : 374;
  long pos = input->tell();
  long endPos=pos+expectedSize;
  if (!input->checkPosition(endPos)) return false;
  f << "ptr=" << std::hex << input->readULong(4) << std::dec << ",";
  int val;
  for (int i = 0; i < 6; i++) {
    val = (int) input->readULong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  m_state->m_headerId = (int) input->readLong(2);
  if (m_state->m_headerId) f << "headerId=" << m_state->m_headerId << ",";
  val = (int) input->readLong(2);
  if (val) f << "unkn=" << val << ",";
  m_state->m_footerId = (int) input->readLong(2);
  if (m_state->m_footerId) f << "footerId=" << m_state->m_footerId << ",";
  for (int i=0; i < 4; ++i) {
    val = (int) input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  int pages[2];
  for (int i=0; i < 2; ++i)
    pages[i]=(int) input->readLong(2);
  if (pages[1]>=1 && pages[1] < 1000 &&
      (pages[0]==1 || (pages[0]>1 && pages[0]<100 && m_parserState->m_kind == MWAWDocument::MWAW_K_DRAW)))
    m_state->m_pages=Vec2i(pages[0],pages[1]);
  // in database field, pages[1] can be very big, this number seems related to the number of record ?
  else if (m_parserState->m_kind != MWAWDocument::MWAW_K_DATABASE || pages[0]!=1) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readDocInfo: the number of pages seems bad\n"));
    f << "###";
  }
  if (pages[0]!=1 || pages[1]!=1)
    f << "pages[num]=" << pages[0] << "x" << pages[1] << ",";
  if (vers==1) {
    ascFile.addDelimiter(input->tell(), '|');
    input->seek(8, librevenge::RVNG_SEEK_CUR);
    ascFile.addDelimiter(input->tell(), '|');

    int numCols = (int) input->readLong(2);
    if (numCols < 1 || numCols > 9) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readDocInfo: pb reading number of columns\n"));
      f << "###numCols=" << numCols;
      numCols = 1;
    }
    if (numCols != 1)
      f << "numCols=" << numCols << ",";
    m_state->m_columns = numCols;
    if (numCols > 1) {
      int colSep = (int) input->readLong(2);
      m_state->m_columnsSep.resize(size_t(numCols-1), colSep);
      f << "colSep=" << colSep << ",";
    }
    else
      input->seek(2, librevenge::RVNG_SEEK_CUR);
  }
  ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(pos+100);
  ascFile.addNote("DocInfo-2");
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read the document header
////////////////////////////////////////////////////////////
bool ClarisWksDocument::readDocHeader()
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  int const vers=m_parserState->m_version;
  long debPos = input->tell();
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  f << "Entries(DocHeader):";

  int val;
  if (vers >= 6) {
    f << "unkn=[";
    for (int i = 0; i < 4; i++) {
      val = (int) input->readLong(1);
      if (val) f << val << ", ";
      else f << "_, ";
    }
    f << "],";
    for (int i = 0; i < 4; i++) {
      val = (int) input->readLong(2);
      if (val) f << "e" << i << "=" << val << ",";
    }
  }
  long pos = input->tell();
  int zone0Length = 52, zone1Length=0;
  switch (vers) {
  case 1:
    zone0Length = 114;
    zone1Length=50;
    break;
  case 2:
  case 3: // checkme: never see a v3 file
    zone0Length = 116;
    zone1Length=112;
    break;
  case 4:
    zone0Length = 120;
    zone1Length=92;
    break;
  case 5:
    zone0Length = 132;
    zone1Length = 92;
    break;
  case 6:
    zone0Length = 124;
    zone1Length = 1126;
    break;
  default:
    break;
  }
  int totalLength = zone0Length+zone1Length;

  input->seek(totalLength, librevenge::RVNG_SEEK_CUR);
  if (input->tell() != pos+totalLength) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: file is too short\n"));
    return false;
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  val = (int) input->readLong(2); // always find 1
  if (val != 1)
    f << "#unkn=" << std::hex << val << std::dec << ",";
  for (int i = 0; i < 4; i++) {
    val = (int) input->readULong(2);
    if (val)
      f << std::hex << "f" << i << "="  << std::hex << val << std::dec << ",";
  }
  int dim[2];
  for (int i = 0; i < 2; i++)
    dim[i] = (int) input->readLong(2);
  f << "dim?=" << dim[1] << "x" << dim[0] << ",";
  int margin[6];
  f << "margin?=[";
  for (int i = 0; i < 6; i++) {
    margin[i] = (int) input->readLong(2);
    f << margin[i] << ",";
  }
  f << "],";
  if (dim[0] > 0 && dim[1] > 0 &&
      margin[0] >= 0 && margin[1] >= 0 && margin[2] >= 0 && margin[3] >= 0 &&
      dim[0] > margin[0]+margin[2] && dim[1] > margin[1]+margin[3]) {

    Vec2i paperSize(dim[1],dim[0]);
    Vec2i lTopMargin(margin[1], margin[0]);
    Vec2i rBotMargin(margin[3], margin[2]);

    m_parser->getPageSpan().setMarginTop(lTopMargin.y()/72.0);
    m_parser->getPageSpan().setMarginBottom(rBotMargin.y()/72.0);
    m_parser->getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
    m_parser->getPageSpan().setMarginRight(rBotMargin.x()/72.0);
    m_parser->getPageSpan().setFormLength(paperSize.y()/72.);
    m_parser->getPageSpan().setFormWidth(paperSize.x()/72.);
    m_state->m_pageSpanSet = true;
  }
  int dim2[2];
  for (int i = 0; i < 2; i++)
    dim2[i] = (int) input->readLong(2);
  f << "dim2?=" << dim2[1] << "x" << dim2[0] << ",";
  int fl[4];
  f << "fl?=[";
  for (int i = 0; i < 4; i++) {
    fl[i] = (int) input->readULong(1);
    if (fl[i])
      f << fl[i] << ",";
    else
      f << "_,";
  }
  f << "],";
  for (int i = 0; i < 9; i++) {
    val = (int) input->readLong(2);
    if (val)
      f << "g" << i << "="  << val << ",";
  }

  if (long(input->tell()) != pos+zone0Length)
    ascFile.addDelimiter(input->tell(), '|');
  input->seek(pos+zone0Length, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(debPos);
  ascFile.addNote(f.str().c_str());

  /* zone 1 actual font, actul pos, .. */
  if (!getTextParser()->readParagraph())
    return false;
  pos = input->tell();
  f.str("");
  f << "DocHeader:zone?=" << input->readULong(2) << ",";
  if (vers >= 4) f << "unkn=" << input->readULong(2) << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  MWAWFont font;
  int posChar;
  if (!getTextParser()->readFont(-1, posChar, font))
    return false;

  /* zone 2, type, unknown */
  pos = input->tell();
  f.str("");
  f << "DocHeader-1:";
  for (int i = 0; i < 6; i++) {
    val = (int) input->readULong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  input->seek(4, librevenge::RVNG_SEEK_CUR);
  int type = (int) input->readULong(1);
  f << "type=" << type << ",";
  val = (int) input->readULong(1);
  if (type != val) {
    if (val >= 0 && val < 7) {
      static char const *(wh[])= { "draw", "text", "spreadsheet", "database", "paint", "presentation", "table" };
      f << "type[display]=" << wh[val] << ",";
    }
    else {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: find unknown display type\n"));
      f << "##unkn=" << val << ",";
    }
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  if (vers <= 2) {
    // the document font ?
    if (!getTextParser()->readFont(-1, posChar, font))
      return false;
    ascFile.addPos(input->tell());
    ascFile.addNote("DocHeader-2");
    if (vers==2) {
      input->seek(46, librevenge::RVNG_SEEK_CUR);
      long actPos = input->tell();
      f.str("");
      f << "DocHeader(Col):";
      int numCols = (int) input->readLong(2);
      if (numCols < 1 || numCols > 9) {
        MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: pb reading number of columns\n"));
        f << "###numCols=" << numCols;
        numCols = 1;
      }
      if (numCols != 1)
        f << "numCols=" << numCols << ",";
      m_state->m_columns = numCols;
      f << "colsW=[";
      for (int i = 0; i < numCols; i++) {
        val = (int) input->readULong(2);
        m_state->m_columnsWidth.push_back(val);
        f << val << ",";
      }
      f << "],";
      input->seek(actPos+20, librevenge::RVNG_SEEK_SET);
      if (numCols > 1) {
        f << "colsS=[";
        for (int i = 0; i < numCols-1; i++) {
          val = (int) input->readULong(2);
          m_state->m_columnsSep.push_back(val);
          f << input->readULong(2) << ",";
        }
        f << "],";
      }
      input->seek(actPos+36, librevenge::RVNG_SEEK_SET);
      val = (int) input->readLong(2);
      if (val) f << "unkn=" << val << ",";
      ascFile.addPos(actPos);
      ascFile.addNote(f.str().c_str());
    }
  }
  else if (long(input->tell()) != pos+zone1Length)
    ascFile.addDelimiter(input->tell(), '|');
  input->seek(pos+zone1Length, librevenge::RVNG_SEEK_SET);
  if (input->isEnd()) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: file is too short\n"));
    return false;
  }
  switch (vers) {
  case 1:
  case 2: {
    pos = input->tell();
    if (!getTextParser()->readParagraphs()) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    pos = input->tell();
    if (!readPrintInfo()) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: can not find print info\n"));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    if (vers==1)
      break;
    pos = input->tell();
    if (!m_styleManager->readPatternList() ||
        !m_styleManager->readGradientList()) {
      input->seek(pos+8, librevenge::RVNG_SEEK_SET);
      return false;
    }
    pos=input->tell();
    f.str("");
    f << "Entries(DocUnkn0):";
    long sz=(long) input->readULong(4);
    if (!sz) {
      ascFile.addPos(pos);
      ascFile.addNote("_");
    }
    else if (input->checkPosition(pos+sz+4)) {
      // find one time with size 0x400 but does not look like a classic struct
      input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    else {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: oops find bad size for DocUnkn0, we may have a problem\n"));
      f << sz << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      return false;
    }
    pos=input->tell();
    if (!readStructZone("DocUnkn1", false)) { // related to link/filename?
      input->seek(pos+4, librevenge::RVNG_SEEK_SET);
      return false;
    }
    pos=input->tell();
    ascFile.addPos(pos);
    ascFile.addNote("Entries(DocUnkn2)"); // another struct ?
    input->seek(4, librevenge::RVNG_SEEK_CUR);
    if (!readStructZone("DocH0", false)) {
      input->seek(pos+4, librevenge::RVNG_SEEK_SET);
      return false;
    }
    pos=input->tell();
    if (!readZoneA()) { // related to struct ?
      input->seek(pos+4, librevenge::RVNG_SEEK_SET);
      return false;
    }
    break;
  }
  case 4:
  case 5:
  case 6: {
    pos = input->tell();
    MWAWEntry entry;
    entry.setBegin(pos);
    entry.setLength(6*260);
    if (!readDSUM(entry, true))
      return false;
    pos = input->tell();
    long sz = (long) input->readULong(4);
    if (!sz) {
      ascFile.addPos(pos);
      ascFile.addNote("Nop");
    }
    else {
      long endPos = pos+4+sz;
      if (!input->checkPosition(endPos)) {
        MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: unexpected LinkInfo size\n"));
        return false;
      }
      ascFile.addPos(pos);
      ascFile.addNote("Entries(LinkInfo)");
      input->seek(endPos, librevenge::RVNG_SEEK_SET);
    }

    if (vers > 4) {
      val = (int) input->readULong(4);
      if (val != long(input->tell())) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: can not find local position\n"));
        ascFile.addPos(pos);
        ascFile.addNote("#");

        return false;
      }
      pos = input->tell(); // series of data with size 42 or 46
      if (!readStructZone("DocUnkn1", false)) {
        input->seek(pos,librevenge::RVNG_SEEK_SET);
        return false;
      }
    }

    pos = input->tell(); // series of data with size 42 or 46
    int expectedSize = 0;
    switch (vers) {
    case 5:
      expectedSize=34;
      break;
    case 6:
      expectedSize=32;
      break;
    default:
      break;
    }
    if (expectedSize) {
      ascFile.addPos(pos);
      ascFile.addNote("DocHeader-3");
      input->seek(pos+expectedSize, librevenge::RVNG_SEEK_SET);
    }

    if (!readPrintInfo()) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: can not find print info\n"));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }

    for (int z = 0; z < 4; z++) { // zone0, zone1 : color palette, zone2 (val:2, id:2)
      if (z==3 && vers!=4) break;
      pos = input->tell();
      sz = (long) input->readULong(4);
      if (!sz) {
        ascFile.addPos(pos);
        ascFile.addNote("Nop");
        continue;
      }
      entry.setBegin(pos);
      entry.setLength(4+sz);
      if (!input->checkPosition(entry.end())) {
        MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: can not read final zones\n"));
        return false;
      }
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      switch (z) {
      case 0:
        ascFile.addPos(pos);
        ascFile.addNote("DocUnkn2");
        break;
      case 1:
        if (!m_styleManager->readColorList(entry)) {
          input->seek(pos, librevenge::RVNG_SEEK_SET);
          return false;
        }
        break;
      case 2: // a serie of id? num
        if (!readStructZone("DocH0", false)) {
          input->seek(pos, librevenge::RVNG_SEEK_SET);
          return false;
        }
        break;
      case 3: // checkme
        ascFile.addPos(pos);
        ascFile.addNote("DocUnkn3");
        break;
      default:
        break;
      }
      input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    }
    break;
  }
  default:
    break;
  }
  return true;
}

////////////////////////////////////////////////////////////
// the end zone (in some v2 file and after )
////////////////////////////////////////////////////////////
bool ClarisWksDocument::readEndTable(long &eof)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  input->seek(0, librevenge::RVNG_SEEK_END);
  eof = input->tell();
  if (m_parserState->m_version<=1) return false;
  if (eof < 20) // this is too short
    return false;
  input->seek(-20, librevenge::RVNG_SEEK_CUR);

  long entryPos= (long) input->readULong(4);
  if (entryPos >= eof-20)
    return false;

  input->seek(entryPos, librevenge::RVNG_SEEK_SET);
  if (input->readULong(4) != 0x4554424c)
    return false;

  long sz = (long) input->readULong(4);
  if (sz <= 16 || (sz%8) != 0 || sz+entryPos+8 != eof) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readEndTable: bad size\n"));
    return false;
  }

  int numEntries = int((sz-16)/8);
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  f << "Entries(ETBL):";
  long prevPos = 0;
  std::vector<MWAWEntry> listEntries;
  MWAWEntry lastEntry;
  for (int i = 0; i < numEntries; i++) {
    std::string name("");
    for (int j = 0; j < 4; j++)
      name+=char(input->readULong(1));
    long pos = (long) input->readULong(4);
    if (pos < prevPos+4 || (i!=numEntries-1 && pos+4 > entryPos)) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readEndTable: bad pos\n"));
      return false;
    }

    lastEntry.setEnd(pos);
    if (i)
      listEntries.push_back(lastEntry);
    lastEntry.setType(name);
    lastEntry.setBegin(pos);

    f << "[" << name << ":" << std::hex << pos << std::dec << "],";
    prevPos = pos;
  }
  lastEntry.setEnd(eof);
  listEntries.push_back(lastEntry);

  ascFile.addPos(entryPos);
  ascFile.addNote(f.str().c_str());

  for (int i = 0; i < numEntries-1; i++) {
    MWAWEntry const &entry = listEntries[(size_t) i];
    bool parsed = false;
    if (entry.type() == "CPRT") {
      readCPRT(entry);
      parsed = true;
    }
    else if (entry.type() == "SNAP") {
      readSNAP(entry);
      parsed = true;
    }
    else if (entry.type() == "STYL") {
      m_styleManager->readStyles(entry);
      parsed = true;
    }
    else if (entry.type() == "DSUM") {
      readDSUM(entry, false);
      parsed = true;
    }
    else if (entry.type() == "TNAM") {
      readTNAM(entry);
      parsed = true;
    }
    else if (entry.type() == "MARK") {
      readMARKList(entry);
      parsed = true;
    }
    else if (entry.type() == "FNTM") {
      input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
      m_styleManager->readFontNames();
      parsed = true;
    }

    // WMBT: crypt password ? 0|fieldSz + PString ?
    if (parsed && input->tell() == entry.end()) continue;
    f.str("");
    f << "Entries(" << entry.type() << ")";
    if (parsed) f << "*";
    ascFile.addPos(entry.begin());
    ascFile.addNote(f.str().c_str());
  }

  if (numEntries)
    eof = listEntries[0].begin();
  return true;
}

////////////////////////////////////////////////////////////
// a list of print info plist
////////////////////////////////////////////////////////////
bool ClarisWksDocument::readCPRT(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.type() != "CPRT")
    return false;
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = entry.begin();
  input->seek(pos+4, librevenge::RVNG_SEEK_SET); // skip header
  long sz = (long) input->readULong(4);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readCPRT: pb with entry length"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  f << "Entries(CPRT)";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  int id = 0;
  while (long(input->tell()) < entry.end()) {
    pos = input->tell();
    sz = (long) input->readULong(4);
    if (pos+sz > entry.end()) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readCPRT: pb with sub zone: %d", id));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    f.str("");
    f << "CPRT-" << id++ << ":";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    if (!sz) continue;
#ifdef DEBUG_WITH_FILES
    librevenge::RVNGBinaryData file;
    input->readDataBlock(sz, file);

    static int volatile cprtName = 0;
    f.str("");
    f << "CPRT" << ++cprtName << ".plist";
    libmwaw::Debug::dumpFile(file, f.str().c_str());

    ascFile.skipZone(pos+4,pos+4+sz-1);
#endif
    input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the mark
////////////////////////////////////////////////////////////
bool ClarisWksDocument::readMARKList(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.type() != "MARK")
    return false;
  int const vers=m_parserState->m_version;
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = entry.begin();
  long sz = entry.length()-8;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  f << "Entries(MARK)[header]:";

  if (input->readULong(4) !=0x4d41524b || input->readLong(4) != sz || sz < 30) {
    f << "###";
    MWAW_DEBUG_MSG(("ClarisWksDocument::readMARKList: find unexpected header\n"));
    ascFile.addPos(entry.begin());
    ascFile.addNote(f.str().c_str());

    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return false;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "MARK[MRKS]:";
  if (input->readULong(4)!=0x4d524b53) { // MRKS
    f << "###";

    MWAW_DEBUG_MSG(("ClarisWksDocument::readMARKList: find unexpected MRKS header\n"));
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return false;
  }
  int val=(int) input->readLong(2);
  if (val != 3)
    f << "f0=" << val << ",";
  int N=(int) input->readLong(2);
  if (N) f << "N=" << N << ",";
  for (int i=0; i<2; ++i) {
    val=(int) input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  ascFile.addDelimiter(input->tell(),'|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  input->seek(pos+30, librevenge::RVNG_SEEK_SET);

  for (int m=0; m < N; ++m) {
    pos = input->tell();
    if (pos+14>entry.end() || input->readULong(4)!=0x4d41524b) { // MARK
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f.str("");
    f << "MARK-" << m << ":";
    val = (int) input->readLong(2);
    if (val != 3)
      f << "f0=" << val << ",";
    int N1=(int) input->readLong(2);
    f << "N1=" << N1 << ",";

    std::string name(""); // can be: Book (anchor), LDOC (link in doc), LURL

    for (int i=0; i<4; i++) {
      char c=(char) input->readLong(1);
      if ((c>='a' && c<='z') || (c>='A' && c<='Z'))
        name += c;
    }
    if (name.size()!=4) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    int what = name=="Book"? 0 : name=="LDOC" ? 1 : name=="LURL" ? 2 : -1;
    if (what==-1) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f << name << ",";
    if (vers < 6) {
      // I think mark in v5, but the code seem to differ from here
      MWAW_DEBUG_MSG(("ClarisWksDocument::readMARKList: OOOPS reading mark data is not implemented\n"));
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      ascFile.addPos(input->tell());
      ascFile.addNote("MARK[End]:###");
      return false;
    }
    f << "f1=" << std::hex << input->readULong(2) << std::dec << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    bool ok=true;
    for (int n=0; n < N1; ++n) {
      pos=input->tell();
      if (pos+54+8>entry.end()) {
        ok=false;
        break;
      }
      f.str("");
      f << "MARK-" << m << "." << n << ":";
      if (input->readLong(2)!=-1 || input->readLong(2)) {
        ok=false;
        break;
      }
      for (int i=0; i < 9; ++i) { // f6:an id?,
        val=(int) input->readULong(2);
        if (val)
          f << "f" << i << "=" << std::hex << val << std::dec << ",";
      }
      int tSz=(int) input->readULong(1);
      if (tSz <= 0 || tSz >=32) {
        ok=false;
        break;
      }
      std::string text("");
      for (int s=0; s < tSz; ++s)
        text+=(char) input->readLong(1);
      f << text << ",";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      input->seek(pos+54, librevenge::RVNG_SEEK_SET);
      pos=input->tell();
      switch (what) {
      case 0:
        ok=readBookmark(entry.end());
        break;
      case 1:
        ok=readDocumentMark(entry.end());
        break;
      case 2:
        ok=readURL(entry.end());
        break;
      default:
        break;
      }
      if (!ok)
        break;
    }
    if (!ok) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
  }

  pos = input->tell();
  if (pos==entry.end())
    return true;
  f.str("");
  f << "###MARK-end:";

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

bool ClarisWksDocument::readURL(long endPos)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=input->tell();
  if (pos+8>endPos) {
    return false;
  }
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  f << "MARK-URL:";
  long type=(long) input->readULong(4);
  if (type==0) {
  }
  else if (type!=0x554c6b64) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readURL: find unexpected header\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  else { // ULkd
    if (input->tell()+32+256+8>endPos) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readURL: date seems too short\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    for (int s=0; s < 2; ++s) {
      int const maxSize = s==0 ? 32: 256;
      long actPos=input->tell();
      int tSz=(int) input->readULong(1);
      if (tSz >= maxSize) {
        MWAW_DEBUG_MSG(("ClarisWksDocument::readURL: find unexpected text size\n"));
        f << "###";
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        return false;
      }
      std::string text("");
      for (int c=0; c < tSz; ++c)
        text+=(char) input->readLong(1);
      f << text << ",";
      input->seek(actPos+maxSize, librevenge::RVNG_SEEK_SET);
    }
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  return readEndMark(endPos);
}

bool ClarisWksDocument::readDocumentMark(long endPos)
{
  // Checkme...
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=input->tell();
  if (pos+8>endPos) {
    return false;
  }
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  f << "MARK-Document:";
  long type=(long) input->readULong(4);
  if (type==0) {
  }
  else if (type!=0x444c6b64) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readDocumentMark: find unexpected header\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  else { // DLkd
    if (input->tell()+32+64+20+8>endPos) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readDocumentMark: date seems too short\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    for (int s=0; s < 2; ++s) {
      int const maxSize = s==0 ? 32: 64;
      long actPos=input->tell();
      int tSz=(int) input->readULong(1);
      if (tSz >= maxSize) {
        MWAW_DEBUG_MSG(("ClarisWksDocument::readDocumentMark: find unexpected text size\n"));
        f << "###";
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        return false;
      }
      std::string text("");
      for (int c=0; c < tSz; ++c)
        text+=(char) input->readLong(1);
      f << text << ",";
      input->seek(actPos+maxSize, librevenge::RVNG_SEEK_SET);
    }
  }
  for (int i=0; i < 10; ++i) { // f7=f9=id ?, other 0
    int val=(int) input->readULong(2);
    if (val)
      f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  return readEndMark(endPos);
}

bool ClarisWksDocument::readBookmark(long endPos)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=input->tell();
  if (pos+8>endPos) {
    return false;
  }
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  f << "MARK-URL:";
  long type=(long) input->readULong(4);
  if (type==0) {
  }
  else if (type!=0x424d6b64) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readBookmark: find unexpected header\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  else { // BMkd
    if (input->tell()+32+8>endPos) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readBookmark: date seems too short\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    int const maxSize = 32;
    long actPos=input->tell();
    int tSz=(int) input->readULong(1);
    if (tSz >= maxSize) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readBookmark: find unexpected text size\n"));
      f << "###";
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      return false;
    }
    std::string text("");
    for (int c=0; c < tSz; ++c)
      text+=(char) input->readLong(1);
    f << text << ",";
    input->seek(actPos+maxSize, librevenge::RVNG_SEEK_SET);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  return readEndMark(endPos);
}

bool ClarisWksDocument::readEndMark(long endPos)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell();
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  f << "MARK[Last]:";
  long val=input->readLong(4);
  if (!val) {
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  f << "f0=" << std::hex << val << std::dec << ",";
  f << "f1=" << std::hex << input->readULong(4) << std::dec << ",";
  for (int i=0; i < 2; ++i) { // g0=1|2|3, g1=0
    val=(int) input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  val=(int) input->readLong(2);
  f << "type=" << val << ",";
  int numExpected=val==1 ? 4: 1;
  if (input->tell()+2*numExpected >endPos) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readEndMark: find unexpected number of element\n"));
    f << "###";
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  f << "unkn=[";
  for (int i=0; i<numExpected; ++i)
    f << input->readLong(2) << ",";
  f << "],";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// a string: temporary file name ?
////////////////////////////////////////////////////////////
bool ClarisWksDocument::readTNAM(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.type() != "TNAM")
    return false;
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = entry.begin();
  long sz = entry.length()-8;
  input->seek(pos+8, librevenge::RVNG_SEEK_SET);

  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  f << "Entries(TNAM):";

  int strSize = (int) input->readULong(1);
  if (strSize != sz-1 || pos+8+sz > entry.end()) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readTNAM: unexpected string size\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  std::string name("");
  for (int i = 0; i < strSize; i++) {
    char c = (char) input->readULong(1);
    if (c) {
      name += c;
      continue;
    }
    MWAW_DEBUG_MSG(("ClarisWksDocument::readTNAM: unexpected string char\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  if (name.length())
    f << name << ",";
  if (long(input->tell()) != entry.end()) {
    ascFile.addDelimiter(input->tell(),'|');
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  }

  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// a list the document property
////////////////////////////////////////////////////////////
bool ClarisWksDocument::readDSUM(MWAWEntry const &entry, bool inHeader)
{
  if (!entry.valid() || (!inHeader && entry.type() != "DSUM"))
    return false;
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = entry.begin();
  long debStrings = inHeader ? pos : pos+8;
  input->seek(debStrings, librevenge::RVNG_SEEK_SET);

  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  f << "Entries(DSUM):";
  for (int entete = 0; entete < 6; entete++) {
    char const *(entryNames[]) = { "Title",  "Category", "Description", "Author", "Version", "Keywords"};
    pos = input->tell();
    long sz = (int) input->readULong(4);
    if (!sz) continue;
    int strSize = (int) input->readULong(1);
    if (strSize != sz-1 || pos+4+sz > entry.end()) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readDSUM: unexpected string size\n"));
      if (pos+4+sz > entry.end() || strSize > sz-1) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        return false;
      }
      f << "###";
    }
    std::string name("");
    for (int i = 0; i < strSize; i++) {
      char c = (char) input->readULong(1);
      if (c) {
        name += c;
        continue;
      }
      MWAW_DEBUG_MSG(("ClarisWksDocument::readDSUM: unexpected string char\n"));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    if (name.length())
      f << entryNames[entete] << "=" << name << ",";
    input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
  }

  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// a generic list of strings
////////////////////////////////////////////////////////////
bool ClarisWksDocument::readStringList(char const *zoneName, bool hasEntete, std::vector<std::string> &res)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell();
  long sz=(long) input->readULong(4);
  long endPos=pos+4+sz;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readStringList: the zone size seems bad\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  if (sz==0) {
    ascFile.addPos(pos-(hasEntete?4:0));
    ascFile.addNote("_");
    return true;
  }
  f << "Entries(" << zoneName << "):strings=[";
  while (!input->isEnd() && input->tell()<endPos) {
    long actPos = input->tell();
    int strSize = (int) input->readULong(1);
    std::string name("");
    if (!strSize) {
      res.push_back(name);
      f << "\"\",";
      continue;
    }
    if (actPos+1+strSize>endPos) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readStringList: unexpected string size\n"));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    for (int i = 0; i < strSize; i++) {
      char c = (char) input->readULong(1);
      if (c) {
        name += c;
        continue;
      }
      MWAW_DEBUG_MSG(("ClarisWksDocument::readStringList: unexpected string char\n"));
      f << "#[0]";
    }
    res.push_back(name);
    f << "\"" << name << "\",";
  }
  f << "],";
  ascFile.addPos(pos-(hasEntete?4:0));
  ascFile.addNote(f.str().c_str());
  return true;
}

///////////////////////////////////////////////////////////
// a list of snapshot
////////////////////////////////////////////////////////////
bool ClarisWksDocument::readSNAP(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.type() != "SNAP")
    return false;
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = entry.begin();
  input->seek(pos+4, librevenge::RVNG_SEEK_SET); // skip header
  long sz = (long) input->readULong(4);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readSNAP: pb with entry length"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  f << "Entries(SNAP)";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  int id = 0;
  while (long(input->tell()) < entry.end()) {
    pos = input->tell();
    int type=(int) input->readLong(1);
    sz = (long) input->readULong(4);
    if (pos+sz > entry.end()) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readSNAP: pb with sub zone: %d", id));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    f.str("");
    f << "SNAP-" << id++ << ":";
    if (type) f << "type=" << type;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

#ifdef DEBUG_WITH_FILES
    librevenge::RVNGBinaryData file;
    input->readDataBlock(sz, file);

    static int volatile snapName = 0;
    f.str("");
    f << "SNAP" << ++snapName << ".pct";
    libmwaw::Debug::dumpFile(file, f.str().c_str());

    if (type == 0)
      ascFile.skipZone(pos+5,pos+5+sz-1);
#endif
    input->seek(pos+5+sz, librevenge::RVNG_SEEK_SET);
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool ClarisWksDocument::readPrintInfo()
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell();
  if (input->readULong(2) != 0) return false;
  long sz = (long) input->readULong(2);
  if (sz < 0x78)
    return false;
  long endPos = pos+4+sz;
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readPrintInfo: file is too short\n"));
    return false;
  }
  input->seek(pos+4, librevenge::RVNG_SEEK_SET);

  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  // print info
  libmwaw::PrinterInfo info;
  if (!info.read(input)) {
    if (sz == 0x78) {
      // the size is ok, so let try to continue
      ascFile.addPos(pos);
      ascFile.addNote("Entries(PrintInfo):##");
      input->seek(endPos, librevenge::RVNG_SEEK_SET);
      MWAW_DEBUG_MSG(("ClarisWksDocument::readPrintInfo: can not read print info, continue\n"));
      return true;
    }
    return false;
  }
  f << "Entries(PrintInfo):"<< info;

  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  if (!m_state->m_pageSpanSet) {
    // define margin from print info
    Vec2i lTopMargin= -1 * info.paper().pos(0);
    Vec2i rBotMargin=info.paper().size() - info.page().size();

    // move margin left | top
    int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
    int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
    lTopMargin -= Vec2i(decalX, decalY);
    rBotMargin += Vec2i(decalX, decalY);

    m_parser->getPageSpan().setMarginTop(lTopMargin.y()/72.0);
    m_parser->getPageSpan().setMarginBottom(rBotMargin.y()/72.0);
    m_parser->getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
    m_parser->getPageSpan().setMarginRight(rBotMargin.x()/72.0);
    m_parser->getPageSpan().setFormLength(paperSize.y()/72.);
    m_parser->getPageSpan().setFormWidth(paperSize.x()/72.);
  }

  if (long(input->tell()) !=endPos) {
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    f << ", #endPos";
    ascFile.addDelimiter(input->tell(), '|');
  }

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  return true;
}

///////////////////////////////////////////////////////////
// try to read a unknown structured zone
////////////////////////////////////////////////////////////
bool ClarisWksDocument::readStructZone(char const *zoneName, bool hasEntete)
{
  if (!m_parserState) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readStructZone: can not find the parser state\n"));
    return false;
  }
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  if (!input->checkPosition(endPos)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksDocument::readStructZone: unexpected size for %s\n", zoneName));
    return false;
  }
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(" << zoneName << "):";

  if (sz == 0) {
    if (hasEntete) {
      ascFile.addPos(pos-4);
      ascFile.addNote(f.str().c_str());
    }
    else {
      ascFile.addPos(pos);
      ascFile.addNote("NOP");
    }
    return true;
  }

  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  int type = (int) input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  int val = (int) input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) input->readULong(2);
  int hSz = (int) input->readULong(2);
  if (!fSz || N *fSz+hSz+12 != sz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksDocument::readStructZone: unexpected size for %s\n", zoneName));
    return false;
  }

  if (long(input->tell()) != pos+4+hSz)
    ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(hasEntete ? pos-4 : pos);
  ascFile.addNote(f.str().c_str());

  long debPos = endPos-N*fSz;
  for (int i = 0; i < N; i++) {
    input->seek(debPos, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << zoneName << "-" << i << ":";

    long actPos = input->tell();
    if (actPos != debPos && actPos != debPos+fSz)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(debPos);
    ascFile.addNote(f.str().c_str());
    debPos += fSz;
  }
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  return true;
}

// try to read a list of structured zone
bool ClarisWksDocument::readStructIntZone(char const *zoneName, bool hasEntete, int intSz, std::vector<int> &res)
{
  if (!m_parserState) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readStructIntZone: can not find the parser state\n"));
    return false;
  }
  res.resize(0);
  if (intSz != 1 && intSz != 2 && intSz != 4) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readStructIntZone: unknown int size: %d\n", intSz));
    return false;
  }

  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksDocument::readStructIntZone: unexpected size for %s\n", zoneName ? zoneName : "unamed"));
    return false;
  }
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  if (zoneName && strlen(zoneName))
    f << "Entries(" << zoneName << "):";

  if (sz == 0) {
    if (hasEntete) {
      ascFile.addPos(pos-4);
      ascFile.addNote(f.str().c_str());
    }
    else {
      ascFile.addPos(pos);
      ascFile.addNote("NOP");
    }
    return true;
  }

  input->seek(pos+4, librevenge::RVNG_SEEK_SET);
  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  int type = (int) input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  long val = input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) input->readULong(2);
  int hSz = (int) input->readULong(2);
  if (fSz != intSz || N *fSz+hSz+12 != sz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksDocument::readStructIntZone: unexpected field size\n"));
    return false;
  }

  long debPos = endPos-N*fSz;
  if (long(input->tell()) != debPos) {
    ascFile.addDelimiter(input->tell(), '|');
    if (N) ascFile.addDelimiter(debPos, '|');
  }
  input->seek(debPos, librevenge::RVNG_SEEK_SET);
  f << "[";
  for (int i = 0; i < N; i++) {
    val = input->readLong(fSz);
    res.push_back((int) val);
    if (val>1000) f << "0x" << std::hex << val << std::dec << ",";
    else f << val << ",";
  }
  f << "]";

  ascFile.addPos(hasEntete ? pos-4 : pos);
  ascFile.addNote(f.str().c_str());

  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  return true;
}

// try to read a list of cell's zone
bool ClarisWksDocument::readStructCellZone(char const *zoneName, bool hasEntete, std::vector<Vec2i> &res)
{
  if (!m_parserState) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readStructCellZone: can not find the parser state\n"));
    return false;
  }
  res.resize(0);

  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksDocument::readStructCellZone: unexpected size for %s\n", zoneName ? zoneName : "unamed"));
    return false;
  }
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  if (zoneName && strlen(zoneName))
    f << "Entries(" << zoneName << "):";

  if (sz == 0) {
    if (hasEntete) {
      ascFile.addPos(pos-4);
      ascFile.addNote(f.str().c_str());
    }
    else {
      ascFile.addPos(pos);
      ascFile.addNote("_");
    }
    return true;
  }

  input->seek(pos+4, librevenge::RVNG_SEEK_SET);
  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  int type = (int) input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  long val = input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) input->readULong(2);
  int hSz = (int) input->readULong(2);
  if (fSz != 4 || N *fSz+hSz+12 != sz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksDocument::readStructCellZone: unexpected field size\n"));
    return false;
  }

  long debPos = endPos-N*fSz;
  if (long(input->tell()) != debPos) {
    ascFile.addDelimiter(input->tell(), '|');
    if (N) ascFile.addDelimiter(debPos, '|');
  }
  input->seek(debPos, librevenge::RVNG_SEEK_SET);
  f << "[";
  for (int i = 0; i < N; i++) {
    int dim[2];
    for (int j = 0; j < 2; j++) dim[j]= (int) input->readLong(2);
    // checkme: do we need to invert the coordinate
    Vec2i cell(dim[0],dim[1]);
    res.push_back(cell);
    f << cell << ",";
  }
  f << "]";

  ascFile.addPos(hasEntete ? pos-4 : pos);
  ascFile.addNote(f.str().c_str());

  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  return true;
}

bool ClarisWksDocument::readZoneA()
{
  if (!m_parserState) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readZoneA: can not find the parser state\n"));
    return false;
  }
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  if (!input->checkPosition(endPos)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksDocument::readZoneA: unexpected size\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(ZoneA):";

  if (sz == 0) {
    ascFile.addPos(pos);
    ascFile.addNote("NOP");
    return true;
  }

  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  int type = (int) input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  int val = (int) input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) input->readULong(2);
  int hSz = (int) input->readULong(2);
  if (!fSz || N *fSz+hSz+12 != sz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksDocument::readZoneA: unexpected size\n"));
    return false;
  }
  if (long(input->tell()) != pos+4+hSz)
    ascFile.addDelimiter(input->tell(), '|');

  input->seek(endPos-N*fSz, librevenge::RVNG_SEEK_SET);
  if (fSz==4) {
    f << "ptrs=[";
    for (int i = 0; i < N; i++) f << std::hex << input->readULong(4) << std::dec << ",";
    f << "],";
  }
  else {
    for (int i = 0; i < N; i++) {
      pos=input->tell();
      f.str("");
      f << "ZoneA-" << i << ":";

      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    MWAW_DEBUG_MSG(("ClarisWksDocument::readZoneA: unexpected field size\n"));
    return true;
  }

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos=input->tell();
  if (!readStructZone("ZoneA",false)) { // find one time a list of id, 0x10
    MWAW_DEBUG_MSG(("ClarisWksDocument::readZoneA: can not read ZoneA-A\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return true;
  }
  ascFile.addPos(pos);
  ascFile.addNote("ZoneA-A:");

  for (int i=0; i<2*N; ++i) {
    pos = input->tell();
    if ((i%2)==0) {
      // the header contains a string: some chart/figure name?
      if (!readStructZone("ZoneA",false)) {
        MWAW_DEBUG_MSG(("ClarisWksDocument::readZoneA: can not read ZoneA-B\n"));
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        return true;
      }
      f.str("");
      f << "ZoneA-B" << i/2 << ":";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      continue;
    }
    f.str("");
    f << "ZoneA-C" << i/2 << "]:";
    // normally a block a 128 bytes: 8 block of 16 bytes?
    sz=(long) input->readULong(4);
    if (!input->checkPosition(pos+sz+4)) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readZoneA: can not find a child field\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      return false;
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// try to order the zones
////////////////////////////////////////////////////////////
void ClarisWksDocument::updateChildPositions()
{
  // try to fix the page position corresponding to the main zone
  int numHorizontalPages=m_parserState->m_kind==MWAWDocument::MWAW_K_DRAW ? getDocumentPages()[0] : 1;
  if (numHorizontalPages <= 0) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::updateChildPositions: the number of accross pages is not set\n"));
    numHorizontalPages=1;
  }
  Vec2f pageDim(72.0f*(float)m_parser->getPageWidth(), 72.0f*(float)getTextHeight());
  float formLength = 72.0f*(float)m_parser->getFormLength();
  std::map<int, shared_ptr<ClarisWksStruct::DSET> >::iterator iter;
  for (iter = m_state->m_zonesMap.begin(); iter != m_state->m_zonesMap.end(); ++iter) {
    shared_ptr<ClarisWksStruct::DSET> zone = iter->second;
    if (!zone) continue;
    zone->updateChildPositions(pageDim, formLength, numHorizontalPages);
  }
}

void ClarisWksDocument::forceParsed(int zoneId)
{
  shared_ptr<ClarisWksStruct::DSET> zMap = getZone(zoneId);
  if (zMap) zMap->m_parsed = true;
}

bool ClarisWksDocument::exploreZonesGraph()
{
  std::map<int, shared_ptr<ClarisWksStruct::DSET> >::iterator iter, iter2;
  // first create the list of fathers
  iter = m_state->m_zonesMap.begin();
  for (; iter != m_state->m_zonesMap.end(); ++iter) {
    shared_ptr<ClarisWksStruct::DSET> zone = iter->second;
    if (!zone) continue;

    int id = zone->m_id;
    size_t numChilds = zone->m_childs.size();
    for (int step = 0; step < 2; step++) {
      for (size_t c = 0; c < numChilds; c++) {
        int cId = step == 0 ? zone->m_childs[c].m_id : zone->m_otherChilds[c];
        if (cId < 0) continue;
        if (cId == 0) {
          MWAW_DEBUG_MSG(("ClarisWksDocument::exploreZonesGraph: find a zone with id=0\n"));
          continue;
        }

        iter2 = m_state->m_zonesMap.find(cId);
        if (iter2 == m_state->m_zonesMap.end()) {
          MWAW_DEBUG_MSG(("ClarisWksDocument::exploreZonesGraph: can not find zone %d\n", cId));
          continue;
        }
        iter2->second->m_fathersList.insert(id);
      }

      if (step == 1) break;
      numChilds = zone->m_otherChilds.size();
    }
  }

  // find the list of potential root
  std::vector<int> rootList;
  std::set<int> notDoneList;
  iter = m_state->m_zonesMap.begin();
  for (; iter != m_state->m_zonesMap.end(); ++iter) {
    shared_ptr<ClarisWksStruct::DSET> zone = iter->second;
    if (!zone) continue;
    zone->m_internal = 0;
    notDoneList.insert(zone->m_id);
    if (zone->m_fathersList.size()) continue;
    rootList.push_back(zone->m_id);
  }

  std::set<int> toDoList(rootList.begin(), rootList.end());
  while (!notDoneList.empty()) {
    int id;
    if (!toDoList.empty()) {
      id = *toDoList.begin();
      toDoList.erase(id);
    }
    else {
      id = *notDoneList.begin();
      MWAW_DEBUG_MSG(("ClarisWksDocument::exploreZonesGraph: find a cycle, choose new root %d\n", id));
      rootList.push_back(id);
    }
    exploreZonesGraphRec(id, notDoneList);
  }

  m_state->m_rootZonesList = rootList;
  if (rootList.empty()) {
    // we have a big problem here, no way to continue
    MWAW_DEBUG_MSG(("ClarisWksDocument::exploreZonesGraph: the graph contains no tree...\n"));
    return false;
  }

  return true;
}

bool ClarisWksDocument::exploreZonesGraphRec(int zId, std::set<int> &notDoneList)
{
  std::map<int, shared_ptr<ClarisWksStruct::DSET> >::iterator iter, iter2;
  notDoneList.erase(zId);
  iter = m_state->m_zonesMap.find(zId);
  if (iter == m_state->m_zonesMap.end()) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::exploreZonesGraphRec: internal problem (can not find zone %d)\n", zId));
    return false;
  }
  shared_ptr<ClarisWksStruct::DSET> zone = iter->second;
  if (!zone) return true;
  zone->m_internal = 1;
  for (int step = 0; step < 2; step++) {
    size_t numChilds = step==0 ? zone->m_childs.size() : zone->m_otherChilds.size();
    std::vector<int> toRemove; // list of bad child
    std::set<int> validChild; // list of good child (already verified)
    for (size_t c = 0; c < numChilds; c++) {
      int cId = step == 0 ? zone->m_childs[c].m_id : zone->m_otherChilds[c];
      if (cId <= 0) continue;
      bool isValid=true;
      if (notDoneList.find(cId) == notDoneList.end()) {
        iter2 = m_state->m_zonesMap.find(cId);
        if (iter2 == m_state->m_zonesMap.end()) {
          MWAW_DEBUG_MSG(("ClarisWksDocument::exploreZonesGraph: can not find zone %d\n", cId));
          isValid=false;
        }
        else if (iter2->second->m_internal==1) {
          MWAW_DEBUG_MSG(("ClarisWksDocument::exploreZonesGraph: find a cycle: for child : %d(<-%d)\n", cId, zId));
          isValid=false;
        }
      }
      else
        exploreZonesGraphRec(cId, notDoneList);
      if (!isValid) {
        toRemove.push_back(cId);
        shared_ptr<ClarisWksStruct::DSET>child=getZone(cId);
        if (child) child->m_fathersList.erase(zone->m_id);
      }
      else if (validChild.find(cId)==validChild.end())
        validChild.insert(cId);
    }
    for (size_t i=0; i<toRemove.size(); ++i)
      zone->removeChild(toRemove[i], step==0);
  }
  zone->m_internal = 2;
  return true;
}

////////////////////////////////////////////////////////////
// try to mark the zones
////////////////////////////////////////////////////////////
void ClarisWksDocument::typeMainZones()
{
  // first type the main zone and its father
  typeMainZonesRec(1, ClarisWksStruct::DSET::P_Main, 100);

  // now check the header/footer
  std::map<int, shared_ptr<ClarisWksStruct::DSET> >::iterator iter;
  if (m_state->m_headerId) {
    iter = m_state->m_zonesMap.find(m_state->m_headerId);
    if (iter != m_state->m_zonesMap.end() && iter->second)
      iter->second->m_position = ClarisWksStruct::DSET::P_Header;
  }
  if (m_state->m_footerId) {
    iter = m_state->m_zonesMap.find(m_state->m_footerId);
    if (iter != m_state->m_zonesMap.end() && iter->second)
      iter->second->m_position = ClarisWksStruct::DSET::P_Footer;
  }

  // then type the slides
  getPresentationParser()->updateSlideTypes();

  iter = m_state->m_zonesMap.begin();
  std::vector<int> listZonesId[ClarisWksStruct::DSET::P_Unknown];
  while (iter != m_state->m_zonesMap.end()) {
    int id = iter->first;
    shared_ptr<ClarisWksStruct::DSET> node = iter++->second;
    ClarisWksStruct::DSET::Position pos = node ? node->m_position : ClarisWksStruct::DSET::P_Unknown;
    if (pos == ClarisWksStruct::DSET::P_Unknown || pos == ClarisWksStruct::DSET::P_Main)
      continue;
    if (node->m_fileType != 1) // only propage data from a text node
      continue;
    if (pos > ClarisWksStruct::DSET::P_Unknown || pos < 0) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::typeMainZones: OOPS, internal problem with type\n"));
      continue;
    }
    listZonesId[pos].push_back(id);
  }
  // try to type parents of typed zone
  for (int pos=ClarisWksStruct::DSET::P_Header; pos < ClarisWksStruct::DSET::P_Slide;  pos++) {
    for (size_t z = 0; z < listZonesId[pos].size(); z++)
      typeMainZonesRec(listZonesId[pos][z], ClarisWksStruct::DSET::Position(pos), 1);
  }
  // update m_zonesTypeMap
  iter = m_state->m_zonesMap.begin();
  while (iter != m_state->m_zonesMap.end()) {
    int id = iter->first;
    shared_ptr<ClarisWksStruct::DSET> node = iter++->second;
    if (node)
      m_state->m_zonesTypeMap[id]=node->m_position;
  }
  // time to do to remove header/footer's zones in the main zones list
  std::vector<int> rootList=m_state->m_rootZonesList;
  m_state->m_rootZonesList.resize(0);
  for (size_t i=0; i<rootList.size(); ++i) {
    shared_ptr<ClarisWksStruct::DSET> zone=getZone(rootList[i]);
    if (!zone) continue;
    if (zone->isHeaderFooter())
      m_state->m_hFZonesList.push_back(rootList[i]);
    else
      m_state->m_rootZonesList.push_back(rootList[i]);
  }
  // remove me
  bool isPresentation=m_parserState->m_kind==MWAWDocument::MWAW_K_PRESENTATION;
  for (int step=0; !isPresentation && step<2; ++step) {
    int id=step==0 ? m_state->m_headerId : m_state->m_footerId;
    if (!id) continue;
    // try to retrieve the father is also a header
    shared_ptr<ClarisWksStruct::DSET> zone=getZone(id);
    if (zone && zone->m_fathersList.size()==1) {
      shared_ptr<ClarisWksStruct::DSET> father=getZone(*zone->m_fathersList.begin());
      if (father && father->isHeaderFooter()) {
        id=*zone->m_fathersList.begin();
        if (step==0) m_state->m_headerId=id;
        else m_state->m_footerId=id;
      }
      if (std::find(m_state->m_hFZonesList.begin(),m_state->m_hFZonesList.end(), id)==m_state->m_hFZonesList.end())
        m_state->m_hFZonesList.push_back(id);
    }
  }
}

int ClarisWksDocument::typeMainZonesRec(int zId, ClarisWksStruct::DSET::Position pos, int maxHeight)
{
  if (maxHeight < 0) return 0;

  shared_ptr<ClarisWksStruct::DSET> node = getZone(zId);
  if (!node) return 0;
  if (node->m_position == ClarisWksStruct::DSET::P_Unknown)
    node->m_position = pos;
  else if (node->m_position != pos)
    return 0;
  if (maxHeight==0)
    return zId;

  int res = zId;
  for (std::set<int>::iterator it = node->m_fathersList.begin();
       it != node->m_fathersList.end(); ++it) {
    int fId = typeMainZonesRec(*it, pos, maxHeight-1);
    if (fId) res = fId;
  }
  return res;
}

////////////////////////////////////////////////////////////
// remove uneeded edge (mainly header/footer edge in main graph)
////////////////////////////////////////////////////////////
void ClarisWksDocument::cleanZonesGraph()
{
  bool isPresentation=m_parserState->m_kind==MWAWDocument::MWAW_K_PRESENTATION;
  if (isPresentation)
    m_presentationParser->disconnectMasterFromContents();
  else if (m_parserState->m_kind==MWAWDocument::MWAW_K_DRAW)
    m_graphParser->disconnectMasterFromContents();

  std::set<int>::iterator it;
  for (size_t i=0; i<m_state->m_hFZonesList.size(); ++i) {
    int id=m_state->m_hFZonesList[i];
    shared_ptr<ClarisWksStruct::DSET> zone=getZone(id);
    if (!zone) continue;
    std::set<int> newFathers;
    for (it = zone->m_fathersList.begin(); it != zone->m_fathersList.end(); ++it) {
      shared_ptr<ClarisWksStruct::DSET> father = getZone(*it);
      if (!father || father->m_position!=ClarisWksStruct::DSET::P_Main || father->m_fileType!=0) {
        newFathers.insert(*it);
        continue;
      }
      father->removeChild(id, true);
    }
    zone->m_fathersList=newFathers;
  }
  std::map<int, shared_ptr<ClarisWksStruct::DSET> >::const_iterator iter = m_state->m_zonesMap.begin();
  for (; iter != m_state->m_zonesMap.end(); ++iter) {
    int id=iter->first;
    shared_ptr<ClarisWksStruct::DSET> zone = iter->second;
    if (!zone || zone->m_fathersList.size()<2) continue;
    it=zone->m_fathersList.begin();
    int fId=*(it++);
    int fId1=*it;
    if (zone->m_fathersList.size()==2) {
      if (fId1==fId+1 && m_state->getZoneType(fId)==ClarisWksStruct::DSET::P_Table) {
        // a table and it auxiliary representation
        shared_ptr<ClarisWksStruct::DSET> group=getZone(fId+1);
        if (group && group->m_fileType==0) {
          zone->m_fathersList.erase(fId1);
          group->removeChild(id, true);
          continue;
        }
      }
      else if (zone->isHeaderFooter() && !isPresentation) {
        // try to detach an header/footer text zone to the main zone
        ClarisWksStruct::DSET::Position fType=m_state->getZoneType(fId);
        ClarisWksStruct::DSET::Position fType1=m_state->getZoneType(fId1);
        int which=0;
        if (fType==ClarisWksStruct::DSET::P_Main && (fType1==ClarisWksStruct::DSET::P_Header || fType1==ClarisWksStruct::DSET::P_Footer))
          which=fId;
        else if (fType1==ClarisWksStruct::DSET::P_Main && (fType==ClarisWksStruct::DSET::P_Header || fType==ClarisWksStruct::DSET::P_Footer))
          which=fId1;
        if (which) {
          shared_ptr<ClarisWksStruct::DSET> father=getZone(which);
          if (father && father->m_fileType==1) {
            /* ok, pointer to a text section, we only erase connection.
             fixme: replace the connection in the text section to the group corresponding to the header */
            father->m_fathersList.erase(which);
            continue;
          }
        }
      }
    }
#ifdef DEBUG
    std::cerr << "Node " << id << " has [";
    for (it=zone->m_fathersList.begin(); it!=zone->m_fathersList.end(); ++it)
      std::cerr << *it << ",";
    std::cerr  << "] parents\n";
#endif
  }
  if (m_parserState->m_kind == MWAWDocument::MWAW_K_TEXT) {
    // time to try detach the main text block
    shared_ptr<ClarisWksStruct::DSET> main=getZone(1);
    if (main && main->m_fileType==1 && main->m_fathersList.size()==1) {
      shared_ptr<ClarisWksStruct::DSET> mainFather=getZone(*main->m_fathersList.begin());
      if (mainFather && mainFather->m_fileType==0) {
        // ok, let remove the connection between the text group and the text and consider that the text group is parsed
        mainFather->m_parsed=true;
        mainFather->removeChild(1, true);
        main->m_fathersList.clear();
        // now try to detach the text group from the document group
        if (mainFather->m_fathersList.size()==1 && !m_state->m_rootZonesList.empty() &&
            *mainFather->m_fathersList.begin()==m_state->m_rootZonesList[0]) {
          shared_ptr<ClarisWksStruct::DSET> root=getZone(m_state->m_rootZonesList[0]);
          if (root) {
            mainFather->m_fathersList.clear();
            root->removeChild(mainFather->m_id, true);
          }
        }
        else {
          MWAW_DEBUG_MSG(("ClarisWksDocument::cleanZonesGraph: oops, can not detach the main text zone from the document zone\n"));
        }
      }
      else {
        MWAW_DEBUG_MSG(("ClarisWksDocument::cleanZonesGraph: oops, can not find the main zone block\n"));
      }
    }
    else {
      MWAW_DEBUG_MSG(("ClarisWksDocument::cleanZonesGraph: oops, can not find the main zone block id\n"));
    }
  }

  /* try to remove orphelan group. There often exists one in word
     processing document and if we do not remove it, it will create an
     empty textbox, so let remove all of them...

     Checkme: removing empty textboxes must be also done as there
     appears frequently (a debutant's error), but as this requires
     interaction with the graph(to check the border/background's
     style) and the text(to check if the textbox are empty) parsers,
     :-~ */
  std::set<int> orphelanSet;
  iter = m_state->m_zonesMap.begin();
  for (; iter != m_state->m_zonesMap.end(); ++iter) {
    int id=iter->first;
    shared_ptr<ClarisWksStruct::DSET> zone = iter->second;
    if (!zone || zone->m_fileType!=0 || !zone->m_childs.empty() || !zone->m_otherChilds.empty() || zone->m_parsed)
      continue;
    orphelanSet.insert(id);
  }
  while (!orphelanSet.empty()) {
    int id=*orphelanSet.begin();
    orphelanSet.erase(id);
    shared_ptr<ClarisWksStruct::DSET> zone = getZone(id);
    if (!zone || zone->m_parsed || zone->m_fathersList.size()!=1)
      continue;
    zone->m_parsed=true;

    int fId=*zone->m_fathersList.begin();
    shared_ptr<ClarisWksStruct::DSET> father = getZone(fId);
    if (!father)
      continue;
    zone->m_fathersList.clear();
    father->removeChild(id);

    if (father->m_fileType!=0 || !father->m_childs.empty() || !father->m_otherChilds.empty() || father->m_parsed)
      continue;
    orphelanSet.insert(fId);
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
