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

namespace libmwaw_tools
{
class InputStream;

//! a small class used to find extended attributes ( if possible )
class XAttr
{
public:
  //! constructor
  XAttr(char const *path) : m_fName("")
  {
    if (path) m_fName=path;
  }

  /** return a inputstream corresponding to a attr or 0

  \note if not 0, the caller is responsible of it (ie. must remove it)*/
  InputStream *getStream(const char *attr) const;
protected:
  /** try to find a FINDER.DAT file and returns the fileInformation/ressource fork data
  \note OS9 method to store a file in a FAT disk */
  InputStream *getUsingFinderDat(char const *what) const;
  /** try to look for a possible file containing fileInfo/resource */
  InputStream *getAuxillarInput() const;
  /** try to find a possible fork in a stream */
  InputStream *unMacMIME(InputStream *inp, char const *what) const;

  //! the file name
  std::string m_fName;
};
}
#endif

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
