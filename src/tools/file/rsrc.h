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

#if defined(__MACH__)
#  include <Carbon/Carbon.h>
#else
#  include <Carbon.h>
#endif

#include <ostream>
#include <string>
#include <vector>

namespace libmwaw_tools
{
class InputStream;

/** \brief the main class to read a resource fork ( only the version data )
 */
class RSRC
{
public:
  struct Version;

  //! the constructor
  RSRC(InputStream &input) : m_input(input) { }
  //! the destructor
  ~RSRC() {}

  //! returns a list of version
  std::vector<Version> getVersionList();

protected:
  //! try to parse a version entry
  bool parseVers(Version &vers);

protected:
  //! try to parse the map and return a list of Version (filed id + file pos )
  bool parseMap(long beg, long end, long dataBegin, std::vector<Version> &versionList);

  //! the input stream
  InputStream &m_input;
private:
  RSRC(RSRC const &orig);
  RSRC &operator=(RSRC const &orig);

public:
  /** a public structure used to return the version */
  struct Version {
    Version() : m_id(-1),  m_begin(-1),
      m_majorVersion(-1), m_minorVersion(0), m_countryCode(0),
      m_string(""), m_versionString(""), m_extra("") {
    }
    bool ok() const {
      return m_id >= 0;
    }
    //! operator<<
    friend std::ostream &operator<< (std::ostream &o, Version const &vers);

    static std::string makePretty(std::string const &orig);

    /** the rsrc id */
    int m_id;
    /** the beginning of rsrc in the data file (the end of the entry is too complicated to get, so...) */
    long m_begin;

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

}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
