/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * For further information visit http://libwps.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

/*
 * entry for WriteNow
 */
#ifndef WN_MWAW_ENTRY
#  define WN_MWAW_ENTRY

#include <iostream>
#include <map>
#include <string>

#include "libmwaw_tools.hxx"
#include "IMWAWEntry.hxx"

struct WNEntry : public IMWAWEntry {
  WNEntry() : IMWAWEntry(), m_id(-1), m_fileType(-1), m_entryType(-1) {
    for (int i = 0; i < 4; i++) m_val[i] = 0;
  }
  //! returns true if this entry store a zone
  bool isZoneType() const {
    return m_fileType == 4 || m_fileType == 6;
  }
  //! returns true if this is a zone
  bool isZone() const {
    return isZoneType() && valid();
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, WNEntry const &entry) {
    if (entry.type().length()) {
      o << entry.type();
      if (entry.m_id >= 0) o << "[" << entry.m_id << "]";
      o << "=";
    }
    o << "[";
    switch(entry.m_fileType) {
    case 0x4:
      o << "zone,";
      break;
    case 0x6:
      o << "zone2,";
      break;
    case 0xc:
      o << "none/data,";
      break;
    default:
      o << "#type=" << entry.m_fileType << ",";
    }
    switch(entry.m_entryType) {
    case -1:
      break;
    case 0:
      o << "docTbl,";
      break;
    case 1:
      o << "textZone,";
      break;
    case 2:
      o << "text,";
      break;
    case 3:
      o << "graphic,";
      break;
    case 4:
      o << "colorMap,";
      break;
    case 5:
      o << "style,";
      break;
    case 6:
      o << "font,";
      break;
    case 7:
      o << "unknZone1,";
      break;
    case 13:
      o << "graphUnkn1,";
      break;
    case 15:
      o << "printInfo,";
      break;
    default:
      o << "#fileType=" << entry.m_entryType << ",";
    }
    for (int i = 0; i < 4; i++) {
      if (entry.m_val[i]) o << "v" << i << "=" << std::hex << entry.m_val[i] << std::dec << ",";
    }
    o << "],";
    return o;
  }
  //! the entry id
  int m_id;
  //! the file entry id
  int m_fileType;
  //! the entry type (if this is a zone : find in the zone)
  mutable int m_entryType;
  //! other values
  int m_val[4];
};

/** the manager of the entries */
struct WNEntryManager {
  WNEntryManager() : m_posMap(), m_typeMap() {}

  //! return an entry for a position
  WNEntry get(long pos) const {
    std::map<long, WNEntry>::const_iterator it = m_posMap.find(pos);
    if (it == m_posMap.end())
      return WNEntry();
    return it->second;
  }

  //! add a new entry
  bool add(WNEntry const &entry) {
    if (!entry.valid()) return false;
    if (m_posMap.find(entry.begin()) != m_posMap.end()) {
      MWAW_DEBUG_MSG(("WNEntryManager:add: an entry for this position already exists\n"));
      return false;
    }
    std::map<long, WNEntry>::iterator it =
      m_posMap.insert(std::pair<long, WNEntry>(entry.begin(), entry)).first;
    m_typeMap.insert
    (std::multimap<std::string, WNEntry const *>::value_type(entry.type(), &(it->second)));
    return true;
  }

  //! reset the data
  void reset() {
    m_posMap.clear();
    m_typeMap.clear();
  }
  //! the list of entries by position
  std::map<long, WNEntry> m_posMap;
  //! the list of entries
  std::multimap<std::string, WNEntry const *> m_typeMap;
};

#endif
