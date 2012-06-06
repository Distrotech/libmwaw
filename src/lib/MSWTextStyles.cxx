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

#include <map>

#include "MWAWContentListener.hxx"

#include "MSWParser.hxx"
#include "MSWText.hxx"

#include "MSWTextStyles.hxx"

/** Internal: the structures of a MSWTextStyles */
namespace MSWTextStylesInternal
{
////////////////////////////////////////
//! Internal: the section
struct Section {
  //! constructor
  Section() : m_id(-1), m_type(0), m_paragraphId(-9999), m_col(1), m_colSep(0.5), m_colBreak(false),
    m_flag(0), m_default(false), m_error("") {
  }
  //! try to read a data
  bool read(MWAWInputStreamPtr &input, long endPos) {
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
    case 0x80: // a small number
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

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Section const &section) {
    if (section.m_id >= 0) o << "S" << section.m_id << ":";
    else o << "S_:";
    if (section.m_type) o << "type=" << std::hex << section.m_type << std::dec << ",";
    if (section.m_paragraphId > -9999) o << "sP=" << section.m_paragraphId << ",";
    if (section.m_col != 1) o << "cols=" << section.m_col << ",";
    if (section.m_colSep < 0.5 || section.m_colSep > 0.5)
      o << "colSep=" << section.m_colSep << "in,";
    if (section.m_colBreak) o << "colBreak,";
    if (section.m_flag)
      o << "fl=" << std::hex << section.m_flag << std::dec << ",";
    if (section.m_error.length()) o << section.m_error << ",";
    return o;
  }
  //! the identificator
  int m_id;
  //! the type
  int m_type;
  //! the paragraph id
  int m_paragraphId;
  //! the num of columns
  int m_col;
  //! the spacing between column
  float m_colSep;
  //! only a column break
  bool m_colBreak;
  //! some flag ( in the main position)
  int m_flag;
  //! true if is default
  bool m_default;
  /** the errors */
  std::string m_error;
};

////////////////////////////////////////
//! Internal: the state of a MSWTextStylesInternal
struct State {
  //! constructor
  State() : m_version(-1), m_defaultFont(2,12), m_sectionList(), m_cols(1) {
  }
  //! the file version
  int m_version;

  //! the default font ( NewYork 12pt)
  MWAWFont m_defaultFont;

  //! the list of section
  std::vector<Section> m_sectionList;

  /** the actual number of columns */
  int m_cols;
};
}

// ------ font -------------
std::ostream &operator<<(std::ostream &o, MSWTextStyles::Font const &font)
{
  for (int i = 0; i < 3; i++) {
    if (!font.m_flags[i]) continue;
    o << "ft" << i << "=";
    o << std::hex << font.m_flags[i] << std::dec << ",";
  }
  if (font.m_size && font.m_size != font.m_font.size())
    o << "#size2=" << font.m_size << ",";
  if (font.m_value) o << "id?=" << font.m_value << ",";
  if (font.m_extra.length())
    o << font.m_extra << ",";
  return o;
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MSWTextStyles::MSWTextStyles
(MWAWInputStreamPtr ip, MSWText &textParser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new MSWTextStylesInternal::State),
  m_mainParser(textParser.m_mainParser), m_textParser(&textParser), m_asciiFile(textParser.ascii())
{
}

MSWTextStyles::~MSWTextStyles()
{ }

int MSWTextStyles::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_textParser->version();
  return m_state->m_version;
}

MWAWFont const &MSWTextStyles::getDefaultFont() const
{
  return m_state->m_defaultFont;
}

