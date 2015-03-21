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
#include "MWAWStringStream.hxx"
#include "MWAWSubDocument.hxx"

#include "RagTime5Graph.hxx"
#include "RagTime5StructManager.hxx"
#include "RagTime5Text.hxx"
#include "RagTime5ZoneManager.hxx"

#include "RagTime5Parser.hxx"

/** Internal: the structures of a RagTime5Parser */
namespace RagTime5ParserInternal
{
////////////////////////////////////////
////////////////////////////////////////
//! Internal: the helper to read index + unicode string for a RagTime5Parser
struct IndexUnicodeParser : public RagTime5StructManager::DataParser {
  //! constructor
  IndexUnicodeParser(RagTime5Parser &, bool readIndex, std::string const &zoneName) :
    RagTime5StructManager::DataParser(zoneName), m_readIndex(readIndex), m_idToStringMap()
  {
  }
  //! try to parse a data
  bool parseData(MWAWInputStreamPtr &input, long endPos, RagTime5Zone &/*zone*/, int n, libmwaw::DebugStream &f)
  {
    long pos=input->tell();
    int id=n;
    if (m_readIndex) {
      if (endPos-pos<4) {
        MWAW_DEBUG_MSG(("RagTime5ParserInternal::IndexUnicodeParser::parse: bad data size\n"));
        return false;
      }
      id=(int) input->readULong(4);
      f << "id=" << id << ",";
    }
    librevenge::RVNGString str("");
    if (endPos==input->tell())
      ;
    else if (!RagTime5StructManager::readUnicodeString(input, endPos, str))
      f << "###";
    f << "\"" << str.cstr() << "\",";
    m_idToStringMap[id]=str;
    return true;
  }

  //! a flag to know if we need to read the index
  bool m_readIndex;
  //! the data
  std::map<int, librevenge::RVNGString> m_idToStringMap;
};

////////////////////////////////////////
//! Internal: the state of a RagTime5Parser
struct State {
  //! constructor
  State() : m_zonesEntry(), m_zonesList(), m_idToTypeMap(), m_dataIdZoneMap(), m_mainIdZoneMap(), m_clusterIdTypeMap(),
    m_pageZonesIdMap(), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }
  //! init the pattern to default
  void initDefaultPatterns(int vers);

