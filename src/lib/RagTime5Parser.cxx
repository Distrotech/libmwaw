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

#include "RagTime5ClusterManager.hxx"
#include "RagTime5Graph.hxx"
#include "RagTime5StructManager.hxx"
#include "RagTime5StyleManager.hxx"
#include "RagTime5Spreadsheet.hxx"
#include "RagTime5Text.hxx"

#include "RagTime5Parser.hxx"

/** Internal: the structures of a RagTime5Parser */
namespace RagTime5ParserInternal
{
//! Internal: the helper to read doc info parse
struct DocInfoFieldParser : public RagTime5StructManager::FieldParser {
  //! constructor
  DocInfoFieldParser(RagTime5Parser &parser) : RagTime5StructManager::FieldParser("DocInfo"),m_mainParser(parser)
  {
  }
  //! parse a field
  virtual bool parseField(RagTime5StructManager::Field &field, RagTime5Zone &zone, int /*n*/, libmwaw::DebugStream &f)
  {
    if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x1f7827) {
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Unstructured && child.m_fileType==0x32040 && child.m_entry.valid()) {
          f << child;

          long actPos=zone.getInput()->tell();
          m_mainParser.readDocInfoClusterData(zone, child.m_entry);
          zone.getInput()->seek(actPos, librevenge::RVNG_SEEK_SET);
          return true;
        }
        MWAW_DEBUG_MSG(("RagTime5ParserInternal::DocInfoFieldParser::parseField: find some unknown mainData block\n"));
        f << "##mainData=" << child << ",";
      }
    }
    else
      f << field;
    return true;
  }

protected:
  //! the main parser
  RagTime5Parser &m_mainParser;
};

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

//! Internal: the helper to read a clustList
struct ClustListParser : public RagTime5StructManager::DataParser {
  //! constructor
  ClustListParser(RagTime5ClusterManager &clusterManager, int fieldSize, std::string const &zoneName) :
    RagTime5StructManager::DataParser(zoneName), m_fieldSize(fieldSize), m_clusterList(), m_idToNameMap(), m_clusterManager(clusterManager)
  {
    if (m_fieldSize<4) {
      MWAW_DEBUG_MSG(("RagTime5ParserInternal::ClustListParser: bad field size\n"));
      m_fieldSize=0;
    }
  }

  std::string getClusterName(int id) const
  {
    return m_clusterManager.getClusterName(id);
  }

  //! try to parse a data
  bool parseData(MWAWInputStreamPtr &input, long endPos, RagTime5Zone &/*zone*/, int n, libmwaw::DebugStream &f)
  {
    long pos=input->tell();
    if (m_idToNameMap.find(n)!=m_idToNameMap.end())
      f << m_idToNameMap.find(n)->second.cstr() << ",";
    if (endPos-pos!=m_fieldSize) {
      MWAW_DEBUG_MSG(("RagTime5ParserInternal::ClustListParser::parse: bad data size\n"));
      return false;
    }
    std::vector<int> listIds;
    if (!RagTime5StructManager::readDataIdList(input, 1, listIds)) {
      MWAW_DEBUG_MSG(("RagTime5ParserInternal::ClustListParser::parse: can not read an cluster id\n"));
      f << "##clusterIds,";
      return false;
    }
    if (listIds[0]) {
      m_clusterList.push_back(listIds[0]);
      // a e,2003,200b, ... cluster
      f << getClusterName(listIds[0]) << ",";
    }
    if (m_fieldSize<10)
      return true;
    unsigned long lVal=input->readULong(4); // c00..small number
    if ((lVal&0xc0000000)==0xc0000000)
      f << "f0=" << (lVal&0x3fffffff) << ",";
    else
      f << "f0*" << lVal << ",";
    int val=(int) input->readLong(2); // 0|2
    if (val) f << "f1=" << val << ",";
    return true;
  }

  //! the field size
  int m_fieldSize;
  //! the list of read cluster
  std::vector<int> m_clusterList;
  //! the name
  std::map<int, librevenge::RVNGString> m_idToNameMap;
private:
  //! the main zone manager
  RagTime5ClusterManager &m_clusterManager;
  //! copy constructor, not implemented
  ClustListParser(ClustListParser &orig);
  //! copy operator, not implemented
  ClustListParser &operator=(ClustListParser &orig);
};

////////////////////////////////////////
//! Internal: the state of a RagTime5Parser
struct State {
  //! constructor
  State() : m_zonesEntry(), m_zonesList(), m_idToTypeMap(), m_dataIdZoneMap(), m_mainIdZoneMap(),
    m_pageZonesIdMap(), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }

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
  MWAWTextParser(input, rsrcParser, header), m_state(), m_graphParser(), m_spreadsheetParser(), m_textParser(), m_clusterManager(), m_structManager(), m_styleManager()
{
  init();
}

RagTime5Parser::~RagTime5Parser()
{
}

