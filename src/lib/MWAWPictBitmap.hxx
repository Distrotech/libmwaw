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

/* This header contains code specific to some bitmap
 */

#ifndef MWAW_PICT_BITMAP
#  define MWAW_PICT_BITMAP

#include <assert.h>

#include <vector>

#include "libmwaw_tools.hxx"
#include "MWAWPict.hxx"

class WPXBinaryData;

////////////////////////////////////////////////////////////
//
//   Some container
//
////////////////////////////////////////////////////////////

/** \brief a template class to store a 2D array of m_data */
template <class T> class MWAWPictBitmapContainer
{
public:
  //! constructor given size
  MWAWPictBitmapContainer(Vec2i const &size) : m_size(size), m_data(0L) {
    if (m_size[0]*m_size[1] != 0) m_data = new T[m_size[0]*m_size[1]];
  }
  //! destructor
  virtual ~MWAWPictBitmapContainer() {
    if (m_data) delete [] m_data;
  }

  //! returns ok, if the m_data is allocated
  bool ok() const {
    return (m_data != 0L);
  }

  //! return the array size
  Vec2i const &size() const {
    return m_size;
  }
  //! gets the number of row
  int numRows() const {
    return m_size[0];
  }
  //! gets the number of column
  int numColumns() const {
    return m_size[1];
  }

  //! accessor of a cell m_data
  T const &get(int i, int j) const {
    assert(m_data != 0L && i>=0 && i < m_size[0] && j>=0 && j < m_size[1]);
    return m_data[i+m_size[0]*j];
  }
  //! accessor of a row m_data
  T const *getRow(int j) const {
    assert(m_data != 0L && j>=0 && j < m_size[1]);
    return m_data+m_size[0]*j;
  }

  //! sets a cell m_data
  void set(int i, int j, T const &v) {
    assert(m_data != 0L && i>=0 && i < m_size[0] && j>=0 && j < m_size[1]);
    m_data[i+j*m_size[0]] = v;
  }

  //! sets a line of m_data
  template <class U>
  void setRow(int j, U const *val) {
    assert(m_data != 0L && j>=0 && j < m_size[1]);
    for (int i = 0, ind=j*m_size[0]; i < m_size[0]; i++, ind++) m_data[ind] = T(val[i]);
  }

  //! sets a column of m_data
  template <class U>
  void setColumn(int i, U const *val) {
    assert(m_data != 0L && i>=0 && i < m_size[0]);
    for (int j = 0, ind=i; j < m_size[1]; j++, ind+=m_size[0]) m_data[ind] = T(val[i]);
  }

private:
  MWAWPictBitmapContainer(MWAWPictBitmapContainer const &orig);
  MWAWPictBitmapContainer &operator=(MWAWPictBitmapContainer const &orig);
protected:
  //! the size
  Vec2i m_size;
  //! the m_data placed by row ie. d_00, d_10, ... , d_{X-1}0, ..
  T *m_data;
};

//! a bool container with a function to put packed row
class MWAWPictBitmapContainerBool : public MWAWPictBitmapContainer<bool>
{
public:
  //! constructor
  MWAWPictBitmapContainerBool(Vec2i const &size) : MWAWPictBitmapContainer<bool>(size) {}

  //! allows to use packed m_data
  void setRowPacked(int j, unsigned char const *val) {
    assert(m_data != 0L && j>=0 && j < m_size[1]);
    for (int i = 0, ind = j*m_size[0]; i < m_size[0]; ) {
      unsigned char v = *(val++);
      unsigned char mask = 0x80;
      for (int p = 0; p < 8 && i < m_size[0]; i++, p++, ind++) {
        m_data[ind] = ((v&mask) != 0);
        mask >>= 1;
      }
    }
  }
};

//! Generic class used to construct bitmap
class MWAWPictBitmap : public MWAWPict
{
public:
  //! the picture subtype: blackwhite, indexed, color
  enum SubType { BW, Indexed, Color };
  //! returns the picture type
  virtual Type getType() const {
    return MWAWPict::Bitmap;
  }
  //! returns the picture subtype
  virtual SubType getSubType() const = 0;

