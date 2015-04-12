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

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWFont.hxx"
#include "MWAWListener.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "RagTime5Parser.hxx"
#include "RagTime5StructManager.hxx"
#include "RagTime5ClusterManager.hxx"

#include "RagTime5Text.hxx"

#include "libmwaw_internal.hxx"

/** Internal: the structures of a RagTime5Text */
namespace RagTime5TextInternal
{
////////////////////////////////////////
//! Internal: the helper to read field for a RagTime5Text
struct FieldParser : public RagTime5StructManager::FieldParser {
  //! constructor
  FieldParser(RagTime5Text &parser) : RagTime5StructManager::FieldParser("TextStyle"), m_mainParser(parser)
  {
  }
  //! return the debug name corresponding to a field
  std::string getZoneName(int n) const
  {
    std::stringstream s;
    s << "TextStyle-TS" << n;
    return s.str();
  }
  //! parse a field
  virtual bool parseField(RagTime5StructManager::Field &field, RagTime5Zone &zone, int /*n*/, libmwaw::DebugStream &f)
  {
    RagTime5StructManager::TextStyle style;
    MWAWInputStreamPtr input=zone.getInput();
    if (style.read(input, field))
      f << style;
    else
      f << "#" << field;
    return true;
  }

protected:
  //! the main parser
  RagTime5Text &m_mainParser;
};

////////////////////////////////////////
//! Internal: the state of a RagTime5Text
struct State {
  //! constructor
  State() : m_numPages(0) { }
  //! the number of pages
  int m_numPages;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
RagTime5Text::RagTime5Text(RagTime5Parser &parser) :
  m_mainParser(parser), m_structManager(m_mainParser.getStructManager()), m_parserState(parser.getParserState()),
  m_state(new RagTime5TextInternal::State)
{
}

RagTime5Text::~RagTime5Text()
{ }

int RagTime5Text::version() const
{
  return m_parserState->m_version;
}

int RagTime5Text::numPages() const
{
  // TODO IMPLEMENT ME
  MWAW_DEBUG_MSG(("RagTime5Text::numPages: is not implemented\n"));
  return 0;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// style
////////////////////////////////////////////////////////////
bool RagTime5Text::readTextStyles(RagTime5ClusterManager::Cluster &cluster)
{
  RagTime5TextInternal::FieldParser fieldParser(*this);
  return m_mainParser.readStructZone(cluster, fieldParser, 14);
}

////////////////////////////////////////////////////////////
// main zone
////////////////////////////////////////////////////////////
bool RagTime5Text::readTextZone(RagTime5ClusterManager::Cluster &cluster)
{
  RagTime5ClusterManager::Link const &link=cluster.m_dataLink;
  if (link.m_ids.size()<2 || !link.m_ids[1]) {
    MWAW_DEBUG_MSG(("RagTime5Text::readTextZone: can not find the data zone\n"));
    return false;
  }

  shared_ptr<RagTime5Zone> dataZone=m_mainParser.getDataZone(link.m_ids[0]);
  if (!dataZone || !dataZone->m_entry.valid() ||
      dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData") {
    MWAW_DEBUG_MSG(("RagTime5Text::readTextZone: can not find the first zone %d\n", link.m_ids[0]));
  }
  else {
    dataZone->m_isParsed=true;
    MWAWEntry entry=dataZone->m_entry;
    libmwaw::DebugFile &ascFile=dataZone->ascii();
    libmwaw::DebugStream f;
    f << "Entries(TextPosition)[" << *dataZone << "]:";
    ascFile.addPos(entry.end());
    ascFile.addNote("_");
    ascFile.addPos(entry.begin());
    ascFile.addNote(f.str().c_str());
  }
  int const dataId=link.m_ids[1];
  dataZone=m_mainParser.getDataZone(dataId);
  if (!dataZone || !dataZone->m_entry.valid() ||
      dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="Unicode") {
    MWAW_DEBUG_MSG(("RagTime5Text::readTextZone: the text zone %d seems bad\n", dataId));
    return false;
  }
  return m_mainParser.readUnicodeString(*dataZone);
}

////////////////////////////////////////////////////////////
// link/list definition
////////////////////////////////////////////////////////////
bool RagTime5Text::readFieldZones(RagTime5ClusterManager::Cluster &/*cluster*/, RagTime5ClusterManager::Link const &link,
                                  bool isDefinition)
{
  if (link.m_ids.size()<2 || !link.m_ids[1])
    return false;

  std::vector<long> decal;
  if (link.m_ids[0])
    m_mainParser.readPositions(link.m_ids[0], decal);
  if (decal.empty())
    decal=link.m_longList;

  int const dataId=link.m_ids[1];
  shared_ptr<RagTime5Zone> dataZone=m_mainParser.getDataZone(dataId);
  int N=int(decal.size());

  if (!dataZone || !dataZone->m_entry.valid() ||
      dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData" || N<=1) {
    if (N==1 && dataZone && !dataZone->m_entry.valid()) {
      // a zone with 0 zone is ok...
      dataZone->m_isParsed=true;
      return true;
    }
    MWAW_DEBUG_MSG(("RagTime5Text::readFieldZones: the data zone %d seems bad\n", dataId));
    return false;
  }

  dataZone->m_isParsed=true;
  MWAWEntry entry=dataZone->m_entry;
  libmwaw::DebugFile &ascFile=dataZone->ascii();
  libmwaw::DebugStream f;
  std::string name(isDefinition ? "FieldDef" : "FieldPos");
  f << "Entries(" << name << ")[" << *dataZone << "]:";
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
    long nextPos=decal[size_t(i+1)];
    if (nextPos==pos) continue;
    if (pos<0 || debPos+nextPos>endPos || pos>nextPos) {
      MWAW_DEBUG_MSG(("RagTime5Text::readFieldZones: can not read the data zone %d-%d seems bad\n", dataId, i));
      if (debPos+pos<endPos) {
        f.str("");
        f << name << "-" << i+1 << ":###";
        ascFile.addPos(debPos+pos);
        ascFile.addNote(f.str().c_str());
      }
      continue;
    }
    input->seek(debPos+pos, librevenge::RVNG_SEEK_SET);
    if ((isDefinition && readFieldDefinition(*dataZone, debPos+nextPos,i+1)) ||
        (!isDefinition && readFieldPosition(*dataZone, debPos+nextPos,i+1)))
      continue;
    f.str("");
    f << name << "-" << i+1 << ":";
    ascFile.addPos(debPos+pos);
    ascFile.addNote(f.str().c_str());
  }

  input->setReadInverted(false);
  return true;
}

bool RagTime5Text::readFieldDefinition(RagTime5Zone &zone, long endPos, int n)
{
  MWAWInputStreamPtr input=zone.getInput();
  long pos=input->tell();
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  f << "FieldDef-" << n << ":";
  if (pos+6>endPos) {
    MWAW_DEBUG_MSG(("RagTime5Text::readFieldDefinition: the zone seems too short\n"));
    f<<"###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  int decal[5]= {6,0,0,0,int(endPos-pos)};
  for (int i=1; i<4; ++i) {
    decal[i]=(int) input->readULong(2);
    if (decal[i]==0) continue;
    if (decal[i]&0x8000) {
      f << "fl" << i << ",";
      decal[i] &= 0x7FFF;
    }
    if (decal[i]<6 || pos+decal[i]>=endPos) {
      MWAW_DEBUG_MSG(("RagTime5Text::readFieldDefinition: the %d pointer seems bad\n", i));
      f << "##decal[" << i << "]=" << decal[i] << ",";
      decal[i]=0;
      continue;
    }
  }
  for (int i=3; i>=1; --i) {
    if (!decal[i])
      decal[i]=decal[i+1];
  }
  for (int i=0; i<4; ++i) {
    if (decal[i+1]==decal[i])
      continue;
    if (decal[i+1]<decal[i]) {
      MWAW_DEBUG_MSG(("RagTime5Text::readFieldDefinition: the %d pointer seems bad\n", i));
      f << "##decal" << i << ",";
      continue;
    }
    switch (i) {
    case 0: {
      if (decal[i+1]-decal[i]<8) {
        MWAW_DEBUG_MSG(("RagTime5Text::readFieldDefinition: the zone 0 size seems bad\n"));
        f << "##decal2,";
        break;
      }
      input->seek(pos+decal[i], librevenge::RVNG_SEEK_SET);
      int val=(int) input->readLong(2); // always 0?
      if (val) f<< "#f0=" << val << ",";
      val=(int) input->readLong(2); // small value often equal to 100
      if (val) f << "f1=" << val << ",";
      f << "f2=" << input->readULong(2) << ","; // big number
      if (input->tell()!=pos+decal[i+1])
        ascFile.addDelimiter(input->tell(),'|');
      ascFile.addDelimiter(pos+decal[i+1],'|');
      break;
    }
    case 1:
      ascFile.addDelimiter(pos+decal[i+1],'|');
      break;
    case 2: {
      // list of small int
      if ((decal[i+1]-decal[i])%4) {
        MWAW_DEBUG_MSG(("RagTime5Text::readFieldDefinition: the zone 2 size seems bad\n"));
        f << "##decal2,";
        break;
      }
      input->seek(pos+decal[i], librevenge::RVNG_SEEK_SET);
      long endDataPos=pos+decal[i+1];
      f << "list2=[";
      while (!input->isEnd()) {
        long begDataPos=input->tell();
        if (begDataPos==endDataPos) break;
        if (begDataPos+4>endDataPos) {
          MWAW_DEBUG_MSG(("RagTime5Text::readFieldDefinition: problem with length for zone 2\n"));
          f << "#end,";
          break;
        }
        // FIXME: this list is either a simple list or a list of data
        long lVal=(long) input->readULong(4);
        if ((lVal>>24)==3) {
          f << std::hex << (lVal&0xFFFFFF) << std::dec << ",";
          continue;
        }
        input->seek(begDataPos, librevenge::RVNG_SEEK_SET);
        std::vector<int> listIds;
        if (begDataPos+8>endDataPos || !m_structManager->readDataIdList(input, 1, listIds)) {
          MWAW_DEBUG_MSG(("RagTime5Text::readFieldDefinition: can not read data  for zone 2\n"));
          f << "#type=" << std::hex << lVal << std::dec << ",";
          break;
        }
        if (listIds[0]) // some cluster data
          f << "data" << listIds[0] << "A:";
        lVal=(long) input->readULong(4);
        f << std::hex << (lVal&0xFFFFFF) << std::dec;
        if ((lVal>>24)!=3) f << "[" << (lVal>>24) << "]";
        f << ",";
      }
      f << "],";
      break;
    }
    case 3: { // list of small int
      if ((decal[i+1]-decal[i])%2) {
        MWAW_DEBUG_MSG(("RagTime5Text::readFieldDefinition: the zone 3 size seems bad\n"));
        f << "##decal3,";
        break;
      }
      int N=(decal[i+1]-decal[i])/2;
      input->seek(pos+decal[i], librevenge::RVNG_SEEK_SET);
      f << "list3=[";
      for (int j=0; j<N; ++j)
        f << input->readLong(2) << ",";
      f << "],";
      break;
    }
    default:
      break;
    }
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool RagTime5Text::readFieldPosition(RagTime5Zone &zone, long endPos, int n)
{
  MWAWInputStreamPtr input=zone.getInput();
  long pos=input->tell();
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  f << "FieldPos-" << n << ":";
  if ((endPos-pos)%8) {
    MWAW_DEBUG_MSG(("RagTime5Text::readFieldPosition: the zone seems bad\n"));
    f<<"###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  int N=int((endPos-pos)/8);
  f << "cluster=[";
  for (int i=0; i<N; ++i) {
    long actPos=input->tell();
    std::vector<int> listIds; // the cluster which contains the definition
    if (!m_structManager->readDataIdList(input, 1, listIds)) {
      MWAW_DEBUG_MSG(("RagTime5Text::readFieldPosition: find unknown block type\n"));
      f << "##type,";
      input->seek(actPos+8, librevenge::RVNG_SEEK_SET);
      continue;
    }
    int id=(int) input->readULong(4);
    if (listIds[0]==0) f << "_,";
    else f << "data" << listIds[0] << "A-TF" << id << ",";
  }
  f << "],";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// link/list definition
////////////////////////////////////////////////////////////
bool RagTime5Text::readLinkZones(RagTime5ClusterManager::Cluster &cluster, RagTime5ClusterManager::Link const &link)
{
  if (link.m_ids.empty()) {
    MWAW_DEBUG_MSG(("RagTime5Text::readLinkZones: can not find the first zone id\n"));
    return false;
  }
  if (link.m_ids.size()>=3 && link.m_ids[2]) {
    std::vector<long> decal;
    if (link.m_ids[1])
      m_mainParser.readPositions(link.m_ids[1], decal);
    if (decal.empty())
      decal=link.m_longList;
    int const dataId=link.m_ids[2];
    shared_ptr<RagTime5Zone> dataZone=m_mainParser.getDataZone(dataId);
    if (!dataZone || !dataZone->m_entry.valid() ||
        dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData") {
      if (decal.size()==1) {
        // a graphic zone with 0 zone is ok...
        dataZone->m_isParsed=true;
      }
      MWAW_DEBUG_MSG(("RagTime5Text::readLinkZones: the data zone %d seems bad\n", dataId));
    }
    else {
      MWAWEntry entry=dataZone->m_entry;
      dataZone->m_isParsed=true;

      libmwaw::DebugFile &ascFile=dataZone->ascii();
      libmwaw::DebugStream f;
      f << "Entries(LinkDef)[" << *dataZone << "]:";
      ascFile.addPos(entry.end());
      ascFile.addNote("_");

      if (decal.size() <= 1) {
        MWAW_DEBUG_MSG(("RagTime5Text::readLinkZones: can not find position for the data zone %d\n", dataId));
        f << "###";
        ascFile.addPos(entry.begin());
        ascFile.addNote(f.str().c_str());
      }
      else {
        int N=int(decal.size());
        MWAWInputStreamPtr input=dataZone->getInput();
        input->setReadInverted(!cluster.m_hiLoEndian); // checkme maybe zone

        ascFile.addPos(entry.begin());
        ascFile.addNote(f.str().c_str());

        for (int i=0; i<N-1; ++i) {
          long pos=decal[size_t(i)], nextPos=decal[size_t(i+1)];
          if (pos==nextPos) continue;
          if (pos<0 || pos>entry.length()) {
            MWAW_DEBUG_MSG(("RagTime5Text::readLinkZones: can not read the data zone %d-%d seems bad\n", dataId, i));
            continue;
          }
          f.str("");
          f << "LinkDef-" << i+1 << ":";
          librevenge::RVNGString string;
          input->seek(pos+entry.begin(), librevenge::RVNG_SEEK_SET);
          if (nextPos>entry.length() || !m_structManager->readUnicodeString(input, entry.begin()+nextPos, string)) {
            MWAW_DEBUG_MSG(("RagTime5Text::readLinkZones: can not read a string\n"));
            f << "###";
          }
          else if (!string.empty() && string.cstr()[0]=='\0')
            f << "\"" << string.cstr()+1 << "\",";
          else
            f << "\"" << string.cstr() << "\",";
          ascFile.addPos(entry.begin()+pos);
          ascFile.addNote(f.str().c_str());
        }
        input->setReadInverted(false);
      }
    }
  }
  // ok no list
  if (!link.m_ids[0])
    return true;
  shared_ptr<RagTime5Zone> dataZone=m_mainParser.getDataZone(link.m_ids[0]);
  if (!dataZone || dataZone->getKindLastPart()!="ItemData") {
    MWAW_DEBUG_MSG(("RagTime5Text::readLinkZones: can not find the first zone %d\n", link.m_ids[0]));
    return false;
  }

  // ok no list
  if (!dataZone->m_entry.valid())
    return true;

  MWAWInputStreamPtr input=dataZone->getInput();
  bool const hiLo=dataZone->m_hiLoEndian;
  input->setReadInverted(!hiLo);
  input->seek(dataZone->m_entry.begin(), librevenge::RVNG_SEEK_SET);
  dataZone->m_isParsed=true;

  libmwaw::DebugFile &ascFile=dataZone->ascii();
  libmwaw::DebugStream f;
  ascFile.addPos(dataZone->m_entry.end());
  ascFile.addNote("_");

  if (dataZone->m_entry.length()<link.m_fieldSize*link.m_N || link.m_fieldSize<=0 || link.m_N<=0) {
    MWAW_DEBUG_MSG(("RagTime5Text::readLinkZones: the position zone %d seems bad\n", dataZone->m_ids[0]));
    f << "Entries(LinkPos)[" << *dataZone << "]:" << link << "###,";
    input->setReadInverted(false);
    ascFile.addPos(dataZone->m_entry.begin());
    ascFile.addNote(f.str().c_str());
    return false;
  }
  for (int i=0; i<link.m_N; ++i) {
    long pos=input->tell();
    f.str("");
    if (i==0)
      f << "Entries(LinkPos)[" << *dataZone << "]:";
    else
      f << "LinkPos-" << i << ":";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+link.m_fieldSize, librevenge::RVNG_SEEK_SET);
  }
  if (input->tell()<dataZone->m_entry.end()) {
    f.str("");
    f << "LinkPos-:end";
    // check me: the size seems always a multiple of 16, so maybe reserved data...
    if (dataZone->m_entry.length()%link.m_fieldSize) {
      f << "###";
      static bool first=true;
      if (first) {
        MWAW_DEBUG_MSG(("RagTime5Text::readLinkZones: find some extra data\n"));
        first=false;
      }
    }
    ascFile.addPos(input->tell());
    ascFile.addNote(f.str().c_str());
  }
  input->setReadInverted(false);
  return true;
}


////////////////////////////////////////////////////////////
// unknown
////////////////////////////////////////////////////////////
bool RagTime5Text::readTextUnknown(int typeId)
{
  if (!typeId)
    return false;

  shared_ptr<RagTime5Zone> zone=m_mainParser.getDataZone(typeId);
  if (!zone || !zone->m_entry.valid() || (zone->m_entry.length()%6) ||
      zone->getKindLastPart(zone->m_kinds[1].empty())!="ItemData") {
    MWAW_DEBUG_MSG(("RagTime5Text::readTextUnknown: the entry of zone %d seems bad\n", typeId));
    return false;
  }
  MWAWEntry entry=zone->m_entry;
  MWAWInputStreamPtr input=zone->getInput();
  input->setReadInverted(!zone->m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);

  libmwaw::DebugFile &ascFile=zone->ascii();
  libmwaw::DebugStream f;
  zone->m_isParsed=true;
  ascFile.addPos(entry.end());
  ascFile.addNote("_");

  f << "Entries(TextUnknown)[" << *zone << "]:";
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());

  int N=int(entry.length()/6);
  for (int i=0; i<N; ++i) {
    long pos=input->tell();
    f.str("");
    f << "TextUnknown-" << i << ":";
    f << "offset?=" << input->readULong(4) << ",";
    f << "TS" << input->readULong(2) << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  input->setReadInverted(false);
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// interface send function
////////////////////////////////////////////////////////////

void RagTime5Text::flushExtra()
{
  MWAW_DEBUG_MSG(("RagTime5Text::flushExtra: is not implemented\n"));
}

////////////////////////////////////////////////////////////
// cluster parser
////////////////////////////////////////////////////////////

namespace RagTime5TextInternal
{
//
//! low level: parser of text cluster
//
struct TextCParser : public RagTime5ClusterManager::ClusterParser {
  //! constructor
  TextCParser(RagTime5ClusterManager &parser, int type, libmwaw::DebugFile &ascii) :
    ClusterParser(parser, type, "ClustText"), m_cluster(new RagTime5ClusterManager::Cluster), m_what(-1), m_linkId(-1), m_fieldName(""), m_asciiFile(ascii)
  {
  }
  //! return the current cluster
  shared_ptr<RagTime5ClusterManager::Cluster> getCluster()
  {
    return m_cluster;
  }
  //! return the text cluster
  shared_ptr<RagTime5ClusterManager::Cluster> getTextCluster()
  {
    return m_cluster;
  }
  //! end of a start zone call
  void endZone()
  {
    if (m_link.empty())
      return;
    switch (m_linkId) {
    default:
      if (m_what==0 || m_what==2) {
        if (m_cluster->m_dataLink.empty())
          m_cluster->m_dataLink=m_link;
        else {
          MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::TextCParser::endZone: oops the main link is already set\n"));
          m_cluster->m_linksList.push_back(m_link);
        }
      }
      else
        m_cluster->m_linksList.push_back(m_link);
      break;
    }
  }
  //! parse a zone
  bool parseZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    m_what=m_linkId=-1;
    m_fieldName="";
    if (N==-5)
      return parseHeaderZone(input,fSz,N,flag,f);
    if (N<0) {
      MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::TextCParser::parseZone: expected N value\n"));
      f << "###N=" << N << ",";
      return true;
    }
    return parseDataZone(input, fSz, N, flag, f);
  }
  //! parse a field
  bool parseField(RagTime5StructManager::Field const &field, int /*m*/, libmwaw::DebugStream &f)
  {
    if (!m_fieldName.empty())
      f << m_fieldName << ",";
    switch (m_what) {
    case 1: // linkdef
      if (field.m_type==RagTime5StructManager::Field::T_FieldList &&
          (field.m_fileType==0x15f4815 /* v5?*/ || field.m_fileType==0x160f815 /* v6? */)) {
        f << "decal=[";
        for (size_t i=0; i<field.m_fieldList.size(); ++i) {
          RagTime5StructManager::Field const &child=field.m_fieldList[i];
          if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==0xce842) {
            for (size_t j=0; j<child.m_longList.size(); ++j)
              f << child.m_longList[j] << ",";
            m_link.m_longList=child.m_longList;
            continue;
          }
          MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::TextCParser::parseField: find unexpected decal child[list]\n"));
          f << "#[" << child << "],";
        }
        f << "],";
        break;
      }
      else if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
        f << "unkn0=" << field.m_extra; // always 2: next value ?
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::TextCParser::parseField: find unexpected child[list]\n"));
      f << "###" << field;
      break;
    case 3: // list link
      if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xce842) {
        f << "pos=[";
        for (size_t i=0; i<field.m_longList.size(); ++i)
          f << field.m_longList[i] << ",";
        f << "],";
        m_link.m_longList=field.m_longList;
        break;
      }
      if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
        // a small value 2|4|a|1c|40
        f << "unkn="<<field.m_extra << ",";
        break;
      }
      if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xcf042) {
        f << "unkn=[";
        for (size_t j=0; j<field.m_longList.size(); ++j) {
          if (field.m_longList[j]==0)
            f << "_,";
          else
            f << field.m_longList[j] << ",";
        }
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::TextCParser::parseField: find unexpected list link field\n"));
      f << "###" << field;
      break;
    default:
      f << field;
      break;
    }
    return true;
  }
protected:
  //! parse a data block
  bool parseDataZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    /* find 80* +
       34,41,39,28,52,
       34,41,39,36,28,52,
       34,41,39,36,29,49,28,52,39,
       34,41,39,36,80,28,52,
       34,41,39,39,36,28,52,
       34,41,69,39,36,28,52,
       34,41,71,39,36,29,49,28,52,39,
       34,46,39,36,28,52,
     */
    f << "fl=" << std::hex << flag << std::dec << ",";
    long pos=input->tell();
    input->seek(pos+12, librevenge::RVNG_SEEK_SET);
    long type=(long) input->readULong(4);
    m_link.m_N=N;
    if (type==0x15f3817||type==0x15e4817) { /* fSz=39, 69 or 71, second 34 */
      m_link.m_fileType[0]=type;
      m_link.m_fileType[1]=(long) input->readULong(2);
      m_link.m_fieldSize=(int) input->readULong(2);
      if (RagTime5StructManager::readDataIdList(input, 1, m_link.m_ids) && m_link.m_ids[0]) {
        if (m_link.m_fileType[0]==0x15e4817) {
          m_fieldName="textUnknown";
          m_link.m_type=RagTime5ClusterManager::Link::L_TextUnknown;
        }
        else {
          m_fieldName="linkDef1";
          m_what=1;
          m_link.m_type=RagTime5ClusterManager::Link::L_LinkDef;
          if (fSz>=71) { // the definitions
            m_asciiFile.addDelimiter(input->tell(),'|');
            input->seek(pos+57, librevenge::RVNG_SEEK_SET);
            m_asciiFile.addDelimiter(input->tell(),'|');
            std::vector<int> dataId;
            if (RagTime5StructManager::readDataIdList(input, 2, dataId) && dataId[1]) {
              m_link.m_ids.push_back(dataId[0]);
              m_link.m_ids.push_back(dataId[1]);
            }
          }
        }
      }
      f << m_link << ",";
      f << m_fieldName << ",";
      return true;
    }

    input->seek(pos, librevenge::RVNG_SEEK_SET);
    bool ok=false;
    int val;
    long linkValues[5];
    std::string mess("");
    switch (fSz) {
    case 28: {
      m_link.m_fileType[0]=(int) input->readULong(4);
      if (m_link.m_fileType[0]!=0x100004) {
        f << "fType=" << std::hex << m_link.m_fileType[0] << std::dec << ",";
        break;
      }
      ok=true;
      m_link.m_type=RagTime5ClusterManager::Link::L_Text;
      m_fieldName="textZone";
      m_what=2;
      f << "f0=" << N << ",";
      val=(int) input->readLong(2); // always 0?
      if (val) f << "f1=" << val << ",";
      val=(int) input->readLong(2); // always f?
      if (val!=15) f << "f2=" << val << ",";
      std::vector<int> listIds;
      if (RagTime5StructManager::readDataIdList(input, 1, listIds))
        m_link.m_ids.push_back((int) listIds[0]);
      else {
        f << "#link0,";
        m_link.m_ids.push_back(0);
      }
      m_link.m_N=(int) input->readULong(4);
      val=(int) input->readLong(1); // always 0?
      if (val) f << "f3=" << val << ",";
      listIds.clear();
      if (RagTime5StructManager::readDataIdList(input, 1, listIds) && listIds[0])
        m_link.m_ids.push_back(listIds[0]);
      else {
        f << "#link1,";
        m_link.m_ids.push_back(0);
      }
      val=(int) input->readLong(1); // always 1?
      if (val) f << "f4=" << val << ",";
      f << m_link;
      break;
    }
    case 32:
    case 36:
    case 41: {
      if (!readLinkHeader(input, fSz, m_link, linkValues, mess))
        break;
      ok=true;
      m_what=3;
      m_link.m_type=RagTime5ClusterManager::Link::L_List;
      m_link.m_N=N;
      m_fieldName="listLink";
      f << m_link << "," << mess;
      /* checkme:
         link.m_fileType[1]==20|0 is a list of fields
         link.m_fileType[1]==1010|1038 is a list of sequence of 16 bytes
      */
      m_asciiFile.addDelimiter(pos+14,'|');
      break;
    }
    case 0x50: {
      if (N!=1) {
        MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::TextCParser::parseDataZone: expected N value[graphDim]\n"));
        f << "##N=" << N << ",";
      }
      ok=true;
      m_fieldName="graphDim";
      val=(int) input->readULong(2); // always 0?
      if (val) f << "f0=" << val << ",";
      val=(int) input->readULong(2);
      if (val) f << "id=" << val << ",";
      val=(int) input->readULong(2); //[04][01248a][01][23]
      if (val) f << "fl=" << std::hex << val << std::dec << ",";
      for (int i=0; i<2; ++i) {
        val=(int) input->readULong(2); //f1=0-1e, f2=0|3ffe
        if (val) f << "f" << i+1 << "=" << val << ",";
      }
      float dim[4];
      for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
      Box2f box(Vec2f(dim[0],dim[1]), Vec2f(dim[2],dim[3]));
      f << "box=" << box << ",";
      for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
      Box2f box2(Vec2f(dim[0],dim[1]), Vec2f(dim[2],dim[3]));
      if (box!=box2)
        f << "boxA=" << box2 << ",";
      for (int i=0; i<7; ++i) { // g1=0|2, g3=9|7
        val=(int) input->readLong(2);
        if (val) f << "g" << i << "=" << val << ",";
      }
      for (int i=0; i<9; ++i) { // h0=a flag?, h2= a flag?, h5=h6=0|-1
        val=(int) input->readLong(2);
        if (val) f << "h" << i << "=" << val << ",";
      }
      break;
    }
    default:
      break;
    }
    if (ok) {
      if (!m_fieldName.empty())
        f << m_fieldName << ",";
      return true;
    }
    if ((fSz==0x27||fSz==0x34) && N>0) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      if (readLinkHeader(input, fSz, m_link, linkValues, mess)) {
        m_link.m_N=N;
        m_fieldName="UnknFixZone";
        f << m_fieldName << "," << m_link << "," << mess;
        m_link.m_fileType[0]=linkValues[0];
      }
      else
        m_link=RagTime5ClusterManager::Link();
    }

    return true;
  }
  //! parse the header zone
  bool parseHeaderZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    f << "header, fl=" << std::hex << flag << std::dec << ",";
    m_fieldName="header";
    m_what=0;
    if (N!=-5 || m_dataId!=0 || (fSz!=135 && fSz!=140 && fSz!=143)) {
      f << "###N=" << N << ",fSz=" << fSz << ",";
      MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::TextCParser::parseHeaderZone: find unexpected main field\n"));
      return true;
    }
    int val;
    for (int i=0; i<2; ++i) { // always 0?
      val=(int) input->readLong(2);
      if (val) f << "f" << i+1 << "=" << val << ",";
    }
    val=(int) input->readLong(2);
    f << "id=" << val << ",";
    val=(int) input->readULong(2);
    if (m_type>0 && val!=m_type) {
      MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::TextCParser::parseHeaderZone: unexpected zone type\n"));
      f << "##zoneType=" << std::hex << val << std::dec << ",";
    }
    for (int i=0; i<2; ++i) { // f0=9-5d, f1=0
      val=(int) input->readLong(4);
      if (val)
        f << "f" << i << "=" << val << ",";
    }
    val=(int) input->readLong(1); // 0|1
    if (val)
      f << "fl=" << val << ",";
    val=(int) input->readULong(2);
    if (val) // [08]0[08][049]
      f << "fl2=" << std::hex << val << std::dec << ",";
    val=(int) input->readLong(1); // 1|1d
    if (val!=1)
      f << "fl3=" << val << ",";
    val=(int) input->readULong(2); // alway 10
    if (val!=0x10)
      f << "f2=" << val << ",";
    val=(int) input->readLong(4);
    if (val)
      f << "f3=" << val << ",";
    for (int i=0; i<11; ++i) { // g8=40|60
      val=(int) input->readLong(2);
      if (val)
        f << "g" << i << "=" << val << ",";
    }
    val=(int) input->readLong(1); // always 1
    if (val!=1)
      f << "fl4=" << val << ",";
    if (fSz==140) {
      for (int i=0; i<5; ++i) { // unsure find only 0 here
        val=(int) input->readLong(1);
        if (val)
          f << "flA" << i << "=" << val << ",";
      }
    }

    for (int i=0; i<2; ++i) { // always 1,2
      val=(int) input->readLong(4);
      if (val!=i+1)
        f << "h" << i << "=" << val << ",";
    }
    for (int i=0; i<2; ++i) { // always 0,4
      val=(int) input->readLong(2);
      if (val)
        f << "h" << i+2 << "=" << val << ",";
    }
    for (int i=0; i<4; ++i) { // always h4=3, h5=small number, h6=h5+1
      val=(int) input->readLong(4);
      if (val)
        f << "h" << i+4 << "=" << val << ",";
    }
    for (int i=0; i<2; ++i) {  // always 1,4
      val=(int) input->readLong(2);
      if (val)
        f << "h" << i+8 << "=" << val << ",";
    }
    val=(int) input->readULong(4);
    if (val!=0x5555)
      f << "#fileType=" << std::hex << val << std::dec << ",";
    val=(int) input->readULong(4);
    if (val!=0x18000)
      f << "#fileType2=" << std::hex << val << std::dec << ",";
    for (int i=0; i<5; ++i) { // always 0
      val=(int) input->readLong(2);
      if (val)
        f << "j" << i << "=" << val << ",";
    }
    for (int i=0; i<5; ++i) { // j5=0|5, j6=0|5, j7=small number, j8=0|5
      val=(int) input->readLong(4);
      if (val)
        f << "j" << i+5 << "=" << val << ",";
    }
    f << "IDS=[";
    for (int i=0; i<2; ++i) // unsure, junk
      f << std::hex << input->readULong(4) << std::dec << ",";
    f << "],";
    val=(int) input->readULong(2); // c00|cef
    if (val)
      f << "fl5=" << std::hex << val << std::dec << ",";
    if (fSz==143) {
      for (int i=0; i<4; ++i) { // always 0
        val=(int) input->readLong(2);
        if (val)
          f << "k" << i << "=" << val << ",";
      }
    }
    return true;
  }

