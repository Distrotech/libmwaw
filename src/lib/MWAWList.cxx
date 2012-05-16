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

#include <cstring>
#include <iostream>

#include <libwpd/WPXDocumentInterface.h>
#include <libwpd/WPXPropertyList.h>

#include "libmwaw_internal.hxx"
#include "libmwaw_internal.hxx"

#include "MWAWList.hxx"

void MWAWList::Level::addTo(WPXPropertyList &propList, int startVal) const
{
  propList.insert("text:min-label-width", m_labelWidth);
  propList.insert("text:space-before", m_labelIndent);

  switch(m_type) {
  case BULLET:
    if (m_bullet.len())
      propList.insert("text:bullet-char", m_bullet.cstr());
    else {
      MWAW_DEBUG_MSG(("MWAWList::Level::addTo: the bullet char is not defined\n"));
      propList.insert("text:bullet-char", "*");
    }
    break;
  case DECIMAL:
  case LOWER_ALPHA:
  case UPPER_ALPHA:
  case LOWER_ROMAN:
  case UPPER_ROMAN:
    if (m_prefix.len()) propList.insert("style:num-prefix",m_prefix);
    if (m_suffix.len()) propList.insert("style:num-suffix", m_suffix);
    if (m_type==DECIMAL) propList.insert("style:num-format", "1");
    else if (m_type==LOWER_ALPHA) propList.insert("style:num-format", "a");
    else if (m_type==UPPER_ALPHA) propList.insert("style:num-format", "A");
    else if (m_type==LOWER_ROMAN) propList.insert("style:num-format", "i");
    else propList.insert("style:num-format", "I");
    propList.insert("text:start-value", startVal);
    break;
  case NONE:
    break;
  default:
    MWAW_DEBUG_MSG(("MWAWList::Level::addTo: the level type is not set\n"));
  }

  m_sendToInterface = true;
}

int MWAWList::Level::cmp(MWAWList::Level const &levl) const
{
  int diff = int(m_type)-int(levl.m_type);
  if (diff) return diff;
  double fDiff = m_labelIndent-levl.m_labelIndent;
  if (fDiff) return fDiff < 0.0 ? -1 : 1;
  fDiff = m_labelWidth-levl.m_labelWidth;
  if (fDiff) return fDiff < 0.0 ? -1 : 1;
  diff = strcmp(m_prefix.cstr(),levl.m_prefix.cstr());
  if (diff) return diff;
  diff = strcmp(m_suffix.cstr(),levl.m_suffix.cstr());
  if (diff) return diff;
  diff = strcmp(m_bullet.cstr(),levl.m_bullet.cstr());
  if (diff) return diff;
  return 0;
}

std::ostream &operator<<(std::ostream &o, MWAWList::Level const &ft)
{
  o << "ListLevel[";
  switch(ft.m_type) {
  case MWAWList::Level::BULLET:
    o << "bullet='" << ft.m_bullet.cstr() <<"'";
    break;
  case MWAWList::Level::DECIMAL:
    o << "decimal";
    break;
  case MWAWList::Level::LOWER_ALPHA:
    o << "alpha";
    break;
  case MWAWList::Level::UPPER_ALPHA:
    o << "ALPHA";
    break;
  case MWAWList::Level::LOWER_ROMAN:
    o << "roman";
    break;
  case MWAWList::Level::UPPER_ROMAN:
    o << "ROMAN";
    break;
  default:
    o << "####";
  }
  if (ft.m_type != MWAWList::Level::BULLET && ft.m_startValue)
    o << ",startVal= " << ft.m_startValue;
  if (ft.m_prefix.len()) o << ", prefix='" << ft.m_prefix.cstr()<<"'";
  if (ft.m_suffix.len()) o << ", suffix='" << ft.m_suffix.cstr()<<"'";
  if (ft.m_labelIndent) o << ", indent=" << ft.m_labelIndent;
  if (ft.m_labelWidth) o << ", width=" << ft.m_labelWidth;
  o << "]";
  return o;
}

void MWAWList::setId(int newId)
{
  if (m_id == newId) return;
  m_previousId = m_id;
  m_id = newId;
  for (int i = 0; i < int(m_levels.size()); i++)
    m_levels[i].resetSendToInterface();
}

bool MWAWList::mustSendLevel(int level) const
{
  if (level <= 0 || level > int(m_levels.size()) ||
      m_levels[level-1].isDefault()) {
    MWAW_DEBUG_MSG(("MWAWList::mustResentLevel: level %d is not defined\n",level));
    return false;
  }
  return !m_levels[level-1].isSendToInterface();
}


void MWAWList::sendTo(WPXDocumentInterface &docInterface, int level) const
{
  if (level <= 0 || level > int(m_levels.size()) ||
      m_levels[level-1].isDefault()) {
    MWAW_DEBUG_MSG(("MWAWList::sendTo: level %d is not defined\n",level));
    return;
  }

  if (m_id==-1) {
    MWAW_DEBUG_MSG(("MWAWList::sendTo: the list id is not set\n"));
    static int falseId = 1000;
    m_id = falseId++;
  }

  if (m_levels[level-1].isSendToInterface()) return;

  WPXPropertyList propList;
  propList.insert("libwpd:id", m_id);
  propList.insert("libwpd:level", level);
  m_levels[level-1].addTo(propList, m_actualIndices[level-1]);
  if (!m_levels[level-1].isNumeric())
    docInterface.defineUnorderedListLevel(propList);
  else
    docInterface.defineOrderedListLevel(propList);
}

void MWAWList::set(int levl, Level const &level)
{
  if (levl < 1) {
    MWAW_DEBUG_MSG(("MWAWList::set: can not set level %d\n",levl));
    return;
  }
  if (levl > int(m_levels.size())) {
    m_levels.resize(levl);
    m_actualIndices.resize(levl, 0);
    m_nextIndices.resize(levl, 1);
  }
  int needReplace = m_levels[levl-1].cmp(level) != 0 ||
                    (level.m_startValue && m_nextIndices[levl-1] !=level.getStartValue());
  if (level.m_startValue > 0 || level.m_type != m_levels[levl-1].m_type) {
    m_nextIndices[levl-1]=level.getStartValue();
  }
  if (needReplace) m_levels[levl-1] = level;
}

void MWAWList::setLevel(int levl) const
{
  if (levl < 1 || levl > int(m_levels.size())) {
    MWAW_DEBUG_MSG(("MWAWList::setLevel: can not set level %d\n",levl));
    return;
  }

  if (levl < int(m_levels.size()))
    m_actualIndices[levl]=
      (m_nextIndices[levl]=m_levels[levl].getStartValue())-1;

  m_actLevel=levl-1;
}

void MWAWList::openElement() const
{
  if (m_actLevel < 0 || m_actLevel >= int(m_levels.size())) {
    MWAW_DEBUG_MSG(("MWAWList::openElement: can not set level %d\n",m_actLevel));
    return;
  }
  if (m_levels[m_actLevel].isNumeric())
    m_actualIndices[m_actLevel]=m_nextIndices[m_actLevel]++;
}

bool MWAWList::isNumeric(int levl) const
{
  if (levl < 1 || levl > int(m_levels.size())) {
    MWAW_DEBUG_MSG(("MWAWList::isNumeric: the level does not exist\n"));
    return false;
  }

  return m_levels[levl-1].isNumeric();
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab: