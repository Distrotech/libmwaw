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
 * Class used to read main style by Claris Works parser
 *
 */
#ifndef CW_STYLE_MANAGER
#  define CW_STYLE_MANAGER

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

class MWAWEntry;
class MWAWParserState;
typedef shared_ptr<MWAWParserState> MWAWParserStatePtr;

class CWParser;

namespace CWStyleManagerInternal
{
struct State;
}

//! a structure to store the style list and the lookup zone
class CWStyleManager
{
public:
  struct Graphic;
  struct KSEN;
  struct Style;
public:
  //! constructor
  CWStyleManager(CWParser &mainParser);
  //! destructor
  ~CWStyleManager();

  /* try to read the styles definition (in v4-6) */
  bool readStyles(MWAWEntry const &entry);

  //! return a mac font id corresponding to a local id
  int getFontId(int localId) const;

  //! return the style corresponding to a styleId
  bool get(int styleId, Style &style) const;
  //! return the ksen style corresponding to a ksenId
  bool get(int ksenId, KSEN &ksen) const;
  //! return the graphic style corresponding to a graphicId
  bool get(int graphId, Graphic &graph) const;

protected:
  //! return the file version
  int version() const;

  /** try to read a STYL_ subzone (in v4-6) */
  bool readGenStyle(int id);

  //! try to read the style definition zone
  bool readStylesDef(int N, int fSz);
  //! try to read the lookup zone
  bool readLookUp(int N, int fSz);

  /* read the STYL CELL sequence */
  bool readCellStyles(int N, int fSz);
  /** read the font name style zone */
  bool readFontNames(int N, int fSz);
  /** read a Graphic sequence */
  bool readGraphStyles(int N, int fSz);
  //! read a KSEN sequence
  bool readKSEN(int N, int fSz);
  /** read a STYL Name sequence */
  bool readStyleNames(int N, int fSz);

protected:
  //! the parser state
  MWAWParserStatePtr m_parserState;
  //! the main parser
  CWParser *m_mainParser;
  //! the state
  shared_ptr<CWStyleManagerInternal::State> m_state;

private:
  CWStyleManager(CWStyleManager const &orig);
  CWStyleManager &operator=(CWStyleManager const &orig);

public:
  //! the Graphic structure in a CWStyleManager
  struct Graphic {
    //! constructor
    Graphic() : m_lineWidth(1), m_extra("") {
      for (int i = 0; i < 2; i++) {
        m_pattern[i] = -1;
        m_patternPercent[i] = 1;
      }
      m_color[0] = MWAWColor::black();
      m_color[1] = MWAWColor::white();
    }
    //! returns the line color
    MWAWColor getLineColor() const;
    //! returns the surface color
    MWAWColor getSurfaceColor() const;
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Graphic const &graph);
    //! the line width
    int m_lineWidth;
    //! the line and surface color
    MWAWColor m_color[2];
    //! the line an surface pattern id
    int m_pattern[2];
    //! the line an surface pattern percent
    float m_patternPercent[2];
    //! extra data
    std::string m_extra;
  };

  //! the KSEN structure a structure related to paragraph and cell style
  struct KSEN {
    //! constructor
    KSEN() : m_valign(0), m_lineType(MWAWBorder::Simple), m_lineRepeat(MWAWBorder::Single), m_lines(0), m_extra("") {
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, KSEN const &ksen);
    //! the vertical alignement
    int m_valign;
    //! the line type
    MWAWBorder::Style m_lineType;
    //! the line repetition
    MWAWBorder::Type m_lineRepeat;
    //! an int used to add some oblique line ( or cross )
    int m_lines;
    //! extra data
    std::string m_extra;
  };

  //! the structure to store the style in a CWStyleManager
  struct Style {
    //! constructor
    Style() : m_fontId(-1), m_fontHash(-1), m_rulerId(-1), m_rulerHash(-1), m_ksenId(-1), m_graphicId(-1), m_localStyleId(-1), m_styleId(-1), m_extra("") {
    }

    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Style const &style);

    //! the char
    int m_fontId;
    //! the fontHash id
    int m_fontHash;
    //! the ruler
    int m_rulerId;
    //! the rulerHash id
    int m_rulerHash;
    //! the ksen id
    int m_ksenId;
    //! the graphic (checkme)
    int m_graphicId;
    //! a local style id
    int m_localStyleId;
    //! the style id
    int m_styleId;
    //! extra data
    std::string m_extra;
  };
};

#endif
