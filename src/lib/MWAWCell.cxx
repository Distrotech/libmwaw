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

#include <time.h>

#include <iomanip>
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWCell.hxx"

void MWAWCell::addTo(WPXPropertyList &propList) const
{
  propList.insert("libwpd:column", position()[0]);
  propList.insert("libwpd:row", position()[1]);

  propList.insert("table:number-columns-spanned", numSpannedCells()[0]);
  propList.insert("table:number-rows-spanned", numSpannedCells()[1]);

  for (size_t c = 0; c < m_bordersList.size(); c++) {
    switch(c) {
    case libmwaw::Left:
      m_bordersList[c].addTo(propList, "left");
      break;
    case libmwaw::Right:
      m_bordersList[c].addTo(propList, "right");
      break;
    case libmwaw::Top:
      m_bordersList[c].addTo(propList, "top");
      break;
    case libmwaw::Bottom:
      m_bordersList[c].addTo(propList, "bottom");
      break;
    default:
      MWAW_DEBUG_MSG(("MWAWCell::addTo: can not send %d border\n",int(c)));
      break;
    }
  }
  if (!backgroundColor().isWhite())
    propList.insert("fo:background-color", backgroundColor().str().c_str());
  if (isProtected())
    propList.insert("style:cell-protect","protected");
  // alignement
  switch(hAlignement()) {
  case HALIGN_LEFT:
    propList.insert("fo:text-align", "first");
    propList.insert("style:text-align-source", "fix");
    break;
  case HALIGN_CENTER:
    propList.insert("fo:text-align", "center");
    propList.insert("style:text-align-source", "fix");
    break;
  case HALIGN_RIGHT:
    propList.insert("fo:text-align", "end");
    propList.insert("style:text-align-source", "fix");
    break;
  case HALIGN_DEFAULT:
    break; // default
  case HALIGN_FULL:
  default:
    MWAW_DEBUG_MSG(("MWAWCell::addTo: called with unknown halign=%d\n", hAlignement()));
  }
  // no padding
  propList.insert("fo:padding", 0, WPX_POINT);
  // alignement
  switch(vAlignement()) {
  case VALIGN_TOP:
    propList.insert("style:vertical-align", "top");
    break;
  case VALIGN_CENTER:
    propList.insert("style:vertical-align", "middle");
    break;
  case VALIGN_BOTTOM:
    propList.insert("style:vertical-align", "bottom");
    break;
  case VALIGN_DEFAULT:
    break; // default
  default:
    MWAW_DEBUG_MSG(("MWAWCell::addTo: called with unknown valign=%d\n", vAlignement()));
  }
}

std::string MWAWCell::getColumnName(int col)
{
  std::stringstream f;
  f << "[.";
  if (col > 26) f << char('A'+col/26);
  f << char('A'+(col%26));
  f << "]";
  return f.str();
}

std::string MWAWCell::getCellName(Vec2i const &pos, Vec2b const &absolute)
{
  std::stringstream f;
  f << "[.";
  if (absolute[1]) f << "$";
  int col = pos[0];
  if (col > 26) f << char('A'+col/26);
  f << char('A'+(col%26));
  if (absolute[0]) f << "$";
  f << pos[1]+1 << ']';
  return f.str();
}

void MWAWCell::setBorders(int wh, MWAWBorder const &border)
{
  int const allBits = libmwaw::LeftBit|libmwaw::RightBit|libmwaw::TopBit|libmwaw::BottomBit|libmwaw::HMiddleBit|libmwaw::VMiddleBit;
  if (wh & (~allBits)) {
    MWAW_DEBUG_MSG(("MWAWCell::setBorders: unknown borders\n"));
    return;
  }
  size_t numData = 4;
  if (wh & (libmwaw::HMiddleBit|libmwaw::VMiddleBit))
    numData=6;
  if (m_bordersList.size() < numData) {
    MWAWBorder emptyBorder;
    emptyBorder.m_style = MWAWBorder::None;
    m_bordersList.resize(numData, emptyBorder);
  }
  if (wh & libmwaw::LeftBit) m_bordersList[libmwaw::Left] = border;
  if (wh & libmwaw::RightBit) m_bordersList[libmwaw::Right] = border;
  if (wh & libmwaw::TopBit) m_bordersList[libmwaw::Top] = border;
  if (wh & libmwaw::BottomBit) m_bordersList[libmwaw::Bottom] = border;
  if (wh & libmwaw::HMiddleBit) m_bordersList[libmwaw::HMiddle] = border;
  if (wh & libmwaw::VMiddleBit) m_bordersList[libmwaw::VMiddle] = border;
}

std::ostream &operator<<(std::ostream &o, MWAWCell const &cell)
{
  o << MWAWCell::getCellName(cell.m_position, Vec2b(false,false)) << ":";
  if (cell.numSpannedCells()[0] != 1 || cell.numSpannedCells()[1] != 1)
    o << "span=[" << cell.numSpannedCells()[0] << "," << cell.numSpannedCells()[1] << "],";

  if (cell.m_protected) o << "protected,";

  switch(cell.m_hAlign) {
  case MWAWCell::HALIGN_LEFT:
    o << "left,";
    break;
  case MWAWCell::HALIGN_CENTER:
    o << "centered,";
    break;
  case MWAWCell::HALIGN_RIGHT:
    o << "right,";
    break;
  case MWAWCell::HALIGN_FULL:
    o << "full,";
    break;
  case MWAWCell::HALIGN_DEFAULT:
  default:
    break; // default
  }
  switch(cell.m_vAlign) {
  case MWAWCell::VALIGN_TOP:
    o << "top,";
    break;
  case MWAWCell::VALIGN_CENTER:
    o << "centered[y],";
    break;
  case MWAWCell::VALIGN_BOTTOM:
    o << "bottom,";
    break;
  case MWAWCell::VALIGN_DEFAULT:
  default:
    break; // default
  }

  if (!cell.m_backgroundColor.isWhite())
    o << "backColor=" << cell.m_backgroundColor << ",";
  for (size_t i = 0; i < cell.m_bordersList.size(); i++) {
    if (cell.m_bordersList[i].m_style == MWAWBorder::None)
      continue;
    o << "bord";
    if (i < 6) {
      static char const *wh[] = { "L", "R", "T", "B", "MiddleH", "MiddleV" };
      o << wh[i];
    } else o << "[#wh=" << i << "]";
    o << "=" << cell.m_bordersList[i] << ",";
  }
  switch (cell.m_extraLine) {
  case MWAWCell::E_None:
    break;
  case MWAWCell::E_Line1:
    o << "line[TL->RB],";
    break;
  case MWAWCell::E_Line2:
    o << "line[BL->RT],";
    break;
  case MWAWCell::E_Cross:
    o << "line[cross],";
    break;
  default:
    break;
  }
  if (cell.m_extraLine!=MWAWCell::E_None)
    o << cell.m_extraLineType << ",";
  return o;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
