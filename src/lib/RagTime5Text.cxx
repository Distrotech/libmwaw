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

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
