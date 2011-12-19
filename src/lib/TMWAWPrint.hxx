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

/* This header contains code specific to a mac file needed to read
 * TPrint structure (ie. the structure which keeps the printer parameters)
 * see http://developer.apple.com/documentation/Mac/QuickDraw/QuickDraw-411.html
 *   or http://www.mactech.com/articles/mactech/Vol.01/01.09/AllAboutPrinting/index.html
 */

#ifndef MWAW_MWAW_PRINT
#  define MWAW_MWAW_PRINT

#  include <assert.h>
#  include <ostream>

#  include "libmwaw_tools.hxx"

class TMWAWInputStream;
typedef shared_ptr<TMWAWInputStream> TMWAWInputStreamPtr;

namespace libmwaw_tools_mac
{
/** \struct PrintInfoImpl
    \brief internal structure used to keep TPrint content */
struct PrintInfoImpl;

//! the Apple© rectangle : Rect
struct PrintRect {
  //! returns the size
  Vec2i size() const {
    return m_pos[1]-m_pos[0];
  }
  //! returns the position ( 0: leftTop, 1:rightBottom )
  Vec2i pos(int i) const {
    return m_pos[i];
  }

  //! operator <<
  friend std::ostream &operator<< (std::ostream &o, PrintRect const & r) {
    o << "[" << r.m_pos[0] << " " << r.m_pos[1] << "]";
    return o;
  }

  //! read value in a file, knowing the resolution
  bool read(TMWAWInputStreamPtr input, Vec2i const &res);

protected:
  //! the LT and RB positions
  Vec2i m_pos[2];
};

//! the Apple© printer information : TPrint
struct PrintInfo {
  //! constructor
  PrintInfo();
  //! destructor
  ~PrintInfo();

  //! returns the page rectangle
  PrintRect page() const;
  //! returns the paper rectangle
  PrintRect paper() const;

  friend std::ostream &operator<< (std::ostream &o, PrintInfo const & r);

  //! reads the struture in a file
  bool read(TMWAWInputStreamPtr input);

protected:
  //! internal data
  PrintInfoImpl *m_data;
private:
  PrintInfo(PrintInfo const &orig) : m_data(0) {
    *this=orig;
  }
  PrintInfo &operator=(PrintInfo const &orig);
};
}

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
