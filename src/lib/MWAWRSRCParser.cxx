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

#include <libwpd/libwpd.h>

#include "MWAWInputStream.hxx"
#include "MWAWEntry.hxx"

#include "MWAWRSRCParser.hxx"

MWAWRSRCParser::MWAWRSRCParser(MWAWInputStreamPtr input) : m_input(input), m_entryMap(), m_parsed(false), m_asciiFile(), m_asciiName("")
{
}

MWAWRSRCParser::~MWAWRSRCParser()
{
#ifdef DEBUG
  try {
    std::multimap<std::string, MWAWEntry>::iterator it = m_entryMap.begin();
    libmwaw::DebugStream f;
    while (it != m_entryMap.end()) {
      MWAWEntry &tEntry = (it++)->second;
      if (tEntry.isParsed()) continue;
      f.str("");
      f << "Entries(RSRC" << tEntry.type() << "):" << tEntry.id();
      ascii().addPos(tEntry.begin()-4);
      ascii().addNote(f.str().c_str());
    }
  } catch (...) {
  }
#endif

  ascii().reset();
}

MWAWEntry MWAWRSRCParser::getEntry(std::string type, int id) const
{
  if (!m_parsed)
    const_cast<MWAWRSRCParser *>(this)->parse();
  std::multimap<std::string, MWAWEntry>::const_iterator it = m_entryMap.lower_bound(type);
  while (it != m_entryMap.end()) {
    if (it->first != type)
      break;
    MWAWEntry const &tEntry = (it++)->second;
    if (tEntry.id()==id)
      return tEntry;
  }

  return MWAWEntry();
}

bool MWAWRSRCParser::parse()
{
  if (m_parsed)
    return m_entryMap.size() != 0;
  m_parsed = true;
  if (!m_input) return false;

  if (m_asciiName.length()) {
    ascii().setStream(m_input);
    ascii().open("RSRC");
  }
  try {
    libmwaw::DebugStream f;
    m_input->seek(0, WPX_SEEK_SET);
    long pos = m_input->tell();
    MWAWEntry data, map;
    data.setBegin(m_input->readLong(4));
    map.setBegin(m_input->readLong(4));
    data.setLength(m_input->readLong(4));
    map.setLength(m_input->readLong(4));
    // data.length()==0 can be ok, if no data
    if (!map.valid() || (!data.valid()&&data.length()!=0)) {
      MWAW_DEBUG_MSG(("MWAWRSRCParser::parse: can not read the header\n"));
      return false;
    }
    long endPos = data.end() > map.end() ? data.end() : map.end();
    m_input->seek(endPos, WPX_SEEK_SET);
    if (m_input->tell() != endPos) {
      MWAW_DEBUG_MSG(("MWAWRSRCParser::parse: stream seems too small\n"));
      return false;
    }
    f << "Header:";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    ascii().addPos(16);
    ascii().addNote("_");

    if (!parseMap(map, data.begin())) return false;

    std::multimap<std::string, MWAWEntry>::iterator it = m_entryMap.begin();
    while (it != m_entryMap.end()) {
      MWAWEntry &tEntry = (it++)->second;
      if (tEntry.begin()+4 >= data.end()) {
        MWAW_DEBUG_MSG(("MWAWRSRCParser::parseMap: can not read entry %s[%d]\n", tEntry.type().c_str(), tEntry.id()));
        continue;
      }
      m_input->seek(tEntry.begin(), WPX_SEEK_SET);
      tEntry.setBegin(tEntry.begin()+4);
      tEntry.setLength((long)m_input->readULong(4));
    }

    it = m_entryMap.lower_bound("vers");
    while (it != m_entryMap.end()) {
      if (it->first != "vers")
        break;
      MWAWEntry &tEntry = (it++)->second;
      Version vers;
      parseVers(tEntry, vers);
    }
    it = m_entryMap.lower_bound("STR ");
    while (it != m_entryMap.end()) {
      if (it->first != "STR ")
        break;
      std::string str;
      MWAWEntry &tEntry = (it++)->second;
      parseSTR(tEntry, str);
    }
  } catch(...) {
    MWAW_DEBUG_MSG(("MWAWRSRCParser::parse: can not parse the input\n"));
    return false;
  }
  return true;
}

