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

#include <sstream>

#include "libmwaw_internal.hxx"

#include "MWAWContentListener.hxx"
#include "MWAWFontConverter.hxx"

#include "MWAWFont.hxx"

std::string MWAWFont::getDebugString(shared_ptr<MWAWFontConverter> &converter) const
{
  std::stringstream o;
  o << std::dec;
  if (id() != -1) {
    if (converter)
      o << "nam='" << converter->getName(id()) << "',";
    else
      o << "id=" << id() << ",";
  }
  if (size() > 0) o << "sz=" << size() << ",";

  if (m_flags) o << "fl=";
  if (m_flags&MWAW_BOLD_BIT) o << "b:";
  if (m_flags&MWAW_ITALICS_BIT) o << "it:";
  if (m_flags&MWAW_UNDERLINE_BIT) o << "underL:";
  if (m_flags&MWAW_OVERLINE_BIT) o << "overL:";
  if (m_flags&MWAW_EMBOSS_BIT) o << "emboss:";
  if (m_flags&MWAW_SHADOW_BIT) o << "shadow:";
  if (m_flags&MWAW_OUTLINE_BIT) o << "outline:";
  if (m_flags&MWAW_DOUBLE_UNDERLINE_BIT) o << "2underL:";
  if (m_flags&MWAW_STRIKEOUT_BIT) o << "strikeout:";
  if (m_flags&MWAW_SMALL_CAPS_BIT) o << "smallCaps:";
  if (m_flags&MWAW_ALL_CAPS_BIT) o << "allCaps:";
  if (m_flags&MWAW_HIDDEN_BIT) o << "hidden:";
  if (m_flags&MWAW_SMALL_PRINT_BIT) o << "consended:";
  if (m_flags&MWAW_LARGE_BIT) o << "extended:";
  if ((m_flags&MWAW_SUPERSCRIPT_BIT) || (m_flags&MWAW_SUPERSCRIPT100_BIT))
    o << "superS:";
  if ((m_flags&MWAW_SUBSCRIPT_BIT) || (m_flags&MWAW_SUBSCRIPT100_BIT))
    o << "subS:";
  if (m_flags) o << ",";

  if (hasColor()) {
    int col[3];
    getColor(col);
    o << "col=(";
    for (int i = 0; i < 3; i++) o << col[i] << ",";
    o << "),";
  }
  return o.str();
}

void MWAWFont::sendTo(MWAWContentListener *listener, shared_ptr<MWAWFontConverter> &convert, MWAWFont &actualFont) const
{
  if (listener == 0L) return;

  std::string fName;
  int dSize = 0;
  int newSize = size();

  if (id() != -1) {
    actualFont.setId(id());
    convert->getOdtInfo(actualFont.id(), fName, dSize);
    listener->setTextFont(fName.c_str());
    // if no size reset to default
    if (newSize == -1) newSize = 12;
  }

  if (newSize > 0) {
    actualFont.setSize(newSize);
    dSize = 0;
    convert->getOdtInfo(actualFont.id(), fName, dSize);
    listener->setFontSize(actualFont.size()+dSize);
  }

  actualFont.setFlags(flags());
  listener->setTextAttribute(actualFont.flags());
  actualFont.setColor(m_color);
  listener->setFontColor(m_color);
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
