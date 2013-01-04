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

#ifndef MWAW_FONT
#  define MWAW_FONT

#include <assert.h>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"

class MWAWContentListener;
class MWAWFontConverter;

//! Class to store font
class MWAWFont
{
public:
  /** a small struct to define a line in MWAWFont */
  struct Line {
    /** the line style */
    enum Style { None, Single, Double, Dot, LargeDot, Dash };
    //! return the properties corresponding to a style line
    static std::string getPropertyValue(Style const &style);
  };
  /** a small struct to define the script position in MWAWFont */
  struct Script {
    //! constructor
    Script(int delta=0, WPXUnit deltaUnit=WPX_PERCENT, int scale=100) :
      m_delta(delta), m_deltaUnit(deltaUnit), m_scale(scale) {
    }
    //! return true if the position is not default
    bool isSet() const {
      return *this != Script();
    }
    //! return a yposition which correspond to a basic subscript
    static Script sub() {
      return Script(-33,WPX_PERCENT,58);
    }
    //! return a yposition which correspond to a basic subscript100
    static Script sub100() {
      return Script(-20);
    }
    //! return a yposition which correspond to a basic superscript
    static Script super() {
      return Script(33,WPX_PERCENT,58);
    }
    //! return a yposition which correspond to a basic superscript100
    static Script super100() {
      return Script(20);
    }
    //! return a string which correspond to style:text-position
    std::string str(int fSize) const;

    //! operator==
    bool operator==(Script const &oth) const {
      return cmp(oth)==0;
    }
    //! operator!=
    bool operator!=(Script const &oth) const {
      return cmp(oth)!=0;
    }
    //! operator<
    bool operator<(Script const &oth) const {
      return cmp(oth)<0;
    }
    //! operator<=
    bool operator<=(Script const &oth) const {
      return cmp(oth)<=0;
    }
    //! operator>
    bool operator>(Script const &oth) const {
      return cmp(oth)>0;
    }
    //! operator>=
    bool operator>=(Script const &oth) const {
      return cmp(oth)>=0;
    }
    //! small comparison function
    int cmp(Script const &oth) const {
      if (m_delta != oth.m_delta) return m_delta-oth.m_delta;
      if (m_deltaUnit != oth.m_deltaUnit) return int(m_deltaUnit)-int(oth.m_deltaUnit);
      if (m_scale != oth.m_scale) return m_scale-oth.m_scale;
      return 0;
    }
    //! the ydelta
    int m_delta;
    //! the ydelta unit ( point or percent )
    WPXUnit m_deltaUnit;
    //! the font scaling ( in percent )
    int m_scale;
  };

  //! the different font bit
  enum FontBits { boldBit=1, italicBit=2, blinkBit=4, embossBit=8, engraveBit=0x10,
                  hiddenBit=0x20, outlineBit=0x40, shadowBit=0x80,
                  reverseVideoBit=0x100, smallCapsBit=0x200, allCapsBit=0x400,
                  strikeOutBit=0x10000, overlineBit=0x20000
                };
  /** constructor
   *
   * \param newId system id font
   * \param sz the font size
   * \param f the font attributes bold, ... */
  MWAWFont(int newId=-1, int sz=12, uint32_t f = 0) : m_id(newId), m_size(sz), m_deltaSpacing(0), m_scriptPosition(),
    m_flags(f), m_underline(Line::None), m_color(MWAWColor::black()), m_backgroundColor(MWAWColor::white()), m_extra("") {
    resetColor();
  };
  //! returns true if the font id is initialized
  bool isSet() const {
    return m_id.isSet();
  }
  //! inserts the set value in the current font
  void insert(MWAWFont &ft) {
    m_id.insert(ft.m_id);
    m_size.insert(ft.m_size);
    m_deltaSpacing.insert(ft.m_deltaSpacing);
    m_scriptPosition.insert(ft.m_scriptPosition);
    if (ft.m_flags.isSet()) {
      if (m_flags.isSet())
        setFlags(flags()| ft.flags());
      else
        m_flags = ft.m_flags;
    }
    m_underline.insert(ft.m_underline);
    m_color.insert(ft.m_color);
    m_extra += ft.m_extra;
  }
  //! sets the font id and resets size to the previous size for this font
  void setFont(int newId) {
    resetColor();
    m_id=newId;
  }

