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

#include <librevenge/librevenge.h>

#include "MWAWTextListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSection.hxx"

#include "ClarisWksDatabase.hxx"
#include "ClarisWksDocument.hxx"
#include "ClarisWksGraph.hxx"
#include "ClarisWksPresentation.hxx"
#include "ClarisWksSpreadsheet.hxx"
#include "ClarisWksStruct.hxx"
#include "ClarisWksStyleManager.hxx"
#include "ClarisWksTable.hxx"
#include "ClarisWksText.hxx"

#include "ClarisWksParser.hxx"

/** Internal: the structures of a ClarisWksParser */
namespace ClarisWksParserInternal
{

////////////////////////////////////////
//! Internal: the state of a ClarisWksParser
struct State {
  //! constructor
  State() : m_kind(MWAWDocument::MWAW_K_UNKNOWN), m_EOF(-1L), m_actPage(0), m_numPages(0),
    m_zonesMap(), m_mainZonesList()
  {
  }

  //! the document kind
  MWAWDocument::Kind m_kind;
  //! the last position
  long m_EOF;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  std::map<int, shared_ptr<ClarisWksStruct::DSET> > m_zonesMap /** the map of zone*/;
  std::vector<int> m_mainZonesList/** the list of main group */;
};

////////////////////////////////////////
//! Internal: the subdocument of a ClarisWksParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(ClarisWksParser &pars, MWAWInputStreamPtr input, int zoneId, MWAWPosition const &pos=MWAWPosition()) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId), m_position(pos) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const
  {
    if (MWAWSubDocument::operator!=(doc)) return true;
    SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
    if (!sDoc) return true;
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
  //! the subdocument id
  int m_id;
  //! the subdocument position if defined
  MWAWPosition m_position;
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("ClarisWksParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  if (m_id == -1) { // a number used to send linked frame
    listener->insertChar(' ');
    return;
  }
  if (m_id == 0) {
    MWAW_DEBUG_MSG(("ClarisWksParserInternal::SubDocument::parse: unknown zone\n"));
    return;
  }

  assert(m_parser);
  reinterpret_cast<ClarisWksParser *>(m_parser)->sendZone(m_id, false,m_position);
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
ClarisWksParser::ClarisWksParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWTextParser(input, rsrcParser, header), m_state(), m_document()
{
  init();
}

ClarisWksParser::~ClarisWksParser()
{
}

void ClarisWksParser::init()
{
  resetTextListener();
  setAsciiName("main-1");

  m_state.reset(new ClarisWksParserInternal::State);
  m_document.reset(new ClarisWksDocument(*this));
  m_document->m_canSendZoneAsGraphic=reinterpret_cast<ClarisWksDocument::CanSendZoneAsGraphic>(&ClarisWksParser::canSendZoneAsGraphic);
  m_document->m_forceParsed=reinterpret_cast<ClarisWksDocument::ForceParsed>(&ClarisWksParser::forceParsed);
  m_document->m_getZone=reinterpret_cast<ClarisWksDocument::GetZone>(&ClarisWksParser::getZone);
  m_document->m_newPage=reinterpret_cast<ClarisWksDocument::NewPage>(&ClarisWksParser::newPage);
  m_document->m_sendFootnote=reinterpret_cast<ClarisWksDocument::SendFootnote>(&ClarisWksParser::sendFootnote);
  m_document->m_sendZone=reinterpret_cast<ClarisWksDocument::SendZone>(&ClarisWksParser::sendZone);
  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}

////////////////////////////////////////////////////////////
// zone
////////////////////////////////////////////////////////////
shared_ptr<ClarisWksStruct::DSET> ClarisWksParser::getZone(int zId) const
{
  std::map<int, shared_ptr<ClarisWksStruct::DSET> >::iterator iter
    = m_state->m_zonesMap.find(zId);
  if (iter != m_state->m_zonesMap.end())
    return iter->second;
  return shared_ptr<ClarisWksStruct::DSET>();
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void ClarisWksParser::newPage(int number)
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
// interface with the different parser
////////////////////////////////////////////////////////////
bool ClarisWksParser::canSendZoneAsGraphic(int zoneId) const
{
  if (m_state->m_zonesMap.find(zoneId) == m_state->m_zonesMap.end())
    return false;
  shared_ptr<ClarisWksStruct::DSET> zMap = m_state->m_zonesMap[zoneId];
  switch (zMap->m_fileType) {
  case 0:
    return m_document->getGraphParser()->canSendGroupAsGraphic(zoneId);
  case 1:
    return m_document->getTextParser()->canSendTextAsGraphic(zoneId);
  case 2:
    return m_document->getSpreadsheetParser()->canSendSpreadsheetAsGraphic(zoneId);
  case 3:
    return m_document->getDatabaseParser()->canSendDatabaseAsGraphic(zoneId);
  case 4:
    return m_document->getGraphParser()->canSendBitmapAsGraphic(zoneId);
  default:
    break;
  }
  return false;
}

bool ClarisWksParser::sendZone(int zoneId, bool asGraphic, MWAWPosition position)
{
  if (m_state->m_zonesMap.find(zoneId) == m_state->m_zonesMap.end())
    return false;
  shared_ptr<ClarisWksStruct::DSET> zMap = m_state->m_zonesMap[zoneId];
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  bool res = false;
  switch (zMap->m_fileType) {
  case 0:
    res = m_document->getGraphParser()->sendGroup(zoneId, asGraphic, position);
    break;
  case 1:
    res = m_document->getTextParser()->sendZone(zoneId, asGraphic);
    break;
  case 4:
    res = m_document->getGraphParser()->sendBitmap(zoneId, asGraphic, position);
    break;
  case 5:
    res = m_document->getPresentationParser()->sendZone(zoneId);
    break;
  case 6:
    res = m_document->getTableParser()->sendZone(zoneId);
    break;
  case 2:
    res = m_document->getSpreadsheetParser()->sendSpreadsheet(zoneId);
    break;
  case 3:
    res = m_document->getDatabaseParser()->sendDatabase(zoneId);
    break;
  default:
    MWAW_DEBUG_MSG(("ClarisWksParser::sendZone: can not send zone: %d\n", zoneId));
    break;
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  zMap->m_parsed = true;
  return res;
}

void ClarisWksParser::sendFootnote(int zoneId)
{
  if (!getTextListener()) return;

  MWAWSubDocumentPtr subdoc(new ClarisWksParserInternal::SubDocument(*this, getInput(), zoneId));
  getTextListener()->insertNote(MWAWNote(MWAWNote::FootNote), subdoc);
}

void ClarisWksParser::forceParsed(int zoneId)
{
  if (m_state->m_zonesMap.find(zoneId) == m_state->m_zonesMap.end())
    return;
  shared_ptr<ClarisWksStruct::DSET> zMap = m_state->m_zonesMap[zoneId];
  if (zMap) zMap->m_parsed = true;
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void ClarisWksParser::parse(librevenge::RVNGTextInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    // fixme: reset the real kind
    if (getHeader()) getHeader()->setKind(m_state->m_kind);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      MWAWPosition pos;
      //pos.m_anchorTo=MWAWPosition::Page;
      int headerId, footerId;
      m_document->getHeaderFooterId(headerId,footerId);
      for (size_t i = 0; i < m_state->m_mainZonesList.size(); i++) {
        // can happens if mainZonesList is not fully reconstruct
        if (m_state->m_mainZonesList[i]==headerId ||
            m_state->m_mainZonesList[i]==footerId)
          continue;
        sendZone(m_state->m_mainZonesList[i], false, pos);
      }
      m_document->getPresentationParser()->flushExtra();
      m_document->getGraphParser()->flushExtra();
      m_document->getTableParser()->flushExtra();
      m_document->getTextParser()->flushExtra();
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("ClarisWksParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetTextListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void ClarisWksParser::createDocument(librevenge::RVNGTextInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getTextListener()) {
    MWAW_DEBUG_MSG(("ClarisWksParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  MWAWPageSpan ps(getPageSpan());

  // decrease right | bottom
  if (ps.getMarginRight()>50./72.)
    ps.setMarginRight(ps.getMarginRight()-50./72.);
  else
    ps.setMarginRight(0);
  if (ps.getMarginBottom()>50./72.)
    ps.setMarginBottom(ps.getMarginBottom()-50./72.);
  else
    ps.setMarginBottom(0);

  int numPage = m_document->getTextParser()->numPages();
  if (m_document->getDatabaseParser()->numPages() > numPage)
    numPage = m_document->getDatabaseParser()->numPages();
  if (m_document->getPresentationParser()->numPages() > numPage)
    numPage = m_document->getPresentationParser()->numPages();
  if (m_document->getGraphParser()->numPages() > numPage)
    numPage = m_document->getGraphParser()->numPages();
  if (m_document->getSpreadsheetParser()->numPages() > numPage)
    numPage = m_document->getSpreadsheetParser()->numPages();
  if (m_document->getTableParser()->numPages() > numPage)
    numPage = m_document->getTableParser()->numPages();
  m_state->m_numPages = numPage;

  int headerId, footerId;
  m_document->getHeaderFooterId(headerId,footerId);
  for (int i = 0; i < 2; i++) {
    int zoneId = i == 0 ? headerId : footerId;
    if (zoneId == 0)
      continue;
    MWAWHeaderFooter hF((i==0) ? MWAWHeaderFooter::HEADER : MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    hF.m_subDocument.reset(new ClarisWksParserInternal::SubDocument(*this, getInput(), zoneId));
    ps.setHeaderFooter(hF);
  }
  ps.setPageSpan(m_state->m_numPages);
  std::vector<MWAWPageSpan> pageList(1,ps);
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
bool ClarisWksParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;

  long pos = input->tell();
  if (version() > 1)
    readEndTable();
  if (m_state->m_EOF > 0)
    input->pushLimit(m_state->m_EOF);

  input->seek(pos, librevenge::RVNG_SEEK_SET);

  if (m_document->readDocHeader() && m_document->readDocInfo()) {
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
    ascii().addPos(input->tell());
    f.str("");
    f << "Entries(Loose): vers=" << version();
    ascii().addNote(f.str().c_str());
  }
  // look for graphic
  while (!input->isEnd()) {
    pos = input->tell();
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
void ClarisWksParser::typeMainZones()
{
  // first type the main zone and its father
  typeMainZonesRec(1, ClarisWksStruct::DSET::T_Main, 100);

  std::map<int, shared_ptr<ClarisWksStruct::DSET> >::iterator iter;
  // then type the slides
  std::vector<int> slidesList = m_document->getPresentationParser()->getSlidesList();
  m_document->getGraphParser()->setSlideList(slidesList);
  for (size_t slide = 0; slide < slidesList.size(); slide++) {
    iter = m_state->m_zonesMap.find(slidesList[slide]);
    if (iter != m_state->m_zonesMap.end() && iter->second)
      iter->second->m_type = ClarisWksStruct::DSET::T_Slide;
  }
  // now check the header/footer
  int headerId, footerId;
  m_document->getHeaderFooterId(headerId,footerId);
  if (headerId) {
    iter = m_state->m_zonesMap.find(headerId);
    if (iter != m_state->m_zonesMap.end() && iter->second)
      iter->second->m_type = ClarisWksStruct::DSET::T_Header;
  }
  if (footerId) {
    iter = m_state->m_zonesMap.find(footerId);
    if (iter != m_state->m_zonesMap.end() && iter->second)
      iter->second->m_type = ClarisWksStruct::DSET::T_Footer;
  }
  iter = m_state->m_zonesMap.begin();
  std::vector<int> listZonesId[ClarisWksStruct::DSET::T_Unknown];
  while (iter != m_state->m_zonesMap.end()) {
    int id = iter->first;
    shared_ptr<ClarisWksStruct::DSET> node = iter++->second;
    ClarisWksStruct::DSET::Type type = node ? node->m_type : ClarisWksStruct::DSET::T_Unknown;
    if (type == ClarisWksStruct::DSET::T_Unknown || type == ClarisWksStruct::DSET::T_Main)
      continue;
    if (node->m_fileType != 1) // only propage data from a text node
      continue;
    if (type > ClarisWksStruct::DSET::T_Unknown || type < 0) {
      MWAW_DEBUG_MSG(("ClarisWksParser::typeMainZones: OOPS, internal problem with type\n"));
      continue;
    }
    listZonesId[type].push_back(id);
  }
  bool isPres = getHeader() && getHeader()->getKind() == MWAWDocument::MWAW_K_PRESENTATION;
  for (int type=ClarisWksStruct::DSET::T_Header; type < ClarisWksStruct::DSET::T_Slide;  type++) {
    for (size_t z = 0; z < listZonesId[type].size(); z++) {
      int fId = typeMainZonesRec(listZonesId[type][z], ClarisWksStruct::DSET::Type(type), 1);
      if (!fId)
        continue;
      if (isPres) // fixme: actually as the main type is not good too dangerous
        fId=listZonesId[type][z];
      if (type==ClarisWksStruct::DSET::T_Header && !headerId)
        m_document->setHeaderFooterId(fId,true);
      else if (type==ClarisWksStruct::DSET::T_Footer && !footerId)
        m_document->setHeaderFooterId(fId,false);
    }
  }
}

int ClarisWksParser::typeMainZonesRec(int zId, ClarisWksStruct::DSET::Type type, int maxHeight)
{
  if (maxHeight < 0) return 0;

  std::map<int, shared_ptr<ClarisWksStruct::DSET> >::iterator iter
    = m_state->m_zonesMap.find(zId);
  if (iter == m_state->m_zonesMap.end() || !iter->second)
    return 0;
  shared_ptr<ClarisWksStruct::DSET> node = iter->second;
  if (node->m_type == ClarisWksStruct::DSET::T_Unknown)
    node->m_type = type;
  else if (node->m_type != type)
    return 0;
  if (maxHeight==0)
    return zId;

  int res = zId;
  for (std::set<int>::iterator it = node->m_fathersList.begin();
       it != node->m_fathersList.end(); ++it) {
    int fId = typeMainZonesRec(*it, type, maxHeight-1);
    if (fId) res = fId;
  }
  return res;
}

////////////////////////////////////////////////////////////
// try to order the zones
////////////////////////////////////////////////////////////
bool ClarisWksParser::exploreZonesGraph()
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
          MWAW_DEBUG_MSG(("ClarisWksParser::exploreZonesGraph: find a zone with id=0\n"));
          continue;
        }

        iter2 = m_state->m_zonesMap.find(cId);
        if (iter2 == m_state->m_zonesMap.end()) {
          MWAW_DEBUG_MSG(("ClarisWksParser::exploreZonesGraph: can not find zone %d\n", cId));
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
      MWAW_DEBUG_MSG(("ClarisWksParser::exploreZonesGraph: find a cycle, choose new root %d\n", id));
      rootList.push_back(id);
    }
    exploreZonesGraphRec(id, notDoneList);
  }

  m_state->m_mainZonesList = rootList;
  size_t numMain = rootList.size();
  if (1 && numMain == 1)
    return true;
#ifdef DEBUG
  // we have do not have find the root note : probably a database...
  iter = m_state->m_zonesMap.begin();
  std::cerr << "--------------------------------------------------------\n";
  std::cerr << "List of potential main zones : ";
  for (size_t i = 0; i < numMain; i++)
    std::cerr << rootList[i] << ",";
  std::cerr << "\n";
  for (; iter != m_state->m_zonesMap.end(); ++iter) {
    shared_ptr<ClarisWksStruct::DSET> zone = iter->second;
    std::cerr << *zone << "\n";
  }
  std::cerr << "--------------------------------------------------------\n";
#endif
  if (numMain == 0) {
    // we have a big problem here, no way to continue
    MWAW_DEBUG_MSG(("ClarisWksParser::exploreZonesGraph: the graph contains no tree...\n"));
    return false;
  }


  return true;
}

bool ClarisWksParser::exploreZonesGraphRec(int zId, std::set<int> &notDoneList)
{
  std::map<int, shared_ptr<ClarisWksStruct::DSET> >::iterator iter, iter2;
  notDoneList.erase(zId);
  iter = m_state->m_zonesMap.find(zId);
  if (iter == m_state->m_zonesMap.end()) {
    MWAW_DEBUG_MSG(("ClarisWksParser::exploreZonesGraphRec: internal problem (can not find zone %d)\n", zId));
    return false;
  }
  shared_ptr<ClarisWksStruct::DSET> zone = iter->second;
  if (!zone) return true;
  zone->m_internal = 1;
  size_t numChilds = zone->m_childs.size();
  int headerId, footerId;
  m_document->getHeaderFooterId(headerId,footerId);
  for (int step = 0; step < 2; step++) {
    for (size_t c = 0; c < numChilds; c++) {
      int cId = step == 0 ? zone->m_childs[c].m_id : zone->m_otherChilds[c];
      if (cId <= 0) continue;
      if (notDoneList.find(cId) == notDoneList.end()) {
        iter2 = m_state->m_zonesMap.find(cId);
        if (iter2 == m_state->m_zonesMap.end()) {
          MWAW_DEBUG_MSG(("ClarisWksParser::exploreZonesGraph: can not find zone %d\n", cId));
        }
        else if (iter2->second->m_internal==1) {
          MWAW_DEBUG_MSG(("ClarisWksParser::exploreZonesGraph: find a cycle: for child : %d(<-%d)\n", cId, zId));
        }
        else if (cId != headerId && cId != footerId)
          zone->m_validedChildList.insert(cId);
      }
      else {
        if (cId != headerId && cId != footerId)
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
bool ClarisWksParser::readEndTable()
{
  if (version() <= 1) return false;

  MWAWInputStreamPtr input = getInput();

  // try to go to the end of file
  while (!input->isEnd())
    input->seek(10000, librevenge::RVNG_SEEK_CUR);

  m_state->m_EOF = input->tell();
  if (m_state->m_EOF < 20) // this is too short
    return false;
  input->seek(-20, librevenge::RVNG_SEEK_CUR);

  long entryPos= (long) input->readULong(4);
  if (entryPos >= m_state->m_EOF-20)
    return false;

  input->seek(entryPos, librevenge::RVNG_SEEK_SET);
  if (input->readULong(4) != 0x4554424c)
    return false;

  long sz = (long) input->readULong(4);
  if (sz <= 16 || (sz%8) != 0 || sz+entryPos+8 != m_state->m_EOF) {
    MWAW_DEBUG_MSG(("ClarisWksParser::readEndTable: bad size\n"));
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
      MWAW_DEBUG_MSG(("ClarisWksParser::readEndTable: bad pos\n"));
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
    }
    else if (entry.type() == "SNAP") {
      readSNAP(entry);
      parsed = true;
    }
    else if (entry.type() == "STYL") {
      m_document->m_styleManager->readStyles(entry);
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

    // WMBT: crypt password ? 0|fieldSz + PString ?
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

bool ClarisWksParser::readZone()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;

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
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return true;
  }
  MWAWEntry entry;
  entry.setBegin(pos);
  entry.setLength(4+sz);

  long actPos = input->tell();
  input->seek(sz, librevenge::RVNG_SEEK_CUR);
  if (long(input->tell()) != actPos+sz) return false;
  bool parsed = false;
  if (name.length()) {
    if (name == "DSET") {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      bool complete;
      if (readDSET(complete))
        return true;
    }
    if (name == "FNTM") {
      input->seek(pos+4, librevenge::RVNG_SEEK_SET);
      if (m_document->readStructZone("FNTM", true))
        return true;
    }
    if (name == "HDNI" && version() <= 4)
      sz = 2;
    f << "Entries(" << name << ")";
  }
  else {
    //
    input->seek(actPos, librevenge::RVNG_SEEK_SET);
    int firstOffset = (int) input->readULong(2);
    if (sz >= 16) {
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

  input->seek(actPos+sz, librevenge::RVNG_SEEK_SET);

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
bool ClarisWksParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = ClarisWksParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;
  libmwaw::DebugStream f;
  int const headerSize=8;
  input->seek(headerSize,librevenge::RVNG_SEEK_SET);
  if (int(input->tell()) != headerSize) {
    MWAW_DEBUG_MSG(("ClarisWksParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,librevenge::RVNG_SEEK_SET);
  f << "FileHeader:";
  int vers = (int) input->readLong(1);
  setVersion(vers);
  if (vers <=0 || vers > 6) {
    MWAW_DEBUG_MSG(("ClarisWksParser::checkHeader: unknown version: %d\n", vers));
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
  switch (vers) {
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
  input->seek(typePos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != typePos)
    return false;
  int type = (int) input->readULong(1);
  switch (type) {
  case 0:
    m_state->m_kind=MWAWDocument::MWAW_K_DRAW;
    break;
  case 1:
    m_state->m_kind=MWAWDocument::MWAW_K_TEXT;
    break;
  case 2:
    m_state->m_kind=MWAWDocument::MWAW_K_SPREADSHEET;
    break;
  case 3:
    m_state->m_kind=MWAWDocument::MWAW_K_DATABASE;
    break;
  case 4:
    m_state->m_kind=MWAWDocument::MWAW_K_PAINT;
    break;
  case 5:
    m_state->m_kind=MWAWDocument::MWAW_K_PRESENTATION;
    break;
  default:
    MWAW_DEBUG_MSG(("ClarisWksParser::checkHeader: unknown type=%d\n", type));
    m_state->m_kind=MWAWDocument::MWAW_K_UNKNOWN;
    break;
  }
  getParserState()->m_kind=m_state->m_kind;
  if (header) {
    header->reset(MWAWDocument::MWAW_T_CLARISWORKS, version());
    header->setKind(m_state->m_kind);
#ifdef DEBUG
    if (type >= 0 && type < 5)
      header->setKind(MWAWDocument::MWAW_K_TEXT);
#else
    if (type == 0 || type == 4)
      header->setKind(MWAWDocument::MWAW_K_TEXT);
#endif
  }

  if (strict && type > 5) return false;
#ifndef DEBUG
  if (type > 8) return false;
#endif
  input->seek(headerSize,librevenge::RVNG_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
// a document part
////////////////////////////////////////////////////////////
shared_ptr<ClarisWksStruct::DSET> ClarisWksParser::readDSET(bool &complete)
{
  complete = false;
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;
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
    MWAW_DEBUG_MSG(("ClarisWksParser::readDSET: file is too short\n"));
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
    res = m_document->getGraphParser()->readGroupZone(dset, entry, complete);
    break;
  case 1:
    res = m_document->getTextParser()->readDSETZone(dset, entry, complete);
    break;
  case 2:
    res = m_document->getSpreadsheetParser()->readSpreadsheetZone(dset, entry, complete);
    break;
  case 3:
    res = m_document->getDatabaseParser()->readDatabaseZone(dset, entry, complete);
    break;
  case 4:
    res = m_document->getGraphParser()->readBitmapZone(dset, entry, complete);
    break;
  case 5:
    res = m_document->getPresentationParser()->readPresentationZone(dset, entry, complete);
    break;
  case 6:
    res = m_document->getTableParser()->readTableZone(dset, entry, complete);
    break;
  default:
    parsed = false;
    break;
  }

  if (parsed) {
    if (!res)
      return shared_ptr<ClarisWksStruct::DSET>();
    if (m_state->m_zonesMap.find(res->m_id) != m_state->m_zonesMap.end()) {
      MWAW_DEBUG_MSG(("ClarisWksParser::readDSET: zone %d already exists!!!!\n",
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

  ascii().addDelimiter(input->tell(), '|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  if (sz-12 != data0Length*N + zone->m_headerSz) {
    MWAW_DEBUG_MSG(("ClarisWksParser::readDSET: unexpected size for zone definition, try to continue\n"));
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
      ascii().addDelimiter(input->tell(),'|');
    ascii().addPos(debPos);
    ascii().addNote(f.str().c_str());
    debPos += data0Length;
  }

  // in general, such a zone is followed by a small zone ( a container)
  zone->m_otherChilds.push_back(zone->m_id+1);
  if (m_state->m_zonesMap.find(zone->m_id) != m_state->m_zonesMap.end()) {
    MWAW_DEBUG_MSG(("ClarisWksParser::readDSET: zone %d already exists!!!!\n",
                    zone->m_id));
  }
  else
    m_state->m_zonesMap[zone->m_id] = zone;

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return zone;
}

///////////////////////////////////////////////////////////
// a list of snapshot
////////////////////////////////////////////////////////////
bool ClarisWksParser::readSNAP(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.type() != "SNAP")
    return false;
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos+4, librevenge::RVNG_SEEK_SET); // skip header
  long sz = (long) input->readULong(4);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("ClarisWksParser::readSNAP: pb with entry length"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
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
      MWAW_DEBUG_MSG(("ClarisWksParser::readSNAP: pb with sub zone: %d", id));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    f.str("");
    f << "SNAP-" << id++ << ":";
    if (type) f << "type=" << type;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

#ifdef DEBUG_WITH_FILES
    librevenge::RVNGBinaryData file;
    input->readDataBlock(sz, file);

    static int volatile snapName = 0;
    f.str("");
    f << "SNAP" << ++snapName << ".pct";
    libmwaw::Debug::dumpFile(file, f.str().c_str());

    if (type == 0)
      ascii().skipZone(pos+5,pos+5+sz-1);
#endif
    input->seek(pos+5+sz, librevenge::RVNG_SEEK_SET);
  }

  return true;
}

////////////////////////////////////////////////////////////
// a list the document property
////////////////////////////////////////////////////////////
bool ClarisWksParser::readDSUM(MWAWEntry const &entry, bool inHeader)
{
  if (!entry.valid() || (!inHeader && entry.type() != "DSUM"))
    return false;
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  long debStrings = inHeader ? pos : pos+8;
  input->seek(debStrings, librevenge::RVNG_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(DSUM):";
  for (int entete = 0; entete < 6; entete++) {
    char const *(entryNames[]) = { "Title",  "Category", "Description", "Author", "Version", "Keywords"};
    pos = input->tell();
    long sz = (int) input->readULong(4);
    if (!sz) continue;
    int strSize = (int) input->readULong(1);
    if (strSize != sz-1 || pos+4+sz > entry.end()) {
      MWAW_DEBUG_MSG(("ClarisWksParser::readDSUM: unexpected string size\n"));
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
      MWAW_DEBUG_MSG(("ClarisWksParser::readDSUM: unexpected string char\n"));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    if (name.length())
      f << entryNames[entete] << "=" << name << ",";
    input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
  }

  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// a string: temporary file name ?
////////////////////////////////////////////////////////////
bool ClarisWksParser::readTNAM(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.type() != "TNAM")
    return false;
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  long sz = entry.length()-8;
  input->seek(pos+8, librevenge::RVNG_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(TNAM):";

  int strSize = (int) input->readULong(1);
  if (strSize != sz-1 || pos+8+sz > entry.end()) {
    MWAW_DEBUG_MSG(("ClarisWksParser::readTNAM: unexpected string size\n"));
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
    MWAW_DEBUG_MSG(("ClarisWksParser::readTNAM: unexpected string char\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  if (name.length())
    f << name << ",";
  if (long(input->tell()) != entry.end()) {
    ascii().addDelimiter(input->tell(),'|');
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  }

  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  return true;
}

bool ClarisWksParser::readMARKList(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.type() != "MARK")
    return false;
  int const vers=version();
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  long sz = entry.length()-8;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(MARK)[header]:";

  if (input->readULong(4) !=0x4d41524b || input->readLong(4) != sz || sz < 30) {
    f << "###";
    MWAW_DEBUG_MSG(("ClarisWksParser::readMARKList: find unexpected header\n"));
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());

    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "MARK[MRKS]:";
  if (input->readULong(4)!=0x4d524b53) { // MRKS
    f << "###";

    MWAW_DEBUG_MSG(("ClarisWksParser::readMARKList: find unexpected MRKS header\n"));
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

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
  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
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
      MWAW_DEBUG_MSG(("ClarisWksParser::readMARKList: OOOPS reading mark data is not implemented\n"));
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      ascii().addPos(input->tell());
      ascii().addNote("MARK[End]:###");
      return false;
    }
    f << "f1=" << std::hex << input->readULong(2) << std::dec << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

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
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());

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

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

bool ClarisWksParser::readURL(long endPos)
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (pos+8>endPos) {
    return false;
  }
  libmwaw::DebugStream f;
  f << "MARK-URL:";
  long type=(long) input->readULong(4);
  if (type==0) {
  }
  else if (type!=0x554c6b64) {
    MWAW_DEBUG_MSG(("ClarisWksParser::readURL: find unexpected header\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  else { // ULkd
    if (input->tell()+32+256+8>endPos) {
      MWAW_DEBUG_MSG(("ClarisWksParser::readURL: date seems too short\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    for (int s=0; s < 2; ++s) {
      int const maxSize = s==0 ? 32: 256;
      long actPos=input->tell();
      int tSz=(int) input->readULong(1);
      if (tSz >= maxSize) {
        MWAW_DEBUG_MSG(("ClarisWksParser::readURL: find unexpected text size\n"));
        f << "###";
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        return false;
      }
      std::string text("");
      for (int c=0; c < tSz; ++c)
        text+=(char) input->readLong(1);
      f << text << ",";
      input->seek(actPos+maxSize, librevenge::RVNG_SEEK_SET);
    }
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return readEndMark(endPos);
}

bool ClarisWksParser::readDocumentMark(long endPos)
{
  // Checkme...
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (pos+8>endPos) {
    return false;
  }
  libmwaw::DebugStream f;
  f << "MARK-Document:";
  long type=(long) input->readULong(4);
  if (type==0) {
  }
  else if (type!=0x444c6b64) {
    MWAW_DEBUG_MSG(("ClarisWksParser::readDocumentMark: find unexpected header\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  else { // DLkd
    if (input->tell()+32+64+20+8>endPos) {
      MWAW_DEBUG_MSG(("ClarisWksParser::readDocumentMark: date seems too short\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    for (int s=0; s < 2; ++s) {
      int const maxSize = s==0 ? 32: 64;
      long actPos=input->tell();
      int tSz=(int) input->readULong(1);
      if (tSz >= maxSize) {
        MWAW_DEBUG_MSG(("ClarisWksParser::readDocumentMark: find unexpected text size\n"));
        f << "###";
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
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
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return readEndMark(endPos);
}

bool ClarisWksParser::readBookmark(long endPos)
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (pos+8>endPos) {
    return false;
  }
  libmwaw::DebugStream f;
  f << "MARK-URL:";
  long type=(long) input->readULong(4);
  if (type==0) {
  }
  else if (type!=0x424d6b64) {
    MWAW_DEBUG_MSG(("ClarisWksParser::readBookmark: find unexpected header\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  else { // BMkd
    if (input->tell()+32+8>endPos) {
      MWAW_DEBUG_MSG(("ClarisWksParser::readBookmark: date seems too short\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    int const maxSize = 32;
    long actPos=input->tell();
    int tSz=(int) input->readULong(1);
    if (tSz >= maxSize) {
      MWAW_DEBUG_MSG(("ClarisWksParser::readBookmark: find unexpected text size\n"));
      f << "###";
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return false;
    }
    std::string text("");
    for (int c=0; c < tSz; ++c)
      text+=(char) input->readLong(1);
    f << text << ",";
    input->seek(actPos+maxSize, librevenge::RVNG_SEEK_SET);
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return readEndMark(endPos);
}

bool ClarisWksParser::readEndMark(long endPos)
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;
  f << "MARK[Last]:";
  long val=input->readLong(4);
  if (!val) {
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
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
    MWAW_DEBUG_MSG(("ClarisWksParser::readEndMark: find unexpected number of element\n"));
    f << "###";
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  f << "unkn=[";
  for (int i=0; i<numExpected; ++i)
    f << input->readLong(2) << ",";
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// a list of print info plist
////////////////////////////////////////////////////////////
bool ClarisWksParser::readCPRT(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.type() != "CPRT")
    return false;
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos+4, librevenge::RVNG_SEEK_SET); // skip header
  long sz = (long) input->readULong(4);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("ClarisWksParser::readCPRT: pb with entry length"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
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
      MWAW_DEBUG_MSG(("ClarisWksParser::readCPRT: pb with sub zone: %d", id));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    f.str("");
    f << "CPRT-" << id++ << ":";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    if (!sz) continue;
#ifdef DEBUG_WITH_FILES
    librevenge::RVNGBinaryData file;
    input->readDataBlock(sz, file);

    static int volatile cprtName = 0;
    f.str("");
    f << "CPRT" << ++cprtName << ".plist";
    libmwaw::Debug::dumpFile(file, f.str().c_str());

    ascii().skipZone(pos+4,pos+4+sz-1);
#endif
    input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
  }

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
