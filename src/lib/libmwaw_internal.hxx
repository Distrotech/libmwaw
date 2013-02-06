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

#ifndef LIBMWAW_INTERNAL_H
#define LIBMWAW_INTERNAL_H
#include <assert.h>
#ifdef DEBUG
#include <stdio.h>
#endif

#include <map>
#include <ostream>
#include <string>
#include <vector>

#include <libwpd-stream/libwpd-stream.h>
#include <libwpd/libwpd.h>

#if defined(_MSC_VER) || defined(__DJGPP__)
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
#else /* !_MSC_VER && !__DJGPP__*/
#include <inttypes.h>
#endif /* _MSC_VER || __DJGPP__ */

/* ---------- memory  --------------- */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(SHAREDPTR_TR1)
#include <tr1/memory>
using std::tr1::shared_ptr;
#elif defined(SHAREDPTR_STD)
#include <memory>
using std::shared_ptr;
#else
#include <boost/shared_ptr.hpp>
using boost::shared_ptr;
#endif

/** an noop deleter used to transform a libwpd pointer in a false shared_ptr */
template <class T>
struct MWAW_shared_ptr_noop_deleter {
  void operator() (T *) {}
};

/* ---------- debug  --------------- */
#ifdef DEBUG
#define MWAW_DEBUG_MSG(M) printf M
#else
#define MWAW_DEBUG_MSG(M)
#endif

namespace libmwaw
{
// Various exceptions:
class VersionException
{
};

class FileException
{
};

class ParseException
{
};

class GenericException
{
};

class WrongPasswordException
{
};
}

/* ---------- input ----------------- */
namespace libmwaw
{
uint8_t readU8(WPXInputStream *input);
}

/* ---------- small enum/class ------------- */
//! the class to store a color
struct MWAWColor {
  //! constructor
  MWAWColor(uint32_t argb=0) : m_value(argb) {
  }
  //! constructor from color
  MWAWColor(unsigned char r, unsigned char g,  unsigned char b, unsigned char a=0) :
    m_value(uint32_t((a<<24)+(r<<16)+(g<<8)+b)) {
  }
  //! operator=
  MWAWColor &operator=(uint32_t argb) {
    m_value = argb;
    return *this;
  }
  //! return the back color
  static MWAWColor black() {
    return MWAWColor(0);
  }
  //! return the white color
  static MWAWColor white() {
    return MWAWColor(0xFFFFFF);
  }

  //! return alpha*colA+beta*colB
  static MWAWColor barycenter(float alpha, MWAWColor const colA,
                              float beta, MWAWColor const colB);
  //! return the rgba value
  uint32_t value() const {
    return m_value;
  }
  //! return true if the color is black
  bool isBlack() const {
    return (m_value&0xFFFFFF)==0;
  }
  //! return true if the color is white
  bool isWhite() const {
    return (m_value&0xFFFFFF)==0xFFFFFF;
  }
  //! operator==
  bool operator==(MWAWColor const &c) const {
    return (c.m_value&0xFFFFFF)==(m_value&0xFFFFFF);
  }
  //! operator!=
  bool operator!=(MWAWColor const &c) const {
    return !operator==(c);
  }
  //! operator<
  bool operator<(MWAWColor const &c) const {
    return (c.m_value&0xFFFFFF)<(m_value&0xFFFFFF);
  }
  //! operator<=
  bool operator<=(MWAWColor const &c) const {
    return (c.m_value&0xFFFFFF)<=(m_value&0xFFFFFF);
  }
  //! operator>
  bool operator>(MWAWColor const &c) const {
    return !operator<=(c);
  }
  //! operator>=
  bool operator>=(MWAWColor const &c) const {
    return !operator<(c);
  }
  //! operator<< in the form \#rrggbb
  friend std::ostream &operator<< (std::ostream &o, MWAWColor const &c);
  //! print the color in the form \#rrggbb
  std::string str() const;
protected:
  //! the argb color
  uint32_t m_value;
};

struct MWAWColumnDefinition {
  MWAWColumnDefinition() : m_width(0), m_leftGutter(0), m_rightGutter(0) {}
  double m_width;
  double m_leftGutter;
  double m_rightGutter;
};

struct MWAWColumnProperties {
  MWAWColumnProperties() : m_attributes(0), m_alignment(0) {}
  uint32_t m_attributes;
  uint8_t m_alignment;
};