bool MWAWRSRCParser::parseMap(MWAWEntry const &entry, long dataBegin)
{
  if (!m_input) return false;

  if (entry.length() < 28) {
    MWAW_DEBUG_MSG(("MWAWRSRCParser::parseMap: entry map is two short\n"));
    return false;
  }

  libmwaw::DebugStream f;
  m_input->seek(entry.begin()+24, WPX_SEEK_SET);
  f << "Entries(RSRCMap):";
  long offsetTypes=(long)m_input->readULong(2);
  long offsetNameLists=(long)m_input->readULong(2);
  int numTypes = (int)m_input->readULong(2);
  if (offsetTypes+2 > entry.length() || offsetNameLists > entry.length()) {
    MWAW_DEBUG_MSG(("MWAWRSRCParser::parseMap: the offsets seems bad\n"));
    return false;
  }
  // this case can appear if no data
  if (numTypes == 0xFFFF)
    numTypes = -1;
  f << "N=" << numTypes+1;
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  if (m_input->tell() != entry.begin()+offsetTypes+2) {
    ascii().addPos(m_input->tell());
    ascii().addNote("_");
  }

  long pos = entry.begin()+offsetTypes+2;
  m_input->seek(pos, WPX_SEEK_SET);
  if (pos+8*(numTypes+1) > entry.end()) {
    MWAW_DEBUG_MSG(("MWAWRSRCParser::parseMap: the type zones seems too short\n"));
    return false;
  }
  std::vector<MWAWEntry> typesList;
  for (int i = 0; i <= numTypes; i++) {
    pos = m_input->tell();
    f.str("");
    f << "RSRCMap[Type" << i << "]:";
    std::string type("");
    for (int c = 0; c < 4; c++)
      type+=(char)m_input->readULong(1);
    MWAWEntry tEntry;
    tEntry.setType(type);
    tEntry.setId((int) m_input->readULong(2)+1); // the number of entry
    tEntry.setBegin(entry.begin()+offsetTypes+(long) m_input->readULong(2));
    typesList.push_back(tEntry);
    f << tEntry << ":" << std::hex << tEntry.begin() << std::dec << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+8, WPX_SEEK_SET);
  }
  ascii().addPos(m_input->tell());
  ascii().addNote("_");
  for (size_t i = 0; i < typesList.size(); i++) {
    MWAWEntry const &tEntry = typesList[i];
    if (tEntry.begin()+12*tEntry.id() > entry.end()) {
      MWAW_DEBUG_MSG(("MWAWRSRCParser::parseMap: can not read entry %s[%d]\n", tEntry.type().c_str(), tEntry.id()));
      continue;
    }
    m_input->seek(tEntry.begin(), WPX_SEEK_SET);
    for (int n = 0; n < tEntry.id(); n++) {
      pos = m_input->tell();
      f.str("");
      f << "RSRCMap[Rsrc" << n << "]:";
      MWAWEntry rsrc(tEntry);
      rsrc.setId((int)m_input->readLong(2));
      long offset = (long) m_input->readULong(2);
      if (offset != 0xFFFF) // never seems
        f << "listNamesOffset=" << std::hex << offset << std::dec << ",";
      unsigned long dOffset = m_input->readULong(4);
      if (dOffset & 0xFF000000) {
        f << "attributes=" << (dOffset>>12) << ",";
        dOffset &= 0xFFFFFF;
      }
      rsrc.setBegin(dataBegin+long(dOffset));
      m_entryMap.insert(std::multimap<std::string, MWAWEntry>::value_type(rsrc.type(), rsrc));
      f << rsrc.type() << ":" << rsrc.id() << ",";
      f << "pos=" << std::hex << rsrc.begin() << std::dec << ",";

      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      m_input->seek(pos+12, WPX_SEEK_SET);
    }
  }
  if (offsetNameLists != entry.length()) {
    ascii().addPos(entry.begin()+offsetNameLists);
    ascii().addNote("RSRCMap[#nameList]");
  }
  return true;
}

