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

#ifndef MWAW_RSRC_PARSER_H
#define MWAW_RSRC_PARSER_H

#include <map>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"

/** \brief the main class to read a Mac resource fork
 */
class MWAWRSRCParser
{
public:
  struct Version;

  //! the constructor
  MWAWRSRCParser(MWAWInputStreamPtr input);
  //! the destructor
  ~MWAWRSRCParser();

  //! try to parse the document
  bool parse();

  //! return the rsrc input
  MWAWInputStreamPtr getInput() {
    return m_input;
  }

  //! returns a entry corresponding to a type and an id (if possible)
  MWAWEntry getEntry(std::string type, int id) const;

  //! returns the entry map (this map is filled by parse)
  std::multimap<std::string, MWAWEntry> &getEntriesMap() {
    if (!m_parsed)
      parse();
    return m_entryMap;
  }
  //! returns the entry map (this map is filled by parse)
  std::multimap<std::string, MWAWEntry> const &getEntriesMap() const {
    if (!m_parsed)
      const_cast<MWAWRSRCParser *>(this)->parse();
    return m_entryMap;
  }

  //! try to parse a STR entry
  bool parseSTR(MWAWEntry const &entry, std::string &str);

  //! try to parse a STR# entry
  bool parseSTRList(MWAWEntry const &entry, std::vector<std::string> &list);

  //! try to parse a PICT entry
  bool parsePICT(MWAWEntry const &entry, librevenge::RVNGBinaryData &pict);

  //! try to color map (clut entry)
  bool parseClut(MWAWEntry const &entry, std::vector<MWAWColor> &list);

  //! try to parse a version entry
  bool parseVers(MWAWEntry const &entry, Version &vers);

  //! Debugging: change the default ascii file
  void setAsciiName(char const *name) {
    m_asciiName = name;
  }
  //! return the ascii file name
  std::string const &asciiName() const {
    return m_asciiName;
  }
  //! a DebugFile used to write what we recognize when we parse the document
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }
protected:
  //! try to parse the map
  bool parseMap(MWAWEntry const &entry, long dataBegin);

  //! the input stream
  MWAWInputStreamPtr m_input;
  //! the list of entries, name->entry
  std::multimap<std::string, MWAWEntry> m_entryMap;
  //! an internal flag used to know if the parsing was done
  bool m_parsed;
  //! the debug file
  libmwaw::DebugFile m_asciiFile;
  //! the debug file name
  std::string m_asciiName;
private:
  MWAWRSRCParser(MWAWRSRCParser const &orig);
  MWAWRSRCParser &operator=(MWAWRSRCParser const &orig);

public:
  /** a public structure used to return the version */
  struct Version {
    Version() : m_majorVersion(-1), m_minorVersion(0), m_countryCode(0),
      m_string(""), m_versionString(""), m_extra("") {
    }
    //! operator<<
    friend std::ostream &operator<< (std::ostream &o, Version const &vers);
    /** the major number */
    int m_majorVersion;
    /** the minor number */
    int m_minorVersion;
    /** the country code */
    int m_countryCode;
    /** the major string */
    std::string m_string;
    /** the version string */
    std::string m_versionString;
    /** extra data */
    std::string m_extra;
  };

};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
