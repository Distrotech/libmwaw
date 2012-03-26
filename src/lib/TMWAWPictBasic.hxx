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

/* This header contains code specific to manage basic picture (line, rectangle, ...)
 *
 * Note: all unit are points
 *
 */

#ifndef MWAW_PICT_BASIC
#  define MWAW_PICT_BASIC

#  include <assert.h>
#  include <ostream>
#  include <vector>

#  include "libmwaw_tools.hxx"
#  include "TMWAWPict.hxx"

class WPXBinaryData;
class WPXPropertyList;
class TMWAWPropertyHandlerEncoder;

/*
   libmwaw:document w="..pt" h="..pt"
   libmwaw:graphicStyle lineColor="#......" lineWidth="..pt" lineFill="solid/none"
           surfaceColor="#......" surfaceFill="solid/none"
           startArrow="true/false" startArrowWidth="..pt"
           endArrow="true/false" endArrowWidth="..pt" /
   libmwaw:drawLine x0=".." y0=".." x1=".." y1=".." /
   libmwaw:drawRectangle x0=".." y0=".."  w=".." h=".." [ rw=".." rh=".." ] /
   libmwaw:drawCircle x0=".." y0=".." w=".." h=".." /
   libmwaw:drawArc x0=".." y0=".." w=".." h=".." angle0=".." angle1=".." /
   libmwaw:drawPolygon x0=".." y0=".." ... x{N-1}=".." y{N-1}=".."  w=".." h=".." /

   /libmwaw:document
*/

namespace libmwaw_tools
{
/** \brief an abstract class which defines basic picture (a line, a rectangle, ...) */
class PictBasic: public Pict
{
public:
  //! virtual destructor
  virtual ~PictBasic() {}

  //! the picture subtype ( line, rectangle, polygon, circle, arc)
  enum SubType { Line, Rectangle, Polygon, Circle, Arc };
  //! returns the picture type
  virtual Type getType() const {
    return Basic;
  }
  //! returns the picture subtype
  virtual SubType getSubType() const = 0;

  //! sets the line width (by default 1.0)
  void setLineWidth(float w) {
    m_lineWidth = w;
    extendBDBox(m_lineWidth, 0);
  }
  /** sets the line color (color must be integer between 0 and 255)
   * default values : 0,0,0, which corresponds to black
   */
  void setLineColor(int r, int g, int b) {
    m_lineColor[0] = r;
    m_lineColor[1] = g;
    m_lineColor[2] = b;
  }

  /** sets the surface color (color must be integer between 0 and 255)
   * default values : 0xFF,0xFF,0xFF, which corresponds to white
   */
  void setSurfaceColor(int r, int g, int b, bool hasColor = true) {
    m_surfaceColor[0] = r;
    m_surfaceColor[1] = g;
    m_surfaceColor[2] = b;
    m_surfaceHasColor = hasColor;
  }

  /** returns the final representation in encoded odg (if possible) */
  virtual bool getBinary(WPXBinaryData &data, std::string &s) const {
    if (!getODGBinary(data)) return false;
    s = "image/mwaw-odg";
    return true;
  }
  /** virtual function which tries to convert the picture in ODG and put the result in a WPXBinaryData */
  virtual bool getODGBinary(WPXBinaryData &) const {
    return false;
  }

  /** a virtual function used to obtain a strict order.
   * -  must be redefined in the subs class
   */
  virtual int cmp(Pict const &a) const {
    int diff = Pict::cmp(a);
    if (diff) return diff;

    PictBasic const &aPict = static_cast<PictBasic const &>(a);
    // the type
    diff = getSubType() - aPict.getSubType();
    if (diff) return (diff < 0) ? -1 : 1;

    float diffF = m_lineWidth - aPict.m_lineWidth;
    if (diffF) return (diffF < 0) ? -1 : 1;

    for (int c=0; c < 3; c++) {
      diff = m_lineColor[c]-aPict.m_lineColor[c];
      if (diff) return (diff < 0) ? -1 : 1;
    }
    for (int c=0; c < 3; c++) {
      diff = m_surfaceColor[c]-aPict.m_surfaceColor[c];
      if (diff) return (diff < 0) ? -1 : 1;
    }
    for (int c = 0; c < 2; c++) {
      float diffF = m_extend[c]-aPict.m_extend[c];
      if (diffF) return diffF < 0.0 ? -1 : 1;
    }
    if (m_surfaceHasColor != aPict.m_surfaceHasColor)
      return m_surfaceHasColor;
    return 0;
  }
protected:
  //! function to implement in subclass in order to get the graphics style
  virtual void getGraphicStyleProperty(WPXPropertyList &list) const = 0;