//! a border list
struct MWAWBorder {
  enum Style { None, Single, Double, Dot, LargeDot, Dash };
  enum Pos { Left = 0, Right = 1, Top = 2, Bottom = 3, HMiddle = 4, VMiddle = 5 };
  enum { LeftBit = 0x01,  RightBit = 0x02, TopBit=0x4, BottomBit = 0x08, HMiddleBit = 0x10, VMiddleBit = 0x20 };

  //! constructor
  MWAWBorder() : m_style(Single), m_width(1), m_color(MWAWColor::black()) { }
  //! return the properties
  std::string getPropertyValue() const;

  //! operator==
  bool operator==(MWAWBorder const &orig) const {
    return m_style == orig.m_style && m_width == orig.m_width
           && m_color == orig.m_color;
  }
  //! operator!=
  bool operator!=(MWAWBorder const &orig) const {
    return !operator==(orig);
  }
  //! compare two cell
  int compare(MWAWBorder const &orig) const;

  //! operator<<: prints data in form "XxY"
  friend std::ostream &operator<< (std::ostream &o, MWAWBorder const &border);
  //! operator<<: prints data in form "none|dot|..."
  friend std::ostream &operator<< (std::ostream &o, MWAWBorder::Style const &style);
  //! the border style
  Style m_style;
  //! the border width
  int m_width;
  //! the border color
  MWAWColor m_color;

};

namespace libmwaw
{
enum NumberingType { NONE, BULLET, ARABIC, LOWERCASE, UPPERCASE, LOWERCASE_ROMAN, UPPERCASE_ROMAN };
std::string numberingTypeToString(NumberingType type);
std::string numberingValueToString(NumberingType type, int value);
enum SubDocumentType { DOC_NONE, DOC_HEADER_FOOTER, DOC_NOTE, DOC_TABLE, DOC_TEXT_BOX, DOC_COMMENT_ANNOTATION };
}

// Generic bits
#define MWAW_LEFT 0x00
#define MWAW_RIGHT 0x01
#define MWAW_CENTER 0x02
#define MWAW_TOP 0x03
#define MWAW_BOTTOM 0x04

//! a generic variable template: value + flag to know if the variable is set
template <class T> struct Variable {
  Variable() : m_data(), m_set(false) {}
  Variable(T def) : m_data(def), m_set(false) {}
  Variable(Variable const &orig) : m_data(orig.m_data), m_set(orig.m_set) {}
  Variable &operator=(Variable const &orig) {
    if (this != &orig) {
      m_data = orig.m_data;
      m_set = orig.m_set;
    }
    return *this;
  }
  Variable &operator=(T val) {
    m_data = val;
    m_set = true;
    return *this;
  }
  void insert(Variable const &orig) {
    if (orig.m_set) {
      m_data = orig.m_data;
      m_set = orig.m_set;
    }
  }
  T const *operator->() const {
    return &m_data;
  }
  T *operator->() {
    m_set = true;
    return &m_data;
  }
  T const &operator*() const {
    return m_data;
  }
  T &operator*() {
    m_set = true;
    return m_data;
  }
  T const &get() const {
    return m_data;
  }
  bool isSet() const {
    return m_set;
  }
  void setSet(bool newVal) {
    m_set=newVal;
  }
protected:
  T m_data;
  bool m_set;
};

/* ---------- vec2/box2f ------------- */
/*! \class Vec2
 *   \brief small class which defines a vector with 2 elements
 */
