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

#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWFontConverter.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWListener.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSpreadsheetListener.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWChart.hxx"

/** Internal: the structures of a MWAWChart */
namespace MWAWChartInternal
{
////////////////////////////////////////
//! Internal: the subdocument of a MWAWChart
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MWAWChart *chart, MWAWChart::TextZone::Type textZone) :
    MWAWSubDocument(0, MWAWInputStreamPtr(), MWAWEntry()), m_chart(chart), m_textZone(textZone)
  {
  }

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const
  {
    if (MWAWSubDocument::operator!=(doc))
      return true;
    SubDocument const *subDoc=dynamic_cast<SubDocument const *>(&doc);
    if (!subDoc) return true;
    return m_chart!=subDoc->m_chart || m_textZone!=subDoc->m_textZone;
  }
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const
  {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type);
protected:
  //! the chart
  MWAWChart *m_chart;
  //! the textzone type
  MWAWChart::TextZone::Type m_textZone;
private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("MWAWChartInternal::SubDocument::parse: no listener\n"));
    return;
  }

  if (!m_chart) {
    MWAW_DEBUG_MSG(("MWAWChartInternal::SubDocument::parse: can not find the chart\n"));
    return;
  }
  m_chart->sendTextZoneContent(m_textZone, listener);
}

}

////////////////////////////////////////////////////////////
// MWAWChart
////////////////////////////////////////////////////////////
MWAWChart::MWAWChart(std::string const &sheetName, MWAWFontConverterPtr fontConverter, MWAWVec2f const &dim) :
  m_sheetName(sheetName), m_dim(dim), m_type(MWAWChart::Series::S_Bar), m_dataStacked(false), m_legend(), m_seriesList(), m_textZoneMap(), m_fontConverter(fontConverter)
{
  for (int i=0; i<3; ++i) m_axis[i]=Axis();
}

MWAWChart::~MWAWChart()
{
}

void MWAWChart::add(int coord, MWAWChart::Axis const &axis)
{
  if (coord<0 || coord>2) {
    MWAW_DEBUG_MSG(("MWAWChart::add[Axis]: called with bad coord\n"));
    return;
  }
  m_axis[coord]=axis;
}

MWAWChart::Axis const &MWAWChart::getAxis(int coord) const
{
  if (coord<0 || coord>2) {
    MWAW_DEBUG_MSG(("MWAWChart::getAxis: called with bad coord\n"));
    static Axis const badAxis;
    return badAxis;
  }
  return m_axis[coord];
}

void MWAWChart::add(MWAWChart::Series const &series)
{
  m_seriesList.push_back(series);
}

bool MWAWChart::getTextZone(MWAWChart::TextZone::Type type, MWAWChart::TextZone &textZone)
{
  if (m_textZoneMap.find(type)==m_textZoneMap.end())
    return false;
  textZone=m_textZoneMap.find(type)->second;
  return true;
}

void MWAWChart::sendTextZoneContent(MWAWChart::TextZone::Type type, MWAWListenerPtr &listener)
{
  if (m_textZoneMap.find(type)==m_textZoneMap.end()) {
    MWAW_DEBUG_MSG(("MWAWChart::sendTextZoneContent: called with unknown zone(%d)\n", int(type)));
    return;
  }
  sendContent(m_textZoneMap.find(type)->second, listener);
}

void MWAWChart::add(MWAWChart::TextZone const &textZone)
{
  m_textZoneMap[textZone.m_type]=textZone;
}

