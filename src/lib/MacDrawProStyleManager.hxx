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

#ifndef MACDRAWPRO_STYLE_MANAGER
#  define MACDRAWPRO_STYLE_MANAGER

#include <map>
#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWInputStream.hxx"

class MWAWFont;

namespace MacDrawProStyleManagerInternal
{
struct State;
}

class MacDrawProParser;

/** \brief the main class to read a MacDraw II file
 *
 */
class MacDrawProStyleManager
{
  friend class MacDrawProParser;
public:
  //! constructor
  MacDrawProStyleManager(MacDrawProParser &parser);
  //! destructor
  virtual ~MacDrawProStyleManager();

protected:
  //! tries to read the RSRC zones
  bool readRSRCZones();

  // Intermediate level

  //! tries to read the header info part which corresponds to style data
  bool readHeaderInfoStylePart(std::string &extra);
  //! tries to read the style zone knowings the size and the number of data in each zones
  bool readStyles(long const(&sizeZones)[5]);

  //! tries to return the color corresponding to an id
  bool getColor(int cId, MWAWColor &color) const;
  //! tries to return the dash definition corresponding to an id
  bool getDash(int dId, std::vector<float> &dash) const;
  //! tries to return the font corresponding to an id
  bool getFont(int fId, MWAWFont &font) const;
  //! tries to return the pattern corresponding to an id
  bool getPattern(int pId, MWAWGraphicStyle::Pattern &pattern) const;
  //! tries to return the pen size corresponding to an id
  bool getPenSize(int pId, float &penSize) const;

  //
  // low level
  //

  //! tries to read the font style ( last style in data fork )
  bool readFontStyles(MWAWEntry const &entry);

  // data fork or rsrc fork

  //! tries to read the Arrow styles or the resource Aset:256
  bool readArrows(MWAWEntry const &entry, bool inRsrc=false);
  //! tries to read the dash settings or the resource Dset:256
  bool readDashs(MWAWEntry const &entry, bool inRsrc=false);
  //! tries to read the Pen styles or the resource PSet:256
  bool readPens(MWAWEntry const &entry, bool inRsrc=false);
  //! tries to read the Ruler styles or the resource Drul:256
  bool readRulers(MWAWEntry const &entry, bool inRsrc=false);

  // rsrc

  //! tries to read the Document Information resource Dinf:256
  bool readDocumentInfo(MWAWEntry const &entry);
  //! tries to read the main Preferences resource Pref:256
  bool readPreferences(MWAWEntry const &entry);
  //! tries to read the font name resources Fmtx:256 and Fnms:256
  bool readFontNames();
  //! tries to read colors map Ctbl:256
  bool readColors(MWAWEntry const &entry);
  //! tries to read the BW pattern bawP:256
  bool readBWPatterns(MWAWEntry const &entry);
  //! tries to read the color pattern colP:256
  bool readColorPatterns(MWAWEntry const &entry);
  //! tries to read the list of pattern patR:256: list of BW/Color patterns list which appear in the patterns tools
  bool readPatternsToolList(MWAWEntry const &entry);
  //! tries to read the Ruler settings resource Rset:256 or Rst2:256
  bool readRulerSettings(MWAWEntry const &entry);
  //! reads the view positions resource Dvws:256
  bool readViews(MWAWEntry const &entry);

  //! reads the Dstl:256 resource (unknown content)
  bool readRSRCDstl(MWAWEntry const &entry);

  //
  // data
  //

protected:
  //! the main parser
  MacDrawProParser &m_parser;
  //! the parser state
  MWAWParserStatePtr m_parserState;
  //! the state
  shared_ptr<MacDrawProStyleManagerInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