  //! the main zone entry
  MWAWEntry m_zonesEntry;
  //! the zone list
  std::vector<shared_ptr<RagTime5Zone> > m_zonesList;
  //! a map id to type string
  std::map<int, std::string> m_idToTypeMap;
  //! a map: data id->entry (datafork)
  std::map<int, shared_ptr<RagTime5Zone> > m_dataIdZoneMap;
  //! a map: main id->entry (datafork)
  std::multimap<int, shared_ptr<RagTime5Zone> > m_mainIdZoneMap;
  //! map cluster id to type
  std::map<int,int> m_clusterIdTypeMap;
  //! a map: page->main zone id
  std::map<int, std::vector<int> > m_pageZonesIdMap;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a RagTime5Parser
class SubDocument : public MWAWSubDocument
{
public:
  // constructor
  SubDocument(RagTime5Parser &pars, MWAWInputStreamPtr input, int zoneId) :
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
    MWAW_DEBUG_MSG(("RagTime5ParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_parser);

  long pos = m_input->tell();
  // TODO
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
RagTime5Parser::RagTime5Parser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWTextParser(input, rsrcParser, header), m_state(), m_graphParser(), m_textParser(), m_structManager(), m_zoneManager()
{
  init();
}

RagTime5Parser::~RagTime5Parser()
{
}

void RagTime5Parser::init()
{
  m_structManager.reset(new RagTime5StructManager);
  m_zoneManager.reset(new RagTime5ZoneManager(*this));
  m_graphParser.reset(new RagTime5Graph(*this));
  m_textParser.reset(new RagTime5Text(*this));
  resetTextListener();
  setAsciiName("main-1");

  m_state.reset(new RagTime5ParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}

shared_ptr<RagTime5StructManager> RagTime5Parser::getStructManager()
{
  return m_structManager;
}

shared_ptr<RagTime5Zone> RagTime5Parser::getDataZone(int dataId) const
{
  if (m_state->m_dataIdZoneMap.find(dataId)==m_state->m_dataIdZoneMap.end())
    return shared_ptr<RagTime5Zone>();
  return m_state->m_dataIdZoneMap.find(dataId)->second;
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void RagTime5Parser::newPage(int number)
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
void RagTime5Parser::parse(librevenge::RVNGTextInterface *docInterface)
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
      //sendZones();
    }

    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("RagTime5Parser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetTextListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void RagTime5Parser::createDocument(librevenge::RVNGTextInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getTextListener()) {
    MWAW_DEBUG_MSG(("RagTime5Parser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  int numPages=1;
  // TODO
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

bool RagTime5Parser::createZones()
{
  int const vers=version();
  if (vers<5) {
    MWAW_DEBUG_MSG(("RagTime5Parser::createZones: must be called for %d document\n", vers));
    return false;
  }
  if (!findDataZones(m_state->m_zonesEntry))
    return false;
  ascii().addPos(m_state->m_zonesEntry.end());
  ascii().addNote("FileHeader-End");

  if (m_state->m_zonesList.size()<20) {
    // even an empty file seems to have almost ~80 zones, so...
    MWAW_DEBUG_MSG(("RagTime5Parser::createZones: the zone list seems too short\n"));
    return false;
  }
  // we need first to join dissociate zone and to read the type data
  libmwaw::DebugStream f;
  for (size_t i=0; i<m_state->m_zonesList.size(); ++i) {
    if (!m_state->m_zonesList[i])
      continue;
    RagTime5Zone &zone=*m_state->m_zonesList[i];
    if (!update(zone))
      continue;

    std::string what("");
    if (zone.m_idsFlag[1]!=0 || (zone.m_ids[1]!=23 && zone.m_ids[1]!=24) || zone.m_ids[2]!=21 ||
        !readString(zone, what) || what.empty())
      continue;
    if (m_state->m_idToTypeMap.find(zone.m_ids[0])!=m_state->m_idToTypeMap.end()) {
      MWAW_DEBUG_MSG(("RagTime5Parser::createZones: a type with id=%d already exists\n", zone.m_ids[0]));
    }
    else {
      m_state->m_idToTypeMap[zone.m_ids[0]]=what;
      f.str("");
      f << what << ",";
      ascii().addPos(zone.m_defPosition);
      ascii().addNote(f.str().c_str());
    }
  }
  // first find the type of all zone and unpack the zone if needed...
  for (size_t i=0; i<m_state->m_zonesList.size(); ++i) {
    if (!m_state->m_zonesList[i])
      continue;
    RagTime5Zone &zone=*m_state->m_zonesList[i];
    if (zone.m_isParsed)
      continue;
    if (zone.m_fileType==RagTime5Zone::F_Main)
      m_state->m_mainIdZoneMap.insert
      (std::multimap<int, shared_ptr<RagTime5Zone> >::value_type
       (zone.m_ids[0],m_state->m_zonesList[i]));
    else if (zone.m_fileType==RagTime5Zone::F_Data) {
      if (m_state->m_dataIdZoneMap.find(zone.m_ids[0])!=m_state->m_dataIdZoneMap.end()) {
        MWAW_DEBUG_MSG(("RagTime5Parser::createZones: data zone with id=%d already exists\n", zone.m_ids[0]));
      }
      else
        m_state->m_dataIdZoneMap[zone.m_ids[0]]=m_state->m_zonesList[i];
    }
    for (int j=1; j<3; ++j) {
      if (!zone.m_ids[j]) continue;
      if (m_state->m_idToTypeMap.find(zone.m_ids[j])==m_state->m_idToTypeMap.end()) {
        // the main zone seems to point to a cluster id...
        if (zone.m_ids[0]<=6) continue;
        MWAW_DEBUG_MSG(("RagTime5Parser::createZones: can not find the type for %d:%d\n", zone.m_ids[0],j));
        ascii().addPos(zone.m_defPosition);
        ascii().addNote("###type,");
      }
      else {
        zone.m_kinds[j-1]=m_state->m_idToTypeMap.find(zone.m_ids[j])->second;
        f.str("");
        f << zone.m_kinds[j-1] << ",";
        ascii().addPos(zone.m_defPosition);
        ascii().addNote(f.str().c_str());
      }
    }
    if (!zone.m_entry.valid()) continue;
    // first unpack the packed zone
    int usedId=zone.m_kinds[1].empty() ? 0 : 1;
    std::string actType=zone.getKindLastPart(usedId==0);
    if (actType=="Pack") {
      if (!unpackZone(zone)) {
        MWAW_DEBUG_MSG(("RagTime5Parser::createZones: can not unpack the zone %d\n", zone.m_ids[0]));
        libmwaw::DebugFile &ascFile=zone.ascii();
        f.str("");
        f << "Entries(BADPACK)[" << zone << "]:###" << zone.m_kinds[usedId];
        ascFile.addPos(zone.m_entry.begin());
        ascFile.addNote(f.str().c_str());
        continue;
      }
      size_t length=zone.m_kinds[usedId].size();
      if (length>5)
        zone.m_kinds[usedId].resize(length-5);
      else
        zone.m_kinds[usedId]="";
    }

    // check hilo
    usedId=zone.m_kinds[1].empty() ? 0 : 1;
    actType=zone.getKindLastPart(usedId==0);
    if (actType=="HiLo" || actType=="LoHi") {
      zone.m_hiLoEndian=actType=="HiLo";
      size_t length=zone.m_kinds[usedId].size();
      if (length>5)
        zone.m_kinds[usedId].resize(length-5);
      else
        zone.m_kinds[usedId]="";
    }
    std::string kind=zone.getKindLastPart();
    if (kind=="Type") {
      size_t length=zone.m_kinds[0].size();
      if (length>5)
        zone.m_kinds[0].resize(length-5);
      else
        zone.m_kinds[0]="";
      zone.m_extra += "type,";
    }
  }

  std::multimap<int, shared_ptr<RagTime5Zone> >::iterator it;
  // list of blocks zone
  it=m_state->m_mainIdZoneMap.lower_bound(10);
  int numZone10=0;
  while (it!=m_state->m_mainIdZoneMap.end() && it->first==10) {
    shared_ptr<RagTime5Zone> zone=it++->second;
    if (!zone || zone->m_variableD[0]!=1 ||
        m_state->m_dataIdZoneMap.find(zone->m_variableD[1])==m_state->m_dataIdZoneMap.end()) {
      MWAW_DEBUG_MSG(("RagTime5Parser::createZones: the main zone 10 seems bads\n"));
      continue;
    }
    shared_ptr<RagTime5Zone> dZone=
      m_state->m_dataIdZoneMap.find(zone->m_variableD[1])->second;
    if (!dZone || !dZone->m_entry.valid()) continue;
    dZone->m_name=zone->getZoneName();
    if (dZone->getKindLastPart()=="ItemData" && m_structManager->readTypeDefinitions(*dZone)) {
      ++numZone10;
      continue;
    }
    MWAW_DEBUG_MSG(("RagTime5Parser::createZones: unexpected list of block type\n"));
  }
  if (numZone10!=1) {
    MWAW_DEBUG_MSG(("RagTime5Parser::createZones: parses %d list of type zone, we may have a problem\n", numZone10));
  }

  readClusterZones();

  // now read the screen rep list zone
  for (size_t i=0; i<m_state->m_zonesList.size(); ++i) {
    if (!m_state->m_zonesList[i])
      continue;
    RagTime5Zone &zone=*m_state->m_zonesList[i];
    if (zone.m_isParsed || zone.getKindLastPart(zone.m_kinds[1].empty())!="ScreenRepList")
      continue;
    m_graphParser->readPictureList(zone);
  }

  for (size_t i=0; i<m_state->m_zonesList.size(); ++i) {
    if (!m_state->m_zonesList[i])
      continue;
    RagTime5Zone &zone=*m_state->m_zonesList[i];
    if (zone.m_isParsed || !zone.m_entry.valid())
      continue;
    readZoneData(zone);
  }
  return false;
}

bool RagTime5Parser::readZoneData(RagTime5Zone &zone)
{
  if (!zone.m_entry.valid()) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readZoneData: can not find the entry\n"));
    return false;
  }
  libmwaw::DebugStream f;
  int usedId=zone.m_kinds[1].empty() ? 0 : 1;
  std::string actType=zone.getKindLastPart(usedId==0);

  std::string kind=zone.getKindLastPart();
  // the "RagTime" string
  if (kind=="CodeName") {
    std::string what;
    if (zone.m_kinds[1]=="BESoftware:7BitASCII:Type" && readString(zone, what))
      return true;
    MWAW_DEBUG_MSG(("RagTime5Parser::readZoneData: can not read codename for zone %d\n", zone.m_ids[0]));
    f << "Entries(CodeName)[" << zone << "]:###";
    libmwaw::DebugFile &ascFile=zone.ascii();
    ascFile.addPos(zone.m_entry.begin());
    ascFile.addNote(f.str().c_str());
    return true;
  }
  //
  // first test for picture data
  //

  if (kind=="ScreenRepMatchData" || kind=="ScreenRepMatchDataColor")
    return m_graphParser->readPictureMatch(zone, kind=="ScreenRepMatchDataColor");
  // test for other pict

  if (kind=="DocuVersion")
    return readDocumentVersion(zone);
  if (m_graphParser->readPicture(zone))
    return true;
  std::string name("");
  if (kind=="OSAScript" || kind=="TCubics")
    name=kind;
  else if (kind=="ItemData" || kind=="ScriptComment" || kind=="Unicode") {
    actType=zone.getKindLastPart(zone.m_kinds[1].empty());
    if (actType=="Unicode" || kind=="ScriptComment" || kind=="Unicode") {
      // hilo/lohi is not always set, so this can cause problem....
      if (readUnicodeString(zone))
        return true;
      MWAW_DEBUG_MSG(("RagTime5Parser::readZoneData: can not read a unicode zone %d\n", zone.m_ids[0]));
      f << "Entries(StringUnicode)[" << zone << "]:###";
      libmwaw::DebugFile &ascFile=zone.ascii();
      ascFile.addPos(zone.m_entry.begin());
      ascFile.addNote(f.str().c_str());
      return true;
    }
    // checkme: some basic ItemData seems very often be unicode strings for instance data19
    name="ItemDta";
  }
  else {
    MWAW_DEBUG_MSG(("RagTime5Parser::readZoneData: find a unknown type for zone=%d\n", zone.m_ids[0]));
    name="UnknownZone";
  }
  libmwaw::DebugFile &ascFile=zone.ascii();
  f << "Entries(" << name << "):" << zone;
  ascFile.addPos(zone.m_entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(zone.m_entry.end());
  ascFile.addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// parse the different zones
////////////////////////////////////////////////////////////
bool RagTime5Parser::readString(RagTime5Zone &zone, std::string &text)
{
  if (!zone.m_entry.valid()) return false;
  MWAWInputStreamPtr input=zone.getInput();
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  f << "Entries(StringZone)[" << zone << "]:";
  input->seek(zone.m_entry.begin(), librevenge::RVNG_SEEK_SET);
  text="";
  for (long i=0; i<zone.m_entry.length(); ++i) {
    char c=(char) input->readULong(1);
    if (c==0 && i+1==zone.m_entry.length()) break;
    if (c<0x1f)
      return false;
    text+=c;
  }
  f << "\"" << text << "\",";
  if (input->tell()!=zone.m_entry.end()) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readString: find extra data\n"));
    f << "###";
    ascFile.addDelimiter(input->tell(),'|');
  }
  zone.m_isParsed=true;
  ascFile.addPos(zone.m_entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(zone.m_entry.end());
  ascFile.addNote("_");
  return true;
}

bool RagTime5Parser::readUnicodeString(RagTime5Zone &zone)
{
  if (zone.m_entry.length()==0) return true;
  MWAWInputStreamPtr input=zone.getInput();
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  f << "Entries(StringUnicode)[" << zone << "]:";
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(zone.m_entry.begin(), librevenge::RVNG_SEEK_SET);
  librevenge::RVNGString string;
  if (!m_structManager->readUnicodeString(input, zone.m_entry.end(), string))
    f << "###";
  else
    f << string.cstr();
  zone.m_isParsed=true;
  ascFile.addPos(zone.m_entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(zone.m_entry.end());
  ascFile.addNote("_");
  input->setReadInverted(false);
  return true;
}

bool RagTime5Parser::readUnicodeStringList(RagTime5ZoneManager::Link const &link, std::map<int, librevenge::RVNGString> &idToStringMap)
{
  RagTime5ParserInternal::IndexUnicodeParser dataParser(*this, false, "UnicodeList");
  if (!readListZone(link, dataParser))
    return false;
  idToStringMap=dataParser.m_idToStringMap;
  return true;
}

bool RagTime5Parser::readPositions(int posId, std::vector<long> &listPosition)
{
  if (!posId)
    return false;

  shared_ptr<RagTime5Zone> zone=getDataZone(posId);
  if (!zone || !zone->m_entry.valid() || (zone->m_entry.length()%4) ||
      zone->getKindLastPart(zone->m_kinds[1].empty())!="ItemData") {
    MWAW_DEBUG_MSG(("RagTime5Parser::readPositions: the position zone %d seems bad\n", posId));
    return false;
  }
  MWAWEntry entry=zone->m_entry;
  MWAWInputStreamPtr input=zone->getInput();
  input->setReadInverted(!zone->m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);

  zone->m_isParsed=true;
  libmwaw::DebugStream f;
  f << "Entries(Positions)[" << *zone << "]:";

  int N=int(entry.length()/4);
  for (int i=0; i<N; ++i) {
    long ptr=input->readLong(4);
    listPosition.push_back(ptr);
    f << ptr << ",";
  }
  input->setReadInverted(false);
  zone->ascii().addPos(entry.begin());
  zone->ascii().addNote(f.str().c_str());
  zone->ascii().addPos(entry.end());
  zone->ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// Cluster
////////////////////////////////////////////////////////////
bool RagTime5Parser::readClusterZones()
{
  // main11 store the root cluster zone
  std::multimap<int, shared_ptr<RagTime5Zone> >::iterator it=
    m_state->m_mainIdZoneMap.lower_bound(11);
  int numZones=0;
  while (it!=m_state->m_mainIdZoneMap.end() && it->first==11) {
    shared_ptr<RagTime5Zone> zone=it++->second;
    if (!zone || zone->m_variableD[0]!=1 ||
        m_state->m_dataIdZoneMap.find(zone->m_variableD[1])==m_state->m_dataIdZoneMap.end()) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZones: the main cluster zone seems bads\n"));
      continue;
    }
    // the main cluster
    shared_ptr<RagTime5Zone> dZone=
      m_state->m_dataIdZoneMap.find(zone->m_variableD[1])->second;
    if (!dZone) continue;
    dZone->m_extra+="main11,";
    if (dZone->getKindLastPart(dZone->m_kinds[1].empty())!="Cluster" || !readClusterZone(*dZone, 0)) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZones: unexpected main cluster zone type\n"));
      continue;
    }
    ++numZones;

    // try to find the cluster list(checkme)
    if (m_state->m_dataIdZoneMap.find(zone->m_variableD[1]+1)==m_state->m_dataIdZoneMap.end()) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZones: can not find the cluster zone\n"));
      continue;
    }
    dZone=m_state->m_dataIdZoneMap.find(zone->m_variableD[1]+1)->second;
    if (!dZone) continue;
    if (dZone->getKindLastPart(dZone->m_kinds[1].empty())!="ItemData" || !readClusterList(*dZone)) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZones: can not find the cluster zone(II)\n"));
      continue;
    }
  }
  if (numZones!=1) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZones: parses %d main11 zone, we may have a problem\n", numZones));
  }
  // read the main zone
  for (std::map<int,int>::const_iterator cIt=m_state->m_clusterIdTypeMap.begin();
       cIt!=m_state->m_clusterIdTypeMap.end(); ++cIt) {
    int id=cIt->first;
    if (m_state->m_dataIdZoneMap.find(id)!=m_state->m_dataIdZoneMap.end()) {
      shared_ptr<RagTime5Zone> dZone=m_state->m_dataIdZoneMap.find(id)->second;
      if (dZone && !dZone->m_isParsed && dZone->getKindLastPart(dZone->m_kinds[1].empty())=="Cluster"
          && readClusterZone(*dZone, cIt->second)) continue;
    }
    MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZones: can not find cluster zone %d\n", id));
  }

  for (size_t i=0; i<m_state->m_zonesList.size(); ++i) {
    if (!m_state->m_zonesList[i])
      continue;
    RagTime5Zone &zone=*m_state->m_zonesList[i];
    if (zone.m_isParsed || !zone.m_entry.valid() || zone.getKindLastPart(zone.m_kinds[1].empty())!="Cluster")
      continue;
    MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZones: find unparsed cluster zone %d\n", zone.m_ids[0]));
    readClusterZone(zone);
  }

  return true;
}

bool RagTime5Parser::readClusterList(RagTime5Zone &zone)
{
  MWAWEntry &entry=zone.m_entry;
  if (entry.length()<24 || (entry.length()%8)) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readClusterList: the item list seems bad\n"));
    return false;
  }
  zone.m_isParsed=true;
  MWAWInputStreamPtr input=zone.getInput();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  input->setReadInverted(!zone.m_hiLoEndian);

  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  ascFile.addPos(entry.end());
  ascFile.addNote("_");

  int N=int(entry.length()/8);
  for (int i=0; i<N; ++i) {
    long pos=input->tell();
    f.str("");
    if (i==0)
      f << "Entries(ClustList)[" << zone << "]:";
    else
      f << "ClustList-" << i << ",";
    std::vector<int> listIds;
    if (!m_structManager->readDataIdList(input, 1, listIds)) {
      input->seek(pos+8, librevenge::RVNG_SEEK_SET);
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      continue;
    }
    if (listIds[0]==0) {
      input->seek(pos+8, librevenge::RVNG_SEEK_SET);
      ascFile.addPos(pos);
      ascFile.addNote("_");
      continue;
    }
    f << "data" << listIds[0] << "A,";
    int val=(int) input->readULong(2); // [02468][0124][08][1234abe]
    if (val) f << "fl=" << std::hex << val << std::dec << ",";
    m_state->m_clusterIdTypeMap[listIds[0]]=val;
    val=(int) input->readLong(2); // always 0?
    if (val) f << "#f1=" << val << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->setReadInverted(false);
  return true;
}


bool RagTime5Parser::readClusterZone(RagTime5Zone &zone, int zoneType)
{
  RagTime5ZoneManager::Cluster cluster;
  if (!m_zoneManager->readClusterZone(zone, cluster, zoneType))
    return false;

  // check child clusters
  for (size_t j=0; j<cluster.m_clusterIds.size(); ++j) {
    int cId=cluster.m_clusterIds[j];
    if (cId==0) continue;
    shared_ptr<RagTime5Zone> data=getDataZone(cId);
    if (!data || !data->m_entry.valid() || data->getKindLastPart(data->m_kinds[1].empty())!="Cluster") {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: the cluster zone %d seems bad\n", cId));
      continue;
    }
  }

  if (cluster.m_type==RagTime5ZoneManager::Cluster::C_GraphicColors)
    return m_graphParser->readGraphicColors(cluster);
  else if (cluster.m_type==RagTime5ZoneManager::Cluster::C_GraphicStyles)
    return m_graphParser->readGraphicStyles(cluster);
  else if (cluster.m_type==RagTime5ZoneManager::Cluster::C_TextStyles)
    return m_textParser->readTextStyles(cluster);
  else if (cluster.m_type==RagTime5ZoneManager::Cluster::C_Units) {
    RagTime5StructManager::FieldParser defaultParser("Units");
    return readStructZone(cluster, defaultParser);
  }
  else if (cluster.m_type==RagTime5ZoneManager::Cluster::C_Formats)
    return readFormats(cluster);

  if (cluster.m_type==RagTime5ZoneManager::Cluster::C_ColorPattern)
    m_graphParser->readColorPatternZone(cluster);
  else if (cluster.m_type==RagTime5ZoneManager::Cluster::C_GraphicData)
    m_graphParser->readGraphicZone(cluster);
  else if (cluster.m_type==RagTime5ZoneManager::Cluster::C_TextData)
    m_textParser->readTextZone(cluster);
  else if (cluster.m_type==RagTime5ZoneManager::Cluster::C_ClusterA)
    return readUnknownClusterAData(cluster);
  else if (cluster.m_type==RagTime5ZoneManager::Cluster::C_ClusterC)
    return readUnknownClusterCData(cluster);
  else if (!cluster.m_dataLink.empty()) {
    RagTime5StructManager::FieldParser defaultParser("StructZone");
    readStructZone(cluster, defaultParser);
  }

  if (!cluster.m_nameLink.empty()) {
    std::map<int, librevenge::RVNGString> idToStringMap;
    readUnicodeStringList(cluster.m_nameLink, idToStringMap);
  }

  for (size_t i=0; i<cluster.m_linksList.size(); ++i) {
    RagTime5ZoneManager::Link const &link=cluster.m_linksList[i];
    if (link.m_type==RagTime5ZoneManager::Link::L_GraphicType) {
      m_graphParser->readGraphicTypes(zone, link);
      continue;
    }
    else if (link.m_type==RagTime5ZoneManager::Link::L_ConditionFormula) {
      RagTime5ZoneManager::Cluster unknCluster;
      unknCluster.m_dataLink=link;
      RagTime5StructManager::FieldParser defaultParser("CondFormula");
      readStructZone(unknCluster, defaultParser, false);
      continue;
    }
    else if (link.m_type==RagTime5ZoneManager::Link::L_SettingsList) {
      RagTime5ZoneManager::Cluster unknCluster;
      unknCluster.m_dataLink=link;
      RagTime5StructManager::FieldParser defaultParser("Settings");
      readStructZone(unknCluster, defaultParser, false);
      continue;
    }
    else if (link.m_type==RagTime5ZoneManager::Link::L_List) {
      readListZone(link);
      continue;
    }
    else if (link.m_type==RagTime5ZoneManager::Link::L_FieldCluster) {
      m_zoneManager->readFieldClusters(link);
      continue;
    }
    else if (link.m_type==RagTime5ZoneManager::Link::L_FieldDef ||
             link.m_type==RagTime5ZoneManager::Link::L_FieldPos) {
      m_textParser->readFieldZones(cluster, link);
      continue;
    }
    else if (link.m_type==RagTime5ZoneManager::Link::L_LinkDef) {
      m_textParser->readLinkZones(cluster, link);
      continue;
    }
    else if (link.m_type==RagTime5ZoneManager::Link::L_UnknownClusterC) {
      m_zoneManager->readUnknownClusterC(link);
      continue;
    }

    if (link.empty()) continue;
    shared_ptr<RagTime5Zone> data=getDataZone(link.m_ids[0]);
    if (!data || data->m_isParsed) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: can not find data zone %d\n", link.m_ids[0]));
      continue;
    }
    data->m_hiLoEndian=zone.m_hiLoEndian;
    if (link.m_fieldSize==6 && link.m_fileType[0]==0 && link.m_fileType[1]==0 && readUnknZoneA(*data, link))
      continue;
    if (link.m_fieldSize==0 && !data->m_entry.valid())
      continue;
    switch (link.m_type) {
    case RagTime5ZoneManager::Link::L_ColorPattern:
    case RagTime5ZoneManager::Link::L_ConditionFormula:
    case RagTime5ZoneManager::Link::L_FieldCluster:
    case RagTime5ZoneManager::Link::L_FieldDef:
    case RagTime5ZoneManager::Link::L_FieldPos:
    case RagTime5ZoneManager::Link::L_FieldsList:
    case RagTime5ZoneManager::Link::L_Graphic:
    case RagTime5ZoneManager::Link::L_GraphicType:
    case RagTime5ZoneManager::Link::L_List:
    case RagTime5ZoneManager::Link::L_SettingsList:
    case RagTime5ZoneManager::Link::L_Text:
    case RagTime5ZoneManager::Link::L_UnicodeList:
    case RagTime5ZoneManager::Link::L_UnknownClusterC:
      break;
    case RagTime5ZoneManager::Link::L_UnknownClusterB:
      if (data->getKindLastPart(data->m_kinds[1].empty())!="Cluster") {
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: the child clusterB %d seems bad\n", link.m_ids[0]));
        break;
      }
      readClusterZone(*data, 0x10000);
      break;
    case RagTime5ZoneManager::Link::L_GraphicTransform:
      m_graphParser->readGraphicTransformations(*data, link);
      break;
    case RagTime5ZoneManager::Link::L_TextUnknown:
      m_textParser->readTextUnknown(link.m_ids[0]);
      break;
    case RagTime5ZoneManager::Link::L_ClusterLink:
      readClusterLinkList(*data, link);
      break;
    case RagTime5ZoneManager::Link::L_LinkDef:
    case RagTime5ZoneManager::Link::L_Unknown:
    default: {
      if (!data->m_entry.valid()) {
        if (link.m_N*link.m_fieldSize) {
          MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: can not find data zone %d\n", link.m_ids[0]));
        }
        break;
      }
      long pos=data->m_entry.begin();
      data->m_isParsed=true;
      libmwaw::DebugFile &dAscFile=data->ascii();
      if (pos+link.m_N*link.m_fieldSize>data->m_entry.end()) {
        MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: bad fieldSize/N for zone %d\n", link.m_ids[0]));
        libmwaw::DebugStream f;
        f << "Entries(" << link.getZoneName() << ")[" << *data << "]:" << "###";
        dAscFile.addPos(pos);
        dAscFile.addNote(f.str().c_str());
        break;
      }
      for (int j=0; j<link.m_N; ++j) {
        libmwaw::DebugStream f;
        if (j==0)
          f << "Entries(" << link.getZoneName() << ")[" << *data << "]:";
        else
          f << link.getZoneName() << "-" << j << ":";
        if (link.m_fieldSize==0) {
          MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: unexpected field size for zone %d\n", link.m_ids[0]));
          f << "###";
          dAscFile.addPos(pos);
          dAscFile.addNote(f.str().c_str());
          break;
        }
        dAscFile.addPos(pos);
        dAscFile.addNote(f.str().c_str());
        pos+=link.m_fieldSize;
      }
      break;
    }
    }
  }
  return true;
}

bool RagTime5Parser::readClusterLinkList(RagTime5Zone &zone, RagTime5ZoneManager::Link const &link)
{
  if (!zone.m_entry.valid()) {
    if (link.m_N*link.m_fieldSize) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterLinkList: can not find data zone %d\n", link.m_ids[0]));
    }
    return false;
  }

  MWAWInputStreamPtr input=zone.getInput();
  bool const hiLo=zone.m_hiLoEndian;
  input->setReadInverted(!hiLo);
  input->seek(zone.m_entry.begin(), librevenge::RVNG_SEEK_SET);
  zone.m_isParsed=true;

  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;

  f << "Entries(ClustLink)[" << zone << "]:";
  if (link.m_N*link.m_fieldSize!=zone.m_entry.length() || link.m_fieldSize!=12) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readClusterLinkList: bad fieldSize/N for zone %d\n", link.m_ids[0]));
    f << "###";
    ascFile.addPos(zone.m_entry.begin());
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(zone.m_entry.begin());
  ascFile.addNote(f.str().c_str());

  for (int i=0; i<link.m_N; ++i) {
    long pos=input->tell();
    f.str("");
    f << "ClustLink-" << i << ":";
    std::vector<int> listIds;
    if (!m_structManager->readDataIdList(input, 1, listIds)) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterLinkList: a link seems bad\n"));
      f << "###id,";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(pos+12, librevenge::RVNG_SEEK_SET);
      continue;
    }
    else if (listIds[0]==0) {
      ascFile.addPos(pos);
      ascFile.addNote("_");
      input->seek(pos+12, librevenge::RVNG_SEEK_SET);
      continue;
    }
    else
      f << "data" << listIds[0] << ",";
    unsigned long val=(unsigned long) input->readULong(4); // 0 or 80000000 and a small int
    if (val&0x80000000)
      f << "f0=" << (val&0x7FFFFFFF) << ",";
    else if (val)
      f << "#f0=" << val << ",";
    val=(unsigned long) input->readULong(4); // a small int
    if (val)
      f << "f1=" << val << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+12, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// unknown
////////////////////////////////////////////////////////////
bool RagTime5Parser::readUnknZoneA(RagTime5Zone &zone, RagTime5ZoneManager::Link const &link)
{
  if (!zone.m_entry.valid()||link.m_N<20)
    return false;
  if (zone.m_entry.length()!=6*link.m_N) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readUnknZoneA: the zone %d seems bad\n", zone.m_ids[0]));
    return false;
  }
  MWAWInputStreamPtr input=zone.getInput();
  bool const hiLo=zone.m_hiLoEndian;
  input->setReadInverted(!hiLo);
  input->seek(zone.m_entry.begin(), librevenge::RVNG_SEEK_SET);
  zone.m_isParsed=true;

  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  input->seek(zone.m_entry.begin(), librevenge::RVNG_SEEK_SET);

  for (int i=0; i<link.m_N; ++i) {
    long pos=input->tell();
    f.str("");
    if (i==0)
      f << "Entries(UnknZoneA)[" << zone << "]:N=" << link.m_N << ",";
    else
      f << "UnknZoneA-" << i << ":";
    int fl, id, val;
    if (hiLo) {
      fl=(int) input->readULong(2);
      id=(int) input->readULong(2);
      val=(int) input->readLong(2);
    }
    else {
      val=(int) input->readLong(2);
      id=(int) input->readULong(2);
      fl=(int) input->readULong(2);
    }
    if (id)
      f << "id=" << id << ",";
    if ((fl&0x7fe))
      f << "#";
    if (fl) f << "fl=" << std::hex << fl << std::dec << ",";
    if (val!=-1)
      f << "f0=" << val << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->setReadInverted(false);
  ascFile.addPos(zone.m_entry.end());
  ascFile.addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// structured zone
////////////////////////////////////////////////////////////
bool RagTime5Parser::readFormats(RagTime5ZoneManager::Cluster &cluster)
{
  RagTime5ZoneManager::Link const &link=cluster.m_dataLink;
  if (link.m_ids.size()<2 || !link.m_ids[1])
    return false;

  std::map<int, librevenge::RVNGString> idToNameMap;
  if (!cluster.m_nameLink.empty()) {
    readUnicodeStringList(cluster.m_nameLink, idToNameMap);
    cluster.m_nameLink=RagTime5ZoneManager::Link();
  }
  std::vector<long> decal;
  if (link.m_ids[0])
    readPositions(link.m_ids[0], decal);
  if (decal.empty())
    decal=link.m_longList;
  int const dataId=link.m_ids[1];
  shared_ptr<RagTime5Zone> dataZone=getDataZone(dataId);
  int N=int(decal.size());

  if (!dataZone || !dataZone->m_entry.valid() ||
      dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData" || N<=1) {
    if (N==1 && dataZone && !dataZone->m_entry.valid()) {
      // a zone with 0 zone is ok...
      dataZone->m_isParsed=true;
      return true;
    }
    MWAW_DEBUG_MSG(("RagTime5Parser::readFormats: the data zone %d seems bad\n", dataId));
    return false;
  }

  dataZone->m_isParsed=true;
  MWAWEntry entry=dataZone->m_entry;
  libmwaw::DebugFile &ascFile=dataZone->ascii();
  libmwaw::DebugStream f;
  f << "Entries(Format)[" << *dataZone << "]:";
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());

  MWAWInputStreamPtr input=dataZone->getInput();
  input->setReadInverted(!dataZone->m_hiLoEndian);
  long debPos=entry.begin();
  long endPos=entry.end();

  for (int i=0; i<N-1; ++i) {
    long pos=decal[size_t(i)];
    if (pos<0 || debPos+pos>endPos) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readFormats: can not read the data zone %d-%d seems bad\n", dataId, i));
      continue;
    }
    input->seek(debPos+pos, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << "Format-" << i+1 << ":";
    if (idToNameMap.find(i+1)!=idToNameMap.end())
      f << "\"" << idToNameMap.find(i+1)->second.cstr() << "\",";
    /* list of
       0000000200010106000000000000[fSz=01]01
     */
    ascFile.addPos(debPos+pos);
    ascFile.addNote(f.str().c_str());
  }

  input->setReadInverted(false);

  for (size_t i=0; i<cluster.m_linksList.size(); ++i) {
    RagTime5ZoneManager::Link const &lnk=cluster.m_linksList[i];
    shared_ptr<RagTime5Zone> data=getDataZone(lnk.m_ids[0]);
    if (!data->m_entry.valid()) {
      if (lnk.m_N*lnk.m_fieldSize) {
        MWAW_DEBUG_MSG(("RagTime5Parser::readFormats: can not find data zone %d\n", lnk.m_ids[0]));
      }
      continue;
    }
    std::string name("Form");
    name += (lnk.m_fileType[0]==0x3e800 ? "A" : lnk.m_fileType[0]==0x35800 ? "B" : lnk.getZoneName().c_str());
    long pos=data->m_entry.begin();
    data->m_isParsed=true;
    libmwaw::DebugFile &dAscFile=data->ascii();
    if (lnk.m_fieldSize<=0 || lnk.m_N*lnk.m_fieldSize!=data->m_entry.length()) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readFormats: bad fieldSize/N for zone %d\n", lnk.m_ids[0]));
      f.str("");
      f << "Entries(" << name << ")[" << *data << "]:" << "###";
      dAscFile.addPos(pos);
      dAscFile.addNote(f.str().c_str());
      continue;
    }
    for (int j=0; j<lnk.m_N; ++j) {
      f.str("");
      if (j==0)
        f << "Entries(" << name << ")[" << *data << "]:";
      else
        f << name << "-" << j << ":";
      dAscFile.addPos(pos);
      dAscFile.addNote(f.str().c_str());
      pos+=lnk.m_fieldSize;
    }
  }

  return true;
}

bool RagTime5Parser::readUnknownClusterAData(RagTime5ZoneManager::Cluster &cluster)
{
  RagTime5ZoneManager::Link const &link=cluster.m_dataLink;
  if (link.m_ids.empty() || !link.m_ids[0]) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readUnknownClusterAData: can not find the main data\n"));
    return false;
  }
  shared_ptr<RagTime5Zone> dataZone=getDataZone(link.m_ids[0]);
  if (!dataZone || !dataZone->m_entry.valid() ||
      dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData") {
    MWAW_DEBUG_MSG(("RagTime5Parser::readUnknownClusterAData: the data zone %d seems bad\n", link.m_ids[0]));
    return false;
  }

  libmwaw::DebugFile &ascFile=dataZone->ascii();
  libmwaw::DebugStream f;
  long pos=dataZone->m_entry.begin();
  dataZone->m_isParsed=true;
  if (link.m_fieldSize<=0 || link.m_N*link.m_fieldSize!=dataZone->m_entry.length()) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readUnknownClusterAData: bad fieldSize/N for zone %d\n", link.m_ids[0]));
    f << "Entries(Data1_clustUnknA)[" << *dataZone << "]:" << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  else {
    for (int i=0; i<link.m_N; ++i) {
      f.str("");
      if (i==0)
        f << "Entries(Data1_clustUnknA)[" << *dataZone << "]:";
      else
        f << "Data1_clustUnknA-" << i << ":";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      pos+=link.m_fieldSize;
    }
  }

  for (size_t i=0; i<cluster.m_linksList.size(); ++i) {
    RagTime5ZoneManager::Link const &lnk=cluster.m_linksList[i];
    shared_ptr<RagTime5Zone> data=getDataZone(lnk.m_ids[0]);
    if (!data->m_entry.valid()) {
      if (lnk.m_N*lnk.m_fieldSize) {
        MWAW_DEBUG_MSG(("RagTime5Parser::readUnknownClusterAData: can not find data zone %d\n", lnk.m_ids[0]));
      }
      continue;
    }
    pos=data->m_entry.begin();
    data->m_isParsed=true;
    libmwaw::DebugFile &dAscFile=data->ascii();
    if (lnk.m_fieldSize<=0 || lnk.m_N*lnk.m_fieldSize!=data->m_entry.length()) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readUnknownClusterAData: bad fieldSize/N for zone %d\n", lnk.m_ids[0]));
      f.str("");
      f << "Entries(Data2_clustUnknA)[" << *data << "]:" << "###";
      dAscFile.addPos(pos);
      dAscFile.addNote(f.str().c_str());
      continue;
    }
    for (int j=0; j<lnk.m_N; ++j) {
      f.str("");
      if (j==0)
        f << "Entries(Data2_clustUnknA)[" << *data << "]:";
      else
        f << "Data2_clustUnknA-" << j << ":";
      dAscFile.addPos(pos);
      dAscFile.addNote(f.str().c_str());
      pos+=lnk.m_fieldSize;
    }
  }

  return true;
}

bool RagTime5Parser::readUnknownClusterCData(RagTime5ZoneManager::Cluster &cluster)
{
  RagTime5ZoneManager::Link const &link=cluster.m_dataLink;
  if (link.m_ids.empty()) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readUnknownClusterCData: can not find the main data\n"));
    return false;
  }
  std::stringstream s;
  s << "Data1_clustUnknC" << link.m_fileType[0];
  std::string zoneName=s.str();
  libmwaw::DebugStream f;

