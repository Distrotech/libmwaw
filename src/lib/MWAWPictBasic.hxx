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

/* This header contains code specific to manage basic picture (line, rectangle, ...)
 *
 * Note: all unit are points
 *
 */

#ifndef MWAW_PICT_BASIC
#  define MWAW_PICT_BASIC

#  include <assert.h>
#  include <ostream>
#  include <string>
#  include <vector>

#  include "libwpd/libwpd.h"
#  include "libmwaw_internal.hxx"
#  include "MWAWPict.hxx"

class MWAWPropertyHandlerEncoder;

/** \brief an abstract class which defines basic picture (a line, a rectangle, ...) */
class MWAWPictBasic: public MWAWPict
{
public:
  struct Style;

  //! virtual destructor
  virtual ~MWAWPictBasic() {}

  //! the picture subtype ( line, rectangle, polygon, circle, arc)
  enum SubType { Arc, Circle, Group, Line, Path, Polygon, Rectangle };
  //! returns the picture type
  virtual Type getType() const {
    return Basic;
  }
  //! returns the picture subtype
  virtual SubType getSubType() const = 0;

  //! returns a ODG (encoded)
  virtual bool getODGBinary(WPXBinaryData &res) const;

  //! returns the current style
  Style const &getStyle() const {
    return m_style;
  }
  //! set the current style
  void setStyle(Style const &style) {
    m_style = style;
    updateBdBox();
  }
  //! return the layer
  int getLayer() const {
    return m_layer;
  }
  //! set the layer
  void setLayer(int layer) {
    m_layer=layer;
  }
  /** returns the final representation in encoded odg (if possible) */
  virtual bool getBinary(WPXBinaryData &data, std::string &s) const {
    if (!getODGBinary(data)) return false;
    s = "image/mwaw-odg2";
    return true;
  }
  //! returns a ODG (low level)
  virtual bool getODGBinary(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const=0;

  /** a virtual function used to obtain a strict order.
   * -  must be redefined in the subs class
   */
  virtual int cmp(MWAWPict const &a) const {
    int diff = MWAWPict::cmp(a);
    if (diff) return diff;

    MWAWPictBasic const &aPict = static_cast<MWAWPictBasic const &>(a);
    if (m_layer < aPict.m_layer) return -1;
    if (m_layer > aPict.m_layer) return 1;

    // the type
    diff = getSubType() - aPict.getSubType();
    if (diff) return (diff < 0) ? -1 : 1;
    diff = m_style.cmp(aPict.m_style);
    if (diff) return diff;
    for (int c = 0; c < 2; c++) {
      float diffF = m_extend[c]-aPict.m_extend[c];
      if (diffF < 0) return -1;
      if (diffF > 0) return 1;
    }
    return 0;
  }

protected:
  //! update the bdbox if needed, must be called if lineWidth or arrows change
  void updateBdBox() {
    extendBDBox(m_style.m_lineWidth, 0);
    extendBDBox((m_style.m_arrows[0] || m_style.m_arrows[1]) ? 5 : 0, 1);
  }

  //! function to implement in subclass in order to get the graphics style
  virtual void getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const = 0;

  //! adds the odg header knowing the minPt and the maxPt
  virtual void startODG(MWAWPropertyHandlerEncoder &doc) const;
  //! adds the odg footer
  virtual void endODG(MWAWPropertyHandlerEncoder &doc) const;

  //! a function to extend the bdbox
  // - \param id=0 corresponds to linewidth
  // - \param id=1 corresponds to a second extension (arrow)
  void extendBDBox(float val, int id) {
    assert(id>=0&& id<=1);
    m_extend[id] = val;
    MWAWPict::extendBDBox(m_extend[0]+m_extend[1]);
  }

  //! protected constructor must not be called directly
  MWAWPictBasic() : MWAWPict(), m_layer(-1000), m_style() {
    for (int c = 0; c < 2; c++) m_extend[c]=0;
    updateBdBox();
  }
  //! protected constructor must not be called directly
  MWAWPictBasic(MWAWPictBasic const &p) : MWAWPict(), m_layer(-1000), m_style() {
    *this=p;
  }
  //! protected= must not be called directly
  MWAWPictBasic &operator=(MWAWPictBasic const &p) {
    if (&p == this) return *this;
    MWAWPict::operator=(p);
    m_layer = p.m_layer;
    m_style = p.m_style;
    for (int c=0; c < 2; c++) m_extend[c] = p.m_extend[c];
    return *this;
  }

public:
  //! a structure used to define the gradient limit
  struct GradientStop {
    //! constructor
    GradientStop(float offset=0.0, MWAWColor const &col=MWAWColor::black(), float opacity=1.0) :
      m_offset(offset), m_color(col), m_opacity(opacity) {
    }
    /** compare two styles */
    int cmp(GradientStop const &a) const {
      if (m_offset < a.m_offset) return -1;
      if (m_offset > a.m_offset) return 1;
      if (m_color < a.m_color) return -1;
      if (m_color > a.m_color) return 1;
      if (m_opacity < a.m_opacity) return -1;
      if (m_opacity > a.m_opacity) return 1;
      return 0;
    }
    //! a print operator
    friend std::ostream &operator<<(std::ostream &o, GradientStop const &st) {
      o << "offset=" << st.m_offset << ",";
      o << "color=" << st.m_color << ",";
      if (st.m_opacity<1.0)
        o << "opacity=" << st.m_opacity*100.f << "%,";
      return o;
    }
    //! the offset
    float m_offset;
    //! the color
    MWAWColor m_color;
    //! the opacity
    float m_opacity;
  };
  //! a structure used to define a picture style
  struct Style {
    //! an enum used to define the basic line cap
    enum LineCap { C_Butt, C_Square, C_Round };
    //! an enum used to define the basic line join
    enum LineJoin { J_Miter, J_Round, J_Bevel };
    //! an enum used to define the gradient type
    enum GradientType { G_None, G_Axial, G_Linear, G_Radial, G_Rectangular, G_Square, G_Ellipsoid };

    //! constructor
    Style() :  m_lineWidth(1), m_lineDashWidth(), m_lineCap(C_Butt), m_lineJoin(J_Miter), m_lineOpacity(1), m_lineColor(MWAWColor::black()),
      m_fillRuleEvenOdd(false), m_surfaceColor(MWAWColor::white()), m_surfaceOpacity(0),
      m_shadowColor(MWAWColor::black()), m_shadowOpacity(0), m_shadowOffset(1,1),
      m_gradientType(G_None), m_gradientStopList(), m_gradientAngle(0), m_gradientBorder(0), m_gradientPercentCenter(0.5f,0.5f), m_gradientRadius(1) {
      m_arrows[0]=m_arrows[1]=false;
      m_gradientStopList.push_back(GradientStop(0.0, MWAWColor::white()));
      m_gradientStopList.push_back(GradientStop(1.0, MWAWColor::black()));
    }
    //! returns true if the border is defined
    bool hasLine() const {
      return m_lineWidth>0 && m_lineOpacity>0;
    }
    //! set the surface color
    void setSurfaceColor(MWAWColor const &col, float opacity = 1) {
      m_surfaceColor = col;
      m_surfaceOpacity = opacity;
    }
    //! returns true if the surface is defined
    bool hasSurface() const {
      return m_surfaceOpacity > 0;
    }
    //! set the shadow color
    void setShadowColor(MWAWColor const &col, float opacity = 1) {
      m_shadowColor = col;
      m_shadowOpacity = opacity;
    }
    //! returns true if the shadow is defined
    bool hasShadow() const {
      return m_shadowOpacity > 0;
    }
    //! returns true if the gradient is defined
    bool hasGradient() const {
      return m_gradientType != G_None && m_gradientStopList.size() >= 2;
    }
    //! a print operator
    friend std::ostream &operator<<(std::ostream &o, Style const &st);
    //! add to propList
    void addTo(WPXPropertyList &pList, WPXPropertyListVector &gradient, bool only1d=false) const;

    /** compare two styles */
    int cmp(Style const &a) const;

    //! the linewidth
    float m_lineWidth;
    //! the dash array: a sequence of (fullsize, emptysize)
    std::vector<float> m_lineDashWidth;
    //! the line cap
    LineCap m_lineCap;
    //! the line join
    LineJoin m_lineJoin;
    //! the line opacity: 0=transparent
    float m_lineOpacity;
    //! the line color
    MWAWColor m_lineColor;
    //! true if the fill rule is evenod
    bool m_fillRuleEvenOdd;
    //! the surface color
    MWAWColor m_surfaceColor;
    //! true if the surface has some color
    float m_surfaceOpacity;

    //! the shadow color
    MWAWColor m_shadowColor;
    //! true if the shadow has some color
    float m_shadowOpacity;
    //! the shadow offset
    Vec2f m_shadowOffset;

    //! the gradient type
    GradientType m_gradientType;
    //! the list of gradient limits
    std::vector<GradientStop> m_gradientStopList;
    //! the gradient angle
    float m_gradientAngle;
    //! the gradient border opacity
    float m_gradientBorder;
    //! the gradient center
    Vec2f m_gradientPercentCenter;
    //! the gradient radius
    float m_gradientRadius;
    //! two bool to indicated if extremity has arrow or not
    bool m_arrows[2];
  };
protected:
  //! the layer number if know
  int m_layer;
  //! the data style
  Style m_style;
private:
  //! m_extend[0]: from lineWidth, m_extend[1]: came from extra data
  float m_extend[2];
};

/** \brief a class to store a simple line */
class MWAWPictLine : public MWAWPictBasic
{
public:
  //! constructor
  MWAWPictLine(Vec2f orig, Vec2f end) : MWAWPictBasic() {
    m_extremity[0] = orig;
    m_extremity[1] = end;
    setBdBox(getBdBox(2,m_extremity));
  }
  //! virtual destructor
  virtual ~MWAWPictLine() {}

