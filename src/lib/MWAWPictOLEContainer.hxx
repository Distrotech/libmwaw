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

/* This header contains code specific to an ole object which can be stored in a
 *      WPXBinaryData
 */

#ifndef MWAW_PICT_OLECONTAINER
#  define MWAW_PICT_OLECONTAINER

#  include <assert.h>
#  include <ostream>

#  include <libwpd/WPXBinaryData.h>

#  include "libmwaw_tools.hxx"
#  include "MWAWPict.hxx"

class WPXBinaryData;
class MWAWInputStream;
typedef shared_ptr<MWAWInputStream> MWAWInputStreamPtr;

/** \brief an abstract class which defines a simple container to store ole data */
class MWAWPictOLEContainer : public MWAWPict
{
public:
  //! returns the picture type
  virtual Type getType() const {
    return MWAWPict::OleContainer;
  }

  //! returns the final WPXBinary oleContainer
  virtual bool getBinary(WPXBinaryData &res, std::string &s) const {
    if (!valid() || isEmpty()) return false;

    s = "object/ole";
    res = m_data;
    return true;
  }

  //! returns true if the picture is valid
  virtual bool valid() const {
    return !m_empty;
  }

  //! returns true if the picture is valid and has size 0 or contains no oleContainer
  bool isEmpty() const {
    return m_empty;
  }

  /** checks if size is positive,
   * - if not or if the pict is empty, returns 0L
   * - if not returns a container of picture */
  static MWAWPictOLEContainer *get(MWAWInputStreamPtr input, int size) {
    MWAWPictOLEContainer *res = 0L;
    Box2f box;
    if (checkOrGet(input, size, box, &res) == MWAW_R_BAD) return 0L;
    return res;
  }

  /** a virtual function used to obtain a strict order,
   * must be redefined in the subs class */
  virtual int cmp(MWAWPict const &a) const {
    int diff = MWAWPict::cmp(a);
    if (diff) return diff;
    MWAWPictOLEContainer const &aPict = static_cast<MWAWPictOLEContainer const &>(a);

    diff = (int) m_empty - (int) aPict.m_empty;
    if (diff) return (diff < 0) ? -1 : 1;
    long diffL = (long) m_data.size() - (long) aPict.m_data.size();
    if (diffL) return  (diff < 0) ? -1 : 1;

    return 0;
  }

protected:
  //! protected constructor: use check to construct a picture
  MWAWPictOLEContainer(Box2f ) : m_data(), m_empty(false) { }
  MWAWPictOLEContainer( ) : m_data(), m_empty(false) { }

  /** \brief checks if size is >= 0
   * - if not returns MWAW_R_BAD
   * - if true
   *    - creates a picture if result is given and if the picture is not empty */
  static ReadResult checkOrGet(MWAWInputStreamPtr input, int size,
                               Box2f &box, MWAWPictOLEContainer **result = 0L);

  //! the oleContainer size (without the empty header of 512 characters)
  WPXBinaryData m_data;

  //! some picture can be valid but empty
  bool m_empty;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
