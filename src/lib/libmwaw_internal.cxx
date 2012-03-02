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

#include "libmwaw_libwpd.hxx"

#include "libmwaw_internal.hxx"

namespace libmwaw
{
namespace internal
{
void printTabs(std::ostream &o, std::vector<DMWAWTabStop> const &tabs)
{
  int nbTabs = tabs.size();
  if (!nbTabs) return;

  o << "tabs=(" << std::dec;
  for (int i = 0; i < nbTabs; i++) {
    o << tabs[i].m_position;

    switch (tabs[i].m_alignment) {
    case LEFT:
      o << "L";
      break;
    case CENTER:
      o << "C";
      break;
    case RIGHT:
      o << "R";
      break;
    case DECIMAL:
      o << ":decimal";
      break; // decimal align
    case BAR:
      o << ":bar";
      break;
    default:
      break;
    }
    if (tabs[i].m_leaderCharacter != '\0')
      o << ":sep='"<< (char) tabs[i].m_leaderCharacter << "'";
    o << ",";
  }
  o << ")";
}
}
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
