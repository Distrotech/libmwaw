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

#ifndef LIBMWAW_TOOLS_H
#define LIBMWAW_TOOLS_H
#include <assert.h>
#include <map>
#include <ostream>
#include <vector>

/* Various functions/defines that need not/should not be exported externally */

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

/*! \def MWAW_DEBUG_MSG(M)
 * print a message \a M on stdout if compiled in debug mode
 */

#ifdef DEBUG
#define MWAW_DEBUG_MSG(M) printf M
#else
#define MWAW_DEBUG_MSG(M)
#endif

/*! \class Vec2
 *   \brief small class which defines a vector with 2 elements
 */
template <class T> class Vec2
{
public:
  //! constructor
  Vec2(T x=0,T y=0) : m_x(x), m_y(y) { }
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

  //! resets the two elements
  void set(T x, T y) {
    m_x = x;
    m_y = y;
  }
  //! resets the first element
  void setX(T x) {
    m_x = x;
  }
  //! resets the second element
  void setY(T y) {
    m_y = y;
  }

  //! increases the actuals values by \a dx and \a dy
  void add(T dx, T dy) {
    m_x += dx;
    m_y += dy;
  }
  //! adds \a dx
  void addX(T dx) {
    m_x += dx;
  }
  //! adds \a dy
  void addY(T dy) {
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
    if (diff) return (diff < 0) ? -1 : 1;
    diff = m_y-p.m_y;
    if (diff) return (diff < 0) ? -1 : 1;
    return 0;
  }
  //! a comparison function: which first compares y then x
  int cmpY(Vec2<T> const &p) const {
    T diff  = m_y-p.m_y;
    if (diff) return (diff < 0) ? -1 : 1;
    diff = m_x-p.m_x;
    if (diff) return (diff < 0) ? -1 : 1;
    return 0;
  }

  //! Debug: prints data in form "(x,y)"
  std::ostream &debugP(std::ostream &o) const {
    o << "(" << m_x << "," << m_y << ")";
    return o;
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
/*! \brief Vec2 of float */
typedef Vec2<float> Vec2f;

/*! \class Vec3
 *   \brief small class which defines a vector with 3 elements
 */
template <class T> class Vec3
{
public:
  //! constructor
  Vec3(T x=0,T y=0,T z=0) {
    m_val[0] = x;
    m_val[1] = y;
    m_val[2] = z;
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

  //! resets the three elements
  void set(T x, T y, T z) {
    m_val[0] = x;
    m_val[1] = y;
    m_val[2] = z;
  }
  //! resets the first element
  void setX(T x) {
    m_val[0] = x;
  }
  //! resets the second element
  void setY(T y) {
    m_val[1] = y;
  }
  //! resets the third element
  void setZ(T z) {
    m_val[2] = z;
  }

  //! increases the actuals values by \a dx, \a dy, \a dz
  void add(T dx, T dy, T dz) {
    m_val[0] += dx;
    m_val[1] += dy;
    m_val[2] += dz;
  }
  //! adds \a dx
  void addX(T dx) {
    m_val[0] += dx;
  }
  //! adds \a dy
  void addY(T dy) {
    m_val[1] += dy;
  }
  //! adds \a dz
  void addZ(T dz) {
    m_val[2] += dz;
  }

  //! operator+=
  Vec3<T> &operator+=(Vec3<T> const &p) {
    for (int c = 0; c < 3; c++) m_val[c] += p.m_val[c];
    return *this;
  }
  //! operator-=
  Vec3<T> &operator-=(Vec3<T> const &p) {
    for (int c = 0; c < 3; c++) m_val[c] -= p.m_val[c];
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

  //! Debug: prints data in form "(x,y,z)"
  std::ostream &debugP(std::ostream &o) const {
    o << "(" << m_val[0] << "," << m_val[1] << "," << m_val[2] << ")";
    return o;
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
    m_pt[0]=minPt;
    m_pt[1] = maxPt;
  }
  //! generic constructor
  template <class U> Box2(Box2<U> const &p) {
    for (int c=0; c < 2; c++) m_pt[c] = p[c];
  }

  //! the minimum 2D point (in x and in y)
  Vec2<T> const & min() const {
    return m_pt[0];
  }
  //! the maximum 2D point (in x and in y)
  Vec2<T> const & max() const {
    return m_pt[1];
  }
  /*! \brief the two extremum points which defined the box
   * \param c value 0 means the minimum
   * \param c value 1 means the maximum
   */
  Vec2<T> const & operator[](int c) const {
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
    Vec2<T> center = 0.5*(m_pt[0]+m_pt[1]);
    m_pt[0] = center - 0.5*sz;
    m_pt[1] = center + (sz - 0.5*sz);
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

  //! print data in form (x0,y0)x(x1,y1)
  std::ostream &debugP(std::ostream &o) const {
    o << m_pt[0] << "x" << m_pt[1];
    return o;
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

#endif /* LIBMWAW_INTERNAL_H */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
