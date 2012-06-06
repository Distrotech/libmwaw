/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * For further information visit http://libmwaw.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#ifndef MWAW_PARAGRAPH
#  define MWAW_PARAGRAPH

#include <assert.h>
#include <iostream>

#include <vector>

#include <libwpd/WPXProperty.h>

#include "libmwaw_internal.hxx"
#include "MWAWList.hxx"

class MWAWContentListener;
class WPXPropertyListVector;

struct MWAWTabStop {
  enum Alignment { LEFT, RIGHT, CENTER, DECIMAL, BAR };
  MWAWTabStop(double position = 0.0, Alignment alignment = LEFT, uint16_t leaderCharacter='\0', uint8_t leaderNumSpaces = 0)  :
    m_position(position), m_alignment(alignment), m_leaderCharacter(leaderCharacter), m_leaderNumSpaces(leaderNumSpaces) {
  }
  void addTo(WPXPropertyListVector &propList, double decalX=0.0);
  //! operator <<
  friend std::ostream &operator<<(std::ostream &o, MWAWTabStop const &ft);
  double m_position;
  Alignment m_alignment;
  uint16_t m_leaderCharacter;
  uint8_t m_leaderNumSpaces;
};

//! class to store the paragraph properties
struct MWAWParagraph {
  typedef MWAWList::Level ListLevel;

  //! constructor
  MWAWParagraph() : m_marginsUnit(WPX_INCH), m_spacingsInterlineUnit(WPX_PERCENT),
    m_tabs(), m_justify(libmwaw::JustificationLeft),
    m_breakStatus(0), m_listLevelIndex(0), m_listLevel(),
    m_border(0), m_borderStyle(libmwaw::BorderSingle), m_borderWidth(1), m_borderColor(0), m_extra("") {
    for(int i = 0; i < 3; i++) m_margins[i] = m_spacings[i] = 0.0;
    m_spacings[0] = 1.0; // interline normal
  }
  virtual ~MWAWParagraph();

  //! send data to the listener
  void send(shared_ptr<MWAWContentListener> listener) const;
  //! operator <<
  friend std::ostream &operator<<(std::ostream &o, MWAWParagraph const &ft);

  /** the margins
   *
   * - 0: first line left margin
   * - 1: left margin
   * - 2: right margin*/
  double m_margins[3]; // 0: first line left, 1: left, 2: right
  /** the margins INCH, ... */
  WPXUnit m_marginsUnit;
  /** the line spacing
   *
   * - 0: interline
   * - 1: before
   * - 2: after */
  double m_spacings[3]; // 0: interline, 1: before, 2: after
  /** the interline unit PERCENT or INCH, ... */
  WPXUnit m_spacingsInterlineUnit;
  //! the tabulations
  std::vector<MWAWTabStop> m_tabs;

  /** the justification */
  libmwaw::Justification m_justify;
  /** a list of bits: 0x1 (unbreakable), 0x2 (do not break after) */
  int m_breakStatus; // BITS: 1: unbreakable, 2: dont break after

  /** the actual level index */
  int m_listLevelIndex;
  /** the actual level */
  ListLevel m_listLevel;

  //! list of bits to indicated a border 1: LeftBorderBit, 2: RightBorderBit, ...
  int m_border;
  //! the border style
  libmwaw::BorderStyle m_borderStyle;
  //! the border width
  int m_borderWidth;
  //! the border color
  uint32_t m_borderColor;

  //! a string to store some errors
  std::string m_extra;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