  //! returns a ODG (encoded)
  virtual bool getODGBinary(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const;

protected:
  //! returns the class type
  virtual SubType getSubType() const {
    return Line;
  }
  //! returns the graphics style
  virtual void getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const;
  //! comparison function
  virtual int cmp(MWAWPict const &a) const {
    int diff = MWAWPictBasic::cmp(a);
    if (diff) return diff;
    MWAWPictLine const &aLine = static_cast<MWAWPictLine const &>(a);
    for (int c = 0; c < 2; c++) {
      diff = m_extremity[c].cmpY(aLine.m_extremity[c]);
      if (diff) return diff;
    }
    return 0;
  }


  //! the extremity coordinate
  Vec2f m_extremity[2];
};

//! \brief a class to define a rectangle (or a rectangle with round corner)
class MWAWPictRectangle : public MWAWPictBasic
{
public:
  //! constructor
  MWAWPictRectangle(Box2f box) : MWAWPictBasic(), m_rectBox(box) {
    setBdBox(box);
    for (int i = 0; i < 2; i++) m_cornerWidth[i] = 0;
  }
  //! virtual destructor
  virtual ~MWAWPictRectangle() {}

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
  virtual bool getODGBinary(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const;

protected:
  //! returns the class type
  virtual SubType getSubType() const {
    return Rectangle;
  }
  //! returns the graphics style
  virtual void getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const;
  //! comparison function
  virtual int cmp(MWAWPict const &a) const {
    int diff = MWAWPictBasic::cmp(a);
    if (diff) return diff;
    MWAWPictRectangle const &aRect = static_cast<MWAWPictRectangle const &>(a);
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
class MWAWPictCircle : public MWAWPictBasic
{
public:
  //! constructor
  MWAWPictCircle(Box2f box) : MWAWPictBasic(), m_circleBox(box) {
    setBdBox(box);
  }
  //! virtual destructor
  virtual ~MWAWPictCircle() {}

  //! returns a ODG (encoded)
  virtual bool getODGBinary(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const;

protected:
  //! returns the class type
  virtual SubType getSubType() const {
    return Circle;
  }
  //! returns the graphics style
  virtual void getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const;
  //! comparison function
  virtual int cmp(MWAWPict const &a) const {
    return MWAWPictBasic::cmp(a);
  }

  // corner point
  Box2f m_circleBox;
};

//! \brief a class used to define an arc
class MWAWPictArc : public MWAWPictBasic
{
public:
  /** \brief constructor:
  bdbox followed by the bdbox of the circle and 2 angles exprimed in degree */
  MWAWPictArc(Box2f box, Box2f ellBox, float ang1, float ang2) : MWAWPictBasic(), m_circleBox(ellBox) {
    setBdBox(box);
    m_angle[0] = ang1;
    m_angle[1] = ang2;
  }
  //! virtual destructor
  virtual ~MWAWPictArc() {}

  //! returns a ODG (encoded)
  virtual bool getODGBinary(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const;

protected:
  //! returns the class type
  virtual SubType getSubType() const {
    return Arc;
  }
  //! returns the graphics style
  virtual void getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const;
  //! comparison function
  virtual int cmp(MWAWPict const &a) const {
    int diff = MWAWPictBasic::cmp(a);
    if (diff) return diff;
    MWAWPictArc const &aArc = static_cast<MWAWPictArc const &>(a);
    // first check the bdbox
    diff = m_circleBox.cmp(aArc.m_circleBox);
    if (diff) return diff;
    for (int c = 0; c < 2; c++) {
      float diffF = m_angle[c]-aArc.m_angle[c];
      if (diffF < 0) return -1;
      if (diffF > 0) return 1;
    }
    return 0;
  }

  //! corner ellipse rectangle point
  Box2f m_circleBox;

  //! the two angles
  float m_angle[2];
};

//! \brief a class used to define a generic path ( a bezier curve, ... )
class MWAWPictPath : public MWAWPictBasic
{
public:
  /** \brief constructor: bdbox followed by the path definition */
  MWAWPictPath(Box2f bdBox, WPXPropertyListVector const &path) : MWAWPictBasic(), m_path(path) {
    setBdBox(bdBox);
  }
  //! virtual destructor
  virtual ~MWAWPictPath() {}

  //! returns a ODG (encoded)
  virtual bool getODGBinary(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const;

protected:
  //! returns the class type
  virtual SubType getSubType() const {
    return Path;
  }
  //! returns the graphics style
  virtual void getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const;
  //! comparison function
  virtual int cmp(MWAWPict const &a) const;

  //! the string represented the path (in svg)
  WPXPropertyListVector m_path;
};

//! \brief a class used to define a polygon
class MWAWPictPolygon : public MWAWPictBasic
{
public:
  /** constructor: bdbox followed by the set of vertices */
  MWAWPictPolygon(Box2f bdBox, std::vector<Vec2f> const &lVect) : MWAWPictBasic(), m_verticesList(lVect) {
    setBdBox(bdBox);
  }
  //! virtual destructor
  virtual ~MWAWPictPolygon() {}

  //! returns a ODG (encoded)
  virtual bool getODGBinary(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const;

protected:
  //! returns the class type
  virtual SubType getSubType() const {
    return Polygon;
  }
  //! returns the graphics style
  virtual void getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const;
  //! comparison function
  virtual int cmp(MWAWPict const &a) const {
    int diff = MWAWPictBasic::cmp(a);
    if (diff) return diff;
    MWAWPictPolygon const &aPoly = static_cast<MWAWPictPolygon const &>(a);
    if (m_verticesList.size()<aPoly.m_verticesList.size())
      return -1;
    if (m_verticesList.size()>aPoly.m_verticesList.size())
      return 1;

    // check the vertices
    for (size_t c = 0; c < m_verticesList.size(); c++) {
      diff = m_verticesList[c].cmpY(aPoly.m_verticesList[c]);
      if (diff) return diff;
    }
    return 0;
  }

  //! the vertices list
  std::vector<Vec2f> m_verticesList;
};

//! \brief a class used to define a polygon
class MWAWPictGroup : public MWAWPictBasic
{
public:
  /** constructor: */
  MWAWPictGroup() : MWAWPictBasic(), m_child() {
  }
  //! virtual destructor
  virtual ~MWAWPictGroup() {}

  //! returns a ODG (encoded)
  virtual bool getODGBinary(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const;

  //! add a new child
  void addChild(shared_ptr<MWAWPictBasic> child);
protected:
  //! returns the class type
  virtual SubType getSubType() const {
    return Group;
  }
  //! returns the graphics style
  virtual void getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const;
  //! comparison function
  virtual int cmp(MWAWPict const &a) const {
    int diff = MWAWPictBasic::cmp(a);
    if (diff) return diff;
    MWAWPictGroup const &aGroup = static_cast<MWAWPictGroup const &>(a);
    if (m_child.size()<aGroup.m_child.size())
      return -1;
    if (m_child.size()>aGroup.m_child.size())
      return 1;

    // check the vertices
    for (size_t c = 0; c < m_child.size(); c++) {
      if (!m_child[c]) {
        if (aGroup.m_child[c]) return -1;
        continue;
      }
      if (!aGroup.m_child[c]) return 1;
      diff = m_child[c]->cmp(*aGroup.m_child[c]);
      if (diff) return diff;
    }
    return 0;
  }

  //! the vertices list
  std::vector<shared_ptr<MWAWPictBasic> > m_child;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
