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

/*
 * entry for WriteNow
 */
#ifndef WN_MWAW_ENTRY
#  define WN_MWAW_ENTRY

#include <iostream>
#include <map>
#include <string>

#include "libmwaw_internal.hxx"
#include "MWAWEntry.hxx"

struct WNEntry : public MWAWEntry {
  WNEntry() : MWAWEntry(), m_fileType(-1)
  {
    for (int i = 0; i < 4; i++) m_val[i] = 0;
  }
  //! returns true if this entry store a zone
  bool isZoneType() const
  {
    return m_fileType == 4 || m_fileType == 6;
  }
  //! returns true if this is a zone
  bool isZone() const
  {
    return isZoneType() && valid();
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, WNEntry const &entry)
  {
    if (entry.type().length()) {
      o << entry.type();
      if (entry.id() >= 0) o << "[" << entry.id() << "]";
      o << "=";
    }
    o << "[";
    switch (entry.m_fileType) {
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
    for (int i = 0; i < 4; i++) {
      if (entry.m_val[i]) o << "v" << i << "=" << std::hex << entry.m_val[i] << std::dec << ",";
    }
    o << "],";
    return o;
  }
  //! the file entry id
  int m_fileType;
  //! other values
  int m_val[4];
};

/** the manager of the entries */
struct WNEntryManager {
  WNEntryManager() : m_posMap(), m_typeMap() {}

  //! return an entry for a position
  WNEntry get(long pos) const
  {
    std::map<long, WNEntry>::const_iterator it = m_posMap.find(pos);
    if (it == m_posMap.end())
      return WNEntry();
    return it->second;
  }

  //! add a new entry
  bool add(WNEntry const &entry)
  {
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
  void reset()
  {
    m_posMap.clear();
    m_typeMap.clear();
  }
  //! the list of entries by position
  std::map<long, WNEntry> m_posMap;
  //! the list of entries
  std::multimap<std::string, WNEntry const *> m_typeMap;
};

#endif