  //! returns the font id
  int id() const {
    return m_id.get();
  }
  //! sets the font id
  void setId(int newId) {
    m_id = newId;
  }

  //! returns the font size
  int size() const {
    return m_size.get();
  }
  //! sets the font size
  void setSize(int sz) {
    m_size = sz;
  }

  //! returns the condensed(negative)/extended(positive) width
  int deltaLetterSpacing() const {
    return m_deltaSpacing.get();
  }
  //! set the letter spacing ( delta value in point )
  void setDeltaLetterSpacing(int d) {
    m_deltaSpacing=d;
  }

  //! returns the script position
  Script const &script() const {
    return m_scriptPosition.get();
  }

  //! sets the script position
  void set(Script const &newscript) {
    m_scriptPosition = newscript;
  }

  //! returns the font flags
  uint32_t flags() const {
    return m_flags.get();
  }
  //! sets the font attributes bold, ...
  void setFlags(uint32_t fl) {
    m_flags = fl;
  }

  //! returns true if the font color is not black
  bool hasColor() const {
    return m_color.isSet() && !m_color.get().isBlack();
  }
  //! returns the font color
  void getColor(MWAWColor &c) const {
    c = m_color.get();
  }
  //! sets the font color
  void setColor(MWAWColor color) {
    m_color = color;
  }

  //! returns the font background color
  void getBackgroundColor(MWAWColor &c) const {
    c = m_backgroundColor.get();
  }
  //! sets the font background color
  void setBackgroundColor(MWAWColor color) {
    m_backgroundColor = color;
  }
  //! resets the font color to black and the background color to white
  void resetColor() {
    m_color = MWAWColor::black();
    m_backgroundColor = MWAWColor::white();
  }

  //! returns the underline style
  Line::Style getUnderlineStyle() const {
    return m_underline.get();
  }
  //! sets the underline style
  void setUnderlineStyle(Line::Style style=Line::None) {
    m_underline = style;
  }

  //! returns a string which can be used for debugging
  std::string getDebugString(shared_ptr<MWAWFontConverter> &converter) const;

  //! operator==
  bool operator==(MWAWFont const &f) const {
    return cmp(f) == 0;
  }
  //! operator!=
  bool operator!=(MWAWFont const &f) const {
    return cmp(f) != 0;
  }

  //! a comparison function
  int cmp(MWAWFont const &oth) const {
    int diff = id() - oth.id();
    if (diff != 0) return diff;
    diff = size() - oth.size();
    if (diff != 0) return diff;
    if (flags() < oth.flags()) return -1;
    if (flags() > oth.flags()) return 1;
    if (m_deltaSpacing.get() < oth.m_deltaSpacing.get()) return -1;
    if (m_deltaSpacing.get() > oth.m_deltaSpacing.get()) return 1;
    diff = script().cmp(oth.script());
    if (diff != 0) return diff;
    if (m_underline.get() < oth.m_underline.get()) return -1;
    if (m_underline.get() > oth.m_underline.get()) return 1;
    if (m_color.get() < oth.m_color.get()) return -1;
    if (m_color.get() > oth.m_color.get()) return 1;
    if (m_backgroundColor.get() < oth.m_backgroundColor.get()) return -1;
    if (m_backgroundColor.get() > oth.m_backgroundColor.get()) return 1;
    return diff;
  }

  /** sends font to a listener */
  void sendTo(MWAWContentListener *listener, shared_ptr<MWAWFontConverter> &convert, MWAWFont &actFont) const;

protected:
  Variable<int> m_id /** font identificator*/, m_size /** font size */;
  Variable<int> m_deltaSpacing /** expand(>0), condensed(<0) depl in point*/;
  Variable<Script> m_scriptPosition /** the sub/super script definition */;
  Variable<uint32_t> m_flags /** font attributes */;
  Variable<Line::Style> m_underline /** underline attributes */;
  Variable<MWAWColor> m_color /** font color */;
  Variable<MWAWColor> m_backgroundColor /** font background color */;
public:
  //! extra data
  std::string m_extra;
};


#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
