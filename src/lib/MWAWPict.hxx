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

/*
 * This header contains code specific to manage basic picture (line, rectangle, ...)
 *
 * Note: generic class for all sort of pict
 *      - PictBitmap: regroup some classes used to store bitmap
 *      - PictData: regroup the data which can be read via a RVNGBinaryData
 */

#ifndef MWAW_PICT
#  define MWAW_PICT

#  include <assert.h>
#  include <ostream>
#  include <vector>

#  include "libmwaw_internal.hxx"

/** \brief Generic function used to define/store a picture */
class MWAWPict
{
public:
  //! virtual destructor
  virtual ~MWAWPict() {}

  /*! \enum Type
   * \brief the different picture types:
   *      - pictData: a classic format of file (Apple© Pict, ...)
   *      - bitmap: a image
   *      - ...
   */
  enum Type { PictData, Bitmap, Unknown };
  //! returns the picture type
  virtual Type getType() const = 0;

  /*! \brief an enum to defined the result of a parsing
   * use by some picture's classes which can read their data
   * - R_OK_EMPTY: data ok but empty content,
   * - R_MAYBE: can not check if the data are valid
   * - ...
   */
  enum ReadResult { MWAW_R_BAD=0, MWAW_R_OK, MWAW_R_OK_EMPTY, MWAW_R_MAYBE };

  //! returns the bdbox of the picture
  Box2f getBdBox() const {
    Box2f res(m_bdbox);
    res.extend(m_bdBoxExt);
    return res;
  }

  //! sets the bdbox of the picture
  void setBdBox(Box2f const &box) {
    m_bdbox = box;
  }

  /** tries to convert the picture in a binary data :
   * - either a basic image/pict
   * - or an encrypted pict in ODG : "image/mwaw-odg"
   */
  virtual bool getBinary(RVNGBinaryData &, std::string &) const {
    return false;
  }

  /** \brief a virtual function used to obtain a strict order,
   * must be redefined in the subs class
   */
  virtual int cmp(MWAWPict const &a) const {
    // first compare the bdbox
    int diff = m_bdbox.cmp(a.m_bdbox);
    if (diff) return diff;
    // the type
    diff = getType() - a.getType();
    if (diff) return (diff < 0) ? -1 : 1;

    return 0;
  }
protected:
  //! computes the minimum and maximum of a list of point
  static Box2f getBdBox(int numPt, Vec2f const *pt) {
    if (numPt <= 0) {
      return Box2f();
    }

    float minV[2], maxV[2];
    for (int c = 0; c < 2; c++) minV[c] = maxV[c] = pt[0][c];

    for (int i = 1; i < numPt; i++) {
      for (int c = 0; c < 2; c++) {
        float v = pt[i][c];
        if (v < minV[c]) minV[c] = v;
        else if (v > maxV[c]) maxV[c] = v;
      }
    }

    return Box2f(Vec2f(minV[0], minV[1]), Vec2f(maxV[0], maxV[1]));
  }

  // a function to extend the bdbox

  //! udaptes the bdbox, by extended it by (val-previousVal)
  void extendBDBox(float val) {
    m_bdBoxExt = val;
  }

  //! protected constructor must not be called directly
  MWAWPict() : m_bdbox(), m_bdBoxExt (0.0) {}
  //! protected constructor must not be called directly
  MWAWPict(MWAWPict const &p) : m_bdbox(), m_bdBoxExt (0.0) {
    *this=p;
  }
  //! protected operator= must not be called directly
  MWAWPict &operator=(MWAWPict const &p) {
    if (&p == this) return *this;
    m_bdbox = p.m_bdbox;
    m_bdBoxExt = p.m_bdBoxExt;
    return *this;
  }

private:
  //! the bdbox (min and max pt)
  Box2f m_bdbox;
  //! the actual extension of the original box,
  float m_bdBoxExt;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