template <class T> class Vec2
{
public:
  //! constructor
  Vec2(T xx=0,T yy=0) : m_x(xx), m_y(yy) { }
  //! generic copy constructor
  template <class U> Vec2(Vec2<U> const &p) : m_x(T(p.x())), m_y(T(p.y())) {}

  //! first element
  T x() const {
    return m_x;
  }
  //! second element
  T y() const {
    return m_y;
  }
  //! operator[]
  T operator[](int c) const {
    assert(c >= 0 && c <= 1);
    return (c==0) ? m_x : m_y;
  }
  //! operator[]
  T &operator[](int c) {
    assert(c >= 0 && c <= 1);
    return (c==0) ? m_x : m_y;
  }

  //! resets the two elements
  void set(T xx, T yy) {
    m_x = xx;
    m_y = yy;
  }
  //! resets the first element
  void setX(T xx) {
    m_x = xx;
  }
  //! resets the second element
  void setY(T yy) {
    m_y = yy;
  }

  //! increases the actuals values by \a dx and \a dy
  void add(T dx, T dy) {
    m_x += dx;
    m_y += dy;
  }

  //! operator+=
  Vec2<T> &operator+=(Vec2<T> const &p) {
    m_x += p.m_x;
    m_y += p.m_y;
    return *this;
  }
  //! operator-=
  Vec2<T> &operator-=(Vec2<T> const &p) {
    m_x -= p.m_x;
    m_y -= p.m_y;
    return *this;
  }
  //! generic operator*=
  template <class U>
  Vec2<T> &operator*=(U scale) {
    m_x = T(m_x*scale);
    m_y = T(m_y*scale);
    return *this;
  }

  //! operator+
  friend Vec2<T> operator+(Vec2<T> const &p1, Vec2<T> const &p2) {
    Vec2<T> p(p1);
    return p+=p2;
  }
  //! operator-
  friend Vec2<T> operator-(Vec2<T> const &p1, Vec2<T> const &p2) {
    Vec2<T> p(p1);
    return p-=p2;
  }
  //! generic operator*
  template <class U>
  friend Vec2<T> operator*(U scale, Vec2<T> const &p1) {
    Vec2<T> p(p1);
    return p *= scale;
  }

  //! comparison==
  bool operator==(Vec2<T> const &p) const {
    return cmpY(p) == 0;
  }
  //! comparison!=
  bool operator!=(Vec2<T> const &p) const {
    return cmpY(p) != 0;
  }
  //! comparison<: sort by y
  bool operator<(Vec2<T> const &p) const {
    return cmpY(p) < 0;
  }
  //! a comparison function: which first compares x then y
  int cmp(Vec2<T> const &p) const {
    T diff  = m_x-p.m_x;
    if (diff < 0) return -1;
    if (diff > 0) return 1;
    diff = m_y-p.m_y;
    if (diff < 0) return -1;
    if (diff > 0) return 1;
    return 0;
  }
  //! a comparison function: which first compares y then x
  int cmpY(Vec2<T> const &p) const {
    T diff  = m_y-p.m_y;
    if (diff < 0) return -1;
    if (diff > 0) return 1;
    diff = m_x-p.m_x;
    if (diff < 0) return -1;
    if (diff > 0) return 1;
    return 0;
  }

  //! operator<<: prints data in form "XxY"
  friend std::ostream &operator<< (std::ostream &o, Vec2<T> const &f) {
    o << f.m_x << "x" << f.m_y;
    return o;
  }

  /*! \struct PosSizeLtX
   * \brief internal struct used to create sorted map, sorted by X
   */
  struct PosSizeLtX {
    //! comparaison function
    bool operator()(Vec2<T> const &s1, Vec2<T> const &s2) const {
      return s1.cmp(s2) < 0;
    }
  };
  /*! \typedef MapX
   *  \brief map of Vec2
   */
  typedef std::map<Vec2<T>, T,struct PosSizeLtX> MapX;

  /*! \struct PosSizeLtY
   * \brief internal struct used to create sorted map, sorted by Y
   */
  struct PosSizeLtY {
    //! comparaison function
    bool operator()(Vec2<T> const &s1, Vec2<T> const &s2) const {
      return s1.cmpY(s2) < 0;
    }
  };
  /*! \typedef MapY
   *  \brief map of Vec2
   */
  typedef std::map<Vec2<T>, T,struct PosSizeLtY> MapY;
protected:
  T m_x/*! \brief first element */, m_y/*! \brief second element */;
};

/*! \brief Vec2 of bool */
typedef Vec2<bool> Vec2b;
/*! \brief Vec2 of int */
typedef Vec2<int> Vec2i;
/*! \brief Vec2 of long */
typedef Vec2<long> Vec2l;
/*! \brief Vec2 of float */
typedef Vec2<float> Vec2f;

/*! \class Vec3
 *   \brief small class which defines a vector with 3 elements
 */
