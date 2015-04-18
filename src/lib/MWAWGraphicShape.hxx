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

class MWAWGraphicStyle;

/** a structure used to define a picture shape */
class MWAWGraphicShape
{
public:
  //! an enum used to define the shape type
  enum Type { Arc, Circle, Line, Rectangle, Path, Pie, Polygon, ShapeUnknown };
  //! an enum used to define the interface command
  enum Command { C_Ellipse, C_Polyline, C_Rectangle, C_Path, C_Polygon, C_Bad };
  //! a simple path component
  struct PathData {
    //! constructor
    PathData(char type, MWAWVec2f const &x=MWAWVec2f(), MWAWVec2f const &x1=MWAWVec2f(), MWAWVec2f const &x2=MWAWVec2f()):
      m_type(type), m_x(x), m_x1(x1), m_x2(x2), m_r(), m_rotate(0), m_largeAngle(false), m_sweep(false)
    {
    }
    //! translate all the coordinate by delta
    void translate(MWAWVec2f const &delta);
    //! scale all the coordinate by a factor
    void scale(MWAWVec2f const &factor);
    //! rotate all the coordinate by angle (origin rotation) then translate coordinate
    void rotate(float angle, MWAWVec2f const &delta);
    //! update the property list to correspond to a command
    bool get(librevenge::RVNGPropertyList &pList, MWAWVec2f const &orig) const;
    //! a print operator
    friend std::ostream &operator<<(std::ostream &o, PathData const &path);
    //! comparison function
    int cmp(PathData const &a) const;
    //! the type: M, L, ...
    char m_type;
    //! the main x value
    MWAWVec2f m_x;
    //! x1 value
    MWAWVec2f m_x1;
    //! x2 value
    MWAWVec2f m_x2;
    //! the radius ( A command)
    MWAWVec2f m_r;
    //! the rotate ( A command)
    float m_rotate;
    //! large angle ( A command)
    bool m_largeAngle;
    //! sweep value ( A command)
    bool m_sweep;
  };

  //! constructor
  MWAWGraphicShape() : m_type(ShapeUnknown), m_bdBox(), m_formBox(), m_cornerWidth(0,0), m_arcAngles(0,0),
    m_vertices(), m_path(), m_extra("")
  {
  }
  //! virtual destructor
  virtual ~MWAWGraphicShape() { }
  //! static constructor to create a line
  static MWAWGraphicShape line(MWAWVec2f const &orign, MWAWVec2f const &dest);
  //! static constructor to create a rectangle
  static MWAWGraphicShape rectangle(MWAWBox2f const &box, MWAWVec2f const &corners=MWAWVec2f(0,0))
  {
    MWAWGraphicShape res;
    res.m_type=Rectangle;
    res.m_bdBox=res.m_formBox=box;
    res.m_cornerWidth=corners;
    return res;
  }
  //! static constructor to create a circle
  static MWAWGraphicShape circle(MWAWBox2f const &box)
  {
    MWAWGraphicShape res;
    res.m_type=Circle;
    res.m_bdBox=res.m_formBox=box;
    return res;
  }
  //! static constructor to create a arc
  static MWAWGraphicShape arc(MWAWBox2f const &box, MWAWBox2f const &circleBox, MWAWVec2f const &angles)
  {
    MWAWGraphicShape res;
    res.m_type=Arc;
    res.m_bdBox=box;
    res.m_formBox=circleBox;
    res.m_arcAngles=angles;
    return res;
  }
  //! static constructor to create a pie
  static MWAWGraphicShape pie(MWAWBox2f const &box, MWAWBox2f const &circleBox, MWAWVec2f const &angles)
  {
    MWAWGraphicShape res;
    res.m_type=Pie;
    res.m_bdBox=box;
    res.m_formBox=circleBox;
    res.m_arcAngles=angles;
    return res;
  }
  //! static constructor to create a polygon
  static MWAWGraphicShape polygon(MWAWBox2f const &box)
  {
    MWAWGraphicShape res;
    res.m_type=Polygon;
    res.m_bdBox=box;
    return res;
  }
  //! static constructor to create a path
  static MWAWGraphicShape path(MWAWBox2f const &box)
  {
    MWAWGraphicShape res;
    res.m_type=Path;
    res.m_bdBox=box;
    return res;
  }

  //! translate all the coordinate by delta
  void translate(MWAWVec2f const &delta);
  //! rescale all the coordinate
  void scale(MWAWVec2f const &factor);
  /** return a new shape corresponding to a rotation from center.

   \note the final bdbox is not tight */
  MWAWGraphicShape rotate(float angle, MWAWVec2f const &center) const;
  //! returns the type corresponding to a shape
  Type getType() const
  {
    return m_type;
  }
  //! returns the basic bdbox
  MWAWBox2f getBdBox() const
  {
    return m_bdBox;
  }
  //! returns the bdbox corresponding to a style
  MWAWBox2f getBdBox(MWAWGraphicStyle const &style, bool moveToO=false) const;
  //! updates the propList to send to an interface
  Command addTo(MWAWVec2f const &orig, bool asSurface, librevenge::RVNGPropertyList &propList) const;
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
  MWAWBox2f m_bdBox;
  //! the internal shape bdbox ( used for arc, circle to store the circle bdbox )
  MWAWBox2f m_formBox;
  //! the rectangle round corner
  MWAWVec2f m_cornerWidth;
  //! the start and end value which defines an arc
  MWAWVec2f m_arcAngles;
  //! the list of vertices for lines or polygons
  std::vector<MWAWVec2f> m_vertices;
  //! the list of path component
  std::vector<PathData> m_path;
  //! extra data
  std::string m_extra;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
