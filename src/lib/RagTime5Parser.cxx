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

#include "RagTime5Parser.hxx"

/** Internal: the structures of a RagTime5Parser */
namespace RagTime5ParserInternal
{
////////////////////////////////////////
//! Internal: the helper to read field for a RagTime5Parser
struct FieldParser : public RagTime5StructManager::FieldParser {
  //! constructor
  FieldParser(RagTime5Parser &, std::string zoneName="StructZone") : RagTime5StructManager::FieldParser(), m_name(zoneName)
  {
  }
  //! return the debug name corresponding to the zone
  std::string getZoneName() const
  {
    return m_name;
  }
  //! return the debug name corresponding to a field
  std::string getZoneName(int n) const
  {
    std::stringstream s;
    s << m_name << "-" << n;
    return s.str();
  }

protected:
  //! the zone name
  std::string m_name;
};

////////////////////////////////////////
//! Internal: the state of a RagTime5Parser
struct State {
  //! constructor
  State() : m_zonesEntry(), m_zonesList(), m_idToTypeMap(), m_dataIdZoneMap(), m_mainIdZoneMap(), m_clusterIdList(),
    m_pageZonesIdMap(), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }
  //! init the pattern to default
  void initDefaultPatterns(int vers);

  //! the main zone entry
  MWAWEntry m_zonesEntry;
  //! the zone list
  std::vector<shared_ptr<RagTime5StructManager::Zone> > m_zonesList;
  //! a map id to type string
  std::map<int, std::string> m_idToTypeMap;
  //! a map: data id->entry (datafork)
  std::map<int, shared_ptr<RagTime5StructManager::Zone> > m_dataIdZoneMap;
  //! a map: main id->entry (datafork)
  std::multimap<int, shared_ptr<RagTime5StructManager::Zone> > m_mainIdZoneMap;
  //! list of cluster id
  std::vector<int> m_clusterIdList;
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
  MWAWTextParser(input, rsrcParser, header), m_state(), m_graphParser(), m_textParser(), m_structManager()
{
  init();
}

RagTime5Parser::~RagTime5Parser()
{
}

void RagTime5Parser::init()
{
  m_structManager.reset(new RagTime5StructManager);
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

shared_ptr<RagTime5StructManager::Zone> RagTime5Parser::getDataZone(int dataId) const
{
  if (m_state->m_dataIdZoneMap.find(dataId)==m_state->m_dataIdZoneMap.end())
    return shared_ptr<RagTime5StructManager::Zone>();
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
    RagTime5StructManager::Zone &zone=*m_state->m_zonesList[i];
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
    RagTime5StructManager::Zone &zone=*m_state->m_zonesList[i];
    if (zone.m_isParsed)
      continue;
    if (zone.m_fileType==RagTime5StructManager::Zone::F_Main)
      m_state->m_mainIdZoneMap.insert
      (std::multimap<int, shared_ptr<RagTime5StructManager::Zone> >::value_type
       (zone.m_ids[0],m_state->m_zonesList[i]));
    else if (zone.m_fileType==RagTime5StructManager::Zone::F_Data) {
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

  std::multimap<int, shared_ptr<RagTime5StructManager::Zone> >::iterator it;
  // list of blocks zone
  it=m_state->m_mainIdZoneMap.lower_bound(10);
  int numZone10=0;
  while (it!=m_state->m_mainIdZoneMap.end() && it->first==10) {
    shared_ptr<RagTime5StructManager::Zone> zone=it++->second;
    if (!zone || zone->m_variableD[0]!=1 ||
        m_state->m_dataIdZoneMap.find(zone->m_variableD[1])==m_state->m_dataIdZoneMap.end()) {
      MWAW_DEBUG_MSG(("RagTime5Parser::createZones: the main zone 10 seems bads\n"));
      continue;
    }
    shared_ptr<RagTime5StructManager::Zone> dZone=
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
    RagTime5StructManager::Zone &zone=*m_state->m_zonesList[i];
    if (zone.m_isParsed || zone.getKindLastPart(zone.m_kinds[1].empty())!="ScreenRepList")
      continue;
    m_graphParser->readPictureList(zone);
  }

  for (size_t i=0; i<m_state->m_zonesList.size(); ++i) {
    if (!m_state->m_zonesList[i])
      continue;
    RagTime5StructManager::Zone &zone=*m_state->m_zonesList[i];
    if (zone.m_isParsed || !zone.m_entry.valid())
      continue;
    readZoneData(zone);
  }
  return false;
}

bool RagTime5Parser::readZoneData(RagTime5StructManager::Zone &zone)
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
bool RagTime5Parser::readString(RagTime5StructManager::Zone &zone, std::string &text)
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

bool RagTime5Parser::readUnicodeString(RagTime5StructManager::Zone &zone)
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

bool RagTime5Parser::readUnicodeStringList(RagTime5StructManager::Link const &link, std::map<int, librevenge::RVNGString> &idToStringMap)
{
  if (link.m_ids.size()<2 || !link.m_ids[1])
    return false;

  std::vector<long> decal;
  if (link.m_ids[0])
    readPositions(link.m_ids[0], decal);
  if (decal.empty())
    decal=link.m_longList;

  int const dataId=link.m_ids[1];
  shared_ptr<RagTime5StructManager::Zone> dataZone=getDataZone(dataId);
  if (!dataZone || !dataZone->m_entry.valid() ||
      dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData") {
    if (decal.size()==1) {
      // a zone with 0 zone is ok...
      dataZone->m_isParsed=true;
      return true;
    }
    MWAW_DEBUG_MSG(("RagTime5Parser::readUnicodeStringList: the data zone %d seems bad\n", dataId));
    return false;
  }

  long length=dataZone->m_entry.length();
  if (!length) {
    if (decal.empty()) return true;
    MWAW_DEBUG_MSG(("RagTime5Parser::readUnicodeStringList: can not find the strings zone %d\n", dataZone->m_ids[0]));
    return false;
  }

  MWAWInputStreamPtr input=dataZone->getInput();
  input->setReadInverted(!dataZone->m_hiLoEndian);
  long begPos=dataZone->m_entry.begin(), endPos=dataZone->m_entry.end();
  dataZone->m_isParsed=true;

  libmwaw::DebugFile &ascFile=dataZone->ascii();
  libmwaw::DebugStream f;
  f << "Entries(UnicodeList)[" << std::hex << link.m_fileType[0] << std::dec << "][" << *dataZone << "]:";
  input->seek(begPos, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(endPos);
  ascFile.addNote("_");
  if (decal.size()<=1) {
    f << "###";
    ascFile.addPos(begPos);
    ascFile.addNote(f.str().c_str());
    input->setReadInverted(false);
    return false;
  }
  ascFile.addPos(begPos);
  ascFile.addNote(f.str().c_str());
  for (size_t i=0; i+1<decal.size(); ++i) {
    if (decal[i]==decal[i+1]) continue;
    long pos=begPos+decal[i], endDataPos=begPos+decal[i+1];
    f.str("");
    f << "UnicodeList-" << i << ":";
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    if (endDataPos>endPos) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readUnicodeStringList: the strings %d seems bad\n", int(i)));
      if (pos<endPos) {
        f << "###";
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
      }
      continue;
    }
    librevenge::RVNGString string;
    if (!m_structManager->readUnicodeString(input, endDataPos, string))
      f << "###";
    else {
      idToStringMap[int(i)+1]=string;
      f << "\"" << string.cstr() << "\",";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->setReadInverted(false);
  return true;
}

bool RagTime5Parser::readDataIdList(MWAWInputStreamPtr input, int n, std::vector<int> &listIds) const
{
  listIds.clear();
  long pos=input->tell();
  for (int i=0; i<n; ++i) {
    int val=(int) MWAWInputStream::readULong(input->input().get(), 2, 0, false);
    if (val==0) {
      listIds.push_back(0);
      input->seek(2, librevenge::RVNG_SEEK_CUR);
      continue;
    }
    if (val!=1) {
      // update the position
      input->seek(pos+4*n, librevenge::RVNG_SEEK_SET);
      return false;
    }
    listIds.push_back((int) MWAWInputStream::readULong(input->input().get(), 2, 0, false));
  }
  return true;
}

bool RagTime5Parser::readPositions(int posId, std::vector<long> &listPosition)
{
  if (!posId)
    return false;

  shared_ptr<RagTime5StructManager::Zone> zone=getDataZone(posId);
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
  std::multimap<int, shared_ptr<RagTime5StructManager::Zone> >::iterator it=
    m_state->m_mainIdZoneMap.lower_bound(11);
  int numZones=0;
  while (it!=m_state->m_mainIdZoneMap.end() && it->first==11) {
    shared_ptr<RagTime5StructManager::Zone> zone=it++->second;
    if (!zone || zone->m_variableD[0]!=1 ||
        m_state->m_dataIdZoneMap.find(zone->m_variableD[1])==m_state->m_dataIdZoneMap.end()) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZones: the main cluster zone seems bads\n"));
      continue;
    }
    // the main cluster
    shared_ptr<RagTime5StructManager::Zone> dZone=
      m_state->m_dataIdZoneMap.find(zone->m_variableD[1])->second;
    if (!dZone) continue;
    dZone->m_extra+="main11,";
    if (dZone->getKindLastPart(dZone->m_kinds[1].empty())!="Cluster" || !readClusterZone(*dZone)) {
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
  for (size_t i=0; i<m_state->m_clusterIdList.size(); ++i) {
    int id=m_state->m_clusterIdList[i];
    if (m_state->m_dataIdZoneMap.find(id)!=m_state->m_dataIdZoneMap.end()) {
      shared_ptr<RagTime5StructManager::Zone> dZone=m_state->m_dataIdZoneMap.find(id)->second;
      if (dZone && !dZone->m_isParsed && dZone->getKindLastPart(dZone->m_kinds[1].empty())=="Cluster"
          && readClusterZone(*dZone)) continue;
    }
    MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZones: can not find cluster zone %d\n", id));
  }

  for (size_t i=0; i<m_state->m_zonesList.size(); ++i) {
    if (!m_state->m_zonesList[i])
      continue;
    RagTime5StructManager::Zone &zone=*m_state->m_zonesList[i];
    if (zone.m_isParsed || !zone.m_entry.valid() || zone.getKindLastPart(zone.m_kinds[1].empty())!="Cluster")
      continue;
    MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZones: find unparsed cluster zone %d\n", zone.m_ids[0]));
    readClusterZone(zone);
  }

  return true;
}

bool RagTime5Parser::readClusterList(RagTime5StructManager::Zone &zone)
{
  MWAWEntry &entry=zone.m_entry;
  if (entry.length()<24 || (entry.length()%8)) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readClusterList: the item list seems bad\n"));
    return false;
  }
  zone.m_isParsed=true;
  MWAWInputStreamPtr input=zone.getInput();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);

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
    int val=(int) input->readLong(2);
    int id=(int) input->readLong(2);
    if (val==0 && id==0) {
      input->seek(4, librevenge::RVNG_SEEK_CUR);
      ascFile.addPos(pos);
      ascFile.addNote("_");
      continue;
    }
    if (val!=1) f << "#f0=" << val << ",";
    f << "id=" << id << ",";
    m_state->m_clusterIdList.push_back(id);
    val=(int) input->readULong(2); // [02468][01234be][02468][01234abe]
    if (val) f << "fl=" << std::hex << val << std::dec << ",";
    val=(int) input->readLong(2); // always 0?
    if (val) f << "#f1=" << val << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}


bool RagTime5Parser::readClusterZone(RagTime5StructManager::Zone &zone)
{
  MWAWEntry &entry=zone.m_entry;
  if (entry.length()==0) return true;
  if (entry.length()<13) return false;

  MWAWInputStreamPtr input=zone.getInput();
  long endPos=entry.end();
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(Cluster)[" << zone << "]:";
  int val;
  for (int i=0; i<4; ++i) { // f0=f1=0, f2=1, f3=small number
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  zone.m_isParsed=true;
  libmwaw::DebugFile &ascFile=zone.ascii();
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  RagTime5StructManager::Cluster cluster;
  cluster.m_hiLoEndian=zone.m_hiLoEndian;
  int n=0;
  while (!input->isEnd()) {
    long pos=input->tell();
    if (pos>=endPos) break;
    long lVal;
    if (!RagTime5StructManager::readCompressedLong(input, endPos, lVal)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f.str("");
    f << "Cluster-" << n << ":";
    f << "f0=" << lVal << ",";
    // always in big endian
    long sz;
    if (!RagTime5StructManager::readCompressedLong(input,endPos,sz) || sz <= 7 || input->tell()+sz>endPos) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f << "sz=" << sz << ",";
    long endDataPos=input->tell()+sz;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    pos=input->tell();
    f.str("");
    f << "Cluster-" << n << "-A:";
    if (!zone.m_hiLoEndian) f << "lohi,";
    long fSz;
    if (!RagTime5StructManager::readCompressedLong(input, endDataPos, fSz) || fSz<6 || input->tell()+fSz>endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: can not read item A\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      ++n;
      input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
      continue;
    }
    long debSubDataPos=input->tell();
    long endSubDataPos=debSubDataPos+fSz;
    int fl=(int) input->readULong(2); // [01][13][0139b]
    f << "fl=" << std::hex << fl << std::dec << ",";
    int N=(int) input->readLong(4);
    std::stringstream s;
    s << "type" << std::hex << N << std::dec;

    enum What { W_Unknown, W_FileName, W_ListLink, W_FixedListLink,
                W_GraphTypes, W_GraphZones,
                W_MainStructZones, W_Formats, W_ListDef,
                W_TextUnknown, W_TextZones
              };
    What what=W_Unknown;
    std::string name=s.str();
    RagTime5StructManager::Link link;
    bool isMainLink=false, isNamedLink=false;

    if ((zone.m_hiLoEndian && N==int(0x80000000)) || (!zone.m_hiLoEndian && N==0x8000)) {
      name="filename";
      what=W_FileName;
    }
    switch (N) {
    case -5:
      name="header";
      if (fSz<12) {
        f << "###data,";
        MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: can not read the data id\n"));
        break;
      }
      for (int i=0; i<2; ++i) { // always 0?
        val=(int) input->readLong(2);
        if (val) f << "f" << i+1 << "=" << val << ",";
      }
      val=(int) input->readLong(2);
      f << "id=" << val << ",";
      if (fSz==58 || fSz==64 || fSz==66 || fSz==68) {
        if (input->readULong(2)!=0x480)
          break;
        link.m_N=val;
        for (int i=0; i<13; ++i) { // g3=2, g4=10, g6 and g8 2 small int
          val=(int) input->readLong(2);
          if (val) f << "g" << i << "=" << val << ",";
        }
        link.m_fileType[0]=(int) input->readULong(4);
        if (link.m_fileType[0] != 0x01473857 && (link.m_fileType[0] != 0x0146e827 || fSz!=58)) {
          link=RagTime5StructManager::Link();
          break;
        }
        link.m_fileType[1]=(int) input->readULong(2); // c018|c030|c038 or type ?
        if (!readDataIdList(input, 2, link.m_ids) || link.m_ids[1]==0) {
          link=RagTime5StructManager::Link();
          break;
        }

        link.m_type=RagTime5StructManager::Link::L_FieldsList;
        what=W_MainStructZones;
        isMainLink=true;
        if (cluster.m_type!=RagTime5StructManager::Cluster::C_Unknown) {
          MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: oops the cluster type is already defined\n"));
          f << "###clusterType";
        }
        if (fSz==58) {
          if (link.m_fileType[0] == 0x0146e827) {
            what=W_Formats;
            link.m_name=name="formats";
            cluster.m_type=RagTime5StructManager::Cluster::C_Formats;
          }
          else {
            link.m_name=name="units";
            cluster.m_type=RagTime5StructManager::Cluster::C_Units;
          }
        }
        else if (fSz==64) {
          link.m_name=name="graphColor";
          cluster.m_type=RagTime5StructManager::Cluster::C_GraphicColors;
        }
        else if (fSz==66) {
          link.m_name=name="textStyle";
          cluster.m_type=RagTime5StructManager::Cluster::C_TextStyles;
        }
        else {
          link.m_name=name="graphStyle";
          cluster.m_type=RagTime5StructManager::Cluster::C_GraphicStyles;
        }
        ascFile.addDelimiter(input->tell(), '|');
        break;
      }
      if (fSz==76 || fSz==110) {
        val=(int) input->readULong(2);
        if ((val&0xBFFF)!=0x204) f << "f3=" << std::hex << val << std::dec << ",";
        else if (val&0x4000) f << "f3[4000],";
        val=(int) input->readLong(2); // always 0?
        if (val) f << "f4=" << val << ",";
        for (int i=0; i<7; ++i) { // g1, g2, g3 small int other 0
          val=(int) input->readLong(4);
          if (i==2)
            link.m_N=val;
          else if (val) f << "g" << i << "=" << val << ",";
        }
        link.m_fileType[1]=(long) input->readULong(2);
        link.m_fieldSize=(int) input->readULong(2);
        std::vector<int> listIds;
        if (link.m_fieldSize!=56 || !readDataIdList(input, 2, listIds) || listIds[0]==0) {
          link=RagTime5StructManager::Link();
          break;
        }
        what=W_FixedListLink;
        link.m_ids.push_back(listIds[0]);
        link.m_type=RagTime5StructManager::Link::L_UnknownZoneB;
        name="unknZoneB";
        f << link << ",";
        if (listIds[1]) {
          cluster.m_clusterIds.push_back(listIds[1]);
          f << "clusterId1=data" << listIds[1] << "A,";
        }
        break;
      }
      if (fSz==118) {
        what=W_GraphZones;
        name="graphZone";
        input->seek(debSubDataPos+42, librevenge::RVNG_SEEK_SET);
        bool hasNoPos;
        link.m_type=RagTime5StructManager::Link::L_Graphic;
        link.m_fileType[0]=(long) input->readULong(4);
        link.m_fileType[1]=(long) input->readULong(2);
        link.m_fieldSize=(int) input->readULong(2);
        bool ok=link.m_fileType[1]==0 && (link.m_fieldSize==0x8000 || link.m_fieldSize==0x8020);
        hasNoPos=(link.m_fieldSize & 0x20);
        ok=ok && readDataIdList(input, 2, link.m_ids) && link.m_ids[1]!=0 &&
           (link.m_ids[0] || hasNoPos);
        input->seek(debSubDataPos+106, librevenge::RVNG_SEEK_SET);
        std::vector<int> listIds;
        if (ok && readDataIdList(input, 3, listIds)) {
          link.m_ids.push_back(listIds[0]);
          for (size_t i=1; i<3; ++i) {
            if (!listIds[i]) continue;
            cluster.m_clusterIds.push_back(listIds[i]);
            f << "clusterId" << i << "=data" << listIds[i] << "A,";
          }
        }
        else if (ok)
          link.m_ids.push_back(0);

        if (ok) {
          cluster.m_type=RagTime5StructManager::Cluster::C_GraphicData;
          isMainLink=true;
          f << "graph=[" << link << "],";
          ascFile.addDelimiter(debSubDataPos+42, '|');
          ascFile.addDelimiter(debSubDataPos+58, '|');
          ascFile.addDelimiter(debSubDataPos+106, '|');
          if (hasNoPos)
            link.m_ids[0]=0;
        }
        else
          link=RagTime5StructManager::Link();
        input->seek(debSubDataPos+6, librevenge::RVNG_SEEK_SET);
        break;
      }
      break;
    case -2: {
      name="head2";
      if (fSz<215) {
        f << "###sz,";
        MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: can not read the head2 size seems bad\n"));
        break;
      }
      val=(int) input->readLong(4); // 8|9|a
      f << "f1=" << val << ",";
      for (int i=0; i<4; ++i) { // f2=0-7, f3=1|3
        val=(int) input->readLong(2);
        if (val) f << "f" << i+2 << "=" << val << ",";
      }
      val=(int) input->readLong(4); // 7|8
      std::vector<int> listIds;
      f << "f6=" << val << ",";
      if (!readDataIdList(input, 1, listIds) || !listIds[0]) {
        f << "###cluster[child],";
        MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone[head2]: can not find the cluster's child\n"));
      }
      else {
        cluster.m_childId=listIds[0];
        f << "cluster[child]=data" << listIds[0] << "A,";
      }
      for (int i=0; i<21; ++i) { // always g0=g11=g18=16, other 0 ?
        val=(int) input->readLong(2);
        if (val) f << "g" << i << "=" << val << ",";
      }
      break;
    }
    default: {
      if (what!=W_Unknown) break;

      input->seek(debSubDataPos+18, librevenge::RVNG_SEEK_SET);
      long type=(long) input->readULong(4);
      if (type==0x15f3817||type==0x15e4817) {
        link.m_fileType[0]=type;
        link.m_fileType[1]=(long) input->readULong(2);
        link.m_fieldSize=(int) input->readULong(2);
        if (readDataIdList(input, 1, link.m_ids) && link.m_ids[0]) {
          link.m_N=N;
          shared_ptr<RagTime5StructManager::Zone> data=getDataZone(link.m_ids[0]);
          if (data) {
            if (link.m_fileType[0]==0x15e4817) {
              name="textUnknown";
              what=W_TextUnknown;
              link.m_type=RagTime5StructManager::Link::L_TextUnknown;
            }
            else {
              name="listDef";
              what=W_ListDef;
              link.m_type=RagTime5StructManager::Link::L_ListDef;
              if (fSz>=71) { // the definitions
                ascFile.addDelimiter(input->tell(),'|');
                input->seek(debSubDataPos+63, librevenge::RVNG_SEEK_SET);
                ascFile.addDelimiter(input->tell(),'|');
                std::vector<int> dataId;
                if (readDataIdList(input, 2, dataId) && dataId[1]) {
                  link.m_ids.push_back(dataId[0]);
                  link.m_ids.push_back(dataId[1]);
                }
              }
            }
            break;
          }
        }
      }

      input->seek(debSubDataPos+6, librevenge::RVNG_SEEK_SET);
      if (fSz==28) {
        link.m_fileType[0]=(int) input->readULong(4);
        if (link.m_fileType[0]!=0x100004) {
          f << "fType=" << std::hex << link.m_fileType[0] << std::dec << ",";
          break;
        }
        link.m_type=RagTime5StructManager::Link::L_Text;
        name="textZone";
        what=W_TextZones;
        cluster.m_type=RagTime5StructManager::Cluster::C_TextData;
        isMainLink=true;
        f << "f0=" << N << ",";
        val=(int) input->readLong(2); // always 0?
        if (val) f << "f1=" << val << ",";
        val=(int) input->readLong(2); // always f?
        if (val!=15) f << "f2=" << val << ",";
        std::vector<int> listIds;
        if (readDataIdList(input, 1, listIds))
          link.m_ids.push_back((int) listIds[0]);
        else {
          f << "#link0,";
          link.m_ids.push_back(0);
        }
        link.m_N=(int) input->readULong(4);
        val=(int) input->readLong(1); // always 0?
        if (val) f << "f3=" << val << ",";
        listIds.clear();
        if (readDataIdList(input, 1, listIds) && listIds[0])
          link.m_ids.push_back(listIds[0]);
        else {
          f << "#link1,";
          link.m_ids.push_back(0);
        }
        val=(int) input->readLong(1); // always 1?
        if (val) f << "f4=" << val << ",";
        f << link;
        break;
      }
      if (fSz==30) {
        link.m_fileType[0]=(int) input->readULong(4);
        input->seek(debSubDataPos+22, librevenge::RVNG_SEEK_SET);
        link.m_fileType[1]=(int) input->readULong(2);
        link.m_fieldSize=(int) input->readULong(2);
        if (readDataIdList(input, 1, link.m_ids) && link.m_ids[0]) {
          link.m_N=N;
          shared_ptr<RagTime5StructManager::Zone> data=getDataZone(link.m_ids[0]);

          if (data) {
            if (!link.m_fieldSize || data->m_entry.length()!=N*link.m_fieldSize) {
              link=RagTime5StructManager::Link();
            }
            else if (link.m_ids[0]==14) { // fixme
              name="clusterList";
              link=RagTime5StructManager::Link();
              break;
            }
            else {
              what=W_FixedListLink;
              if (link.m_fileType[0]==0x9f840) {
                if (link.m_fileType[1]!=0x10) // 10 or 18
                  f << "f1=" << link.m_fileType[1] << ",";
                link.m_fileType[1]=0;
                link.m_type=RagTime5StructManager::Link::L_GraphicTransform;
                name="graphTransform";
              }
              else {
                link.m_fileType[0]=0;
                name="listLn2";
              }
              ascFile.addDelimiter(debSubDataPos+22,'|');
            }
          }
          else
            link=RagTime5StructManager::Link();
        }
        else
          link=RagTime5StructManager::Link();
        input->seek(debSubDataPos+6, librevenge::RVNG_SEEK_SET);
        break;
      }
      if (fSz==32) {
        what=W_ListLink;
        long unknType=(long) input->readULong(4);
        if (unknType) f << "unknType=" << std::hex << unknType << std::dec << ",";
        link.m_fileType[0]=(long) input->readULong(4);
        input->seek(debSubDataPos+20, librevenge::RVNG_SEEK_SET);
        link.m_type=RagTime5StructManager::Link::L_List;
        val=(int) input->readLong(2);
        if (val) f << "f3=" << val << ",";
        link.m_fileType[1]=(long) input->readULong(2); // 0x220
        if (!readDataIdList(input, 2, link.m_ids) || !link.m_ids[1]) {
          f << "###";
          link=RagTime5StructManager::Link();
          break;
        }
        if (link.m_fileType[1]==0x220 || link.m_fileType[1]==0x200 || link.m_fileType[1]==0x620) {
          name="unicodeList";
          if (link.m_fileType[0]==0x7d01a) name+="[layout]";
          isNamedLink=true;
          link.m_type=RagTime5StructManager::Link::L_UnicodeList;
        }
        else if (unknType==0x47040) {
          if (link.m_fileType[1]==0x20)
            f << "hasNoPos,";
          else if (link.m_fileType[1]) {
            MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone[settins]: find unexpected flags\n"));
            f << "###flags=" << std::hex << link.m_fileType[1] << std::dec << ",";
          }
          name="settings";
          link.m_type=RagTime5StructManager::Link::L_SettingsList;
        }
        // todo 0x80045080 is a list of 2 int
        // fileType[1]==4030: layout list
        else if (link.m_fileType[1]==0x30) {
          name="condFormula";
          link.m_type=RagTime5StructManager::Link::L_ConditionFormula;
        }
        else
          name="listLink";
        /* checkme:
           link.m_fileType[0]==20|0 is a list of fields
           link.m_fileType[0]==1010|1038 is a list of sequence of 16 bytes
        */
        ascFile.addDelimiter(debSubDataPos+20,'|');
        break;
      }
      if (fSz==52 && N==1) {
        name="graphTypes";
        what=W_GraphTypes;
        for (int i=0; i<2; ++i) { // always 0 and small val
          val=(int) input->readLong(2);
          if (val) f << "f" << i+1 << "=" << val << ",";
        }
        if (input->readULong(4)!=0x14e6042)
          break;
        for (int i=0; i<16; ++i) { // g1=0-2, g2=10[size?], g4=1-8[N], g13=30
          val=(int) input->readLong(2);
          if (val) f << "g" << i << "=" << val << ",";
        }
        if (readDataIdList(input, 1, link.m_ids) && link.m_ids[0]) {
          link.m_type=RagTime5StructManager::Link::L_GraphicType;
          link.m_fileType[0]=0x30;
          link.m_fileType[1]=0;
          link.m_fieldSize=16;
        }
        else
          link.m_ids.push_back(0);
        val=(int) input->readLong(2);
        if (val) // small number
          f << "h0=" << val << ",";
        break;
      }
      break;
    }
    }
    if (link.empty())
      f << name << "-" << lVal << ",";
    else
      f << link << ",";
    if (fSz>32 && N>0 && what==W_Unknown) {
      long actPos=input->tell();
      input->seek(debSubDataPos+18, librevenge::RVNG_SEEK_SET);
      link.m_fileType[0]=(long) input->readULong(4);
      link.m_fileType[1]=(long) input->readULong(2);
      link.m_fieldSize=(int) input->readULong(2);
      if (readDataIdList(input, 1, link.m_ids) && link.m_ids[0]) {
        link.m_N=N;
        shared_ptr<RagTime5StructManager::Zone> data=getDataZone(link.m_ids[0]);
        if (data && data->m_entry.length()==N*link.m_fieldSize) {
          name+="-fixZone";
          what=W_FixedListLink;

          f << link << ",";
          ascFile.addDelimiter(debSubDataPos+18,'|');
          ascFile.addDelimiter(debSubDataPos+30,'|');
        }
        else
          link=RagTime5StructManager::Link();
      }
      else
        link=RagTime5StructManager::Link();
      input->seek(actPos, librevenge::RVNG_SEEK_SET);
    }
    if (input->tell()!=pos && input->tell()!=endSubDataPos)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(endSubDataPos, librevenge::RVNG_SEEK_SET);

    int m=0;

    while (!input->isEnd()) {
      pos=input->tell();
      if (pos+4>endDataPos)
        break;
      RagTime5StructManager::Field field;
      if (!m_structManager->readField(zone, endDataPos, field)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
      f.str("");
      f << "Cluster-" << n << "-B" << m++ << "[" << name << "]:";
      if (!zone.m_hiLoEndian) f << "lohi,";
      bool done=false;
      switch (what) {
      case W_FileName:
        if (field.m_type==RagTime5StructManager::Field::T_Unicode && field.m_fileType==0xc8042) {
          f << field.m_string.cstr();
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: find unexpected filename field\n"));
        f << "###";
        break;
      case W_ListLink: {
        if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xce842) {
          f << "pos=[";
          for (size_t i=0; i<field.m_longList.size(); ++i)
            f << field.m_longList[i] << ",";
          f << "],";
          link.m_longList=field.m_longList;
          done=true;
          break;
        }
        if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
          // a small value 2|4|a|1c|40
          f << "unkn="<<field.m_extra << ",";
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: find unexpected list link field\n"));
        f << "###";
        break;
      }
      case W_Formats: {
        if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x146e815) {
          f << "decal=[";
          for (size_t i=0; i<field.m_fieldList.size(); ++i) {
            RagTime5StructManager::Field const &child=field.m_fieldList[i];
            if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==0xce842) {
              for (size_t j=0; j<child.m_longList.size(); ++j)
                f << child.m_longList[j] << ",";
              link.m_longList=child.m_longList;
              continue;
            }
            if (child.m_type==RagTime5StructManager::Field::T_Unstructured && child.m_fileType==0xce017) {
              // a list of small int 0104|0110|22f8ffff7f3f
              f << "unkn0=" << child.m_extra << ",";
              continue;
            }
            f << "#[" << child << "],";
          }
          f << "],";
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: find unexpected child[format]\n"));
        break;
      }
      case W_ListDef: {
        if (field.m_type==RagTime5StructManager::Field::T_FieldList &&
            (field.m_fileType==0x15f4815 /* v5?*/ || field.m_fileType==0x160f815 /* v6? */)) {
          f << "decal=[";
          for (size_t i=0; i<field.m_fieldList.size(); ++i) {
            RagTime5StructManager::Field const &child=field.m_fieldList[i];
            if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==0xce842) {
              for (size_t j=0; j<child.m_longList.size(); ++j)
                f << child.m_longList[j] << ",";
              link.m_longList=child.m_longList;
              continue;
            }
            MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: find unexpected decal child[list]\n"));
            f << "#[" << child << "],";
          }
          f << "],";
          done=true;
          break;
        }
        else if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
          f << "unkn0=" << field.m_extra; // always 2: next value ?
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: find unexpected child[list]\n"));
        break;
      }

      case W_MainStructZones: {
        if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x1473815) {
          f << "decal=[";
          for (size_t i=0; i<field.m_fieldList.size(); ++i) {
            RagTime5StructManager::Field const &child=field.m_fieldList[i];
            if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==0xce842) {
              for (size_t j=0; j<child.m_longList.size(); ++j)
                f << child.m_longList[j] << ",";
              link.m_longList=child.m_longList;
              continue;
            }
            else if (child.m_type==RagTime5StructManager::Field::T_Unstructured && child.m_fileType==0xce017) {
              /* find 00000003,00000007,00000008,0000002000,000002,000042,000400,000400000000000000000000,0008
                 000810,004000,00400000,800a00 : a list of bool, one for each style ?
                 checkme: maybe related to style with name
              */
              f << "unkn=[" << child.m_extra << "],";
              continue;
            }
            MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: find unexpected decal child[%s]\n", link.m_name.c_str()));
            f << "#[" << child << "],";
          }
          f << "],";
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: find unexpected child[%s]\n", link.m_name.c_str()));
        break;
      }
      case W_GraphTypes: {
        if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x14eb015) {
          f << "decal=[";
          for (size_t i=0; i<field.m_fieldList.size(); ++i) {
            RagTime5StructManager::Field const &child=field.m_fieldList[i];
            if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==0xce842) {
              for (size_t j=0; j<child.m_longList.size(); ++j)
                f << child.m_longList[j] << ",";
              link.m_longList=child.m_longList;
              continue;
            }
            MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: find unexpected decal child[graphType]\n"));
            f << "#[" << child << "],";
          }
          f << "],";
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: find unexpected child[graphType]\n"));
        break;
      }
      case W_GraphZones: {
        if (field.m_type==RagTime5StructManager::Field::T_Long && field.m_fileType==0x3c057) {
          // a small value 3|4
          f << "f0="<<field.m_longValue[0] << ",";
          done=true;
          break;
        }
        if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x14e6825) {
          f << "decal=[";
          for (size_t i=0; i<field.m_fieldList.size(); ++i) {
            RagTime5StructManager::Field const &child=field.m_fieldList[i];
            if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==0xce842) {
              for (size_t j=0; j<child.m_longList.size(); ++j)
                f << child.m_longList[j] << ",";
              link.m_longList=child.m_longList;
              continue;
            }
            MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: find unexpected decal child[graph]\n"));
            f << "#[" << child << "],";
          }
          f << "],";
          done=true;
          break;
        }
        if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x14e6875) {
          f << "listFlag?=[";
          for (size_t i=0; i<field.m_fieldList.size(); ++i) {
            RagTime5StructManager::Field const &child=field.m_fieldList[i];
            if (child.m_type==RagTime5StructManager::Field::T_Unstructured && child.m_fileType==0xce017)
              f << child.m_extra << ","; // find data with different length there
            else {
              MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: find unexpected unstructured child[graphZones]\n"));
              f << "#" << child << ",";
            }
          }
          f << "],";
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: find unexpected child[graphZones]\n"));
        break;
      }
      case W_TextUnknown:
      case W_TextZones:
        MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: find unexpected text zones child\n"));
        f << "###";
        break;
      case W_FixedListLink:
      case W_Unknown:
      default:
        break;
      }
      if (done) {
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        continue;
      }
      f << field;
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    if (!link.empty()) {
      if (isMainLink) {
        if (cluster.m_dataLink.empty())
          cluster.m_dataLink=link;
        else {
          MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: oops the main link is already set\n"));
          cluster.m_linksList.push_back(link);
        }
      }
      else if (isNamedLink) {
        if (cluster.m_nameLink.empty())
          cluster.m_nameLink=link;
        else {
          MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: oops the name link is already set\n"));
          cluster.m_linksList.push_back(link);
        }
      }
      else
        cluster.m_linksList.push_back(link);
    }
    pos=input->tell();
    if (pos!=endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: find some extra data\n"));
      f.str("");
      f << "Cluster-" << n << ":" << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    ++n;
    input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
  }
  long pos=input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: find some extra data\n"));
    f.str("");
    f << "Cluster###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->setReadInverted(false);

  // check cluster
  if (cluster.m_childId) {
    bool ok=false;
    if (m_state->m_dataIdZoneMap.find(cluster.m_childId)!=m_state->m_dataIdZoneMap.end()) {
      shared_ptr<RagTime5StructManager::Zone> dZone=m_state->m_dataIdZoneMap.find(cluster.m_childId)->second;
      ok=dZone && !dZone->m_isParsed && dZone->getKindLastPart(dZone->m_kinds[1].empty())=="Cluster"
         && readClusterZone(*dZone);
    }
    if (!ok) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: the child cluster id %d seems bad\n", cluster.m_childId));
    }
  }

  for (size_t j=0; j<cluster.m_clusterIds.size(); ++j) {
    int cId=cluster.m_clusterIds[j];
    if (cId==0) continue;
    shared_ptr<RagTime5StructManager::Zone> data=getDataZone(cId);
    if (!data || !data->m_entry.valid() || data->getKindLastPart(data->m_kinds[1].empty())!="Cluster") {
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: the cluster zone %d seems bad\n", cId));
      continue;
    }
  }

  // main block
  if (cluster.m_type==RagTime5StructManager::Cluster::C_GraphicColors)
    m_graphParser->readGraphicColors(cluster);
  else if (cluster.m_type==RagTime5StructManager::Cluster::C_GraphicData)
    m_graphParser->readGraphicZone(cluster);
  else if (cluster.m_type==RagTime5StructManager::Cluster::C_GraphicStyles)
    m_graphParser->readGraphicStyles(cluster);
  else if (cluster.m_type==RagTime5StructManager::Cluster::C_TextData)
    m_textParser->readTextZone(cluster);
  else if (cluster.m_type==RagTime5StructManager::Cluster::C_TextStyles)
    m_textParser->readTextStyles(cluster);
  else if (cluster.m_type==RagTime5StructManager::Cluster::C_Units) {
    RagTime5ParserInternal::FieldParser defaultParser(*this, "Units");
    readStructZone(cluster, defaultParser);
  }
  else if (cluster.m_type==RagTime5StructManager::Cluster::C_Formats)
    readFormats(cluster);
  else if (!cluster.m_dataLink.empty()) {
    RagTime5ParserInternal::FieldParser defaultParser(*this);
    readStructZone(cluster, defaultParser);
  }

  if (!cluster.m_nameLink.empty()) {
    std::map<int, librevenge::RVNGString> idToStringMap;
    readUnicodeStringList(cluster.m_nameLink, idToStringMap);
  }

  for (size_t i=0; i<cluster.m_linksList.size(); ++i) {
    RagTime5StructManager::Link const &link=cluster.m_linksList[i];
    if (link.m_type==RagTime5StructManager::Link::L_GraphicType) {
      m_graphParser->readGraphicTypes(zone, link);
      continue;
    }
    else if (link.m_type==RagTime5StructManager::Link::L_ConditionFormula) {
      RagTime5StructManager::Cluster unknCluster;
      unknCluster.m_dataLink=link;
      RagTime5ParserInternal::FieldParser defaultParser(*this, "CondFormula");
      readStructZone(unknCluster, defaultParser, false);
      continue;
    }
    else if (link.m_type==RagTime5StructManager::Link::L_SettingsList) {
      RagTime5StructManager::Cluster unknCluster;
      unknCluster.m_dataLink=link;
      RagTime5ParserInternal::FieldParser defaultParser(*this, "Settings");
      readStructZone(unknCluster, defaultParser, false);
      continue;
    }
    else if (link.m_type==RagTime5StructManager::Link::L_UnknownZoneC) {
      RagTime5StructManager::Cluster unknCluster;
      unknCluster.m_dataLink=link;
      RagTime5ParserInternal::FieldParser defaultParser(*this, "UnknZoneC");
      readStructZone(unknCluster, defaultParser, false);
      continue;
    }
    else if (link.m_type==RagTime5StructManager::Link::L_List) {
      readListZone(zone, link);
      continue;
    }
    else if (link.m_type==RagTime5StructManager::Link::L_ListDef) {
      m_textParser->readListZones(cluster, link);
      continue;
    }

    if (link.empty()) continue;
    shared_ptr<RagTime5StructManager::Zone> data=getDataZone(link.m_ids[0]);
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
    case RagTime5StructManager::Link::L_ConditionFormula:
    case RagTime5StructManager::Link::L_FieldsList:
    case RagTime5StructManager::Link::L_Graphic:
    case RagTime5StructManager::Link::L_GraphicType:
    case RagTime5StructManager::Link::L_List:
    case RagTime5StructManager::Link::L_SettingsList:
    case RagTime5StructManager::Link::L_Text:
    case RagTime5StructManager::Link::L_UnicodeList:
    case RagTime5StructManager::Link::L_UnknownZoneC:
      break;
    case RagTime5StructManager::Link::L_GraphicTransform:
      m_graphParser->readGraphicTransformations(*data, link);
      break;
    case RagTime5StructManager::Link::L_TextUnknown:
      m_textParser->readTextUnknown(link.m_ids[0]);
      break;
    case RagTime5StructManager::Link::L_ListDef:
    case RagTime5StructManager::Link::L_Unknown:
    case RagTime5StructManager::Link::L_UnknownZoneB:
    default: {
      if (!data->m_entry.valid()) {
        if (link.m_N*link.m_fieldSize) {
          MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: can not find data zone %d\n", link.m_ids[0]));
        }
        break;
      }
      pos=data->m_entry.begin();
      data->m_isParsed=true;
      libmwaw::DebugFile &dAscFile=data->ascii();
      if (pos+link.m_N*link.m_fieldSize>data->m_entry.end()) {
        MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: bad fieldSize/N for zone %d\n", link.m_ids[0]));
        f.str("");
        f << "Entries(" << link.getZoneName() << ")[" << *data << "]:" << "###";
        dAscFile.addPos(pos);
        dAscFile.addNote(f.str().c_str());
        break;
      }
      for (int j=0; j<link.m_N; ++j) {
        f.str("");
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

////////////////////////////////////////////////////////////
// unknown
////////////////////////////////////////////////////////////
bool RagTime5Parser::readUnknZoneA(RagTime5StructManager::Zone &zone, RagTime5StructManager::Link const &link)
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
bool RagTime5Parser::readFormats(RagTime5StructManager::Cluster &cluster)
{
  RagTime5StructManager::Link const &link=cluster.m_dataLink;
  if (link.m_ids.size()<2 || !link.m_ids[1])
    return false;

  std::map<int, librevenge::RVNGString> idToNameMap;
  if (!cluster.m_nameLink.empty()) {
    readUnicodeStringList(cluster.m_nameLink, idToNameMap);
    cluster.m_nameLink=RagTime5StructManager::Link();
  }
  std::vector<long> decal;
  if (link.m_ids[0])
    readPositions(link.m_ids[0], decal);
  if (decal.empty())
    decal=link.m_longList;
  int const dataId=link.m_ids[1];
  shared_ptr<RagTime5StructManager::Zone> dataZone=getDataZone(dataId);
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
  return true;
}

bool RagTime5Parser::readListZone(RagTime5StructManager::Zone &/*zone*/, RagTime5StructManager::Link const &link)
{
  if (link.m_ids.size()<2 || !link.m_ids[1])
    return false;

  std::vector<long> decal;
  if (link.m_ids[0])
    readPositions(link.m_ids[0], decal);
  if (decal.empty())
    decal=link.m_longList;

  int const dataId=link.m_ids[1];
  shared_ptr<RagTime5StructManager::Zone> dataZone=getDataZone(dataId);
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
  f << "Entries(" << link.getZoneName() << ")[" << *dataZone << "]:";
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
      MWAW_DEBUG_MSG(("RagTime5Parser::readListZone: can not read the data zone %d-%d seems bad\n", dataId, i));
      continue;
    }
    input->seek(debPos+pos, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << link.getZoneName() << "-" << i+1 << ":";
    ascFile.addPos(debPos+pos);
    ascFile.addNote(f.str().c_str());
  }

  input->setReadInverted(false);
  return true;
}

bool RagTime5Parser::readStructZone(RagTime5StructManager::Cluster &cluster, RagTime5StructManager::FieldParser &parser,
                                    bool hasHeader)
{
  RagTime5StructManager::Link const &link=cluster.m_dataLink;
  if (link.m_ids.size()<2 || !link.m_ids[1])
    return false;

  std::map<int, librevenge::RVNGString> idToNameMap;
  if (!cluster.m_nameLink.empty()) {
    readUnicodeStringList(cluster.m_nameLink, idToNameMap);
    cluster.m_nameLink=RagTime5StructManager::Link();
  }
  std::vector<long> decal;
  if (link.m_ids[0])
    readPositions(link.m_ids[0], decal);
  if (decal.empty())
    decal=link.m_longList;
  int const dataId=link.m_ids[1];
  shared_ptr<RagTime5StructManager::Zone> dataZone=getDataZone(dataId);
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

bool RagTime5Parser::readStructData(RagTime5StructManager::Zone &zone, long endPos, int n, bool hasHeader,
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
    if (!m_structManager->readField(zone, endPos, field, hasHeader ? 0 : endPos-actPos)) {
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
bool RagTime5Parser::update(RagTime5StructManager::Zone &zone)
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

bool RagTime5Parser::unpackZone(RagTime5StructManager::Zone &zone, MWAWEntry const &entry, std::vector<unsigned char> &data)
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

bool RagTime5Parser::unpackZone(RagTime5StructManager::Zone &zone)
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
bool RagTime5Parser::readDocumentVersion(RagTime5StructManager::Zone &zone)
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
    shared_ptr<RagTime5StructManager::Zone> zone(new RagTime5StructManager::Zone(input, ascii()));
    zone->m_defPosition=pos;
    switch (type) {
    case 1:
      zone->m_fileType=RagTime5StructManager::Zone::F_Data;
      break;
    case 2:
      zone->m_fileType=RagTime5StructManager::Zone::F_Main;
      break;
    case 3:
      zone->m_fileType=RagTime5StructManager::Zone::F_Empty;
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
4e011000000001|000000000154a04200000000:000100a100010073000000110000000000000000010000643f847ae147ae147b3f847800000000000000000000000000000000000000000000000012

114:000000 [Entries(ItemDta):Data114A[1],BESoftware:ItemData,BESoftware:ItemData,0<->8,packed,,]0001007100010085

 */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
