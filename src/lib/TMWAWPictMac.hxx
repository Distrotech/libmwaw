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

/* This header contains code specific to a pict mac file
 * see http://developer.apple.com/legacy/mac/library/documentation/mac/QuickDraw/QuickDraw-458.html
 */

#ifndef MWAW_PICT_MWAW
#  define MWAW_PICT_MWAW

#  include <assert.h>
#  include <ostream>
#  include <string>

#  include <libwpd/WPXBinaryData.h>

#  include "libmwaw_tools.hxx"
#  include "TMWAWPictData.hxx"

namespace libmwaw_tools
{
/** \brief Class to read/store a Mac Pict1.0/2.0 */
class PictMac : public PictData
{
public:

  //! returns the picture subtype
  virtual SubType getSubType() const {
    return PictData::PictMac;
  }

  //! returns the final WPXBinary data
  virtual bool getBinary(WPXBinaryData &res, std::string &s) const {
    if (!valid() || isEmpty()) return false;

    s = "image/pict";
    if (m_version == 1) {
      WPXBinaryData dataV2;
      if (convertPict1To2(m_data, dataV2)) {
        createFileData(dataV2, res);
        return true;
      }
    }
    createFileData(m_data, res);
    return true;
  }

  //! returns true if the picture is valid
  virtual bool valid() const {
    return (m_version >= 1) && (m_version <= 2);
  }

  /** a virtual function used to obtain a strict order,
   * must be redefined in the subs class */
  virtual int cmp(Pict const &a) const {
    int diff = PictData::cmp(a);
    if (diff) return diff;
    PictMac const &aPict = static_cast<PictMac const &>(a);

    long diffL = (long) m_data.size() - (long) aPict.m_data.size();
    if (diffL) return  (diff < 0) ? -1 : 1;
    diff = m_version - (int) aPict.m_version;
    if (diff) return (diff < 0) ? -1 : 1;
    diff = m_subVersion - (int) aPict.m_subVersion;
    if (diff) return (diff < 0) ? -1 : 1;

    // as we can not compare data, we only compare the pict position
    diffL = (long) this - (long) &aPict;
    if (diffL) return  (diff < 0) ? -1 : 1;

    return 0;
  }

  //! convert a Pict1.0 in Pict2.0, if possible
  static bool convertPict1To2(WPXBinaryData const &orig, WPXBinaryData &result);

  /** \brief tries to parse a Pict1.0 and dump the file
   * Actually mainly used for debugging, but will be a first step,
   * if we want convert such a Pict in a Odg picture */
  static void parsePict1(WPXBinaryData const &orig, std::string const &fname);

  /** \brief tries to parse a Pict2. and dump the file
   * Actually mainly used for debugging, but will be a first step,
   * if we want convert such a Pict in a Odg picture */
  static void parsePict2(WPXBinaryData const &orig, std::string const &fname);

protected:
  //! protected constructor: use check to construct a picture
  PictMac(Box2f box) : PictData(box), m_version(-1), m_subVersion(-1) {
    extendBDBox(1.0);
  }

  friend class PictData;
  /** checks if the data pointed by input and of given size is a pict 1.0, 2.0 or 2.1
     - if not returns MWAW_R_BAD
     - if true
     - fills box if possible
     - creates a picture if result is given
  */
  static ReadResult checkOrGet(TMWAWInputStreamPtr input, int size,
                               Box2f &box, PictData **result = 0L);

  //! the picture version
  int m_version;
  //! the picture subversion
  int m_subVersion;
};
}

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
