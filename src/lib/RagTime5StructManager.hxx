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

#ifndef RAG_TIME_5_STRUCT_MANAGER
#  define RAG_TIME_5_STRUCT_MANAGER

#include <ostream>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"
#include "MWAWGraphicStyle.hxx"

class RagTime5Zone;

//! basic class used to store RagTime 5/6 structures
class RagTime5StructManager
{
public:
  struct Field;
  //! constructor
  RagTime5StructManager();
  //! destructor
  ~RagTime5StructManager();

  //! try to read a list of type definition
  bool readTypeDefinitions(RagTime5Zone &zone);
  //! try to read a field
  bool readField(MWAWInputStreamPtr input, long endPos, libmwaw::DebugFile &ascFile,
                 Field &field, long fSz=0);
  //! try to read a compressed long
  static bool readCompressedLong(MWAWInputStreamPtr &input, long endPos, long &val);
  //! try to read a unicode string
  static bool readUnicodeString(MWAWInputStreamPtr input, long endPos, librevenge::RVNGString &string);
  //! try to read n data id
  static bool readDataIdList(MWAWInputStreamPtr input, int n, std::vector<int> &listIds);

  //! a tabulation in RagTime 5/6 structures
  struct TabStop {
    //! constructor
    TabStop() : m_position(0), m_type(1), m_leader("")
    {
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, TabStop const &tab)
    {
      o << tab.m_position;
      switch (tab.m_type) {
      case 1:
        break;
      case 2:
        o << "R";
        break;
      case 3:
        o << "C";
        break;
      case 4:
        o << "D";
        break;
      case 5: // Kintou Waritsuke: sort of center
        o << "K";
        break;
      default:
        o << ":#type=" << tab.m_type;
        break;
      }
      if (!tab.m_leader.empty())
        o << ":leader=" << tab.m_leader.cstr();
      return o;
    }
    //! the position
    float m_position;
    //! the type
    int m_type;
    //! the leader char
    librevenge::RVNGString m_leader;
  };
  //! a field of RagTime 5/6 structures
  struct Field {
    //! the different type
    enum Type { T_Unknown, T_Bool, T_Double, T_Long, T_2Long, T_FieldList, T_LongList, T_DoubleList, T_TabList,
                T_Code, T_Color, T_CondColor, T_PrintInfo, T_String, T_Unicode, T_LongDouble, T_Unstructured
              };