  //! returns the final WPXBinary data
  virtual bool getBinary(WPXBinaryData &res, std::string &s) const {
    if (!valid()) return false;

    s = "image/pict";
    createFileData(res);
    return true;
  }

  //! returns true if the picture is valid
  virtual bool valid() const {
    return false;
  }

  /** a virtual function used to obtain a strict order,
  must be redefined in the subs class */
  virtual int cmp(MWAWPict const &a) const {
    int diff = MWAWPict::cmp(a);
    if (diff) return diff;
    MWAWPictBitmap const &aPict = static_cast<MWAWPictBitmap const &>(a);

    // the type
    diff = getSubType() - aPict.getSubType();
    if (diff) return (diff < 0) ? -1 : 1;

    return 0;
  }

protected:
  //! abstract function which creates the result file
  virtual bool createFileData(WPXBinaryData &result) const = 0;

  //! protected constructor: use check to construct a picture
  MWAWPictBitmap(Vec2i const &sz) {
    setBdBox(Box2f(Vec2f(0,0), sz));
  }
};

/** a bitmap of bool to store black-white bitmap */
class MWAWPictBitmapBW : public MWAWPictBitmap
{
public:
  //! returns the picture subtype
  virtual SubType getSubType() const {
    return BW;
  }

  /** a virtual function used to obtain a strict order,
  must be redefined in the subs class */
  virtual int cmp(MWAWPict const &a) const {
    int diff = MWAWPictBitmap::cmp(a);
    if (diff) return diff;
    MWAWPictBitmapBW const &aPict = static_cast<MWAWPictBitmapBW const &>(a);

    diff=m_data.size().cmpY(aPict.m_data.size());
    if (diff) return diff;

    long diffL = (long) this -(long) &aPict;
    if (diffL) return (diffL<0) ? -1 : 1;

    return 0;
  }

  //! returns true if the picture is valid
  virtual bool valid() const {
    return m_data.ok();
  }

  //! the constructor
  MWAWPictBitmapBW(Vec2i const &sz) : MWAWPictBitmap(sz), m_data(sz) { }

  //! the picture size
  Vec2i const &size() const {
    return m_data.size();
  }
  //! the number of rows
  int numRows() const {
    return m_data.numRows();
  }
  //! the number of columns
  int numColumns() const {
    return m_data.numColumns();
  }
  //! returns a cell content
  bool get(int i, int j) const {
    return m_data.get(i,j);
  }
  //! returns the cells content of a row
  bool const *getRow(int j) const {
    return m_data.getRow(j);
  }
  //! sets a cell contents
  void set(int i, int j, bool v) {
    m_data.set(i,j, v);
  }
  //! sets all cell contents of a row
  void setRow(int j, bool const *val) {
    m_data.setRow(j, val);
  }
  //! sets all cell contents of a row given packed m_data
  void setRowPacked(int j, unsigned char const *val) {
    m_data.setRowPacked(j, val);
  }
  //! sets all cell contents of a column
  void setColumn(int i, bool const *val) {
    m_data.setColumn(i, val);
  }

protected:
  //! function which creates the result file
  virtual bool createFileData(WPXBinaryData &result) const;

  //! the data
  MWAWPictBitmapContainerBool m_data;
};

/** a bitmap of int to store indexed bitmap */
class MWAWPictBitmapIndexed : public MWAWPictBitmap
{
public:
  //! return the picture subtype
  virtual SubType getSubType() const {
    return Indexed;
  }

  /** a virtual function used to obtain a strict order,
  must be redefined in the subs class */
  virtual int cmp(MWAWPict const &a) const {
    int diff = MWAWPictBitmap::cmp(a);
    if (diff) return diff;
    MWAWPictBitmapIndexed const &aPict = static_cast<MWAWPictBitmapIndexed const &>(a);

    diff=m_data.size().cmpY(aPict.m_data.size());
    if (diff) return diff;

    diff=m_colors.size()-aPict.m_colors.size();
    if (diff) return (diff < 0) ? -1 : 1;

    long diffL = (long) this -(long) &aPict;
    if (diffL) return (diffL<0) ? -1 : 1;

    return 0;
  }

