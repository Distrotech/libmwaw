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

#ifndef MWAW_XATTR_H
#  define MWAW_XATTR_H

#include <string>

namespace libmwaw_zip
{
class InputStream;

//! a small class used to retrieve and encode extended attributes (if possible)
class XAttr
{
public:
  //! constructor
  XAttr(char const *path) : m_fName("")
  {
    if (path) m_fName=path;
  }
  /** return a inputstream corresponding to the MacClassic OS attributes (if possible) */
  shared_ptr<InputStream> getClassicStream() const;
  /** return a inputstream corresponding to the attributes (if possible) */
  shared_ptr<InputStream> getStream() const;
protected:

  //! the file name
  std::string m_fName;
};
}
#endif

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
