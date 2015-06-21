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

#  include "librevenge/librevenge.h"
#  include "libmwaw_internal.hxx"

/** a structure used to define a picture style

 \note in order to define the internal surface style, first it looks for
 a gradient, if so it uses it. Then it looks for a pattern. Finally if
 it found nothing, it uses surfaceColor and surfaceOpacity.*/
class MWAWGraphicStyle
{
public:
  //! an enum used to define the basic line cap
  enum LineCap { C_Butt, C_Square, C_Round };
  //! an enum used to define the basic line join
  enum LineJoin { J_Miter, J_Round, J_Bevel };
  //! an enum used to define the gradient type
  enum GradientType { G_None, G_Axial, G_Linear, G_Radial, G_Rectangular, G_Square, G_Ellipsoid };

  //! a structure used to define an arrow
  struct Arrow {
    //! constructor ( no arrow)
    Arrow() : m_type(0)
    {
    }
    //! returns a basic plain arrow
    static Arrow plain()
    {
      Arrow arrow;
      arrow.m_type=1;
      return arrow;
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Arrow const &arrow)
    {
      if (arrow.isEmpty()) return o;
      o << "plain,";
      return o;
    }
    //! operator==
    bool operator==(Arrow const &arrow) const
    {
      return m_type==arrow.m_type;
    }
    //! operator!=
    bool operator!=(Arrow const &arrow) const
    {
      return !(*this==arrow);
    }
    //! operator<
    bool operator<(Arrow const &arrow) const
    {
      return m_type<arrow.m_type;
    }
    //! operator<=
    bool operator<=(Arrow const &arrow) const
    {
      return *this<arrow || *this==arrow;
    }
    //! operator>
    bool operator>(Arrow const &arrow) const
    {
      return !(*this<=arrow);
    }
    //! operator>=
    bool operator>=(Arrow const &arrow) const
    {
      return !(*this<arrow);
    }
    //! returns true if there is no arrow
    bool isEmpty() const
    {
      return m_type==0;
    }
    //! add a arrow to the propList knowing the type (start, end)
    void addTo(librevenge::RVNGPropertyList &propList, std::string const &type) const;

  protected:
    //! the arrow type
    int m_type;
  };

