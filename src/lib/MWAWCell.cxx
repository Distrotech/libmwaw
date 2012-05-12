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

#include <time.h>

#include <iomanip>
#include <sstream>

#include <libwpd/WPXPropertyList.h>

#include "MWAWCell.hxx"

int MWAWCellFormat::compare(MWAWCellFormat const &cell) const
{
  int diff = int(m_format) - int(cell.m_format);
  if (diff) return diff;
  diff = m_subFormat - int(cell.m_subFormat);
  if (diff) return diff;
  diff = m_digits - int(cell.m_digits);
  if (diff) return diff;
  diff = int(m_protected) - int(cell.m_protected);
  if (diff) return diff;
  diff = int(m_hAlign) - int(cell.m_hAlign);
  if (diff) return diff;
  diff = m_bordersList - cell.m_bordersList;
  if (diff) return diff;
  return 0;
}

std::ostream &operator<<(std::ostream &o, MWAWCellFormat const &cell)
{
  int subForm = cell.m_subFormat;
  switch(cell.m_format) {
  case MWAWCellFormat::F_TEXT:
    o << "text";
    if (subForm) {
      o << "[Fo" << subForm << "]";
      subForm = 0;
    }
    break;
  case MWAWCellFormat::F_NUMBER:
    o << "number";
    switch(subForm) {
    case 1:
      o << "[decimal]";
      subForm = 0;
      break;
    case 2:
      o << "[exp]";
      subForm = 0;
      break;
    case 3:
      o << "[percent]";
      subForm = 0;
      break;
    case 4:
      o << "[money]";
      subForm = 0;
      break;
    case 5:
      o << "[thousand]";
      subForm = 0;
      break;
    case 6:
      o << "[percent,thousand]";
      subForm = 0;
      break;
    case 7:
      o << "[money,thousand]";
      subForm = 0;
      break;
    default:
      break;
    }
    break;
  case MWAWCellFormat::F_DATE:
    o << "date";
    switch(subForm) {
    case 1:
      o << "[mm/dd/yy]";
      subForm = 0;
      break;
    case 2:
      o << "[dd Mon, yyyy]";
      subForm = 0;
      break;
    case 3:
      o << "[dd, Mon]";
      subForm = 0;
      break;
    case 4:
      o << "[Mon, yyyy]";
      subForm = 0;
      break;
    case 5:
      o << "[Da, Mon dd, yyyy]";
      subForm = 0;
      break;
    case 6:
      o << "[Month dd yyyy]";
      subForm = 0;
      break;
    case 7:
      o << "[Day, Month dd, yyyy]";
      subForm = 0;
      break;
    default:
      break;
    }
    break;
  case MWAWCellFormat::F_TIME:
    o << "time";
    switch(subForm) {
    case 1:
      o << "[hh:mm:ss AM]";
      subForm = 0;
      break;
    case 2:
      o << "[hh:mm AM]";
      subForm = 0;
      break;
    case 3:
      o << "[hh:mm:ss]";
      subForm = 0;
      break;
    case 4:
      o << "[hh:mm]";
      subForm = 0;
      break;
    default:
      break;
    }
    break;
  default:
    break; // default
  }
  if (subForm) o << "[format=#" << subForm << "]";
  if (cell.m_digits) o << ",digits=" << cell.m_digits;
  if (cell.m_protected) o << ",protected";

  switch(cell.m_hAlign) {
  case MWAWCellFormat::HALIGN_LEFT:
    o << ",left";
    break;
  case MWAWCellFormat::HALIGN_CENTER:
    o << ",centered";
    break;
  case MWAWCellFormat::HALIGN_RIGHT:
    o << ",right";
    break;
  case MWAWCellFormat::HALIGN_FULL:
    o << ",full";
    break;
  default:
    break; // default
  }
  int border = cell.m_bordersList;
  if (border) {
    o << ",bord=[";
    if (border&DMWAW_TABLE_CELL_LEFT_BORDER_OFF) o << "Lef";
    if (border&DMWAW_TABLE_CELL_RIGHT_BORDER_OFF) o << "Rig";
    if (border&DMWAW_TABLE_CELL_TOP_BORDER_OFF) o << "Top";
    if (border&DMWAW_TABLE_CELL_BOTTOM_BORDER_OFF) o << "Bot";
    o << "]";
  }
  return o;
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
bool MWAWCellContent::getDataCellProperty(MWAWCellFormat::Format format, WPXPropertyList &propList,
    std::string &text) const
{
  propList.clear();
  text = "";
  std::stringstream f;

  bool isFormula = false;

  if (content() == MWAWCellContent::C_FORMULA && formula().length()) {
    f.str("");
    f << "=" << formula();
    propList.insert("table:formula", f.str().c_str());
    isFormula = true;
  }
  if (!isFormula && !isValueSet()) return false;

  std::string actualText = MWAWCellContent::text();
  switch (format) {
  case MWAWCellFormat::F_TEXT:
  case MWAWCellFormat::F_NUMBER:
    propList.insert("office:value-type", "float");
    // if (actualText.length() == 0) {
    f.str("");
    f << value();
    text = f.str();
    propList.insert("office:value", text.c_str());
    return true;
  case MWAWCellFormat::F_DATE: {
    int Y=0, M=0, D=0;
    bool dateSet = false;
    if (isValueSet() && MWAWCellContent::double2Date(value(), Y, M, D)) {
      dateSet = true;
      f.str("");
      f << std::setfill('0');
      // office:date-value="2010-03-02"
      f << Y << "-" << std::setw(2) << M << "-" << std::setw(2) << D;
      propList.insert("office:value-type", "date");
      propList.insert("office:date-value", f.str().c_str());
    } else if (isFormula)
      propList.insert("office:value-type", "date");
    else break;

    if (actualText.length()==0 && dateSet) {
      f.str("");
      f << std::setfill('0');
      f << Y << "/" << std::setw(2) << D << "/"	<< std::setw(2) << M;
      actualText = f.str();
    }
    text = actualText;
    return true;
  }
  case MWAWCellFormat::F_TIME: {
    bool tFind = false;
    int H=0, M=0, S=0;
    if (isValueSet() && MWAWCellContent::double2Time(value(),H,M,S)) {
      tFind = true;
      f.str("");
      f << std::setfill('0');
      // office:time-value="PT10H13M00S"
      f << "PT" << std::setw(2) << H << "H" << std::setw(2) << M << "M"
        << std::setw(2) << S << "S";
      propList.insert("office:value-type", "time");
      propList.insert("office:time-value", f.str().c_str());
    } else if (isFormula)
      propList.insert("office:value-type", "time");
    else break;

    if (actualText.length()==0 && tFind) {
      f.str("");
      f << std::setfill('0');
      f << std::setw(2) << H << ":" << std::setw(2) << M << ":"
        << std::setw(2) << S ;
      actualText = f.str();
    }
    text = actualText;
    return true;
  }
  default:
    break;
  }
  propList.clear();
  text = "";
  return false;
}

bool MWAWCellContent::double2Date(double val, int &Y, int &M, int &D)
{
  // number of day since 1/1/1970
  time_t date= time_t((val-24107-1462+0.4)*24.*3600);
  struct tm *dateTm = gmtime(&date);
  if (!dateTm) return false;

  Y = dateTm->tm_year+1900;
  M=dateTm->tm_mon+1;
  D=dateTm->tm_mday;
  return true;
}

bool MWAWCellContent::double2Time(double val, int &H, int &M, int &S)
{
  if (val < 0.0 || val > 1.0) return false;
  double time = 24.*3600.*val+0.5;
  H=int(time/3600.);
  time -= H*3600.;
  M=int(time/60.);
  time -= M*60.;
  S=int(time);
  return true;
}

std::ostream &operator<<(std::ostream &o, MWAWCellContent const &cell)
{
  switch(cell.content()) {
  case MWAWCellContent::C_NONE:
    break;
  case MWAWCellContent::C_TEXT:
    o << ",text=\"" << cell.text() << "\"";
    break;
  case MWAWCellContent::C_NUMBER: {
    o << ",val=";
    bool textAndVal = false;
    if (cell.isTextSet()) {
      o << "\"" << cell.text() << "\"";
      textAndVal = cell.isValueSet();
    }
    if (textAndVal) o << "[";
    if (cell.isValueSet()) o << cell.value();
    if (textAndVal) o << "]";
  }
  break;
  case MWAWCellContent::C_FORMULA:
    o << ",formula=" << cell.formula();
    if (cell.isValueSet()) o << "[" << cell.value() << "]";
    break;
  default:
    o << ",###unknown type";
  }

  return o;
}


////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
std::ostream &operator<<(std::ostream &o, MWAWCell const &cell)
{
  o << MWAWCell::getCellName(cell.m_position, Vec2b(false,false)) << ":";
  if (cell.numSpannedCells()[0] != 1 || cell.numSpannedCells()[1] != 1)
    o << "span=[" << cell.numSpannedCells()[0] << "," << cell.numSpannedCells()[1] << "],";
  o << static_cast<MWAWCellFormat const &>(cell);

  return o;
}

std::string MWAWCell::getColumnName(int col)
{
  std::stringstream f;
  f << "[.";
  if (col > 26) f << char('A'+col/26);
  f << char('A'+(col%26));
  f << "]";
  return f.str();
}

std::string MWAWCell::getCellName(Vec2i const &pos, Vec2b const &absolute)
{
  std::stringstream f;
  f << "[.";
  if (absolute[1]) f << "$";
  int col = pos[0];
  if (col > 26) f << char('A'+col/26);
  f << char('A'+(col%26));
  if (absolute[0]) f << "$";
  f << pos[1]+1 << ']';
  return f.str();
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