////////////////////////////////////////////////////////////
// try to read a font
////////////////////////////////////////////////////////////
bool MSWTextStyles::readFont(MSWTextStyles::Font &font, bool mainZone, bool initFont)
{
  if (initFont)
    font = MSWTextStyles::Font();

  libmwaw::DebugStream f;

  long pos = m_input->tell();
  int sz = (int) m_input->readULong(1);
  if (sz > 20 || sz == 3) {
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  if (sz == 0) return true;
  font.m_default = false;

  int flag = (int) m_input->readULong(1);
  uint32_t flags = 0;
  if (flag&0x80) flags |= MWAW_BOLD_BIT;
  if (flag&0x40) flags |= MWAW_ITALICS_BIT;
  if (flag&0x20) flags |= MWAW_STRIKEOUT_BIT;
  if (flag&0x10) flags |= MWAW_OUTLINE_BIT;
  if (flag&0x8) flags |= MWAW_SHADOW_BIT;
  if (flag&0x4) flags |= MWAW_SMALL_CAPS_BIT;
  if (flag&0x2) flags |= MWAW_ALL_CAPS_BIT;
  if (flag&0x1) flags |= MWAW_HIDDEN_BIT;

  int what = 0;
  if (sz >= 2) what = (int) m_input->readULong(1);

  /*  01: horizontal decal, 2: vertical decal, 4; underline, 08: fSize,  10: set font, 20: font color, 40: ???(maybe reset)
  */
  font.m_font.setId(m_state->m_defaultFont.id());
  if (sz >= 4) {
    int fId = (int) m_input->readULong(2);
    if (fId) {
      if (mainZone && (what & 0x50)==0) f << "#fId,";
      font.m_font.setId(fId);
    } else if (what & 0x10) {
    }
    what &= 0xEF;
  } else if (what & 0x10) {
  }
  if (initFont)
    font.m_font.setSize(0);
  if (sz >= 5) {
    int fSz = (int) m_input->readULong(1)/2;
    if (fSz) {
      if (mainZone && (what & 0x48)==0) f << "#fSz,";
      font.m_font.setSize(fSz);
    }
    what &= 0xF7;
  } else if (initFont) // reset to default
    font.m_font.setSize(m_state->m_defaultFont.size());

  if (sz >= 6) {
    int decal = (int) m_input->readLong(1); // unit point
    if (decal) {
      if (what & 0x2) {
        if (decal > 0)
          flags |= MWAW_SUPERSCRIPT100_BIT;
        else
          flags |= MWAW_SUBSCRIPT100_BIT;
      } else
        f << "#vDecal=" << decal;
    }
    what &= 0xFD;
  }
  if (sz >= 7) {
    int decal = (int) m_input->readLong(1); // unit point > 0 -> expand < 0: condensed
    if (decal) {
      if ((what & 0x1) == 0) f << "#";
      f << "hDecal=" << decal <<",";
    }
    what &= 0xFE;
  }

  if (sz >= 8) {
    int val = (int) m_input->readULong(1);
    if (val & 0xF0) {
      if (what & 0x20) {
        Vec3uc col;
        if (m_mainParser->getColor((val>>4),col))
          font.m_font.setColor(col);
        else
          f << "#fColor=" << (val>>4) << ",";
      } else
        f << "#fColor=" << (val>>4) << ",";
    }
    what &= 0xDF;

    if (val && (what & 0x4)) {
      flags |= MWAW_UNDERLINE_BIT;
      if (val != 2) f << "#underline=" << (val &0xf) << ",";
      what &= 0xFB;
    } else if (val & 0xf)
      f << "#underline?=" << (val &0xf) << ",";
  }

  font.m_flags[0] =what;
  font.m_font.setFlags(flags);

  bool ok = false;
  if (mainZone && sz >= 10 && sz <= 12) {
    int wh = (int) m_input->readULong(1);
    long pictPos = 0;
    for (int i = 10; i < 13; i++) {
      pictPos <<= 8;
      if (i <= sz) pictPos += m_input->readULong(1);
    }
    long actPos = m_input->tell();
    if (m_mainParser->checkPicturePos(pictPos, wh)) {
      ok = true;
      m_input->seek(actPos, WPX_SEEK_SET);
      f << "pict=" << std::hex << pictPos << std::dec << "[" << wh << "],";
    } else
      m_input->seek(pos+1+8, WPX_SEEK_SET);
  }
  if (!ok && sz >= 9) {
    int wh = (int) m_input->readLong(1);
    switch(wh) {
    case -1:
      ok = true;
      break;
    case 0: // line height ?
      if (sz < 10) break;
      font.m_size=(int) m_input->readULong(1)/2;
      ok = true;
      break;
    default:
      break;
    }
  }
  if (!ok && sz >= 9) {
    m_input->seek(pos+1+8, WPX_SEEK_SET);
    f << "#";
  }
  if (long(m_input->tell()) != pos+1+sz)
    ascii().addDelimiter(m_input->tell(), '|');

  m_input->seek(pos+1+sz, WPX_SEEK_SET);
  font.m_extra = f.str();
  return true;
}

void MSWTextStyles::setProperty(MSWTextStyles::Font const &font)
{
  if (!m_listener) return;
  MWAWFont tmp;
  font.m_font.sendTo(m_listener.get(), m_convertissor, tmp);
}

////////////////////////////////////////////////////////////
// read/send the section zone
////////////////////////////////////////////////////////////
bool MSWTextStyles::readSection(MSWEntry &entry)
{
  if (entry.length() < 14 || (entry.length()%10) != 4) {
    MWAW_DEBUG_MSG(("MSWTextStyles::readSection: the zone size seems odd\n"));
    return false;
  }
  long pos = entry.begin();
  entry.setParsed(true);
  m_input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Section:";
  size_t N=size_t(entry.length()/10);
  std::vector<long> positions; // checkme
  positions.resize(N+1);
  for (size_t i = 0; i <= N; i++) positions[i] = (long) m_input->readULong(4);

  MSWText::PLC plc(MSWText::PLC::Section);
  std::multimap<long, MSWText::PLC> &plcMap = m_textParser->getTextPLCMap();
  long textLength = m_textParser->getMainTextLength();
  for (size_t i = 0; i < N; i++) {
    MSWTextStylesInternal::Section sec;
    sec.m_type = (int) m_input->readULong(1);
    sec.m_flag = (int) m_input->readULong(1);
    sec.m_id = int(i);
    unsigned long filePos = m_input->readULong(4);
    if (textLength && positions[i] > textLength) {
      MWAW_DEBUG_MSG(("MSWTextStyles::readSection: text positions is bad...\n"));
      f << "#";
    } else {
      plc.m_id = int(i);
      plcMap.insert(std::multimap<long,MSWText::PLC>::value_type(positions[i],plc));
    }
    f << std::hex << "pos?=" << positions[i] << ":[" << sec << ",";
    if (filePos != 0xFFFFFFFFL) {
      f << "pos=" << std::hex << filePos << std::dec << ",";
      long actPos = m_input->tell();
      readSection(sec,(long) filePos);
      m_input->seek(actPos, WPX_SEEK_SET);
    }
    f << "],";

    m_state->m_sectionList.push_back(sec);
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

bool MSWTextStyles::readSection(MSWTextStylesInternal::Section &sec, long debPos)
{
  if (!m_mainParser->isFilePos(debPos)) {
    MWAW_DEBUG_MSG(("MSWTextStyles::readSection: can not find section data...\n"));
    return false;
  }
  m_input->seek(debPos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  int sz = (int) m_input->readULong(1);
  long endPos = debPos+sz+1;
  if (sz < 1 || sz >= 255) {
    MWAW_DEBUG_MSG(("MSWTextStyles::readSection: data section size seems bad...\n"));
    f << "Section-" << sec << ":#";
    ascii().addPos(debPos);
    ascii().addNote(f.str().c_str());
    return false;
  }

  while (m_input->tell() < endPos) {
    long pos = m_input->tell();
    if (sec.read(m_input, endPos)) continue;
    f << "#";
    ascii().addDelimiter(pos,'|');
    break;
  }
  f << "Section-" << sec;
  ascii().addPos(debPos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(endPos);
  ascii().addNote("_");
  return true;
}

void MSWTextStyles::setProperty(MSWTextStylesInternal::Section const &sec,
                                MSWTextStyles::Font &/*actFont*/, bool recursive)
{
  if (!m_listener) return;
  int numCols = sec.m_col;
  if (numCols >= 1 && m_state->m_cols > 1 && sec.m_colBreak) {
    if (!m_listener->isSectionOpened()) {
      MWAW_DEBUG_MSG(("MSWTextStyles::setProperty: section is not opened\n"));
    } else
      m_listener->insertBreak(MWAW_COLUMN_BREAK);
  } else {
    if (m_listener->isSectionOpened())
      m_listener->closeSection();
    if (numCols<=1) m_listener->openSection();
    else {
      // column seems to have equal size
      int colWidth = int((72.0*m_mainParser->pageWidth())/numCols);
      std::vector<int> colSize;
      colSize.resize((size_t) numCols);
      for (int i = 0; i < numCols; i++) colSize[(size_t)i] = colWidth;
      m_listener->openSection(colSize, WPX_POINT);
    }
    m_state->m_cols = numCols;
  }
  if (sec.m_paragraphId > -9999 && !recursive) {
    // OSNOLA: FIXME
#if 0
    if (m_state->m_styleParagraphMap.find(sec.m_paragraphId) ==
        m_state->m_styleParagraphMap.end()) {
      MWAW_DEBUG_MSG(("MSWTextStyles::setProperty: can not find paragraph in section\n"));
    } else
      setProperty(m_state->m_styleParagraphMap.find(sec.m_paragraphId)->second,
                  actFont, true);
#endif
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
