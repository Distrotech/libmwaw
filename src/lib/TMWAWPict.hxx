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

/*
 * This header contains code specific to manage basic picture (line, rectangle, ...)
 *
 * Note: generic class for all sort of pict
 *      - PictBasic: regroup basic pict shape (line, circle, ...)
 *      - PictBitmap: regroup some classes used to store bitmap
 *      - PictData: regroup the data which can be read via a WPXBinaryData
 */

#ifndef MWAW_PICT
#  define MWAW_PICT

#  include <assert.h>
#  include <ostream>
#  include <vector>

#  include "libmwaw_tools.hxx"

class WPXBinaryData;

namespace libmwaw_tools
{
/** \brief Generic function used to define/store a picture */
class Pict
{
public:
  //! virtual destructor
  virtual ~Pict() {}

  /*! \enum Type
   * \brief the different picture types:
   *      - basic: line, rectangle,
   *      - pictData: a classic format of file (Apple© Pict, ...)
   *      - bitmap: a image
   *      - OleContainer: simple container to an ole object
   *      - ...
   */
  enum Type { Basic, PictData, Bitmap, OleContainer, Unknown };
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
  virtual bool getBinary(WPXBinaryData &, std::string &) const {
    return false;
  }

  /** \brief a virtual function used to obtain a strict order,
   * must be redefined in the subs class
   */
  virtual int cmp(Pict const &a) const {
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
  Pict() : m_bdbox(), m_bdBoxExt (0.0) {}
  //! protected constructor must not be called directly
  Pict(Pict const &p) : m_bdbox(), m_bdBoxExt (0.0) {
    *this=p;
  }
  //! protected operator= must not be called directly
  Pict &operator=(Pict const &p) {
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

}

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