template <class T> class Vec3
{
public:
  //! constructor
  Vec3(T xx=0,T yy=0,T zz=0) {
    m_val[0] = xx;
    m_val[1] = yy;
    m_val[2] = zz;
  }
  //! generic copy constructor
  template <class U> Vec3(Vec3<U> const &p) {
    for (int c = 0; c < 3; c++) m_val[c] = T(p[c]);
  }

  //! first element
  T x() const {
    return m_val[0];
  }
  //! second element
  T y() const {
    return m_val[1];
  }
  //! third element
  T z() const {
    return m_val[2];
  }
  //! operator[]
  T operator[](int c) const {
    assert(c >= 0 && c <= 2);
    return m_val[c];
  }
  //! operator[]
  T &operator[](int c) {
    assert(c >= 0 && c <= 2);
    return m_val[c];
  }

  //! resets the three elements
  void set(T xx, T yy, T zz) {
    m_val[0] = xx;
    m_val[1] = yy;
    m_val[2] = zz;
  }
  //! resets the first element
  void setX(T xx) {
    m_val[0] = xx;
  }
  //! resets the second element
  void setY(T yy) {
    m_val[1] = yy;
  }
  //! resets the third element
  void setZ(T zz) {
    m_val[2] = zz;
  }

  //! increases the actuals values by \a dx, \a dy, \a dz
  void add(T dx, T dy, T dz) {
    m_val[0] += dx;
    m_val[1] += dy;
    m_val[2] += dz;
  }

  //! operator+=
  Vec3<T> &operator+=(Vec3<T> const &p) {
    for (int c = 0; c < 3; c++) m_val[c] = T(m_val[c]+p.m_val[c]);
    return *this;
  }
  //! operator-=
  Vec3<T> &operator-=(Vec3<T> const &p) {
    for (int c = 0; c < 3; c++) m_val[c] = T(m_val[c]-p.m_val[c]);
    return *this;
  }
  //! generic operator*=
  template <class U>
  Vec3<T> &operator*=(U scale) {
    for (int c = 0; c < 3; c++) m_val[c] = T(m_val[c]*scale);
    return *this;
  }

  //! operator+
  friend Vec3<T> operator+(Vec3<T> const &p1, Vec3<T> const &p2) {
    Vec3<T> p(p1);
    return p+=p2;
  }
  //! operator-
  friend Vec3<T> operator-(Vec3<T> const &p1, Vec3<T> const &p2) {
    Vec3<T> p(p1);
    return p-=p2;
  }
  //! generic operator*
  template <class U>
  friend Vec3<T> operator*(U scale, Vec3<T> const &p1) {
    Vec3<T> p(p1);
    return p *= scale;
  }

  //! comparison==
  bool operator==(Vec3<T> const &p) const {
    return cmp(p) == 0;
  }
  //! comparison!=
  bool operator!=(Vec3<T> const &p) const {
    return cmp(p) != 0;
  }
  //! comparison<: which first compares x values, then y values then z values.
  bool operator<(Vec3<T> const &p) const {
    return cmp(p) < 0;
  }
  //! a comparison function: which first compares x values, then y values then z values.
  int cmp(Vec3<T> const &p) const {
    for (int c = 0; c < 3; c++) {
      T diff  = m_val[c]-p.m_val[c];
      if (diff) return (diff < 0) ? -1 : 1;
    }
    return 0;
  }

  //! operator<<: prints data in form "XxYxZ"
  friend std::ostream &operator<< (std::ostream &o, Vec3<T> const &f) {
    o << f.m_val[0] << "x" << f.m_val[1] << "x" << f.m_val[2];
    return o;
  }

  /*! \struct PosSizeLt
   * \brief internal struct used to create sorted map, sorted by X, Y, Z
   */
  struct PosSizeLt {
    //! comparaison function
    bool operator()(Vec3<T> const &s1, Vec3<T> const &s2) const {
      return s1.cmp(s2) < 0;
    }
  };
  /*! \typedef Map
   *  \brief map of Vec3
   */
  typedef std::map<Vec3<T>, T,struct PosSizeLt> Map;

protected:
  //! the values
  T m_val[3];
};

/*! \brief Vec3 of unsigned char */
typedef Vec3<unsigned char> Vec3uc;
/*! \brief Vec3 of int */
typedef Vec3<int> Vec3i;
/*! \brief Vec3 of float */
typedef Vec3<float> Vec3f;

/*! \class Box2
 *   \brief small class which defines a 2D Box
 */
