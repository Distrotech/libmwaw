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

#include "MWAWDebug.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWSection.hxx"

#include "MsWrdStruct.hxx"

namespace MsWrdStruct
{
// ------ font -------------
std::ostream &operator<<(std::ostream &o, Font const &font)
{
  char const *what[Font::NumFlags]= {
    "b", "it", "strikeout", "outline", "shadow", "smallCaps", "allCaps",
    "hidden", "underline"
  };
  for (int i=0; i < Font::NumFlags; i++) {
    if (!font.m_flags[i].isSet()) continue;
    o << what[i];
    switch (font.m_flags[i].get()) {
    case 0x80:
      o << "=noStyle";
      break;
    case 0:
      o << "=no";
      break;
    case 0x81:
      o << "=style";
    case 1:
      break;
    default:
      o << "=" << std::hex << font.m_flags[i].get() << std::dec << ",";
    }
    o << ",";
  }
  if (font.m_picturePos.get())
    o << "pict=" << std::hex << font.m_picturePos.get() << std::dec << ",";
  if (font.m_unknown.get())
    o << "ft=" << std::hex << font.m_unknown.get() << std::dec << ",";
  if (font.m_size.isSet() && (font.m_size.get() < font.m_font->size() ||
                              font.m_size.get() > font.m_font->size()))
    o << "#size2=" << font.m_size.get() << ",";
  if (font.m_value.isSet()) o << "id?=" << font.m_value.get() << ",";
  if (font.m_extra.length())
    o << font.m_extra << ",";
  return o;
}

void Font::insert(Font const &font, Font const *styleFont)
{
  updateFontToFinalState(styleFont);
  if (!m_font.isSet())
    m_font=font.m_font;
  else if (font.m_font.isSet())
    m_font->insert(font.m_font.get());
  m_size.insert(font.m_size);
  m_value.insert(font.m_value);
  m_picturePos.insert(font.m_picturePos);
  m_unknown.insert(font.m_unknown);
  for (int i = 0; i < NumFlags; i++)
    m_flags[i] = font.m_flags[i];
  m_extra+=font.m_extra;
}

void Font::updateFontToFinalState(Font const *styleFont)
{
  uint32_t res=0;
  uint32_t const fl[NumFlags] = {
    MWAWFont::boldBit, MWAWFont::italicBit, 0, MWAWFont::outlineBit,
    MWAWFont::shadowBit, MWAWFont::smallCapsBit, MWAWFont::allCapsBit, MWAWFont::hiddenBit, 0
  };
  if (m_font.isSet()) res = m_font->flags();
  bool flagsMod = false;
  for (int i=0; i < NumFlags; i++) {
    if (!m_flags[i].isSet()) continue;
    int action = m_flags[i].get();
    if (action&0xFF7E) continue;
    bool on = (action&1);
    if ((action&0x80) && styleFont) {
      bool prev=false;
      if (i==2)
        prev = styleFont->m_font->getStrikeOut().isSet();
      else if (i==8)
        prev = styleFont->m_font->getUnderline().isSet();
      else
        prev = styleFont->m_font->flags()&fl[i];
      if (action==0x81)
        on = prev;
      else
        on = !prev;
    }
    switch (i) {
    case 2:
      if (on)
        m_font->setStrikeOutStyle(MWAWFont::Line::Simple);
      else
        m_font->setStrikeOutStyle(MWAWFont::Line::None);
      break;
    case 8:
      if (on)
        m_font->setUnderlineStyle(MWAWFont::Line::Simple);
      else
        m_font->setUnderlineStyle(MWAWFont::Line::None);
      break;
    default:
      if (on) res|=fl[i];
      else res &= ~(fl[i]);
      flagsMod=true;
      break;
    }
  }
  if (flagsMod)
    m_font->setFlags(res);
}

// ------ section -------------
MWAWSection Section::getSection(double pageWidth) const
{
  MWAWSection sec;
  int numCols = *m_col;
  if (numCols <= 1)
    return sec;
  MWAWSection::Column col;
  col.m_width=pageWidth/double(numCols);
  col.m_widthUnit=librevenge::RVNG_INCH;
  col.m_margins[libmwaw::Left] = col.m_margins[libmwaw::Right] = *m_colSep/2.;
  sec.m_columns.resize(size_t(numCols), col);
  sec.m_columns[0].m_margins[libmwaw::Left] = 0;
  sec.m_columns[size_t(numCols-1)].m_margins[libmwaw::Right] = 0;
  sec.m_balanceText = true;
  return sec;
}

bool Section::read(MWAWInputStreamPtr &input, long endPos)
{
  long pos = input->tell();
  long dSz = endPos-pos;
  if (dSz < 1) return false;
  libmwaw::DebugStream f;
  int c = (int) input->readULong(1), val;
  switch (c) {
  case 0x75: // column break
    if (dSz < 2) return false;
    val = (int) input->readLong(1);
    switch (val) {
    case 0:
      m_colBreak = false;
      return true;
    case 1:
      m_colBreak = true;
      return true;
    default:
      f << "#f75=" << val << ",";
      break;
    }
    break;
  case 0x77: // num column
    if (dSz<3) return false;
    m_col = (int) input->readLong(2)+1;
    return true;
  case 0x78:
    if (dSz<3) return false;
    m_colSep = float(input->readULong(2))/1440.f;
    return true;
  // FIXME: UNKNOWN
  case 0x80: // a small number: 0, 8, 10
  case 0x76: // always 1 ?
  case 0x79:
  case 0x7d: // always 1
  case 0x7e: // always 0
    if (dSz<2) return false;
    f << "f" << std::hex << c << std::dec << "=" << input->readLong(1) << ",";
    break;
  case 0x7b: // 2e1 3d08
  case 0x7c: // 4da, 6a5, 15ff
    if (dSz<3) return false;
    f << "f" << std::hex << c << std::dec << "=";
    f << std::hex << input->readULong(1) << std::dec << ":";
    f << std::hex << input->readULong(1) << std::dec << ",";
    break;
  case 0x82: // find one time with 168 (related to 7e ?)
    if (dSz<3) return false;
    f << "f" << std::hex << c << std::dec << "=" << input->readLong(2) << ",";
    break;
  case 0x83:
  case 0x84:
    if (dSz < 3) return false;
    val = (int) input->readLong(2);
    if (c == 0x83) f << "header[top]=" << val/1440. << ",";
    else f << "header[bottom]=" << val/1440. << ",";
    break;
  default:
    return false;
  }
  m_extra += f.str();
  return true;
}

// data seems very different in v3, so a new function....
bool Section::readV3(MWAWInputStreamPtr &input, long endPos)
{
  long pos = input->tell();
  long dSz = endPos-pos;
  if (dSz < 1) return false;
  libmwaw::DebugStream f;
  int wh = (int) input->readULong(1), val;
  switch (wh) {
  case 0x36:
    if (dSz < 2) return false;
    val = (int) input->readLong(1);
    switch (val) {
    case 0:
      f << "division=no,";
      break;
    case 1:
      f << "division=columns,";
      break;
    case 2:
      f << "division=page,";
      break; // default
    case 3:
      f << "division=evenpage,";
      break;
    case 4:
      f << "division=oddpage,";
      break;
    default:
      f << "#division=" << val << ",";
      break;
    }
    break;
  case 0x37:
    if (dSz < 2) return false;
    val = (int) input->readLong(1);
    switch (val) {
    case 0:
      break; // no front page
    case 1:
      f << "frontPage,";
      break;
    default:
      f << "#frontPage=" << val << ",";
      break;
    }
    break;
  case 0x3a:
    if (dSz < 2) return false;
    val = (int) input->readLong(1);
    switch (val) {
    case 1:
      f << "addNumbering,";
      break;
    default:
      f << "#addNumbering=" << val << ",";
      break;
    }
    break;
  case 0x3b:
    if (dSz < 2) return false;
    val = (int) input->readLong(1);
    switch (val) {
    case 1:
      f << "numbering=arabic,";
      break; // normal
    case 2:
      f << "numbering=roman[upper],";
      break;
    case 3:
      f << "numbering=alpha[upper],";
      break;
    case 4:
      f << "numbering=alpha[lower],";
      break;
    case 5:
      f << "numbering=roman[lower],";
      break;
    default:
      f << "#numbering[type]=" << val << ",";
      break;
    }
    break;
  case 0x3e:
    if (dSz < 2) return false;
    val = (int) input->readLong(1);
    switch (val) {
    case 1:
      f << "newNumber=byPage,";
      break;
    default:
      f << "#newNumber=" << val << ",";
      break;
    }
    break;
  case 0x3f:
    if (dSz < 2) return false;
    val = (int) input->readLong(1);
    switch (val) {
    case 0:
      f << "footnote,";
      break;
    case 1:
      f << "endnote,";
      break; // default
    default:
      f << "#endnote=" << val << ",";
      break;
    }
    break;
  case 0x40:
    if (dSz < 2) return false;
    val = (int) input->readLong(1);
    switch (val) {
    case 1:
      f << "numberline=byDivision,";
      break;
    case 2:
      f << "numberline=consecutive,";
      break;
    default:
      f << "#numberline=" << val << ",";
      break;
    }
    break;
  case 0x41: // 8 or a
    if (dSz < 2) return false;
    val = (int) input->readLong(1);
    f << "f41=" << std::hex << val << std::dec << ",";
    break;
  case 0x38:
  case 0x39:
  case 0x3c:
  case 0x3d:
  case 0x42:
  case 0x43: // find 0x168
  case 0x44:
  case 0x45:
    if (dSz < 3) return false;
    val = (int) input->readLong(2);
    switch (wh) {
    case 0x38:
      m_col = (int)val+1;
      break;
    case 0x42:
      f << "numberline[lines]=" << val << ",";
      break;
    case 0x39:
      m_colSep = float(val)/1440.f;
      break;
    case 0x3c:
      f << "numberingPos[T]=" << val/1440.0 << ",";
      break;
    case 0x3d:
      f << "numberingPos[R]=" << val/1440.0 << ",";
      break;
    case 0x44:
      f << "headerSize=" << val/1440.0 << ",";
      break;
    case 0x45:
      f << "footerSize=" << val/1440.0 << ",";
      break;
    default:
      f << "f" << std::hex << wh << std::dec << "=" << val << ",";
      break;
    }
    break;
  default:
    return false;
  }
  m_extra += f.str();
  return true;
}

std::ostream &operator<<(std::ostream &o, Section const &section)
{
  if (section.m_type.get()) // 0|0x40|0x80|0xc0
    o << "type=" << std::hex << section.m_type.get() << std::dec << ",";
  if (section.m_paragraphId.isSet() && section.m_paragraphId.get() > -9999)
    o << "sP=" << section.m_paragraphId.get() << ",";
  if (section.m_col.isSet() && section.m_col.get() != 1)
    o << "cols=" << section.m_col.get() << ",";
  if (section.m_colSep.isSet())
    o << "colSep=" << section.m_colSep.get() << "in,";
  if (section.m_colBreak.get()) o << "colBreak,";
  if (section.m_flag.get())
    o << "fl=" << std::hex << section.m_flag.get() << std::dec << ",";
  if (section.m_extra.length()) o << section.m_extra << ",";
  return o;
}

// table
MWAWVariable<Table::Cell> &Table::getCell(int id)
{
  if (id < 0) {
    static MWAWVariable<Table::Cell> badCell;
    MWAW_DEBUG_MSG(("MsWrdStruct::Table::getCell: can not return a negative cell id\n"));
    return badCell;
  }
  if (m_cells.size() <= size_t(id)) m_cells.resize(size_t(id)+1);
  return m_cells[size_t(id)];
}

bool Table::read(MWAWInputStreamPtr &input, long endPos)
{
  long pos = input->tell();
  long dSz = endPos-pos;
  if (dSz < 1) return false;
  libmwaw::DebugStream f, f2;
  int c = (int) input->readULong(1), val;
  switch (c) {
  case 0x98: { // tabs columns
    int sz = (int) input->readULong(2);
    if (!sz || dSz < 2+sz) return false;
    int N = (int) input->readULong(1);
    if (1+(N+1)*2 > sz) {
      MWAW_DEBUG_MSG(("MsWrdStruct::Table::read: table definition seems odd\n"));
      f << "#colDef";
      input->seek(pos+2+sz, librevenge::RVNG_SEEK_SET);
      break;
    }
    for (int i=0; i <= N; i++)
      m_columns->push_back(float(input->readLong(2))/20.0f);
    int N1 = (sz-(N+1)*2-2);
    for (int i=0; i < (N1+8)/10; i++) {
      Cell cell;
      int numElt= (N1-10*i)/2;
      if (numElt>5) numElt=5;
      f2.str("");
      val = (int) input->readULong(2);
      switch (val) {
      case 0:
        break;
      case 0x4000:
        f2 << "empty,";
        break;
      case 0x8000:
        f2 << "merge,";
        break;
      default:
        f2 << "#type=" << std::hex << val << std::dec << ",";
      }
      if (numElt > 0) {
        cell.m_borders.resize(size_t(numElt)-1);
        for (int b = 0; b < numElt-1; b++) {
          std::string bExtra;
          MWAWBorder border = getBorder((int) input->readULong(2), bExtra);
          cell.m_borders[size_t(b)] = border;
          if (bExtra.length())
            f2 << "#bord" << b << "=" << bExtra << ",";
        }
      }
      cell.m_extra = f2.str();
      m_cells.push_back(cell);
      m_cells.back().setSet(true);
    }
    input->seek(pos+2+sz, librevenge::RVNG_SEEK_SET);
    return true;
  }
  case 0x92: // table alignment
    if (dSz < 3) return false;
    val = (int) input->readULong(2);
    switch (val&3) {
    case 0:
      m_justify = MWAWParagraph::JustificationLeft;
      break;
    case 1:
      m_justify = MWAWParagraph::JustificationCenter;
      break;
    case 2:
      m_justify = MWAWParagraph::JustificationRight;
      break;
    case 3:
      m_justify = MWAWParagraph::JustificationFull;
      break;
    default:
      break;
    }
    if (val&0xFFFC) f << "#align=" << std::hex << (val&0xFFFC) << std::dec << ",";
    break;
  case 0x93: // table alignment indent
    if (dSz < 3) return false;
    m_indent = float(input->readLong(2))/1440.f;
    return true;
  case 0x99: // table height DWIPP
    if (dSz < 3) return false;
    m_height = float(input->readLong(2))/1440.f;
    return true;
  case 0x9a: { // the table shading
    if (dSz < 2) return false;
    int sz = (int) input->readULong(1);
    if (2+sz > dSz || (sz%2)) return false;
    for (int i = 0; i < sz/2; i++) {
      float backColor=1.f-float(input->readULong(2))/10000.f;
      getCell(i)->m_backColor = backColor;
    }
    break;
  }
  case 0x9d: { // table cell shading
    if (dSz < 6) return false;
    val = (int) input->readLong(1); // a small number often 4
    if (val!=4) f << "backColor[unkn?]=" << val << ",";
    int firstCol = (int) input->readLong(1);
    int lastCol = (int) input->readLong(1);
    if (firstCol < 0 || lastCol < 0 || firstCol+1 > lastCol) {
      input->seek(2, librevenge::RVNG_SEEK_CUR);
      MWAW_DEBUG_MSG(("MsWrdStruct::Table::read: pb for background range\n"));
      f << "###backRange=" << firstCol << "<->" << lastCol-1 << ",";
      break;
    }
    float backColor = 1.f-float(input->readULong(2))/10000.f;
    for (int i = firstCol; i < lastCol; i++)
      getCell(i)->m_backColor = backColor;
    break;
  }
  case 0xa0: { // table dimension modifier
    if (dSz < 5) return false;
    int firstCol = (int) input->readULong(1);
    int lastCol = (int) input->readULong(1);
    float dim=float(input->readLong(2))/20.0f;
    f << "col[widthMod" << firstCol << "<->" << lastCol-1 << "]=" << dim<< ",";
    if (m_columnsWidthMod->size()<=size_t(firstCol))
      m_columnsWidthMod->resize(size_t(firstCol)+1,-1);
    (*m_columnsWidthMod)[size_t(firstCol)]=dim;
    break;
  }
  case 0xa3: {
    if (dSz < 6) return false;
    // range 0: means A1
    int firstCol = (int) input->readLong(1);
    int lastCol = (int) input->readLong(1);
    if (firstCol < 0 || lastCol < 0 || firstCol+1 > lastCol) {
      input->seek(3, librevenge::RVNG_SEEK_CUR);
      MWAW_DEBUG_MSG(("MsWrdStruct::Table::read: pb for mod color\n"));
      f << "###backRange=" << firstCol << "<->" << lastCol-1 << ",";
      break;
    }
    val = int(input->readULong(1));
    size_t maxVal = 0;
    if (val & 1) maxVal = 1;
    if (val & 2) maxVal = 2;
    if (val & 4) maxVal = 3;
    if (val & 8) maxVal = 4;
    if (val&0xF0) f << "borderMod[wh=#" << std::hex << int(val&0xF0) << std::dec << "],";
    std::string bExtra;
    MWAWBorder border = getBorder((int) input->readULong(2), bExtra);
    for (int i = firstCol; i < lastCol; i++) {
      MWAWVariable<Cell> &cell = getCell(i);
      if (cell->m_borders.size() < maxVal)
        cell->m_borders.resize(maxVal);
      if (val&1) cell->m_borders[0] = border;
      if (val&2) cell->m_borders[1] = border;
      if (val&4) cell->m_borders[2] = border;
      if (val&8) cell->m_borders[3] = border;
    }
    if (bExtra.length()) f << "borderMod=" << bExtra << ",";
    break;
  }
  default:
    return false;
  }
  m_extra += f.str();
  return true;
}

void Table::insert(Table const &table)
{
  m_height.insert(table.m_height);
  m_justify.insert(table.m_justify);
  m_indent.insert(table.m_indent);
  m_columns.insert(table.m_columns);
  if (table.m_columnsWidthMod.isSet() && !m_columns->empty()
      && !table.m_columnsWidthMod->empty()) {
    size_t numCols=m_columns->size();
    std::vector<float> colWidth(numCols-1,0);
    for (size_t c=0; c+1<numCols; ++c)
      colWidth[c]=(*m_columns)[c+1]-(*m_columns)[c];
    for (size_t c=0; c<table.m_columnsWidthMod->size(); ++c) {
      if (c+1 >= numCols) break;
      if (table.m_columnsWidthMod.get()[c]<0) continue;
      colWidth[c]=table.m_columnsWidthMod.get()[c];
    }
    for (size_t c=0; c+1<numCols; ++c)
      (*m_columns)[c+1]=colWidth[c]+(*m_columns)[c];
  }
  size_t tNumCells = table.m_cells.size();
  if (tNumCells > m_cells.size())
    m_cells.resize(tNumCells, MWAWVariable<Cell>());
  for (size_t i=0; i < tNumCells; i++) {
    if (!m_cells[i].isSet())
      m_cells[i] = table.m_cells[i];
    else if (table.m_cells[i].isSet())
      m_cells[i]->insert(*table.m_cells[i]);
  }
  m_extra+=table.m_extra;
}

std::ostream &operator<<(std::ostream &o, Table::Cell const &cell)
{
  if (cell.hasBorders()) {
    o << "borders=[";
    char const *(wh[]) = { "T", "L", "B", "R" };
    for (size_t i = 0; i < cell.m_borders.size() ; i++) {
      if (!cell.m_borders[i].isSet()) continue;
      if (i < 4) o << wh[i];
      else o << "#" << i;
      o << "=" << cell.m_borders[i].get() << ",";
    }
    o << "],";
  }
  if (cell.m_backColor.isSet()) o << "backColor=" << cell.m_backColor.get() << ",";
  if (cell.m_extra.length()) o << cell.m_extra;
  return o;
}

std::ostream &operator<<(std::ostream &o, Table const &table)
{
  if (table.m_height.isSet()) { // 0 means automatic
    if (table.m_height.get() > 0)
      o << "height[row]=" << table.m_height.get() << "[atLeast],";
    else if (table.m_height.get() < 0)
      o << "height[row]=" << table.m_height.get() << ",";
  }
  if (table.m_justify.isSet()) {
    switch (table.m_justify.get()) {
    case MWAWParagraph::JustificationLeft:
      o << "just=left,";
      break;
    case MWAWParagraph::JustificationCenter:
      o << "just=centered, ";
      break;
    case MWAWParagraph::JustificationRight:
      o << "just=right, ";
      break;
    case MWAWParagraph::JustificationFull:
      o << "just=full, ";
      break;
    case MWAWParagraph::JustificationFullAllLines:
      o << "just=fullAllLines, ";
      break;
    default:
      o << "just=" << table.m_justify.get() << ", ";
      break;
    }
  }
  if (table.m_indent.isSet()) o << "indent=" << table.m_indent.get() << ",";
  if (table.m_columns.isSet() && table.m_columns->size()) {
    o << "cols=[";
    for (size_t i = 0; i < table.m_columns->size(); i++)
      o << table.m_columns.get()[i] << ",";
    o << "],";
  }
  if (table.m_columnsWidthMod.isSet() && table.m_columnsWidthMod->size()) {
    for (size_t i = 0; i < table.m_columnsWidthMod->size(); i++) {
      if (table.m_columnsWidthMod.get()[i] >= 0)
        o << "col" << i << "[width]=" << (*table.m_columnsWidthMod)[i] << ",";
    }
  }
  if (table.m_columns->size()) {
    o << "cols=[";
    for (size_t i = 0; i < table.m_columns->size(); i++)
      o << table.m_columns.get()[i] << ",";
    o << "],";
  }
  if (table.m_cells.size()) {
    o << "cells=[";
    for (size_t i = 0; i < table.m_cells.size(); i++)
      o << "[" << table.m_cells[i].get() << "],";
    o << "],";
  }
  if (table.m_extra.length()) o << table.m_extra;
  return o;
}

// paragraph info
bool ParagraphInfo::read(MWAWInputStreamPtr &input, long endPos, int vers)
{
  long pos = input->tell();
  if ((vers <= 3 && endPos < pos+2) || (vers > 3 && endPos < pos+6)) {
    MWAW_DEBUG_MSG(("ParagraphInfo::read: zone is too short\n"));
    return false;
  }
  m_type = (int) input->readULong(1); // 0, 20(then ff), 40, 60(then ff)
  m_numLines = (int) input->readLong(1);
  if (vers <= 3)
    return true;
  m_dim->setX(float(input->readULong(2))/1440.f);
  m_dim->setY(float(input->readLong(2))/72.f);
  return true;
}
void ParagraphInfo::insert(ParagraphInfo const &pInfo)
{
  m_type.insert(pInfo.m_type);
  m_dim.insert(pInfo.m_dim);
  m_numLines.insert(pInfo.m_numLines);
  m_error += pInfo.m_error;
}

// paragraph
bool Paragraph::read(MWAWInputStreamPtr &input, long endPos)
{
  long pos = input->tell();
  bool sectionSet = m_section.isSet();
  if (m_version > 3 && m_section->read(input,endPos))
    return true;
  if (m_version == 3 && m_section->readV3(input,endPos))
    return true;
  if (!sectionSet) m_section.setSet(false);

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  bool tableSet = m_table.isSet();
  if (m_table->read(input,endPos))
    return true;
  if (!tableSet) m_table.setSet(false);

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  long dSz = endPos-pos;
  if (dSz < 1) return false;
  libmwaw::DebugStream f;
  int c = (int) input->readULong(1), val;
  switch (c) {
  case 0x2: // sprmPIstd
    if (dSz < 2) return false;
    m_styleId = (int) input->readLong(1);
    break;
  case 0x3: { // sprmPIstdPermute: checkme: alway find with 3,3,2,3
    if (dSz < 2) return false;
    int num= (int) input->readULong(1);
    if (num<=0 || dSz < 2+num) return false;
    MWAW_DEBUG_MSG(("Paragraph::read: find istd permute list, unimplemented\n"));
    f << "istPerm=[";
    for (int i = 0; i < num; i++)
      f << input->readLong(1) << ",";
    f << "],";
    break;
  }
  case 0x5:
    if (dSz < 2) return false;
    val = (int) input->readLong(1);
    switch (val) {
    case 0:
      m_justify = MWAWParagraph::JustificationLeft;
      return true;
    case 1:
      m_justify = MWAWParagraph::JustificationCenter;
      return true;
    case 2:
      m_justify = MWAWParagraph::JustificationRight;
      return true;
    case 3:
      m_justify = MWAWParagraph::JustificationFull;
      return true;
    default:
      MWAW_DEBUG_MSG(("MsWrdStruct::Paragraph::read: can not read align\n"));
      f << "#align=" << val << ",";
      break;
    }
    break;
  case 0x6: // a small number : always 1 ?
  case 0x7:
  case 0x8:
  case 0x9:
  case 0xe: // a small number always 0 ?
    if (dSz < 2) return false;
    val = (int) input->readLong(1);
    switch (c) {
    case 7:
      if (val==0)
        m_breakStatus = m_breakStatus.get()&(~MWAWParagraph::NoBreakBit);
      else
        m_breakStatus = m_breakStatus.get()|MWAWParagraph::NoBreakBit;
      f << "noBreak";
      break;
    case 8:
      if (val==0)
        m_breakStatus = m_breakStatus.get()&(~MWAWParagraph::NoBreakWithNextBit);
      else
        m_breakStatus = m_breakStatus.get()|MWAWParagraph::NoBreakWithNextBit;
      f << "noBreakWithNext";
      break;
    case 9: // normally ok, ...
      f << "pagebreakbefore";
      break;
    default:
      f << "f" << std::hex << c << std::dec;
      break;
    }
    if (val==0) f << "*";
    else if (val != 1) f << "=" << val;
    f << ",";
    break;
  case 0xa: {
    if (dSz < 2) return false;
    val = (int) input->readLong(1);
    MWAWBorder border;
    switch (val) {
    case 0:
      break; // normal, checkme
    case 1:
      border.m_width = 2;
      break;
    case 2:
      border.m_type = MWAWBorder::Double;
      break;
    case 3:
      border.m_width = 2;
      f << "border[shadow],";
      break;
    default:
      f << "#borders=" << val << ",";
      break;
    }
    m_bordersStyle = border;
    break;
  }
  case 0xb:
    if (dSz < 2) return false;
    val = (int) input->readULong(1);
    if (val && val <= 0x10) {
      if (m_borders.size() < 4)
        resizeBorders(4);
      if (val & 0x1)
        m_borders[libmwaw::Top] = m_bordersStyle.get();
      if (val & 0x2)
        m_borders[libmwaw::Bottom] = m_bordersStyle.get();
      // val & 0x4
      if ((val & 0x4) || val==0x10)
        m_borders[libmwaw::Left] = m_bordersStyle.get();
      if (val & 0x8)
        m_borders[libmwaw::Right] = m_bordersStyle.get();
      return true;
    }
    else if (val)
      f << "#borders=" << val << ",";
    break;
  case 0xf: // tabs
  case 0x17:  { // tabs
    int sz = (int) input->readULong(1);
    if (sz<2 || 2+sz > dSz) {
      MWAW_DEBUG_MSG(("MsWrdStruct::Paragraph::read: can not read tab\n"));
      return false;
    }
    int deletedSz=(c==0x17) ? 4 : 2;
    int N0 = (int) input->readULong(1);
    if (deletedSz*N0 > sz) {
      MWAW_DEBUG_MSG(("MsWrdStruct::Paragraph::read: num of deleted tabs seems odd\n"));
      return false;
    }
    if (N0) {
      for (int i = 0; i < N0; i++)
        m_deletedTabs.push_back(float(input->readLong(2))/1440.f);
    }
    if (c==0x17 && N0) {
      f << "del17=[";
      for (int i = 0; i < N0; i++) { // always with 0x19 ?
        val=(int) input->readULong(2);
        if (val==0x19)
          f << "_";
        else
          f << std::hex << val << std::dec << ",";
      }
      f << "],";
    }
    int N = (int) input->readULong(1);
    if (N*3+deletedSz*N0+2 != sz) {
      MWAW_DEBUG_MSG(("MsWrdStruct::Paragraph::read: num tab seems odd\n"));
      f << "#";
      m_extra += f.str();
      return false;
    }
    std::vector<float> tabs;
    tabs.resize((size_t) N);
    for (int i = 0; i < N; i++) tabs[(size_t) i] = float(input->readLong(2))/1440.f;
    for (int i = 0; i < N; i++) {
      MWAWTabStop tab;
      tab.m_position = tabs[(size_t) i];
      val = (int) input->readULong(1);
      switch (val>>5) {
      case 0:
        break;
      case 1:
        tab.m_alignment = MWAWTabStop::CENTER;
        break;
      case 2:
        tab.m_alignment = MWAWTabStop::RIGHT;
        break;
      case 3:
        tab.m_alignment = MWAWTabStop::DECIMAL;
        break;
      case 4:
        tab.m_alignment = MWAWTabStop::BAR;
        break;
      default:
        f << "#tabAlign=" << int(val>>5) << ",";
        break;
      }
      switch ((val>>2)&3) {
      case 1:
        tab.m_leaderCharacter = '.';
        break;
      case 2:
        tab.m_leaderCharacter = '-';
        break;
      case 3:
        tab.m_leaderCharacter = '_';
        break;
      case 0:
      default:
        break;
      }
      if (val & 0x13)
        f << "#tabsFlags=" << std::hex << (val & 0x13) << ",";
      m_tabs->push_back(tab);
    }
    break;
  }
  case 0x10: // right
  case 0x11: // left
  case 0x13: // first left
    if (dSz < 3) return false;
    val = (int) input->readLong(2);
    if (c == 0x13) m_margins[0] = val/1440.;
    else if (c == 0x11) m_margins[1] = val/1440.;
    else m_margins[2] = val/1440.;
    return true;
  case 0x14: // alignment : 240 normal, 480 : double, ..
    if (dSz < 3) return false;
    val = (int) input->readLong(2);
    m_interline=double(val)/1440.;
    return true;
  case 0x15: // spacing before DWIPP
  case 0x16: // spacing after DWIPP
    if (dSz < 3) return false;
    val = (int) input->readLong(2);
    m_spacings[c-0x14] = double(val)/1440.;
    return true;
  case 0x18:
  case 0x19:
    if (dSz < 2) return false;
    val = (int) input->readULong(1);
    if (c==0x18) m_inCell = (val!=0);
    else m_tableDef = (val != 0);
    if (val != 0 && val!=1) {
      f << "#f" << std::hex << c << std::dec << "=" << val << ",";
      break;
    }
    return true;
  case 0x1a: // frame x,y,w
  case 0x1b:
  case 0x1c: {
    if (dSz < 3) return false;
    val = (int) input->readLong(2);
    char const *what[] = {"frameX", "frameY", "frameWidth" };
    f << what[c-0x1a] << "=";
    if (val < 0) {
      val = -val;
      int type=(val&0x1C)>>2;
      if (c==0x1a) {
        switch (type) {
        case 1:
          f << "center";
          break;
        case 2:
          f << "right";
          break;
        case 3:
          f << "inside";
          break;
        case 4:
          f << "outside";
          break;
        default:
          f << "#" << type;
          break;
        }
        val &= 0xFFE3;
      }
      else if (c==0x1b) {
        switch (type) {
        case 1:
          f << "top,";
          break;
        case 2:
          f << "center,";
          break;
        case 3:
          f << "bottom,";
          break;
        default:
          f << "#" << type;
          break;
        }
        val &= 0xFFE3;
      }
      if (val)
        f << "[" << val << "]";
      f << ",";
    }
    else f << val/1440. << ",";
    break;
  }
  case 0x1d:
    if (dSz < 2) return false;
    val = (int) input->readULong(1);
    switch (val&3) {
    case 0:
      break; // column
    case 1:
      f << "Xrel=margin,";
      break;
    case 2:
      f << "Xrel=page,";
      break;
    default:
      f << "#Xrel=3,";
      break;
    }
    if (val&4) f << "Yrel=page,"; // def margin
    if (val&0xF8) f << "#rel=" << (val>>3) << ",";
    break;
  case 0x1e: // checkme: find one time in v3 (text struct file) with data 0x80
  case 0x1f:
  case 0x20:
  case 0x21:
  case 0x22: {
    if (dSz < 3) return false;
    std::string bExtra;
    MWAWBorder border = getBorder((int) input->readULong(2), bExtra);
    if (bExtra.length())
      f << "bord" << (c-0x1e) << "[" << bExtra << "],";
    if (c < 0x22) {
      size_t const wh[] = { libmwaw::Top, libmwaw::Left,
                            libmwaw::Bottom, libmwaw::Right
                          };
      size_t p = wh[c-0x1e];
      if (m_borders.size() <= p)
        resizeBorders(p+1);
      m_borders[p] = border;
    }
    else {
      if (m_borders.size() < 6)
        resizeBorders(6);
      m_borders[libmwaw::HMiddle] = border;
      m_borders[libmwaw::VMiddle] = border;
    }
    break;
  }
  case 0x24: { // distance to text (related to frame ?)
    if (dSz < 3) return false;
    val = (int) input->readULong(2);
    if (val) f << "distToText=" << val/1440. << ",";
    break;
  }
  default:
    return false;
  }
  m_extra += f.str();
  return true;
}

// paragraph
std::ostream &operator<<(std::ostream &o, Paragraph const &ind)
{
  if (ind.m_styleId.isSet())
    o << "styleId[orig]=" << ind.m_styleId.get() << ",";
  if (!ind.m_deletedTabs.empty()) {
    o << "deletedTab=[";
    for (size_t i = 0; i < ind.m_deletedTabs.size(); i++)
      o << ind.m_deletedTabs[i] << ",";
    o << "],";
  }
  if (ind.m_interline.isSet())
    o << "interline=" << *ind.m_interline << ",";
  if (ind.m_info.isSet())
    o << "dim=[" << *ind.m_info << "],";
  o << static_cast<MWAWParagraph const &>(ind);
  if (ind.m_bordersStyle.isSet())
    o << "borders[style]=" << ind.m_bordersStyle.get() << ",";
  if (ind.m_section.isSet()) o << ind.m_section.get() << ",";
  if (ind.m_inCell.get()) o << "cell,";
  if (ind.m_tableDef.get()) o << "table[def],";
  if (ind.m_table.isSet()) o << "table=[" << ind.m_table.get() << "],";
  return o;
}

bool Paragraph::getFont(Font &font, Font const *styleFont) const
{
  bool res = true;
  if (m_font.isSet())
    font = m_font.get();
  else
    res = false;
  if (m_modFont.isSet()) {
    font.insert(*m_modFont, styleFont);
    res = true;
  }
  return res;
}

void Paragraph::updateParagraphToFinalState(Paragraph const *style)
{
  if (!m_interline.isSet()) return;
  double interline=*m_interline;
  if (interline<-1 || interline>1) {
    MWAW_DEBUG_MSG(("MsWrdStruct::Paragraph::updateParagraphToFinalState: interline spacing seems odd\n"));
    setInterline(1.0, librevenge::RVNG_PERCENT);
    return;
  }
  if (interline>0) {
    setInterline(interline, librevenge::RVNG_INCH, MWAWParagraph::AtLeast);
    return;
  }
  if (interline<0) {
    setInterline(-interline, librevenge::RVNG_INCH);
    return;
  }
  // checkme: what to do when m_interline=0, use the style interline or the para info?
  if (!style || !style->m_interline.isSet())
    return;
  interline=*style->m_interline;
  if (interline>0 && interline <=1)
    setInterline(interline, librevenge::RVNG_INCH, MWAWParagraph::AtLeast);
  else if (interline<0 && interline >=-1)
    setInterline(-interline, librevenge::RVNG_INCH, MWAWParagraph::AtLeast);
}

void Paragraph::insert(Paragraph const &para, bool insertModif)
{
  if (m_tabs.isSet()) {
    for (int st=0; st<2; ++st) {
      std::vector<float> const &deletedTabs=st==0?para.m_deletedTabs:m_deletedTabs;
      for (size_t i = 0; i < deletedTabs.size(); ++i) {
        float val = deletedTabs[i];
        for (size_t j = 0; j < m_tabs->size(); j++) {
          if (m_tabs.get()[j].m_position < val-1e-3 || m_tabs.get()[j].m_position > val+1e-3)
            continue;
          m_tabs->erase(m_tabs->begin()+int(j));
          break;
        }
      }
    }
  }
  MWAWParagraph::insert(para);
  m_styleId.insert(para.m_styleId);
  m_interline.insert(para.m_interline);
  if (para.m_info.isSet() && para.m_info->isLineSet())
    m_info.insert(para.m_info);
  if (!m_font.isSet())
    m_font=para.m_font;
  else if (para.m_font.isSet())
    m_font->insert(*para.m_font);
  if (insertModif)
    m_modFont->insert(*para.m_modFont);
  if (!m_section.isSet())
    m_section = para.m_section;
  else if (para.m_section.isSet())
    m_section->insert(*para.m_section);
  if (!m_bordersStyle.isSet() || para.m_bordersStyle.isSet())
    m_bordersStyle = para.m_bordersStyle;
  m_inCell.insert(para.m_inCell);
  if (!m_table.isSet())
    m_table = para.m_table;
  else if (para.m_table.isSet())
    m_table->insert(*para.m_table);
  m_tableDef.insert(para.m_tableDef);
}

void Paragraph::print(std::ostream &o, MWAWFontConverterPtr converter) const
{
  if (m_font.isSet())
    o << "font=[" << m_font->m_font->getDebugString(converter) << m_font.get() << "],";
  if (m_modFont.isSet())
    o << "modifFont=[" << m_modFont->m_font->getDebugString(converter) << m_modFont.get() << "],";
  o << *this;
}

// generic
MWAWBorder getBorder(int val, std::string &extra)
{
  MWAWBorder border;
  libmwaw::DebugStream f;

  if (val & 0x3E00) f << "textSep=" << int((val& 0x3E00)>>9) << "pt";
  if (val & 0x4000) f << "shad,";
  if (val & 0x8000) f << "*";
  switch (val &0x1FF) {
  case 0:
    border.m_style = MWAWBorder::None;
    break;
  case 0x40:
    break; // normal
  case 0x49:
    border.m_type = MWAWBorder::Double;
    break;
  case 0x80:
    border.m_width = 2;
    break;
  case 0x180:
    border.m_style = MWAWBorder::Dot;
    break;
  case 0x1c0:
    border.m_width = 0.5;
    break;
  default:
    f << "#bType=" << std::hex << (val & 0x1FF) << std::dec;
    break;
  }

  extra=f.str();
  return border;
}

}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
