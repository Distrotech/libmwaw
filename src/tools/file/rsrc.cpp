/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */

/* libmwaw: tools
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

#include <stdint.h>
#include <iostream>
#include <sstream>
#include <string>

#include "file_internal.h"
#include "input.h"

#include "rsrc.h"

namespace libmwaw_tools
{
std::string RSRC::Version::makePretty(std::string const &orig)
{
  size_t j;
  std::string res(orig);
  for (; (j = res.find( 0xd )) != std::string::npos;)
    res.replace( j, 1, " -- " );
  return res;
}

std::ostream &operator<< (std::ostream &o, RSRC::Version const &vers)
{
  if (vers.m_id >= 0) o << "[" << vers.m_id << "]";
  o << ":\t";
  o << RSRC::Version::makePretty(vers.m_string);
  if (vers.m_versionString.length())
    o << "(" << vers.m_versionString << ")";
  o << ",";
  o << "version=" << vers.m_majorVersion;
  if (vers.m_minorVersion)
    o << "(" << vers.m_minorVersion << ")";
  o << ",";
#ifdef DEBUG
  if (vers.m_countryCode)
    o << "country=" << std::hex << vers.m_countryCode << std::dec << ",";
  o << vers.m_extra;
#endif
  return o;
}

bool RSRC::createMapEntries()
{
  if (m_typeMapEntryMap.size()) return true;

  long eof = m_input.length();
  if (eof < 16) return false;
  m_input.seek(0, InputStream::SK_SET);
  m_dataOffset = (long) m_input.readU32();
  long mapBegin = (long) m_input.readU32();
  long dataLength = (long) m_input.readU32();
  long mapLength = (long) m_input.readU32();
  long mapEnd = mapBegin+mapLength;
  if (mapLength < 28 || dataLength < 0 || m_dataOffset+dataLength > eof || mapBegin+mapLength > eof) {
    MWAW_DEBUG_MSG(("RSRC::createMapEntries: can not read the structure size\n"));
    return false;
  }

  m_input.seek(mapBegin+24, InputStream::SK_SET);
  long offsetTypes=(long)m_input.readU16();
  long offsetNameLists=(long)m_input.readU16();
  int numTypes = (int)m_input.readU16();
  if (offsetTypes+2 > mapLength || offsetNameLists > mapLength) {
    MWAW_DEBUG_MSG(("RSRC::createMapEntries: the offsets( limits seem bad\n"));
    return false;
  }
  // this case can appear if no data
  if (numTypes == 0xFFFF)
    numTypes = -1;

  long pos = mapBegin+offsetTypes+2;
  m_input.seek(pos, InputStream::SK_SET);
  if (pos+8*(numTypes+1) > mapEnd) {
    MWAW_DEBUG_MSG(("RSRC::createMapEntries: the type zones seems too short\n"));
    return false;
  }
  for (int i = 0; i <= numTypes; i++) {
    pos = m_input.tell();
    MapEntry entry;
    std::string type("");
    for (int c = 0; c < 4; c++)
      type+=(char)m_input.readU8();
    entry.m_type = type;
    entry.m_numEntry = (int) m_input.readU16()+1; // the number of entry
    entry.m_pos = mapBegin+offsetTypes+(long) m_input.readU16();

    if (entry.m_pos < 0 || entry.m_pos+12*entry.m_numEntry > mapEnd) {
      MWAW_DEBUG_MSG(("RSRC::parseMap: can not read an entry: %s\n", type.c_str()));
      continue;
    }
    m_typeMapEntryMap[type] = entry;
  }

  return true;
}

std::vector<RSRC::MapEntry> RSRC::getMapEntries(std::string type)
{
  std::vector<MapEntry> res;
  if (!createMapEntries() ||
      m_typeMapEntryMap.find(type) ==  m_typeMapEntryMap.end())
    return res;

  long eof = m_input.length();
  MapEntry const &entry = m_typeMapEntryMap.find(type)->second;
  m_input.seek(entry.m_pos, InputStream::SK_SET);
  for (int n = 0; n < entry.m_numEntry; n++) {
    long pos = m_input.tell();
    MapEntry lEntry;
    lEntry.m_type = type;
    lEntry.m_id = (int)m_input.read16();
    long offset = (long) m_input.readU16();
    if (offset != 0xFFFF) {
      // fixme: possible to read the entry name here ( see MWAWRSRCParser )
    }
    unsigned long dOffset = m_input.readU32();
    if (dOffset & 0xFF000000)
      dOffset &= 0xFFFFFF;
    lEntry.m_pos = m_dataOffset+long(dOffset);
    if (lEntry.m_pos >= 0 && lEntry.m_pos+4 < eof)
      res.push_back(lEntry);
    else {
      MWAW_DEBUG_MSG(("RSRC::getMapEntries: find bad pos %lx\n", lEntry.m_pos));
    }
    m_input.seek(pos+12, InputStream::SK_SET);
  }
  return res;
}

bool RSRC::hasEntry(std::string type, int id)
{
  std::vector<MapEntry> lEntries = RSRC::getMapEntries(type);
  for (size_t n = 0; n < lEntries.size(); n++) {
    if (lEntries[n].m_id == id) return true;
  }
  return false;
}

std::string RSRC::getString(int id)
{
  std::string res("");
  std::vector<MapEntry> lEntries = RSRC::getMapEntries("STR ");
  for (size_t n = 0; n < lEntries.size(); n++) {
    MapEntry const &entry = lEntries[n];
    if (entry.m_id != id) continue;
    m_input.seek(entry.m_pos, InputStream::SK_SET);

    long sz = (long) m_input.readU32();
    if (entry.m_pos+4+sz > m_input.length()) {
      MWAW_DEBUG_MSG(("RSRC::getString: entry is invalid\n"));
      return res;
    }
    int fSz = (int) m_input.readU8();
    if (fSz+1 > sz) {
      MWAW_DEBUG_MSG(("RSRC::getString: pascal size seems bad\n"));
      return res;
    }
    for (int i = 0; i < fSz; i++)
      res += (char) m_input.readU8();
    break;
  }

  return res;
}


std::vector<RSRC::Version> RSRC::getVersionList()
{
  std::vector<Version> res;
  std::vector<MapEntry> lEntries = RSRC::getMapEntries("vers");
  for (size_t n = 0; n < lEntries.size(); n++) {
    MapEntry &entry = lEntries[n];
    Version vers;
    vers.m_id = entry.m_id;
    vers.m_begin = entry.m_pos;
    parseVers(vers);
    res.push_back(vers);
  }

  return res;
}

bool RSRC::parseVers(RSRC::Version &vers)
{
  m_input.seek(vers.m_begin, InputStream::SK_SET);
  long sz = (long) m_input.readU32();
  if (vers.m_begin+4+sz > m_input.length() || vers.m_begin <= 0) {
    MWAW_DEBUG_MSG(("RSRC::parseVers: entry is invalid\n"));
    return false;
  }
  long end = vers.m_begin+4+sz;

  vers.m_majorVersion = (int) m_input.readU8();
  vers.m_minorVersion = (int) m_input.readU8();
  std::stringstream f;
  long val = (long) m_input.readU8();
  if (val) f << "devStage=" << val << ",";
  val = (long) m_input.readU8();
  if (val) f << "preReleaseLevel=" << std::hex << val << std::dec << ",";
  vers.m_countryCode = (int) m_input.readU16();
  for (int i = 0; i < 2; i++) {
    sz = (long) m_input.readU8();
    long pos = m_input.tell();
    if (pos+sz > end) {
      MWAW_DEBUG_MSG(("RSRC::parseVers: can not read strings %d\n", i));
      return false;
    }
    std::string str("");
    for (int c = 0; c < sz; c++)
      str+=(char) m_input.readU8();
    if (i==0)
      vers.m_versionString = str;
    else
      vers.m_string = str;
  }
  vers.m_extra = f.str();
  return true;
}

}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