  if (link.m_type==RagTime5ZoneManager::Link::L_List) {
    if (link.m_fileType[1]==0x330) {
      // find id=8,"Rechenblatt 1": spreadsheet name ?
      RagTime5ParserInternal::IndexUnicodeParser parser(*this, true, zoneName);
      readListZone(link, parser);
    }
    else {
      RagTime5StructManager::DataParser parser(zoneName);
      readListZone(link, parser);
    }
  }
  else {
    shared_ptr<RagTime5Zone> dataZone;
    if (link.m_ids[0])
      dataZone=getDataZone(link.m_ids[0]);
    if (!dataZone || !dataZone->m_entry.valid() ||
        dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData") {
      MWAW_DEBUG_MSG(("RagTime5Parser::readUnknownClusterCData: the data zone %d seems bad\n", link.m_ids[0]));
      return false;
    }
    long pos=dataZone->m_entry.begin();
    libmwaw::DebugFile &ascFile=dataZone->ascii();
    dataZone->m_isParsed=true;
    if (link.m_fieldSize<=0 || link.m_N*link.m_fieldSize!=dataZone->m_entry.length()) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readUnknownClusterCData: bad fieldSize/N for zone %d\n", link.m_ids[0]));
      f << "Entries(" << zoneName << ")[" << *dataZone << "]:" << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    else {
      for (int i=0; i<link.m_N; ++i) {
        f.str("");
        if (i==0)
          f << "Entries(" << zoneName << ")[" << *dataZone << "]:";
        else
          f << zoneName << "-" << i << ":";
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        pos+=link.m_fieldSize;
      }
    }
  }
  for (size_t i=0; i<cluster.m_linksList.size(); ++i) {
    RagTime5ZoneManager::Link const &lnk=cluster.m_linksList[i];
    shared_ptr<RagTime5Zone> data=getDataZone(lnk.m_ids[0]);
    if (!data->m_entry.valid()) {
      if (lnk.m_N*lnk.m_fieldSize) {
        MWAW_DEBUG_MSG(("RagTime5Parser::readUnknownClusterCData: can not find data zone %d\n", lnk.m_ids[0]));
      }
      continue;
    }
    long pos=data->m_entry.begin();
    data->m_isParsed=true;
    libmwaw::DebugFile &dAscFile=data->ascii();
    if (lnk.m_fieldSize<=0 || lnk.m_N*lnk.m_fieldSize!=data->m_entry.length()) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readUnknownClusterCData: bad fieldSize/N for zone %d\n", lnk.m_ids[0]));
      f.str("");
      f << "Entries(Data2_clustUnknC)[" << *data << "]:" << "###";
      dAscFile.addPos(pos);
      dAscFile.addNote(f.str().c_str());
      continue;
    }
    for (int j=0; j<lnk.m_N; ++j) {
      f.str("");
      if (j==0)
        f << "Entries(Data2_clustUnknC)[" << *data << "]:";
      else
        f << "Data2_clustUnknC-" << j << ":";
      dAscFile.addPos(pos);
      dAscFile.addNote(f.str().c_str());
      pos+=lnk.m_fieldSize;
    }
  }

  return true;
}

bool RagTime5Parser::readListZone(RagTime5ZoneManager::Link const &link)
{
  // fixme: must not be here
  if (link.m_fileType[0]==0 && link.m_fileType[1]==0x330) {
    RagTime5ParserInternal::IndexUnicodeParser parser(*this, true, "UnicodeIndexList");
    return readListZone(link, parser);
  }
  RagTime5StructManager::DataParser parser(link.getZoneName());
  return readListZone(link, parser);
}

bool RagTime5Parser::readListZone(RagTime5ZoneManager::Link const &link, RagTime5StructManager::DataParser &parser)
{
  if (link.m_ids.size()<2 || !link.m_ids[1])
    return false;

  std::vector<long> decal;
  if (link.m_ids[0])
    readPositions(link.m_ids[0], decal);
  if (decal.empty())
    decal=link.m_longList;

  int const dataId=link.m_ids[1];
  shared_ptr<RagTime5Zone> dataZone=getDataZone(dataId);
  int N=int(decal.size());

  if (!dataZone || !dataZone->m_entry.valid() ||
      dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData" || N<=1) {
    if (N==1 && dataZone && !dataZone->m_entry.valid()) {
      // a zone with 0 zone is ok...
      dataZone->m_isParsed=true;
      return true;
    }
    MWAW_DEBUG_MSG(("RagTime5Parser::readListZone: the data zone %d seems bad\n", dataId));
    return false;
  }

  dataZone->m_isParsed=true;
  MWAWEntry entry=dataZone->m_entry;
  libmwaw::DebugFile &ascFile=dataZone->ascii();
  libmwaw::DebugStream f;
  f << "Entries(" << parser.getZoneName() << ")[" << *dataZone << "]:";
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());

  MWAWInputStreamPtr input=dataZone->getInput();
  input->setReadInverted(!dataZone->m_hiLoEndian);
  long debPos=entry.begin();
  long endPos=entry.end();

  for (int i=0; i<N-1; ++i) {
    long pos=decal[size_t(i)], lastPos=decal[size_t(i+1)];
    if (pos==lastPos) continue;
    if (pos<0 || pos>lastPos || debPos+lastPos>endPos) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readListZone: can not read the data zone %d-%d seems bad\n", dataId, i));
      continue;
    }
    input->seek(debPos+pos, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << parser.getZoneName(i+1) << ":";
    if (!parser.parseData(input, debPos+lastPos, *dataZone, i+1, f))
      f << "###";
    ascFile.addPos(debPos+pos);
    ascFile.addNote(f.str().c_str());
    ascFile.addPos(debPos+lastPos);
    ascFile.addNote("_");
  }

  input->setReadInverted(false);
  return true;
}

bool RagTime5Parser::readStructZone(RagTime5ZoneManager::Cluster &cluster, RagTime5StructManager::FieldParser &parser,
                                    bool hasHeader)
{
  RagTime5ZoneManager::Link const &link=cluster.m_dataLink;
  if (link.m_ids.size()<2 || !link.m_ids[1])
    return false;

  std::map<int, librevenge::RVNGString> idToNameMap;
  if (!cluster.m_nameLink.empty()) {
    readUnicodeStringList(cluster.m_nameLink, idToNameMap);
    cluster.m_nameLink=RagTime5ZoneManager::Link();
  }
  std::vector<long> decal;
  if (link.m_ids[0])
    readPositions(link.m_ids[0], decal);
  if (decal.empty())
    decal=link.m_longList;
  int const dataId=link.m_ids[1];
  shared_ptr<RagTime5Zone> dataZone=getDataZone(dataId);
  if (!dataZone || !dataZone->m_entry.valid() ||
      dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData") {
    if (decal.size()==1) {
      // a zone with 0 zone is ok...
      dataZone->m_isParsed=true;
      return true;
    }
    MWAW_DEBUG_MSG(("RagTime5Parser::readStructZone: the data zone %d seems bad\n", dataId));
    return false;
  }
  dataZone->m_isParsed=true;
  MWAWEntry entry=dataZone->m_entry;
  libmwaw::DebugFile &ascFile=dataZone->ascii();
  libmwaw::DebugStream f;
  f << "Entries(" << parser.getZoneName() << ")[" << *dataZone << "]:";
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());

  int N=int(decal.size());
  MWAWInputStreamPtr input=dataZone->getInput();
  input->setReadInverted(!dataZone->m_hiLoEndian);
  long debPos=entry.begin();
  long endPos=entry.end();
  if (N==0) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readStructZone: can not find decal list for zone %d, let try to continue\n", dataId));
    input->seek(debPos, librevenge::RVNG_SEEK_SET);
    int n=0;
    while (input->tell()+8 < endPos) {
      long pos=input->tell();
      int id=++n;
      librevenge::RVNGString name("");
      if (idToNameMap.find(id)!=idToNameMap.end())
        name=idToNameMap.find(id)->second;
      if (!readStructData(*dataZone, endPos, id, hasHeader, parser, name)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
    }
    if (input->tell()!=endPos) {
      static bool first=true;
      if (first) {
        MWAW_DEBUG_MSG(("RagTime5Parser::readStructZone: can not read some block\n"));
        first=false;
      }
      ascFile.addPos(debPos);
      ascFile.addNote("###");
    }
  }
  else {
    for (int i=0; i<N-1; ++i) {
      long pos=decal[size_t(i)];
      long nextPos=decal[size_t(i+1)];
      if (pos<0 || debPos+pos>endPos) {
        MWAW_DEBUG_MSG(("RagTime5Parser::readStructZone: can not read the data zone %d-%d seems bad\n", dataId, i));
        continue;
      }
      librevenge::RVNGString name("");
      if (idToNameMap.find(i+1)!=idToNameMap.end())
        name=idToNameMap.find(i+1)->second;
      input->seek(debPos+pos, librevenge::RVNG_SEEK_SET);
      readStructData(*dataZone, debPos+nextPos, i+1, hasHeader, parser, name);
      if (input->tell()!=debPos+nextPos) {
        static bool first=true;
        if (first) {
          MWAW_DEBUG_MSG(("RagTime5Parser::readStructZone: can not read some block\n"));
          first=false;
        }
        ascFile.addPos(debPos+pos);
        ascFile.addNote("###");
      }
    }
  }
  return true;
}

