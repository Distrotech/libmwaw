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

#ifndef RAG_TIME_5_STYLE_MANAGER
#  define RAG_TIME_5_STYLE_MANAGER

#include <map>
#include <ostream>
#include <sstream>
#include <set>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"
#include "MWAWGraphicStyle.hxx"

#include "RagTime5ClusterManager.hxx"
#include "RagTime5StructManager.hxx"

namespace RagTime5StyleManagerInternal
{
struct State;
}

class RagTime5Parser;

//! basic class used to read/store RagTime 5/6 styles
class RagTime5StyleManager
{
  friend class RagTime5Parser;
public:
  //! constructor
  RagTime5StyleManager(RagTime5Parser &parser);
  //! destructor
  ~RagTime5StyleManager();

  struct GraphicStyle;
  struct TextStyle;

  //! try to read a graphic color zone
  bool readGraphicColors(RagTime5ClusterManager::Cluster &cluster);
  //! try to read a main graphic styles
  bool readGraphicStyles(RagTime5ClusterManager::Cluster &cluster);
  //! try to read a main text styles
  bool readTextStyles(RagTime5ClusterManager::Cluster &cluster);

  //! returns the line color corresponding to a graphic style
  bool getLineColor(int graphicId, MWAWColor &color);
  //! update the font and the paragraph properties using a text style
  bool update(int textId, MWAWFont &font, MWAWParagraph &para);

protected:
  //! recursive function use to update the style list
  void updateTextStyles(size_t id, RagTime5StyleManager::TextStyle const &style,
                        std::vector<RagTime5StyleManager::TextStyle> const &listReadStyles,
                        std::multimap<size_t, size_t> const &idToChildIpMap,
                        std::set<size_t> &seens);
  //! recursive function use to update the style list
  void updateGraphicStyles(size_t id, RagTime5StyleManager::GraphicStyle const &style,
                           std::vector<RagTime5StyleManager::GraphicStyle> const &listReadStyles,
                           std::multimap<size_t, size_t> const &idToChildIpMap,
                           std::set<size_t> &seens);

public:
  //! the graphic style of a RagTime v5-v6 document
  struct GraphicStyle {
    //! constructor
    GraphicStyle() : m_parentId(-1000), m_width(-1), m_dash(), m_pattern(), m_gradient(-1), m_gradientRotation(-1000), m_gradientCenter(MWAWVec2f(0.5f,0.5f)),
      m_position(-1), m_cap(1), m_mitter(-1), m_limitPercent(-1), m_hidden(false), m_extra("")
    {
      m_colors[0]=MWAWVariable<MWAWColor>(MWAWColor::black());
      m_colors[1]=MWAWVariable<MWAWColor>(MWAWColor::white());
      m_colorsAlpha[0]=m_colorsAlpha[1]=-1;
    }
    //! destructor
    virtual ~GraphicStyle()
    {
    }
    //! returns true if the line style is default
    bool isDefault() const
    {
      return m_parentId<=-1000 && m_width<0 && !m_dash.isSet() && !m_pattern &&
             m_gradient<0 && m_gradientRotation<=-1000 && !m_gradientCenter.isSet() &&
             m_position<0 && m_cap<0 && m_mitter<0 &&
             !m_colors[0].isSet() && !m_colors[1].isSet() && m_colorsAlpha[0]<0 && m_colorsAlpha[1]<0 &&
             m_limitPercent<0 && !m_hidden.isSet() && m_extra.empty();
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, GraphicStyle const &style);
    //! update the current style
    void insert(GraphicStyle const &childStyle);
    //! try to read a line style
    bool read(MWAWInputStreamPtr &input, RagTime5StructManager::Field const &field);
    //! the parent id
    int m_parentId;
    //! the line width (in point)
    float m_width;
    //! the first and second color
    MWAWVariable<MWAWColor> m_colors[2];
    //! alpha of the first and second color
    float m_colorsAlpha[2];
    //! the line dash/...
    MWAWVariable<std::vector<long> > m_dash;
    //! the line pattern
    shared_ptr<MWAWGraphicStyle::Pattern> m_pattern;
    //! the gradient 0: none, normal, radial
    int m_gradient;
    //! the gradient rotation(checkme)
    float m_gradientRotation;
    //! the rotation center(checkme)
    MWAWVariable<MWAWVec2f> m_gradientCenter;
    //! the line position inside=1/normal/outside/round
    int m_position;
    //! the line caps ( normal=1, round, square)
    int m_cap;
    //! the line mitter ( triangle=1, round, out)
    int m_mitter;
    //! the line limit
    float m_limitPercent;
    //! flag to know if we need to print the shape
    MWAWVariable<bool> m_hidden;
    //! extra data
    std::string m_extra;
  };
  //! the text style of a RagTime v5-v6 document
  struct TextStyle {
    //! constructor
    TextStyle() : m_linkIdList(),
      m_dateStyleId(-1), m_graphStyleId(-1), m_graphLineStyleId(-1), m_keepWithNext(false), m_justify(-1), m_breakMethod(-1), m_tabList(),
      m_fontName(""), m_fontId(-1), m_fontSize(-1), m_scriptPosition(0), m_fontScaling(-1), m_underline(-1), m_caps(-1), m_language(-1), m_widthStreching(-1),
      m_numColumns(-1), m_columnGap(-1), m_extra("")
    {
      m_parentId[0]=m_parentId[1]=-1;
      m_fontFlags[0]=m_fontFlags[1]=0;
      for (int i=0; i<3; ++i) {
        m_margins[i]=-1;
        m_spacings[i]=-1;
        m_spacingUnits[i]=-1;
      }
      for (int i=0; i<4; ++i) m_letterSpacings[i]=0;
    }
    //! destructor
    virtual ~TextStyle()
    {
    }
    //! returns true if the line style is default
    bool isDefault() const
    {
      if (m_parentId[0]>=0 || m_parentId[1]>=0 || !m_linkIdList.empty() ||
          m_dateStyleId>=0 || m_graphStyleId>=0 || m_graphLineStyleId>=0 ||
          m_keepWithNext.isSet() || m_justify>=0 || m_breakMethod>=0 || !m_tabList.empty() ||
          !m_fontName.empty() || m_fontId>=0 || m_fontSize>=0 || m_fontFlags[0] || m_fontFlags[1] || m_scriptPosition.isSet() ||
          m_fontScaling>=0 || m_underline>=0 || m_caps>=0 || m_language>=0 || m_widthStreching>=0 ||
          m_numColumns>=0 || m_columnGap>=0 || !m_extra.empty())
        return false;
      for (int i=0; i<3; ++i) {
        if (m_margins[i]>=0 || m_spacings[i]>=0 || m_spacingUnits[i]>=0)
          return false;
      }
      for (int i=0; i<4; ++i) {
        if (m_letterSpacings[i]>0 || m_letterSpacings[i]<0)
          return false;
      }
      return true;
    }
    //! returns the language locale name corresponding to an id ( if known)
    static std::string getLanguageLocale(int id);

    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, TextStyle const &style);
    //! update the current style
    void insert(TextStyle const &childStyle);
    //! try to read a line style
    bool read(RagTime5StructManager::Field const &field);
    //! the parent id ( main and style ?)
    int m_parentId[2];
    //! the link id list
    std::vector<int> m_linkIdList;
    //! the date style id
    int m_dateStyleId;
    //! the graphic style id
    int m_graphStyleId;
    //! the graphic line style id
    int m_graphLineStyleId;