void RagTime5Parser::init()
{
  m_structManager.reset(new RagTime5StructManager);
  m_clusterManager.reset(new RagTime5ClusterManager(*this));
  m_styleManager.reset(new RagTime5StyleManager(*this));

  m_graphParser.reset(new RagTime5Graph(*this));
  m_spreadsheetParser.reset(new RagTime5Spreadsheet(*this));
  m_textParser.reset(new RagTime5Text(*this));
  resetTextListener();
  setAsciiName("main-1");

  m_state.reset(new RagTime5ParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}

shared_ptr<RagTime5ClusterManager> RagTime5Parser::getClusterManager()
{
  return m_clusterManager;
}

shared_ptr<RagTime5StructManager> RagTime5Parser::getStructManager()
{
  return m_structManager;
}

shared_ptr<RagTime5StyleManager> RagTime5Parser::getStyleManager()
{
  return m_styleManager;
}

bool RagTime5Parser::readChartCluster(RagTime5Zone &zone, int zoneType)
{
  return m_spreadsheetParser->readChartCluster(zone, zoneType);
}

bool RagTime5Parser::readGraphicCluster(RagTime5Zone &zone, int zoneType)
{
  return m_graphParser->readGraphicCluster(zone, zoneType);
}

bool RagTime5Parser::readPictureCluster(RagTime5Zone &zone, int zoneType)
{
  return m_graphParser->readPictureCluster(zone, zoneType);
}

bool RagTime5Parser::readSpreadsheetCluster(RagTime5Zone &zone, int zoneType)
{
  return m_spreadsheetParser->readSpreadsheetCluster(zone, zoneType);
}

bool RagTime5Parser::readTextCluster(RagTime5Zone &zone, int zoneType)
{
  return m_textParser->readTextCluster(zone, zoneType);
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
  if (!getInput().get() || !checkHeader(0L))  throw(libmwaw::ParseException());
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
      flushExtra();
#endif
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
    if (!zone || zone->m_variableD[0]!=1) {
      MWAW_DEBUG_MSG(("RagTime5Parser::createZones: the main zone 10 seems bads\n"));
      continue;
    }
    shared_ptr<RagTime5Zone> dZone=getDataZone(zone->m_variableD[1]);
    if (!dZone || !dZone->m_entry.valid()) {
      MWAW_DEBUG_MSG(("RagTime5Parser::createZones: can not find the type zone\n"));
      continue;
    }
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

  // read the clusters
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

  return true;
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
    // FIXME: read these zones before
    if (zone.m_fileType==RagTime5Zone::F_Main && readStructMainZone(zone))
      return true;
    if (zone.m_entry.length()==164 && zone.m_fileType==RagTime5Zone::F_Data)
      name="ZoneUnkn0";
    else {
      name="ItemDta";
      // checkme: often Data22 is not parsed, but there can be others
      MWAW_DEBUG_MSG(("RagTime5Parser::readZoneData: find a unparsed %s zone %d\n", zone.m_fileType==RagTime5Zone::F_Data ? "data" : "main", zone.m_ids[0]));
    }
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

bool RagTime5Parser::readUnicodeStringList(RagTime5ClusterManager::Link const &link, std::map<int, librevenge::RVNGString> &idToStringMap)
{
  RagTime5ParserInternal::IndexUnicodeParser dataParser(*this, false, "UnicodeList");
  if (!readListZone(link, dataParser))
    return false;
  idToStringMap=dataParser.m_idToStringMap;
  return true;
}

bool RagTime5Parser::readLongListWithSize(int dataId, int fSz, std::vector<long> &listPosition, std::string const &zoneName)
{
  listPosition.clear();
  if (!dataId || fSz<=0 || fSz>4)
    return false;

  shared_ptr<RagTime5Zone> zone=getDataZone(dataId);
  if (!zone || !zone->m_entry.valid() || (zone->m_entry.length()%fSz) ||
      zone->getKindLastPart(zone->m_kinds[1].empty())!="ItemData") {
    MWAW_DEBUG_MSG(("RagTime5Parser::readLongListWithSize: the zone %d seems bad\n", dataId));
    return false;
  }
  MWAWEntry entry=zone->m_entry;
  MWAWInputStreamPtr input=zone->getInput();
  input->setReadInverted(!zone->m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);

  zone->m_isParsed=true;
  libmwaw::DebugStream f;
  if (!zoneName.empty())
    f << "Entries(" << zoneName << ")[" << *zone << "]:";
  else
    f << "Entries(ListLong" << fSz << ")[" << *zone << "]:";
  int N=int(entry.length()/fSz);
  for (int i=0; i<N; ++i) {
    long ptr=input->readLong(fSz);
    listPosition.push_back(ptr);
    if (ptr)
      f << ptr << ",";
    else
      f << "_,";
  }
  input->setReadInverted(false);
  zone->ascii().addPos(entry.begin());
  zone->ascii().addNote(f.str().c_str());
  zone->ascii().addPos(entry.end());
  zone->ascii().addNote("_");
  return true;
}

bool RagTime5Parser::readLongList(RagTime5ClusterManager::Link const &link, std::vector<long> &list)
{
  if (!link.m_ids.empty() && link.m_ids[0] &&
      readLongListWithSize(link.m_ids[0], link.m_fieldSize, list, link.m_name))
    return true;
  list=link.m_longList;
  return !list.empty();
}

bool RagTime5Parser::readPositions(int posId, std::vector<long> &listPosition)
{
  return readLongListWithSize(posId, 4, listPosition, "Positions");
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
    if (!zone || zone->m_variableD[0]!=1) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZones: the main cluster zone seems bads\n"));
      continue;
    }
    // the main cluster
    shared_ptr<RagTime5Zone> dZone=getDataZone(zone->m_variableD[1]);
    if (!dZone) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZones: can not find the main cluster zone\n"));
      continue;
    }
    dZone->m_extra+="main11,";
    if (dZone->getKindLastPart(dZone->m_kinds[1].empty())!="Cluster" || !readClusterZone(*dZone, 0)) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZones: unexpected main cluster zone type\n"));
      continue;
    }
    ++numZones;
  }
  if (numZones!=1) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZones: parses %d main11 zone, we may have a problem\n", numZones));
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

