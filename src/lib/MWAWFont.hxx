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

#ifndef MWAW_FONT
#  define MWAW_FONT

#include <assert.h>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"

class MWAWContentListener;
class MWAWFontConverter;

//! Class to store font
class MWAWFont
{
public:
  /** constructor
   *
   * \param id system id font
   * \param size the font size
   * \param f the font attributes bold, ... */
  MWAWFont(int id=-1, int size=12, int f = 0) : m_id(id), m_size(size), m_flags(f), m_color() {
    resetColor();
  };
  //! resets the font color to black
  void resetColor() {
    m_color = Vec3uc();
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
  void setColor(Vec3uc color) {
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
  void getColor(Vec3uc &c) const {
    for (int i = 0; i < 3; i++) c[i]=m_color[i];
  }
  //! returns a string which can be used for debugging
  std::string getDebugString(shared_ptr<MWAWFontConverter> &converter) const;

  //! operator==
  bool operator==(MWAWFont const &f) const {
    return cmp(f) == 0;
  }
  //! operator!=
  bool operator!=(MWAWFont const &f) const {
    return cmp(f) != 0;
  }

  //! a comparison function
  int cmp(MWAWFont const &oth) const {
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

  /** sends font to a listener */
  void sendTo(MWAWContentListener *listener, shared_ptr<MWAWFontConverter> &convert, MWAWFont &actFont) const;

protected:
  int m_id /** font identificator*/, m_size /** font size */, m_flags /** font attributes */;
  Vec3uc m_color /** font color */;
};


#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
