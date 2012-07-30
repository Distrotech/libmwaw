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
   * \param newId system id font
   * \param sz the font size
   * \param f the font attributes bold, ... */
  MWAWFont(int newId=-1, int sz=12, uint32_t f = 0) : m_id(newId), m_size(sz), m_flags(f), m_color() {
    resetColor();
  };
  //! inserts the set value in the current font
  void insert(MWAWFont &ft) {
    m_id.insert(ft.m_id);
    m_size.insert(ft.m_size);
    if (ft.m_flags.isSet()) {
      if (m_flags.isSet())
        setFlags(flags()| ft.flags());
      else
        m_flags = ft.m_flags;
    }
    m_color.insert(ft.m_color);
  }
  //! resets the font color to black
  void resetColor() {
    m_color = Vec3uc();
  }
  //! sets the font id and resets size to the previous size for this font
  void setFont(int newId) {
    resetColor();
    m_id=newId;
  }
  //! sets the font id
  void setId(int newId) {
    m_id = newId;
  }
  //! sets the font size
  void setSize(int sz) {
    m_size = sz;
  }
  //! sets the font attributes bold, ...
  void setFlags(uint32_t fl) {
    m_flags = fl;
  }
  //! sets the font color
  void setColor(Vec3uc color) {
    m_color = color;
  }
  //! returns true if the font id is initialized
  bool isSet() const {
    return m_id.isSet();
  }
  //! returns the font id
  int id() const {
    return m_id.get();
  }
  //! returns the font size
  int size() const {
    return m_size.get();
  }
  //! returns the font flags
  uint32_t flags() const {
    return m_flags.get();
  }
  //! returns true if the font color is not black
  bool hasColor() const {
    return m_color.isSet();
  }
  //! returns the font color
  void getColor(Vec3uc &c) const {
    c = m_color.get();
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
    if (flags() != oth.flags()) return diff;
    for (int i = 0; i < 3; i++) {
      diff = m_color.get()[i] - oth.m_color.get()[i];
      if (diff!=0) return diff;
    }
    return diff;
  }

  /** sends font to a listener */
  void sendTo(MWAWContentListener *listener, shared_ptr<MWAWFontConverter> &convert, MWAWFont &actFont) const;

protected:
  Variable<int> m_id /** font identificator*/, m_size /** font size */;
  Variable<uint32_t> m_flags /** font attributes */;
  Variable<Vec3uc> m_color /** font color */;
};


#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