bool RagTime5Parser::readClusterRootData(RagTime5ClusterManager::ClusterRoot &cluster)
{
  // first read the list of child cluster and update the list of cluster for the cluster manager
  std::vector<int> listClusters;
  for (size_t i=0; i<m_state->m_zonesList.size(); ++i) {
    if (!m_state->m_zonesList[i])
      continue;
    RagTime5Zone &zone=*m_state->m_zonesList[i];
    if (zone.m_isParsed || !zone.m_entry.valid() || zone.getKindLastPart(zone.m_kinds[1].empty())!="Cluster")
      continue;
    listClusters.push_back(zone.m_ids[0]);
  }

  if (cluster.m_listClusterId==0) {
    MWAW_DEBUG_MSG(("RagTime5ClusterManager::readClusterRootData: cluster list id is not set, try zone id+1\n"));
    cluster.m_listClusterId=cluster.m_zoneId+1;
  }
  std::vector<int> listChilds;
  m_clusterManager->readClusterMainList(cluster, listChilds, listClusters);

  std::set<int> seens;
  // the list of graphic type
  if (!cluster.m_graphicTypeLink.empty() && m_graphParser->readGraphicTypes(cluster.m_graphicTypeLink)) {
    if (cluster.m_graphicTypeLink.m_ids.size()>2 && cluster.m_graphicTypeLink.m_ids[1])
      seens.insert(cluster.m_graphicTypeLink.m_ids[1]);
  }
  // the different styles ( beginning with colors, then graphic styles and text styles )
  for (int i=0; i<8; ++i) {
    int const(order[])= {7, 6, 1, 2, 0, 4, 3, 5};
    int cId=cluster.m_styleClusterIds[order[i]];
    if (!cId) continue;

    int const(wh[])= {0x480, 0x480, 0x480, 0x480, 0x480, -1, 0x480, 0x8042};
    shared_ptr<RagTime5Zone> dZone= getDataZone(cId);
    if (!dZone || dZone->getKindLastPart(dZone->m_kinds[1].empty())!="Cluster" || !readClusterZone(*dZone, wh[order[i]])) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterRootData: can not find cluster style zone %d\n", cId));
      continue;
    }
    seens.insert(cId);
  }
  //! the field def cluster list
  if (!cluster.m_listClusterLink[1].empty()) {
    RagTime5ParserInternal::ClustListParser parser(*m_clusterManager.get(), 4, "RootClustLst1");
    readFixedSizeZone(cluster.m_listClusterLink[1], parser);
    // TODO: read the field cluster's data here
  }
  // now the main cluster list
  for (int i=0; i<1; ++i) {
    int cId=cluster.m_clusterIds[i];
    if (cId==0) continue;
    shared_ptr<RagTime5Zone> data=getDataZone(cId);
    if (!data || !data->m_entry.valid() || data->getKindLastPart(data->m_kinds[1].empty())!="Cluster") {
      MWAW_DEBUG_MSG(("RagTime5ClusterManager::readClusterRootData: the cluster zone %d seems bad\n", cId));
      continue;
    }
    int const(wh[])= {0x10000};
    if (readClusterZone(*data, wh[i]))
      seens.insert(cId);
  }
  if (!cluster.m_fieldClusterLink.empty())
    m_clusterManager->readFieldClusters(cluster.m_fieldClusterLink);
  for (int wh=0; wh<2; ++wh) {
    std::vector<RagTime5ClusterManager::Link> const &list=wh==0 ? cluster.m_conditionFormulaLinks : cluster.m_settingLinks;
    for (size_t i=0; i<list.size(); ++i) {
      if (list[i].empty()) continue;
      RagTime5ClusterManager::Cluster unknCluster;
      unknCluster.m_dataLink=list[i];
      RagTime5StructManager::FieldParser defaultParser(wh==0 ? "CondFormula" : "Settings");
      readStructZone(unknCluster, defaultParser, 0);
    }
  }
  if (!cluster.m_docInfoLink.empty()) {
    RagTime5ClusterManager::Cluster unknCluster;
    unknCluster.m_dataLink=cluster.m_docInfoLink;
    RagTime5ParserInternal::DocInfoFieldParser parser(*this);
    readStructZone(unknCluster, parser, 18);
  }
  // unknown link
  if (!cluster.m_linkUnknown.empty()) { // defined by always with no data...
    RagTime5StructManager::DataParser parser("UnknZoneC");
    readListZone(cluster.m_linkUnknown, parser);
  }
  // now read the not parsed childs
  for (size_t i=0; i<listChilds.size(); ++i) {
    int cId=listChilds[i];
    if (cId==0 || seens.find(cId)!=seens.end())
      continue;
    shared_ptr<RagTime5Zone> dZone= getDataZone(cId);
    if (!dZone || dZone->getKindLastPart(dZone->m_kinds[1].empty())!="Cluster" || !readClusterZone(*dZone)) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterRootData: can not find cluster zone %d\n", cId));
      continue;
    }
    seens.insert(cId);
  }

  for (size_t i=0; i<cluster.m_linksList.size(); ++i) {
    RagTime5ClusterManager::Link const &link=cluster.m_linksList[i];
    if (link.m_type==RagTime5ClusterManager::Link::L_List) {
      readListZone(link);
      continue;
    }
    else if (link.m_type==RagTime5ClusterManager::Link::L_LongList) {
      std::vector<long> list;
      readLongList(link, list);
      continue;
    }
    else if (link.m_type==RagTime5ClusterManager::Link::L_LinkDef) {
      m_textParser->readLinkZones(cluster, link);
      continue;
    }
    else if (link.m_type==RagTime5ClusterManager::Link::L_UnknownClusterC) {
      m_clusterManager->readUnknownClusterC(link);
      continue;
    }

    if (link.empty()) continue;
    shared_ptr<RagTime5Zone> data=getDataZone(link.m_ids[0]);
    if (!data || data->m_isParsed) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterRootData: can not find data zone %d\n", link.m_ids[0]));
      continue;
    }
    data->m_hiLoEndian=cluster.m_hiLoEndian;
    if (link.m_fieldSize==0 && !data->m_entry.valid())
      continue;
    switch (link.m_type) {
    case RagTime5ClusterManager::Link::L_FieldsList:
    case RagTime5ClusterManager::Link::L_List:
    case RagTime5ClusterManager::Link::L_LongList:
    case RagTime5ClusterManager::Link::L_UnicodeList:
    case RagTime5ClusterManager::Link::L_UnknownClusterC:
      break;
    case RagTime5ClusterManager::Link::L_ClusterLink:
      readClusterLinkList(*data, link);
      break;
    case RagTime5ClusterManager::Link::L_LinkDef:
    case RagTime5ClusterManager::Link::L_Unknown:
    default:
      readFixedSizeZone(link, "");
      break;
    }
  }

  return true;
}

