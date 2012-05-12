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

#include "MWAWContentListener.hxx"

#include "MWAWStruct.hxx"
#include "MWAWTools.hxx"

namespace MWAWStruct
{

void Font::sendTo(MWAWContentListener *listener, MWAWTools::ConvertissorPtr &convert, Font &actualFont, bool force) const
{
  if (listener == 0L) return;
  int newSize = size();

  if (id() != -1 && (force || id() != actualFont.id())) {
    actualFont.setId(id());

    std::string fName;
    int dSize = 0;
    convert->getFontOdtInfo(actualFont.id(), fName, dSize);

    listener->setTextFont(fName.c_str());
    if (actualFont.size() > 0)
      listener->setFontSize(actualFont.size()+dSize);
    // if no size, reset to default
    if (newSize == -1) newSize = 12;
  }

  if (newSize > 0 && (force || newSize != actualFont.size())) {
    actualFont.setSize(newSize);
    std::string fName;
    int dSize = 0;
    convert->getFontOdtInfo(actualFont.id(), fName, dSize);
    listener->setFontSize(actualFont.size()+dSize);
  }

  if (force || flags() != actualFont.flags()) {
    actualFont.setFlags(flags());
    listener->setTextAttribute(actualFont.flags());
  }

  if (force || hasColor() || actualFont.hasColor()) {
    actualFont.setColor(m_color);
    listener->setFontColor(m_color);
  }
}
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
