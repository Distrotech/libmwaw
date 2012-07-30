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
  case NONE:
    propList.insert("text:bullet-char", " ");
    break;
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
  case DEFAULT:
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
  if (fDiff < 0.0) return -1;
  if (fDiff > 0.0) return 1;
  fDiff = m_labelWidth-levl.m_labelWidth;
  if (fDiff < 0.0) return -1;
  if (fDiff > 0.0) return 1;
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
  case MWAWList::Level::NONE:
    break;
  case MWAWList::Level::DEFAULT:
  default:
    o << "####";
  }
  if (ft.m_type != MWAWList::Level::BULLET && ft.m_startValue)
    o << ",startVal= " << ft.m_startValue;
  if (ft.m_prefix.len()) o << ", prefix='" << ft.m_prefix.cstr()<<"'";
  if (ft.m_suffix.len()) o << ", suffix='" << ft.m_suffix.cstr()<<"'";
  if (ft.m_labelIndent < 0 || ft.m_labelIndent > 0) o << ", indent=" << ft.m_labelIndent;
  if (ft.m_labelWidth < 0 || ft.m_labelWidth > 0) o << ", width=" << ft.m_labelWidth;
  o << "]";
  return o;
}

void MWAWList::setId(int newId)
{
  if (m_id == newId) return;
  m_previousId = m_id;
  m_id = newId;
  for (size_t i = 0; i < m_levels.size(); i++)
    m_levels[i].resetSendToInterface();
}

bool MWAWList::mustSendLevel(int level) const
{
  if (level <= 0 || level > int(m_levels.size()) ||
      m_levels[size_t(level-1)].isDefault()) {
    MWAW_DEBUG_MSG(("MWAWList::mustResentLevel: level %d is not defined\n",level));
    return false;
  }
  return !m_levels[size_t(level-1)].isSendToInterface();
}


void MWAWList::sendTo(WPXDocumentInterface &docInterface, int level) const
{
  if (level <= 0 || level > int(m_levels.size()) ||
      m_levels[size_t(level-1)].isDefault()) {
    MWAW_DEBUG_MSG(("MWAWList::sendTo: level %d is not defined\n",level));
    return;
  }

  if (m_id==-1) {
    MWAW_DEBUG_MSG(("MWAWList::sendTo: the list id is not set\n"));
    static int falseId = 1000;
    m_id = falseId++;
  }

  if (m_levels[size_t(level-1)].isSendToInterface()) return;

  WPXPropertyList propList;
  propList.insert("libwpd:id", m_id);
  propList.insert("libwpd:level", level);
  m_levels[size_t(level-1)].addTo(propList, m_actualIndices[size_t(level-1)]);
  if (!m_levels[size_t(level-1)].isNumeric())
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
    m_levels.resize(size_t(levl));
    m_actualIndices.resize(size_t(levl), 0);
    m_nextIndices.resize(size_t(levl), 1);
  }
  int needReplace = m_levels[size_t(levl-1)].cmp(level) != 0 ||
                    (level.m_startValue && m_nextIndices[size_t(levl-1)] !=level.getStartValue());
  if (level.m_startValue > 0 || level.m_type != m_levels[size_t(levl-1)].m_type) {
    m_nextIndices[size_t(levl-1)]=level.getStartValue();
  }
  if (needReplace) m_levels[size_t(levl-1)] = level;
}

void MWAWList::setLevel(int levl) const
{
  if (levl < 1 || levl > int(m_levels.size())) {
    MWAW_DEBUG_MSG(("MWAWList::setLevel: can not set level %d\n",levl));
    return;
  }

  if (levl < int(m_levels.size()))
    m_actualIndices[size_t(levl)]=
      (m_nextIndices[size_t(levl)]=m_levels[size_t(levl)].getStartValue())-1;

  m_actLevel=levl-1;
}

void MWAWList::openElement() const
{
  if (m_actLevel < 0 || m_actLevel >= int(m_levels.size())) {
    MWAW_DEBUG_MSG(("MWAWList::openElement: can not set level %d\n",m_actLevel));
    return;
  }
  if (m_levels[size_t(m_actLevel)].isNumeric())
    m_actualIndices[size_t(m_actLevel)]=m_nextIndices[size_t(m_actLevel)]++;
}

bool MWAWList::isNumeric(int levl) const
{
  if (levl < 1 || levl > int(m_levels.size())) {
    MWAW_DEBUG_MSG(("MWAWList::isNumeric: the level does not exist\n"));
    return false;
  }

  return m_levels[size_t(levl-1)].isNumeric();
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