void MWAWChart::sendChart(MWAWSpreadsheetListenerPtr &listener, librevenge::RVNGSpreadsheetInterface *interface)
{
  if (!listener || !interface) {
    MWAW_DEBUG_MSG(("MWAWChart::sendChart: can not find listener or interface\n"));
    return;
  }
  if (m_seriesList.empty()) {
    MWAW_DEBUG_MSG(("MWAWChart::sendChart: can not find the series\n"));
    return;
  }
  shared_ptr<MWAWListener> genericListener=listener;
  int styleId=0;

  librevenge::RVNGPropertyList style;
  style.insert("librevenge:chart-id", styleId);
  style.insert("draw:stroke", "none");
  style.insert("draw:fill", "none");
  interface->defineChartStyle(style);

  librevenge::RVNGPropertyList chart;
  chart.insert("svg:width", m_dim[0], librevenge::RVNG_POINT);
  chart.insert("svg:height", m_dim[1], librevenge::RVNG_POINT);
  if (!m_seriesList.empty())
    chart.insert("chart:class", Series::getSeriesTypeName(m_seriesList[0].m_type).c_str());
  else
    chart.insert("chart:class", Series::getSeriesTypeName(m_type).c_str());
  chart.insert("librevenge:chart-id", styleId++);
  interface->openChart(chart);

  // legend
  if (m_legend.m_show && m_fontConverter) {
    style=librevenge::RVNGPropertyList();
    m_legend.addStyleTo(style, m_fontConverter);
    style.insert("librevenge:chart-id", styleId);
    interface->defineChartStyle(style);
    librevenge::RVNGPropertyList legend;
    m_legend.addContentTo(legend);
    legend.insert("librevenge:chart-id", styleId++);
    legend.insert("librevenge:zone-type", "legend");
    interface->openChartTextObject(legend);
    interface->closeChartTextObject();
  }
  std::map<TextZone::Type, TextZone>::const_iterator textIt;
  for (textIt=m_textZoneMap.begin(); textIt!=m_textZoneMap.end();) {
    TextZone const &zone= textIt++->second;
    if (zone.m_type != TextZone::T_Title && zone.m_type != TextZone::T_SubTitle)
      continue;
    style=librevenge::RVNGPropertyList();
    zone.addStyleTo(style, m_fontConverter);
    style.insert("librevenge:chart-id", styleId);
    interface->defineChartStyle(style);
    librevenge::RVNGPropertyList textZone;
    zone.addContentTo(m_sheetName, textZone);
    textZone.insert("librevenge:chart-id", styleId++);
    textZone.insert("librevenge:zone-type", zone.m_type==TextZone::T_Title ? "title":"subtitle");
    interface->openChartTextObject(textZone);
    if (zone.m_contentType==TextZone::C_Text) {
      shared_ptr<MWAWSubDocument> doc(new MWAWChartInternal::SubDocument(this, zone.m_type));
      listener->handleSubDocument(doc, libmwaw::DOC_CHART_ZONE);
    }
    interface->closeChartTextObject();
  }
  // plot area
  style=librevenge::RVNGPropertyList();
  style.insert("librevenge:chart-id", styleId);
  style.insert("chart:include-hidden-cells","false");
  style.insert("chart:auto-position","true");
  style.insert("chart:auto-size","true");
  style.insert("chart:treat-empty-cells","leave-gap");
  style.insert("chart:right-angled-axes","true");
  style.insert("chart:stacked", m_dataStacked);
  interface->defineChartStyle(style);

  librevenge::RVNGPropertyList plotArea;
  if (m_dim[0]>80) {
    plotArea.insert("svg:x", 20, librevenge::RVNG_POINT);
    plotArea.insert("svg:width", m_dim[0]-40, librevenge::RVNG_POINT);
  }
  if (m_dim[1]>80) {
    plotArea.insert("svg:y", 20, librevenge::RVNG_POINT);
    plotArea.insert("svg:height", m_dim[1]-40, librevenge::RVNG_POINT);
  }
  plotArea.insert("librevenge:chart-id", styleId++);

  librevenge::RVNGPropertyList floor, wall;
  librevenge::RVNGPropertyListVector vect;
  style=librevenge::RVNGPropertyList();
  style.insert("draw:stroke","solid");
  style.insert("svg:stroke-color","#b3b3b3");
  style.insert("draw:fill","none");
  style.insert("librevenge:chart-id", styleId);
  interface->defineChartStyle(style);
  floor.insert("librevenge:type", "floor");
  floor.insert("librevenge:chart-id", styleId++);
  vect.append(floor);

  style.insert("draw:fill","solid");
  style.insert("draw:fill-color","#ffffff");
  style.insert("librevenge:chart-id", styleId);
  interface->defineChartStyle(style);
  wall.insert("librevenge:type", "wall");
  wall.insert("librevenge:chart-id", styleId++);
  vect.append(wall);
  plotArea.insert("librevenge:childs", vect);
  interface->openChartPlotArea(plotArea);
  // axis
  for (int i=0; i<3; ++i) {
    if (m_axis[i].m_type==Axis::A_None) continue;
    style=librevenge::RVNGPropertyList();
    m_axis[i].addStyleTo(style);
    style.insert("librevenge:chart-id", styleId);
    interface->defineChartStyle(style);
    librevenge::RVNGPropertyList axis;
    m_axis[i].addContentTo(m_sheetName, i, axis);
    axis.insert("librevenge:chart-id", styleId++);
    interface->insertChartAxis(axis);
  }
  // label
  for (textIt=m_textZoneMap.begin(); textIt!=m_textZoneMap.end();) {
    TextZone const &zone= textIt++->second;
    if (zone.m_type == TextZone::T_Title || zone.m_type == TextZone::T_SubTitle)
      continue;
    style=librevenge::RVNGPropertyList();
    zone.addStyleTo(style, m_fontConverter);
    style.insert("librevenge:chart-id", styleId);
    interface->defineChartStyle(style);
    librevenge::RVNGPropertyList textZone;
    zone.addContentTo(m_sheetName, textZone);
    textZone.insert("librevenge:chart-id", styleId++);
    textZone.insert("librevenge:zone-type", "label");
    interface->openChartTextObject(textZone);
    if (zone.m_contentType==TextZone::C_Text) {
      shared_ptr<MWAWSubDocument> doc(new MWAWChartInternal::SubDocument(this, zone.m_type));
      listener->handleSubDocument(doc, libmwaw::DOC_CHART_ZONE);
    }
    interface->closeChartTextObject();
  }
  // series
  for (size_t i=0; i < m_seriesList.size(); ++i) {
    style=librevenge::RVNGPropertyList();
    m_seriesList[i].addStyleTo(style);
    style.insert("librevenge:chart-id", styleId);
    interface->defineChartStyle(style);
    librevenge::RVNGPropertyList series;
    m_seriesList[i].addContentTo(m_sheetName, series);
    series.insert("librevenge:chart-id", styleId++);
    interface->openChartSerie(series);
    interface->closeChartSerie();
  }
  interface->closeChartPlotArea();

  interface->closeChart();
}

