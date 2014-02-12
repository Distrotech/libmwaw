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

#ifndef MS_WKS_ZONE
#  define MS_WKS_ZONE

#include <string>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"

/** \brief a zone in a Microsoft Works file
 *
 *
 *
 */
class MsWksZone
{
public:
  //! constructor using the parser
  MsWksZone(MWAWInputStreamPtr input, MWAWParser &parser);
  //! destructor
  virtual ~MsWksZone();

  //! returns the actual input
  MWAWInputStreamPtr &getInput()
  {
    return m_input;
  }

  //! return the color which correspond to an index
  bool getColor(int id, MWAWColor &col, int vers) const;
  //! return a list of color corresponding to a version
  static std::vector<MWAWColor> const &getPalette(int vers);

  //! inits the ascii file
  void initAsciiFile(std::string const &name);
  //! a DebugFile used to write what we recognize when we parse the document
  libmwaw::DebugFile &ascii()
  {
    return m_asciiFile;
  }
protected:
  //! a pointer to the parser
  MWAWParser *m_parser;
  //! the input which can be an OLE in MSWorks 4 file
  MWAWInputStreamPtr m_input;
  //! the debug file of the actual input
  libmwaw::DebugFile m_asciiFile;

private:
  MsWksZone(MsWksZone const &orig);
  MsWksZone operator=(MsWksZone const &orig);
};

#endif