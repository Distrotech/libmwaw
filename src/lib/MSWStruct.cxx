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

#include "MWAWDebug.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWInputStream.hxx"

#include "MSWStruct.hxx"

namespace MSWStruct
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
    switch(font.m_flags[i].get()) {
    case 0x80:
      o << "=no*";
      break;
    case 0:
      o << "=no";
      break;
    case 0x81:
      o << "*";
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
  if (font.m_size.isSet() && font.m_size.get() != font.m_font->size())
    o << "#size2=" << font.m_size.get() << ",";
  if (font.m_value.isSet()) o << "id?=" << font.m_value.get() << ",";
  if (font.m_extra.length())
    o << font.m_extra << ",";
  return o;
}

void Font::insert(Font const &font)
{
  uint32_t flags = getFlags();
  if (flags) m_font->setFlags(flags);
  m_font.insert(font.m_font);
  m_size.insert(font.m_size);
  m_value.insert(font.m_value);
  m_picturePos.insert(font.m_picturePos);
  m_unknown.insert(font.m_unknown);
  for (int i = 0; i < NumFlags; i++)
    m_flags[i] = font.m_flags[i];
  m_extra+=font.m_extra;
}

uint32_t Font::getFlags() const
{
  uint32_t res=0;
  uint32_t const fl[NumFlags] = {
    MWAW_BOLD_BIT, MWAW_ITALICS_BIT, MWAW_STRIKEOUT_BIT, MWAW_OUTLINE_BIT,
    MWAW_SHADOW_BIT, MWAW_SMALL_CAPS_BIT, MWAW_ALL_CAPS_BIT, MWAW_HIDDEN_BIT,
    MWAW_UNDERLINE_BIT
  };
  if (m_font.isSet()) res = m_font->flags();
  for (int i=0; i < NumFlags; i++) {
    if (!m_flags[i].isSet()) continue;
    int action = m_flags[i].get();
    if (action&0xFF7E) continue;
    if (action & 1) res|=fl[i];
    else res &= ~(fl[i]);
  }
  return res;
}

