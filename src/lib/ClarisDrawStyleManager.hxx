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

#ifndef CLARISDRAW_STYLE_MANAGER
#  define CLARISDRAW_STYLE_MANAGER

#include <map>
#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWInputStream.hxx"

class MWAWFont;

namespace ClarisDrawStyleManagerInternal
{
struct State;
}

class ClarisDrawParser;

/** \brief the main class to read a ClarisDraw style
 *
 */
class ClarisDrawStyleManager
{
  friend class ClarisDrawParser;
public:
  //! constructor
  ClarisDrawStyleManager(ClarisDrawParser &parser);
  //! destructor
  virtual ~ClarisDrawStyleManager();

  // Intermediate level

  //! tries to return the color corresponding to an id
  bool getColor(int cId, MWAWColor &color) const;
  //! tries to return the dash definition corresponding to an id
  bool getDash(int dId, std::vector<float> &dash) const;
  //! tries to return the font corresponding to an id
  bool getFont(int fId, MWAWFont &font) const;
  //! tries to return the paragraph corresponding to an id
  bool getParagraph(int pId, MWAWParagraph &para) const;
  //! tries to return the pattern corresponding to an id
  bool getPattern(int pId, MWAWGraphicStyle::Pattern &pattern) const;
  //! tries to update the style gradient
  bool updateGradient(int gId, MWAWGraphicStyle &style) const;

protected:
  //
  // low level
  //

  //! tries to read the font style
  bool readFontStyles();
  //! tries to read the paragraph style
  bool readParagraphStyles();
  //! tries to read the font names zone
  bool readFontNames();

  // tries to read the color list
  bool readColorList();
  // tries to read the gradient list
  bool readPatternList();
  // tries to read the gradient list
  bool readGradientList();

  //! tries to read the Arrow styles
  bool readArrows();
  //! tries to read the dash settings
  bool readDashs();
  //! tries to read the Ruler styles
  bool readRulers();

  //
  // data
  //

protected:
  //! the main parser
  ClarisDrawParser &m_parser;
  //! the parser state
  MWAWParserStatePtr m_parserState;
  //! the state
  shared_ptr<ClarisDrawStyleManagerInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
