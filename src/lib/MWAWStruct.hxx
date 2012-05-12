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

#ifndef MWAW_MWAW_STRUCT
#  define MWAW_MWAW_STRUCT

#include <assert.h>
#include <iostream>

#include <vector>

#include "libmwaw_tools.hxx"

class MWAWContentListener;
namespace MWAWTools
{
class Convertissor;
typedef shared_ptr<Convertissor> ConvertissorPtr;
}

//! namespace which defines some basic classes used to parse a version all MW-mac
namespace MWAWStruct
{
//! Class to store font
class Font
{
public:
  /** constructor
   *
   * \param id system id font
   * \param size the font size
   * \param f the font attributes bold, ... */
  Font(int id=-1, int size=12, int f = 0) : m_id(id), m_size(size), m_flags(f) {
    resetColor();
  };
  //! resets the font color to black
  void resetColor() {
    m_color[0] = m_color[1] = m_color[2] = 0;
  }
  //! sets the font id and resets size to the previous size for this font
  void setFont(int id) {
    resetColor();
    m_id=id;
  }
  //! sets the font id
  void setId(int id) {
    m_id = id;
  }
  //! sets the font size
  void setSize(int size) {
    m_size = size;
  }
  //! sets the font attributes bold, ...
  void setFlags(int flags) {
    m_flags = flags;
  }
  //! sets the font color
  void setColor(int const color[3]) {
    for (int i = 0; i < 3; i++) m_color[i] = color[i];
  }
  //! returns true if the font id is initialized
  bool isSet() const {
    return m_id >= 0;
  }
  //! returns the font id
  int id() const {
    return m_id;
  }
  //! returns the font size
  int size() const {
    return m_size;
  }
  //! returns the font flags
  int flags() const {
    return m_flags;
  }
  //! returns true if the font color is not black
  bool hasColor() const {
    for (int i = 0; i < 3; i++) if (m_color[i]) return true;
    return false;
  }
  //! returns the font color
  void getColor(int (&c) [3]) const {
    for (int i = 0; i < 3; i++) c[i]=m_color[i];
  }

  //! operator==
  bool operator==(Font const &f) const {
    return cmp(f) == 0;
  }
  //! operator!=
  bool operator!=(Font const &f) const {
    return cmp(f) != 0;
  }

  //! a comparison function
  int cmp(Font const &oth) const {
    int diff = id() - oth.id();
    if (diff != 0) return diff;
    diff = size() - oth.size();
    if (diff != 0) return diff;
    diff = flags() - oth.flags();
    if (diff != 0) return diff;
    for (int i = 0; i < 3; i++) {
      diff = m_color[i] - oth.m_color[i];
      if (diff!=0) return diff;
    }
    return diff;
  }

  /** sends font to a listener
   *
   * if \a force = false, sends only difference with actFont */
  void sendTo(MWAWContentListener *listener, MWAWTools::ConvertissorPtr &convert, Font &actFont, bool force = false) const;

protected:
  int m_id /** font identificator*/, m_size /** font size */, m_flags /** font attributes */, m_color[3] /** font color */;
};
}



#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