  //! returns the basic style property for 1D form (line, ...)
  void getStyle1DProperty(WPXPropertyList &list) const;
  //! returns the basic style property for 2D form (line, ...)
  void getStyle2DProperty(WPXPropertyList &list) const;

  //! adds the odg header knowing the minPt and the maxPt
  void startODG(TMWAWPropertyHandlerEncoder &doc) const;
  //! adds the odg footer
  void endODG(TMWAWPropertyHandlerEncoder &doc) const;

  //! a function to extend the bdbox
  // - \param id=0 corresponds to linewidth
  // - \param id=1 corresponds to a second extension (arrow)
  void extendBDBox(float val, int id) {
    assert(id>=0&& id<=1);
    m_extend[id] = val;
    Pict::extendBDBox(m_extend[0]+m_extend[1]);
  }

  //! protected constructor must not be called directly
  PictBasic() : m_lineWidth(1.0), m_surfaceHasColor(false) {
    for (int c = 0; c < 2; c++) m_extend[c]=0;
    for (int c = 0; c < 3; c++) m_lineColor[c]=0;
    for (int c = 0; c < 3; c++) m_surfaceColor[c]=255;
    setLineWidth(1.0);
  }
  //! protected constructor must not be called directly
  PictBasic(PictBasic const &p) : Pict(), m_lineWidth(1.0), m_surfaceHasColor(false) {
    *this=p;
  }
  //! protected= must not be called directly
  PictBasic &operator=(PictBasic const &p) {
    if (&p == this) return *this;
    Pict::operator=(p);
    m_lineWidth = p.m_lineWidth;
    for (int c=0; c < 3; c++) m_lineColor[c] = p.m_lineColor[c];
    for (int c=0; c < 3; c++) m_surfaceColor[c] = p.m_surfaceColor[c];
    for (int c=0; c < 2; c++) m_extend[c] = p.m_extend[c];
    m_surfaceHasColor = p.m_surfaceHasColor;
    return *this;
  }

private:
  //! the linewidth
  float m_lineWidth;
  //! the line color (in rgb)
  int m_lineColor[3];
  //! the line color (in rgb)
  int m_surfaceColor[3];
  //! true if the surface has some color
  bool m_surfaceHasColor;
  //! m_extend[0]: from lineWidth, m_extend[1]: came from extra data
  float m_extend[2];
};

/** \brief a class to store a simple line */
class PictLine : public PictBasic
{
public:
  //! constructor
  PictLine(Vec2f orig, Vec2f end) : PictBasic() {
    m_extremity[0] = orig;
    m_extremity[1] = end;
    m_arrows[0] = m_arrows[1] = false;
    setBdBox(getBdBox(2,m_extremity));
  }
  //! virtual destructor
  virtual ~PictLine() {}
  //! sets the arrow: orig(v=0), end(v=1)
  void setArrow(int v, bool val) {
    assert(v>=0 && v<=1);
    m_arrows[v]=val;
    extendBDBox ((m_arrows[0] || m_arrows[1]) ? 5 : 0, 1);
  }

  //! returns a ODG (encoded)
  virtual bool getODGBinary(WPXBinaryData &res) const;

protected:
  //! returns the class type
  virtual SubType getSubType() const {
    return Line;
  }
  //! returns the graphics style
  virtual void getGraphicStyleProperty(WPXPropertyList &list) const;
  //! comparison function
  virtual int cmp(Pict const &a) const {
    int diff = PictBasic::cmp(a);
    if (diff) return diff;
    PictLine const &aLine = static_cast<PictLine const &>(a);
    for (int c = 0; c < 2; c++) {
      diff = m_extremity[c].cmpY(aLine.m_extremity[c]);
      if (diff) return diff;
    }
    for (int c = 0; c < 2; c++) {
      diff = m_arrows[c]-aLine.m_arrows[c];
      if (diff) return (diff < 0) ? -1 : 1;
    }
    return 0;
  }


  //! the extremity coordinate
  Vec2f m_extremity[2];
  //! two bool to indicated if extremity has arrow or not
  bool m_arrows[2];
};

//! \brief a class to define a rectangle (or a rectangle with round corner)
class PictRectangle : public PictBasic
{
public:
  //! constructor
  PictRectangle(Box2f box) : PictBasic(), m_rectBox(box) {
    setBdBox(box);
    for (int i = 0; i < 2; i++) m_cornerWidth[i] = 0;
  }
  //! virtual destructor
  virtual ~PictRectangle() {}

  //! sets the corner width
  void setRoundCornerWidth(int w) {
    m_cornerWidth[0] = m_cornerWidth[1] = w;
  }

