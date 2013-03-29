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

#ifndef MWAW_OLE_H
#  define MWAW_OLE_H
#include <ostream>
#include <map>
#include <string>
#include <vector>

namespace libmwaw_tools
{
class InputStream;

/** \brief the main class to read a OLE file ( only the main dir entry to retrieve the clsid type )
 */
class OLE
{
public:
  //! the constructor
  OLE(InputStream &input) : m_input(input) { }
  //! the destructor
  ~OLE() {}

  //! returns a string correspond to the main clsid or ""
  std::string getCLSIDType();

protected:
  //! read a short in the input file
  unsigned short readU16();
  //! read a int in the input file
  unsigned int readU32();
  //! the input stream
  InputStream &m_input;

private:
  OLE(OLE const &orig);
  OLE &operator=(OLE const &orig);
};

}
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