////////////////////////////////////////////////////////////
// Axis
////////////////////////////////////////////////////////////
MWAWChart::Axis::Axis() : m_type(MWAWChart::Axis::A_None), m_showGrid(true), m_showLabel(true),
  m_labelRange(MWAWVec2f(0,0), MWAWVec2f(-1,-1)), m_style()
{
  m_style.m_lineWidth=0;
}

MWAWChart::Axis::~Axis()
{
}

void MWAWChart::Axis::addContentTo(std::string const &sheetName, int coord, librevenge::RVNGPropertyList &propList) const
{
  std::string axis("");
  axis += char('x'+coord);
  propList.insert("chart:dimension",axis.c_str());
  axis = "primary-"+axis;
  propList.insert("chart:name",axis.c_str());
  if (m_showGrid && (m_type==A_Numeric || m_type==A_Logarithmic)) {
    librevenge::RVNGPropertyList grid;
    grid.insert("librevenge:type", "grid");
    grid.insert("chart:class", "major");
    librevenge::RVNGPropertyListVector childs;
    childs.append(grid);
    propList.insert("librevenge:childs", childs);
  }
  if (m_showLabel && m_labelRange.size()[0]>=0 && m_labelRange.size()[1]>=0) {
    librevenge::RVNGPropertyList range;
    range.insert("librevenge:sheet-name", sheetName.c_str());
    range.insert("librevenge:start-row", m_labelRange.min()[1]);
    range.insert("librevenge:start-column", m_labelRange.min()[0]);
    range.insert("librevenge:end-row", m_labelRange.max()[1]);
    range.insert("librevenge:end-column", m_labelRange.max()[0]);
    librevenge::RVNGPropertyListVector vect;
    vect.append(range);
    propList.insert("chart:label-cell-address", vect);
  }
}

void MWAWChart::Axis::addStyleTo(librevenge::RVNGPropertyList &propList) const
{
  propList.insert("chart:display-label", m_showLabel);
  propList.insert("chart:axis-position", 0, librevenge::RVNG_GENERIC);
  propList.insert("chart:reverse-direction", false);
  propList.insert("chart:logarithmic", m_type==MWAWChart::Axis::A_Logarithmic);
  propList.insert("text:line-break", false);
  m_style.addTo(propList, true);
}

