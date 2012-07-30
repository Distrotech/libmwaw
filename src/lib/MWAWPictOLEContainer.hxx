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

/* This header contains code specific to an ole object which can be stored in a
 *      WPXBinaryData
 */

#ifndef MWAW_PICT_OLECONTAINER
#  define MWAW_PICT_OLECONTAINER

#  include <assert.h>
#  include <ostream>

#  include <libwpd/WPXBinaryData.h>

#  include "libmwaw_internal.hxx"
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