    // paragraph

    //! the keep with next flag
    MWAWVariable<bool> m_keepWithNext;
    //! justify 0: left, 1:center, 2:right, 3:full, 4:full all
    int m_justify;
    //! the interline/before/after value
    double m_spacings[3];
    //! the interline/before/after unit 0: line, 1:point
    int m_spacingUnits[3];
    //! the break method 0: asIs, next container, next page, next even page, next odd page
    int m_breakMethod;
    //! the spacings in point ( left, right, first)
    double m_margins[3];
    //! the tabulations
    std::vector<RagTime5StructManager::TabStop> m_tabList;

    // character

    //! the font name
    librevenge::RVNGString m_fontName;
    //! the font id
    int m_fontId;
    //! the font size
    float m_fontSize;
    //! the font flags (add and remove )
    uint32_t m_fontFlags[2];
    //! the font script position ( in percent)
    MWAWVariable<float> m_scriptPosition;
    //! the font script position ( in percent)
    float m_fontScaling;
    //! underline : none, single, double
    int m_underline;
    //! caps : none, all caps, lower caps, inital caps + other lowers
    int m_caps;
    //! the language
    int m_language;
    //! the spacings in percent ( normal, minimum, maximum)
    double m_letterSpacings[4];
    //! the width streching
    double m_widthStreching;

    // column

    //! the number of columns
    int m_numColumns;
    //! the gap between columns
    double m_columnGap;

    //! extra data
    std::string m_extra;
  };

protected:
  //
  // data
  //

  //! the parser
  RagTime5Parser &m_mainParser;
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<RagTime5StyleManagerInternal::State> m_state;

private:
  RagTime5StyleManager(RagTime5StyleManager const &orig);
  RagTime5StyleManager operator=(RagTime5StyleManager const &orig);
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