bool RagTime5Parser::checkClusterList(std::vector<int> const &list)
{
  bool ok=true;
  for (size_t j=0; j<list.size(); ++j) {
    int cId=list[j];
    if (cId==0) continue;
    shared_ptr<RagTime5Zone> data=getDataZone(cId);
    if (!data || !data->m_entry.valid() || data->getKindLastPart(data->m_kinds[1].empty())!="Cluster") {
      MWAW_DEBUG_MSG(("RagTime5ClusterManager::checkClusterList: the cluster zone %d seems bad\n", cId));
      ok=false;
    }
  }
  return ok;
}

bool RagTime5Parser::readClusterZone(RagTime5Zone &zone, int zoneType)
{
  shared_ptr<RagTime5ClusterManager::Cluster> cluster;
  if (!m_clusterManager->readCluster(zone, cluster, zoneType))
    return false;
  if (!cluster)
    return true;
  checkClusterList(cluster->m_clusterIdsList);

  if (cluster->m_type==RagTime5ClusterManager::Cluster::C_Root) {
    RagTime5ClusterManager::ClusterRoot *root=dynamic_cast<RagTime5ClusterManager::ClusterRoot *>(cluster.get());
    if (!root) {
      MWAW_DEBUG_MSG(("RagTime5ClusterManager::readClusterZone: can not find the root pointer\n"));
      return false;
    }
    readClusterRootData(*root);
    return true;
  }
  if (cluster->m_type==RagTime5ClusterManager::Cluster::C_Layout) {
    RagTime5ClusterManager::ClusterLayout *clust=dynamic_cast<RagTime5ClusterManager::ClusterLayout *>(cluster.get());
    if (!clust) {
      MWAW_DEBUG_MSG(("RagTime5ClusterManager::readClusterZone: can not find the layout pointer\n"));
      return false;
    }
    readClusterLayoutData(*clust);
    return true;
  }
  if (cluster->m_type==RagTime5ClusterManager::Cluster::C_Pipeline)
    return readClusterPipelineData(*cluster);
  else if (cluster->m_type==RagTime5ClusterManager::Cluster::C_Script) {
    RagTime5ClusterManager::ClusterScript *clust=dynamic_cast<RagTime5ClusterManager::ClusterScript *>(cluster.get());
    if (!clust) {
      MWAW_DEBUG_MSG(("RagTime5ClusterManager::readClusterZone: can not find the script pointer\n"));
      return false;
    }
    else
      return readClusterScriptData(*clust);
  }
  else if (cluster->m_type==RagTime5ClusterManager::Cluster::C_ClusterB)
    return readUnknownClusterBData(*cluster);
  else if (cluster->m_type==RagTime5ClusterManager::Cluster::C_ClusterC)
    return readUnknownClusterCData(*cluster);
  else if (cluster->m_type==RagTime5ClusterManager::Cluster::C_ColorPattern)
    return m_graphParser->readColorPatternZone(*cluster);
  else if (cluster->m_type==RagTime5ClusterManager::Cluster::C_Fields)
    return readClusterFieldsData(*cluster);
  else if (cluster->m_type==RagTime5ClusterManager::Cluster::C_FormatStyles)
    return readFormats(*cluster);
  else if (cluster->m_type==RagTime5ClusterManager::Cluster::C_ColorStyles)
    return m_styleManager->readGraphicColors(*cluster);
  else if (cluster->m_type==RagTime5ClusterManager::Cluster::C_GraphicStyles)
    return m_styleManager->readGraphicStyles(*cluster);
  else if (cluster->m_type==RagTime5ClusterManager::Cluster::C_TextStyles)
    return m_styleManager->readTextStyles(*cluster);
  else if (cluster->m_type==RagTime5ClusterManager::Cluster::C_UnitStyles) {
    RagTime5StructManager::FieldParser defaultParser("Units");
    return readStructZone(*cluster, defaultParser, 14);
  }

  if (!cluster->m_nameLink.empty()) {
    std::map<int, librevenge::RVNGString> idToStringMap;
    readUnicodeStringList(cluster->m_nameLink, idToStringMap);
  }

  for (size_t i=0; i<cluster->m_linksList.size(); ++i) {
    RagTime5ClusterManager::Link const &link=cluster->m_linksList[i];
    if (link.m_type==RagTime5ClusterManager::Link::L_List)
      readListZone(link);
    else
      readFixedSizeZone(link, "");
  }
  return true;
}

bool RagTime5Parser::readClusterLinkList
(RagTime5ClusterManager::Link const &link, RagTime5ClusterManager::Link const &nameLink,
 std::vector<int> &list, std::string const &name)
{
  RagTime5ParserInternal::ClustListParser parser(*m_clusterManager.get(), 10, !name.empty() ? name : link.getZoneName());
  if (!nameLink.empty())
    readUnicodeStringList(nameLink, parser.m_idToNameMap);
  if (!link.empty())
    readListZone(link, parser);
  list=parser.m_clusterList;
  return true;
}

