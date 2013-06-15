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

#include <libwpd/libwpd.h>

#include "libmwaw_internal.hxx"

#include "MWAWList.hxx"
#include "MWAWPosition.hxx"

#include "MWAWParagraph.hxx"

////////////////////////////////////////////////////////////
// paragraph
////////////////////////////////////////////////////////////
void MWAWTabStop::addTo(WPXPropertyListVector &propList, double decalX) const
{
  WPXPropertyList tab;

  // type
  switch (m_alignment) {
  case RIGHT:
    tab.insert("style:type", "right");
    break;
  case CENTER:
    tab.insert("style:type", "center");
    break;
  case DECIMAL:
    tab.insert("style:type", "char");
    if (m_decimalCharacter) {
      WPXString sDecimal;
      libmwaw::appendUnicode(m_decimalCharacter, sDecimal);
      tab.insert("style:char", sDecimal);
    } else
      tab.insert("style:char", ".");
    break;
  case LEFT:
  case BAR: // BAR is not handled in OO
  default:
    break;
  }

  // leader character
  if (m_leaderCharacter != 0x0000) {
    WPXString sLeader;
    libmwaw::appendUnicode(m_leaderCharacter, sLeader);
    tab.insert("style:leader-text", sLeader);
    tab.insert("style:leader-style", "solid");
  }

  // position
  double position = m_position+decalX;
  if (position < 0.00005f && position > -0.00005f)
    position = 0.0;
  tab.insert("style:position", position);

  propList.append(tab);
}

//! operator<<
std::ostream &operator<<(std::ostream &o, MWAWTabStop const &tab)
{
  o << tab.m_position;

  switch (tab.m_alignment) {
  case MWAWTabStop::LEFT:
    o << "L";
    break;
  case MWAWTabStop::CENTER:
    o << "C";
    break;
  case MWAWTabStop::RIGHT:
    o << "R";
    break;
  case MWAWTabStop::DECIMAL:
    o << ":decimal";
    break;
  case MWAWTabStop::BAR:
    o << ":bar";
    break;
  default:
    o << ":#type=" << int(tab.m_alignment);
    break;
  }
  if (tab.m_leaderCharacter != '\0')
    o << ":sep='"<< (char) tab.m_leaderCharacter << "'";
  if (tab.m_decimalCharacter && tab.m_decimalCharacter != '.')
    o << ":dec='" << (char) tab.m_decimalCharacter << "'";
  return o;
}

////////////////////////////////////////////////////////////
// paragraph
////////////////////////////////////////////////////////////
MWAWParagraph::MWAWParagraph() : m_marginsUnit(WPX_INCH), m_spacingsInterlineUnit(WPX_PERCENT), m_spacingsInterlineType(Fixed),
  m_tabs(), m_tabsRelativeToLeftMargin(false), m_justify(JustificationLeft), m_breakStatus(0),
  m_listLevelIndex(0), m_listId(-1), m_listStartValue(-1), m_listLevel(), m_backgroundColor(MWAWColor::white()),
  m_borders(), m_extra("")
{
  for(int i = 0; i < 3; i++) m_margins[i] = m_spacings[i] = 0.0;
  m_spacings[0] = 1.0; // interline normal
  for(int i = 0; i < 3; i++) {
    m_margins[i].setSet(false);
    m_spacings[i].setSet(false);
  }
}

MWAWParagraph::~MWAWParagraph()
{
}

bool MWAWParagraph::operator!=(MWAWParagraph const &para) const
{
  for(int i = 0; i < 3; i++) {
    if (*(m_margins[i]) < *(para.m_margins[i]) ||
        *(m_margins[i]) > *(para.m_margins[i]) ||
        *(m_spacings[i]) < *(para.m_spacings[i]) ||
        *(m_spacings[i]) > *(para.m_spacings[i]))
      return true;
  }
  if (*m_justify != *para.m_justify || *m_marginsUnit != *para.m_marginsUnit ||
      *m_spacingsInterlineUnit != *para.m_spacingsInterlineUnit ||
      *m_spacingsInterlineType != *para.m_spacingsInterlineType)
    return true;
  if (*m_tabsRelativeToLeftMargin != *para.m_tabsRelativeToLeftMargin)
    return true;
  if (m_tabs->size() != para.m_tabs->size()) return true;
  for (size_t i=0; i < m_tabs->size(); i++) {
    if ((*m_tabs)[i] != (*para.m_tabs)[i])
      return true;
  }
  if (*m_breakStatus != *para.m_breakStatus ||
      *m_listLevelIndex != *para.m_listLevelIndex || *m_listId != *para.m_listId ||
      *m_listStartValue != *para.m_listStartValue || m_listLevel->cmp(*para.m_listLevel) ||
      *m_backgroundColor != *para.m_backgroundColor)
    return true;
  if (m_borders.size() != para.m_borders.size()) return true;
  for (size_t i=0; i < m_borders.size(); i++) {
    if (m_borders[i].isSet() != para.m_borders[i].isSet() ||
        *(m_borders[i]) != *(para.m_borders[i]))
      return true;
  }

  return false;
}

