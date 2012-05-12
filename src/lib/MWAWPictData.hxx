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

/* This header contains code specific to a pict which can be stored in a
 *      WPXBinaryData, this includes :
 *         - the mac Pict format (in MWAWPictMac)
 *         - some old data names db3
 *         - some potential short data file
 */

#ifndef MWAW_PICT_DATA
#  define MWAW_PICT_DATA

#  include <assert.h>
#  include <ostream>

#  include <libwpd/WPXBinaryData.h>

#  include "libmwaw_tools.hxx"
#  include "MWAWPict.hxx"

class WPXBinaryData;
class MWAWInputStream;
typedef shared_ptr<MWAWInputStream> MWAWInputStreamPtr;

/** \brief an abstract class which defines basic formated picture ( Apple© Pict, DB3, ...) */
class MWAWPictData : public MWAWPict
{
public:
  //! the picture subtype
  enum SubType { PictMac, DB3, Unknown };
  //! returns the picture type
  virtual Type getType() const {
    return MWAWPict::PictData;
  }
  //! returns the picture subtype
  virtual SubType getSubType() const = 0;

  //! returns the final WPXBinary data
  virtual bool getBinary(WPXBinaryData &res, std::string &s) const {
    if (!valid() || isEmpty()) return false;

    s = "image/pict";
    createFileData(m_data, res);
    return true;
  }

  //! returns true if we are relatively sure that the data are correct
  virtual bool sure() const {
    return getSubType() != Unknown;
  }

  //! returns true if the picture is valid
  virtual bool valid() const {
    return false;
  }

  //! returns true if the picture is valid and has size 0 or contains no data
  bool isEmpty() const {
    return m_empty;
  }

  /** checks if the data pointed by input is known
     - if not return MWAW_R_BAD
     - if true
     - fills box if possible, if not set box=Box2f() */
  static ReadResult check(MWAWInputStreamPtr input, int size, Box2f &box) {
    return checkOrGet(input, size, box, 0L);
  }

  /** checks if the data pointed by input is known
   * - if not or if the pict is empty, returns 0L
   * - if not returns a container of picture */
  static MWAWPictData *get(MWAWInputStreamPtr input, int size) {
    MWAWPictData *res = 0L;
    Box2f box;
    if (checkOrGet(input, size, box, &res) == MWAW_R_BAD) return 0L;
    if (res) { // if the bdbox is good, we set it
      Vec2f sz = box.size();
      if (sz.x()>0 && sz.y()>0) res->setBdBox(box);
    }
    return res;
  }

  /** a virtual function used to obtain a strict order,
   * must be redefined in the subs class */
  virtual int cmp(MWAWPict const &a) const {
    int diff = MWAWPict::cmp(a);
    if (diff) return diff;
    MWAWPictData const &aPict = static_cast<MWAWPictData const &>(a);

    diff = (int) m_empty - (int) aPict.m_empty;
    if (diff) return (diff < 0) ? -1 : 1;
    long diffL = (long) m_data.size() - (long) aPict.m_data.size();
    if (diffL) return  (diff < 0) ? -1 : 1;

    // the type
    diff = getSubType() - aPict.getSubType();
    if (diff) return (diff < 0) ? -1 : 1;

    return 0;
  }

protected:
  /** a file pict can be created from the data pict by adding a header with size 512,
   * this function do this conversion needed to return the final picture */
  static bool createFileData(WPXBinaryData const &orig, WPXBinaryData &result);

  //! protected constructor: use check to construct a picture
  MWAWPictData(): m_data(), m_empty(false) { }
  MWAWPictData(Box2f &): m_data(), m_empty(false) { }

  /** \brief checks if the data pointed by input and of given size is a pict
   * - if not returns MWAW_R_BAD
   * - if true
   *    - fills the box size
   *    - creates a picture if result is given and if the picture is not empty */
  static ReadResult checkOrGet(MWAWInputStreamPtr input, int size,
                               Box2f &box, MWAWPictData **result = 0L);

  //! the data size (without the empty header of 512 characters)
  WPXBinaryData m_data;

  //! some picture can be valid but empty
  bool m_empty;
};

//! a small table file (known by open office)
class MWAWPictDB3 : public MWAWPictData
{
public:
  //! returns the picture subtype
  virtual SubType getSubType() const {
    return DB3;
  }

  //! returns true if the picture is valid
  virtual bool valid() const {
    return m_data.size() != 0;
  }

  /** a virtual function used to obtain a strict order,
  must be redefined in the subs class */
  virtual int cmp(MWAWPict const &a) const {
    return MWAWPictData::cmp(a);
  }

protected:

  //! protected constructor: uses check to construct a picture
  MWAWPictDB3() {
    m_empty = false;
  }

  friend class MWAWPictData;
  /** \brief checks if the data pointed by input and of given size is a pict
   * - if not returns MWAW_R_BAD
   * - if true
   *    - set empty to true if the picture contains no data
   *    - creates a picture if result is given and if the picture is not empty */
  static ReadResult checkOrGet(MWAWInputStreamPtr input, int size, MWAWPictData **result = 0L);
};

//! class to store small data which are potentially a picture
class MWAWPictDUnknown : public MWAWPictData
{
public:
  //! returns the picture subtype
  virtual SubType getSubType() const {
    return Unknown;
  }

  //! returns true if the picture is valid
  virtual bool valid() const {
    return m_data.size() != 0;
  }

  /** a virtual function used to obtain a strict order,
   * must be redefined in the subs class */
  virtual int cmp(MWAWPict const &a) const {
    return MWAWPictData::cmp(a);
  }

protected:

  //! protected constructor: uses check to construct a picture
  MWAWPictDUnknown() {
    m_empty = false;
  }

  friend class MWAWPictData;

  /** \brief checks if the data pointed by input and of given size is a pict
   * - if not returns MWAW_R_BAD
   * - if true
   *    - set empty to true if the picture contains no data
   *    - creates a picture if result is given and if the picture is not empty */
  static ReadResult checkOrGet(MWAWInputStreamPtr input, int size, MWAWPictData **result = 0L);
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
