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

#include <librevenge/librevenge.h>

#include "MWAWCell.hxx"
#include "MWAWTextListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "FullWrtGraph.hxx"
#include "FullWrtStruct.hxx"
#include "FullWrtText.hxx"

#include "FullWrtParser.hxx"

/** Internal: the structures of a FullWrtParser */
namespace FullWrtParserInternal
{
//! Internal and low level: a structure used to define the list of zone in Zone 0 data of a FullWrite file
struct DocZoneStruct {
  //! constructor
  DocZoneStruct() : m_pos(-1), m_structType(0), m_type(-1), m_nextId(0), m_fatherId(-1), m_childList() {}
  //! the operator<<
  friend std::ostream &operator<<(std::ostream &o, DocZoneStruct const &dt)
  {
    switch (dt.m_structType) {
    case 0:
      o << "empty,";
      break;
    case 1: // normal
      break;
    case 4: // hidden?
      o << "hidden,";
      break;
    default:
      o << "#structType=" << dt.m_structType << ",";
      break;
    }
    if (dt.m_type!=-1)
      o << FullWrtStruct::getTypeName(dt.m_type);
    if (dt.m_nextId) o << "nId=" << dt.m_nextId << ",";
    if (dt.m_fatherId>=0) o << "fId=" << dt.m_fatherId << ",";
    if (dt.m_childList.size()) {
      o << "childs=[";
      for (size_t i = 0; i < dt.m_childList.size(); i++)
        o << dt.m_childList[i] << ",";
      o << "],";
    }
    return o;
  }
  //! the file position
  long m_pos;
  //! the struct type
  int m_structType;
  //! the type
  int m_type;
  //! the next id
  int m_nextId;
  //! the father id
  int m_fatherId;
  //! the list of child id
  std::vector<int> m_childList;
};

////////////////////////////////////////
//! Internal: the sidebar of a FullWrtParser
struct SideBar : public FullWrtStruct::ZoneHeader {
  //! constructor
  SideBar(FullWrtStruct::ZoneHeader &): m_box(), m_page(0)
  {
  }
  //! the position (in point)
  Box2f m_box;
  //! the page
  int m_page;
};

////////////////////////////////////////
//! Internal: the reference data call of a FullWrtParser
struct ReferenceCalledData {
  // constructor
  ReferenceCalledData() : m_id(-1)
  {
    for (int i = 0; i < 5; i++) m_values[i] = 0;
  }
  //! the operator<<
  friend std::ostream &operator<<(std::ostream &o, ReferenceCalledData const &dt)
  {
    if (dt.m_id >= 0) o << "refId=" << dt.m_id << ",";
    for (int i = 0; i < 5; i++) {
      if (dt.m_values[i])
        o << "f" << i << "=" << dt.m_values[i] << ",";
    }
    return o;
  }
  //! the reference id
  int m_id;
  //! some unknown values
  int m_values[5];
};

////////////////////////////////////////
//! Internal: the state of a FullWrtParser
struct State {
  //! constructor
  State() : m_pageSpanSet(false), m_fileZoneList(), m_fileZoneFlagsList(), m_docZoneList(), m_docFileIdMap(), m_fileDocIdMap(),
    m_biblioId(-1), m_entryMap(), m_variableRedirectMap(), m_referenceRedirectMap(),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
    for (int i=0; i < 3; i++) m_zoneFlagsId[i] = -1;
  }

  //! insert a docId fileId in the correspondance map
  bool addCorrespondance(int docId, int fileId)
  {
    if (m_docFileIdMap.find(docId) != m_docFileIdMap.end() ||
        m_fileDocIdMap.find(fileId) != m_fileDocIdMap.end()) {
      MWAW_DEBUG_MSG(("FullWrtParserInternal::State::addCorrespondance can not insert %d<->%d\n", docId, fileId));
      return false;
    }
    m_fileDocIdMap[fileId]=docId;
    m_docFileIdMap[docId]=fileId;
    // update the zone type ( if possible )
    if (docId >= 0 && docId < int(m_docZoneList.size()) &&
        m_entryMap.find(fileId) != m_entryMap.end() &&
        m_entryMap.find(fileId)->second)
      m_entryMap.find(fileId)->second->m_type = m_docZoneList[size_t(docId)].m_type;
    else {
      MWAW_DEBUG_MSG(("FullWrtParserInternal::State::addCorrespondance can not update the zone type for %d<->%d\n", docId, fileId));
    }
    return true;
  }
  //! return the file zone id ( if found or -1)
  int getFileZoneId(int docId) const
  {
    std::map<int,int>::const_iterator it = m_docFileIdMap.find(docId);
    if (it == m_docFileIdMap.end()) {
      MWAW_DEBUG_MSG(("FullWrtParserInternal::State::getFileZoneId can not find %d\n", docId));
      return -1;
    }
    return it->second;
  }
  //! a flag to know if the page span has been filled
  bool m_pageSpanSet;
  //! the list of main zone flags id
  int m_zoneFlagsId[3];

  //! the list of file zone position
  FullWrtStruct::EntryPtr m_fileZoneList;

  //! the list of file zone flags
  FullWrtStruct::EntryPtr m_fileZoneFlagsList;

  //! the list of the documents zone list
  std::vector<DocZoneStruct> m_docZoneList;

  //! the correspondance doc id -> file id
  std::map<int,int> m_docFileIdMap;

  //! the correspondance file id -> doc id
  std::map<int,int> m_fileDocIdMap;

  //! the bibliography id
  int m_biblioId;

  //! zoneId -> entry
  std::multimap<int, FullWrtStruct::EntryPtr > m_entryMap;

  //! redirection docId -> variable docId
  std::map<int,int> m_variableRedirectMap;

  //! redirection docId -> reference docId
  std::map<int,ReferenceCalledData> m_referenceRedirectMap;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a FullWrtParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(FullWrtParser &pars, MWAWInputStreamPtr input, int zoneId) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

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
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_parser);

  long pos = m_input->tell();
  static_cast<FullWrtParser *>(m_parser)->send(m_id);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
FullWrtParser::FullWrtParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWTextParser(input, rsrcParser, header), m_state(), m_graphParser(), m_textParser()
{
  init();
}

FullWrtParser::~FullWrtParser()
{
  std::multimap<int, FullWrtStruct::EntryPtr >::iterator it;
  for (it = m_state->m_entryMap.begin(); it != m_state->m_entryMap.end(); ++it) {
    FullWrtStruct::EntryPtr zone = it->second;
    if (zone) zone->closeDebugFile();
  }
}

void FullWrtParser::init()
{
  resetTextListener();
  setAsciiName("main-1");

  m_state.reset(new FullWrtParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);

  m_graphParser.reset(new FullWrtGraph(*this));
  m_textParser.reset(new FullWrtText(*this));
}

////////////////////////////////////////////////////////////
// new page, interface function
////////////////////////////////////////////////////////////
void FullWrtParser::newPage(int number)
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

Vec2f FullWrtParser::getPageLeftTop() const
{
  return Vec2f(float(getPageSpan().getMarginLeft()),
               float(getPageSpan().getMarginTop()+m_state->m_headerHeight/72.0));
}

bool FullWrtParser::getBorder(int bId, FullWrtStruct::Border &border) const
{
  return m_graphParser->getBorder(bId, border);
}

int FullWrtParser::getNumDocZoneStruct() const
{
  return int(m_state->m_docZoneList.size());
}

std::string FullWrtParser::getDocumentTypeName(int id) const
{
  if (id<0||id >= int(m_state->m_docZoneList.size()))
    return "";
  return FullWrtStruct::getTypeName(m_state->m_docZoneList[size_t(id)].m_type);
}