void MWAWParagraph::insert(MWAWParagraph const &para)
{
  for(int i = 0; i < 3; i++) {
    m_margins[i].insert(para.m_margins[i]);
    m_spacings[i].insert(para.m_spacings[i]);
  }
  m_marginsUnit.insert(para.m_marginsUnit);
  m_spacingsInterlineUnit.insert(para.m_spacingsInterlineUnit);
  m_spacingsInterlineType.insert(para.m_spacingsInterlineType);
  m_tabs.insert(para.m_tabs);
  m_tabsRelativeToLeftMargin.insert(para.m_tabsRelativeToLeftMargin);
  m_justify.insert(para.m_justify);
  m_breakStatus.insert(para.m_breakStatus);
  m_listLevelIndex.insert(para.m_listLevelIndex);
  m_listId.insert(para.m_listId);
  m_listStartValue.insert(m_listStartValue);
  m_listLevel.insert(para.m_listLevel);
  m_backgroundColor.insert(para.m_backgroundColor);
  if (m_borders.size() < para.m_borders.size())
    m_borders.resize(para.m_borders.size());
  for (size_t i = 0; i < para.m_borders.size(); i++)
    m_borders[i].insert(para.m_borders[i]);
  m_extra += para.m_extra;
}

bool MWAWParagraph::hasBorders() const
{
  for (size_t i = 0; i < m_borders.size() && i < 4; i++) {
    if (!m_borders[i].isSet())
      continue;
    if (!m_borders[i]->isEmpty())
      return true;
  }
  return false;
}

bool MWAWParagraph::hasDifferentBorders() const
{
  if (!hasBorders()) return false;
  if (m_borders.size() < 4) return true;
  for (size_t i = 1; i < m_borders.size(); i++) {
    if (m_borders[i].isSet() != m_borders[0].isSet())
      return true;
    if (*(m_borders[i]) != *(m_borders[0]))
      return true;
  }
  return false;
}

double MWAWParagraph::getMarginsWidth() const
{
  double factor = (double) MWAWPosition::getScaleFactor(*m_marginsUnit, WPX_INCH);
  return factor*(*(m_margins[1])+*(m_margins[2]));
}

void MWAWParagraph::addTo(WPXPropertyList &propList, bool inTable) const
{
  switch (*m_justify) {
  case JustificationLeft:
    // doesn't require a paragraph prop - it is the default
    propList.insert("fo:text-align", "left");
    break;
  case JustificationCenter:
    propList.insert("fo:text-align", "center");
    break;
  case JustificationRight:
    propList.insert("fo:text-align", "end");
    break;
  case JustificationFull:
    propList.insert("fo:text-align", "justify");
    break;
  case JustificationFullAllLines:
    propList.insert("fo:text-align", "justify");
    propList.insert("fo:text-align-last", "justify");
    break;
  default:
    break;
  }
  if (!inTable) {
    propList.insert("fo:margin-left", *m_margins[1], *m_marginsUnit);
    propList.insert("fo:text-indent", *m_margins[0], *m_marginsUnit);
    propList.insert("fo:margin-right", *m_margins[2], *m_marginsUnit);
    if (!m_backgroundColor->isWhite())
      propList.insert("fo:background-color", m_backgroundColor->str().c_str());
    if (hasBorders()) {
      bool setAll = !hasDifferentBorders();
      for (size_t w = 0; w < m_borders.size() && w < 4; w++) {
        if (w && setAll)
          break;
        if (!m_borders[w].isSet())
          continue;
        MWAWBorder const &border = *(m_borders[w]);
        if (border.isEmpty())
          continue;
        if (setAll) {
          border.addTo(propList);
          break;
        }
        switch(w) {
        case libmwaw::Left:
          border.addTo(propList,"left");
          break;
        case libmwaw::Right:
          border.addTo(propList,"right");
          break;
        case libmwaw::Top:
          border.addTo(propList,"top");
          break;
        case libmwaw::Bottom:
          border.addTo(propList,"bottom");
          break;
        default:
          MWAW_DEBUG_MSG(("MWAWParagraph::addTo: can not send %d border\n",int(w)));
          break;
        }
      }
    }
  }
  propList.insert("fo:margin-top", *(m_spacings[1]));
  propList.insert("fo:margin-bottom", *(m_spacings[2]));
  switch (*m_spacingsInterlineType) {
  case Fixed:
    propList.insert("fo:line-height", *(m_spacings[0]), *m_spacingsInterlineUnit);
    break;
  case AtLeast:
    if (*(m_spacings[0]) <= 0 && *(m_spacings[0]) >= 0)
      break;
    if (*(m_spacings[0]) < 0) {
      static bool first = true;
      if (first) {
        MWAW_DEBUG_MSG(("MWAWParagraph::addTo: interline spacing seems bad\n"));
        first = false;
      }
    } else if (*m_spacingsInterlineUnit != WPX_PERCENT)
      propList.insert("style:line-height-at-least", *(m_spacings[0]), *m_spacingsInterlineUnit);
    else {
      propList.insert("style:line-height-at-least", *(m_spacings[0])*12.0, WPX_POINT);
      static bool first = true;
      if (first) {
        first = false;
        MWAW_DEBUG_MSG(("MWAWParagraph::addTo: assume height=12 to set line spacing at least with percent type\n"));
      }
    }
    break;
  default:
    MWAW_DEBUG_MSG(("MWAWParagraph::addTo: can not set line spacing type: %d\n",int(*m_spacingsInterlineType)));
    break;
  }
  if (*m_breakStatus & NoBreakBit)
    propList.insert("fo:keep-together", "always");
  if (*m_breakStatus & NoBreakWithNextBit)
    propList.insert("fo:keep-with-next", "always");
}