bool RagTime5Parser::readStructData(RagTime5Zone &zone, long endPos, int n, bool hasHeader,
                                    RagTime5StructManager::FieldParser &parser, librevenge::RVNGString const &dataName)
{
  MWAWInputStreamPtr input=zone.getInput();
  long pos=input->tell();
  if ((hasHeader && pos+14>endPos) || (!hasHeader && pos+5>endPos)) return false;
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  std::string const zoneName=parser.getZoneName(n);
  int val;
  if (hasHeader) {
    f << zoneName << "[A]:";
    if (!dataName.empty()) f << dataName.cstr() << ",";
    val=(int) input->readLong(4);
    if (val!=1) f << "numUsed=" << val << ",";
    f << "f1=" << std::hex << input->readULong(2) << std::dec << ",";
    val=(int) input->readLong(2); // sometimes form an increasing sequence but not always
    if (val!=n) f << "id=" << val << ",";
    f << "type=" << std::hex << input->readULong(4) << std::dec << ","; // 0 or 0x14[5-b][0-f][08]42 or 17d5042
    val=(int) input->readLong(2); // small number
    if (val) f << "f3=" << val << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  int m=0;
  pos=input->tell();
  if (parser.m_regroupFields) {
    f.str("");
    f << zoneName << "[B]:";
    if (!hasHeader && !dataName.empty()) f << dataName.cstr() << ",";
  }
  while (!input->isEnd()) {
    long actPos=input->tell();
    if (actPos>=endPos) break;

    if (!parser.m_regroupFields) {
      f.str("");
      f << zoneName << "[B" << ++m << "]:";
      if (m==1 && !hasHeader && !dataName.empty()) f << dataName.cstr() << ",";
    }
    RagTime5StructManager::Field field;
    if (!m_structManager->readField(input, endPos, ascFile, field, hasHeader ? 0 : endPos-actPos)) {
      input->seek(actPos, librevenge::RVNG_SEEK_SET);
      break;
    }
    if (!parser.parseField(field, zone, n, f))
      f << "#" << field;
    if (!parser.m_regroupFields) {
      ascFile.addPos(actPos);
      ascFile.addNote(f.str().c_str());
    }
  }
  if (parser.m_regroupFields && pos!=input->tell()) {
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// zone unpack/create ascii file, ...
////////////////////////////////////////////////////////////
bool RagTime5Parser::update(RagTime5Zone &zone)
{
  if (zone.m_entriesList.empty())
    return true;
  if (zone.isHeaderZone()) {
    zone.m_isParsed=true;
    return false;
  }
  std::stringstream s;
  s << "Zone" << std::hex << zone.m_entriesList[0].begin() << std::dec;
  zone.setAsciiFileName(s.str());

  if (zone.m_entriesList.size()==1) {
    zone.m_entry=zone.m_entriesList[0];
    return true;
  }

  libmwaw::DebugStream f;
  f << "Entries(" << zone.getZoneName() << "):";
  MWAWInputStreamPtr input = getInput();
  shared_ptr<MWAWStringStream> newStream;
  for (size_t z=0; z<zone.m_entriesList.size(); ++z) {
    MWAWEntry const &entry=zone.m_entriesList[z];
    if (!entry.valid() || !input->checkPosition(entry.end())) {
      MWAW_DEBUG_MSG(("RagTime5Parser::update: can not read some data\n"));
      f << "###";
      ascii().addPos(entry.begin());
      ascii().addNote(f.str().c_str());
      return false;
    }
    input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);

    unsigned long read;
    const unsigned char *dt = input->read((unsigned long)entry.length(), read);
    if (!dt || long(read) != entry.length()) {
      MWAW_DEBUG_MSG(("RagTime5Parser::update: can not read some data\n"));
      f << "###";
      ascii().addPos(entry.begin());
      ascii().addNote(f.str().c_str());
      return false;
    }
    ascii().skipZone(entry.begin(), entry.end()-1);
    if (z==0)
      newStream.reset(new MWAWStringStream(dt, (unsigned int) entry.length()));
    else
      newStream->append(dt, (unsigned int) entry.length());
  }

  MWAWInputStreamPtr newInput(new MWAWInputStream(newStream, false));
  zone.setInput(newInput);
  zone.m_entry.setBegin(0);
  zone.m_entry.setLength(newInput->size());

  return true;
}

bool RagTime5Parser::unpackZone(RagTime5Zone &zone, MWAWEntry const &entry, std::vector<unsigned char> &data)
{
  if (!entry.valid())
    return false;

  MWAWInputStreamPtr input=zone.getInput();
  long pos=entry.begin(), endPos=entry.end();
  if (entry.length()<4 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTime5Parser::unpackZone: the input seems bad\n"));
    return false;
  }

  bool actEndian=input->readInverted();
  input->setReadInverted(false);
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  data.resize(0);
  unsigned long sz=(unsigned long) input->readULong(4);
  if (sz==0) {
    input->setReadInverted(actEndian);
    return true;
  }
  int flag=int(sz>>24);
  sz &= 0xFFFFFF;
  if ((flag&0xf) || (flag&0xf0)==0 || !(sz&0xFFFFFF)) {
    input->setReadInverted(actEndian);
    return false;
  }

  int nBytesRead=0, szField=9;
  unsigned int read=0;
  size_t mapPos=0;
  data.reserve(size_t(sz));
  std::vector<std::vector<unsigned char> > mapToString;
  mapToString.reserve(size_t(entry.length()-6));
  bool ok=false;
  while (!input->isEnd()) {
    if ((int) mapPos==(1<<szField)-0x102)
      ++szField;
    if (input->tell()>=endPos) {
      MWAW_DEBUG_MSG(("RagTime5Parser::unpackZone: oops can not find last data\n"));
      ok=false;
      break;
    }
    do {
      read = (read<<8)+(unsigned int) input->readULong(1);
      nBytesRead+=8;
    }
    while (nBytesRead<szField);
    unsigned int val=(read >> (nBytesRead-szField));
    nBytesRead-=szField;
    read &= ((1<<nBytesRead)-1);

    if (val<0x100) {
      unsigned char c=(unsigned char) val;
      data.push_back(c);
      if (mapPos>= mapToString.size())
        mapToString.resize(mapPos+1);
      mapToString[mapPos++]=std::vector<unsigned char>(1,c);
      continue;
    }
    if (val==0x100) { // begin
      if (!data.empty()) {
        // data are reset when mapPos=3835, so it is ok
        mapPos=0;
        mapToString.resize(0);
        szField=9;
      }
      continue;
    }
    if (val==0x101) {
      ok=read==0;
      if (!ok) {
        MWAW_DEBUG_MSG(("RagTime5Parser::unpackZone: find 0x101 in bad position\n"));
      }
      break;
    }
    size_t readPos=size_t(val-0x102);
    if (readPos >= mapToString.size()) {
      MWAW_DEBUG_MSG(("RagTime5Parser::unpackZone: find bad position\n"));
      ok = false;
      break;
    }
    std::vector<unsigned char> final=mapToString[readPos++];
    if (readPos==mapToString.size())
      final.push_back(final[0]);
    else
      final.push_back(mapToString[readPos][0]);
    data.insert(data.end(), final.begin(), final.end());
    if (mapPos>= mapToString.size())
      mapToString.resize(mapPos+1);
    mapToString[mapPos++]=final;
  }

  if (ok && data.size()!=(size_t) sz) {
    MWAW_DEBUG_MSG(("RagTime5Parser::unpackZone: oops the data file is bad\n"));
    ok=false;
  }
  if (!ok) {
    MWAW_DEBUG_MSG(("RagTime5Parser::unpackZone: stop with mapPos=%ld and totalSize=%ld/%ld\n", long(mapPos), long(data.size()), long(sz)));
  }
  input->setReadInverted(actEndian);
  return ok;
}

bool RagTime5Parser::unpackZone(RagTime5Zone &zone)
{
  if (!zone.m_entry.valid())
    return false;

  std::vector<unsigned char> newData;
  if (!unpackZone(zone, zone.m_entry, newData))
    return false;
  long pos=zone.m_entry.begin(), endPos=zone.m_entry.end();
  MWAWInputStreamPtr input=zone.getInput();
  if (input->tell()!=endPos) {
    MWAW_DEBUG_MSG(("RagTime5Parser::unpackZone: find some extra data\n"));
    return false;
  }
  if (newData.empty()) {
    // empty zone
    zone.ascii().addPos(pos);
    zone.ascii().addNote("_");
    zone.m_entry.setLength(0);
    zone.m_extra += "packed,";
    return true;
  }

  if (input.get()==getInput().get())
    ascii().skipZone(pos, endPos-1);

  shared_ptr<MWAWStringStream> newStream(new MWAWStringStream(&newData[0], (unsigned int) newData.size()));
  MWAWInputStreamPtr newInput(new MWAWInputStream(newStream, false));
  zone.setInput(newInput);
  zone.m_entry.setBegin(0);
  zone.m_entry.setLength(newInput->size());
  zone.m_extra += "packed,";
  return true;
}

////////////////////////////////////////////////////////////
// read the different zones
////////////////////////////////////////////////////////////
bool RagTime5Parser::readDocumentVersion(RagTime5Zone &zone)
{
  MWAWInputStreamPtr input = zone.getInput();
  MWAWEntry &entry=zone.m_entry;

  zone.m_isParsed=true;
  ascii().addPos(zone.m_defPosition);
  ascii().addNote("doc[version],");

  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  f << "Entries(DocVersion):";
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
  if ((entry.length())%6!=2) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readDocumentVersion: the entry size seem bads\n"));
    f << "###";
    ascFile.addPos(entry.begin());
    ascFile.addNote(f.str().c_str());
    return true;
  }
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  int val=(int) input->readLong(1); // find 2-4
  f << "f0=" << val << ",";
  val=(int) input->readLong(1); // always 0
  if (val)
    f << "f1=" << val << ",";
  int N=int(entry.length()/6);
  for (int i=0; i<N; ++i) {
    // v0: last used version, v1: first used version, ... ?
    f << "v" << i << "=" << input->readLong(1);
    val = (int) input->readULong(1);
    if (val)
      f << "." << val;
    val = (int) input->readULong(1); // 20|60|80
    if (val != 0x80)
      f << ":" << std::hex << val << std::dec;
    for (int j=0; j<3; ++j) { // often 0 or small number
      val = (int) input->readULong(1);
      if (val)
        f << ":" << val << "[" << j << "]";
    }
    f << ",";
  }
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// find the different zones
////////////////////////////////////////////////////////////
bool RagTime5Parser::findDataZones(MWAWEntry const &entry)
{
  libmwaw::DebugStream f;
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (!input->checkPosition(entry.end())) {
    MWAW_DEBUG_MSG(("RagTime5Parser::findDataZones: main entry seems too bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }

  int n=0;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  while (!input->isEnd()) {
    pos=input->tell();
    if (pos>=entry.end()) break;
    int type=(int) input->readULong(1);
    if (type==0x18) {
      while (input->tell()<entry.end()) {
        if (input->readULong(1)==0xFF)
          continue;
        input->seek(-1, librevenge::RVNG_SEEK_CUR);
        break;
      }
      ascii().addPos(pos);
      ascii().addNote("_");
      continue;
    }
    f.str("");
    shared_ptr<RagTime5Zone> zone(new RagTime5Zone(input, ascii()));
    zone->m_defPosition=pos;
    switch (type) {
    case 1:
      zone->m_fileType=RagTime5Zone::F_Data;
      break;
    case 2:
      zone->m_fileType=RagTime5Zone::F_Main;
      break;
    case 3:
      zone->m_fileType=RagTime5Zone::F_Empty;
      break;
    default:
      zone->m_subType=type;
      break;
    }
    // type=3: 0001, 59-78 + sometimes g4=[_,1]
    if (pos+4>entry.end() || type < 1 || type > 3) {
      zone->m_extra=f.str();
      if (n++==0)
        f << "Entries(Zones)[1]:";
      else
        f << "Zones-" << n << ":";
      f << *zone << "###";
      MWAW_DEBUG_MSG(("RagTime5Parser::findDataZones: find unknown type\n"));
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      break;
    }
    for (int i=0; i<4-type; ++i) {
      zone->m_idsFlag[i]=(int) input->readULong(2); // alway 0/1?
      zone->m_ids[i]=(int) input->readULong(2);
    }
    bool ok=true;
    do {
      int type2=(int)  input->readULong(1);
      switch (type2) {
      case 4: // always 0, 1
      case 0xa: { // always 0, 0: never seens in v5 but frequent in v6
        ok = input->tell()+4+(type2==4 ? 1 : 0)<=entry.end();
        if (!ok) break;
        int data[2];
        for (int i=0; i<2; ++i)
          data[i]=(int) input->readULong(2);
        if (type2==4) {
          if (data[0]==0 && data[1]==1)
            f << "selected,";
          else if (data[0]==0)
            f << "#selected=" << data[1] << ",";
          else
            f << "#selected=[" << data[0] << "," << data[1] << "],";
        }
        else
          f << "g" << std::hex << type2 << std::dec << "=[" << data[0] << "," << data[1] << "],";
        break;
      }
      case 5:
      case 6: {
        ok = input->tell()+8+(type2==6 ? 1 : 0)<=entry.end();
        if (!ok) break;
        MWAWEntry zEntry;
        zEntry.setBegin((long) input->readULong(4));
        zEntry.setLength((long) input->readULong(4));
        zone->m_entriesList.push_back(zEntry);
        break;
      }
      case 9:
        ok=input->tell()<=entry.end();
        break;
      case 0xd: // always 0 || c000
        ok = input->tell()+4<=entry.end();
        if (!ok) break;
        for (int i=0; i<2; ++i)
          zone->m_variableD[i]=(int) input->readULong(2);
        break;
      case 0x18:
        while (input->tell()<entry.end()) {
          if (input->readULong(1)==0xFF)
            continue;
          input->seek(-1, librevenge::RVNG_SEEK_CUR);
          break;
        }
        ok=input->tell()+1<entry.end();
        break;
      default:
        ok=false;
        MWAW_DEBUG_MSG(("RagTime5Parser::findDataZones: find unknown type2=%d\n", type2));
        f << "type2=" << type2 << ",";
        break;
      }
      if (!ok || (type2&1) || (type2==0xa))
        break;
    }
    while (1);
    zone->m_extra=f.str();
    m_state->m_zonesList.push_back(zone);
    f.str("");
    if (n++==0)
      f << "Entries(Zones)[1]:";
    else
      f << "Zones-" << n << ":";
    f << *zone;
    if (!ok) {
      MWAW_DEBUG_MSG(("RagTime5Parser::findDataZones: find unknown data\n"));
      f << "###";
      if (input->tell()!=pos)
        ascii().addDelimiter(input->tell(),'|');
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      break;
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// color map
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool RagTime5Parser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = RagTime5ParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";
  if (!input->checkPosition(32)) {
    MWAW_DEBUG_MSG(("RagTime5Parser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,librevenge::RVNG_SEEK_SET);
  if (input->readULong(4)!=0x43232b44 || input->readULong(4)!=0xa4434da5
      || input->readULong(4)!=0x486472d7)
    return false;
  int val;
  for (int i=0; i<3; ++i) {
    val=(int) input->readLong(2);
    if (val!=i) f << "f" << i << "=" << val << ",";
  }
  val=(int) input->readLong(2); // always 0?
  if (val) f << "f3=" << val << ",";
  m_state->m_zonesEntry.setBegin((long) input->readULong(4));
  m_state->m_zonesEntry.setLength((long) input->readULong(4));
  if (m_state->m_zonesEntry.length()<137 ||
      !input->checkPosition(m_state->m_zonesEntry.begin()+137))
    return false;
  if (strict && !input->checkPosition(m_state->m_zonesEntry.end()))
    return false;
  val=(int) input->readLong(1);
  if (val==1)
    f << "compacted,";
  else if (val)
    f << "g0=" << val << ",";
  val=(int) input->readLong(1);
  setVersion(5);
  switch (val) {
  case 0:
    f << "vers=5,";
    break;
  case 4:
    f << "vers=6.5,";
    setVersion(6);
    break;
  default:
    f << "#vers=" << val << ",";
    break;
  }
  for (int i=0; i<2; ++i) {
    val=(int) input->readLong(1);
    if (val) f << "g" << i+1 << "=" << val << ",";
  }
  // ok, we can finish initialization
  if (header)
    header->reset(MWAWDocument::MWAW_T_RAGTIME, version());
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  ascii().addPos(input->tell());
  ascii().addNote("_");

  return true;
}

////////////////////////////////////////////////////////////
// send data to the listener
////////////////////////////////////////////////////////////

void RagTime5Parser::flushExtra()
{
  MWAWListenerPtr listener=getMainListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("RagTime5Parser::flushExtra: can not find the listener\n"));
    return;
  }
  // todo
}
/* cluster id
80860033fffffffb|00000000001660030000000b00000000000000000000000000000000000c000000020000000300000004000000050000000600000007:000100710001007500000008001000000000000348000000000000000000000000000020000000000001000000010001006100000009000000000000000a000500000000000000000000
80860033fffffffb|00000000001a60030000000900000000000000000000000000000000000c000000020000000300000004000000050000000600000007:0001008500000000000000000010000000000003480000000000000000000000000000200000000000010000000100000000000000080000000000000000000500000000000000000000
80860033fffffffb|00000000000960030000000000000000000000000000000000000000000000000002000000030000000400000005000000060000000700000000:00010088000000000010000000000003480000000000000000000000000000200000000000010000000100000000000000080000000000000009000500000000000000000000

114:000000 [Entries(ItemDta):Data114A[1],BESoftware:ItemData,BESoftware:ItemData,0<->8,packed,,]0001007100010085

 */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