  //! returns true if the picture is valid
  virtual bool valid() const {
    return m_data.ok();
  }

  //! the constructor
  MWAWPictBitmapIndexed(Vec2i const &sz) : MWAWPictBitmap(sz), m_data(sz), m_colors() { }

  //! the picture size
  Vec2i const &size() const {
    return m_data.size();
  }
  //! the number of rows
  int numRows() const {
    return m_data.numRows();
  }
  //! the number of columns
  int numColumns() const {
    return m_data.numColumns();
  }
  //! returns a cell content
  int get(int i, int j) const {
    return m_data.get(i,j);
  }
  //! returns the cells content of a row
  int const *getRow(int j) const {
    return m_data.getRow(j);
  }

  //! sets a cell contents
  void set(int i, int j, int v) {
    m_data.set(i,j, v);
  }
  //! sets all cell contents of a row
  template <class U> void setRow(int j, U const *val) {
    m_data.setRow(j, val);
  }
  //! sets all cell contents of a column
  template <class U> void setColumn(int i, U const *val) {
    m_data.setColumn(i, val);
  }

  //! returns the array of indexed colors
  std::vector<Vec3uc> const &getColors() const {
    return m_colors;
  }
  //! sets the array of indexed colors
  void setColors(std::vector<Vec3uc> const &cols) {
    m_colors = cols;
  }

protected:
  //! the function which creates the result file
  virtual bool createFileData(WPXBinaryData &result) const;

  //! the m_data
  MWAWPictBitmapContainer<int> m_data;
  //! the colors
  std::vector<Vec3uc> m_colors;
};

/** a bitmap of Vec3u to store true color bitmap */
class MWAWPictBitmapColor : public MWAWPictBitmap
{
public:
  //! return the picture subtype
  virtual SubType getSubType() const {
    return Indexed;
  }

  /** a virtual function used to obtain a strict order,
  must be redefined in the subs class */
  virtual int cmp(MWAWPict const &a) const {
    int diff = MWAWPictBitmap::cmp(a);
    if (diff) return diff;
    MWAWPictBitmapColor const &aPict = static_cast<MWAWPictBitmapColor const &>(a);

    diff=m_data.size().cmpY(aPict.m_data.size());
    if (diff) return diff;

    long diffL = (long) this -(long) &aPict;
    if (diffL) return (diffL<0) ? -1 : 1;

    return 0;
  }

  //! returns true if the picture is valid
  virtual bool valid() const {
    return m_data.ok();
  }

  //! the constructor
  MWAWPictBitmapColor(Vec2i const &sz) : MWAWPictBitmap(sz), m_data(sz) { }

  //! the picture size
  Vec2i const &size() const {
    return m_data.size();
  }
  //! the number of rows
  int numRows() const {
    return m_data.numRows();
  }
  //! the number of columns
  int numColumns() const {
    return m_data.numColumns();
  }
  //! returns a cell content
  Vec3uc get(int i, int j) const {
    return m_data.get(i,j);
  }
  //! returns the cells content of a row
  Vec3uc const *getRow(int j) const {
    return m_data.getRow(j);
  }

  //! sets a cell contents
  void set(int i, int j, Vec3uc const &v) {
    m_data.set(i,j, v);
  }
  //! sets all cell contents of a row
  void setRow(int j, Vec3uc const *val) {
    m_data.setRow(j, val);
  }
  //! sets all cell contents of a column
  void setColumn(int i, Vec3uc const *val) {
    m_data.setColumn(i, val);
  }

protected:
  //! the function which creates the result file
  virtual bool createFileData(WPXBinaryData &result) const;

  //! the data
  MWAWPictBitmapContainer<Vec3uc> m_data;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
