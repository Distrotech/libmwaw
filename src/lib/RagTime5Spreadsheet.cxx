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
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "RagTime5Parser.hxx"
#include "RagTime5StructManager.hxx"
#include "RagTime5ClusterManager.hxx"

#include "RagTime5Spreadsheet.hxx"

#include "libmwaw_internal.hxx"

/** Internal: the structures of a RagTime5Spreadsheet */
namespace RagTime5SpreadsheetInternal
{
//! Internal: the helper to read a clustList
struct ClustListParser : public RagTime5StructManager::DataParser {
  //! constructor
  ClustListParser(RagTime5ClusterManager &clusterManager, std::string const &zoneName) :
    RagTime5StructManager::DataParser(zoneName), m_clusterList(), m_clusterManager(clusterManager)
  {
  }

  std::string getClusterName(int id) const
  {
    return m_clusterManager.getClusterName(id);
  }

  //! try to parse a data
  bool parseData(MWAWInputStreamPtr &input, long endPos, RagTime5Zone &/*zone*/, int /*n*/, libmwaw::DebugStream &f)
  {
    long pos=input->tell();
    if (endPos-pos!=24) {
      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::ClustListParser::parse: bad data size\n"));
      return false;
    }

    std::vector<int> listIds;
    if (!RagTime5StructManager::readDataIdList(input, 1, listIds)) {
      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::ClustListParser::parse: can not read an cluster id\n"));
      f << "##clusterIds,";
      return false;
    }
    if (listIds[0]) {
      m_clusterList.push_back(listIds[0]);
      // a e,2003,200b, ... cluster
      f << getClusterName(listIds[0]) << ",";
    }
    unsigned long lVal=input->readULong(4); // c00..small number
    if ((lVal&0xc0000000)==0xc0000000)
      f << "f0=" << (lVal&0x3fffffff) << ",";
    else
      f << "f0*" << lVal << ",";
    for (int i=0; i<8; ++i) { // f1=0|1, f2=f3=f4=0, f5=0|c, f6=0|d|e
      int val=(int) input->readLong(2);
      if (val) f << "f" << i << "=" << val << ",";
    }
    return true;
  }

  //! the list of read cluster
  std::vector<int> m_clusterList;
private:
  //! the main zone manager
  RagTime5ClusterManager &m_clusterManager;
  //! copy constructor, not implemented
  ClustListParser(ClustListParser &orig);
  //! copy operator, not implemented
  ClustListParser &operator=(ClustListParser &orig);
};

//! Internal: the helper to read a cell content
struct ContentParser : public RagTime5StructManager::DataParser {
  //! constructor
  ContentParser() : RagTime5StructManager::DataParser("SheetContent")
  {
  }

  //! try to parse a data
  bool parseData(MWAWInputStreamPtr &input, long endPos, RagTime5Zone &/*zone*/, int /*n*/, libmwaw::DebugStream &f)
  {
    long pos=input->tell();
    long fSz=endPos-pos;
    if (fSz<2) {
      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::ContentParser::parse: bad data size\n"));
      return false;
    }
    int type=(int) input->readULong(2);
    bool hasIndex[3]= {(type&0x40)!=0, (type&0x80)!=0, (type&0x2000)!=0};
    if (type&0x4E30)
      f << "fl" << std::hex << (type&0x4E30) << std::dec << ",";
    type &= 0x910F;
    switch (type) {
    case 0: // empty
      break;
    case 1:
    case 0xa:
      if (fSz<4) {
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::ContentParser::parse: find bad size for long\n"));
        f << "##fSz[long],";
        return true;
      }
      f << "val=" << input->readLong(4) << ",";
      break;
    case 2: // find 800b88[47]8, so ?
      if (fSz<4) {
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::ContentParser::parse: find bad size for long\n"));
        f << "##fSz[long3],";
        return true;
      }
      f << "val=" << std::hex << input->readULong(4) << std::dec << ",";
      break;
    case 4:
    case 5: // date
    case 6: { // time
      if (fSz<10) {
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::ContentParser::parse: find bad size for double\n"));
        f << "##fSz[double],";
        return true;
      }
      double res;
      bool isNan;
      if (!input->readDouble8(res, isNan)) {
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::ContentParser::parse: can not read a double\n"));
        f << "###double,";
        return true;
      }
      f << "val=" << res << ",";
      break;
    }
    case 7: {
      for (int i=0; i<3; ++i) {
        if (!hasIndex[i]) continue;
        if (input->tell()+4>endPos) {
          MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::ContentParser::parse: can not read index\n"));
          f << "##index[string],";
          return true;
        }
        f << "f" << i << "=" << input->readLong(4) << ",";
        hasIndex[i]=false;
      }
      librevenge::RVNGString res("");
      if (!RagTime5StructManager::readUnicodeString(input, endPos, res)) {
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::ContentParser::parse: can not read a string\n"));
        f << "##string,";
        return true;
      }
      f << "val=\"" << res.cstr() << "\",";
      break;
    }
    case 8:
    case 9: {
      if (fSz<4) {
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::ContentParser::parse: find bad size for long2\n"));
        f << "##fSz[long2],";
        return true;
      }
      long val=(long) input->readULong(4);
      f << "val=" << (val&0xFFFFFF);
      if (val&0xFF000000) // always 4?
        f << ":" << (val>>24);
      f << ",";
      break;
    }
    default:
      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::ContentParser::parse: find unexpected type\n"));
      f << "##type=" << std::hex << type << std::dec << ",";
      return true;
    }
    for (int i=0; i<3; ++i) {
      if (!hasIndex[i]) continue;
      if (input->tell()+4>endPos) {
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::ContentParser::parse: can not read an index\n"));
        f << "##index,";
        return true;
      }
      f << "f" << i << "=" << input->readLong(4) << ",";
    }
    if (input->tell()!=endPos) {
      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::ContentParser::parse: find extra data\n"));
      f << "##extra,";
    }
    return true;
  }
};

//! Internal: the helper to read a unknown list data
struct Unknown5Parser : public RagTime5StructManager::DataParser {
  //! constructor
  Unknown5Parser(std::string const &zoneName, int fieldSize) : RagTime5StructManager::DataParser(zoneName), m_fieldSize(fieldSize)
  {
  }

