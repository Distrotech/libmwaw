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
#include <cmath>
#include <sstream>

#include "MWAWTools.hxx"

#include "IMWAWCell.hxx"
#include "IMWAWContentListener.hxx"
#include "TMWAWFont.hxx"

namespace MWAWTools
{
Convertissor::Convertissor() : m_fontManager(new libmwaw_tools_mac::Font), m_palette3(), m_palette4()
{ }
Convertissor::~Convertissor() {}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
int Convertissor::getUnicode(MWAWStruct::Font const &f, unsigned char c) const
{
  return m_fontManager->unicode(f.id(),c);
}

WPXString Convertissor::getUnicode(MWAWStruct::Font const &f, std::string const &str) const
{
  int len = str.length();
  WPXString res("");
  for (int l = 0; l < len; l++) {
    long unicode = m_fontManager->unicode(f.id(), str[l]);
    if (unicode != -1) IMWAWContentListener::appendUnicode(unicode,res);
    else if (str[l] >= 28) res.append(str[l]);
  }
  return res;
}

void Convertissor::setFontCorrespondance(int macId, std::string const &name)
{
  m_fontManager->setCorrespondance(macId, name);
}

std::string Convertissor::getFontName(int macId) const
{
  return m_fontManager->getName(macId);
}


int Convertissor::getFontId(std::string const &name) const
{
  return m_fontManager->getId(name);
}

void Convertissor::getFontOdtInfo(int macId, std::string &name, int &deltaSize) const
{
  m_fontManager->getOdtInfo(macId, name, deltaSize);
}

#ifdef DEBUG
std::string Convertissor::getFontDebugString(MWAWStruct::Font const &ft) const
{
  std::stringstream o;
  int flags = ft.flags();
  o << std::dec;
  if (ft.id() != -1)
    o << "nam='" << m_fontManager->getName(ft.id()) << "',";
  if (ft.size() > 0) o << "sz=" << ft.size() << ",";

  if (flags) o << "fl=";
  if (flags&DMWAW_BOLD_BIT) o << "b:";
  if (flags&DMWAW_ITALICS_BIT) o << "it:";
  if (flags&DMWAW_UNDERLINE_BIT) o << "underL:";
  if (flags&DMWAW_EMBOSS_BIT) o << "emboss:";
  if (flags&DMWAW_SHADOW_BIT) o << "shadow:";
  if (flags&DMWAW_DOUBLE_UNDERLINE_BIT) o << "2underL:";
  if (flags&DMWAW_STRIKEOUT_BIT) o << "strikeout:";
  if (flags&DMWAW_SUPERSCRIPT_BIT) o << "superS:";
  if (flags&DMWAW_SUBSCRIPT_BIT) o << "subS:";
  if (flags) o << ",";

  if (ft.hasColor()) {
    int col[3];
    ft.getColor(col);
    o << "col=(";
    for (int i = 0; i < 3; i++) o << col[i] << ",";
    o << "),";
  }
  return o.str();
}
#endif

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
void Convertissor::initPalettes() const
{
  if (m_palette3.size()) return;

  //
  // v3 color map
  //
  m_palette3.resize(256);
  // the first 6 lines of 32 colors consisted of 6x6x6 blocks(clampsed) with gratuated color
  int ind=0;
  for (int k = 0; k < 6; k++) {
    for (int j = 0; j < 6; j++) {
      for (int i = 0; i < 6; i++, ind++) {
        if (j==5 && i==2) break;
        m_palette3[ind]=Vec3uc(255-51*i, 255-51*k, 255-51*j);
      }
    }
  }

  // the last 2 lines
  for (int r = 0; r < 2; r++) {
    // the black, red, green, blue zone of 5*2
    for (int c = 0; c < 4; c++) {
      for (int i = 0; i < 5; i++, ind++) {
        int val = 17*r+51*i;
        if (c == 0) {
          m_palette3[ind]=Vec3uc(val, val, val);
          continue;
        }
        int col[3]= {0,0,0};
        col[c-1]=val;
        m_palette3[ind]=Vec3uc(col[0],col[1],col[2]);
      }
    }
    // last part of j==5, i=2..5
    for (int k = r; k < 6; k+=2) {
      for (int i = 2; i < 6; i++, ind++) m_palette3[ind]=Vec3uc(255-51*i, 255-51*k, 255-51*5);
    }
  }

  //
  // v4 color map
  //
  m_palette4.resize(256);
  // the first 6 lines of 32 colors consisted of 6x6x6 blocks with gratuated color
  // excepted m_palette4[215]=black -> set latter
  ind=0;
  for (int k = 0; k < 6; k++) {
    for (int j = 0; j < 6; j++) {
      for (int i = 0; i < 6; i++, ind++) {
        m_palette4[ind]=Vec3uc(255-51*k, 255-51*j, 255-51*i);
      }
    }
  }

  ind--; // remove the black color
  for (int c = 0; c < 4; c++) {
    int col[3] = {0,0,0};
    int val=251;
    for (int i = 0; i < 10; i++) {
      val -= 17;
      if (c == 3) m_palette4[ind++]=Vec3uc(val, val, val);
      else {
        col[c] = val;
        m_palette4[ind++]=Vec3uc(col[0],col[1],col[2]);
      }
      if ((i%2)==1) val -=17;
    }
  }

  // last is black
  m_palette4[ind++]=Vec3uc(0,0,0);
}

/*
  read a cell
*/
static bool readCell(TMWAWInputStreamPtr input, bool is2D, std::string &res)
{
  res = std::string("");
  bool absolute[2] = { false, false};
  bool ok = true;
  if (is2D) {
    int type = input->readULong(1);
    if (type & 0x80) {
      absolute[0] = true;
      type &= 0x7F;
    }
    if (type & 0x40) {
      absolute[1] = true;
      type &= 0xBF;
    }
    if (type) {
      MWAW_DEBUG_MSG(("MWAWTools::readFormula:Pb find fl=%d when reading a cell\n", type));
      ok = false;
    }
  }
  int n0 = input->readULong(1);
  if (!is2D)
    res = IMWAWCell::getColumnName(n0);
  else {
    int n1 = input->readULong(1);
    res = IMWAWCell::getCellName(Vec2i(n0-1,n1), Vec2b(absolute[0], absolute[1]));
  }
  return ok;
}
/*
  read data
*/
bool readString(TMWAWInputStreamPtr input, long endPos, std::string &res)
{
  res = "";
  int error = 0;
  int ok = 0;
  while(input->tell() != endPos) {
    char c = input->readLong(1);
    if (c < 27 && c != '\t' && c != '\n') error++;
    else ok++;
    res += c;
  }
  return ok >= error;
}

/* reads a number which can be preceded by a string */
bool readNumber(TMWAWInputStreamPtr input, long endPos, double &res, std::string &str)
{
  res = 0;
  long pos = input->tell();
  if (endPos - pos < 10) return false;

  if (endPos == pos+10) str = "";
  else if (!readString(input, endPos-10,str) || input->tell() != endPos-10) return false;

  int exp = input->readULong(2);
  int sign = 1;
  if (exp & 0x8000) {
    exp &= 0x7fff;
    sign = -1;
  }
  exp -= 0x3fff;

  unsigned long mantisse = input->readULong(4);
  if ((mantisse & 0x80000000) == 0) {
    if (input->readULong(4) != 0) return false;

    if (exp == -0x3fff && mantisse == 0) return true; // ok zero
    if (exp == 0x4000 && (mantisse & 0xFFFFFFL)==0) { // ok Nan
      res=std::numeric_limits<double>::quiet_NaN();
      return true;
    }
    return false;
  }
  res = std::ldexp(double(mantisse)/double(0x80000000), exp);
  if (sign == -1) {
    res *= -1.;
  }
  input->seek(4, WPX_SEEK_CUR);
  return true;
}

/** reads a formula, returns the formula and the result's string*/
bool readFormula(TMWAWInputStreamPtr input, long endPos, int dim, std::string &str, std::string &res, double &value)
{
  long pos = input->tell();
  str="";
  res="";
  value = 0.0;
  if (pos == endPos) return false;

  std::stringstream f;
  while(input->tell() !=endPos) {
    pos = input->tell();
    int code = input->readLong(1);

    switch (code) {
    case 0x0:
      f << "+";
      continue;
    case 0x2:
      f << "-";
      continue;
    case 0x4:
      f << "*";
      continue;
    case 0x6:
      f << "/";
      continue;
    case 0x8: { // a number
      int sz = input->readULong(1);

      std::string s;
      double val;
      if (pos+sz+12 <= endPos && readNumber(input, (pos+2)+sz+10, val, s)) {
        f << s;
        continue;
      }
      break;
    }
    case 0x0a: { // a cell
      std::string name;
      if (!readCell(input, dim != 1, name))
        f << "#";
      f << name;
      continue;
    }
    case 0x0c: { // function
      int v = input->readULong(1);
      static char const *(listFunc) [0x41] = {
        "Abs", "Sum", "Na", "Error", "ACos", "And", "ASin", "ATan",
        "ATan2", "Average", "Choose", "Cos", "Count", "Exp", "False", "FV",
        "HLookup", "If", "Index", "Int", "IRR", "IsBlank", "IsError", "IsNa",
        "##Funct[30]", "Ln", "Lookup", "Log10", "Max", "Min", "Mod", "Not",
        "NPer", "NPV", "Or", "Pi", "Pmt", "PV", "Rand", "Round",
        "Sign", "Sin", "Sqrt", "StDev", "Tan", "True", "Var", "VLookup",
        "Match", "MIRR", "Rate", "Type", "Radians", "Degrees", "Sum" /*"SSum: checkme"*/, "Date",
        "Day", "Hour", "Minute", "Month", "Now", "Second", "Time", "Weekday",
        "Year"
      };
      if ((v%2) == 0 && v >= 0 && v/2 <= 0x40)
        f << listFunc[v/2];
      else
        f << "##funct[" << std::hex << v << std::dec << "]";
      continue;
    }
    case 0x0e: { // list of cell
      std::string name;
      if (!readCell(input, dim != 1, name))
        f << "#";
      f << name << ":";
      if (!readCell(input, dim != 1, name))
        f << "#";
      f << name;
      continue;
    }
    case 0x10:
      f << "(";
      continue;
    case 0x12:
      f << ")";
      continue;
    case 0x14:
      f << ";";
      continue;
    case 0x18:
      f << "<";
      continue;
    case 0x1a:
      f << ">";
      continue;
    case 0x1c:
      f << "=";
      continue;
    case 0x1e:
      f << "<=";
      continue;
    case 0x20:
      f << ">=";
      continue;
    case 0x22:
      f << "<>";
      continue;
    }
    input->seek(pos, WPX_SEEK_SET);
    break;
  }

  str = f.str();

  pos = input->tell();
  if (str.size() == 0) return false;
  if (endPos - pos < 21 ) return true;

  // test if we have the value
  if (input->readLong(1) != 0x16) {
    input->seek(-1, WPX_SEEK_CUR);
    return true;
  }

  f.str("");
  f << std::dec << "unk1=[";
  // looks a little as a zone of cell ?? but this seems eroneous
  for (int i = 0; i < 2; i++) {
    int v = input->readULong(1);

    int n0 = input->readULong(1);
    int n1 = input->readULong(1);
    if (i == 1) f << ":";
    f << n0 << "x" << n1;
    if (v) f << "##v";
  }
  f << std::hex << "],unk2=["; // 0, followed by a small number between 1 and 140
  for (int i = 0; i < 2; i++)
    f << input->readULong(2) << ",";
  f << "]";

  // the value
  if (!readNumber(input, endPos, value, res)) {
    MWAW_DEBUG_MSG(("MWAWTools::readFormula: can not read val number\n"));
    input->seek(pos, WPX_SEEK_SET);
    f << ",###";
    res = f.str();
    value = 0.0;
    return true;
  }
  res = f.str();
  return true;
}

}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