  //! sets the corner width
  void setRoundCornerWidth(int xw, int yw) {
    m_cornerWidth[0] = xw;
    m_cornerWidth[1] = yw;
  }

  //! returns a ODG (encoded)
  virtual bool getODGBinary(WPXBinaryData &res) const;

protected:
  //! returns the class type
  virtual SubType getSubType() const {
    return Rectangle;
  }
  //! returns the graphics style
  virtual void getGraphicStyleProperty(WPXPropertyList &list) const;
  //! comparison function
  virtual int cmp(Pict const &a) const {
    int diff = PictBasic::cmp(a);
    if (diff) return diff;
    PictRectangle const &aRect = static_cast<PictRectangle const &>(a);
    for (int i = 0; i < 2; i++) {
      diff = m_cornerWidth[i] - aRect.m_cornerWidth[i];
      if (diff) return (diff < 0) ? -1 : 1;
    }
    return 0;
  }

  //! an int used to define round corner
  int m_cornerWidth[2];
  //! corner point
  Box2f m_rectBox;
};

//! a class used to define a circle or an ellipse
class PictCircle : public PictBasic
{
public:
  //! constructor
  PictCircle(Box2f box) : PictBasic(), m_circleBox(box) {
    setBdBox(box);
  }
  //! virtual destructor
  virtual ~PictCircle() {}

  //! returns a ODG (encoded)
  virtual bool getODGBinary(WPXBinaryData &res) const;

protected:
  //! returns the class type
  virtual SubType getSubType() const {
    return Circle;
  }
  //! returns the graphics style
  virtual void getGraphicStyleProperty(WPXPropertyList &list) const;
  //! comparison function
  virtual int cmp(Pict const &a) const {
    return PictBasic::cmp(a);
  }

  // corner point
  Box2f m_circleBox;
};

//! \brief a class used to define an arc
class PictArc : public PictBasic
{
public:
  /** \brief constructor:
  bdbox followed by the bdbox of the circle and 2 angles exprimed in degree */
  PictArc(Box2f box, Box2f ellBox, float ang1, float ang2) : PictBasic(), m_circleBox(ellBox) {
    setBdBox(box);
    m_angle[0] = ang1;
    m_angle[1] = ang2;
  }
  //! virtual destructor
  virtual ~PictArc() {}

  //! returns a ODG (encoded)
  virtual bool getODGBinary(WPXBinaryData &res) const;

protected:
  //! returns the class type
  virtual SubType getSubType() const {
    return Arc;
  }
  //! returns the graphics style
  virtual void getGraphicStyleProperty(WPXPropertyList &list) const;
  //! comparison function
  virtual int cmp(Pict const &a) const {
    int diff = PictBasic::cmp(a);
    if (diff) return diff;
    PictArc const &aArc = static_cast<PictArc const &>(a);
    // first check the bdbox
    diff = m_circleBox.cmp(m_circleBox);
    if (diff) return diff;
    for (int c = 0; c < 2; c++) {
      float diff = m_angle[c]-aArc.m_angle[c];
      if (diff) return (diff <0) ? -1 : 1;
    }
    return 0;
  }

  //! corner ellipse rectangle point
  Box2f m_circleBox;

  //! the two angles
  float m_angle[2];
};

//! \brief a class used to define a polygon
class PictPolygon : public PictBasic
{
public:
  /** constructor: bdbox followed by the bdbox of the circle
  and 2 angl exprimed in degree */
  PictPolygon(Box2f bdBox, std::vector<Vec2f> const &lVect) : PictBasic(), m_verticesList(lVect) {
    setBdBox(bdBox);
  }
  //! virtual destructor
  virtual ~PictPolygon() {}

  //! returns a ODG (encoded)
  virtual bool getODGBinary(WPXBinaryData &res) const;

protected:
  //! returns the class type
  virtual SubType getSubType() const {
    return Polygon;
  }
  //! returns the graphics style
  virtual void getGraphicStyleProperty(WPXPropertyList &list) const;
  //! comparison function
  virtual int cmp(Pict const &a) const {
    int diff = PictBasic::cmp(a);
    if (diff) return diff;
    PictPolygon const &aPoly = static_cast<PictPolygon const &>(a);
    diff = m_verticesList.size()-aPoly.m_verticesList.size();
    if (diff) return (diff <0) ? -1 : 1;

    // check the vertices
    for (int c = 0; c < 2; c++) {
      diff = m_verticesList[c].cmpY(aPoly.m_verticesList[c]);
      if (diff) return diff;
    }
    return 0;
  }

  //! the vertices list
  std::vector<Vec2f> m_verticesList;
};

}

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
