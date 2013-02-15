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

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPrinter.hxx"

#include "CWDatabase.hxx"
#include "CWGraph.hxx"
#include "CWPresentation.hxx"
#include "CWSpreadsheet.hxx"
#include "CWStruct.hxx"
#include "CWStyleManager.hxx"
#include "CWTable.hxx"
#include "CWText.hxx"

#include "CWParser.hxx"

/** Internal: the structures of a CWParser */
namespace CWParserInternal
{

////////////////////////////////////////
//! Internal: the state of a CWParser
struct State {
  //! constructor
  State() : m_EOF(-1L), m_actPage(0), m_numPages(0),
    m_columns(1), m_columnsWidth(), m_columnsSep(),
    m_headerId(0), m_footerId(0), m_headerHeight(0), m_footerHeight(0),
    m_zonesMap(), m_mainZonesList() {
  }

  //! the last position
  long m_EOF;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  /** the number of columns */
  int m_columns;
  /** the columns witdh in Points*/
  std::vector<int> m_columnsWidth;
  /** the columns separator in Points*/
  std::vector<int> m_columnsSep;
  int m_headerId /** the header zone if known */,
      m_footerId /** the footer zone if known */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
  std::map<int, shared_ptr<CWStruct::DSET> > m_zonesMap /** the map of zone*/;
  std::vector<int> m_mainZonesList/** the list of main group */;
};

////////////////////////////////////////
//! Internal: the subdocument of a CWParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(CWParser &pars, MWAWInputStreamPtr input, int zoneId) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const {
    if (MWAWSubDocument::operator!=(doc)) return true;
    SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
    if (!sDoc) return true;
    if (m_id != sDoc->m_id) return true;
    return false;
  }

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
    MWAW_DEBUG_MSG(("CWParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  if (m_id == -1) { // a number used to send linked frame
    listener->insertChar(' ');
    return;
  }
  if (m_id == 0) {
    MWAW_DEBUG_MSG(("CWParserInternal::SubDocument::parse: unknown zone\n"));
    return;
  }

  assert(m_parser);

  reinterpret_cast<CWParser *>(m_parser)->sendZone(m_id);
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
CWParser::CWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_state(),
  m_pageSpan(), m_pageSpanSet(false), m_databaseParser(), m_graphParser(), m_presentationParser(),
  m_spreadsheetParser(), m_styleManager(), m_tableParser(), m_textParser()
{
  init();
}

CWParser::~CWParser()
{
}

void CWParser::init()
{
  resetListener();
  setAsciiName("main-1");

  m_state.reset(new CWParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  m_pageSpan.setMarginTop(0.1);
  m_pageSpan.setMarginBottom(0.1);
  m_pageSpan.setMarginLeft(0.1);
  m_pageSpan.setMarginRight(0.1);

  m_styleManager.reset(new CWStyleManager(*this));

  m_databaseParser.reset(new CWDatabase(*this));
  m_graphParser.reset(new CWGraph(*this));
  m_presentationParser.reset(new CWPresentation(*this));
  m_spreadsheetParser.reset(new CWSpreadsheet(*this));
  m_tableParser.reset(new CWTable(*this));
  m_textParser.reset(new CWText(*this));
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
float CWParser::pageHeight() const
{
  return float(m_pageSpan.getFormLength()-m_pageSpan.getMarginTop()-m_pageSpan.getMarginBottom()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0);
}

float CWParser::pageWidth() const
{
  return float(m_pageSpan.getFormWidth()-m_pageSpan.getMarginLeft()-m_pageSpan.getMarginRight());
}

Vec2f CWParser::getPageLeftTop() const
{
  return Vec2f(float(m_pageSpan.getMarginLeft()),
               float(m_pageSpan.getMarginTop()+m_state->m_headerHeight/72.0));
}

bool CWParser::getColor(int colId, MWAWColor &col) const
{
  return m_graphParser->getColor(colId, col);
}

float CWParser::getPatternPercent(int id) const
{
  return m_graphParser->getPatternPercent(id);
}

CWStruct::DSET::Type CWParser::getZoneType(int zId) const
{
  std::map<int, shared_ptr<CWStruct::DSET> >::iterator iter
    = m_state->m_zonesMap.find(zId);
  if (iter == m_state->m_zonesMap.end() || !iter->second)
    return CWStruct::DSET::T_Unknown;
  return iter->second->m_type;
}

void CWParser::getHeaderFooterId(int &headerId, int &footerId) const
{
  headerId = m_state->m_headerId;
  footerId = m_state->m_footerId;
}

void CWParser::checkOrdering(std::vector<int16_t> &vec16, std::vector<int32_t> &vec32) const
{
  if (version() < 4) return;
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
// new page
////////////////////////////////////////////////////////////
void CWParser::newPage(int number)
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
// interface with the different parser
////////////////////////////////////////////////////////////
bool CWParser::sendZone(int zoneId, MWAWPosition position)
{
  if (m_state->m_zonesMap.find(zoneId) == m_state->m_zonesMap.end())
    return false;
  shared_ptr<CWStruct::DSET> zMap = m_state->m_zonesMap[zoneId];
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  bool res = false;
  switch(zMap->m_fileType) {
  case 0: // group
  case 4: // bitmap
    res = m_graphParser->sendZone(zoneId, position);
    break;
  case 1:
    res = m_textParser->sendZone(zoneId);
    break;
  case 5:
    res = m_presentationParser->sendZone(zoneId);
    break;
  case 6:
    res = m_tableParser->sendZone(zoneId);
    break;
  case 2:
    // res = m_spreadsheetParser->sendZone(zoneId);
    MWAW_DEBUG_MSG(("CWParser::sendZone: sending a spreadsheet is not implemented\n"));
    break;
  case 3:
    // res=m_databaseParser->sendZone(zoneId);
    MWAW_DEBUG_MSG(("CWParser::sendZone: sending a database is not implemented\n"));
    break;
  default:
    MWAW_DEBUG_MSG(("CWParser::sendZone: can not send zone: %d\n", zoneId));
    break;
  }
  input->seek(pos, WPX_SEEK_SET);
  zMap->m_parsed = true;
  return res;
}

////////////////////////////////////////////////////////////
// interface with the text parser
////////////////////////////////////////////////////////////
void CWParser::getColumnInfo(int &numColumns, std::vector<int> &width,
                             std::vector<int> &sep) const
{
  numColumns = m_state->m_columns;
  width = m_state->m_columnsWidth;
  sep = m_state->m_columnsSep;
}

void CWParser::sendFootnote(int zoneId)
{
  if (!getListener()) return;

  MWAWSubDocumentPtr subdoc(new CWParserInternal::SubDocument(*this, getInput(), zoneId));
  getListener()->insertNote(MWAWContentListener::FOOTNOTE, subdoc);
}

void CWParser::sendZoneInFrame(int zoneId, MWAWPosition pos, WPXPropertyList extras, WPXPropertyList frameExtras)
{
  if (!getListener()) return;

  MWAWSubDocumentPtr subdoc(new CWParserInternal::SubDocument(*this, getInput(), zoneId));
  getListener()->insertTextBox(pos, subdoc, extras, frameExtras);
}

void CWParser::forceParsed(int zoneId)
{
  if (m_state->m_zonesMap.find(zoneId) == m_state->m_zonesMap.end())
    return;
  shared_ptr<CWStruct::DSET> zMap = m_state->m_zonesMap[zoneId];
  if (zMap) zMap->m_parsed = true;
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void CWParser::parse(WPXDocumentInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      for (size_t i = 0; i < m_state->m_mainZonesList.size(); i++)
        sendZone(m_state->m_mainZonesList[i]);
      m_presentationParser->flushExtra();
      m_graphParser->flushExtra();
      m_tableParser->flushExtra();
      m_textParser->flushExtra();
    }
    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("CWParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void CWParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getListener()) {
    MWAW_DEBUG_MSG(("CWParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan ps(m_pageSpan);
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
  m_state->m_numPages = numPage;

  for (int i = 0; i < 2; i++) {
    int zoneId = i == 0 ? m_state->m_headerId : m_state->m_footerId;
    if (zoneId == 0)
      continue;

    shared_ptr<MWAWSubDocument> subdoc(new CWParserInternal::SubDocument(*this, getInput(), zoneId));
    ps.setHeaderFooter((i==0) ? MWAWPageSpan::HEADER : MWAWPageSpan::FOOTER, MWAWPageSpan::ALL, subdoc);
  }

  for (int i = 0; i <= m_state->m_numPages; i++) pageList.push_back(ps);

  //
  MWAWContentListenerPtr listen(new MWAWContentListener(getFontConverter(), pageList, documentInterface));
  setListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool CWParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;

  long pos = input->tell();
  if (version() > 1)
    readEndTable();
  if (m_state->m_EOF > 0)
    input->pushLimit(m_state->m_EOF);

  input->seek(pos, WPX_SEEK_SET);

  if (readDocHeader()) {
    pos = input->tell();
    while(!input->atEOS()) {
      if (!readZone()) {
        input->seek(pos, WPX_SEEK_SET);
        break;
      }
      pos = input->tell();
    }
  }
  if (!input->atEOS()) {
    ascii().addPos(input->tell());
    f.str("");
    f << "Entries(Loose): vers=" << version();
    ascii().addNote(f.str().c_str());
  }
  // look for graphic
  while (!input->atEOS()) {
    pos = input->tell();
    int val = (int) input->readULong(2);
    if (input->atEOS()) break;
    bool ok = false;
    if (val == 0x4453) {
      if (input->readULong(2) == 0x4554) {
        ok = true;
        input->seek(-4, WPX_SEEK_CUR);
      }
    }
    if (!ok && (val == 0x1101 || val == 0x1102)) {
      long debPos = (val == 0x1102) ? pos-15 : pos-14;
      input->seek(debPos, WPX_SEEK_SET);
      if (input->readULong(2) == 0) {
        int sz = (int) input->readULong(2);
        int fSz  = (int) input->readULong(2);
        if (sz >= 0x10 && (val == 0x1102 || sz == fSz)) {
          ok = true;
          input->seek(-6, WPX_SEEK_CUR);
        }
      }
    }
    if (!ok) {
      input->seek(pos+1, WPX_SEEK_SET);
      continue;
    }

    if (input->atEOS()) break;

    long prevPos = pos;
    ok = false;
    while(!input->atEOS()) {
      if (!readZone()) {
        input->seek(pos+1, WPX_SEEK_SET);
        break;
      }
      pos = input->tell();
      if (pos <= prevPos)
        break;
      ok = true;
    }
    if (!ok || pos <= prevPos) {
      input->seek(prevPos+1, WPX_SEEK_SET);
      continue;
    }
    if (input->atEOS()) break;

    ascii().addPos(pos);
    ascii().addNote("Entries(End)");
  }
  if (m_state->m_EOF > 0)
    input->popLimit();
  exploreZonesGraph();
  typeMainZones();
  return m_state->m_zonesMap.size() != 0;
}

////////////////////////////////////////////////////////////
// try to mark the zones
////////////////////////////////////////////////////////////
void CWParser::typeMainZones()
{
  // first (ype the main zone and its father
  typeMainZonesRec(1, CWStruct::DSET::T_Main, 100);

  std::map<int, shared_ptr<CWStruct::DSET> >::iterator iter;
  // finally the slid
  std::vector<int> slidesList = m_presentationParser->getSlidesList();
  for (size_t slide = 0; slide < slidesList.size(); slide++) {
    iter = m_state->m_zonesMap.find(slidesList[slide]);
    if (iter != m_state->m_zonesMap.end() && iter->second)
      iter->second->m_type = CWStruct::DSET::T_Slide;
  }
  // now check the header/footer
  if (m_state->m_headerId) {
    iter = m_state->m_zonesMap.find(m_state->m_headerId);
    if (iter != m_state->m_zonesMap.end() && iter->second)
      iter->second->m_type = CWStruct::DSET::T_Header;
  }
  if (m_state->m_footerId) {
    iter = m_state->m_zonesMap.find(m_state->m_footerId);
    if (iter != m_state->m_zonesMap.end() && iter->second)
      iter->second->m_type = CWStruct::DSET::T_Footer;
  }
  iter = m_state->m_zonesMap.begin();
  std::vector<int> listZonesId[CWStruct::DSET::T_Unknown];
  while (iter != m_state->m_zonesMap.end()) {
    int id = iter->first;
    shared_ptr<CWStruct::DSET> node = iter++->second;
    CWStruct::DSET::Type type = node ? node->m_type : CWStruct::DSET::T_Unknown;
    if (type == CWStruct::DSET::T_Unknown || type == CWStruct::DSET::T_Main)
      continue;
    if (node->m_fileType != 1) // only propage data from a text node
      continue;
    if (type > CWStruct::DSET::T_Unknown || type < 0) {
      MWAW_DEBUG_MSG(("CWParser::typeMainZones: OOPS, internal problem with type\n"));
      continue;
    }
    listZonesId[type].push_back(id);
  }
  bool isPres = getHeader() && getHeader()->getKind() == MWAWDocument::K_PRESENTATION;
  for (int type=CWStruct::DSET::T_Header; type < CWStruct::DSET::T_Slide;  type++) {
    for (size_t z = 0; z < listZonesId[type].size(); z++) {
      int fId = typeMainZonesRec(listZonesId[type][z], CWStruct::DSET::Type(type), 1);
      if (!fId)
        continue;
      if (isPres) // fixme: actually as the main type is not good too dangerous
        fId=listZonesId[type][z];
      if (type==CWStruct::DSET::T_Header && !m_state->m_headerId)
        m_state->m_headerId = fId;
      else if (type==CWStruct::DSET::T_Footer && !m_state->m_footerId)
        m_state->m_footerId = fId;
    }
  }
}

int CWParser::typeMainZonesRec(int zId, CWStruct::DSET::Type type, int maxHeight)
{
  if (maxHeight < 0) return 0;

  std::map<int, shared_ptr<CWStruct::DSET> >::iterator iter
    = m_state->m_zonesMap.find(zId);
  if (iter == m_state->m_zonesMap.end() || !iter->second)
    return 0;
  shared_ptr<CWStruct::DSET> node = iter->second;
  if (node->m_type == CWStruct::DSET::T_Unknown)
    node->m_type = type;
  else if (node->m_type != type)
    return 0;
  if (maxHeight==0)
    return zId;

  int res = zId;
  for (std::set<int>::iterator it = node->m_fathersList.begin();
       it != node->m_fathersList.end(); it++) {
    int fId = typeMainZonesRec(*it, type, maxHeight-1);
    if (fId) res = fId;
  }
  return res;
}

////////////////////////////////////////////////////////////
// try to order the zones
////////////////////////////////////////////////////////////
bool CWParser::exploreZonesGraph()
{
  std::map<int, shared_ptr<CWStruct::DSET> >::iterator iter, iter2;
  // first create the list of fathers
  iter = m_state->m_zonesMap.begin();
  for ( ; iter != m_state->m_zonesMap.end(); iter++) {
    shared_ptr<CWStruct::DSET> zone = iter->second;
    if (!zone) continue;

    int id = zone->m_id;
    size_t numChilds = zone->m_childs.size();
    for (int step = 0; step < 2; step++) {
      for (size_t c = 0; c < numChilds; c++) {
        int cId = step == 0 ? zone->m_childs[c].m_id : zone->m_otherChilds[c];
        if (cId < 0) continue;
        if (cId == 0) {
          MWAW_DEBUG_MSG(("CWParser::exploreZonesGraph: find a zone with id=0\n"));
          continue;
        }

        iter2 = m_state->m_zonesMap.find(cId);
        if (iter2 == m_state->m_zonesMap.end()) {
          MWAW_DEBUG_MSG(("CWParser::exploreZonesGraph: can not find zone %d\n", cId));
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
  for ( ; iter != m_state->m_zonesMap.end(); iter++) {
    shared_ptr<CWStruct::DSET> zone = iter->second;
    if (!zone) continue;
    zone->m_internal = 0;
    notDoneList.insert(zone->m_id);
    if (zone->m_fathersList.size()) continue;
    rootList.push_back(zone->m_id);
  }

  std::set<int> toDoList(rootList.begin(), rootList.end());
  while(notDoneList.size()) {
    int id;
    if (toDoList.size()) {
      id = *toDoList.begin();
      toDoList.erase(id);
    } else {
      id = *notDoneList.begin();
      MWAW_DEBUG_MSG(("CWParser::exploreZonesGraph: find a cycle, choose new root %d\n", id));
      rootList.push_back(id);
    }
    exploreZonesGraphRec(id, notDoneList);
  }

  m_state->m_mainZonesList = rootList;
  size_t numMain = rootList.size();
  if (numMain == 1)
    return true;
#ifdef DEBUG
  // we have do not have find the root note : probably a database...
  iter = m_state->m_zonesMap.begin();
  std::cerr << "--------------------------------------------------------\n";
  std::cerr << "List of potential main zones : ";
  for (size_t i = 0; i < numMain; i++)
    std::cerr << rootList[i] << ",";
  std::cerr << "\n";
  for ( ; iter != m_state->m_zonesMap.end(); iter++) {
    shared_ptr<CWStruct::DSET> zone = iter->second;
    std::cerr << *zone << "\n";
  }
  std::cerr << "--------------------------------------------------------\n";
#endif
  if (numMain == 0) {
    // we have a big problem here, no way to continue
    MWAW_DEBUG_MSG(("CWParser::exploreZonesGraph: the graph contains no tree...\n"));
    return false;
  }


  return true;
}

bool CWParser::exploreZonesGraphRec(int zId, std::set<int> &notDoneList)
{
  std::map<int, shared_ptr<CWStruct::DSET> >::iterator iter, iter2;
  notDoneList.erase(zId);
  iter = m_state->m_zonesMap.find(zId);
  if (iter == m_state->m_zonesMap.end()) {
    MWAW_DEBUG_MSG(("CWParser::exploreZonesGraphRec: internal problem (can not find zone %d)\n", zId));
    return false;
  }
  shared_ptr<CWStruct::DSET> zone = iter->second;
  if (!zone) return true;
  zone->m_internal = 1;
  size_t numChilds = zone->m_childs.size();
  for (int step = 0; step < 2; step++) {
    for (size_t c = 0; c < numChilds; c++) {
      int cId = step == 0 ? zone->m_childs[c].m_id : zone->m_otherChilds[c];
      if (cId <= 0) continue;
      if (notDoneList.find(cId) == notDoneList.end()) {
        iter2 = m_state->m_zonesMap.find(cId);
        if (iter2 == m_state->m_zonesMap.end()) {
          MWAW_DEBUG_MSG(("CWParser::exploreZonesGraph: can not find zone %d\n", cId));
        } else if (iter2->second->m_internal==1) {
          MWAW_DEBUG_MSG(("CWParser::exploreZonesGraph: find a cycle: for child : %d(<-%d)\n", cId, zId));
        } else
          zone->m_validedChildList.insert(cId);
      } else {
        zone->m_validedChildList.insert(cId);
        exploreZonesGraphRec(cId, notDoneList);
      }
    }
    if (step == 1) break;
    numChilds = zone->m_otherChilds.size();
  }
  zone->m_internal = 2;
  return true;
}

////////////////////////////////////////////////////////////
// the end zone (in some v2 file and after )
////////////////////////////////////////////////////////////
bool CWParser::readEndTable()
{
  if (version() <= 1) return false;

  MWAWInputStreamPtr input = getInput();

  // try to go to the end of file
  while (!input->atEOS())
    input->seek(10000, WPX_SEEK_CUR);

  m_state->m_EOF = input->tell();
  if (m_state->m_EOF < 20) // this is too short
    return false;
  input->seek(-20, WPX_SEEK_CUR);

  long entryPos= (long) input->readULong(4);
  if (entryPos >= m_state->m_EOF-20)
    return false;

  input->seek(entryPos, WPX_SEEK_SET);
  if (input->readULong(4) != 0x4554424c)
    return false;

  long sz = (long) input->readULong(4);
  if (sz <= 16 || (sz%8) != 0 || sz+entryPos+8 != m_state->m_EOF) {
    MWAW_DEBUG_MSG(("CWParser::readEndTable: bad size\n"));
    return false;
  }

  int numEntries = int((sz-16)/8);
  libmwaw::DebugStream f;
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
      MWAW_DEBUG_MSG(("CWParser::readEndTable: bad pos\n"));
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
  lastEntry.setEnd(m_state->m_EOF);
  listEntries.push_back(lastEntry);

  ascii().addPos(entryPos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < numEntries-1; i++) {
    MWAWEntry const &entry = listEntries[(size_t) i];
    long debPos = entry.begin();
    bool parsed = false;
    if (entry.type() == "CPRT") {
      readCPRT(entry);
      parsed = true;
    } else if (entry.type() == "SNAP") {
      readSNAP(entry);
      parsed = true;
    } else if (entry.type() == "STYL") {
      m_styleManager->readStyles(entry);
      parsed = true;
    } else if (entry.type() == "DSUM") {
      readDSUM(entry, false);
      parsed = true;
    } else if (entry.type() == "TNAM") {
      readTNAM(entry);
      parsed = true;
    }
    if (parsed) {
      debPos = input->tell();
      if (debPos == entry.end()) continue;
    }
    f.str("");
    f << "Entries(" << entry.type() << ")";
    if (parsed) f << "*";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
  }

  if (numEntries)
    m_state->m_EOF = listEntries[0].begin();
  return true;
}

bool CWParser::readZone()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;

  std::string name("");
  char c = (char) input->readULong(1);
  if (!c)
    input->seek(-1, WPX_SEEK_CUR);
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
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return true;
  }
  MWAWEntry entry;
  entry.setBegin(pos);
  entry.setLength(4+sz);

  long actPos = input->tell();
  input->seek(sz, WPX_SEEK_CUR);
  if (long(input->tell()) != actPos+sz) return false;
  bool parsed = false, complete;
  if (name.length()) {
    if (name == "DSET") {
      input->seek(pos, WPX_SEEK_SET);
      if (readDSET(complete))
        return true;
    }
    if (name == "HDNI" && version() <= 4)
      sz = 2;
    f << "Entries(" << name << ")";
  } else {
    //
    input->seek(actPos, WPX_SEEK_SET);
    int firstOffset = (int) input->readULong(2);
    if (sz >= 16) {
      input->seek(8, WPX_SEEK_CUR);
      int val = (int) input->readULong(2);
      if (val == 0x1101  && firstOffset == sz)
        parsed = true;
      else if (val == 0x11 && input->readULong(1)==0x2)
        parsed = true;

      if (parsed) {
#ifdef DEBUG_WITH_FILES
        WPXBinaryData file;
        input->seek(actPos, WPX_SEEK_SET);
        input->readDataBlock(sz, file);

        libmwaw::DebugStream f2;
        static int volatile pictName = 0;
        f2 << "Parser" << ++pictName << ".pct";
        libmwaw::Debug::dumpFile(file, f2.str().c_str());
#endif
        f << "Entries(PICT)";
        ascii().skipZone(actPos, actPos+sz-1);
      }
    }
    if (!parsed)
      f << "Entries(UnknownA" << sz << "A)";
  }

  if (!parsed)
    ascii().addDelimiter(actPos, '|');

  input->seek(actPos+sz, WPX_SEEK_SET);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(input->tell());
  ascii().addNote("_");

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
bool CWParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = CWParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;
  libmwaw::DebugStream f;
  int const headerSize=8;
  input->seek(headerSize,WPX_SEEK_SET);
  if (int(input->tell()) != headerSize) {
    MWAW_DEBUG_MSG(("CWParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,WPX_SEEK_SET);
  f << "FileHeader:";
  int vers = (int) input->readLong(1);
  setVersion(vers);
  if (vers <=0 || vers > 6) {
    MWAW_DEBUG_MSG(("CWParser::checkHeader: unknown version: %d\n", vers));
    return false;
  }
  f << "vers=" << vers << ",";
  f << "unk=" << std::hex << input->readULong(2) << ",";
  int val = (int) input->readLong(1);
  if (val)
    f << "unkn1=" << val << ",";
  if (input->readULong(2) != 0x424f && input->readULong(2) != 0x424f)
    return false;

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  int typePos = 0;
  switch(vers) {
  case 1:
    typePos = 243;
    break;
  case 2:
  case 3:
    typePos = 249;
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
  input->seek(typePos, WPX_SEEK_SET);
  if (long(input->tell()) != typePos)
    return false;
  int type = (int) input->readULong(1);
  if (header) {
    header->reset(MWAWDocument::CW, version());
    switch (type) {
    case 0:
      header->setKind(MWAWDocument::K_DRAW);
      break;
    case 1:
      header->setKind(MWAWDocument::K_TEXT);
      break;
    case 2:
      header->setKind(MWAWDocument::K_SPREADSHEET);
      break;
    case 3:
      header->setKind(MWAWDocument::K_DATABASE);
      break;
    case 4:
      header->setKind(MWAWDocument::K_PAINT);
      break;
    case 5:
      header->setKind(MWAWDocument::K_PRESENTATION);
      break;
    default:
      MWAW_DEBUG_MSG(("CWParser::checkHeader: unknown type=%d\n", type));
      header->setKind(MWAWDocument::K_UNKNOWN);
      break;
    }
#ifdef DEBUG
    if (type >= 0 && type < 5)
      header->setKind(MWAWDocument::K_TEXT);
#else
    if (type == 0 || type == 4)
      header->setKind(MWAWDocument::K_TEXT);
#endif
  }

  if (strict && type > 5) return false;
#ifndef DEBUG
  if (type > 8) return false;
#endif
  input->seek(headerSize,WPX_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
// a document part
////////////////////////////////////////////////////////////
shared_ptr<CWStruct::DSET> CWParser::readDSET(bool &complete)
{
  complete = false;
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;
  if (input->readULong(4) != 0x44534554L)
    return shared_ptr<CWStruct::DSET>();
  long sz = (long) input->readULong(4);
  MWAWEntry entry;
  entry.setBegin(pos);
  entry.setLength(sz+8);

  if (sz < 16) return shared_ptr<CWStruct::DSET>();
  long endPos = entry.end();
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("CWParser::readDSET: file is too short\n"));
    return shared_ptr<CWStruct::DSET>();
  }

  CWStruct::DSET dset;
  input->seek(pos+8, WPX_SEEK_SET);
  dset.m_size = sz;
  dset.m_numData = (int) input->readULong(2);

  input->seek(10, WPX_SEEK_CUR);
  dset.m_fileType = (int) input->readULong(1);
  input->seek(-11, WPX_SEEK_CUR);
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
  shared_ptr<CWStruct::DSET> res;
  switch (dset.m_fileType) {
  case 0:
    res = m_graphParser->readGroupZone(dset, entry, complete);
    break;
  case 1:
    res = m_textParser->readDSETZone(dset, entry, complete);
    break;
  case 2:
    res = m_spreadsheetParser->readSpreadsheetZone(dset, entry, complete);
    break;
  case 3:
    res = m_databaseParser->readDatabaseZone(dset, entry, complete);
    break;
  case 4:
    res = m_graphParser->readBitmapZone(dset, entry, complete);
    break;
  case 5:
    res = m_presentationParser->readPresentationZone(dset, entry, complete);
    break;
  case 6:
    res = m_tableParser->readTableZone(dset, entry, complete);
    break;
  default:
    parsed = false;
    break;
  }

  if (parsed) {
    if (!res)
      return shared_ptr<CWStruct::DSET>();
    if (m_state->m_zonesMap.find(res->m_id) != m_state->m_zonesMap.end()) {
      MWAW_DEBUG_MSG(("CWParser::readDSET: zone %d already exists!!!!\n",
                      res->m_id));
    } else
      m_state->m_zonesMap[res->m_id] = res;
    return res;
  }

  shared_ptr<CWStruct::DSET> zone(new CWStruct::DSET(dset));
  f << "Entries(DSETU): " << *zone;

  int data0Length = (int) zone->m_dataSz;
  int N = (int) zone->m_numData;

  ascii().addDelimiter(input->tell(), '|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  if (sz-12 != data0Length*N + zone->m_headerSz) {
    MWAW_DEBUG_MSG(("CWParser::readDSET: unexpected size for zone definition, try to continue\n"));
    input->seek(endPos, WPX_SEEK_SET);
    return zone;
  }

  long debPos = endPos-N*data0Length;
  for (int i = 0; i < zone->m_numData; i++) {
    input->seek(debPos, WPX_SEEK_SET);
    f.str("");
    f << "DSETU-" << i << ":";

    long actPos = input->tell();
    if (actPos != debPos && actPos != debPos+data0Length)
      ascii().addDelimiter(input->tell(),'|');
    ascii().addPos(debPos);
    ascii().addNote(f.str().c_str());
    debPos += data0Length;
  }

  // in general, such a zone is followed by a small zone ( a container)
  zone->m_otherChilds.push_back(zone->m_id+1);
  if (m_state->m_zonesMap.find(zone->m_id) != m_state->m_zonesMap.end()) {
    MWAW_DEBUG_MSG(("CWParser::readDSET: zone %d already exists!!!!\n",
                    zone->m_id));
  } else
    m_state->m_zonesMap[zone->m_id] = zone;

  input->seek(endPos, WPX_SEEK_SET);
  return zone;
}

///////////////////////////////////////////////////////////
// try to read a unknown structured zone
////////////////////////////////////////////////////////////
bool CWParser::readStructZone(char const *zoneName, bool hasEntete)
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos,WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWParser::readStructZone: unexpected size for %s\n", zoneName));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(" << zoneName << "):";

  if (sz == 0) {
    if (hasEntete) {
      ascii().addPos(pos-4);
      ascii().addNote(f.str().c_str());
    } else {
      ascii().addPos(pos);
      ascii().addNote("NOP");
    }
    return true;
  }

  input->seek(pos+4, WPX_SEEK_SET);
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
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWParser::readStructZone: unexpected size for %s\n", zoneName));
    return false;
  }

  if (long(input->tell()) != pos+4+hSz)
    ascii().addDelimiter(input->tell(), '|');
  ascii().addPos(hasEntete ? pos-4 : pos);
  ascii().addNote(f.str().c_str());


  long debPos = endPos-N*fSz;
  for (int i = 0; i < N; i++) {
    input->seek(debPos, WPX_SEEK_SET);
    f.str("");
    f << zoneName << "-" << i << ":";

    long actPos = input->tell();
    if (actPos != debPos && actPos != debPos+fSz)
      ascii().addDelimiter(input->tell(),'|');
    ascii().addPos(debPos);
    ascii().addNote(f.str().c_str());
    debPos += fSz;
  }
  input->seek(endPos,WPX_SEEK_SET);
  return true;
}

// try to read a list of structured zone
bool CWParser::readStructIntZone(char const *zoneName, bool hasEntete, int intSz, std::vector<int> &res)
{
  res.resize(0);
  if (intSz != 1 && intSz != 2 && intSz != 4) {
    MWAW_DEBUG_MSG(("CWParser::readStructIntZone: unknown int size: %d\n", intSz));
    return false;
  }

  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos,WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWParser::readStructIntZone: unexpected size for %s\n", zoneName));
    return false;
  }
  libmwaw::DebugStream f;
  if (zoneName && strlen(zoneName))
    f << "Entries(" << zoneName << "):";

  if (sz == 0) {
    if (hasEntete) {
      ascii().addPos(pos-4);
      ascii().addNote(f.str().c_str());
    } else {
      ascii().addPos(pos);
      ascii().addNote("NOP");
    }
    return true;
  }

  input->seek(pos+4, WPX_SEEK_SET);
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
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWParser::readStructIntZone: unexpected  field size\n"));
    return false;
  }

  long debPos = endPos-N*fSz;
  if (long(input->tell()) != debPos) {
    ascii().addDelimiter(input->tell(), '|');
    if (N) ascii().addDelimiter(debPos, '|');
  }
  input->seek(debPos, WPX_SEEK_SET);
  f << "[";
  for (int i = 0; i < N; i++) {
    val = input->readLong(fSz);
    res.push_back((int) val);
    f << val << ",";
  }
  f << "]";

  ascii().addPos(hasEntete ? pos-4 : pos);
  ascii().addNote(f.str().c_str());

  input->seek(endPos,WPX_SEEK_SET);
  return true;
}

///////////////////////////////////////////////////////////
// a list of snapshot
////////////////////////////////////////////////////////////
bool CWParser::readSNAP(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.type() != "SNAP")
    return false;
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos+4, WPX_SEEK_SET); // skip header
  long sz = (long) input->readULong(4);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("CWParser::readSNAP: pb with entry length"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  libmwaw::DebugStream f;
  f << "Entries(SNAP)";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  int id = 0;
  while (long(input->tell()) < entry.end()) {
    pos = input->tell();
    int type=(int) input->readLong(1);
    sz = (long) input->readULong(4);
    if (pos+sz > entry.end()) {
      MWAW_DEBUG_MSG(("CWParser::readSNAP: pb with sub zone: %d", id));
      input->seek(pos, WPX_SEEK_SET);
      return false;
    }
    f.str("");
    f << "SNAP-" << id++ << ":";
    if (type) f << "type=" << type;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

#ifdef DEBUG_WITH_FILES
    WPXBinaryData file;
    input->readDataBlock(sz, file);

    static int volatile snapName = 0;
    f.str("");
    f << "SNAP" << ++snapName << ".pct";
    libmwaw::Debug::dumpFile(file, f.str().c_str());

    if (type == 0)
      ascii().skipZone(pos+5,pos+5+sz-1);
#endif
    input->seek(pos+5+sz, WPX_SEEK_SET);
  }

  return true;
}

////////////////////////////////////////////////////////////
// a list the document property
////////////////////////////////////////////////////////////
bool CWParser::readDSUM(MWAWEntry const &entry, bool inHeader)
{
  if (!entry.valid() || (!inHeader && entry.type() != "DSUM"))
    return false;
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  long debStrings = inHeader ? pos : pos+8;
  input->seek(debStrings, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(DSUM):";
  for (int entete = 0; entete < 6; entete++) {
    char const *(entryNames[]) = { "Title",  "Category", "Description", "Author", "Version", "Keywords"};
    pos = input->tell();
    long sz = (int) input->readULong(4);
    if (!sz) continue;
    int strSize = (int) input->readULong(1);
    if (strSize != sz-1 || pos+4+sz > entry.end()) {
      MWAW_DEBUG_MSG(("CWParser::readDSUM: unexpected string size\n"));
      if (pos+4+sz > entry.end() || strSize > sz-1) {
        input->seek(pos, WPX_SEEK_SET);
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
      MWAW_DEBUG_MSG(("CWParser::readDSUM: unexpected string char\n"));
      input->seek(pos, WPX_SEEK_SET);
      return false;
    }
    if (name.length())
      f << entryNames[entete] << "=" << name << ",";
    input->seek(pos+4+sz, WPX_SEEK_SET);
  }

  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// a string: temporary file name ?
////////////////////////////////////////////////////////////
bool CWParser::readTNAM(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.type() != "TNAM")
    return false;
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  long sz = entry.length()-8;
  input->seek(pos+8, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(TNAM):";

  int strSize = (int) input->readULong(1);
  if (strSize != sz-1 || pos+8+sz > entry.end()) {
    MWAW_DEBUG_MSG(("CWParser::readTNAM: unexpected string size\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  std::string name("");
  for (int i = 0; i < strSize; i++) {
    char c = (char) input->readULong(1);
    if (c) {
      name += c;
      continue;
    }
    MWAW_DEBUG_MSG(("CWParser::readTNAM: unexpected string char\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  if (name.length())
    f << name << ",";
  if (long(input->tell()) != entry.end()) {
    ascii().addDelimiter(input->tell(),'|');
    input->seek(entry.end(), WPX_SEEK_SET);
  }

  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// a list of print info plist
////////////////////////////////////////////////////////////
bool CWParser::readCPRT(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.type() != "CPRT")
    return false;
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos+4, WPX_SEEK_SET); // skip header
  long sz = (long) input->readULong(4);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("CWParser::readCPRT: pb with entry length"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  libmwaw::DebugStream f;
  f << "Entries(CPRT)";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  int id = 0;
  while (long(input->tell()) < entry.end()) {
    pos = input->tell();
    sz = (long) input->readULong(4);
    if (pos+sz > entry.end()) {
      MWAW_DEBUG_MSG(("CWParser::readCPRT: pb with sub zone: %d", id));
      input->seek(pos, WPX_SEEK_SET);
      return false;
    }
    f.str("");
    f << "CPRT-" << id++ << ":";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    if (!sz) continue;
#ifdef DEBUG_WITH_FILES
    WPXBinaryData file;
    input->readDataBlock(sz, file);

    static int volatile cprtName = 0;
    f.str("");
    f << "CPRT" << ++cprtName << ".plist";
    libmwaw::Debug::dumpFile(file, f.str().c_str());

    ascii().skipZone(pos+4,pos+4+sz-1);
#endif
    input->seek(pos+4+sz, WPX_SEEK_SET);
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool CWParser::readDocHeader()
{
  MWAWInputStreamPtr input = getInput();
  long debPos = input->tell();
  libmwaw::DebugStream f;
  f << "Entries(DocHeader):";

  int val;
  if (version() >= 6) {
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
  int zone0Length = 52, zone1Length=0, zoneFinalLength = 0;
  switch(version()) {
  case 1:
    zone0Length = 114;
    zone1Length=50;
    zoneFinalLength = 352;
    break;
  case 2:
    zone0Length = 116;
    zone1Length=112;
    zoneFinalLength = 428;
    break;
  case 3:
    zone0Length = 116;
    zone1Length=112; // check me
    break;
  case 4:
    zone0Length = 120;
    zone1Length=92;
    zoneFinalLength = 376;//408;
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
  int totalLength = zone0Length+zone1Length+zoneFinalLength;

  input->seek(totalLength, WPX_SEEK_CUR);
  if (input->tell() != pos+totalLength) {
    MWAW_DEBUG_MSG(("CWParser::readDocHeader: file is too short\n"));
    return false;
  }
  input->seek(pos, WPX_SEEK_SET);
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

    // decrease right | bottom
    int rightMarg = rBotMargin.x() -50;
    if (rightMarg < 0) rightMarg=0;
    int botMarg = rBotMargin.y() -50;
    if (botMarg < 0) botMarg=0;

    m_pageSpan.setMarginTop(lTopMargin.y()/72.0);
    m_pageSpan.setMarginBottom(botMarg/72.0);
    m_pageSpan.setMarginLeft(lTopMargin.x()/72.0);
    m_pageSpan.setMarginRight(rightMarg/72.0);
    m_pageSpan.setFormLength(paperSize.y()/72.);
    m_pageSpan.setFormWidth(paperSize.x()/72.);
    m_pageSpanSet = true;
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
    ascii().addDelimiter(input->tell(), '|');
  input->seek(pos+zone0Length, WPX_SEEK_SET);
  ascii().addPos(debPos);
  ascii().addNote(f.str().c_str());

  /* zone 1 actual font, actul pos, .. */
  if (!m_textParser->readParagraph())
    return false;
  pos = input->tell();
  f.str("");
  f << "DocHeader:zone?=" << input->readULong(2) << ",";
  if (version() >= 4) f << "unkn=" << input->readULong(2) << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  MWAWFont font;
  int posChar;
  if (!m_textParser->readFont(-1, posChar, font))
    return false;

  /* zone 2, type, unknown */
  pos = input->tell();
  f.str("");
  f << "DocHeader-1:";
  for (int i = 0; i < 6; i++) {
    val = (int) input->readULong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  input->seek(4, WPX_SEEK_CUR);
  int type = (int) input->readULong(1);
  f << "type=" << type << ",";
  val = (int) input->readULong(1);
  if (type != val) f << "#unkn=" << val << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  if (version() <= 2) {
    // the document font ?
    if (!m_textParser->readFont(-1, posChar, font))
      return false;
    ascii().addPos(input->tell());
    ascii().addNote("DocHeader-2");
    if (version()==2) {
      input->seek(46, WPX_SEEK_CUR);
      long actPos = input->tell();
      f.str("");
      f << "DocHeader(Col):";
      int numCols = (int) input->readLong(2);
      if (numCols < 1 || numCols > 9) {
        MWAW_DEBUG_MSG(("CWParser::readDocHeader: pb reading number of columns\n"));
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
      input->seek(actPos+20, WPX_SEEK_SET);
      if (numCols > 1) {
        f << "colsS=[";
        for (int i = 0; i < numCols-1; i++) {
          val = (int) input->readULong(2);
          m_state->m_columnsSep.push_back(val);
          f << input->readULong(2) << ",";
        }
        f << "],";
      }
      input->seek(actPos+36, WPX_SEEK_SET);
      val = (int) input->readLong(2);
      if (val) f << "unkn=" << val << ",";
      ascii().addPos(actPos);
      ascii().addNote(f.str().c_str());
    }
  } else if (long(input->tell()) != pos+zone1Length)
    ascii().addDelimiter(input->tell(), '|');
  input->seek(pos+zone1Length, WPX_SEEK_SET);
  if (input->atEOS()) {
    MWAW_DEBUG_MSG(("CWParser::readDocHeader: file is too short\n"));
    return false;
  }
  switch (version()) {
  case 1:
  case 2: {
    pos = input->tell();
    if (!m_textParser->readParagraphs())
      return false;
    pos = input->tell();
    if (!readPrintInfo()) {
      MWAW_DEBUG_MSG(("CWParser::readDocHeader: can not find print info\n"));
      input->seek(pos, WPX_SEEK_SET);
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
      ascii().addPos(pos);
      ascii().addNote("Nop");
    } else {
      long endPos = pos+4+sz;
      input->seek(endPos, WPX_SEEK_SET);
      if (long(input->tell()) != endPos) {
        input->seek(pos, WPX_SEEK_SET);
        MWAW_DEBUG_MSG(("CWParser::readDocHeader: unexpected DocUnkn0 size\n"));
        return false;
      }
      ascii().addPos(pos);
      ascii().addNote("Entries(DocUnkn0)");
      input->seek(endPos, WPX_SEEK_SET);
    }

    if (version() > 4) {
      val = (int) input->readULong(4);
      if (val != long(input->tell())) {
        input->seek(pos, WPX_SEEK_SET);
        MWAW_DEBUG_MSG(("CWParser::readDocHeader: can not find local position\n"));
        ascii().addPos(pos);
        ascii().addNote("#");

        return false;
      }
      pos = input->tell();
      if (!readStructZone("DocUnkn1", false)) {
        input->seek(pos,WPX_SEEK_SET);
        return false;
      }
    }

    pos = input->tell();
    int expectedSize = 0;
    switch (version()) {
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
      ascii().addPos(pos);
      ascii().addNote("DocHeader-3");
      input->seek(pos+expectedSize, WPX_SEEK_SET);
    }

    if (!readPrintInfo()) {
      MWAW_DEBUG_MSG(("CWParser::readDocHeader: can not find print info\n"));
      input->seek(pos, WPX_SEEK_SET);
      return false;
    }

    break;
  }
  default:
    break;
  }
  if (version() >= 4) {
    for (int z = 0; z < 3; z++) { // zone0, zone1 : color palette, zone2 (val:2, id:2)
      pos = input->tell();
      long sz = (long) input->readULong(4);
      if (!sz) {
        ascii().addPos(pos);
        ascii().addNote("Nop");
        continue;
      }
      MWAWEntry entry;
      entry.setBegin(pos);
      entry.setLength(4+sz);
      input->seek(entry.end(), WPX_SEEK_SET);
      if (long(input->tell()) != entry.end()) {
        MWAW_DEBUG_MSG(("CWParser::readDocHeader: can not read final zones\n"));
        input->seek(pos, WPX_SEEK_SET);
        return false;
      }
      input->seek(pos, WPX_SEEK_SET);
      switch(z) {
      case 0:
        ascii().addPos(pos);
        ascii().addNote("DocUnkn2");
        break;
      case 1:
        if (!m_graphParser->readColorList(entry)) {
          input->seek(pos, WPX_SEEK_SET);
          return false;
        }
        break;
      case 2:
        if (!readStructZone("DocUnkn3", false)) {
          input->seek(pos, WPX_SEEK_SET);
          return false;
        }
        break;
      default:
        break;
      }
      input->seek(entry.end(), WPX_SEEK_SET);
    }
  }
  if (zoneFinalLength) {
    pos = input->tell();
    f.str("");
    f << "Entries(HeaderEnd):";
    if (version()<=2) {
      if (version()==2) {
        input->seek(56, WPX_SEEK_CUR);
        ascii().addDelimiter(input->tell(), '|');
      }
      f << "ptr=" << std::hex << input->readULong(4) << std::dec << ",";
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
      if (version()==1) {
        ascii().addDelimiter(input->tell(), '|');
        input->seek(20, WPX_SEEK_CUR);
        ascii().addDelimiter(input->tell(), '|');

        int numCols = (int) input->readLong(2);
        if (numCols < 1 || numCols > 9) {
          MWAW_DEBUG_MSG(("CWParser::readDocHeader: pb reading number of columns\n"));
          f << "###numCols=" << numCols;
          numCols = 1;
        }
        if (numCols != 1)
          f << "numCols=" << numCols << ",";
        m_state->m_columns = numCols;
        if (numCols > 1) {
          int colSep = (int) input->readLong(2);
          for (int i = 0; i < numCols; i++)
            m_state->m_columnsSep.push_back(colSep);
          f << "colSep=" << colSep << ",";
        } else
          input->seek(2, WPX_SEEK_CUR);
      }
    }
    if (int(input->tell()) != pos)
      ascii().addDelimiter(input->tell(), '|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+zoneFinalLength, WPX_SEEK_SET);
  }
  return zoneFinalLength != 0;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool CWParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (input->readULong(2) != 0) return false;
  long sz = (long) input->readULong(2);
  if (sz < 0x78)
    return false;
  long endPos = pos+4+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("CWParser::readPrintInfo: file is too short\n"));
    return false;
  }
  input->seek(pos+4, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  // print info
  libmwaw::PrinterInfo info;
  if (!info.read(input)) {
    if (sz == 0x78) {
      // the size is ok, so let try to continue
      ascii().addPos(pos);
      ascii().addNote("Entries(PrintInfo):##");
      input->seek(endPos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("CWParser::readPrintInfo: can not read print info, continue\n"));
      return true;
    }
    return false;
  }
  f << "Entries(PrintInfo):"<< info;

  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  if (!m_pageSpanSet) {
    // define margin from print info
    Vec2i lTopMargin= -1 * info.paper().pos(0);
    Vec2i rBotMargin=info.paper().size() - info.page().size();

    // move margin left | top
    int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
    int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
    lTopMargin -= Vec2i(decalX, decalY);
    rBotMargin += Vec2i(decalX, decalY);

    // decrease right | bottom
    int rightMarg = rBotMargin.x() -50;
    if (rightMarg < 0) rightMarg=0;
    int botMarg = rBotMargin.y() -50;
    if (botMarg < 0) botMarg=0;

    m_pageSpan.setMarginTop(lTopMargin.y()/72.0);
    m_pageSpan.setMarginBottom(botMarg/72.0);
    m_pageSpan.setMarginLeft(lTopMargin.x()/72.0);
    m_pageSpan.setMarginRight(rightMarg/72.0);
    m_pageSpan.setFormLength(paperSize.y()/72.);
    m_pageSpan.setFormWidth(paperSize.x()/72.);
  }

  if (long(input->tell()) !=endPos) {
    input->seek(endPos, WPX_SEEK_SET);
    f << ", #endPos";
    ascii().addDelimiter(input->tell(), '|');
  }

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