std::ostream &operator<<(std::ostream &o, MWAWChart::Axis const &axis)
{
  switch (axis.m_type) {
  case MWAWChart::Axis::A_None:
    o << "none,";
    break;
  case MWAWChart::Axis::A_Numeric:
    o << "numeric,";
    break;
  case MWAWChart::Axis::A_Logarithmic:
    o << "logarithmic,";
    break;
  case MWAWChart::Axis::A_Sequence:
    o << "sequence,";
    break;
  case MWAWChart::Axis::A_Sequence_Skip_Empty:
    o << "sequence[noEmpty],";
    break;
  default:
    o << "###type,";
    MWAW_DEBUG_MSG(("MWAWChart::Axis: unexpected type\n"));
    break;
  }
  if (axis.m_showGrid) o << "show[grid],";
  if (axis.m_showLabel) o << "show[label],";
  if (axis.m_labelRange.size()[0]>=0 && axis.m_labelRange.size()[1]>=0)
    o << "label[range]=" << axis.m_labelRange << ",";
  o << axis.m_style;
  return o;
}

////////////////////////////////////////////////////////////
// Legend
////////////////////////////////////////////////////////////
void MWAWChart::Legend::addContentTo(librevenge::RVNGPropertyList &propList) const
{
  propList.insert("svg:x", m_position[0], librevenge::RVNG_POINT);
  propList.insert("svg:y", m_position[1], librevenge::RVNG_POINT);
  if (!m_autoPosition || !m_relativePosition)
    return;
  std::stringstream s;
  if (m_relativePosition&libmwaw::TopBit)
    s << "top";
  else if (m_relativePosition&libmwaw::BottomBit)
    s << "bottom";
  if (s.str().length() && (m_relativePosition&(libmwaw::LeftBit|libmwaw::RightBit)))
    s << "-";
  if (m_relativePosition&libmwaw::LeftBit)
    s << "start";
  else if (m_relativePosition&libmwaw::RightBit)
    s << "end";
  propList.insert("chart:legend-position", s.str().c_str());
}

void MWAWChart::Legend::addStyleTo(librevenge::RVNGPropertyList &propList, shared_ptr<MWAWFontConverter> fontConverter) const
{
  propList.insert("chart:auto-position", m_autoPosition);
  m_font.addTo(propList, fontConverter);
  m_style.addTo(propList);
}

std::ostream &operator<<(std::ostream &o, MWAWChart::Legend const &legend)
{
  if (legend.m_show)
    o << "show,";
  if (legend.m_autoPosition) {
    o << "automaticPos[";
    if (legend.m_relativePosition&libmwaw::TopBit)
      o << "t";
    else if (legend.m_relativePosition&libmwaw::RightBit)
      o << "b";
    else
      o << "c";
    if (legend.m_relativePosition&libmwaw::LeftBit)
      o << "L";
    else if (legend.m_relativePosition&libmwaw::BottomBit)
      o << "R";
    else
      o << "C";
    o << "]";
  }
  else
    o << "pos=" << legend.m_position << ",";
  o << legend.m_style;
  return o;
}

////////////////////////////////////////////////////////////
// Serie
////////////////////////////////////////////////////////////
MWAWChart::Series::Series() : m_type(MWAWChart::Series::S_Bar), m_range(), m_style()
{
  m_style.m_lineWidth=0;
  m_style.setSurfaceColor(MWAWColor(0x80,0x80,0xFF));
}

MWAWChart::Series::~Series()
{
}

std::string MWAWChart::Series::getSeriesTypeName(Type type)
{
  switch (type) {
  case S_Area:
    return "chart:area";
  case S_Column:
    return "chart:column";
  case S_Line:
    return "chart:line";
  case S_Pie:
    return "chart:pie";
  case S_Scatter:
    return "chart:scatter";
  case S_Stock:
    return "chart:stock";
  case S_Bar:
    return "chart:bar";
  default:
    break;
  }
  return "chart:bar";
}

void MWAWChart::Series::addContentTo(std::string const &sheetName, librevenge::RVNGPropertyList &serie) const
{
  serie.insert("chart:class",getSeriesTypeName(m_type).c_str());
  librevenge::RVNGPropertyList range, datapoint;
  range.insert("librevenge:sheet-name", sheetName.c_str());
  range.insert("librevenge:start-row", m_range.min()[1]);
  range.insert("librevenge:start-column", m_range.min()[0]);
  range.insert("librevenge:end-row", m_range.max()[1]);
  range.insert("librevenge:end-column", m_range.max()[0]);
  librevenge::RVNGPropertyListVector vect;
  vect.append(range);
  serie.insert("chart:values-cell-range-address", vect);
  vect.clear();
  int numPt=m_range.size()[0]>m_range.size()[1] ?
            m_range.size()[0]+1 : m_range.size()[1]+1;
  datapoint.insert("librevenge:type", "data-point");
  datapoint.insert("chart:repeated", numPt);
  vect.append(datapoint);
  serie.insert("librevenge:childs", vect);
}