template <class T> class Box2
{
public:
  //! constructor
  Box2(Vec2<T> minPt=Vec2<T>(), Vec2<T> maxPt=Vec2<T>()) {
    m_pt[0] = minPt;
    m_pt[1] = maxPt;
  }
  //! generic constructor
  template <class U> Box2(Box2<U> const &p) {
    for (int c=0; c < 2; c++) m_pt[c] = p[c];
  }

  //! the minimum 2D point (in x and in y)
  Vec2<T> const &min() const {
    return m_pt[0];
  }
  //! the maximum 2D point (in x and in y)
  Vec2<T> const &max() const {
    return m_pt[1];
  }
  //! the minimum 2D point (in x and in y)
  Vec2<T> &min() {
    return m_pt[0];
  }
  //! the maximum 2D point (in x and in y)
  Vec2<T> &max() {
    return m_pt[1];
  }
  /*! \brief the two extremum points which defined the box
   * \param c value 0 means the minimum
   * \param c value 1 means the maximum
   */
  Vec2<T> const &operator[](int c) const {
    assert(c >= 0 && c <= 1);
    return m_pt[c];
  }
  //! the box size
  Vec2<T> size() const {
    return m_pt[1]-m_pt[0];
  }
  //! the box center
  Vec2<T> center() const {
    return 0.5*(m_pt[0]+m_pt[1]);
  }

  //! resets the data to minimum \a x and maximum \a y
  void set(Vec2<T> const &x, Vec2<T> const &y) {
    m_pt[0] = x;
    m_pt[1] = y;
  }
  //! resets the minimum point
  void setMin(Vec2<T> const &x) {
    m_pt[0] = x;
  }
  //! resets the maximum point
  void setMax(Vec2<T> const &y) {
    m_pt[1] = y;
  }

  //!  resize the box keeping the minimum
  void resizeFromMin(Vec2<T> const &sz) {
    m_pt[1] = m_pt[0]+sz;
  }
  //!  resize the box keeping the maximum
  void resizeFromMax(Vec2<T> const &sz) {
    m_pt[0] = m_pt[1]-sz;
  }
  //!  resize the box keeping the center
  void resizeFromCenter(Vec2<T> const &sz) {
    Vec2<T> centerPt = 0.5*(m_pt[0]+m_pt[1]);
    m_pt[0] = centerPt - 0.5*sz;
    m_pt[1] = centerPt + (sz - 0.5*sz);
  }

  //! scales all points of the box by \a factor
  template <class U> void scale(U factor) {
    m_pt[0] *= factor;
    m_pt[1] *= factor;
  }

  //! extends the bdbox by (\a val, \a val) keeping the center
  void extend(T val) {
    m_pt[0] -= Vec2<T>(val/2,val/2);
    m_pt[1] += Vec2<T>(val-(val/2),val-(val/2));
  }

  //! comparison operator==
  bool operator==(Box2<T> const &p) const {
    return cmp(p) == 0;
  }
  //! comparison operator!=
  bool operator!=(Box2<T> const &p) const {
    return cmp(p) != 0;
  }
  //! comparison operator< : fist sorts min by Y,X values then max extremity
  bool operator<(Box2<T> const &p) const {
    return cmp(p) < 0;
  }

  //! comparison function : fist sorts min by Y,X values then max extremity
  int cmp(Box2<T> const &p) const {
    int diff  = m_pt[0].cmpY(p.m_pt[0]);
    if (diff) return diff;
    diff  = m_pt[1].cmpY(p.m_pt[1]);
    if (diff) return diff;
    return 0;
  }

  //! print data in form X0xY0<->X1xY1
  friend std::ostream &operator<< (std::ostream &o, Box2<T> const &f) {
    o << "(" << f.m_pt[0] << "<->" << f.m_pt[1] << ")";
    return o;
  }

  /*! \struct PosSizeLt
   * \brief internal struct used to create sorted map, sorted first min then max
   */
  struct PosSizeLt {
    //! comparaison function
    bool operator()(Box2<T> const &s1, Box2<T> const &s2) const {
      return s1.cmp(s2) < 0;
    }
  };
  /*! \typedef Map
   *  \brief map of Box2
   */
  typedef std::map<Box2<T>, T,struct PosSizeLt> Map;

protected:
  //! the two extremities
  Vec2<T> m_pt[2];
};

/*! \brief Box2 of int */
typedef Box2<int> Box2i;
/*! \brief Box2 of float */
typedef Box2<float> Box2f;
/*! \brief Box2 of long */
typedef Box2<long> Box2l;

#endif /* LIBMWAW_INTERNAL_H */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
