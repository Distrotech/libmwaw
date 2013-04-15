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

#include <time.h>

#include <iomanip>
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWCell.hxx"

void MWAWCellFormat::setBorders(int wh, MWAWBorder const &border)
{
  int const allBits = libmwaw::LeftBit|libmwaw::RightBit|libmwaw::TopBit|libmwaw::BottomBit|libmwaw::HMiddleBit|libmwaw::VMiddleBit;
  if (wh & (~allBits)) {
    MWAW_DEBUG_MSG(("MWAWCellFormat::setBorders: unknown borders\n"));
    return;
  }
  size_t numData = 4;
  if (wh & (libmwaw::HMiddleBit|libmwaw::VMiddleBit))
    numData=6;
  if (m_bordersList.size() < numData) {
    MWAWBorder emptyBorder;
    emptyBorder.m_style = MWAWBorder::None;
    m_bordersList.resize(numData, emptyBorder);
  }
  if (wh & libmwaw::LeftBit) m_bordersList[libmwaw::Left] = border;
  if (wh & libmwaw::RightBit) m_bordersList[libmwaw::Right] = border;
  if (wh & libmwaw::TopBit) m_bordersList[libmwaw::Top] = border;
  if (wh & libmwaw::BottomBit) m_bordersList[libmwaw::Bottom] = border;
  if (wh & libmwaw::HMiddleBit) m_bordersList[libmwaw::HMiddle] = border;
  if (wh & libmwaw::VMiddleBit) m_bordersList[libmwaw::VMiddle] = border;
}

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
  diff = int(m_vAlign) - int(cell.m_vAlign);
  if (diff) return diff;
  if (m_backgroundColor != cell.m_backgroundColor)
    return m_backgroundColor < cell.m_backgroundColor ? -1 : 1;
  diff = int(m_bordersList.size()) - int(cell.m_bordersList.size());
  if (diff) return diff;
  for (size_t c = 0; c < m_bordersList.size(); c++) {
    diff = m_bordersList[c].compare(cell.m_bordersList[c]);
    if (diff) return diff;
  }
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
  case MWAWCellFormat::F_UNKNOWN:
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
  case MWAWCellFormat::HALIGN_DEFAULT:
  default:
    break; // default
  }
  switch(cell.m_vAlign) {
  case MWAWCellFormat::VALIGN_TOP:
    o << ",top";
    break;
  case MWAWCellFormat::VALIGN_CENTER:
    o << ",centered[y]";
    break;
  case MWAWCellFormat::VALIGN_BOTTOM:
    o << ",bottom";
    break;
  case MWAWCellFormat::VALIGN_DEFAULT:
  default:
    break; // default
  }

  if (!cell.m_backgroundColor.isWhite())
    o << ",backColor=" << cell.m_backgroundColor << ",";
  for (size_t i = 0; i < cell.m_bordersList.size(); i++) {
    if (cell.m_bordersList[i].m_style == MWAWBorder::None)
      continue;
    o << "bord";
    char const *wh[] = { "L", "R", "T", "B", "MiddleH", "MiddleV" };
    if (i < 6) o << wh[i];
    else o << "[#wh=" << i << "]";
    o << "=" << cell.m_bordersList[i] << ",";
  }
  return o;
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
bool MWAWCellContent::getDataCellProperty(MWAWCellFormat::Format format, WPXPropertyList &propList,
    std::string &textVal) const
{
  propList.clear();
  textVal = "";
  std::stringstream f;

  bool isFormula = false;

  if (content() == MWAWCellContent::C_FORMULA && formula().length()) {
    f.str("");
    f << "=" << formula();
    propList.insert("table:formula", f.str().c_str());
    isFormula = true;
  }
  if (!isFormula && !isValueSet()) return false;

  std::string actualText = text();
  switch (format) {
  case MWAWCellFormat::F_TEXT:
  case MWAWCellFormat::F_NUMBER:
    propList.insert("office:value-type", "float");
    // if (actualText.length() == 0) {
    f.str("");
    f << value();
    textVal = f.str();
    propList.insert("office:value", textVal.c_str());
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
    textVal = actualText;
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
    textVal = actualText;
    return true;
  }
  case MWAWCellFormat::F_UNKNOWN:
  default:
    break;
  }
  propList.clear();
  textVal = "";
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
  case MWAWCellContent::C_UNKNOWN:
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
