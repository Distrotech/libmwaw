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
#ifndef MWAW_GRAPHIC_STYLE
#  define MWAW_GRAPHIC_STYLE

#  include <ostream>
#  include <string>
#  include <vector>

#  include "libwpd/libwpd.h"
#  include "libmwaw_internal.hxx"

//! a small class used to manage graphics
class MWAWGraphicStyleManager
{
public:
  //! constructor
  MWAWGraphicStyleManager(shared_ptr<MWAWFontConverter> &fontConverter);
  //! destructor
  ~MWAWGraphicStyleManager();
  //! returns the font converter
  MWAWFontConverter &getFontConverter();
  //! returns a new id for a layer
  int getNewLayerId() const {
    return ++m_numLayer;
  }
  //! returns a new id for a graphic object
  int getNewGraphicObjectId() const {
    return ++m_numGraphicObject;
  }
protected:
  //! the font converter
  shared_ptr<MWAWFontConverter> &m_fontConverter;
  //! the number of layer
  mutable int m_numLayer;
  //! the number of graphic object
  mutable int m_numGraphicObject;
};

//! a structure used to define a picture style
class MWAWGraphicStyle
{
public:
  //! an enum used to define the basic line cap
  enum LineCap { C_Butt, C_Square, C_Round };
  //! an enum used to define the basic line join
  enum LineJoin { J_Miter, J_Round, J_Bevel };
  //! an enum used to define the gradient type
  enum GradientType { G_None, G_Axial, G_Linear, G_Radial, G_Rectangular, G_Square, G_Ellipsoid };

  //! a structure used to define the gradient limit
  struct GradientStop {
    //! constructor
    GradientStop(float offset=0.0, MWAWColor const &col=MWAWColor::black(), float opacity=1.0) :
      m_offset(offset), m_color(col), m_opacity(opacity) {
    }
    /** compare two gradient */
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
  //! a basic pattern 8x8, 16x16, 32x32 use in a MWAWGraphicStyle
  struct Pattern {
    //! constructor
    Pattern() : m_dim(0,0), m_data() {
    }
    //! return true if we does not have a pattern
    bool empty() const {
      if (m_dim[0]!=8 && m_dim[0]!=16 && m_dim[0]!=32) return true;
      return m_dim[1]==0 || m_data.size()!=size_t((m_dim[0]/8)*m_dim[1]);
    }
    /** tries to convert the picture in a binary data ( ppm) */
    bool getBinary(WPXBinaryData &data, std::string &type) const;

    /** compare two patterns */
    int cmp(Pattern const &a) const {
      int diff = m_dim.cmp(a.m_dim);
      if (diff) return diff;
      if (m_data.size() < a.m_data.size()) return -1;
      if (m_data.size() > a.m_data.size()) return 1;
      for (size_t h=0; h < m_data.size(); ++h) {
        if (m_data[h]<a.m_data[h]) return 1;
        if (m_data[h]>a.m_data[h]) return -1;
      }
      return 0;
    }
    //! a print operator
    friend std::ostream &operator<<(std::ostream &o, Pattern const &pat) {
      o << "dim=" << pat.m_dim << ",";
      o << "[";
      for (size_t h=0; h < pat.m_data.size(); ++h)
        o << std::hex << pat.m_data[h] << std::dec << ",";
      o << "],";
      return o;
    }
    //! the dimension width x height
    Vec2i m_dim;
    //! the pattern data: a sequence of data: p[0..7,0],p[8..15,0]...p[0..7,1],p[8..15,1], ...
    std::vector<unsigned char> m_data;
  };
  //! constructor
  MWAWGraphicStyle() :  m_lineWidth(1), m_lineDashWidth(), m_lineCap(C_Butt), m_lineJoin(J_Miter), m_lineOpacity(1), m_lineColor(MWAWColor::black()),
    m_fillRuleEvenOdd(false), m_surfaceColor(MWAWColor::white()), m_surfaceOpacity(0),
    m_shadowColor(MWAWColor::black()), m_shadowOpacity(0), m_shadowOffset(1,1),
    m_pattern(),
    m_gradientType(G_None), m_gradientStopList(), m_gradientAngle(0), m_gradientBorder(0), m_gradientPercentCenter(0.5f,0.5f), m_gradientRadius(1),
    m_extra("") {
    m_arrows[0]=m_arrows[1]=false;
    m_gradientStopList.push_back(GradientStop(0.0, MWAWColor::white()));
    m_gradientStopList.push_back(GradientStop(1.0, MWAWColor::black()));
  }
  //! virtual destructor
  virtual ~MWAWGraphicStyle() { }
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
  //! returns true if the pattern is defined
  bool hasPattern() const {
    return !m_pattern.empty();
  }
  //! returns true if the gradient is defined
  bool hasGradient() const {
    return m_gradientType != G_None && m_gradientStopList.size() >= 2;
  }
  //! a print operator
  friend std::ostream &operator<<(std::ostream &o, MWAWGraphicStyle const &st);
  //! add to propList
  void addTo(WPXPropertyList &pList, WPXPropertyListVector &gradient, bool only1d=false) const;

  /** compare two styles */
  int cmp(MWAWGraphicStyle const &a) const;

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

  //! the pattern if it exists
  Pattern m_pattern;

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
  //! extra data
  std::string m_extra;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