bool RagTime5Parser::readClusterLinkList(RagTime5Zone &zone, RagTime5ClusterManager::Link const &link)
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
  std::string zoneName=link.m_name.empty() ? "ClustLink" : link.m_name;
  f << "Entries(" << zoneName << ")[" << zone << "]:";
  if (link.m_N*link.m_fieldSize>zone.m_entry.length() || link.m_fieldSize!=12) {
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
    f << zoneName << "-" << i << ":";
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
      f << m_clusterManager->getClusterName(listIds[0]) << ",";
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
  if (input->tell()!=zone.m_entry.end()) {
    f.str("");
    f << zoneName << ":end";
    ascFile.addPos(input->tell());
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// structured zone
////////////////////////////////////////////////////////////
bool RagTime5Parser::readFormats(RagTime5ClusterManager::Cluster &cluster)
{
  RagTime5ClusterManager::Link const &link=cluster.m_dataLink;
  if (link.m_ids.size()<2 || !link.m_ids[1])
    return false;

  std::map<int, librevenge::RVNGString> idToNameMap;
  if (!cluster.m_nameLink.empty()) {
    readUnicodeStringList(cluster.m_nameLink, idToNameMap);
    cluster.m_nameLink=RagTime5ClusterManager::Link();
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
    RagTime5ClusterManager::Link const &lnk=cluster.m_linksList[i];
    std::string name("Form");
    name += (lnk.m_fileType[0]==0x3e800 ? "A" : lnk.m_fileType[0]==0x35800 ? "B" : lnk.getZoneName().c_str());
    readFixedSizeZone(lnk, name);
  }

  return true;
}

bool RagTime5Parser::readClusterFieldsData(RagTime5ClusterManager::Cluster &cluster)
{
  RagTime5ClusterManager::Link const &link=cluster.m_dataLink;
  m_textParser->readFieldZones(cluster, link, link.m_fileType[0]==0x20000);

  for (size_t i=0; i<cluster.m_linksList.size(); ++i) {
    RagTime5ClusterManager::Link const &lnk=cluster.m_linksList[i];
    readFixedSizeZone(lnk, "Data2_FieldUnkn");
  }

  return true;
}

bool RagTime5Parser::readDocInfoClusterData(RagTime5Zone &zone, MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<160) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readDocInfoClusterData: the entry does not seems valid\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  MWAWInputStreamPtr input=zone.getInput();
  long pos=entry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  f << "DocInfo[dataA]:";
  // checkme the field data seems always in hilo endian...
  bool actEndian=input->readInverted();
  input->setReadInverted(false);

  int val=(int) input->readULong(2); // always 0
  if (val) f << "f0=" << val;
  long dataSz=(long) input->readULong(4);
  if (pos+dataSz>entry.end()) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readDocInfoClusterData: the main data size seems bad\n"));
    f << "###dSz=" << dataSz << ",";
    ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    input->setReadInverted(actEndian);
    return true;
  }
  for (int i=0; i<2; ++i) { // f1=2
    val=(int) input->readULong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  int sSz=(int) input->readULong(1);
  long actPos=input->tell();
  if (sSz>25) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readDocInfoClusterData: the dataA string size seems bad\n"));
    f << "###sSz=" << sSz << ",";
    sSz=0;
  }
  std::string text("");
  for (int i=0; i<sSz; ++i) text += (char) input->readULong(1);
  f << text << ",";
  input->seek(actPos+25, librevenge::RVNG_SEEK_SET);
  f << "IDS=["; // maybe some char
  for (int i=0; i<7; ++i) { // _, ?, ?, ?, 0, 0|4, ?
    val=(int) input->readULong(2);
    if (val) f << std::hex << val << std::dec << ",";
    else f << "_,";
  }
  f << "],";
  sSz=(int) input->readULong(1);
  actPos=input->tell();
  if (sSz>62) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readDocInfoClusterData: the dataA string2 size seems bad\n"));
    f << "###sSz2=" << sSz << ",";
    sSz=0;
  }
  text=("");
  for (int i=0; i<sSz; ++i) text += (char) input->readULong(1);
  f << text << ",";
  input->seek(actPos+63, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "DocInfo[dataB]:";
  f << "IDS=["; // maybe some char
  for (int i=0; i<8; ++i) {
    val=(int) input->readULong(2);
    if (val) f << std::hex << val << std::dec << ",";
    else f << "_,";
  }
  f << "],";
  for (int i=0; i<11; ++i) { // f0=-1|2|6, f1=-1|2|4, f3=0|17|21,
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  val=(int) input->readLong(1); // 0
  if (val) f << "f11=" << val << ",";
  sSz=(int) input->readULong(1);
  if (sSz>64||pos+sSz+4>entry.end()) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readDocInfoClusterData: the string size for dataB data seems bad\n"));
    f << "###sSz3=" << sSz << ",";
    ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    input->setReadInverted(actEndian);
    return true;
  }
  text=("");
  for (int i=0; i<sSz; ++i) text += (char) input->readULong(1);
  f << text << ",";
  if ((sSz%2)==1)
    input->seek(1, librevenge::RVNG_SEEK_CUR);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "DocInfo[dataC]:";
  if (input->readLong(2)!=1 || (val=(int)input->readLong(2))<=0 || (val%4) || pos+6+val>entry.end()) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readDocInfoClusterData: oops something is bad[dataC]\n"));
    f << "###val=" << val << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    input->setReadInverted(actEndian);
    return true;
  }
  int N=val/4;
  f << "list=[";
  for (int i=0; i<N; ++i) {
    val=(int) input->readLong(4);
    if (val)
      f << std::hex << val << std::dec << ",";
    else
      f << "_,";
  }
  f << "],";
  val=(int) input->readLong(2); // always 2
  if (val!=2) f << "f0=" << val << ",";
  sSz=(int) input->readULong(2);
  if (input->tell()+sSz+4>entry.end()) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readDocInfoClusterData: string size seems bad[dataC]\n"));
    f << "###sSz=" << sSz << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    input->setReadInverted(actEndian);
    return true;
  }
  text=("");
  for (int i=0; i<sSz; ++i) text += (char) input->readULong(1);
  f << text << ",";
  if ((sSz%2)==1)
    input->seek(1, librevenge::RVNG_SEEK_CUR);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "DocInfo[dataD]:";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  input->setReadInverted(actEndian);
  return true;
}