void MWAWChart::Series::addStyleTo(librevenge::RVNGPropertyList &propList) const
{
  m_style.addTo(propList);
}

std::ostream &operator<<(std::ostream &o, MWAWChart::Series const &series)
{
  switch (series.m_type) {
  case MWAWChart::Series::S_Area:
    o << "area,";
    break;
  case MWAWChart::Series::S_Bar:
    o << "bar,";
    break;
  case MWAWChart::Series::S_Column:
    o << "column,";
    break;
  case MWAWChart::Series::S_Line:
    o << "line,";
    break;
  case MWAWChart::Series::S_Pie:
    o << "pie,";
    break;
  case MWAWChart::Series::S_Scatter:
    o << "scatter,";
    break;
  case MWAWChart::Series::S_Stock:
    o << "stock,";
    break;
  default:
    o << "###type,";
    MWAW_DEBUG_MSG(("MWAWChart::Series: unexpected type\n"));
    break;
  }
  o << "range=" << series.m_range << ",";
  o << series.m_style;
  return o;
}

////////////////////////////////////////////////////////////
// TextZone
////////////////////////////////////////////////////////////
MWAWChart::TextZone::TextZone() :
  m_type(MWAWChart::TextZone::T_Title), m_contentType(MWAWChart::TextZone::C_Cell),
  m_position(-1,-1), m_cell(), m_textEntry(), m_font(), m_style()
{
  m_style.m_lineWidth=0;
}

MWAWChart::TextZone::~TextZone()
{
}

void MWAWChart::TextZone::addContentTo(std::string const &sheetName, librevenge::RVNGPropertyList &propList) const
{
  if (m_position[0]>=0 && m_position[1]>=0) {
    propList.insert("svg:x", m_position[0], librevenge::RVNG_POINT);
    propList.insert("svg:y", m_position[1], librevenge::RVNG_POINT);
  }
  switch (m_type) {
  case T_Title:
    propList.insert("librevenge:zone-type", "title");
    break;
  case T_SubTitle:
    propList.insert("librevenge:zone-type", "subtitle");
    break;
  case T_AxisX:
  case T_AxisY:
  case T_AxisZ:
    propList.insert("librevenge:zone-type", "label");
    return;
  default:
    MWAW_DEBUG_MSG(("MWAWChart::TextZone:addContentTo: unexpected type\n"));
    break;
  }
  if (m_contentType==C_Cell) {
    librevenge::RVNGPropertyList range;
    librevenge::RVNGPropertyListVector vect;
    range.insert("librevenge:sheet-name", sheetName.c_str());
    range.insert("librevenge:row", m_cell[1]);
    range.insert("librevenge:column", m_cell[0]);
    vect.append(range);
    propList.insert("table:cell-range", vect);
  }
}

void MWAWChart::TextZone::addStyleTo(librevenge::RVNGPropertyList &propList, shared_ptr<MWAWFontConverter> fontConverter) const
{
  m_font.addTo(propList, fontConverter);
  m_style.addTo(propList);
}

std::ostream &operator<<(std::ostream &o, MWAWChart::TextZone const &zone)
{
  switch (zone.m_type) {
  case MWAWChart::TextZone::T_SubTitle:
    o << "sub";
  case MWAWChart::TextZone::T_Title:
    o << "title";
    if (zone.m_contentType==MWAWChart::TextZone::C_Cell)
      o << "[" << zone.m_cell << "]";
    o << ",";
    break;
  case MWAWChart::TextZone::T_AxisX:
  case MWAWChart::TextZone::T_AxisY:
  case MWAWChart::TextZone::T_AxisZ:
    if (zone.m_type==MWAWChart::TextZone::T_AxisX)
      o << "axisX";
    else if (zone.m_type==MWAWChart::TextZone::T_AxisY)
      o << "axisY";
    else
      o << "axisZ";
    if (zone.m_contentType==MWAWChart::TextZone::C_Cell)
      o << "[cells]";
    o << ",";
    break;
  default:
    o << "###type,";
    MWAW_DEBUG_MSG(("MWAWChart::TextZone: unexpected type\n"));
    break;
  }
  if (zone.m_contentType==MWAWChart::TextZone::C_Text)
    o << "text,";
  if (zone.m_position[0]>0 || zone.m_position[1]>0)
    o << "pos=" << zone.m_position << ",";
  o << zone.m_style;
  return o;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