  //! the current cluster
  shared_ptr<RagTime5ClusterManager::Cluster> m_cluster;
  //! a index to know which field is parsed :  0: main, 1: linkdef, 2: textZone(store in mainData), 3: list
  int m_what;
  //! the link id:
  int m_linkId;
  //! the actual field name
  std::string m_fieldName;
  //! the ascii file
  libmwaw::DebugFile &m_asciiFile;
private:
  //! copy constructor (not implemented)
  TextCParser(TextCParser const &orig);
  //! copy operator (not implemented)
  TextCParser &operator=(TextCParser const &orig);
};

}

bool RagTime5Text::readTextCluster(RagTime5Zone &zone, int zoneType)
{
  shared_ptr<RagTime5ClusterManager> clusterManager=m_mainParser.getClusterManager();
  if (!clusterManager) {
    MWAW_DEBUG_MSG(("RagTime5Text::readTextCluster: oops can not find the cluster manager\n"));
    return false;
  }
  RagTime5TextInternal::TextCParser parser(*clusterManager, zoneType, zone.ascii());
  if (!clusterManager->readCluster(zone, parser) || !parser.getTextCluster()) {
    MWAW_DEBUG_MSG(("RagTime5Text::readTextCluster: oops can not find the cluster\n"));
    return false;
  }
  shared_ptr<RagTime5ClusterManager::Cluster> cluster=parser.getTextCluster();
  m_mainParser.checkClusterList(cluster->m_clusterIdsList);

  readTextZone(*cluster);
  if (!cluster->m_nameLink.empty()) {
    std::map<int, librevenge::RVNGString> idToStringMap;
    m_mainParser.readUnicodeStringList(cluster->m_nameLink, idToStringMap);
  }

  for (size_t i=0; i<cluster->m_linksList.size(); ++i) {
    RagTime5ClusterManager::Link const &link=cluster->m_linksList[i];
    if (link.m_type==RagTime5ClusterManager::Link::L_List) {
      m_mainParser.readListZone(link);
      continue;
    }
    else if (link.m_type==RagTime5ClusterManager::Link::L_LongList) {
      std::vector<long> list;
      m_mainParser.readLongList(link, list);
      continue;
    }
    else if (link.m_type==RagTime5ClusterManager::Link::L_LinkDef) {
      readLinkZones(*cluster, link);
      continue;
    }

    if (link.empty()) continue;
    if (link.m_type==RagTime5ClusterManager::Link::L_TextUnknown) {
      readTextUnknown(link.m_ids[0]);
      continue;
    }
    shared_ptr<RagTime5Zone> data=m_mainParser.getDataZone(link.m_ids[0]);
    if (!data || data->m_isParsed) {
      MWAW_DEBUG_MSG(("RagTime5Text::readTextCluster: can not find data zone %d\n", link.m_ids[0]));
      continue;
    }
    data->m_hiLoEndian=zone.m_hiLoEndian;
    if (link.m_fieldSize==6 && link.m_fileType[0]==0 && link.m_fileType[1]==0 && m_mainParser.readUnknZoneA(*data, link))
      continue;
    if (link.m_fieldSize==0 && !data->m_entry.valid())
      continue;

    if (link.m_type==RagTime5ClusterManager::Link::L_UnknownItem) {
      if (!data->m_entry.valid())
        continue;
      MWAW_DEBUG_MSG(("RagTime5Parser::readClusterZone: find an unknown link %d\n", link.m_ids[0]));
      long pos=data->m_entry.begin();
      data->m_isParsed=true;
      libmwaw::DebugFile &dAscFile=data->ascii();
      libmwaw::DebugStream f;
      f << "Entries(" << link.getZoneName() << ")[" << *data << "]:" << "###";
      dAscFile.addPos(pos);
      dAscFile.addNote(f.str().c_str());
    }
    else
      m_mainParser.readFixedSizeZone(link, "");
  }
  return true;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