void FullWrtParser::sendGraphic(int id)
{
  if (id < 0 || id >= int(m_state->m_docZoneList.size())) {
    MWAW_DEBUG_MSG(("FullWrtParser::sendGraphic: can not find graphic data for zone %d\n", id));
  }
  else {
    FullWrtParserInternal::DocZoneStruct const &data =
      m_state->m_docZoneList[size_t(id)];
    if (data.m_type != 0x15) {
      MWAW_DEBUG_MSG(("FullWrtParser::sendGraphic: call for zone[%x]\n", data.m_type));
    }
  }
  m_graphParser->sendGraphic(m_state->getFileZoneId(id));
}

bool FullWrtParser::send(int fileId, MWAWColor fontColor)
{
  if (fileId < 0) {
    if (getTextListener()) getTextListener()->insertChar(' ');
    return true;
  }
  return m_textParser->send(fileId, fontColor);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void FullWrtParser::parse(librevenge::RVNGTextInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = true;
  try {
#ifdef DEBUG
    // just test if a rsrc exists
    if (getRSRCParser()) {
      MWAW_DEBUG_MSG(("FullWrtParser::parse: find a ressource fork\n"));
      getRSRCParser()->getEntry("STR ", 700);
    }
#endif
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      m_graphParser->sendPageGraphics();
      m_textParser->sendMainText();
      m_graphParser->flushExtra();
#ifdef DEBUG
      m_textParser->flushExtra();
#endif
    }
    bool first = true;
    std::multimap<int, FullWrtStruct::EntryPtr >::iterator it;
    libmwaw::DebugStream f;
    for (it = m_state->m_entryMap.begin(); it != m_state->m_entryMap.end(); ++it) {
      FullWrtStruct::EntryPtr &zone = it->second;
      if (!zone || !zone->valid() || zone->isParsed()) continue;
      f.str("");
      if (zone->hasType("UnknownZone"))
        f << "Entries(NotParsed)";
      else
        f << "Entries(" << zone->type() << ")";
      if (zone->hasType("Biblio")) {
        MWAW_DEBUG_MSG(("FullWrtParser::parse: find some biblio zone unparsed!!!\n"));
      }
      else if (first) {
        f << "###";
        first = false;
        MWAW_DEBUG_MSG(("FullWrtParser::parse: find some unparsed zone!!!\n"));
      }
      if (zone->m_nextId != -2) f << "[" << zone->m_nextId << "]";
      f << "|" << *zone << ":";
      libmwaw::DebugFile &asciiFile = zone->getAsciiFile();

      asciiFile.addPos(zone->begin());
      asciiFile.addNote(f.str().c_str());
      asciiFile.addPos(zone->end());
      asciiFile.addNote("_");

      zone->closeDebugFile();
    }

    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("FullWrtParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetTextListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void FullWrtParser::createDocument(librevenge::RVNGTextInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getTextListener()) {
    MWAW_DEBUG_MSG(("FullWrtParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;
  int numPage = m_textParser->numPages();
  if (m_graphParser->numPages()>numPage)
    numPage=m_graphParser->numPages();
  m_state->m_numPages = numPage;

  // create the page list
  int actHeaderId = -1, actFooterId = -1;
  shared_ptr<FullWrtParserInternal::SubDocument> headerSubdoc, footerSubdoc;
  std::vector<MWAWPageSpan> pageList;
  for (int i = 0; i < m_state->m_numPages;) {
    int numSim[2]= {1,1};
    int headerId =  m_textParser->getHeaderFooterId(true,i+1, numSim[0]);
    if (headerId != actHeaderId) {
      actHeaderId = headerId;
      if (actHeaderId == -1)
        headerSubdoc.reset();
      else
        headerSubdoc.reset
        (new FullWrtParserInternal::SubDocument(*this, getInput(), headerId));
    }
    int footerId =  m_textParser->getHeaderFooterId(false, i+1, numSim[1]);
    if (footerId != actFooterId) {
      actFooterId = footerId;
      if (actFooterId == -1)
        footerSubdoc.reset();
      else
        footerSubdoc.reset
        (new FullWrtParserInternal::SubDocument(*this, getInput(), footerId));
    }

    MWAWPageSpan ps(getPageSpan());
    if (headerSubdoc) {
      MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
      header.m_subDocument=headerSubdoc;
      ps.setHeaderFooter(header);
    }
    if (footerSubdoc) {
      MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
      footer.m_subDocument=footerSubdoc;
      ps.setHeaderFooter(footer);
    }
    if (numSim[1] < numSim[0]) numSim[0]=numSim[1];
    if (numSim[0] < 1) numSim[0]=1;
    ps.setPageSpan(numSim[0]);
    i+=numSim[0];
    pageList.push_back(ps);
  }

  // retrieve the header/footer
  MWAWTextListenerPtr listen(new MWAWTextListener(*getParserState(), pageList, documentInterface));
  setTextListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool FullWrtParser::createFileZones()
{
  /* FileZonePos: define a list of zone in the file and a link between these zone
     FileZoneFlags: define the list of contents zone and link each to the first file zone
   */

  // first creates the zone (entry are mapped by number of the zones in the file)
  if (m_state->m_fileZoneList)
    readFileZonePos(m_state->m_fileZoneList);
  // update each main filezone, ie. move appart the 3 main zone and sets their final ids
  if (m_state->m_fileZoneFlagsList)
    readFileZoneFlags(m_state->m_fileZoneFlagsList);

  // finally, remapped the enry by fId
  std::multimap<int, FullWrtStruct::EntryPtr >::iterator it;
  std::vector<FullWrtStruct::EntryPtr > listZones;
  for (it = m_state->m_entryMap.begin(); it != m_state->m_entryMap.end(); ++it)
    listZones.push_back(it->second);
  m_state->m_entryMap.clear();
  for (size_t z = 0; z < listZones.size(); z++) {
    FullWrtStruct::EntryPtr &entry = listZones[z];
    if (!entry->valid() || entry->isParsed()) continue;
    int fId = entry->id();
    if (entry->m_typeId == -1) fId=-fId-1;
    if (m_state->m_entryMap.find(fId) != m_state->m_entryMap.end()) {
      MWAW_DEBUG_MSG(("FullWrtParser::createFileZones: can not find generic zone id %d\n",int(z)));
    }
    else
      m_state->m_entryMap.insert
      (std::multimap<int, FullWrtStruct::EntryPtr >::value_type(fId, entry));
  }
  return true;
}

bool FullWrtParser::createZones()
{
  createFileZones();

  std::multimap<int, FullWrtStruct::EntryPtr >::iterator it;
  // first treat the main zones
  std::vector<FullWrtStruct::EntryPtr > mainZones;
  mainZones.resize(3);
  for (it = m_state->m_entryMap.begin(); it != m_state->m_entryMap.end(); ++it) {
    FullWrtStruct::EntryPtr &zone = it->second;
    if (!zone || !zone->valid() || zone->isParsed()) continue;
    if (zone->m_typeId != -1 || zone->id() < 0 || zone->id() >= 3)
      continue;
    size_t zId = size_t(zone->id());
    if (mainZones[size_t(zId)]) {
      MWAW_DEBUG_MSG(("FullWrtParser::createZones: Oops main zone %d already founded\n", int(zId)));
      continue;
    }
    mainZones[zId] = zone;
  }
  if (!mainZones[1] || !readDocZoneStruct(mainZones[1])) {
    MWAW_DEBUG_MSG(("FullWrtParser::createZones: can not read the docZoneStruct zone\n"));
  }
  if (!mainZones[0] || !readDocZoneData(mainZones[0])) {
    MWAW_DEBUG_MSG(("FullWrtParser::createZones: can not read the docZoneData zone\n"));
  }
  if (!mainZones[2] || !readDocInfo(mainZones[2])) {
    MWAW_DEBUG_MSG(("FullWrtParser::createZones: can not read the document information zone\n"));
  }

  // now treat the other zones
  for (it = m_state->m_entryMap.begin(); it != m_state->m_entryMap.end(); ++it) {
    FullWrtStruct::EntryPtr &zone = it->second;
    if (!zone || !zone->valid() || zone->isParsed()) continue;
    if (zone->m_typeId >= 0) {
      // first use the zone type
      bool done = false;
      switch (zone->m_type) {
      case 0x15:
        done = m_graphParser->readGraphic(zone);
        break;
      case 0xa:
      case 0xb:
      case 0xc:
      case 0xd:
      case 0xe:
      case 0xf:
      case 0x10:
      case 0x11:
      case 0x12:
      case 0x13:
      case 0x14:
      case 0x18:
        done = m_textParser->readTextData(zone);
        break;
      default:
        break;
      }
      if (done) continue;

      // unknown, so try all possibilities
      if (m_graphParser->readGraphic(zone)) continue;
      if (m_textParser->readTextData(zone)) continue;
    }
    else if (zone->m_typeId == -1) {
      if (zone->id()>=0 && zone->id()< 3) {
        MWAW_DEBUG_MSG(("FullWrtParser::createZones: Oops find an unparsed main zone %d\n", zone->id()));
      }
      else if (zone->hasType("Biblio")) {
        MWAW_DEBUG_MSG(("FullWrtParser::createZones: find a bibliography zone: unparsed\n"));
      }
      else {
        MWAW_DEBUG_MSG(("FullWrtParser::createZones: find unexpected general zone\n"));
      }
    }
  }
  m_textParser->prepareData();
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
bool FullWrtParser::checkHeader(MWAWHeader *header, bool /*strict*/)
{
  *m_state = FullWrtParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;

  int const minSize=50;
  input->seek(minSize,librevenge::RVNG_SEEK_SET);
  if (int(input->tell()) != minSize) {
    MWAW_DEBUG_MSG(("FullWrtParser::checkHeader: file is too short\n"));
    return false;
  }

  if (!readDocPosition())
    return false;

  input->seek(0,librevenge::RVNG_SEEK_SET);

  if (header)
    header->reset(MWAWDocument::MWAW_T_FULLWRITE, 1);

  return true;
}

bool FullWrtParser::readDocInfo(FullWrtStruct::EntryPtr zone)
{
  if (zone->length() < 0x4b2)
    return false;
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->getAsciiFile();
  libmwaw::DebugStream f;

  long pos = zone->begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int N = (int)input->readLong(2);
  if (!N) return false;
  input->seek(4, librevenge::RVNG_SEEK_CUR);
  int val = (int)input->readLong(2);
  if (N < val-2 || N > val+2) return false;
  input->seek(2, librevenge::RVNG_SEEK_CUR);
  int nC = (int)input->readULong(1);
  if (!nC || nC > 70) return false;
  for (int i = 0; i < nC; i++) {
    int c = (int)input->readULong(1);
    if (c < 0x20) return false;
  }

  zone->setParsed(true);
  input->seek(pos+2, librevenge::RVNG_SEEK_SET);
  f << "Entries(DocInfo)|" << *zone << ":";
  f << "N0=" << N << ",";
  f << "unkn0=" << std::hex << input->readULong(2) << std::dec << ","; // big number
  for (int i = 0; i < 2; i++) { // 0|1|a7|ff followed by a small number
    val = (int)input->readULong(1);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  val = (int)input->readLong(2); // almost always N
  if (val != N) f << "N1=" << val << ",";
  f << "unkn1=" << std::hex << input->readULong(2) << std::dec << ","; // big number
  std::string title("");
  nC = (int)input->readULong(1);
  for (int i = 0; i < nC; i++)
    title += (char) input->readLong(1);
  f << "title=" << title << ",";
  asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  pos += 10+70; // checkme
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f.str("");
  f << "DocInfo-A:";
  for (int i = 0; i < 4; i++) { // 0, 1, 2, 80
    val = (int)input->readULong(1);
    if (val)
      f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  val = (int)input->readLong(2); // almost always 0, but find also 53, 64, ba, f4
  if (val) f << "f0=" << val << ",";
  f << "unkn=["; // 2 big number which seems realted
  for (int i = 0; i < 2; i++)
    f << std::hex << input->readULong(4) << std::dec << ",";
  f << "],";
  for (int i = 4; i < 6; i++) { // always 1, 0 ?
    val = (int)input->readULong(1);
    if (val!=5-i) f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  // 5 relative big number : multiple of 4
  long ptr = (long) input->readULong(4);
  f << "unkn2=[" << std::hex << ptr << "," << std::dec;
  for (int i = 0; i < 4; i++)
    f << long(input->readULong(4))-ptr << ",";
  f << "],";
  val = (int)input->readLong(2); // almost always -2, but find also 6
  if (val != -2)
    f << "f1=" << val << ",";
  for (int i = 0; i < 2; i++) { // always 0,0
    val = (int)input->readULong(2);
    if (val) f << "f" << i+2 << "=" << std::hex << val << std::dec << ",";
  }
  for (int st=0; st < 2; st++) {
    long actPos = input->tell();
    val = (int)input->readULong(2); // big number
    if (val) f << "g" << st << "=" << std::hex << val << std::dec << ",";

    nC = (int)input->readULong(1);
    if (nC > 32) {
      MWAW_DEBUG_MSG(("FullWrtParser::readDocInfo: can not read user name\n"));
      nC = 0;
    }
    std::string s("");
    for (int i = 0; i < nC; i++) s+=char(input->readLong(1));
    if (nC)
      f << "Username" << st << "=" << s << ",";
    input->seek(actPos+36, librevenge::RVNG_SEEK_SET);
  }

  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  pos = input->tell();
  asciiFile.addPos(pos);
  asciiFile.addNote("DocInfo-B");
  pos+=196;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i = 0; i < 6; i++) {
    pos = input->tell();
    f.str("");
    f << "DocInfo-C" << i << ":" << input->readLong(2);

    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+48, librevenge::RVNG_SEEK_SET);
  }
  for (int i = 0; i < 9; i++) {
    pos = input->tell();
    f.str("");
    f << "DocInfo-D" << i << ":";

    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+38, librevenge::RVNG_SEEK_SET);
  }
  pos = input->tell();
  asciiFile.addPos(pos);
  asciiFile.addNote("DocInfo-E");
  input->seek(pos+182, librevenge::RVNG_SEEK_SET);

  // this part in v1 and v2
  pos = input->tell();
  f.str("");
  f << "DocInfo-F:";
  bool ok = true;
  if (version()==2) {
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+436, librevenge::RVNG_SEEK_SET);
    pos = input->tell();
    if (!readEndDocInfo(zone)) {
      asciiFile.addPos(pos);
      asciiFile.addNote("DocInfoII#");
      ok = false;
    }
  }
  else {
    if (pos+18 < zone->end()) {
      val = (int)input->readLong(2); // always 0
      if (val) f << "f0=" << val << ",";
      val = (int)input->readLong(4); // can be a big number (often neg )
      if (val) f << "f1=" << val << ",";
      // always 0 except one time g0=a,g1=1d,g5=50fe
      for (int i = 0; i < 6; i++) {
        val = (int)input->readULong(2);
        if (val) f << "g" << i << "=" << std::hex << val << std::dec << ",";
      }
    }
    else {
      ok = false;
      f << "#";
    }
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
  }

  if (ok) {
    pos=input->tell();
    f.str("");
    f << "DocInfo-G:";
    int type=(int) input->readULong(2);
    f << "f0=" << std::hex << type << std::dec << ",";
    int dim[4];
    for (int i=0; i < 4; ++i)
      dim[i]=(int) input->readLong(2);
    f << "dim=" << dim[1] << "x" << dim[0] << "<->"
      << dim[3] << "x" << dim[2] << ",";
    // checkme: 3: followed by 2 int, 6-b: followed by 3 int, e-f: by 4, other?
    int num=(type&0xF000)>=0xE000 ? 4:(type&0xF000)>0x3000 ? 3:2;
    for (int i=0; i< num; ++i) { // f1=0x100, f3=3
      val=(int) input->readLong(2);
      if (val) f << "f" << i+1 << "=" << val << ",";
    }
    for (int st=0; st<2; ++st) { // probably left/right page
      int margins[4];
      for (int i=0; i < 4; ++i)
        margins[i]=(int) input->readLong(2);
      f << "pageDim" << st << "=" << margins[1] << "x" << margins[0] << "<->"
        << margins[3] << "x" << margins[2] << ",";
      if (st==1)
        break;
      if (margins[0]<margins[2] && margins[2]<dim[2] && 2*(margins[2]-margins[0]) > dim[2] &&
          margins[1]<margins[3] && margins[3]<dim[3] && 2*(margins[3]-margins[1]) > dim[3] &&
          dim[2]>100 && dim[2]<2000 && dim[3]>100 && dim[3]<2000) {
        getPageSpan().setMarginTop(double(margins[0])/72.0);
        getPageSpan().setMarginBottom(double(dim[2]-margins[2])/72.0);
        getPageSpan().setMarginLeft(double(margins[1])/72.0);
        getPageSpan().setMarginRight(double(dim[3]-margins[3])/72.0);
        getPageSpan().setFormLength(double(dim[2])/72.);
        getPageSpan().setFormWidth(double(dim[3])/72.);
        m_state->m_pageSpanSet=true;
      }
      else {
        MWAW_DEBUG_MSG(("FullWrtParser::readDocInfo:can not read document margins!\n"));
      }
    }
    asciiFile.addDelimiter(input->tell(),'|');

    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    /* seems to end with
       00000009000043686f6f73657200
       000000100000436f6e74726f6c2050616e656c7300
       0000000b000046696e642046696c6500
       0000001500004772617068696e672043616c63756c61746f7200
       0000000f00004a69677361772050757a7a6c6500
       0000000a00004b6579204361707300
       0000000a00004e6f74652050616400
       0000001500000000000000000000000000006361000000000000
    */
  }
  // try to retrieve the last part: printInfo+3 int
  input->seek(zone->end()-130, librevenge::RVNG_SEEK_SET);
  if (readPrintInfo(zone)) {
    pos = input->tell();
    f.str("");
    f << "DocInfo-End:";
    if (pos == zone->end()-6) {
      // f0=0, f1=0|1(in v2) but can also be a big number in v1, f2=0|1a
      for (int i = 0; i < 3; i++) {
        val = (int)input->readLong(2);
        if (val) f << "f" << i << "=" << val << ",";
      }
    }
    else
      f << "#";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
  }
  else {
    MWAW_DEBUG_MSG(("FullWrtParser::readDocInfo: can not find print info\n"));
    asciiFile.addPos(zone->end()-130);
    asciiFile.addNote("DocInfo-G#");
  }

  zone->closeDebugFile();
  return true;
}

////////////////////////////////////////////////////////////
// read the end of the zone data
bool FullWrtParser::readEndDocInfo(FullWrtStruct::EntryPtr zone)
{
  if (version() < 2)
    return false;

  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->getAsciiFile();
  libmwaw::DebugStream f;

  // at least 4 possible zones, maybe more...
  for (int i = 0; i < 5; i++) {
    long pos = input->tell();
    bool ok = true;
    std::string name("");
    for (int j = 0; j < 4; j++) {
      int val=int(input->readULong(1));
      if (val < 9) {
        ok = false;
        break;
      }
      name+=char(val);
    }
    if (!ok || input->readULong(1)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    ok = false;
    if (name=="font") // block0 : unseen
      ;
    else if (name=="bord")
      ok = m_graphParser->readBorderDocInfo(zone);
    else if (name=="extr")
      ok = m_textParser->readParaModDocInfo(zone);
    else if (name=="cite") // block3
      ok = readCitationDocInfo(zone);

    if (ok)
      continue;

    MWAW_DEBUG_MSG(("FullWrtParser::readEndDocInfo: can not read block %s\n", name.c_str()));
    input->seek(pos+5, librevenge::RVNG_SEEK_SET);
    long blckSz = input->readLong(4);
    if (blckSz < 2 || pos+8+blckSz > zone->end()) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    int num=int(input->readULong(2));
    f.str("");
    f << "Entries(Doc" << name << "):N=" << num << ",###";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+9+blckSz, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

bool FullWrtParser::readCitationDocInfo(FullWrtStruct::EntryPtr zone)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->getAsciiFile();
  libmwaw::DebugStream f;
  long pos = input->tell();
  if (input->readULong(4)!=0x63697465 || input->readULong(1)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  long blckSz = input->readLong(4);
  long endData = pos+9+blckSz;
  int num = (int) input->readULong(2), val;
  f << "Entries(RefValues):N=" << num << ",";
  if (blckSz <= 2 || endData > zone->end() || pos+num > endData) {
    MWAW_DEBUG_MSG(("FullWrtParser::readCitationDocInfo: problem reading the data block or the number of data\n"));
    f << "###";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    if (endData <= zone->end()) {
      input->seek(endData, librevenge::RVNG_SEEK_SET);
      return true;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  for (int i = 0; i < num; i++) {
    f.str("");
    f << "RefValues-" << i << ",";
    pos = input->tell();
    int sz = int(input->readULong(1));
    if (input->tell()+sz > endData)
      break;
    std::string name("");
    bool ok = true;
    for (int j = 0; j < sz; j++) {
      val = int(input->readULong(1));
      if (val < 0x9) {
        ok = false;
        break;
      }
      name+=char(val);
    }
    if (!ok) break;
    f << "\"" << name << "\",";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
  }
  if (input->tell() != endData) {
    f.str("");
    f << "RefValues-##";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(endData, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool FullWrtParser::readPrintInfo(FullWrtStruct::EntryPtr zone)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->getAsciiFile();

  long pos = input->tell();
  if (input->readULong(2) != 0) return false;
  long sz = (long) input->readULong(2);
  if (sz != 0x78)
    return false;
  long endPos = pos+4+sz;
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("FullWrtParser::readPrintInfo: file is too short\n"));
    return false;
  }
  input->seek(pos+4, librevenge::RVNG_SEEK_SET);

  libmwaw::DebugStream f;
  // print info
  libmwaw::PrinterInfo info;
  if (!info.read(input)) {
    if (sz == 0x78) {
      // the size is ok, so let try to continue
      asciiFile.addPos(pos);
      asciiFile.addNote("Entries(PrintInfo):##");
      input->seek(endPos, librevenge::RVNG_SEEK_SET);
      MWAW_DEBUG_MSG(("FullWrtParser::readPrintInfo: can not read print info, continue\n"));
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

    // decrease right | bottom
    int rightMarg = rBotMargin.x() -50;
    if (rightMarg < 0) rightMarg=0;
    int botMarg = rBotMargin.y() -50;
    if (botMarg < 0) botMarg=0;

    getPageSpan().setMarginTop(lTopMargin.y()/72.0);
    getPageSpan().setMarginBottom(botMarg/72.0);
    getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
    getPageSpan().setMarginRight(rightMarg/72.0);
    getPageSpan().setFormLength(paperSize.y()/72.);
    getPageSpan().setFormWidth(paperSize.x()/72.);
  }
  if (long(input->tell()) !=endPos) {
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    f << ", #endPos";
    asciiFile.addDelimiter(input->tell(), '|');
  }

  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// read the document zone data
bool FullWrtParser::readDocZoneData(FullWrtStruct::EntryPtr zone)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->getAsciiFile();
  libmwaw::DebugStream f;
  //  int vers = version();

  long pos = zone->begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  zone->setParsed(true);
  f << "Entries(DZoneData)|" << *zone << ":";
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  int val, prevTypeOk=-1;
  // first try to read normally the zone
  size_t numDocTypes = m_state->m_docZoneList.size();
  for (size_t z = 1; z < m_state->m_docZoneList.size(); z++) {
    FullWrtParserInternal::DocZoneStruct const &doc = m_state->m_docZoneList[z];
    if (doc.m_type < 0) continue;

    pos = input->tell();
    if (pos+2 > zone->end()) break;

    bool done = false;
    for (int st = 0; st < 4; st++) {
      input->seek(pos+st, librevenge::RVNG_SEEK_SET);
      FullWrtStruct::ZoneHeader docData;
      shared_ptr<FullWrtStruct::ZoneHeader> res;
      docData.m_type = doc.m_type;
      switch (doc.m_type) {
      case 0:
        done = m_textParser->readColumns(zone);
        break;
      case 1:
        done = m_textParser->readParagraphTabs(zone, int(z));
        break;
      case 2:
        done = m_textParser->readItem(zone, int(z), doc.m_structType==4);
        break;
      case 3:
        done = m_textParser->readStyle(zone);
        break;
      case 4: {
        if (pos+st+4> zone->end()) break;
        f.str("");
        f << "Entries(DZone4):";
        int numOk = 0;
        for (int j = 0; j < 2; j++) { // always 0|0
          val = int(input->readLong(2));
          if (val >= -2 && val <= 0) numOk++;
          if (val) f << "f" << j << "=" << val << ",";
        }
        if (!numOk) break;
        asciiFile.addPos(pos+st);
        asciiFile.addNote(f.str().c_str());
        done = true;
        break;
      }
      case 6:
        if (pos+st+2> zone->end()) break;
        val = int(input->readULong(2));
        if (!val || val > 0x200) break;
        f.str("");
        f << "Entries(DZone6):" << doc << ",v=" << val;
        asciiFile.addPos(pos+st);
        asciiFile.addNote(f.str().c_str());
        done = true;
        break;
      case 0x13:
      case 0x14:
        res=m_graphParser->readSideBar(zone, docData);
        break;
      case 0x15:
        res=m_graphParser->readGraphicData(zone, docData);
        break;
      case 0x19: // reference data?
        done=readReferenceData(zone);
        break;
      case 0x1a: {
        if (pos+st+12> zone->end()) break;
        FullWrtParserInternal::ReferenceCalledData refData;
        refData.m_id = int(input->readULong(2));
        if (refData.m_id < 0 || refData.m_id >= int(numDocTypes) ||
            m_state->m_docZoneList[size_t(refData.m_id)].m_type != 0x19)
          break;
        // two small number and then f0=first id in refData, f1=RefValueId?
        // after 0 except one time f2=0x71
        for (int j = 0; j < 5; j++)
          refData.m_values[j] = int(input->readULong(2));

        f.str("");
        f << "Entries(RefCalled):docId=" << refData;
        if (m_state->m_referenceRedirectMap.find(int(z)) == m_state->m_referenceRedirectMap.end())
          m_state->m_referenceRedirectMap[int(z)]=refData;
        else {
          MWAW_DEBUG_MSG(("FullWrtParser::readDocZoneData: oops, reference redirection already exists for docId=%d\n", int(z)));
        }
        asciiFile.addPos(pos+st);
        asciiFile.addNote(f.str().c_str());
        input->seek(pos+st+12, librevenge::RVNG_SEEK_SET);
        done = true;
        break;
      }
      case 0x1e:
        if (pos+st+2> zone->end()) break;
        val = int(input->readULong(2));
        if (val<=0 || val >= int(numDocTypes)) break;

        f.str("");
        // normally a type 15 or 18 zone
        f << "Entries(VariableData):docId=" << val
          << "[" << std::hex << m_state->m_docZoneList[size_t(val)].m_type
          << std::dec << "],";
        if (m_state->m_variableRedirectMap.find(int(z)) == m_state->m_variableRedirectMap.end())
          m_state->m_variableRedirectMap[int(z)]=val;
        else {
          MWAW_DEBUG_MSG(("FullWrtParser::readDocZoneData: oops, variable redirection already exists for docId=%d\n", int(z)));
        }
        asciiFile.addPos(pos+st);
        asciiFile.addNote(f.str().c_str());
        done = true;
        break;
      case 0x1f:
        done = m_textParser->readDataMod(zone, int(z));
        break;
      default:
        done=doc.m_type<=0x18 && readGenericDocData(zone, docData);
        break;
      }
      if (res)
        done=true;
      if (done) {
        int docId=res ? res->m_docId : docData.m_docId;
        if (docId >= 0 && docId != int(z)) {
          MWAW_DEBUG_MSG(("FullWrtParser::readDocZoneData: unexpected id %d != %d\n", docId, int(z)));
          done = false;
        }
        else {
          if (res)
            m_state->addCorrespondance(res->m_docId, res->m_fileId);
          break;
        }
      }
      input->seek(pos+st, librevenge::RVNG_SEEK_SET);
      if (input->readLong(1)) break;
    }
    if (done) {
      prevTypeOk = doc.m_type;
      continue;
    }

    f.str("");
    f << "Entries(DZoneData)##:" << doc;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());

    MWAW_DEBUG_MSG(("FullWrtParser::readDocZoneData: loose reading at zone %d[%d:%d]\n", int(z), doc.m_type, prevTypeOk));
    if (prevTypeOk != -1) prevTypeOk = -1;
    //
    input->seek(pos+4, librevenge::RVNG_SEEK_SET);
    bool lastOk = false;
    while (input->tell()+4 < zone->end()) {
      bool prevLastOk = lastOk;
      lastOk = true;
      pos = input->tell();
      done=m_textParser->readParagraphTabs(zone)||m_textParser->readColumns(zone);
      if (done) continue;
      FullWrtStruct::ZoneHeader docData;
      done=docData.read(zone);
      if (done) {
        if (docData.m_docId >int(z) && docData.m_docId < int(m_state->m_docZoneList.size())) {
          z = size_t(docData.m_docId-1);
          input->seek(pos, librevenge::RVNG_SEEK_SET);
          MWAW_DEBUG_MSG(("FullWrtParser::readDocZoneData: continue reading at zone %d\n", docData.m_docId));
          break;
        }
        continue;
      }
      if (prevLastOk) {
        asciiFile.addPos(pos);
        asciiFile.addNote("DZoneData##:");
      }
      lastOk = false;
      input->seek(pos+1, librevenge::RVNG_SEEK_SET);
    }
  }

  return true;
}

bool FullWrtParser::readDocZoneStruct(FullWrtStruct::EntryPtr zone)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->getAsciiFile();
  libmwaw::DebugStream f;

  long pos = zone->begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int N = (int)input->readLong(2);
  if ((N & 0xF)||N<=0) return false;
  input->seek(pos+6, librevenge::RVNG_SEEK_SET);
  for (int i = 0; i < N-1; i++) {
    if (input->tell() >= zone->end())
      return false;
    long v = input->readLong(1);
    if (v==0) continue;
    if (v!=1 && v!=4) {
      if (2*i > N) {
        MWAW_DEBUG_MSG(("FullWrtParser::readDocZoneStruct: find only %d/%d entries\n", i, N));
        break;
      }
      return false;
    }
    input->seek(4+v, librevenge::RVNG_SEEK_CUR);
  }
  if (input->tell() > zone->end())
    return false;

  zone->setParsed(true);
  f << "Entries(DZoneStruct)|" << *zone <<":";
  if (N%16) { // always a multiple of 16?
    MWAW_DEBUG_MSG(("FullWrtParser::readDocZoneStruct: N(%d) seems odd\n", N));
    f << "###";
  }
  f << "N=" << N << ",";
  input->seek(pos+2, librevenge::RVNG_SEEK_SET);
  f << "unkn=" << std::hex << input->readULong(4) << std::dec << ",";
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  std::set<int> seenSet;
  std::vector<FullWrtParserInternal::DocZoneStruct> &zoneList = m_state->m_docZoneList;
  zoneList.resize(size_t(N)+1);
  for (int i = 0; i < N-1; i++) {
    pos = input->tell();
    int type = (int) input->readULong(1);
    if (type > 1 && type!=4) {
      asciiFile.addPos(pos);
      asciiFile.addNote("DZoneStruct-###");
      break;
    }

    FullWrtParserInternal::DocZoneStruct dt;
    dt.m_structType = type;
    dt.m_pos = pos;
    f.str("");
    f << "DZoneStruct-" << i+1 << ":";
    if (type) {
      dt.m_type = (int) input->readULong(1); // small number between 0 and 1f
      dt.m_nextId = (int) input->readLong(2);
      dt.m_fatherId = (int) input->readLong(2);
      if (dt.m_nextId < 0 || dt.m_nextId > N) {
        f << "#nId=" << dt.m_nextId <<",";
        dt.m_nextId = 0;
      }
      if (dt.m_fatherId < 0 || dt.m_fatherId > N) {
        f << "#fId=" << dt.m_fatherId <<",";
        dt.m_fatherId = -1;
      }
      if (dt.m_nextId) {
        if (seenSet.find(dt.m_nextId) != seenSet.end()) {
          f << "##nId=" << dt.m_nextId << ",";
          dt.m_nextId = 0;
        }
        else
          seenSet.insert(dt.m_nextId);
      }
      if (type==4) {
        asciiFile.addDelimiter(input->tell(),'|');
        input->seek(3, librevenge::RVNG_SEEK_CUR);
      }
      f << dt;
    }
    zoneList[size_t(i)+1]=dt;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
  }
  if (input->tell() != zone->end()) {
    MWAW_DEBUG_MSG(("FullWrtParser::readDocZoneStruct: end seems odd\n"));
  }
  // build child list
  for (size_t i = 0; i <= size_t(N); i++) {
    int fId = zoneList[i].m_fatherId;
    int nId = zoneList[i].m_nextId;
    if (nId && zoneList[size_t(nId)].m_fatherId != fId) {
      MWAW_DEBUG_MSG(("FullWrtParser::readDocZoneStruct: find incoherent children: %d and %d\n", int(i), nId));
      continue;
    }
    int cId = int(i);
    if (seenSet.find(int(cId))!=seenSet.end() || fId < 0) continue;
    // insert the child and its siblings
    while (cId > 0 && cId <= N) {
      zoneList[size_t(fId)].m_childList.push_back(cId);
      cId = zoneList[size_t(cId)].m_nextId;
    }
  }
  // check that we have no dag or cycle
  seenSet.clear();
  std::vector<int> toDoList;
  toDoList.push_back(0);
  seenSet.insert(0);
  while (toDoList.size()) {
    int id = toDoList.back();
    toDoList.pop_back();

    FullWrtParserInternal::DocZoneStruct &nd = zoneList[size_t(id)];
    size_t c = 0;
    while (c < nd.m_childList.size()) {
      int cId = nd.m_childList[c++];
      if (seenSet.find(cId)==seenSet.end()) {
        seenSet.insert(cId);
        toDoList.push_back(cId);
        continue;
      }
      MWAW_DEBUG_MSG(("FullWrtParser::readDocZoneStruct: oops, find a unexpected dag or cycle\n"));
      c--;
      nd.m_childList.erase(nd.m_childList.begin()+int(c));
    }
  }
  for (size_t i = 0; i <= size_t(N); i++) {
    FullWrtParserInternal::DocZoneStruct const &nd = zoneList[i];
    if (!nd.m_childList.size()) continue;
    f.str("");
    f << "childs=[";
    for (size_t c = 0; c < nd.m_childList.size(); c++)
      f << nd.m_childList[c] << ",";
    f << "],";
    asciiFile.addPos(nd.m_pos >= 0 ? nd.m_pos : zone->begin());
    asciiFile.addNote(f.str().c_str());
  }
  zone->closeDebugFile();
  return true;
}

////////////////////////////////////////////////////////////
// read the correspondance data
bool FullWrtParser::readGenericDocData(FullWrtStruct::EntryPtr zone, FullWrtStruct::ZoneHeader &doc)
{
  MWAWInputStreamPtr input = zone->m_input;
  long pos = input->tell();
  if (!doc.read(zone)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  int const vers = version();
  libmwaw::DebugFile &asciiFile = zone->getAsciiFile();
  libmwaw::DebugStream f;

  int val;
  int numSzFollowBlock = 0;
  switch (doc.m_type) {
  case 0xc:
  case 0xd:
  case 0xf:
  case 0x11:
  case 0x12:
  case 0x15:
    break;
  case 0xa:
  case 0xb:
  case 0xe:
  case 0x10:
  case 0x18:
    numSzFollowBlock = 1;
    break;
  case 0x13:
    numSzFollowBlock = 3;
    break;
  default:
    MWAW_DEBUG_MSG(("FullWrtParser::readGenericDocData: called with type=%d\n",doc.m_type));
  case -1:
    break;
  }
  if (input->tell()+1 > zone->end()) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  f.str("");
  if (doc.m_type > 0)
    f << "Entries(DZone" << std::hex << doc.m_type << std::dec << "):";
  else
    f << "Entries(DZoneUnkn" << "):";
  f << doc;
  if (!m_state->addCorrespondance(doc.m_docId, doc.m_fileId))
    f << "#";

  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  for (int i = 0; i < numSzFollowBlock; i++) {
    f.str("");
    f << "DZone" << std::hex << doc.m_type << std::dec << "[" << i << "]:";
    pos = input->tell();
    long sz = (long) input->readULong(4);
    if (sz < 0 || pos+sz+4 > zone->end()) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      f << "#";
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      return true;
    }
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    if (sz) input->seek(sz, librevenge::RVNG_SEEK_CUR);
  }

  if (doc.m_type==0xa) {
    asciiFile.addPos(input->tell());
    asciiFile.addNote("DZonea[1]#");
    input->seek(vers==2 ? 8 : 66, librevenge::RVNG_SEEK_CUR);
  }

  val = int(input->readLong(1));
  if (doc.m_type==0xa) ;
  else if (val==1) {
    pos = input->tell();
    long sz = (long) input->readULong(4);
    if (sz && input->tell()+sz <= zone->end()) {
      f.str("");
      f << "DZone" << std::hex << doc.m_type << std::dec << "[end]:";
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      input->seek(sz, librevenge::RVNG_SEEK_CUR);
    }
    else {
      MWAW_DEBUG_MSG(("FullWrtParser::readGenericDocData: find bad end data\n"));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
    }
  }
  else if (val) {
    MWAW_DEBUG_MSG(("FullWrtParser::readGenericDocData: find bad end data(II)\n"));
  }
  return true;
}

bool FullWrtParser::readReferenceData(FullWrtStruct::EntryPtr zone)
{
  MWAWInputStreamPtr input = zone->m_input;
  long pos = input->tell();
  if (pos+22 > zone->end()) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  libmwaw::DebugFile &asciiFile = zone->getAsciiFile();
  libmwaw::DebugStream f;
  f.str("");
  f << "Entries(RefData):";
  long val = int(input->readULong(2));
  int numOk = 0;
  if (val == 0xa || val == 0xc) numOk++;
  f << "type?=" << val << ",";

  f << "unkn=["; // 3 small number and 0, f0 or f2 is probably an Id, f1=0|1|2
  for (int i = 0; i < 4; i++) {
    val = long(input->readULong(2));
    if (val)
      f << val << ",";
    else
      f << "_,";
    if (i==3) break;
    if (val>0 && val < 0x100) numOk++;
  }
  f << "],";
  if (numOk <= 2) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  f << "ptr=" << std::hex << input->readULong(4) << std::dec << ",";
  for (int i = 0; i < 2; i++) { // always 0 ?
    val = long(input->readULong(2));
    if (val)
      f << "f" << i << "=" << val << ",";
  }

  long sz = input->readLong(4);
  if (sz < 0 || pos+22+sz > zone->end()) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  int numZones=int(m_state->m_docZoneList.size());
  f << "callerId=["; // normally zone of type 0x1a
  for (int i = 0; i < sz/2; i++) {
    int id = int(input->readLong(2));
    if (id < 0 || id >= numZones ||
        m_state->m_docZoneList[size_t(id)].m_type != 0x1a)
      f << "#";
    f << id << ",";
  }
  f << "],";
  input->seek(pos+22+sz, librevenge::RVNG_SEEK_SET);
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the zone flag and positions
////////////////////////////////////////////////////////////
bool FullWrtParser::readFileZoneFlags(FullWrtStruct::EntryPtr zone)
{
  int vers = version();
  int dataSz = vers==1 ? 22 : 16;
  if (!zone || zone->length()%dataSz) {
    MWAW_DEBUG_MSG(("FullWrtParser::readFileZoneFlags: size seems odd\n"));
    return false;
  }
  zone->setParsed(true);
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->getAsciiFile();

  libmwaw::DebugStream f;
  long numElt = zone->length()/dataSz;
  input->seek(zone->begin(), librevenge::RVNG_SEEK_SET);
  std::multimap<int, FullWrtStruct::EntryPtr >::iterator it;
  int numNegZone=3;
  for (long i = 0; i < numElt; i++) {
    long pos = input->tell();
    int id = (int)input->readLong(2);
    it = m_state->m_entryMap.find(id);
    FullWrtStruct::EntryPtr entry;
    f.str("");
    if (it == m_state->m_entryMap.end()) {
      if (id != -2) {
        MWAW_DEBUG_MSG(("FullWrtParser::readFileZoneFlags: can not find entry %d\n",id));
        f << "###";
      }
      entry.reset(new FullWrtStruct::Entry(input));
      entry->setId(1000+id); // false id
    }
    else
      entry = it->second;
    entry->setType("UnknownZone");
    int val = (int)input->readLong(2); // always -2 ?
    if (val != -2) f << "g0=" << val << ",";
    val  = (int)input->readLong(2); // always 0 ?
    if (val) f << "g1=" << val << ",";
    // a small number between 1 and 0x100
    entry->m_values[0] = (int)input->readLong(2);
    for (int j = 0; j < 2; j++) { // always 0
      val  = (int)input->readLong(2);
      if (val) f << "g" << j+2 << "=" << val << ",";
    }
    /** -1: generic, -2: null, other fId */
    entry->m_typeId = (int)input->readLong(2);
    if (entry->m_typeId == -2) ;
    else if (entry->m_typeId != -1)
      entry->setId(int(i));
    else {
      bool find = false;
      for (int j = 0; j < 3; j++) {
        if (i!=m_state->m_zoneFlagsId[j]) continue;
        find = true;
        entry->setId(j);
        break;
      }
      if (!find) {
        MWAW_DEBUG_MSG(("FullWrtParser::readFileZoneFlags: can not find generic zone id %ld\n",i));
        f << "#";
        entry->setId(numNegZone);
      }
      numNegZone++;
    }
    // v2: always  0|0x14, v1 two small number or 0x7FFF
    entry->m_values[1] = (int)input->readLong(1);
    entry->m_values[2] = (int)input->readLong(1);
    if (vers == 1) {
      for (int j = 0; j < 3; j++) { // always 0, -2|0, 0 ?
        val  = (int)input->readLong(2);
        if ((j==1 && val !=-2) || (j!=1 && val))
          f << "g" << j+4 << "=" << val << ",";
      }
    }

    std::string extra = f.str();
    f.str("");
    if (i==0) f << "Entries(FZoneFlags):";
    else f << "FZoneFlags-" << i << ":";
    f << *entry << ",";
    f << extra;

    if (entry->id() < 0) {
      if (entry->m_typeId != -2) {
        MWAW_DEBUG_MSG(("FullWrtParser::readFileZoneFlags: find a null zone with unexpected type\n"));
      }
    }

    input->seek(pos+dataSz, librevenge::RVNG_SEEK_SET);
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
  }
  asciiFile.addPos(zone->end());
  asciiFile.addNote("Entries(ZoneAfter)");
  return true;
}

bool FullWrtParser::readFileZonePos(FullWrtStruct::EntryPtr zone)
{
  int vers = version();
  int dataSz = vers==1 ? 10 : 8;
  if (zone->length()%dataSz) {
    MWAW_DEBUG_MSG(("FullWrtParser::readFileZonePos: size seems odd\n"));
    return false;
  }
  zone->setParsed(true);
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->getAsciiFile();

  libmwaw::DebugStream f;
  int numElt = int(zone->length()/dataSz);
  input->seek(zone->begin(), librevenge::RVNG_SEEK_SET);
  int val;

  // read data
  std::set<long> filePositions;
  std::vector<FullWrtStruct::EntryPtr > listEntry;
  if (numElt>0)
    listEntry.resize(size_t(numElt));
  for (int i = 0; i < numElt; i++) {
    long pos = input->tell();
    long fPos = input->readLong(4);

    FullWrtStruct::EntryPtr entry(new FullWrtStruct::Entry(input));
    if (i == m_state->m_biblioId)
      entry->setType("Biblio");
    else
      entry->setType("Unknown");
    entry->m_nextId = (int) input->readLong(2);
    int id = (int) input->readLong(2);
    f << "realId=" << id << ",";
    entry->setId(i);
    if (fPos >= 0) {
      filePositions.insert(fPos);
      entry->setBegin(fPos);
    }
    f.str("");

    if (entry->begin()>=0)
      f << "pos=" << std::hex << entry->begin() << std::dec << ",";
    if (entry->m_nextId != -2) f << "nextId=" << entry->m_nextId << ",";
    if (vers==1) {
      val = (int) input->readLong(2);
      if (val) f << "f0=" << val << ",";
    }

    input->seek(pos+dataSz, librevenge::RVNG_SEEK_SET);

    entry->setExtra(f.str());
    f.str("");
    if (i == 0) f << "Entries(FZonePos):";
    else f << "FZonePos" << i << ":";
    f << *entry;

    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());

    if (id != -2 && (id < 1 || id >= numElt)) {
      MWAW_DEBUG_MSG(("FullWrtParser::readFileZonePos: entry id seems bad\n"));
    }
    listEntry[size_t(i)] = entry;
  }
  filePositions.insert(zone->begin());

  // compute end of each entry
  for (int i = 0; i < numElt; i++) {
    FullWrtStruct::EntryPtr entry = listEntry[size_t(i)];
    if (!entry || entry->begin() < 0)
      continue;
    std::set<long>::iterator it=filePositions.find(entry->begin());
    if (it == filePositions.end()) {
      MWAW_DEBUG_MSG(("FullWrtParser::readFileZonePos: can not find my entry\n"));
      continue;
    }
    if (++it == filePositions.end()) {
      MWAW_DEBUG_MSG(("FullWrtParser::readFileZonePos: can not find my entry\n"));
      continue;
    }

    entry->setEnd(*it);
    if (entry->m_nextId < 0) continue;
    if (entry->m_nextId >= numElt) {
      entry->m_nextId = -1;
      MWAW_DEBUG_MSG(("FullWrtParser::readFileZonePos: can not find the next entry\n"));
      continue;
    }
    if (!listEntry[size_t(entry->m_nextId)] || listEntry[size_t(entry->m_nextId)]->isParsed()) {
      entry->m_nextId = -1;
      MWAW_DEBUG_MSG(("FullWrtParser::readFileZonePos:  next entry %d is already used\n",
                      entry->m_nextId));
      continue;
    }
    listEntry[size_t(entry->m_nextId)]->setParsed(true);
  }

  for (int i = 0; i < numElt; i++) {
    FullWrtStruct::EntryPtr entry = listEntry[size_t(i)];
    if (!entry || !entry->valid() || entry->isParsed()) continue;

    m_state->m_entryMap.insert
    (std::multimap<int, FullWrtStruct::EntryPtr >::value_type(i, entry));

    if (entry->m_nextId < 0) {
      entry->m_input = input;
      entry->m_asciiFile = shared_ptr<libmwaw::DebugFile>
                           (&asciiFile, MWAW_shared_ptr_noop_deleter<libmwaw::DebugFile>());
      continue;
    }
    // ok we must reconstruct a file
    FullWrtStruct::EntryPtr actEnt = entry;
    librevenge::RVNGBinaryData &data = entry->m_data;
    while (1) {
      if (!actEnt->valid()) break;
      input->seek(actEnt->begin(), librevenge::RVNG_SEEK_SET);
      unsigned long read;
      const unsigned char *dt = input->read((size_t)actEnt->length(), read);
      data.append(dt, read);
      asciiFile.skipZone(actEnt->begin(), actEnt->end()-1);
      if (actEnt->m_nextId < 0) break;
      actEnt = listEntry[size_t(actEnt->m_nextId)];
      if (actEnt) actEnt->setParsed(true);
    }
    entry->update();
  }

  asciiFile.addPos(zone->end());
  asciiFile.addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the document header
////////////////////////////////////////////////////////////
bool FullWrtParser::readDocPosition()
{
  MWAWInputStreamPtr input = getInput();
  if (!input->checkPosition(48))
    return false;

  libmwaw::DebugStream f;
  input->seek(-48, librevenge::RVNG_SEEK_END);
  long pos = input->tell();
  f << "Entries(DocPosition):";

  long val;
  m_state->m_biblioId = (int) input->readLong(2);
  if (m_state->m_biblioId != -2)
    f << "bibId=" << m_state->m_biblioId << ",";
  for (int i = 0; i < 4; i++) { // always 0?
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << val <<",";
  }
  long sz[2];
  for (int i = 0; i < 2; i++) {
    FullWrtStruct::EntryPtr zone(new FullWrtStruct::Entry(input));
    zone->m_asciiFile = shared_ptr<libmwaw::DebugFile>
                        (&ascii(), MWAW_shared_ptr_noop_deleter<libmwaw::DebugFile>());
    zone->setBegin((long)input->readULong(4));
    zone->setLength((sz[i]=(long)input->readULong(4)));
    if (!input->checkPosition(zone->end()) || !zone->valid())
      return false;
    if (i == 1) m_state->m_fileZoneList = zone;
    else m_state->m_fileZoneFlagsList = zone;
  }
  f << "flZones=[";
  for (int i = 0; i < 3; i++) {
    m_state->m_zoneFlagsId[2-i] = (int)input->readLong(2);
    f << m_state->m_zoneFlagsId[2-i] << ",";
  }
  f << "],";
  val = input->readLong(2); // always 0 ?
  if (val) f << "g0=" << val << ",";
  // a big number
  f << std::hex << "unkn=" << input->readULong(2) << std::dec << ",";
  val = (long) input->readULong(4);
  if (val != 1 && val != 0xbeecf54L) // always 1 in v1 and 0xbeecf54L in v2 ?
    f << std::hex << "unkn2=" << val << std::dec << ",";
  // always 1 ?
  val = (long) input->readULong(4);
  if (val != 1) // always 1 in v1
    f << "g1=" << val << ",";
  val = (long) input->readULong(4);
  if (val==0x46575254L) {
    if ((sz[0]%16)==0 && (sz[1]%8)==0)
      setVersion(2);
    else if ((sz[0]%22)==0 && (sz[1]%10)==0)
      setVersion(1);
    else
      return false;
  }
  else {
    if (val != 1) f << "g2=" << val << ",";
    if ((sz[0]%22)==0 && (sz[1]%10)==0)
      setVersion(1);
    else
      return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// the variable part
////////////////////////////////////////////////////////////
void FullWrtParser::sendReference(int id)
{
  if (!getTextListener()) return;

  if (id < 0 || id >= int(m_state->m_docZoneList.size())) {
    MWAW_DEBUG_MSG(("FullWrtParser::sendReference: can not find data for id=%d\n", id));
    return;
  }
  if (m_state->m_docZoneList[size_t(id)].m_type != 0x1a) {
    MWAW_DEBUG_MSG(("FullWrtParser::sendReference: find unexpected type for fieldDataRedirect=%x\n", m_state->m_docZoneList[size_t(id)].m_type));
    return;
  }
  if (m_state->m_referenceRedirectMap.find(id) == m_state->m_referenceRedirectMap.end())
    return; // ok, we have not read the reference
  int docId = m_state->m_referenceRedirectMap.find(id)->second.m_id;
  if (docId < 0 || docId >= int(m_state->m_docZoneList.size()) ||
      m_state->m_docZoneList[size_t(docId)].m_type != 0x19) {
    MWAW_DEBUG_MSG(("FullWrtParser::sendReference: find unexpected redirection id[%d] for reference %d\n", docId, id));
    return;
  }
  static bool first = true;
  if (first) {
    first = false;
    MWAW_DEBUG_MSG(("FullWrtParser::sendReference: sorry, this function is not implemented\n"));
  }
}

////////////////////////////////////////////////////////////
// the variable part
////////////////////////////////////////////////////////////
void FullWrtParser::sendVariable(int id)
{
  if (!getTextListener()) return;

  if (id < 0 || id >= int(m_state->m_docZoneList.size())) {
    MWAW_DEBUG_MSG(("FullWrtParser::sendVariable: can not find data for id=%d\n", id));
    return;
  }
  if (m_state->m_docZoneList[size_t(id)].m_type != 0x1e) {
    MWAW_DEBUG_MSG(("FullWrtParser::sendVariable: find unexpected type for fieldDataRedirect=%x\n", m_state->m_docZoneList[size_t(id)].m_type));
    return;
  }
  if (m_state->m_variableRedirectMap.find(id) == m_state->m_variableRedirectMap.end()) {
    MWAW_DEBUG_MSG(("FullWrtParser::sendVariable: can not find redirection for id=%d\n", id));
    return;
  }
  int docId = m_state->m_variableRedirectMap.find(id)->second;
  if (docId < 0 || docId >= int(m_state->m_docZoneList.size())) {
    MWAW_DEBUG_MSG(("FullWrtParser::sendVariable: find unexpected redirection id[%d] for variable %d\n", docId, id));
    return;
  }
  FullWrtParserInternal::DocZoneStruct const &data =
    m_state->m_docZoneList[size_t(docId)];
  if (data.m_type==0x15)
    sendGraphic(docId);
  else if (data.m_type == 0x18) {
    /** in this case, the content seems to be a textbox which contains the field display,
        but as in general this zone is not read correctly (ie. the field is not found ) and
        as sending textbox is not implemented, better to stop here...
     */
    static bool first = true;
    if (first) {
      first = false;
      MWAW_DEBUG_MSG(("FullWrtParser::sendVariable: sorry, send text/field variable is not implemented\n"));
    }
  }
  else {
    MWAW_DEBUG_MSG(("FullWrtParser::sendVariable: find unexpected redirection type[%x] for variable %d\n", data.m_type, id));
  }
}

////////////////////////////////////////////////////////////
// send a text zone
////////////////////////////////////////////////////////////
void FullWrtParser::sendText(int id, libmwaw::SubDocumentType type, MWAWNote::Type wh)
{
  if (!getTextListener()) return;

  if (id >= 0 && id < int(m_state->m_docZoneList.size())) {
    FullWrtParserInternal::DocZoneStruct const &data =
      m_state->m_docZoneList[size_t(id)];
    int docType = data.m_type;
    if (type==libmwaw::DOC_NOTE && (docType==0xc|| docType==0xd)) ;
    else if (type == libmwaw::DOC_COMMENT_ANNOTATION && docType == 0xb) ;
    else {
      MWAW_DEBUG_MSG(("FullWrtParser::sendText: call with %d[%x]\n", int(type),docType));
    }
  }
  else {
    MWAW_DEBUG_MSG(("FullWrtParser::sendText: can not find data for id=%d\n", id));
  }
  int fId = m_state->getFileZoneId(id);
  MWAWSubDocumentPtr subdoc(new FullWrtParserInternal::SubDocument(*this, getInput(), fId));
  if (type==libmwaw::DOC_NOTE)
    getTextListener()->insertNote(MWAWNote(wh), subdoc);
  else if (type==libmwaw::DOC_COMMENT_ANNOTATION)
    getTextListener()->insertComment(subdoc);
  else {
    MWAW_DEBUG_MSG(("FullWrtParser::sendText: unexpected type\n"));
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