bool RagTime5Parser::readClusterLayoutData(RagTime5ClusterManager::ClusterLayout &cluster)
{
  if (!cluster.m_listItemLink.empty())
    readFixedSizeZone(cluster.m_listItemLink, "LayoutUnknown0");
  if (!cluster.m_pipelineLink.empty() && cluster.m_pipelineLink.m_ids.size()==1) {
    if (cluster.m_pipelineLink.m_fieldSize==4) {
      RagTime5ParserInternal::ClustListParser parser(*m_clusterManager, 4, "LayoutPipeline");
      readFixedSizeZone(cluster.m_pipelineLink, parser);
      checkClusterList(parser.m_clusterList);
    }
    else {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterLayoutData: find unexpected field size for pipeline data\n"));
      readFixedSizeZone(cluster.m_pipelineLink, "LayoutPipelineBAD");
    }
  }
  // can have some setting
  for (int wh=0; wh<2; ++wh) {
    std::vector<RagTime5ClusterManager::Link> const &list=wh==0 ? cluster.m_conditionFormulaLinks : cluster.m_settingLinks;
    for (size_t i=0; i<list.size(); ++i) {
      if (list[i].empty()) continue;
      RagTime5ClusterManager::Cluster unknCluster;
      unknCluster.m_dataLink=list[i];
      RagTime5StructManager::FieldParser defaultParser(wh==0 ? "CondFormula" : "Settings");
      readStructZone(unknCluster, defaultParser, 0);
    }
  }

  if (!cluster.m_nameLink.empty()) {
    std::map<int, librevenge::RVNGString> idToStringMap;
    readUnicodeStringList(cluster.m_nameLink, idToStringMap);
  }

  for (size_t i=0; i<cluster.m_linksList.size(); ++i) {
    RagTime5ClusterManager::Link const &lnk=cluster.m_linksList[i];
    if (lnk.m_type==RagTime5ClusterManager::Link::L_List) {
      readListZone(lnk);
      continue;
    }
    std::stringstream s;
    s << "Layout_Data" << lnk.m_fieldSize;
    RagTime5StructManager::DataParser defaultParser(s.str());
    readFixedSizeZone(lnk, defaultParser);
  }

  return true;
}

bool RagTime5Parser::readClusterPipelineData(RagTime5ClusterManager::Cluster &cluster)
{
  if (cluster.m_dataLink.empty()) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readClusterPipelineData: can not find the main data\n"));
  }
  else {
    RagTime5StructManager::DataParser defaultParser("PipelineUnknown0");
    readFixedSizeZone(cluster.m_dataLink, defaultParser);
  }

  for (size_t i=0; i<cluster.m_linksList.size(); ++i) {
    RagTime5ClusterManager::Link const &lnk=cluster.m_linksList[i];
    RagTime5StructManager::DataParser defaultParser("PipelineUnknown1");
    readFixedSizeZone(lnk, defaultParser);
  }

  return true;
}

bool RagTime5Parser::readClusterScriptData(RagTime5ClusterManager::ClusterScript &cluster)
{
  if (!cluster.m_scriptComment.empty() && !cluster.m_scriptComment.m_ids.empty()) {
    shared_ptr<RagTime5Zone> dataZone=getDataZone(cluster.m_scriptComment.m_ids[0]);
    if (!dataZone || !dataZone->m_entry.valid() ||
        dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="Unicode") {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterScriptData: the script comment zone %d seems bad\n", cluster.m_scriptComment.m_ids[0]));
    }
    else
      readUnicodeString(*dataZone);
  }

  std::vector<int> listCluster;
  readClusterLinkList(cluster.m_dataLink, cluster.m_nameLink, listCluster, "ScriptClustLst");
  checkClusterList(listCluster);

  for (size_t i=0; i<cluster.m_linksList.size(); ++i) {
    RagTime5ClusterManager::Link const &lnk=cluster.m_linksList[i];
    if (lnk.m_type==RagTime5ClusterManager::Link::L_List) {
      readListZone(lnk);
      continue;
    }
    std::stringstream s;
    s << "DataScript_" << lnk.m_fieldSize;
    RagTime5StructManager::DataParser defaultParser(s.str());
    readFixedSizeZone(lnk, defaultParser);
  }

  return true;
}

bool RagTime5Parser::readUnknownClusterBData(RagTime5ClusterManager::Cluster &cluster)
{
  RagTime5ClusterManager::Link const &link=cluster.m_dataLink;
  if (link.m_ids.size()<2 || !link.m_ids[1]) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readUnknownClusterBData: can not find the main data\n"));
    return false;
  }
  libmwaw::DebugStream f;
  if (link.m_type==RagTime5ClusterManager::Link::L_List) {
    RagTime5StructManager::FieldParser defaultParser("UnknBUnknown0");
    readStructZone(cluster, defaultParser, 8);
  }
  else {
    shared_ptr<RagTime5Zone> dataZone=getDataZone(link.m_ids[1]);
    if (!dataZone || !dataZone->m_entry.valid() ||
        dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData") {
      MWAW_DEBUG_MSG(("RagTime5Parser::readUnknownClusterBData: the data zone %d seems bad\n", link.m_ids[1]));
      return false;
    }

    MWAW_DEBUG_MSG(("RagTime5Parser::readUnknownClusterBData: unexpected type for zone %d\n", link.m_ids[1]));
    libmwaw::DebugFile &ascFile=dataZone->ascii();
    long pos=dataZone->m_entry.begin();
    dataZone->m_isParsed=true;
    f << "Entries(UnknBUnknown1)[" << *dataZone << "]:" << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  for (size_t i=0; i<cluster.m_linksList.size(); ++i) {
    RagTime5ClusterManager::Link const &lnk=cluster.m_linksList[i];
    RagTime5StructManager::DataParser defaultParser("UnknBUnknown2");
    readFixedSizeZone(lnk, defaultParser);
  }

  return true;
}