    //! constructor
    Field() : m_type(T_Unknown), m_fileType(0), m_name(""), m_doubleValue(0), m_color(), m_string(""),
      m_longList(), m_doubleList(), m_numLongByData(1), m_tabList(), m_fieldList(), m_entry(), m_extra("")
    {
      for (int i=0; i<2; ++i) m_longValue[i]=0;
    }
    //! destructor
    ~Field()
    {
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Field const &field);
    //! the field type
    Type m_type;
    //! the file type
    long m_fileType;
    //! the field type name
    std::string m_name;
    //! the long value
    long m_longValue[2];
    //! the double value
    double m_doubleValue;
    //! the color
    MWAWColor m_color;
    //! small string use to store a string or a 4 char code
    librevenge::RVNGString m_string;
    //! the list of long value
    std::vector<long> m_longList;
    //! the list of double value
    std::vector<double> m_doubleList;
    //! the number of long by data (in m_longList)
    int m_numLongByData;
    //! the list of tabStop
    std::vector<TabStop> m_tabList;
    //! the list of field
    std::vector<Field> m_fieldList;
    //! entry to defined the position of a String or Unstructured data
    MWAWEntry m_entry;
    //! extra data
    std::string m_extra;
  };
  //! virtual class use to parse the field data
  struct FieldParser {
    //! constructor
    FieldParser() : m_regroupFields(false) {}
    //! destructor
    virtual ~FieldParser() {}
    //! return the debug name corresponding to a zone
    virtual std::string getZoneName() const=0;
    //! return the debug name corresponding to a field
    virtual std::string getZoneName(int n) const=0;
    //! parse a field
    virtual bool parseField(Field &field, RagTime5Zone &/*zone*/, int /*n*/, libmwaw::DebugStream &f)
    {
      f << field;
      return true;
    }
    //! a flag use to decide if we output one debug message by field or not
    bool m_regroupFields;
  private:
    FieldParser(FieldParser const &orig);
    FieldParser &operator=(FieldParser const &orig);
  };
  //! the graphic style of a RagTime v5-v6 document
  struct GraphicStyle {
    //! constructor
    GraphicStyle() : m_parentId(-1), m_width(-1), m_dash(), m_pattern(), m_gradient(0), m_gradientRotation(0), m_gradientCenter(0.5f,0.5f),
      m_position(2), m_cap(1), m_mitter(1), m_limitPercent(1), m_hidden(false), m_extra("")
    {
      m_colors[0]=MWAWColor::black();
      m_colors[1]=MWAWColor::white();
      m_colorsAlpha[0]=m_colorsAlpha[1]=1;
    }
    //! destructor
    virtual ~GraphicStyle()
    {
    }
    //! returns true if the line style is default
    bool isDefault() const
    {
      return m_parentId<0 && m_width<0 && m_dash.empty() && !m_pattern &&
             m_gradient==0 && m_gradientRotation<=0 && m_gradientRotation>=0 && m_gradientCenter!=Vec2f(0.5f, 0.5f) &&
             m_position==2 && m_cap==1 && m_mitter==1 &&
             m_colors[0].isBlack() && m_colors[1].isWhite() && m_colorsAlpha[0]>=1 && m_colorsAlpha[1]>=1 &&
             m_limitPercent>=1 && m_limitPercent<=1 && !m_hidden && m_extra.empty();
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, GraphicStyle const &style);
    //! try to read a line style
    bool read(MWAWInputStreamPtr &input, Field const &field);
    //! the parent id
    int m_parentId;
    //! the line width (in point)
    float m_width;
    //! the first and second color
    MWAWColor m_colors[2];
    //! alpha of the first and second color
    float m_colorsAlpha[2];
    //! the line dash/...
    std::vector<long> m_dash;
    //! the line pattern
    shared_ptr<MWAWGraphicStyle::Pattern> m_pattern;
    //! the gradient 0: none, normal, radial
    int m_gradient;
    //! the gradient rotation(checkme)
    float m_gradientRotation;
    //! the rotation center(checkme)
    Vec2f m_gradientCenter;
    //! the line position inside=1/normal/outside/round
    int m_position;
    //! the line caps ( normal=1, round, square)
    int m_cap;
    //! the line mitter ( triangle=1, round, out)
    int m_mitter;
    //! the line limit
    float m_limitPercent;
    //! flag to know if we need to print the shape
    bool m_hidden;
    //! extra data
    std::string m_extra;
  };
  //! the text style of a RagTime v5-v6 document
  struct TextStyle {
    //! constructor
    TextStyle() : m_linkIdList(),
      m_dateStyleId(-1), m_graphStyleId(-1), m_graphLineStyleId(-1), m_keepWithNext(false), m_justify(-1), m_breakMethod(-1), m_tabList(),
      m_fontName(""), m_fontId(-1), m_fontSize(-1), m_scriptPosition(0), m_fontScaling(-1), m_underline(-1), m_caps(-1), m_language(-1),
      m_numColumns(-1), m_columnGap(-1), m_extra("")
    {
      m_parentId[0]=m_parentId[1]=-1;
      m_fontFlags[0]=m_fontFlags[1]=0;
      for (int i=0; i<3; ++i) {
        m_margins[i]=-1;
        m_spacings[i]=-1;
        m_spacingUnits[i]=-1;
        m_letterSpacings[i]=-1;
      }
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
          m_fontScaling>=0 || m_underline>=0 || m_caps>=0 || m_language>=0 ||
          m_numColumns>=0 || m_columnGap>=0 || !m_extra.empty())
        return false;
      for (int i=0; i<3; ++i) {
        if (m_margins[i]>=0 || m_spacings[i]>=0 || m_spacingUnits[i]>=0 || m_letterSpacings[i]>=0)
          return false;
      }
      return true;
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, TextStyle const &style);
    //! try to read a line style
    bool read(MWAWInputStreamPtr &input, Field const &field);
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
    Variable<bool> m_keepWithNext;
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
    std::vector<TabStop> m_tabList;

    // character

    //! the font name
    librevenge::RVNGString m_fontName;
    //! the font id
    long m_fontId;
    //! the font size
    float m_fontSize;
    //! the font flags (add and remove )
    uint32_t m_fontFlags[2];
    //! the font script position ( in percent)
    Variable<float> m_scriptPosition;
    //! the font script position ( in percent)
    float m_fontScaling;
    //! underline : none, single, double
    int m_underline;
    //! caps : none, all caps, lower caps, inital caps + other lowers
    int m_caps;
    //! the language
    int m_language;
    //! the spacings in percent ( normal, minimum, maximum)
    double m_letterSpacings[3];

    // column

    //! the number of columns
    int m_numColumns;
    //! the gap between columns
    double m_columnGap;

    //! extra data
    std::string m_extra;
  };

private:
  RagTime5StructManager(RagTime5StructManager const &orig);
  RagTime5StructManager operator=(RagTime5StructManager const &orig);
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
