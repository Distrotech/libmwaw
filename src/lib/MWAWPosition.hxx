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

#ifndef MWAW_POSITION_H
#define MWAW_POSITION_H

#include <ostream>

#include <libwpd/libwpd.h>

#include "libmwaw_internal.hxx"

/** Class to define the position of an object (textbox, picture, ..) in the document
 *
 * Stores the page, object position, object size, anchor, wrapping, ...
 */
class MWAWPosition
{
public:
  //! a list of enum used to defined the anchor
  enum AnchorTo { Char, CharBaseLine, Frame, Paragraph, Page, Unknown };
  //! an enum used to define the wrapping
  enum Wrapping { WNone, WBackground, WDynamic, WRunThrough };
  //! an enum used to define the relative X position
  enum XPos { XRight, XLeft, XCenter, XFull };
  //! an enum used to define the relative Y position
  enum YPos { YTop, YBottom, YCenter, YFull };

public:
  //! constructor
  MWAWPosition(Vec2f const &orig=Vec2f(), Vec2f const &sz=Vec2f(), WPXUnit theUnit=WPX_INCH):
    m_anchorTo(Unknown), m_xPos(XLeft), m_yPos(YTop), m_wrapping(WNone),
    m_page(0), m_orig(orig), m_size(sz), m_naturalSize(), m_LTClip(), m_RBClip(), m_unit(theUnit), m_order(0) {}

  virtual ~MWAWPosition() {}
  //! operator<<
  friend  std::ostream &operator<<(std::ostream &o, MWAWPosition const &pos) {
    Vec2f dest(pos.m_orig+pos.m_size);
    o << "Pos=(" << pos.m_orig << ")x(" << dest << ")";
    switch(pos.m_unit) {
    case WPX_INCH:
      o << "(inch)";
      break;
    case WPX_POINT:
      o << "(pt)";
      break;
    case WPX_TWIP:
      o << "(tw)";
      break;
    case WPX_PERCENT:
    case WPX_GENERIC:
    default:
      break;
    }
    if (pos.page()>0) o << ", page=" << pos.page();
    return o;
  }
  //! basic operator==
  bool operator==(MWAWPosition const &f) const {
    return cmp(f) == 0;
  }
  //! basic operator!=
  bool operator!=(MWAWPosition const &f) const {
    return cmp(f) != 0;
  }
  //! basic operator<
  bool operator<(MWAWPosition const &f) const {
    return cmp(f) < 0;
  }

  //! returns the frame page
  int page() const {
    return m_page;
  }
  //! return the frame origin
  Vec2f const &origin() const {
    return m_orig;
  }
  //! returns the frame size
  Vec2f const &size() const {
    return m_size;
  }
  //! returns the natural size (if known)
  Vec2f const &naturalSize() const {
    return m_naturalSize;
  }
  //! returns the left top clipping
  Vec2f const &leftTopClipping() const {
    return m_LTClip;
  }
  //! returns the right bottom clipping
  Vec2f const &rightBottomClipping() const {
    return m_RBClip;
  }
  //! returns the unit
  WPXUnit unit() const {
    return m_unit;
  }
  static float getScaleFactor(WPXUnit orig, WPXUnit dest) {
    float actSc = 1.0, newSc = 1.0;
    switch(orig) {
    case WPX_TWIP:
      break;
    case WPX_POINT:
      actSc=20;
      break;
    case WPX_INCH:
      actSc = 1440;
      break;
    case WPX_PERCENT:
    case WPX_GENERIC:
    default:
      MWAW_DEBUG_MSG(("MWAWPosition::getScaleFactor %d unit must not appear\n", int(orig)));
    }
    switch(dest) {
    case WPX_TWIP:
      break;
    case WPX_POINT:
      newSc=20;
      break;
    case WPX_INCH:
      newSc = 1440;
      break;
    case WPX_PERCENT:
    case WPX_GENERIC:
    default:
      MWAW_DEBUG_MSG(("MWAWPosition::getScaleFactor %d unit must not appear\n", int(dest)));
    }
    return actSc/newSc;
  }
  //! returns a float which can be used to scale some data in object unit
  float getInvUnitScale(WPXUnit fromUnit) const {
    return getScaleFactor(fromUnit, m_unit);
  }

  //! sets the page
  void setPage(int pg) const {
    const_cast<MWAWPosition *>(this)->m_page = pg;
  }
  //! sets the frame origin
  void setOrigin(Vec2f const &orig) {
    m_orig = orig;
  }
  //! sets the frame size
  void setSize(Vec2f const &sz) {
    m_size = sz;
  }
  //! sets the natural size (if known)
  void setNaturalSize(Vec2f const &naturalSz) {
    m_naturalSize = naturalSz;
  }
  //! sets the dimension unit
  void setUnit(WPXUnit newUnit) {
    m_unit = newUnit;
  }
  //! sets/resets the page and the origin
  void setPagePos(int pg, Vec2f const &newOrig) const {
    const_cast<MWAWPosition *>(this)->m_page = pg;
    const_cast<MWAWPosition *>(this)->m_orig = newOrig;
  }

  //! sets the relative position
  void setRelativePosition(AnchorTo anchor, XPos X = XLeft, YPos Y=YTop) {
    m_anchorTo = anchor;
    m_xPos = X;
    m_yPos = Y;
  }

  //! sets the clipping position
  void setClippingPosition(Vec2f lTop, Vec2f rBottom) {
    m_LTClip = lTop;
    m_RBClip = rBottom;
  }

  //! returns background/foward order
  int order() const {
    return m_order;
  }
  //! set background/foward order
  void setOrder(int ord) const {
    m_order = ord;
  }

  //! anchor position
  AnchorTo m_anchorTo;
  //! X relative position
  XPos m_xPos;
  //! Y relative position
  YPos m_yPos;
  //! Wrapping
  Wrapping m_wrapping;

protected:
  //! basic function to compare two positions
  int cmp(MWAWPosition const &f) const {
    int diff = int(m_anchorTo) - int(f.m_anchorTo);
    if (diff) return diff < 0 ? -1 : 1;
    diff = int(m_xPos) - int(f.m_xPos);
    if (diff) return diff < 0 ? -1 : 1;
    diff = int(m_yPos) - int(f.m_yPos);
    if (diff) return diff < 0 ? -1 : 1;
    diff = page() - f.page();
    if (diff) return diff < 0 ? -1 : 1;
    diff = int(m_unit) - int(f.m_unit);
    if (diff) return diff < 0 ? -1 : 1;
    diff = m_orig.cmpY(f.m_orig);
    if (diff) return diff;
    diff = m_size.cmpY(f.m_size);
    if (diff) return diff;
    diff = m_naturalSize.cmpY(f.m_naturalSize);
    if (diff) return diff;
    diff = m_LTClip.cmpY(f.m_LTClip);
    if (diff) return diff;
    diff = m_RBClip.cmpY(f.m_RBClip);
    if (diff) return diff;

    return 0;
  }

  //! the page
  int m_page;
  Vec2f m_orig /** the origin position in a page */, m_size /* the size of the data*/, m_naturalSize /** the natural size of the data (if known) */;
  Vec2f m_LTClip /** the left top clip position */, m_RBClip /* the right bottom clip position */;
  //! the unit used in \a orig, in \a m_size and in \a m_LTClip , .... Default: in inches
  WPXUnit m_unit;
  //! background/foward order
  mutable int m_order;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
