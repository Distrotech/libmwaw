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

/* This header contains code specific to a pict mac file
 * see http://developer.apple.com/legacy/mac/library/documentation/mac/QuickDraw/QuickDraw-458.html
 */

#ifndef MWAW_PICT_MWAW
#  define MWAW_PICT_MWAW

#  include <assert.h>
#  include <ostream>
#  include <string>

#  include <libwpd/libwpd.h>

#  include "libmwaw_internal.hxx"
#  include "MWAWPictData.hxx"

/** \brief Class to read/store a Mac Pict1.0/2.0 */
class MWAWPictMac : public MWAWPictData
{
public:

  //! returns the picture subtype
  virtual SubType getSubType() const {
    return MWAWPictData::PictMac;
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
  virtual int cmp(MWAWPict const &a) const {
    int diff = MWAWPictData::cmp(a);
    if (diff) return diff;
    MWAWPictMac const &aPict = static_cast<MWAWPictMac const &>(a);

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
  MWAWPictMac(Box2f box) : MWAWPictData(box), m_version(-1), m_subVersion(-1) {
    extendBDBox(1.0);
  }

  friend class MWAWPictData;
  /** checks if the data pointed by input and of given size is a pict 1.0, 2.0 or 2.1
     - if not returns MWAW_R_BAD
     - if true
     - fills box if possible
     - creates a picture if result is given
  */
  static ReadResult checkOrGet(MWAWInputStreamPtr input, int size,
                               Box2f &box, MWAWPictData **result = 0L);

  //! the picture version
  int m_version;
  //! the picture subversion
  int m_subVersion;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