// ------ section -------------
bool Section::read(MWAWInputStreamPtr &input, long endPos)
{
  long pos = input->tell();
  long dSz = endPos-pos;
  if (dSz < 1) return false;
  libmwaw::DebugStream f;
  int c = (int) input->readULong(1), val;
  switch(c) {
  case 0x75: // column break
    if (dSz < 2) return false;
    val = (int) input->readLong(1);
    switch(val) {
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
  m_error += f.str();
  return true;
}

std::ostream &operator<<(std::ostream &o, Section const &section)
{
  if (section.m_type.get())
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
  if (section.m_error.length()) o << section.m_error << ",";
  return o;
}

// paragraph
std::ostream &operator<<(std::ostream &o, Paragraph::Cell const &cell)
{
  if (cell.hasBorders()) {
    o << "border=";
    if (cell.m_borders[0]) o << "T";
    if (cell.m_borders[1]) o << "L";
    if (cell.m_borders[2]) o << "B";
    if (cell.m_borders[3]) o << "R";
    o << ",";
  }
  if (cell.m_extra.length()) o << cell.m_extra;
  return o;
}

bool Paragraph::read(MWAWInputStreamPtr &input, long endPos)
{
  long pos = input->tell();
  bool sectionSet = m_section.isSet();
  if (m_section->read(input,endPos))
    return true;
  if (!sectionSet) m_section.setSet(false);

  input->seek(pos, WPX_SEEK_SET);
  long dSz = endPos-pos;
  if (dSz < 1) return false;
  libmwaw::DebugStream f, f2;
  int c = (int) input->readULong(1), val;
  switch(c) {
  case 0x5:
    if (dSz < 2) return false;
    val = (int) input->readLong(1);
    switch (val) {
    case 0:
      return true;
    case 1:
      m_justify = libmwaw::JustificationCenter;
      return true;
    case 2:
      m_justify = libmwaw::JustificationRight;
      return true;
    case 3:
      m_justify = libmwaw::JustificationFull;
      return true;
    default:
      MWAW_DEBUG_MSG(("MSWStruct::Paragraph::read: can not read align\n"));
      f << "#align=" << val << ",";
      break;
    }
    break;
  case 0x6: // a small number : always 1 ?
  case 0x7:
  case 0x8:
  case 0x9:
    if (dSz < 2) return false;
    val = (int) input->readLong(1);
    switch(c) {
    case 7:
      f << "keeplineTogether";
      break;
    case 8:
      f << "keepwithnext";
      break;
    case 9:
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
  case 0xf: { // tabs
    int sz = (int) input->readULong(1);
    if (sz<2 || 2+sz > dSz) {
      MWAW_DEBUG_MSG(("MSWStruct::Paragraph::read: can not read tab\n"));
      return false;
    }
    int N0 = (int) input->readULong(1);
    if (2*N0 > sz) {
      MWAW_DEBUG_MSG(("MSWStruct::Paragraph::read: num tab0 seems odd\n"));
      return false;
    }
    if (N0) { // CHECKME: an increasing list, but what is it ? Often N0=0 or N ?
      f << "tabs0?=[";
      for (int i = 0; i < N0; i++) {
        f << float(input->readLong(2))/1440.f << ",";
      }
      f << "],";
    }
    int N = (int) input->readULong(1);
    if (N*3+2*N0+2 != sz ) {
      MWAW_DEBUG_MSG(("MSWStruct::Paragraph::read: num tab seems odd\n"));
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
      switch(val>>5) {
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
      switch((val>>2)&3) {
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
  case 0x14: // alignement : 240 normal, 480 : double, ..
  case 0x15: // spacing before DWIPP
  case 0x16: // spacing after DWIPP
    if (dSz < 3) return false;
    val = (int) input->readLong(2);
    m_spacings[c-0x14] = val/1440.;
    if (c != 0x14) return true;
    m_spacingsInterlineUnit = WPX_INCH;
    if (val > 0)
      m_extra += "interline[atLeast],";
    else if (val < 0)
      *(m_spacings[0]) *= -1;
    else {
      m_spacings[0] = 1.0;
      m_spacingsInterlineUnit = WPX_PERCENT;
    }
    return true;
  case 0x18:
  case 0x19:
    if (dSz < 2) return false;
    val = (int) input->readULong(1);
    if (c==0x18) m_inCell = (val!=0);
    else m_tableDef = (val != 0);
    if (val != 0 && val!=1)
      f << "#f" << std::hex << c << std::dec << "=" << val << ",";
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
        switch(type) {
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
      } else if (c==0x1b) {
        switch(type) {
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
    } else f << val/1440. << ",";
    break;
  }
  case 0x1d:
    if (dSz < 2) return false;
    val = (int) input->readULong(1);
    switch(val&3) {
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
  case 0x1e:
  case 0x1f:
  case 0x20:
  case 0x21:
  case 0x22: {
    if (dSz < 3) return false;
    val = (int) input->readULong(2);
    MWAWBorder border;
    if (val & 0x3E00) f << "bord" << (c-0x1e) << "[textSep]=" << int((val& 0x3E00)>>9) << "pt";
    if (val & 0x4000) f << "bord" << (c-0x1e) << "[textSep]=shad,";
    if (val & 0x8000) f << "bord" << (c-0x1e) << "*";
    switch(val &0x1FF) {
    case 0:
      border.m_style = MWAWBorder::None;
      break; // checkme none
    case 0x40:
      break; // normal
    case 0x49:
      border.m_width = 2;
      break;
    case 0x80:
      border.m_style = MWAWBorder::Double;
      break;
    case 0x180:
      border.m_style = MWAWBorder::Dot;
      break;
    case 0x1c0:
      border.m_style = MWAWBorder::Dash;
      break;
    default:
      f << ":fl=" << std::hex << (val & 0x1FF) << std::dec;
      break;
    }
    if (c < 0x22) {
      size_t const wh[] = { MWAWBorder::Top, MWAWBorder::Left,
                            MWAWBorder::Bottom, MWAWBorder::Right
                          };
      size_t p = wh[c-0x1e];
      if (m_borders.size() <= p)
        m_borders.resize(p+1);
      m_borders[p] = border;
    } else {
      m_borders[MWAWBorder::HMiddle] = border;
      m_borders[MWAWBorder::VMiddle] = border;
    }
    break;
  }
  case 0x24: { // distance to text (related to frame ?)
    if (dSz < 3) return false;
    val = (int) input->readULong(2);
    if (val) f << "distToText=" << val/1440. << ",";
    break;
  }
  case 0x38:
    if (dSz < 4) return false;
    val = (int) input->readLong(1);
    if (val != 2) f << "#shadType=" <<  val << ",";
    f << "shad=" << input->readLong(2)/100. << "%,";
    break;
  case 0x98: { // tabs columns
    int sz = (int) input->readULong(2);
    if (!sz || dSz < 2+sz) return false;
    int N = (int) input->readULong(1);
    if (1+(N+1)*2 > sz) {
      MWAW_DEBUG_MSG(("MSWStruct::Paragraph::read: table definition seems odd\n"));
      f << "#tableDef";
      input->seek(pos+2+sz, WPX_SEEK_SET);
      break;
    }
    for (int i=0; i <= N; i++)
      m_tableColumns->push_back(float(input->readLong(2)/20.0));
    int N1 = (sz-(N+1)*2-2);
    for (int i=0; i < (N1+8)/10; i++) {
      Cell cell;
      int numElt= (N1-10*i)/2;
      if (numElt>5) numElt=5;
      f2.str("");
      val = (int) input->readULong(2);
      switch(val) {
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
      for (int b = 0; b < numElt-1; b++) {
        val = (int) input->readULong(2);
        if (!val) continue;
        cell.m_borders[b] = true;
        if (val != 0x40)
          f2 << "#bord" << b << "=" << std::hex << val << std::dec << ",";
      }
      cell.m_extra = f2.str();
      m_tableCells->push_back(cell);
    }
    input->seek(pos+2+sz, WPX_SEEK_SET);
    return true;
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
  if (ind.m_dim.isSet() && (ind.m_dim.get()[0]>0 || ind.m_dim.get()[1]>0))
    o << "dim=" << ind.m_dim.get() << ",";
  o << reinterpret_cast<MWAWParagraph const &>(ind);
  if (ind.m_section.isSet()) o << ind.m_section.get() << ",";
  if (ind.m_inCell.get()) o << "cell,";
  if (ind.m_tableDef.get()) o << "table[def],";
  if (ind.m_tableColumns->size()) {
    o << "tableCols=[";
    for (size_t i = 0; i < ind.m_tableColumns->size(); i++)
      o << ind.m_tableColumns.get()[i] << ",";
    o << "],";
  }
  if (ind.m_tableCells->size()) {
    o << "tableCells=[";
    for (size_t i = 0; i < ind.m_tableCells->size(); i++)
      o << "[" << ind.m_tableCells.get()[i] << "],";
    o << "],";
  }
  return o;
}

bool Paragraph::getFont(Font &font) const
{
  bool res = true;
  if (m_font2.isSet())
    font = m_font2.get();
  else if (m_font.isSet())
    font = m_font.get();
  else
    res = false;
  if (m_modFont.isSet()) {
    font.insert(*m_modFont);
    res = true;
  }
  return res;
}

void Paragraph::print(std::ostream &o, MWAWFontConverterPtr m_convertissor) const
{
  if (m_font2.isSet())
    o << "font2=[" << m_font2->m_font->getDebugString(m_convertissor) << m_font2.get() << "],";
  if (m_font.isSet())
    o << "font=[" << m_font->m_font->getDebugString(m_convertissor) << m_font.get() << "],";
  if (m_modFont.isSet())
    o << "modifFont=[" << m_modFont->m_font->getDebugString(m_convertissor) << m_modFont.get() << "],";
  o << *this;
}
}
