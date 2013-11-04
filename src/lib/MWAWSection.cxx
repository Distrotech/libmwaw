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

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"
#include "MWAWPosition.hxx"

#include "MWAWSection.hxx"

////////////////////////////////////////////////////////////
// Columns
////////////////////////////////////////////////////////////
std::ostream &operator<<(std::ostream &o, MWAWSection::Column const &col)
{
  if (col.m_width > 0) o << "w=" << col.m_width << ",";
  static char const *(wh[4])= {"L", "R", "T", "B"};
  for (int i = 0; i < 4; i++) {
    if (col.m_margins[i]>0)
      o << "col" << wh[i] << "=" << col.m_margins[i] << ",";
  }
  return o;
}

bool MWAWSection::Column::addTo(RVNGPropertyList &propList) const
{
  // The "style:rel-width" is expressed in twips (1440 twips per inch) and includes the left and right Gutter
  double factor = 1.0;
  switch(m_widthUnit) {
  case RVNG_POINT:
  case RVNG_INCH:
    factor = MWAWPosition::getScaleFactor(m_widthUnit, RVNG_TWIP);
  case RVNG_TWIP:
    break;
  case RVNG_PERCENT:
  case RVNG_GENERIC:
  case RVNG_UNIT_ERROR:
  default:
    MWAW_DEBUG_MSG(("MWAWSection::Column::addTo: unknown unit\n"));
    return false;
  }
  propList.insert("style:rel-width", m_width * factor, RVNG_TWIP);
  propList.insert("fo:start-indent", m_margins[libmwaw::Left]);
  propList.insert("fo:end-indent", m_margins[libmwaw::Right]);
  static bool first = true;
  if (first && (m_margins[libmwaw::Top]>0||m_margins[libmwaw::Bottom]>0)) {
    first=false;
    MWAW_DEBUG_MSG(("MWAWSection::Column::addTo: sending before/after margins is not implemented\n"));
  }
  return true;
}

////////////////////////////////////////////////////////////
// Section
////////////////////////////////////////////////////////////
std::ostream &operator<<(std::ostream &o, MWAWSection const &sec)
{
  if (sec.m_width>0)
    o << "width=" << sec.m_width << ",";
  if (!sec.m_backgroundColor.isWhite())
    o << "bColor=" << sec.m_backgroundColor << ",";
  if (sec.m_balanceText)
    o << "text[balance],";
  for (size_t c=0; c < sec.m_columns.size(); c++)
    o << "col" << c << "=[" << sec.m_columns[c] << "],";
  if (sec.m_columnSeparator.m_style != MWAWBorder::None &&
      sec.m_columnSeparator.m_width > 0)
    o << "colSep=[" << sec.m_columnSeparator << "],";
  return o;
}

void MWAWSection::setColumns(int num, double width, RVNGUnit widthUnit, double colSep)
{
  if (num<0) {
    MWAW_DEBUG_MSG(("MWAWSection::setColumns: called with negative number of column\n"));
    num=1;
  } else if (num > 1 && width<=0) {
    MWAW_DEBUG_MSG(("MWAWSection::setColumns: called without width\n"));
    num=1;
  }
  m_columns.resize(0);
  if (num==1 && (width<=0 || colSep<=0))
    return;

  Column column;
  column.m_width=width;
  column.m_widthUnit = widthUnit;
  column.m_margins[libmwaw::Left] = column.m_margins[libmwaw::Right] = colSep/2.;
  m_columns.resize(size_t(num), column);
}

void MWAWSection::addTo(RVNGPropertyList &propList) const
{
  propList.insert("fo:margin-left", 0.0);
  propList.insert("fo:margin-right", 0.0);
  if (m_columns.size() > 1)
    propList.insert("text:dont-balance-text-columns", !m_balanceText);
  if (!m_backgroundColor.isWhite())
    propList.insert("fo:background-color", m_backgroundColor.str().c_str());
  if (m_columnSeparator.m_style != MWAWBorder::None &&
      m_columnSeparator.m_width > 0) {
    propList.insert("libwpd:colsep-width", m_columnSeparator.m_width, RVNG_POINT);
    propList.insert("libwpd:colsep-color", m_columnSeparator.m_color.str().c_str());
    propList.insert("libwpd:colsep-height", "100%");
    propList.insert("libwpd:colsep-vertical-align", "middle");
  }
}

void MWAWSection::addColumnsTo(RVNGPropertyListVector &propVec) const
{
  size_t numCol = m_columns.size();
  if (!numCol) return;
  for (size_t c=0; c < numCol; c++) {
    RVNGPropertyList propList;
    if (m_columns[c].addTo(propList))
      propVec.append(propList);
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