  //! a structure used to define the gradient limit in MWAWGraphicStyle
  struct GradientStop {
    //! constructor
    GradientStop(float offset=0.0, MWAWColor const &col=MWAWColor::black(), float opacity=1.0) :
      m_offset(offset), m_color(col), m_opacity(opacity)
    {
    }
    /** compare two gradient */
    int cmp(GradientStop const &a) const
    {
      if (m_offset < a.m_offset) return -1;
      if (m_offset > a.m_offset) return 1;
      if (m_color < a.m_color) return -1;
      if (m_color > a.m_color) return 1;
      if (m_opacity < a.m_opacity) return -1;
      if (m_opacity > a.m_opacity) return 1;
      return 0;
    }
    //! a print operator
    friend std::ostream &operator<<(std::ostream &o, GradientStop const &st)
    {
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
  /** a basic pattern used in a MWAWGraphicStyle:
      - either given a list of 8x8, 16x16, 32x32 bytes with two colors
      - or with a picture ( and an average color)
   */
  struct Pattern {
    //! constructor
    Pattern() : m_dim(0,0), m_data(), m_picture(), m_pictureMime(""), m_pictureAverageColor(MWAWColor::white())
    {
      m_colors[0]=MWAWColor::black();
      m_colors[1]=MWAWColor::white();
    }
    //!  constructor from a binary data
    Pattern(MWAWVec2i dim, librevenge::RVNGBinaryData const &picture, std::string const &mime, MWAWColor const &avColor) :
      m_dim(dim), m_data(), m_picture(picture), m_pictureMime(mime), m_pictureAverageColor(avColor)
    {
      m_colors[0]=MWAWColor::black();
      m_colors[1]=MWAWColor::white();
    }
    //! virtual destructor
    virtual ~Pattern() {}
    //! return true if we does not have a pattern
    bool empty() const
    {
      if (m_dim[0]==0 || m_dim[1]==0) return true;
      if (m_picture.size()) return false;
      if (m_dim[0]!=8 && m_dim[0]!=16 && m_dim[0]!=32) return true;
      return m_data.size()!=size_t((m_dim[0]/8)*m_dim[1]);
    }
    //! return the average color
    bool getAverageColor(MWAWColor &col) const;
    //! check if the pattern has only one color; if so returns true...
    bool getUniqueColor(MWAWColor &col) const;
    /** tries to convert the picture in a binary data ( ppm) */
    bool getBinary(librevenge::RVNGBinaryData &data, std::string &type) const;

    /** compare two patterns */
    int cmp(Pattern const &a) const
    {
      int diff = m_dim.cmp(a.m_dim);
      if (diff) return diff;
      if (m_data.size() < a.m_data.size()) return -1;
      if (m_data.size() > a.m_data.size()) return 1;
      for (size_t h=0; h < m_data.size(); ++h) {
        if (m_data[h]<a.m_data[h]) return 1;
        if (m_data[h]>a.m_data[h]) return -1;
      }
      for (int i=0; i<2; ++i) {
        if (m_colors[i] < a.m_colors[i]) return 1;
        if (m_colors[i] > a.m_colors[i]) return -1;
      }
      if (m_pictureAverageColor < a.m_pictureAverageColor) return 1;
      if (m_pictureAverageColor > a.m_pictureAverageColor) return -1;
      if (m_pictureMime < a.m_pictureMime) return 1;
      if (m_pictureMime > a.m_pictureMime) return -1;
      if (m_picture.size() < a.m_picture.size()) return 1;
      if (m_picture.size() > a.m_picture.size()) return -1;
      const unsigned char *ptr=m_picture.getDataBuffer();
      const unsigned char *aPtr=a.m_picture.getDataBuffer();
      if (!ptr || !aPtr) return 0; // must only appear if the two buffers are empty
      for (unsigned long h=0; h < m_picture.size(); ++h, ++ptr, ++aPtr) {
        if (*ptr < *aPtr) return 1;
        if (*ptr > *aPtr) return -1;
      }
      return 0;
    }
    //! a print operator
    friend std::ostream &operator<<(std::ostream &o, Pattern const &pat)
    {
      o << "dim=" << pat.m_dim << ",";
      if (pat.m_picture.size()) {
        o << "type=" << pat.m_pictureMime << ",";
        o << "col[average]=" << pat.m_pictureAverageColor << ",";
      }
      else {
        if (!pat.m_colors[0].isBlack()) o << "col0=" << pat.m_colors[0] << ",";
        if (!pat.m_colors[1].isWhite()) o << "col1=" << pat.m_colors[1] << ",";
        o << "[";
        for (size_t h=0; h < pat.m_data.size(); ++h)
          o << std::hex << (int) pat.m_data[h] << std::dec << ",";
        o << "],";
      }
      return o;
    }
    //! the dimension width x height
    MWAWVec2i m_dim;

    //! the two indexed colors
    MWAWColor m_colors[2];
    //! the pattern data: a sequence of data: p[0..7,0],p[8..15,0]...p[0..7,1],p[8..15,1], ...
    std::vector<unsigned char> m_data;
  protected:
    //! a picture
    librevenge::RVNGBinaryData m_picture;
    //! the picture type
    std::string m_pictureMime;
    //! the picture average color
    MWAWColor m_pictureAverageColor;
  };
  //! constructor
  MWAWGraphicStyle() :  m_lineWidth(1), m_lineDashWidth(), m_lineCap(C_Butt), m_lineJoin(J_Miter), m_lineOpacity(1), m_lineColor(MWAWColor::black()),
    m_fillRuleEvenOdd(false), m_surfaceColor(MWAWColor::white()), m_surfaceOpacity(0),
    m_shadowColor(MWAWColor::black()), m_shadowOpacity(0), m_shadowOffset(1,1),
    m_pattern(),
    m_gradientType(G_None), m_gradientStopList(), m_gradientAngle(0), m_gradientBorder(0), m_gradientPercentCenter(0.5f,0.5f), m_gradientRadius(1),
    m_backgroundColor(MWAWColor::white()), m_backgroundOpacity(-1), m_bordersList(), m_frameName(""), m_frameNextName(""),
    m_rotate(0), m_extra("")
  {
    m_arrows[0]=m_arrows[1]=Arrow();
    m_flip[0]=m_flip[1]=false;
    m_gradientStopList.push_back(GradientStop(0.0, MWAWColor::white()));
    m_gradientStopList.push_back(GradientStop(1.0, MWAWColor::black()));
  }
  /** returns an empty style. Can be used to initialize a default frame style...*/
  static MWAWGraphicStyle emptyStyle()
  {
    MWAWGraphicStyle res;
    res.m_lineWidth=0;
    return res;
  }
  //! virtual destructor
  virtual ~MWAWGraphicStyle() { }
  //! returns true if the border is defined
  bool hasLine() const
  {
    return m_lineWidth>0 && m_lineOpacity>0;
  }
  //! set the surface color
  void setSurfaceColor(MWAWColor const &col, float opacity = 1)
  {
    m_surfaceColor = col;
    m_surfaceOpacity = opacity;
  }
  //! returns true if the surface is defined
  bool hasSurfaceColor() const
  {
    return m_surfaceOpacity > 0;
  }
  //! set the pattern
  void setPattern(Pattern const &pat, float opacity = 1)
  {
    m_pattern=pat;
    m_surfaceOpacity = opacity;
  }
  //! returns true if the pattern is defined
  bool hasPattern() const
  {
    return !m_pattern.empty() && m_surfaceOpacity > 0;
  }
  //! returns true if the gradient is defined
  bool hasGradient(bool complex=false) const
  {
    return m_gradientType != G_None && (int) m_gradientStopList.size() >= (complex ? 3 : 2);
  }
  //! returns true if the interior surface is defined
  bool hasSurface() const
  {
    return hasSurfaceColor() || hasPattern() || hasGradient();
  }
  //! set the background color
  void setBackgroundColor(MWAWColor const &col, float opacity = 1)
  {
    m_backgroundColor = col;
    m_backgroundOpacity = opacity;
  }
  //! returns true if the background is defined
  bool hasBackgroundColor() const
  {
    return m_backgroundOpacity > 0;
  }
  //! set the shadow color
  void setShadowColor(MWAWColor const &col, float opacity = 1)
  {
    m_shadowColor = col;
    m_shadowOpacity = opacity;
  }
  //! returns true if the shadow is defined
  bool hasShadow() const
  {
    return m_shadowOpacity > 0;
  }
  //! return true if the frame has some border
  bool hasBorders() const
  {
    return !m_bordersList.empty();
  }
  //! return true if the frame has some border
  bool hasSameBorders() const
  {
    if (m_bordersList.empty()) return true;
    if (m_bordersList.size()!=4) return false;
    for (size_t i=1; i<m_bordersList.size(); ++i) {
      if (m_bordersList[i]!=m_bordersList[0])
        return false;
    }
    return true;
  }
  //! return the frame border: libmwaw::Left | ...
  std::vector<MWAWBorder> const &borders() const
  {
    return m_bordersList;
  }
  //! reset the border
  void resetBorders()
  {
    m_bordersList.resize(0);
  }
  //! sets the cell border: wh=libmwaw::LeftBit|...
  void setBorders(int wh, MWAWBorder const &border);
  //! a print operator
  friend std::ostream &operator<<(std::ostream &o, MWAWGraphicStyle const &st);
  //! add all the parameters to the propList excepted the frame parameter: the background and the borders
  void addTo(librevenge::RVNGPropertyList &pList, bool only1d=false) const;
  //! add all the frame parameters to propList: the background and the borders
  void addFrameTo(librevenge::RVNGPropertyList &pList) const;
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
  MWAWVec2f m_shadowOffset;

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
  MWAWVec2f m_gradientPercentCenter;
  //! the gradient radius
  float m_gradientRadius;

  //! the two arrows corresponding to start and end extremity
  Arrow m_arrows[2];

  //
  // related to the frame
  //

  //! the background color
  MWAWColor m_backgroundColor;
  //! true if the background has some color
  float m_backgroundOpacity;
  //! the borders MWAWBorder::Pos (for a frame)
  std::vector<MWAWBorder> m_bordersList;
  //! the frame name
  std::string m_frameName;
  //! the frame next name (if there is a link)
  std::string m_frameNextName;

  //
  // some transformation: must probably be somewhere else
  //

  //! the rotation
  float m_rotate;
  //! two bool to indicated we need to flip the shape or not
  bool m_flip[2];

  //! extra data
  std::string m_extra;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
