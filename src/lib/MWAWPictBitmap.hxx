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

/* This header contains code specific to some bitmap
 */

#ifndef MWAW_PICT_BITMAP
#  define MWAW_PICT_BITMAP

#include <assert.h>

#include <vector>

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"
#include "MWAWPict.hxx"

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
  MWAWPictBitmapContainer(Vec2i const &sz) : m_size(sz), m_data(0L)
  {
    if (m_size[0]*m_size[1] == 0) return;
    m_data = new T[size_t(m_size[0]*m_size[1])];
    std::uninitialized_fill_n(m_data, m_size[0] * m_size[1], T());
  }
  //! destructor
  virtual ~MWAWPictBitmapContainer()
  {
    if (m_data) delete [] m_data;
  }

  //! returns ok, if the m_data is allocated
  bool ok() const
  {
    return (m_data != 0L);
  }

  //! a comparison operator
  int cmp(MWAWPictBitmapContainer<T> const &orig) const
  {
    int diff = m_size.cmpY(orig.m_size);
    if (diff) return diff;
    if (!m_data) return orig.m_data ? 1 : 0;
    if (!orig.m_data) return -1;
    for (int i=0; i < m_size[0]*m_size[1]; i++) {
      if (m_data[i] < orig.m_data[i]) return -1;
      if (m_data[i] > orig.m_data[i]) return 1;
    }
    return 0;
  }
  //! return the array size
  Vec2i const &size() const
  {
    return m_size;
  }
  //! gets the number of row
  int numRows() const
  {
    return m_size[0];
  }
  //! gets the number of column
  int numColumns() const
  {
    return m_size[1];
  }

  //! accessor of a cell m_data
  T const &get(int i, int j) const
  {
    if (m_data == 0L || i<0 || i >= m_size[0] || j<0 || j >= m_size[1])
      throw libmwaw::GenericException();
    return m_data[i+m_size[0]*j];
  }
  //! accessor of a row m_data
  T const *getRow(int j) const
  {
    if (m_data == 0L || j<0 || j >= m_size[1])
      throw libmwaw::GenericException();
    return m_data+m_size[0]*j;
  }

  //! sets a cell m_data
  void set(int i, int j, T const &v)
  {
    if (m_data == 0L || i<0 || i >= m_size[0] || j<0 || j >= m_size[1]) {
      MWAW_DEBUG_MSG(("MWAWPictBitmapContainer::set: call with bad coordinate %d %d\n", i, j));
      return;
    }
    m_data[i+j*m_size[0]] = v;
  }

  //! sets a line of m_data
  template <class U>
  void setRow(int j, U const *val)
  {
    if (m_data == 0L || j<0 || j >= m_size[1]) {
      MWAW_DEBUG_MSG(("MWAWPictBitmapContainer::setRow: call with bad coordinate %d\n", j));
      return;
    }
    for (int i = 0, ind=j*m_size[0]; i < m_size[0]; i++, ind++) m_data[ind] = T(val[i]);
  }

  //! sets a column of m_data
  template <class U>
  void setColumn(int i, U const *val)
  {
    if (m_data == 0L || i<0 || i >= m_size[0]) {
      MWAW_DEBUG_MSG(("MWAWPictBitmapContainer::setColumn: call with bad coordinate %d\n", i));
      return;
    }
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
  MWAWPictBitmapContainerBool(Vec2i const &sz) : MWAWPictBitmapContainer<bool>(sz) {}

  //! a comparison operator
  int cmp(MWAWPictBitmapContainerBool const &orig) const
  {
    int diff = m_size.cmpY(orig.m_size);
    if (diff) return diff;
    if (!m_data) return orig.m_data ? 1 : 0;
    if (!orig.m_data) return -1;
    for (int i=0; i < m_size[0]*m_size[1]; i++) {
      if (m_data[i] == orig.m_data[i]) continue;
      return m_data[i] ? 1 : -1;
    }
    return 0;
  }

  //! allows to use packed m_data
  void setRowPacked(int j, unsigned char const *val)
  {
    if (m_data == 0L || j<0 || j >= m_size[1]) {
      MWAW_DEBUG_MSG(("MWAWPictBitmapContainerBool::setRowPacked: call with bad coordinate %d %d\n", j));
      return;
    }
    for (int i = 0, ind = j*m_size[0]; i < m_size[0];) {
      unsigned char v = *(val++);
      unsigned char mask = 0x80;
      for (int p = 0; p < 8 && i < m_size[0]; i++, p++, ind++) {
        m_data[ind] = ((v&mask) != 0);
        mask = (unsigned char)(mask >> 1);
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
  virtual Type getType() const
  {
    return MWAWPict::Bitmap;
  }
  //! returns the picture subtype
  virtual SubType getSubType() const = 0;

  //! returns the final librevenge::RVNGBinary data
  virtual bool getBinary(librevenge::RVNGBinaryData &res, std::string &s) const
  {
    if (!valid()) return false;

    s = "image/pict";
    createFileData(res);
    return true;
  }

  //! returns true if the picture is valid
  virtual bool valid() const
  {
    return false;
  }

  /** a virtual function used to obtain a strict order,
  must be redefined in the subs class */
  virtual int cmp(MWAWPict const &a) const
  {
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
  virtual bool createFileData(librevenge::RVNGBinaryData &result) const = 0;

  //! protected constructor: use check to construct a picture
  MWAWPictBitmap(Vec2i const &sz)
  {
    setBdBox(Box2f(Vec2f(0,0), sz));
  }
};

/** a bitmap of bool to store black-white bitmap */
class MWAWPictBitmapBW : public MWAWPictBitmap
{
public:
  //! returns the picture subtype
  virtual SubType getSubType() const
  {
    return BW;
  }

  /** a virtual function used to obtain a strict order,
  must be redefined in the subs class */
  virtual int cmp(MWAWPict const &a) const
  {
    int diff = MWAWPictBitmap::cmp(a);
    if (diff) return diff;
    MWAWPictBitmapBW const &aPict = static_cast<MWAWPictBitmapBW const &>(a);

    return m_data.cmp(aPict.m_data);
  }

  //! returns true if the picture is valid
  virtual bool valid() const
  {
    return m_data.ok();
  }

  //! the constructor
  MWAWPictBitmapBW(Vec2i const &sz) : MWAWPictBitmap(sz), m_data(sz) { }

  //! the picture size
  Vec2i const &size() const
  {
    return m_data.size();
  }
  //! the number of rows
  int numRows() const
  {
    return m_data.numRows();
  }
  //! the number of columns
  int numColumns() const
  {
    return m_data.numColumns();
  }
  //! returns a cell content
  bool get(int i, int j) const
  {
    return m_data.get(i,j);
  }
  //! returns the cells content of a row
  bool const *getRow(int j) const
  {
    return m_data.getRow(j);
  }
  //! sets a cell contents
  void set(int i, int j, bool v)
  {
    m_data.set(i,j, v);
  }
  //! sets all cell contents of a row
  void setRow(int j, bool const *val)
  {
    m_data.setRow(j, val);
  }
  //! sets all cell contents of a row given packed m_data
  void setRowPacked(int j, unsigned char const *val)
  {
    m_data.setRowPacked(j, val);
  }
  //! sets all cell contents of a column
  void setColumn(int i, bool const *val)
  {
    m_data.setColumn(i, val);
  }

protected:
  //! function which creates the result file
  virtual bool createFileData(librevenge::RVNGBinaryData &result) const;

  //! the data
  MWAWPictBitmapContainerBool m_data;
};

/** a bitmap of int to store indexed bitmap */
class MWAWPictBitmapIndexed : public MWAWPictBitmap
{
public:
  //! return the picture subtype
  virtual SubType getSubType() const
  {
    return Indexed;
  }

  /** a virtual function used to obtain a strict order,
  must be redefined in the subs class */
  virtual int cmp(MWAWPict const &a) const
  {
    int diff = MWAWPictBitmap::cmp(a);
    if (diff) return diff;
    MWAWPictBitmapIndexed const &aPict = static_cast<MWAWPictBitmapIndexed const &>(a);

    diff=int(m_colors.size())-int(aPict.m_colors.size());
    if (diff) return (diff < 0) ? -1 : 1;
    for (size_t c=0; c < m_colors.size(); c++) {
      if (m_colors[c] < aPict.m_colors[c])
        return 1;
      if (m_colors[c] > aPict.m_colors[c])
        return -1;
    }
    return m_data.cmp(aPict.m_data);
  }

  //! returns true if the picture is valid
  virtual bool valid() const
  {
    return m_data.ok();
  }

  //! the constructor
  MWAWPictBitmapIndexed(Vec2i const &sz) : MWAWPictBitmap(sz), m_data(sz), m_colors() { }

  //! the picture size
  Vec2i const &size() const
  {
    return m_data.size();
  }
  //! the number of rows
  int numRows() const
  {
    return m_data.numRows();
  }
  //! the number of columns
  int numColumns() const
  {
    return m_data.numColumns();
  }
  //! returns a cell content
  int get(int i, int j) const
  {
    return m_data.get(i,j);
  }
  //! returns the cells content of a row
  int const *getRow(int j) const
  {
    return m_data.getRow(j);
  }

  //! sets a cell contents
  void set(int i, int j, int v)
  {
    m_data.set(i,j, v);
  }
  //! sets all cell contents of a row
  template <class U> void setRow(int j, U const *val)
  {
    m_data.setRow(j, val);
  }
  //! sets all cell contents of a column
  template <class U> void setColumn(int i, U const *val)
  {
    m_data.setColumn(i, val);
  }

  //! returns the array of indexed colors
  std::vector<MWAWColor> const &getColors() const
  {
    return m_colors;
  }
  //! sets the array of indexed colors
  void setColors(std::vector<MWAWColor> const &cols)
  {
    m_colors = cols;
  }

protected:
  //! the function which creates the result file
  virtual bool createFileData(librevenge::RVNGBinaryData &result) const;

  //! the m_data
  MWAWPictBitmapContainer<int> m_data;
  //! the colors
  std::vector<MWAWColor> m_colors;
};

/** a bitmap of MWAWColor to store true color bitmap

    \note: this class is actually the only class which can create
    bitmap with transparency (by creating a BMP), but as
    LibreOffice/OpenOffice seem to ignore the alpha channel when
    importing BMP pictures...
 */
class MWAWPictBitmapColor : public MWAWPictBitmap
{
public:
  //! return the picture subtype
  virtual SubType getSubType() const
  {
    return Indexed;
  }

  /** a virtual function used to obtain a strict order,
  must be redefined in the subs class */
  virtual int cmp(MWAWPict const &a) const
  {
    int diff = MWAWPictBitmap::cmp(a);
    if (diff) return diff;
    MWAWPictBitmapColor const &aPict = static_cast<MWAWPictBitmapColor const &>(a);

    return m_data.cmp(aPict.m_data);
  }

  //! returns true if the picture is valid
  virtual bool valid() const
  {
    return m_data.ok();
  }

  //! the constructor
  MWAWPictBitmapColor(Vec2i const &sz, bool useAlphaChannel=false) : MWAWPictBitmap(sz), m_data(sz), m_hasAlpha(useAlphaChannel) { }

  //! the picture size
  Vec2i const &size() const
  {
    return m_data.size();
  }
  //! the number of rows
  int numRows() const
  {
    return m_data.numRows();
  }
  //! the number of columns
  int numColumns() const
  {
    return m_data.numColumns();
  }
  //! returns a cell content
  MWAWColor get(int i, int j) const
  {
    return m_data.get(i,j);
  }
  //! returns the cells content of a row
  MWAWColor const *getRow(int j) const
  {
    return m_data.getRow(j);
  }

  //! sets a cell contents
  void set(int i, int j, MWAWColor const &v)
  {
    m_data.set(i,j, v);
  }
  //! sets all cell contents of a row
  void setRow(int j, MWAWColor const *val)
  {
    m_data.setRow(j, val);
  }
  //! sets all cell contents of a column
  void setColumn(int i, MWAWColor const *val)
  {
    m_data.setColumn(i, val);
  }

protected:
  //! the function which creates the result file
  virtual bool createFileData(librevenge::RVNGBinaryData &result) const;

  //! the data
  MWAWPictBitmapContainer<MWAWColor> m_data;

  //! true if the bitmap has alpha color
  bool m_hasAlpha;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
