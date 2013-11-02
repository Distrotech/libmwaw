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
#ifndef MWAW_GRAPHIC_SHAPE
#  define MWAW_GRAPHIC_SHAPE
#  include <ostream>
#  include <string>
#  include <vector>

#  include "librevenge/librevenge.h"
#  include "libmwaw_internal.hxx"

class RVNGPropertyList;
class MWAWGraphicStyle;

/** a structure used to define a picture shape */
class MWAWGraphicShape
{
public:
  //! an enum used to define the shape type
  enum Type { Arc, Circle, Line, Rectangle, Path, Pie, Polygon, ShapeUnknown };
  //! a simple path component
  struct PathData {
    //! constructor
    PathData(char type, Vec2f const &x=Vec2f(), Vec2f const &x1=Vec2f(), Vec2f const &x2=Vec2f()):
      m_type(type), m_x(x), m_x1(x1), m_x2(x2), m_r(), m_rotate(0), m_largeAngle(false), m_sweep(false) {
    }
    //! translate all the coordinate by delta
    void translate(Vec2f const &delta);
    //! rotate all the coordinate by angle (origin rotation) then translate coordinate
    void rotate(float angle, Vec2f const &delta);
    //! update the property list to correspond to a command
    bool get(RVNGPropertyList &pList, Vec2f const &orig) const;
    //! a print operator
    friend std::ostream &operator<<(std::ostream &o, PathData const &path);
    //! comparison function
    int cmp(PathData const &a) const;
    //! the type: M, L, ...
    char m_type;
    //! the main x value
    Vec2f m_x;
    //! x1 value
    Vec2f m_x1;
    //! x2 value
    Vec2f m_x2;
    //! the radius ( A command)
    Vec2f m_r;
    //! the rotate ( A command)
    float m_rotate;
    //! large angle ( A command)
    bool m_largeAngle;
    //! sweep value ( A command)
    bool m_sweep;
  };

  //! constructor
  MWAWGraphicShape() : m_type(ShapeUnknown), m_bdBox(), m_formBox(), m_cornerWidth(0,0), m_arcAngles(0,0),
    m_vertices(), m_path(), m_extra("") {
  }
  //! virtual destructor
  virtual ~MWAWGraphicShape() { }
  //! static constructor to create a line
  static MWAWGraphicShape line(Vec2f const &orign, Vec2f const &dest);
  //! static constructor to create a rectangle
  static MWAWGraphicShape rectangle(Box2f const &box, Vec2f const &corners=Vec2f(0,0)) {
    MWAWGraphicShape res;
    res.m_type=Rectangle;
    res.m_bdBox=res.m_formBox=box;
    res.m_cornerWidth=corners;
    return res;
  }
  //! static constructor to create a circle
  static MWAWGraphicShape circle(Box2f const &box) {
    MWAWGraphicShape res;
    res.m_type=Circle;
    res.m_bdBox=res.m_formBox=box;
    return res;
  }
  //! static constructor to create a arc
  static MWAWGraphicShape arc(Box2f const &box, Box2f const &circleBox, Vec2f const &angles) {
    MWAWGraphicShape res;
    res.m_type=Arc;
    res.m_bdBox=box;
    res.m_formBox=circleBox;
    res.m_arcAngles=angles;
    return res;
  }
  //! static constructor to create a pie
  static MWAWGraphicShape pie(Box2f const &box, Box2f const &circleBox, Vec2f const &angles) {
    MWAWGraphicShape res;
    res.m_type=Pie;
    res.m_bdBox=box;
    res.m_formBox=circleBox;
    res.m_arcAngles=angles;
    return res;
  }
  //! static constructor to create a polygon
  static MWAWGraphicShape polygon(Box2f const &box) {
    MWAWGraphicShape res;
    res.m_type=Polygon;
    res.m_bdBox=box;
    return res;
  }
  //! static constructor to create a path
  static MWAWGraphicShape path(Box2f const &box) {
    MWAWGraphicShape res;
    res.m_type=Path;
    res.m_bdBox=box;
    return res;
  }

  //! translate all the coordinate by delta
  void translate(Vec2f const &delta);
  /** return a new shape corresponding to a rotation from center.

   \note the final bdbox is not tight */
  MWAWGraphicShape rotate(float angle, Vec2f const &center) const;
  //! returns the bdbox corresponding to a style
  Box2f getBdBox(MWAWGraphicStyle const &style, bool moveToO=false) const;
  //! add shape to a graphic listener
  bool send(MWAWGraphicInterface &interface, MWAWGraphicStyle const &style, Vec2f const &orig) const;
  //! a print operator
  friend std::ostream &operator<<(std::ostream &o, MWAWGraphicShape const &sh);
  /** compare two shapes */
  int cmp(MWAWGraphicShape const &a) const;
protected:
  //! return a path corresponding to the shape
  std::vector<PathData> getPath() const;
public:
  //! the type
  Type m_type;
  //! the shape bdbox
  Box2f m_bdBox;
  //! the internal shape bdbox ( used for arc, circle to store the circle bdbox )
  Box2f m_formBox;
  //! the rectangle round corner
  Vec2f m_cornerWidth;
  //! the start and end value which defines an arc
  Vec2f m_arcAngles;
  //! the list of vertices for lines or polygons
  std::vector<Vec2f> m_vertices;
  //! the list of path component
  std::vector<PathData> m_path;
  //! extra data
  std::string m_extra;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
