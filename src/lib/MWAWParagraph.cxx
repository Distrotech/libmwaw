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

#include "MWAWContentListener.hxx"
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
    tab.insert("style:char", "."); // Assume a decimal point for now
    break;
  case LEFT:
  case BAR: // BAR is not handled in OO
  default:
    break;
  }

  // leader character
  if (m_leaderCharacter != 0x0000) {
    WPXString sLeader;
    sLeader.sprintf("%c", m_leaderCharacter);
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
  return o;
}

////////////////////////////////////////////////////////////
// paragraph
////////////////////////////////////////////////////////////
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
  if ((*m_justify) != *(para.m_justify) ||
      (*m_marginsUnit) != *(para.m_marginsUnit) ||
      (*m_spacingsInterlineUnit) != *(para.m_spacingsInterlineUnit) ||
      (*m_spacingsInterlineType) != *(para.m_spacingsInterlineType))
    return true;
  if (m_tabs->size() != para.m_tabs->size()) return true;
  for (size_t i=0; i < m_tabs->size(); i++) {
    if ((*m_tabs)[i] != (*para.m_tabs)[i])
      return true;
  }
  if ((*m_breakStatus) != *(para.m_breakStatus) ||
      *(m_listLevelIndex) != *(para.m_listLevelIndex) ||
      m_listLevel->cmp(*(para.m_listLevel)) ||
      *(m_backgroundColor) != *(para.m_backgroundColor))
    return true;
  if (m_borders.size() != para.m_borders.size()) return true;
  for (size_t i=0; i < m_borders.size(); i++) {
    if (m_borders[i].isSet() != para.m_borders[i].isSet() ||
        *(m_borders[i]) != *(para.m_borders[i]))
      return true;
  }

  return false;
}

bool MWAWParagraph::hasBorders() const
{
  for (size_t i = 0; i < m_borders.size() && i < 4; i++) {
    if (m_borders[i]->m_style != MWAWBorder::None)
      return true;
  }
  return false;
}

bool MWAWParagraph::hasDifferentBorders() const
{
  if (!hasBorders()) return false;
  if (m_borders.size() < 4) return true;
  for (size_t i = 1; i < m_borders.size(); i++) {
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
  if (pp.m_listLevelIndex.get() >= 1)
    o << pp.m_listLevel.get() << ":" << pp.m_listLevelIndex.get() <<",";

  for (size_t i = 0; i < pp.m_borders.size(); i++) {
    if (!pp.m_borders[i].isSet() || pp.m_borders[i]->m_style == MWAWBorder::None)
      continue;
    MWAWBorder const &border = pp.m_borders[i].get();
    o << "bord";
    char const *wh[] = { "L", "R", "T", "B", "MiddleH", "MiddleV" };
    if (i < 6) o << wh[i];
    else o << "[#wh=" << i << "]";
    o << "=" << border << ",";
  }

  if (!pp.m_extra.empty()) o << "extras=(" << pp.m_extra << ")";
  return o;
}

void MWAWParagraph::send(shared_ptr<MWAWContentListener> listener) const
{
  if (!listener)
    return;
  listener->setParagraph(*this);

  MWAWList::Level level;
  if (m_listLevelIndex.get() >= 1) {
    double leftMargin = m_margins[1].get();
    float factorToLevel = MWAWPosition::getScaleFactor(m_marginsUnit.get(), WPX_INCH);
    level = m_listLevel.get();
    level.m_labelWidth = (factorToLevel*leftMargin-level.m_labelIndent);
    if (level.m_labelWidth<0.1)
      level.m_labelWidth = 0.1;
    leftMargin=level.m_labelIndent/factorToLevel;
    listener->setParagraphMargin(leftMargin, MWAW_LEFT, m_marginsUnit.get());
    level.m_labelIndent = 0;
    shared_ptr<MWAWList> list=listener->getCurrentList();
    if (!list) {
      list.reset(new MWAWList);
      list->set(m_listLevelIndex.get(), level);
      listener->setCurrentList(list);
    } else
      list->set(m_listLevelIndex.get(), level);
  }
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