////////////////////////////////////////////////////////////
// read a string resource
// -16396: application-missing name string resource
// -16397: application-missing string resource
////////////////////////////////////////////////////////////
bool MWAWRSRCParser::parseSTR(MWAWEntry const &entry, std::string &str)
{
  str="";
  if (!m_input || !entry.valid()) {
    MWAW_DEBUG_MSG(("MWAWRSRCParser::parseSTR: entry is invalid\n"));
    return false;
  }
  entry.setParsed(true);
  libmwaw::DebugStream f;
  m_input->seek(entry.begin(), WPX_SEEK_SET);
  long sz = (long) m_input->readULong(1);
  if (sz+1 > entry.length()) {
    MWAW_DEBUG_MSG(("MWAWRSRCParser::parseSTR: string length is too small\n"));
    return false;
  }
  for (long i = 0; i < sz; i++) {
    if (m_input->atEOS()) {
      MWAW_DEBUG_MSG(("MWAWRSRCParser::parseSTR: file is too short\n"));
      return false;
    }
    str += (char) m_input->readULong(1);
  }
  f << "Entries(RSRCSTR)[" << entry.id() << "]:" << str;
  if (sz+1 != entry.length()) {
    // can this happens ?
    MWAW_DEBUG_MSG(("MWAWRSRCParser::parseSTR: ARGGS multiple strings\n"));
    ascii().addDelimiter(m_input->tell(),'|');
    f << "###";
  }
  ascii().addPos(entry.begin()-4);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the version:
//     id=1 version of the appliction
//     id=2 version of the file
////////////////////////////////////////////////////////////
std::ostream &operator<< (std::ostream &o, MWAWRSRCParser::Version const &vers)
{
  o << vers.m_string;
  if (vers.m_versionString.length())
    o << "(" << vers.m_versionString << ")";
  o << ",";
  o << "vers=" << vers.m_majorVersion;
  if (vers.m_minorVersion)
    o << "(" << vers.m_minorVersion << ")";
  o << ",";
  if (vers.m_countryCode)
    o << "country=" << std::hex << vers.m_countryCode << std::dec << ",";
  o << vers.m_extra;
  return o;
}

bool MWAWRSRCParser::parseVers(MWAWEntry const &entry, Version &vers)
{
  vers = Version();
  if (!m_input || !entry.valid() || entry.length()<8) {
    MWAW_DEBUG_MSG(("MWAWRSRCParser::parseVers: entry is invalid\n"));
    return false;
  }
  entry.setParsed(true);
  libmwaw::DebugStream f;
  m_input->seek(entry.begin(), WPX_SEEK_SET);
  vers.m_majorVersion = (int) m_input->readULong(1);
  vers.m_minorVersion = (int) m_input->readULong(1);
  long val = (long) m_input->readULong(1);
  if (val) f << "devStage=" << val << ",";
  val = (long) m_input->readULong(1);
  if (val) f << "preReleaseLevel=" << std::hex << val << std::dec << ",";
  vers.m_countryCode = (int) m_input->readULong(2);
  for (int i = 0; i < 2; i++) {
    int sz = (int) m_input->readULong(1);
    long pos = m_input->tell();
    if (pos+sz > entry.end()) {
      MWAW_DEBUG_MSG(("MWAWRSRCParser::parseVers: can not read strings %d\n",i));
      return false;
    }
    std::string str("");
    for (int c = 0; c < sz; c++)
      str+=(char) m_input->readULong(1);
    if (i==0)
      vers.m_versionString = str;
    else
      vers.m_string = str;
  }
  vers.m_extra = f.str();
  f << "Entries(RSRCvers)[" << entry.id() << "]:" << vers;
  ascii().addPos(entry.begin()-4);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read a pict resource
////////////////////////////////////////////////////////////
bool MWAWRSRCParser::parsePICT(MWAWEntry const &entry, WPXBinaryData &pict)
{
  pict.clear();
  if (!m_input || !entry.valid() || entry.length()<0xd) {
    MWAW_DEBUG_MSG(("MWAWRSRCParser::parsePICT: entry is invalid\n"));
    return false;
  }

  libmwaw::DebugStream f;
  f << "Entries(RSRC" << entry.type() << ")[" << entry.id() << "]:";
  m_input->seek(entry.begin(), WPX_SEEK_SET);
  m_input->readDataBlock(entry.length(), pict);

#ifdef DEBUG_WITH_FILES
  if (!entry.isParsed()) {
    ascii().skipZone(entry.begin(), entry.end()-1);
    libmwaw::DebugStream f2;
    f2 << "RSRC-" << entry.type() << "-" << entry.id() << ".pct";
    libmwaw::Debug::dumpFile(pict, f2.str().c_str());
  }
#endif

  ascii().addPos(entry.begin()-4);
  ascii().addNote(f.str().c_str());

  entry.setParsed(true);
  return true;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab: