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

#include <libwpd/WPXPropertyListVector.h>

#include "libmwaw_internal.hxx"

#include "MWAWContentListener.hxx"
#include "MWAWList.hxx"
#include "MWAWPosition.hxx"

#include "MWAWParagraph.hxx"

void MWAWTabStop::addTo(WPXPropertyListVector &propList, double decalX)
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

MWAWParagraph::~MWAWParagraph()
{
}
//! operator<<
std::ostream &operator<<(std::ostream &o, MWAWParagraph const &pp)
{
  if (pp.m_margins[0].get()<0||pp.m_margins[0].get()>0)
    o << "textIndent=" << pp.m_margins[0].get() << ",";
  if (pp.m_margins[1].get()<0||pp.m_margins[1].get()>0)
    o << "leftMarg=" << pp.m_margins[1].get() << ",";
  if (pp.m_margins[2].get()<0||pp.m_margins[2].get()>0)
    o << "rightMarg=" << pp.m_margins[2].get() << ",";

  if (pp.m_spacingsInterlineUnit.get()==WPX_PERCENT) {
    if (pp.m_spacings[0].get() < 1.0 || pp.m_spacings[0].get() > 1.0)
      o << "interLineSpacing=" << pp.m_spacings[0].get() << "%,";
  } else if (pp.m_spacings[0].get() > 0.0) {
    o << "interLineSpacing=" << pp.m_spacings[0].get();
    if (pp.m_spacingsInterlineType.get()==libmwaw::AtLeast)
      o << "[atLeast]";
    o << ",";
  }
  if (pp.m_spacings[1].get()<0||pp.m_spacings[1].get()>0)
    o << "befSpacing=" << pp.m_spacings[1].get() << ",";
  if (pp.m_spacings[2].get()<0||pp.m_spacings[2].get()>0)
    o << "aftSpacing=" << pp.m_spacings[2].get() << ",";

  if (pp.m_breakStatus.get() & libmwaw::NoBreakBit) o << "dontbreak,";
  if (pp.m_breakStatus.get() & libmwaw::NoBreakWithNextBit) o << "dontbreakafter,";

  switch(pp.m_justify.get()) {
  case libmwaw::JustificationLeft:
    break;
  case libmwaw::JustificationCenter:
    o << "just=centered, ";
    break;
  case libmwaw::JustificationRight:
    o << "just=right, ";
    break;
  case libmwaw::JustificationFull:
    o << "just=full, ";
    break;
  case libmwaw::JustificationFullAllLines:
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
  listener->setParagraphJustification(m_justify.get());
  listener->setTabs(m_tabs.get());

  double leftMargin = m_margins[1].get();
  MWAWList::Level level;
  if (m_listLevelIndex.get() >= 1) {
    float factorToLevel = MWAWPosition::getScaleFactor(m_marginsUnit.get(), WPX_INCH);
    level = m_listLevel.get();
    level.m_labelWidth = (factorToLevel*leftMargin-level.m_labelIndent);
    if (level.m_labelWidth<0.1)
      level.m_labelWidth = 0.1;
    leftMargin=level.m_labelIndent/factorToLevel;
    level.m_labelIndent = 0;
  }
  listener->setParagraphMargin(leftMargin, MWAW_LEFT, m_marginsUnit.get());
  listener->setParagraphMargin(m_margins[2].get(), MWAW_RIGHT, m_marginsUnit.get());
  listener->setParagraphTextIndent(m_margins[0].get(), m_marginsUnit.get());

  double interline = m_spacings[0].get();
  if (interline<= 0.0 || m_spacingsInterlineUnit.get()==WPX_PERCENT)
    listener->setParagraphLineSpacing(interline>0.0 ? interline : 1.0, WPX_PERCENT);
  else
    listener->setParagraphLineSpacing(interline, m_spacingsInterlineUnit.get(), m_spacingsInterlineType.get());

  listener->setParagraphMargin(m_spacings[1].get(),MWAW_TOP);
  listener->setParagraphMargin(m_spacings[2].get(),MWAW_BOTTOM);

  if (m_listLevelIndex.get() >= 1) {
    if (!listener->getCurrentList())
      listener->setCurrentList(shared_ptr<MWAWList>(new MWAWList));
    listener->getCurrentList()->set(m_listLevelIndex.get(), level);
    listener->setCurrentListLevel(m_listLevelIndex.get());
  } else
    listener->setCurrentListLevel(0);

  listener->resetParagraphBorders();
  int const wh[] = { MWAWBorder::LeftBit, MWAWBorder::RightBit, MWAWBorder::TopBit, MWAWBorder::BottomBit };
  for (size_t i = 0; i < m_borders.size(); i++) {
    if (!m_borders[i].isSet() || m_borders[i]->m_style == MWAWBorder::None)
      continue;
    if (i >= 4) {
      MWAW_DEBUG_MSG(("MWAWParagraph::send: find odd borders\n"));
      break;
    }
    listener->setParagraphBorder(wh[i], *m_borders[i]);
  }
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
