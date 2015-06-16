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

#ifndef MACDRAFT5_STYLE_MANAGER
#  define MACDRAFT5_STYLE_MANAGER

#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace MacDraft5StyleManagerInternal
{
struct State;
}

class MacDraft5Parser;

/** \brief class to read/store MacDraft5 v4-v5 styles
 *
 */
class MacDraft5StyleManager
{
  friend class MacDraft5Parser;
public:
  //! constructor
  MacDraft5StyleManager(MacDraft5Parser &parser);
  //! destructor
  virtual ~MacDraft5StyleManager();
protected:

  //! try to read the resource block: either the resource fork(v4) or last file's part (v5)
  bool readResources();
  //! try to read the bitmap zones: end file(v4) or the zone before the rsrc part (v5)
  bool readBitmapZones();
  //! returns the end of data position (before the bitmap zones) if known or -1
  long getEndDataPosition() const;
  //! tries to return the color corresponding to an id
  bool getColor(int cId, MWAWColor &color) const;

  //
  // read resource
  //

  //! try to get a pixpat pattern
  bool getPixmap(int pId, librevenge::RVNGBinaryData &data, std::string &type, MWAWVec2i &pictSize, MWAWColor &avColor) const;
  //! try to update the pattern list
  void updatePatterns();

  //! try to read a resource
  bool readResource(MWAWEntry &entry, bool inRsrc);

  //! try to a bitmap
  bool readBitmap(MWAWEntry const &entry);
  //! try to read a list of colors : pltt 128
  bool readColors(MWAWEntry const &entry, bool inRsrc);
  //! try to read a list of dashs : DASH 128
  bool readDashes(MWAWEntry const &entry, bool inRsrc);
  //! try to read FNUS:1 resource
  bool readFonts(MWAWEntry const &entry, bool inRsrc);
  //! try to read a list of patterns/gradient? : PLDT 128
  bool readPatterns(MWAWEntry const &entry, bool inRsrc);
  //! try to read a ppat resource
  bool readPixPat(MWAWEntry const &entry, bool inRsrc);
  //! try to read a version (in data fork)
  bool readVersion(MWAWEntry &entry);

  //! try to read a resource list: PATL:128 or Opac:128+xxx
  bool readRSRCList(MWAWEntry const &entry, bool inRsrc);

  //! try to read BITList:0 resource
  bool readBitmapList(MWAWEntry const &entry, bool inRsrc);
  //! try to read Opcd:131 resource (unknown)
  bool readOpcd(MWAWEntry const &entry, bool inRsrc);

  //
  // data
  //

protected:
  //! the main parser
  MacDraft5Parser &m_parser;
  //! the parser state
  MWAWParserStatePtr m_parserState;
  //! the state
  shared_ptr<MacDraft5StyleManagerInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