void MWAWParagraph::addTabsTo(WPXPropertyListVector &pList, double decalX) const
{
  if (!*m_tabsRelativeToLeftMargin) {
    // tabs are absolute, we must remove left margin
    double factor = (double) MWAWPosition::getScaleFactor(*m_marginsUnit, WPX_INCH);
    decalX -= m_margins[1].get()*factor;
  }
  for (size_t i=0; i<m_tabs->size(); i++)
    m_tabs.get()[i].addTo(pList, decalX);
}

std::ostream &operator<<(std::ostream &o, MWAWParagraph const &pp)
{
  if (pp.m_margins[0].get()<0||pp.m_margins[0].get()>0)
    o << "textIndent=" << pp.m_margins[0].get() << ",";
  if (pp.m_margins[1].get()<0||pp.m_margins[1].get()>0)
    o << "leftMarg=" << pp.m_margins[1].get() << ",";
  if (pp.m_margins[2].get()<0||pp.m_margins[2].get()>0)
    o << "rightMarg=" << pp.m_margins[2].get() << ",";

  if (pp.m_spacingsInterlineUnit.get()==WPX_PERCENT) {
    if (pp.m_spacings[0].get() < 1.0 || pp.m_spacings[0].get() > 1.0) {
      o << "interLineSpacing=" << pp.m_spacings[0].get() << "%";
      if (pp.m_spacingsInterlineType.get()==MWAWParagraph::AtLeast)
        o << "[atLeast]";
      o << ",";
    }
  } else if (pp.m_spacings[0].get() > 0.0) {
    o << "interLineSpacing=" << pp.m_spacings[0].get();
    if (pp.m_spacingsInterlineType.get()==MWAWParagraph::AtLeast)
      o << "[atLeast]";
    o << ",";
  }
  if (pp.m_spacings[1].get()<0||pp.m_spacings[1].get()>0)
    o << "befSpacing=" << pp.m_spacings[1].get() << ",";
  if (pp.m_spacings[2].get()<0||pp.m_spacings[2].get()>0)
    o << "aftSpacing=" << pp.m_spacings[2].get() << ",";

  if (pp.m_breakStatus.get() & MWAWParagraph::NoBreakBit) o << "dontbreak,";
  if (pp.m_breakStatus.get() & MWAWParagraph::NoBreakWithNextBit) o << "dontbreakafter,";

  switch(pp.m_justify.get()) {
  case MWAWParagraph::JustificationLeft:
    break;
  case MWAWParagraph::JustificationCenter:
    o << "just=centered, ";
    break;
  case MWAWParagraph::JustificationRight:
    o << "just=right, ";
    break;
  case MWAWParagraph::JustificationFull:
    o << "just=full, ";
    break;
  case MWAWParagraph::JustificationFullAllLines:
    o << "just=fullAllLines, ";
    break;
  default:
    o << "just=" << pp.m_justify.get() << ", ";
    break;
  }

  if (pp.m_tabs->size()) {
    o << "tabs=(";
    for (size_t i = 0; i < pp.m_tabs->size(); i++)
      o << pp.m_tabs.get()[i] << ",";
    o << "),";
  }
  if (!pp.m_backgroundColor.get().isWhite())
    o << "backgroundColor=" << pp.m_backgroundColor.get() << ",";
  if (*pp.m_listId >= 0) o << "listId=" << *pp.m_listId << ",";
  if (pp.m_listLevelIndex.get() >= 1)
    o << pp.m_listLevel.get() << ":" << pp.m_listLevelIndex.get() <<",";

  for (size_t i = 0; i < pp.m_borders.size(); i++) {
    if (!pp.m_borders[i].isSet())
      continue;
    MWAWBorder const &border = pp.m_borders[i].get();
    if (border.isEmpty())
      continue;
    o << "bord";
    if (i < 6) {
      static char const *wh[] = { "L", "R", "T", "B", "MiddleH", "MiddleV" };
      o << wh[i];
    } else o << "[#wh=" << i << "]";
    o << "=" << border << ",";
  }

  if (!pp.m_extra.empty()) o << "extras=(" << pp.m_extra << ")";
  return o;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