bool RagTime5Parser::readUnknownClusterCData(RagTime5ClusterManager::Cluster &cluster)
{
  RagTime5ClusterManager::Link const &link=cluster.m_dataLink;
  if (link.m_ids.empty()) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readUnknownClusterCData: can not find the main data\n"));
    return false;
  }
  std::stringstream s;
  s << "UnknC_" << char('A'+link.m_fileType[0]) << "_";
  std::string zoneName=s.str();

  if (link.m_type==RagTime5ClusterManager::Link::L_List) {
    if (link.m_fileType[1]==0x310) {
      // find id=8,"Rechenblatt 1": spreadsheet name ?
      RagTime5ParserInternal::IndexUnicodeParser parser(*this, true, zoneName+"0");
      readListZone(link, parser);
    }
    else {
      RagTime5StructManager::DataParser parser(zoneName+"0");
      readListZone(link, parser);
    }
  }
  else {
    RagTime5StructManager::DataParser defaultParser(zoneName+"0");
    readFixedSizeZone(link, defaultParser);
  }
  for (size_t i=0; i<cluster.m_linksList.size(); ++i) {
    RagTime5ClusterManager::Link const &lnk=cluster.m_linksList[i];
    RagTime5StructManager::DataParser parser(zoneName+"1");
    readFixedSizeZone(lnk, parser);
  }

  return true;
}

bool RagTime5Parser::readListZone(RagTime5ClusterManager::Link const &link)
{
  // fixme: must not be here
  if (link.m_fileType[0]==0 && link.m_fileType[1]==0x310) {
    RagTime5ParserInternal::IndexUnicodeParser parser(*this, true, "UnicodeIndexList");
    return readListZone(link, parser);
  }
  RagTime5StructManager::DataParser parser(link.getZoneName());
  return readListZone(link, parser);
}

bool RagTime5Parser::readListZone(RagTime5ClusterManager::Link const &link, RagTime5StructManager::DataParser &parser)
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

bool RagTime5Parser::readFixedSizeZone(RagTime5ClusterManager::Link const &link, std::string const &name)
{
  RagTime5StructManager::DataParser parser(name.empty() ? link.getZoneName() : name);
  return readFixedSizeZone(link, parser);
}

