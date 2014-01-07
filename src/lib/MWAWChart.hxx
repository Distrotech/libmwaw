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

/*
 * Structure to store and construct a chart from an unstructured list
 * of cell
 *
 */

#ifndef MWAW_CHART
#  define MWAW_CHART

#include <iostream>
#include <vector>
#include <map>

#include "libmwaw_internal.hxx"

#include "MWAWEntry.hxx"
#include "MWAWFont.hxx"
#include "MWAWGraphicStyle.hxx"

namespace MWAWChartInternal
{
class SubDocument;
}
/** a class used to store a chart associated to a spreadsheet .... */
class MWAWChart
{
  friend class MWAWChartInternal::SubDocument;
public:
  //! a axis in a chart
  struct Axis {
    //! the axis content
    enum Type { A_None, A_Numeric, A_Logarithmic, A_Sequence, A_Sequence_Skip_Empty };
    //! constructor
    Axis();
    //! destructor
    ~Axis();
    //! add content to the propList
    void addContentTo(std::string const &sheetName, int coord, librevenge::RVNGPropertyList &propList) const;
    //! add style to the propList
    void addStyleTo(librevenge::RVNGPropertyList &propList) const;
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Axis const &axis);
    //! the sequence type
    Type m_type;
    //! show or not the grid
    bool m_showGrid;
    //! show or not the label
    bool m_showLabel;
    //! the label range if defined
    Box2i m_labelRange;
    //! the graphic style
    MWAWGraphicStyle m_style;
  };
  //! a legend in a chart
  struct Legend {
    //! constructor
    Legend() : m_show(false), m_autoPosition(true), m_relativePosition(libmwaw::RightBit), m_position(0,0), m_font(), m_style()
    {
    }
    //! add content to the propList
    void addContentTo(librevenge::RVNGPropertyList &propList) const;
    //! add style to the propList
    void addStyleTo(librevenge::RVNGPropertyList &propList, shared_ptr<MWAWFontConverter> fontConverter) const;
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Legend const &legend);
    //! show or not the legend
    bool m_show;
    //! automatic position
    bool m_autoPosition;
    //! the automatic position libmwaw::LeftBit|...
    int m_relativePosition;
    //! the position in points
    Vec2f m_position;
    //! the font
    MWAWFont m_font;
    //! the graphic style
    MWAWGraphicStyle m_style;
  };
  //! a series in a chart
  struct Series {
    //! the series type
    enum Type { S_Area, S_Bar, S_Column, S_Line, S_Pie, S_Scatter, S_Stock };
    //! constructor
    Series();
    //! destructor
    virtual ~Series();
    //! add content to the propList
    void addContentTo(std::string const &sheetName, librevenge::RVNGPropertyList &propList) const;
    //! add style to the propList
    void addStyleTo(librevenge::RVNGPropertyList &propList) const;
    //! returns a string corresponding to a series type
    static std::string getSeriesTypeName(Type type);
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Series const &series);
    //! the type
    Type m_type;
    //! the data range
    Box2i m_range;
    //! the graphic style
    MWAWGraphicStyle m_style;
  };
  //! a text zone a chart
  struct TextZone {
    //! the text type
    enum Type { T_Title, T_SubTitle, T_AxisX, T_AxisY, T_AxisZ };
    //! the text content type
    enum ContentType { C_Cell, C_Text };

    //! constructor
    TextZone();
    //! destructor
    ~TextZone();
    //! add content to the propList
    void addContentTo(std::string const &sheetName, librevenge::RVNGPropertyList &propList) const;
    //! add to the propList
    void addStyleTo(librevenge::RVNGPropertyList &propList, shared_ptr<MWAWFontConverter> fontConverter) const;
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, TextZone const &zone);
    //! the zone type
    Type m_type;
    //! the content type
    ContentType m_contentType;
    //! the position in the zone
    Vec2f m_position;
    //! the cell position ( for title and subtitle )
    Vec2i m_cell;
    //! the text entry
    MWAWEntry m_textEntry;
    //! the zone format
    MWAWFont m_font;
    //! the graphic style
    MWAWGraphicStyle m_style;
  };

  //! the constructor
  MWAWChart(std::string const &sheetName, MWAWFontConverterPtr fontConverter, Vec2f const &dim=Vec2f());
  //! the destructor
  virtual ~MWAWChart();
  //! send the chart to the listener
  void sendChart(MWAWSpreadsheetListenerPtr &listener, librevenge::RVNGSpreadsheetInterface *interface);
  //! send the zone content (called when the zone is of text type)
  virtual void sendContent(TextZone const &zone, MWAWListenerPtr &listener)=0;

  //! sets the chart type
  void setDataType(Series::Type type, bool dataStacked)
  {
    m_type=type;
    m_dataStacked=dataStacked;
  }

  //! return the chart dimension
  Vec2f const &getDimension() const
  {
    return m_dim;
  }
  //! return the chart dimension
  void setDimension(Vec2f const &dim)
  {
    m_dim=dim;
  }
  //! adds an axis (corresponding to a coord)
  void add(int coord, Axis const &axis);
  //! return an axis (corresponding to a coord)
  Axis const &getAxis(int coord) const;

  //! set the legend
  void set(Legend const &legend)
  {
    m_legend=legend;
  }
  //! return the legend
  Legend const &getLegend() const
  {
    return m_legend;
  }

  //! adds a series
  void add(Series const &series);
  //! return the list of series
  std::vector<Series> const &getSeries() const
  {
    return m_seriesList;
  }

  //! adds a textzone
  void add(TextZone const &textZone);
  //! returns a textzone content(if set)
  bool getTextZone(TextZone::Type type, TextZone &textZone);

protected:
  //! sends a textzone content
  void sendTextZoneContent(TextZone::Type type, MWAWListenerPtr &listener);

protected:
  //! the sheet name
  std::string m_sheetName;
  //! the chart dimension in point
  Vec2f m_dim;
  //! the chart type (if no series)
  Series::Type m_type;
  //! a flag to know if the data are stacked or not
  bool m_dataStacked;
  //! the x,y,z axis
  Axis m_axis[3];
  //! the legend
  Legend m_legend;
  //! the list of series
  std::vector<Series> m_seriesList;
  //! a map text zone type to text zone
  std::map<TextZone::Type, TextZone> m_textZoneMap;
  //! the font converter
  MWAWFontConverterPtr m_fontConverter;
private:
  MWAWChart(MWAWChart const &orig);
  MWAWChart &operator=(MWAWChart const &orig);
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