  //! try to parse a data
  bool parseData(MWAWInputStreamPtr &input, long endPos, RagTime5Zone &zone, int n, libmwaw::DebugStream &f)
  {
    long pos=input->tell();
    long fSz=endPos-pos;
    if (fSz<4 || (m_fieldSize!=6 && m_fieldSize!=10 && m_fieldSize!=14) || (fSz%m_fieldSize)!=4) {
      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::Unknown5Parser::parse: bad data size\n"));
      return false;
    }
    f << "id=" << input->readLong(2) << ",";
    int N=(int) input->readLong(2);
    f << "N=" << N << ",";
    if (fSz!=4+m_fieldSize*N) {
      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::Unknown5Parser::parse: N seems bad\n"));
      f << "###";
      return true;
    }
    libmwaw::DebugStream f1;
    libmwaw::DebugFile &ascii=zone.ascii();
    for (int i=0; i<N; ++i) {
      pos=input->tell();
      f1.str("");
      f1 << m_name << "-" << n << "-A" << i << ":";
      f1 << "sz=" << m_fieldSize << ",";
      f1 << "id=" << input->readLong(2) << ",";
      int val=(int) input->readLong(2);
      if (val)
        f1 << "col?=" << val << ",";
      f << "f0=" << input->readLong(2) << ","; // small number
      if (m_fieldSize==10) {
        val=(int) input->readLong(2); // 0|4|8|9|c
        if (val)
          f << "f1=" << val << ",";
        val=(int) input->readULong(2);
        if (val)
          f << "fl=" << std::hex << val << std::dec << ",";
      }
      else if (m_fieldSize==14) {
        val=(int) input->readLong(2);
        if (val)
          f1 << "col2?=" << val << ",";
        for (int j=0; j<2; ++j) { // always 0
          val=(int) input->readLong(2);
          f << "f" << j+1 << "=" << val << ",";
        }
        for (int j=0; j<2; ++j) { // f3=0|3, f4=0|2
          val=(int) input->readLong(1);
          f << "f" << j+3 << "=" << val << ",";
        }
      }
      input->seek(pos+m_fieldSize, librevenge::RVNG_SEEK_SET);
      ascii.addPos(pos);
      ascii.addNote(f1.str().c_str());
    }
    return true;
  }
  //! the field size
  int m_fieldSize;
private:
  //! copy constructor, not implemented
  Unknown5Parser(Unknown5Parser &orig);
  //! copy operator, not implemented
  Unknown5Parser &operator=(Unknown5Parser &orig);
};

////////////////////////////////////////
//! Internal: the state of a RagTime5Spreadsheet
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
RagTime5Spreadsheet::RagTime5Spreadsheet(RagTime5Parser &parser) :
  m_mainParser(parser), m_structManager(m_mainParser.getStructManager()), m_parserState(parser.getParserState()),
  m_state(new RagTime5SpreadsheetInternal::State)
{
}

RagTime5Spreadsheet::~RagTime5Spreadsheet()
{ }

int RagTime5Spreadsheet::version() const
{
  return m_parserState->m_version;
}

int RagTime5Spreadsheet::numPages() const
{
  // TODO IMPLEMENT ME
  MWAW_DEBUG_MSG(("RagTime5Spreadsheet::numPages: is not implemented\n"));
  return 0;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////
bool RagTime5Spreadsheet::readUnknownZone1(RagTime5Zone &zone, RagTime5ClusterManager::Link const &link)
{
  MWAWEntry const &entry=zone.m_entry;
  if (!entry.valid() || link.m_fieldSize!=10 || link.m_fieldSize*link.m_N>entry.length()) {
    MWAW_DEBUG_MSG(("RagTime5Spreadsheet::readUnknownZone1: the zone seems bad\n"));
    if (entry.valid()) {
      libmwaw::DebugFile &ascFile=zone.ascii();
      libmwaw::DebugStream f;
      f << "Entries(SheetZone1)[" << zone << "]:###";
      zone.m_isParsed=true;
      ascFile.addPos(entry.begin());
      ascFile.addNote(f.str().c_str());
    }
    return false;
  }
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  f << "Entries(SheetZone1)[" << zone << "]:";
  zone.m_isParsed=true;
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  MWAWInputStreamPtr input=zone.getInput();
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  long endPos=entry.end();

  for (int i=0; i<link.m_N; ++i) {
    long pos=input->tell();
    f.str("");
    f << "SheetZone1-" << i+1 << ":";
    f << "id1=" << input->readLong(4) << ","; // 1-40 maybe col ?
    int fl=(int) input->readULong(2); // [0145][08][0-f][0-f]
    if (fl) f << "fl=" << std::hex << fl << std::dec << ",";
    f << "id2=" << input->readLong(4) << ","; // maybe row ?
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  long pos=input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("RagTime5Spreadsheet::readUnknownZone1: find extra data\n"));
    ascFile.addPos(pos);
    ascFile.addNote("SheetZone1:end###");
  }
  input->setReadInverted(false);
  return true;
}

bool RagTime5Spreadsheet::readUnknownZone2(RagTime5Zone &zone, RagTime5ClusterManager::Link const &link)
{
  MWAWEntry const &entry=zone.m_entry;
  if (!entry.valid() || link.m_fieldSize!=22 || link.m_fieldSize*link.m_N>entry.length()) {
    MWAW_DEBUG_MSG(("RagTime5Spreadsheet::readUnknownZone2: the zone seems bad\n"));
    if (entry.valid()) {
      libmwaw::DebugFile &ascFile=zone.ascii();
      libmwaw::DebugStream f;
      f << "Entries(SheetZone2)[" << zone << "]:###";
      zone.m_isParsed=true;
      ascFile.addPos(entry.begin());
      ascFile.addNote(f.str().c_str());
    }
    return false;
  }
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  f << "Entries(SheetZone2)[" << zone << "]:";
  zone.m_isParsed=true;
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  MWAWInputStreamPtr input=zone.getInput();
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  long endPos=entry.end();

  for (int i=0; i<link.m_N; ++i) {
    long pos=input->tell();
    f.str("");
    f << "SheetZone2-" << i+1 << ":";
    f << "id1=" << input->readLong(4) << ","; // 1-a4 maybe col ?
    f << "num=[";
    for (int j=0; j<6; ++j) { // small number or 32000
      int val=(int) input->readLong(2);
      if (val)
        f << val << ",";
      else
        f << "_,";
    }
    f << "],";
    for (int j=0; j<6; ++j) { // fl0=1|2|2a|2b|42, fl2=0|1, fl3=0-ff, fl4=0|1
      int val=(int) input->readULong(1);
      if (val)
        f << "fl" << j << "=" << std::hex << val << std::dec << ",";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  long pos=input->tell();
  if (pos!=endPos) {
    // extra data seems rare, but possible...
    ascFile.addPos(pos);
    ascFile.addNote("SheetZone2:end");
  }
  input->setReadInverted(false);
  return true;
}

bool RagTime5Spreadsheet::readUnknownZone3(RagTime5Zone &zone, RagTime5ClusterManager::Link const &link)
{
  MWAWEntry const &entry=zone.m_entry;
  if (!entry.valid() || link.m_fieldSize!=24 || link.m_fieldSize*link.m_N>entry.length()) {
    MWAW_DEBUG_MSG(("RagTime5Spreadsheet::readUnknownZone3: the zone seems bad\n"));
    if (entry.valid()) {
      libmwaw::DebugFile &ascFile=zone.ascii();
      libmwaw::DebugStream f;
      f << "Entries(SheetZone3)[" << zone << "]:###";
      zone.m_isParsed=true;
      ascFile.addPos(entry.begin());
      ascFile.addNote(f.str().c_str());
    }
    return false;
  }
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  f << "Entries(SheetZone3)[" << zone << "]:";
  zone.m_isParsed=true;
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  MWAWInputStreamPtr input=zone.getInput();
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  long endPos=entry.end();

  for (int i=0; i<link.m_N; ++i) {
    long pos=input->tell();
    f.str("");
    f << "SheetZone3-" << i+1 << ":";
    f << "id1=" << input->readLong(4) << ","; // 1-a4 maybe col ?
    f << "num=[";
    for (int j=0; j<6; ++j) { // small number or 32000
      int val=(int) input->readLong(2);
      if (val)
        f << val << ",";
      else
        f << "_,";
    }
    f << "],";
    long val=(long) input->readULong(4);
    if (val) {
      f << "f0=" << std::hex << (val&0xFFFFFF) << std::dec;
      if (val&0xFF000000)
        f << ":" << (val>>24); // 0|2
      f << ",";
    }
    for (int j=0; j<2; ++j) { // f1=0|1
      val=input->readLong(2);
      if (val)
        f << "f" << i+1 << "=" << val << ",";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  long pos=input->tell();
  if (pos!=endPos) {
    // no rare
    ascFile.addPos(pos);
    ascFile.addNote("SheetZone3:end");
  }
  input->setReadInverted(false);
  return true;
}

bool RagTime5Spreadsheet::readUnknownZone4(RagTime5Zone &zone, RagTime5ClusterManager::Link const &link)
{
  MWAWEntry const &entry=zone.m_entry;
  if (!entry.valid() || link.m_fieldSize!=8 || link.m_fieldSize*link.m_N>entry.length()) {
    MWAW_DEBUG_MSG(("RagTime5Spreadsheet::readUnknownZone4: the zone seems bad\n"));
    if (entry.valid()) {
      libmwaw::DebugFile &ascFile=zone.ascii();
      libmwaw::DebugStream f;
      f << "Entries(SheetZone4)[" << zone << "]:###";
      zone.m_isParsed=true;
      ascFile.addPos(entry.begin());
      ascFile.addNote(f.str().c_str());
    }
    return false;
  }
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  f << "Entries(SheetZone4)[" << zone << "]:";
  zone.m_isParsed=true;
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  MWAWInputStreamPtr input=zone.getInput();
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  long endPos=entry.end();

  for (int i=0; i<link.m_N; ++i) {
    long pos=input->tell();
    f.str("");
    f << "SheetZone4-" << i+1 << ":";
    for (int j=0; j<2; ++j) {
      long val=(long) input->readULong(4);
      if (!val)
        continue;
      f << "f" << j << "=" << (val&0xFFFFFF);
      if (val&0xFF000000) f << ":" << std::hex << (val>>24) << std::dec; // 0|1|2|3|11
      f << ",";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  long pos=input->tell();
  if (pos!=endPos) { // no frequent, but can happens
    ascFile.addPos(pos);
    ascFile.addNote("SheetZone4:end");
  }
  input->setReadInverted(false);
  return true;
}

////////////////////////////////////////////////////////////
// interface send function
////////////////////////////////////////////////////////////

void RagTime5Spreadsheet::flushExtra()
{
  MWAW_DEBUG_MSG(("RagTime5Spreadsheet::flushExtra: is not implemented\n"));
}

////////////////////////////////////////////////////////////
// cluster parser
////////////////////////////////////////////////////////////

namespace RagTime5SpreadsheetInternal
{

//! low level: the spreadsheet cluster data
struct ClusterSpreadsheet : public RagTime5ClusterManager::Cluster {
  //! constructor
  ClusterSpreadsheet() : RagTime5ClusterManager::Cluster(), m_contentLink(), m_listLink()
  {
  }
  //! destructor
  virtual ~ClusterSpreadsheet() {}
  //! the main content
  RagTime5ClusterManager::Link m_contentLink;
  //! cluster links list of size 10 and 24
  RagTime5ClusterManager::Link m_clusterLink[2];
  //! the other fixed zone of size 10, 16( 1 or 2), 18(can be multiple), 8
  std::vector<RagTime5ClusterManager::Link> m_fixedLink[4];
  //! the other list zone
  std::vector<RagTime5ClusterManager::Link> m_listLink;
};

//
//! low level: parser of main spreadsheet cluster
//
struct SpreadsheetCParser : public RagTime5ClusterManager::ClusterParser {
  //! constructor
  SpreadsheetCParser(RagTime5ClusterManager &parser, int type) :
    ClusterParser(parser, type, "ClustSheet"), m_cluster(new ClusterSpreadsheet), m_what(-1), m_linkId(-1), m_fieldName("")
  {
  }
  //! return the spreadsheet cluster
  shared_ptr<ClusterSpreadsheet> getSpreadsheetCluster()
  {
    return m_cluster;
  }
  //! return the current cluster
  shared_ptr<RagTime5ClusterManager::Cluster> getCluster()
  {
    return m_cluster;
  }
  //! end of a start zone call
  void endZone()
  {
    if (m_link.empty())
      return;
    switch (m_linkId) {
    case 0:
      if (m_cluster->m_nameLink.empty())
        m_cluster->m_nameLink=m_link;
      else {
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::endZone: oops the name link is already set\n"));
        m_cluster->m_linksList.push_back(m_link);
      }
      break;
    case 1:
    case 2:
      if (m_cluster->m_clusterLink[m_linkId-1].empty())
        m_cluster->m_clusterLink[m_linkId-1]=m_link;
      else {
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::endZone: oops the cluster link %d is already set\n", m_linkId));
        m_cluster->m_linksList.push_back(m_link);
      }
      break;
    case 3:
    case 4:
    case 5:
    case 6:
      m_cluster->m_fixedLink[m_linkId-3].push_back(m_link);
      break;
    case 7:
      m_cluster->m_listLink.push_back(m_link);
      break;
    case 8:
      if (m_cluster->m_contentLink.empty())
        m_cluster->m_contentLink=m_link;
      else {
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::endZone: oops the content link is already set\n"));
        m_cluster->m_linksList.push_back(m_link);
      }
      break;
    default:
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
      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseZone: expected N value\n"));
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
    case 1: // list link
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
      // only with long2 list and with unk=[10-15]
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

      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseField: find unexpected list link field\n"));
      f << "###" << field;
      break;
    case 2:
      if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xce842) {
        f << "pos=[";
        for (size_t i=0; i<field.m_longList.size(); ++i)
          f << field.m_longList[i] << ",";
        f << "],";
        m_link.m_longList=field.m_longList;
        break;
      }
      if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x1671845) {
        f << "list=["; // find only 0x3e800001
        for (size_t i=0; i<field.m_fieldList.size(); ++i) {
          RagTime5StructManager::Field const &child=field.m_fieldList[i];
          if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==0xce842) {
            for (size_t j=0; j<child.m_longList.size(); ++j)
              f << std::hex << child.m_longList[j] << std::dec << ",";
            continue;
          }
          MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseField: find unexpected child[fSz=91]\n"));
          f << "##[" << child << "],";
        }
        f << "],";
        break;
      }
      if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0x1671817) {
        f << "unkn=[";
        for (size_t i=0; i<field.m_longList.size(); ++i) // list of 0 and 1
          f << field.m_longList[i] << ",";
        f << "],";
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseField: find unexpected fSz69.. field\n"));
      f << "###" << field;
      break;
    default:
      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseField: find unexpected field\n"));
      f << "###" << field;
      break;
    }
    return true;
  }
protected:
  //! parse a data block
  bool parseDataZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    // seems to begin by 34,[58]^{0|1},69,71,16,
    f << "fl=" << std::hex << flag << std::dec << ",";
    long pos=input->tell();
    //long endPos=pos+fSz-6;
    int val;
    long linkValues[5];
    std::string mess;
    m_link.m_N=N;
    switch (fSz) {
    case 16:
      for (int i=0; i<2; ++i) { // either 0,0 or g1=g0+1
        val=(int) input->readLong(4);
        if (val)
          f << "g" << i << "=" << val << ",";
      }
      val=(int) input->readLong(2); // 4 or 8
      if (val!=4) f << "g2=" << val << ",";
      break;
    case 28:
    case 29:
    case 30:
    case 32:
    case 34:
    case 36: {
      if (!readLinkHeader(input, fSz, m_link, linkValues, mess)) {
        if (fSz==36) {
          input->seek(pos, librevenge::RVNG_SEEK_SET);
          val=(int) input->readLong(4);
          long type=(int) input->readULong(4);
          if (val==0x35800 && type==0x1454857) {
            f << "type=" << std::hex << type << std::dec << ",";
            for (int i=0; i<2; ++i) { // g1=1669817
              val=(int) input->readLong(4);
              if (val) f << "g" << i << "=" << std::hex << val << std::dec << ",";
            }
            val=(int) input->readULong(2);
            if (val)
              f << "fileType1=" << std::hex << val << std::dec << ",";
            // increasing sequence
            for (int i=0; i<3; ++i) { // g0=d, g1=g0+1, g2=g1+1
              val=(int) input->readLong(4);
              if (val) f << "g" << i+2 << "=" << val << ",";
            }
            break;
          }
        }
        f << "###fType=" << std::hex << m_link.m_fileType[0] << std::dec << ",";
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseDataZone: the field fSz28... type seems bad\n"));
        return true;
      }
      long expectedFileType1=0;
      m_what=1;
      if (m_link.m_fileType[0]==0x35800)
        m_fieldName="zone:longs";
      else if (m_link.m_fileType[0]==0x3e800)
        m_fieldName="list:longs0";
      else if (m_link.m_fileType[0]==0x3c052) {
        if (linkValues[0]!=0x1454877) {
          MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseDataZone: find unexpected linkValue[0]\n"));
          f << "#lValues0,";
        }
        // linkValues[2]=5|8|9
        expectedFileType1=0x50;
        m_fieldName="zone:longs2";
      }
      else if (fSz==30 && m_link.m_fieldSize==12) { // find link to checkbox, ...
        expectedFileType1=0xd0;
        m_fieldName="clustLink";
        m_link.m_name="Sheet_ListClust1";
        m_link.m_type=RagTime5ClusterManager::Link::L_ClusterLink;
        m_linkId=1;
      }
      else if (fSz==32 && m_link.m_fileType[0]==0) {
        expectedFileType1=0x200;
        m_fieldName="unicode";
        m_linkId=0;
        m_link.m_type=RagTime5ClusterManager::Link::L_UnicodeList;
      }
      else if (fSz==34 && m_link.m_fileType[0]==0) {
        expectedFileType1=0x10;
        m_fieldName=m_link.m_name="content";
        m_linkId=8;
      }
      else if (fSz==34 && m_link.m_fieldSize==0xa) {
        expectedFileType1=-1;
        m_linkId=3;
        m_fieldName=m_link.m_name="sheetZone1";
      }
      else if (fSz==34 && m_link.m_fieldSize==0x16) {
        expectedFileType1=-1;
        m_linkId=4;
        m_fieldName=m_link.m_name="sheetZone2";
      }
      else if (fSz==34 && m_link.m_fieldSize==0x18) {
        expectedFileType1=-1;
        m_linkId=5;
        m_fieldName=m_link.m_name="sheetZone3";
      }
      else if (fSz==36 && m_link.m_fileType[0]==0) {
        expectedFileType1=0x10;
        m_linkId=2;
        m_link.m_name=m_fieldName="clusterLink2";
      }
      else {
        f << "###fType=" << m_link << ",";
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseDataZone: the field fSz28 type seems bad\n"));
        return true;
      }
      if (expectedFileType1>=0 && (m_link.m_fileType[1]&0xFFD7)!=expectedFileType1) {
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseDataZone: fileType1 seems odd[fSz=28...]\n"));
        f << "###fileType1=" << std::hex << m_link.m_fileType[1] << std::dec << ",";
      }
      m_link.m_fileType[0]=0;
      f << m_link << "," << mess;
      if (fSz==29) {
        val=(int) input->readLong(1);
        if (val!=1) // always 1
          f << "g0=" << val << ",";
        break;
      }
      break;
    }
    case 58: {
      f << "N1=" << N << "n";
      val=(int) input->readLong(2); // always 1
      if (val!=1) f << "g0=" << val << ",";
      m_link.m_N=(int) input->readLong(4); // can be big
      long actPos=input->tell();
      m_linkId=6;
      m_fieldName=m_link.m_name="sheetZone4";
      if (!readLinkHeader(input, fSz, m_link, linkValues, mess)) {
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseDataZone: can not read link for fSz58\n"));
        input->seek(actPos+30, librevenge::RVNG_SEEK_SET);
        f << "###link,";
      }
      else {
        f << m_link << "," << mess;
        m_link.m_fileType[0]=0;
        if ((m_link.m_fileType[1]&0xFFD7)!=0x40) {
          MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseDataZone: fileType1 seems odd[fSz58]\n"));
          f << "###fileType1=" << std::hex << m_link.m_fileType[1] << std::dec << ",";
        }
      }
      val=(int) input->readLong(4); // 9-c
      if (val) f << "N2=" << val << ",";
      f << "num=[";
      for (int i=0; i<7; ++i) {
        val=(int) input->readULong(2);
        if (!val)
          f << "_,";
        else if (val&0x8000)
          f << (val&0x7FFF) << "*,";
        else
          f << val << ",";
      }
      f << "],";
      val=(int) input->readLong(4); // always 1?
      if (val!=1)
        f << "g2=" << val << ",";
      break;
    }
    case 68:
      for (int i=0; i<2; ++i) { // always 0
        val=(int) input->readLong(2);
        if (val)
          f << "f" << i << "=" << val << ",";
      }
      for (int i=0; i<2; ++i) { // f2=1, f3=0|a big number
        val=(int) input->readLong(4);
        if (val)
          f << "f" << i+2 << "=" << val << ",";
      }
      val=(int) input->readULong(4);
      if (val!=0x1646042) {
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseDataZone: fileType0 seems odd[fSz68]\n"));
        f << "###fileType0=" << std::hex << val << std::dec << ",";
      }
      for (int i=0; i<4; ++i) { // always 0
        val=(int) input->readLong(2);
        if (val)
          f << "f" << i+4 << "=" << val << ",";
      }
      f << "num0=[";
      for (int i=0; i<3; ++i) { // small number
        val=(int) input->readLong(2);
        if (val)
          f << val << ",";
        else
          f << "_,";
      }
      f << "],";
      val=(int) input->readULong(4); // always 1
      if (val!=1) f << "f8=" << val << ",";
      for (int i=0; i<2; ++i) { // always 0
        val=(int) input->readLong(2);
        if (val)
          f << "f" << i+9 << "=" << val << ",";
      }
      f << "num1=[";
      for (int i=0; i<10; ++i) { // find X,_,_,X,X,_,_,X,X,_ where X are some small numbers
        val=(int) input->readLong(1);
        if (val)
          f << val << ",";
        else
          f << "_,";
      }
      f << "],";
      f << "num2=[";
      for (int i=0; i<7; ++i) { // first always 0, other some ints
        val=(int) input->readLong(2);
        if (val)
          f << val << ",";
        else
          f << "_,";
      }
      f << "],";
      break;
    case 69:
    case 71: {
      m_what=2;
      if (!readLinkHeader(input, fSz, m_link, linkValues, mess)) {
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseDataZone: can not read link for fSz69...\n"));
        input->seek(pos+26, librevenge::RVNG_SEEK_SET);
        f << "###link,";
      }
      else  {
        if ((m_link.m_fileType[1]&0xFFD7)!=0x8000) {
          MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseDataZone: fileType1 seems odd[fSz69...]\n"));
          f << "###fileType1=" << std::hex << m_link.m_fileType[1] << std::dec << ",";
        }
        f << m_link << ",";
      }
      val=(int) input->readLong(4); // always 1
      if (val!=1)
        f << "g0=" << val << ",";
      m_link.m_fieldSize=(int) input->readLong(2);
      val=(int) input->readULong(2);
      if ((val==0x3e80 && (m_link.m_fieldSize==6 || m_link.m_fieldSize==10)) ||
          (val==0x3e81 && m_link.m_fieldSize==14)) {
        m_linkId=7;
        m_fieldName=m_link.m_name="sheetZone5";
      }
      else {
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseDataZone: fileType2 seems odd[fSz69...]\n"));
        f << "###fileType2=" << std::hex << val << std::dec << ",";
      }
      val=(int) input->readLong(2); // always 1
      if (val!=1)
        f << "g2=" << val << ",";
      val=(int) input->readLong(4); // 1-2
      if (val!=2)
        f << "g3=" << val << ",";
      val=(int) input->readULong(4);
      if (val!=0x34800) {
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseDataZone: fileType3 seems odd[fSz69...]\n"));
        f << "###fileType3=" << std::hex << val << std::dec << ",";
      }
      for (int i=0; i<9; ++i) { // h6=32
        val=(int) input->readLong(2);
        if (val)
          f << "h" << i << "=" << val << ",";
      }
      val=(int) input->readLong(1); // always 1
      if (val!=1)
        f << "h9=" << val << ",";
      if (fSz==69)
        break;
      val=(int) input->readLong(2); // always 0
      if (val)
        f << "h10=" << val << ",";
      break;
    }
    default:
      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseDataZone: find unexpected file size\n"));
      f << "###fSz=" << fSz << ",";
      break;
    }
    if (!m_fieldName.empty())
      f << m_fieldName << ",";
    return true;
  }
  //! parse the header zone
  bool parseHeaderZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    f << "header, fl=" << std::hex << flag << std::dec << ",";
    m_fieldName="header";
    m_what=0;
    if (N!=-5 || m_dataId!=0 || fSz != 134) {
      f << "###N=" << N << ",fSz=" << fSz << ",";
      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseHeaderZone: find unexpected main field\n"));
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
      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseHeaderZone: unexpected zone type\n"));
      f << "##zoneType=" << std::hex << val << std::dec << ",";
    }
    for (int i=0; i<9; ++i) { // f1=0|7-b, f3=0|8-b
      val=(int) input->readULong(2);
      if (val)
        f << "f" << i << "=" << val << ",";
    }
    for (int i=0; i<2; ++i) { // g0=0|1, g1=0|1
      val=(int) input->readULong(1);
      if (val)
        f << "g" << i << "=" << val << ",";
    }
    val=(int) input->readULong(2); // [02][08]0[12c]
    if (val) f << "fl=" << std::hex << val << std::dec << ",";
    for (int i=0; i<12; ++i) { // h1=2, h3=0|3, h5=3|4, h7=h5+1, h9=h7+1, h11=h9+1
      val=(int) input->readULong(2);
      if (val)
        f << "h" << i << "=" << val << ",";
    }
    std::vector<int> listIds;
    if (!RagTime5StructManager::readDataIdList(input, 2, listIds)) {
      f << "##field,";
      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseHeaderZone: can not read the field definitions\n"));
    }
    else if (listIds[0] || listIds[1]) { // fielddef and fieldpos
      RagTime5ClusterManager::Link fieldLink;
      fieldLink.m_type=RagTime5ClusterManager::Link::L_ClusterLink;
      fieldLink.m_ids=listIds;
      m_cluster->m_fieldClusterLink=fieldLink;
      f << "fields," << fieldLink << ",";
    }
    if (!m_fieldName.empty())
      f << m_fieldName << ",";
    return true;
  }

  //! the current cluster
  shared_ptr<ClusterSpreadsheet> m_cluster;
  //! a index to know which field is parsed :  0: main, 1: list, 2: fSz69...
  int m_what;
  //! the link id: 0: unicode, 1: clust link, 2: clust link 2, 3-6: the fixed zones of size 10, 16, 18, 8, 7: the list zone with fieldSize=6|10|14, 8: main content
  int m_linkId;
  //! the actual field name
  std::string m_fieldName;
private:
  //! copy constructor (not implemented)
  SpreadsheetCParser(SpreadsheetCParser const &orig);
  //! copy operator (not implemented)
  SpreadsheetCParser &operator=(SpreadsheetCParser const &orig);
};
}

bool RagTime5Spreadsheet::readSpreadsheetCluster(RagTime5Zone &zone, int zoneType)
{
  shared_ptr<RagTime5ClusterManager> clusterManager=m_mainParser.getClusterManager();
  if (!clusterManager) {
    MWAW_DEBUG_MSG(("RagTime5Spreadsheet::readSpreadsheetCluster: oops can not find the cluster manager\n"));
    return false;
  }
  RagTime5SpreadsheetInternal::SpreadsheetCParser parser(*clusterManager, zoneType);
  if (!clusterManager->readCluster(zone, parser) || !parser.getSpreadsheetCluster()) {
    MWAW_DEBUG_MSG(("RagTime5Spreadsheet::readSpreadsheetCluster: oops can not find the cluster\n"));
    return false;
  }
  shared_ptr<RagTime5SpreadsheetInternal::ClusterSpreadsheet> cluster=parser.getSpreadsheetCluster();
  m_mainParser.checkClusterList(cluster->m_clusterIdsList);

  if (!cluster->m_dataLink.empty()) {
    MWAW_DEBUG_MSG(("RagTime5Spreadsheet::readSpreadsheetCluster: oops do not how to parse the main data\n"));
  }
  if (!cluster->m_contentLink.empty()) {
    RagTime5SpreadsheetInternal::ContentParser contentParser;
    m_mainParser.readListZone(cluster->m_contentLink, contentParser);
  }

  if (!cluster->m_fieldClusterLink.empty())
    m_mainParser.getClusterManager()->readFieldClusters(cluster->m_fieldClusterLink);
  if (!cluster->m_clusterLink[0].m_ids.empty()) {
    shared_ptr<RagTime5Zone> dataZone=m_mainParser.getDataZone(cluster->m_clusterLink[0].m_ids[0]);
    if (!dataZone || !dataZone->m_entry.valid() ||
        dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData") {
      MWAW_DEBUG_MSG(("RagTime5Spreadsheet::readSpreadsheetCluster: the data zone %d seems bad\n",
                      cluster->m_clusterLink[0].m_ids[0]));
    }
    else
      m_mainParser.readClusterLinkList(*dataZone, cluster->m_clusterLink[0]);
  }
  if (!cluster->m_clusterLink[1].empty()) {
    RagTime5SpreadsheetInternal::ClustListParser linkParser(*clusterManager, "Sheet_ListClust2");
    m_mainParser.readListZone(cluster->m_clusterLink[1], linkParser);
    m_mainParser.checkClusterList(linkParser.m_clusterList);
  }
  for (int i=0; i<4; ++i) {
    for (size_t j=0; j<cluster->m_fixedLink[i].size(); ++j) {
      RagTime5ClusterManager::Link link=cluster->m_fixedLink[i][j];
      int cId=link.m_ids.empty() ? 0 : link.m_ids[0];
      shared_ptr<RagTime5Zone> dataZone=m_mainParser.getDataZone(cId);
      if (!dataZone || !dataZone->m_entry.valid() ||
          dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData") {
        if (dataZone && dataZone->getKindLastPart()=="ItemData" && link.m_N==0)
          continue;
        MWAW_DEBUG_MSG(("RagTime5Spreadsheet::readSpreadsheetCluster: the data zone %d seems bad\n", cId));
        continue;
      }
      if (i==0)
        readUnknownZone1(*dataZone, link);
      else if (i==1)
        readUnknownZone2(*dataZone, link);
      else if (i==2)
        readUnknownZone3(*dataZone, link);
      else
        readUnknownZone4(*dataZone, link);
    }
  }
  for (size_t i=0; i<cluster->m_listLink.size(); ++i) {
    RagTime5ClusterManager::Link link=cluster->m_listLink[i];
    RagTime5SpreadsheetInternal::Unknown5Parser listParser("SheetZone5", link.m_fieldSize);
    m_mainParser.readListZone(link, listParser);
  }
  if (!cluster->m_nameLink.empty()) {
    std::map<int, librevenge::RVNGString> idToStringMap;
    m_mainParser.readUnicodeStringList(cluster->m_nameLink, idToStringMap);
  }

  for (size_t i=0; i<cluster->m_linksList.size(); ++i) {
    RagTime5ClusterManager::Link const &lnk=cluster->m_linksList[i];
    if (lnk.m_type==RagTime5ClusterManager::Link::L_List) {
      m_mainParser.readListZone(lnk);
      continue;
    }
    std::stringstream s;
    s << "Data" << lnk.m_fieldSize << "_sheet";
    RagTime5StructManager::DataParser defaultParser(lnk.m_name.empty() ? s.str() : lnk.m_name);
    m_mainParser.readFixedSizeZone(lnk, defaultParser);
  }

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