bool RagTime5Parser::readFixedSizeZone(RagTime5ClusterManager::Link const &link, RagTime5StructManager::DataParser &parser)
{
  if (link.m_ids.empty() || !link.m_ids[0])
    return false;

  int const dataId=link.m_ids[0];
  shared_ptr<RagTime5Zone> dataZone=getDataZone(dataId);

  if (!dataZone || !dataZone->m_entry.valid() ||
      dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData" ||
      link.m_fieldSize<=0 || link.m_N*link.m_fieldSize>dataZone->m_entry.length()) {
    if (link.m_N*link.m_fieldSize==0 && !dataZone->m_entry.valid()) {
      // a zone with 0 zone is ok...
      dataZone->m_isParsed=true;
      return true;
    }
    MWAW_DEBUG_MSG(("RagTime5Parser::readFixedSizeZone: the data zone %d seems bad\n", dataId));
    if (dataZone && dataZone->m_entry.valid()) {
      libmwaw::DebugFile &ascFile=dataZone->ascii();
      libmwaw::DebugStream f;
      f << "Entries(" << parser.getZoneName() << ")[" << *dataZone << "]:###";
      ascFile.addPos(dataZone->m_entry.begin());
      ascFile.addNote(f.str().c_str());
    }
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
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  long endPos=entry.end();

  for (int i=0; i<link.m_N; ++i) {
    long pos=input->tell();
    f.str("");
    f << parser.getZoneName(i+1) << ":";
    if (!parser.parseData(input, pos+link.m_fieldSize, *dataZone, i+1, f))
      f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+link.m_fieldSize, librevenge::RVNG_SEEK_SET);
  }
  long pos=input->tell();
  if (pos!=endPos) {
    f.str("");
    f << parser.getZoneName() << ":#end";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->setReadInverted(false);
  return true;
}

bool RagTime5Parser::readStructZone(RagTime5ClusterManager::Cluster &cluster, RagTime5StructManager::FieldParser &parser, int headerSz)
{
  RagTime5ClusterManager::Link const &link=cluster.m_dataLink;
  if (link.m_ids.size()<2 || !link.m_ids[1])
    return false;

  std::map<int, librevenge::RVNGString> idToNameMap;
  if (!cluster.m_nameLink.empty()) {
    readUnicodeStringList(cluster.m_nameLink, idToNameMap);
    cluster.m_nameLink=RagTime5ClusterManager::Link();
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
      if (dataZone)
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
      if (!readStructData(*dataZone, endPos, id, headerSz, parser, name)) {
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
      readStructData(*dataZone, debPos+nextPos, i+1, headerSz, parser, name);
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

bool RagTime5Parser::readStructMainZone(RagTime5Zone &zone)
{
  MWAWEntry entry=zone.m_entry;
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  MWAWInputStreamPtr input=zone.getInput();
  input->setReadInverted(!zone.m_hiLoEndian);
  long debPos=entry.begin();
  long endPos=entry.end();

  input->seek(debPos, librevenge::RVNG_SEEK_SET);
  int n=0;
  while (input->tell()+8 < endPos) {
    long pos=input->tell();
    RagTime5StructManager::Field field;
    long type=(long) input->readULong(4);
    if (n==0 && type==0x5a610600) { // rare, 3 can be good in one file and 1 bad, so...
      MWAW_DEBUG_MSG(("RagTime5Parser::readStructMainZone: endian seems bad, reverts it\n"));
      input->setReadInverted(zone.m_hiLoEndian);
      ascFile.addPos(pos);
      ascFile.addNote("###badEndian,");
      type=0x6615a;
    }
    int fSz=(int) input->readULong(1);
    if (pos+5+fSz>endPos || !m_structManager->readField(input, endPos, ascFile, field, fSz)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      if (n==0) {
        input->setReadInverted(false);
        return false;
      }
      break;
    }
    f.str("");
    f << "MainUnknown-" << ++n << "]:list" << std::hex << type << std::dec << "=[" << field << "]";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+5+fSz, librevenge::RVNG_SEEK_SET);
  }

  long pos=input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readStructMainZone: find extra data\n"));
    ascFile.addPos(pos);
    ascFile.addNote("MainUnknown:##extra");
  }
  zone.m_isParsed=true;
  f.str("");
  f << "Entries(MainUnknown)[" << zone << "]:";
  ascFile.addPos(debPos);
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(endPos);
  ascFile.addNote("_");
  input->setReadInverted(false);
  return true;
}

bool RagTime5Parser::readStructData(RagTime5Zone &zone, long endPos, int n, int headerSz,
                                    RagTime5StructManager::FieldParser &parser, librevenge::RVNGString const &dataName)
{
  MWAWInputStreamPtr input=zone.getInput();
  long pos=input->tell();
  if ((headerSz && pos+headerSz>endPos) || (headerSz==0 && pos+5>endPos)) return false;
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  std::string const zoneName=parser.getZoneName(n);
  int m=0;
  if (headerSz) {
    f << zoneName << "[A]:";
    if (!dataName.empty()) f << dataName.cstr() << ",";
    int val;
    if (headerSz==14) {
      val=(int) input->readLong(4);
      if (val!=1) f << "numUsed=" << val << ",";
      f << "f1=" << std::hex << input->readULong(2) << std::dec << ",";
      val=(int) input->readLong(2); // sometimes form an increasing sequence but not always
      if (val!=n) f << "id=" << val << ",";

      RagTime5StructManager::Field field;
      field.m_fileType=(long) input->readULong(4);
      field.m_type=RagTime5StructManager::Field::T_Long;
      field.m_longValue[0]=input->readLong(2);
      parser.parseHeaderField(field, zone, n, f);
    }
    else if (headerSz==8) {
      val=(int) input->readLong(2);
      if (val!=1) f << "numUsed=" << val << ",";
      val=(int) input->readLong(2); // sometimes form an increasing sequence but not always
      if (val!=n) f << "id=" << val << ",";
      f << "type=" << std::hex << input->readULong(4) << std::dec << ","; // 0 or 01458042
    }
    else if (headerSz==18) { // docinfo header
      val=(int) input->readLong(4); // 1 or 3
      if (val!=1) f << "numUsed?=" << val << ",";
      val=(int) input->readLong(4); // always 0
      if (val) f << "f0=" << val << ",";
      f << "ID=" << std::hex << input->readULong(4) << ","; // a big number
      val=(int) input->readLong(4);
      if (val!=0x1f6817) // doc info type
        f << "type=" << std::hex << val << std::dec << ",";
      val=(int) input->readLong(2); // always 0
      if (val) f << "f1=" << val << ",";
      input->seek(pos+headerSz, librevenge::RVNG_SEEK_SET);
    }
    else {
      MWAW_DEBUG_MSG(("RagTime5Parser::readStructData: find unknown header size\n"));
      f << "###hSz";
      input->seek(pos+headerSz, librevenge::RVNG_SEEK_SET);
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  pos=input->tell();
  if (parser.m_regroupFields) {
    f.str("");
    f << zoneName << "[B]:";
    if (headerSz==0 && !dataName.empty()) f << dataName.cstr() << ",";
  }
  while (!input->isEnd()) {
    long actPos=input->tell();
    if (actPos>=endPos) break;

    if (!parser.m_regroupFields) {
      f.str("");
      f << zoneName << "[B" << ++m << "]:";
      if (m==1 && headerSz==0 && !dataName.empty()) f << dataName.cstr() << ",";
    }
    RagTime5StructManager::Field field;
    if (!m_structManager->readField(input, endPos, ascFile, field, headerSz ? 0 : endPos-actPos)) {
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

bool RagTime5Parser::sendZones()
{
  MWAWListenerPtr listener=getMainListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("RagTime5Parser::sendZones: can not find the listener\n"));
    return false;
  }
  MWAW_DEBUG_MSG(("RagTime5Parser::sendZones: not implemented\n"));
  return true;
}

void RagTime5Parser::flushExtra()
{
  MWAWListenerPtr listener=getMainListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("RagTime5Parser::flushExtra: can not find the listener\n"));
    return;
  }
  m_graphParser->flushExtra();
  m_textParser->flushExtra();
  m_spreadsheetParser->flushExtra();

  // look for unparsed data
  for (size_t i=0; i<m_state->m_zonesList.size(); ++i) {
    if (!m_state->m_zonesList[i])
      continue;
    RagTime5Zone &zone=*m_state->m_zonesList[i];
    if (zone.m_isParsed || !zone.m_entry.valid())
      continue;
    readZoneData(zone);
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
