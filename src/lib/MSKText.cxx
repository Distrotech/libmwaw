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

#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

#include <libwpd/WPXString.h>

#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"

#include "MSKParser.hxx"

#include "MSKText.hxx"

/** Internal: the structures of a MSKText */
namespace MSKTextInternal
{
////////////////////////////////////////
//! Internal: header zone
struct LineZone {
  //! the constructor
  LineZone() : m_type(-1), m_pos(), m_id(0), m_flags(), m_height(0) {}
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, LineZone const &z) {
    switch(z.m_type>>5) {
    case 0:
    case 1:
    case 2:
    case 3:
      o << "Text,";
      break;
    case 4:
      o << "Tabs,";
      break;
    default:
      o << "##type=" << std::hex << (z.m_type >> 5) << std::dec << ",";
    }
    if (z.m_type&0x8) o << "note,";
    if (z.m_type&0x7) o << "#type=" << std::hex << (z.m_type&0x7) << std::dec << ",";
    if (z.m_id) o  << "id=" << z.m_id << ",";
    if (z.m_flags&1) o << "softBreak,";
    if (z.m_flags&2) o << "hardBreak,";
    if (!(z.m_flags&4)) o << "beginDoc,";
    if (z.m_flags&8) o << "graphics,";
    if (z.m_flags&0x20) o << "noteFl,";
    if (z.m_flags&0x40) o << "fields,";
    if (z.m_flags&0x90) o << "#flags=" << std::hex << (z.m_flags&0x90) << std::dec << ",";
    if (z.m_height) o << "h=" << z.m_height << ",";
    return o;
  }
  //! return true if this is a note
  bool isNote() const {
    return m_type&0x8;
  }
  //! return true if this is a tabs
  bool isRuler() const {
    return (m_type&0xE0)==0x80;
  }
  //! the type
  int m_type;
  //! the file position
  MWAWEntry m_pos;
  //! the id
  int m_id;
  //! the zone flags
  int m_flags;
  //! the height
  int m_height;
};

////////////////////////////////////////
//! Internal: the fonts
struct Font {
  //! the constructor
  Font(): m_font(), m_extra("") {
    for (int i = 0; i < 3; i++) m_flags[i] = 0;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Font const &font) {
    for (int i = 0; i < 3; i++) {
      if (!font.m_flags[i]) continue;
      o << "ft" << i << "=";
      if (i == 0) o << std::hex;
      o << font.m_flags[i] << std::dec << ",";
    }
    if (font.m_extra.length())
      o << font.m_extra << ",";
    return o;
  }

  //! the font
  MWAWFont m_font;
  //! some unknown flag
  int m_flags[3];
  //! extra data
  std::string m_extra;
};

/** Internal: class to store the paragraph properties */
struct Paragraph : public MWAWParagraph {
  //! Constructor
  Paragraph() : MWAWParagraph() {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Paragraph const &ind) {
    o << reinterpret_cast<MWAWParagraph const &>(ind);
    return o;
  }
};

////////////////////////////////////////
//! Internal: the text zone
struct TextZone {
  enum Type { Header, Footer, Main, Unknown };
  //! constructor
  TextZone() : m_type(Unknown), m_id(0), m_zonesList(), m_linesHeight(), m_pagesHeight(), m_pagesPosition(), m_footnoteMap(), m_text(""), m_isSent(false) { }

  //! return true if this is the main zone
  bool isMain() const {
    return m_type == Main;
  }
  //! the zone type;
  int m_type;
  //! the zone id
  int m_id;
  //! the list of zones
  std::vector<LineZone> m_zonesList;
  //! the line heigth
  std::vector<int> m_linesHeight;
  //! the pages heigth
  std::vector<int> m_pagesHeight;
  //! the zone id -> hard break
  std::map<int, bool> m_pagesPosition;
  //! the note id -> zone limit
  std::map<int, Vec2i> m_footnoteMap;
  //! a string used to store v1-2 files header/footer
  std::string m_text;
  //! flag to know if the zone is send or not
  bool m_isSent;
};

////////////////////////////////////////
//! Internal: the state of a MSKText
struct State {
  //! constructor
  State() : m_version(-1), m_numColumns(1), m_zones(),
    m_numPages(1), m_actualPage(1), m_font(20, 12, 0) {
  }
  //! the file version
  mutable int m_version;
  //! the number of column
  int m_numColumns;

  //! the main zone
  std::vector<TextZone> m_zones;

  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
  //! the actual font
  MWAWFont m_font;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MSKText::MSKText
(MWAWInputStreamPtr ip, MSKParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new MSKTextInternal::State),
  m_mainParser(&parser), m_asciiFile(parser.ascii())
{
}

MSKText::~MSKText()
{ }

int MSKText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_mainParser->version();
  return m_state->m_version;
}

int MSKText::numPages(int zoneId) const
{
  if (zoneId < 0 || zoneId >= int(m_state->m_zones.size())) {
    MWAW_DEBUG_MSG(("MSKText::numPages: unknown zone %d\n", zoneId));
    return 0;
  }
  MSKTextInternal::TextZone const &zone = m_state->m_zones[(size_t) zoneId];
  int nPages = 1 + int(zone.m_pagesPosition.size());
  if (zone.isMain()) {
    m_state->m_actualPage = 1;
    m_state->m_numPages = nPages;
  } else {
    MWAW_DEBUG_MSG(("MSKText::numPages: called on no main zone: %d\n", zoneId));
  }
  return nPages;
}

bool MSKText::getLinesPagesHeight
(int zoneId, std::vector<int> &lines, std::vector<int> &pages)
{
  lines.resize(0);
  pages.resize(0);
  if (zoneId < 0 || zoneId >= int(m_state->m_zones.size())) {
    MWAW_DEBUG_MSG(("MSKText::getLinesPagesHeight: unknown zone %d\n", zoneId));
    return false;
  }
  lines = m_state->m_zones[(size_t)zoneId].m_linesHeight;
  pages = m_state->m_zones[(size_t)zoneId].m_pagesHeight;
  return true;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////
int MSKText::getMainZone() const
{
  for (size_t i = 0; i < m_state->m_zones.size(); i++)
    if (m_state->m_zones[i].m_type != MSKTextInternal::TextZone::Header &&
        m_state->m_zones[i].m_type != MSKTextInternal::TextZone::Footer)
      return int(i);
  return -1;
}

int MSKText::getHeader() const
{
  for (size_t i = 0; i < m_state->m_zones.size(); i++)
    if (m_state->m_zones[i].m_type == MSKTextInternal::TextZone::Header)
      return int(i);
  return -1;
}

int MSKText::getFooter() const
{
  for (size_t i = 0; i < m_state->m_zones.size(); i++)
    if (m_state->m_zones[i].m_type == MSKTextInternal::TextZone::Footer)
      return int(i);
  return -1;
}

////////////////////////////////////////////////////////////
// try to find the different zone
////////////////////////////////////////////////////////////
int MSKText::createZones(int numLines, bool mainZone)
{
  MSKTextInternal::LineZone zone;
  int zoneId = int(m_state->m_zones.size());
  m_state->m_zones.push_back(MSKTextInternal::TextZone());
  MSKTextInternal::TextZone &actualZone = m_state->m_zones.back();
  actualZone.m_id = zoneId;
  if (mainZone)
    actualZone.m_type = MSKTextInternal::TextZone::Main;
  bool hasNote=false;
  int firstNote=0;
  while(!m_input->atEOS()) {
    if (numLines==0) break;
    if (numLines>0) numLines--;
    long pos = m_input->tell();
    if (!readZoneHeader(zone)) {
      m_input->seek(pos, WPX_SEEK_SET);
      break;
    }
    if (!hasNote && zone.isNote()) {
      firstNote = int(actualZone.m_zonesList.size());
      hasNote = true;
    }
    actualZone.m_zonesList.push_back(zone);
    m_input->seek(zone.m_pos.end(), WPX_SEEK_SET);
  }
  int numLineZones = int(actualZone.m_zonesList.size());
  if (numLineZones == 0) {
    m_state->m_zones.pop_back();
    return -1;
  }

  update(actualZone);
  if (hasNote)
    updateNotes(actualZone, firstNote);
  return zoneId;
}

void MSKText::update(MSKTextInternal::TextZone &zone)
{
  size_t numLineZones = zone.m_zonesList.size();
  if (numLineZones == 0) return;

  int textHeight = int(72.*m_mainParser->pageHeight());

  int actH = 0, actualPH = 0;
  zone.m_linesHeight.push_back(0);
  for (size_t i = 0; i < numLineZones; i++) {
    MSKTextInternal::LineZone &z = zone.m_zonesList[i];
    if (z.isNote()) continue; // a note
    actH += z.m_height;
    zone.m_linesHeight.push_back(actH);
    bool newPage = ((z.m_flags&1) && actualPH) || (z.m_flags&2);
    actualPH += z.m_height;
    if (newPage || (actualPH > textHeight && textHeight > 0)) {
      zone.m_pagesPosition[int(i)]=(z.m_flags&2);
      zone.m_pagesHeight.push_back(actualPH-z.m_height);
      actualPH=z.m_height;
    }
  }
}

void MSKText::updateNotes(MSKTextInternal::TextZone &zone, int firstNote)
{
  int numLineZones = int(zone.m_zonesList.size());
  if (firstNote < 0 || firstNote >= numLineZones) {
    MWAW_DEBUG_MSG(("MSKText::updateNotes: can not find first note position\n"));
    return;
  }

  MSKTextInternal::Font font;
  int noteId = -1;
  long lastIndentPos = -1;
  Vec2i notePos;

  for (int n = firstNote; n < numLineZones; n++) {
    MSKTextInternal::LineZone const &z = zone.m_zonesList[(size_t) n];
    if (!z.isNote()) {
      noteId=-1;
      MWAW_DEBUG_MSG(("MSKText::updateNotes: find extra data in notes, stop recording note\n"));
      break;
    }
    if (z.isRuler()) {
      lastIndentPos = n;
      continue;
    }
    if (z.m_pos.length() < 8) continue;
    long actPos = z.m_pos.begin();
    m_input->seek(actPos+6,WPX_SEEK_SET);

    int c = (int) m_input->readULong(1);
    if ((c == 1 || c == 2) && readFont(font, z.m_pos.end())) {
      if (long(m_input->tell())+2 > z.m_pos.end())
        continue;
      c = (int) m_input->readULong(1);
      if (c <= 4) {
        if (long(m_input->tell())+2 > z.m_pos.end())
          continue;
        c = (int) m_input->readULong(1);
      }
    }
    if (c != 0x14) continue;
    if (noteId >= 0) {
      notePos[1] = (lastIndentPos != -1) ? int(lastIndentPos) : n;
      if (zone.m_footnoteMap.find(noteId)==zone.m_footnoteMap.end())
        zone.m_footnoteMap[noteId] = notePos;
      else {
        MWAW_DEBUG_MSG(("MSKText::updateNotes: note %d is already defined, ignored\n", noteId));
      }
    }
    noteId =  (int) m_input->readULong(2);
    notePos[0] = (lastIndentPos != -1) ? int(lastIndentPos) : n;
    lastIndentPos = -1;
  }
  if (noteId >= 0) {
    notePos[1] = numLineZones;
    if (zone.m_footnoteMap.find(noteId)==zone.m_footnoteMap.end())
      zone.m_footnoteMap[noteId] = notePos;
    else {
      MWAW_DEBUG_MSG(("MSKText::updateNotes: note %d is already defined, ignored\n", noteId));
    }
  }
}

bool MSKText::readZoneHeader(MSKTextInternal::LineZone &zone) const
{
  zone = MSKTextInternal::LineZone();
  long pos = m_input->tell();
  if (!m_mainParser->checkIfPositionValid(pos+6)) return false;
  zone.m_pos.setBegin(pos);
  zone.m_type = (int) m_input->readULong(1);
  if (zone.m_type & 0x17) return false;
  zone.m_id = (int) m_input->readULong(1);
  zone.m_flags = (int) m_input->readULong(1);
  zone.m_height = (int) m_input->readULong(1);
  zone.m_pos.setLength(6+(long)m_input->readULong(2));
  if (!m_mainParser->checkIfPositionValid(zone.m_pos.end())) return false;
  return true;
}

////////////////////////////////////////////////////////////
// the text:
////////////////////////////////////////////////////////////
bool MSKText::sendText(MSKTextInternal::LineZone &zone, int zoneId)
{
  m_input->seek(zone.m_pos.begin()+6, WPX_SEEK_SET);
  int vers = version();
  libmwaw::DebugStream f;
  f << "Entries(TextZone):" << zone << ",";
  MSKTextInternal::Font actFont, font;
  actFont.m_font = m_state->m_font;
  if (m_listener && zone.m_height > 0)
    m_listener->setParagraphLineSpacing(zone.m_height, WPX_POINT);
  bool firstChar = true;
  while(!m_input->atEOS()) {
    long pos = m_input->tell();
    if (pos >= zone.m_pos.end()) break;
    int c = (int) m_input->readULong(1);
    if ((c == 1 || c == 2) && readFont(font, zone.m_pos.end())) {
      actFont = font;
      setProperty(font);
      f << "[" << font.m_font.getDebugString(m_convertissor) << font << "]";
      continue;
    }
    if (c == 0) {
      f << "#";
      continue;
    }
    f << char(c);
    if (!m_listener)
      continue;
    switch(c) {
    case 0x9:
      m_listener->insertTab();
      break;
    case 0x10: // cursor pos
    case 0x11:
      break;
    default:
      if (c >= 0x14 && c <= 0x19 && vers >= 3) {
        int sz = (c==0x19) ? 0 : (c == 0x18) ? 1 : 2;
        int id = (sz && pos+1+sz <=  zone.m_pos.end()) ? int(m_input->readLong(sz)) : 0;
        if (id) f << "[" << id << "]";
        switch (c) {
        case 0x19:
          m_listener->insertField(MWAWContentListener::Title);
          break;
        case 0x18:
          m_listener->insertField(MWAWContentListener::PageNumber);
          break;
        case 0x16:
          m_listener->insertField(MWAWContentListener::Time);
          break;
        case 0x17: // id = 0 : short date ; id=9 : long date
          m_listener->insertField(MWAWContentListener::Date);
          break;
        case 0x15:
          MWAW_DEBUG_MSG(("MSKText::sendText: find unknown field type 0x15\n"));
          break;
        case 0x14:
          if (!zone.isNote() || !firstChar)
            m_mainParser->sendFootNote(zoneId, id);
          break;
        default:
          break;
        }
      } else if (c <= 0x1f) {
        f << "#" << std::hex << c << std::dec << "]";
        ascii().addDelimiter(pos,'#');
        MWAW_DEBUG_MSG(("MSKText::sendText: find char=%x\n",int(c)));
      } else {
        int unicode = m_convertissor->unicode (actFont.m_font.id(), (unsigned char)c);
        if (unicode == -1)
          m_listener->insertCharacter((uint8_t)c);
        else
          m_listener->insertUnicode((uint32_t) unicode);
      }
      firstChar = false;
      break;
    }
  }
  if (m_listener)
    m_listener->insertEOL();
  ascii().addPos(zone.m_pos.begin());
  ascii().addNote(f.str().c_str());
  return true;
}

bool MSKText::sendString(std::string &str)
{
  if (!m_listener)
    return true;
  MSKTextInternal::Font defFont;
  defFont.m_font = MWAWFont(20,12);
  setProperty(defFont);

  for (int i = 0; i < int(str.length()); i++) {
    char c = str[(size_t)i];
    switch(c) {
    case 0x9:
      m_listener->insertTab();
      break;
    case 0x10: // cursor pos
    case 0x11:
      break;
    case 0x19:
      m_listener->insertField(MWAWContentListener::Title);
      break;
    case 0x18:
      m_listener->insertField(MWAWContentListener::PageNumber);
      break;
    case 0x16:
      m_listener->insertField(MWAWContentListener::Time);
      break;
    case 0x17: // id = 0 : short date ; id=9 : long date
      m_listener->insertField(MWAWContentListener::Date);
      break;
    case 0x15:
      MWAW_DEBUG_MSG(("MSKText::sendString: find unknown field type 0x15\n"));
      break;
    case 0x14: // fixme
      MWAW_DEBUG_MSG(("MSKText::sendString: footnote are not implemented\n"));
      break;
    default:
      if (c <= 0x1f) {
        MWAW_DEBUG_MSG(("MSKText::sendString: find char=%x\n",int(c)));
      } else {
        int unicode = m_convertissor->unicode(defFont.m_font.id(), (unsigned char)c);
        if (unicode == -1)
          m_listener->insertCharacter((uint8_t) c);
        else
          m_listener->insertUnicode((uint32_t)unicode);
      }
      break;
    }
  }
  return true;
}

////////////////////////////////////////////////////////////
// the font:
////////////////////////////////////////////////////////////
bool MSKText::readFont(MSKTextInternal::Font &font, long endPos)
{
  font = MSKTextInternal::Font();
  long pos  = m_input->tell();
  m_input->seek(-1, WPX_SEEK_CUR);
  int type = (int) m_input->readLong(1);
  if ((type != 1 && type != 2) || pos+type+3 > endPos) {
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  libmwaw::DebugStream f;
  int flag = (int) m_input->readULong(1); // check or font ?
  if (flag) f << "#f0=" << flag << ",";
  font.m_font.setId((int) m_input->readULong(1));
  font.m_font.setSize((int) m_input->readULong(1));
  flag = (int) m_input->readULong(1);
  uint32_t flags = 0;
  if (flag & 0x1) flags |= MWAW_BOLD_BIT;
  if (flag & 0x2) flags |= MWAW_ITALICS_BIT;
  if (flag & 0x4) flags |= MWAW_UNDERLINE_BIT;
  if (flag & 0x8) flags |= MWAW_EMBOSS_BIT;
  if (flag & 0x10) flags |= MWAW_SHADOW_BIT;
  if (flag & 0x20) flags |= MWAW_SUPERSCRIPT100_BIT;
  if (flag & 0x40) flags |= MWAW_SUBSCRIPT100_BIT;
  if ((flag & 0x80) && !(flag & 0x60)) f << "fFl80#,";
  font.m_font.setFlags(flags);
  int color = 1;
  if (type == 2) {
    color=(int) m_input->readULong(1);
  } else if (pos+type+5 <= endPos) {
    int val = (int) m_input->readULong(1);
    if (val == 0)
      f << "end0#,";
    else
      m_input->seek(-1, WPX_SEEK_CUR);
  }
  if (color != 1) {
    Vec3uc col;
    if (m_mainParser->getColor(color,col))
      font.m_font.setColor(col);
    else
      f << "#fColor=" << color << ",";
  }
  font.m_extra = f.str();
  return true;
}

void MSKText::setProperty(MSKTextInternal::Font const &font)
{
  if (!m_listener) return;
  font.m_font.sendTo(m_listener.get(), m_convertissor, m_state->m_font);
}

////////////////////////////////////////////////////////////
// the tabulations:
////////////////////////////////////////////////////////////
bool MSKText::readParagraph(MSKTextInternal::LineZone &zone, MSKTextInternal::Paragraph &parag)
{
  int dataSize = int(zone.m_pos.length())-6;
  if (dataSize < 15) return false;
  m_input->seek(zone.m_pos.begin()+6, WPX_SEEK_SET);

  parag= MSKTextInternal::Paragraph();
  libmwaw::DebugStream f;

  int fl[2];
  bool firstFlag = (dataSize & 1) == 0;
  fl[0] = firstFlag ? (int) m_input->readULong(1) : 0x4c;
  switch(fl[0]) {
  case 0x4c:
    break;
  case 0x43:
    parag.m_justify = libmwaw::JustificationCenter;
    break;
  case 0x52:
    parag.m_justify = libmwaw::JustificationRight;
    break;
  case 0x46:
    parag.m_justify = libmwaw::JustificationFull;
    break;
  default:
    f << "#align=" << std::hex << fl[0] << ",";
    MWAW_DEBUG_MSG(("MSKText::readParagraph: unknown alignment %x\n", fl[0]));
    break;
  }
  fl[1] = (int) m_input->readULong(1);
  if (fl[1])
    f << "fl0=" <<fl[1] << std::dec << ",";
  int dim[3];
  bool ok = true;
  for (int i = 0; i < 3; i++) {
    dim[i] = (int) m_input->readULong(2);
    if (i==0&&(dim[0]&0x8000)) {
      dim[0] &= 0x7FFF;
      f << "6linesByInches,";
    }
    if (dim[i] > 3000) ok = false;
  }
  if (!ok) {
    MWAW_DEBUG_MSG(("MSKText::readParagraph: size is very odd\n"));
    f << "##";
  }
  if (dim[0] || dim[1] || dim[2]) {
    f << "size=" << dim[1] << "x" << dim[0];
    if (dim[2]) f << "[" << dim[2] << "]";
    f << ",";
  }

  int fl2[2];
  for (int i = 0; i < 2; i++) // between -16 and 16
    fl2[i] = (int) m_input->readULong(1);
  if (fl2[0] || fl2[1])
    f << "fl2=(" << std::hex << fl2[0] << ", " <<fl2[1] << ")" << std::dec << ",";

  for (int i = 0; i < 3; i++) {
    int val = (int) m_input->readULong(2);
    int flag = (val & 0xc000) >> 14;
    val = (val & 0x3fff);
    if (val > 8000 || flag) {
      MWAW_DEBUG_MSG(("MSKText::readParagraph: find odd margin %d\n", i));
      f << "#margin" << i << "=" << val  << "(" << flag << "),";
    }
    if (val > 8000) continue;
    // i = 0 (last), i = 1 (firstL), i=2 (nextL)
    parag.m_margins[2-i] = val/72.0;
  }
  parag.m_margins[0] -= parag.m_margins[1];
  if (parag.m_margins[2] > 0.0)
    parag.m_margins[2] = m_mainParser->pageWidth()-parag.m_margins[2];
  if (parag.m_margins[2] > 56./72.) parag.m_margins[2] -= 28./72.;
  else if (parag.m_margins[2] >=0.0) parag.m_margins[2] *= 0.5;
  else parag.m_margins[2] = 0.0;
  int numVal = (dataSize-9)/2-3;
  parag.m_tabs.resize((size_t)numVal);
  size_t numTabs = 0;

  // checkme: in order to avoid x_tabs > textWidth (appears sometimes when i=0)
  long maxWidth = long(m_mainParser->pageWidth()*72-36);
  if (dim[1] > maxWidth) maxWidth = dim[1];

  for (int i = 0; i < numVal; i++) {
    int val = (int) m_input->readULong(2);
    MWAWTabStop::Alignment align = MWAWTabStop::LEFT;
    switch (val >> 14) {
    case 1:
      align = MWAWTabStop::DECIMAL;
      break;
    case 2:
      align = MWAWTabStop::RIGHT;
      break;
    case 3:
      align = MWAWTabStop::CENTER;
      break;
    default:
      break;
    }
    val = (val & 0x3fff);
    if (val > maxWidth) {
      static bool first = true;
      if (first) {
        MWAW_DEBUG_MSG(("MSKText::readParagraph: finds some odd tabs (ignored)\n"));
        first = false;
      }
      f << "#tabs" << i << "=" << val << ",";
      continue;
    }
    parag.m_tabs[numTabs].m_alignment = align;
    parag.m_tabs[numTabs++].m_position = val/72.0;
  }
  if (int(numTabs)!=numVal) parag.m_tabs.resize(numTabs);
  parag.m_extra = f.str();

  f.str("");
  f << "Entries(Paragraph):" << zone << "," << parag << ",";
  ascii().addPos(zone.m_pos.begin());
  ascii().addNote(f.str().c_str());

  return true;
}

void MSKText::setProperty(MSKTextInternal::Paragraph const &para)
{
  if (!m_listener) return;
  para.send(m_listener);
}

////////////////////////////////////////////////////////////
// read v1-v2 header/footer string
////////////////////////////////////////////////////////////
std::string MSKText::readHeaderFooterString(bool header)
{
  std::string res("");
  int numChar = (int) m_input->readULong(1);
  if (!numChar) return res;
  for (int i = 0; i < numChar; i++) {
    unsigned char c = (unsigned char) m_input->readULong(1);
    if (c == 0) {
      m_input->seek(-1, WPX_SEEK_CUR);
      break;
    }
    if (c == '&') {
      unsigned char nextC = (unsigned char) m_input->readULong(1);
      bool field = true;
      switch (nextC) {
      case 'F':
        res += char(0x19);
        break; // file name
      case 'D':
        res += char(0x17);
        break; // date
      case 'P':
        res += char(0x18);
        break; // page
      case 'T':
        res += char(0x16);
        break; // time
      default:
        field = false;
      }
      if (field) continue;
      m_input->seek(-1, WPX_SEEK_CUR);
    }
    res += (char) c;
  }
  if (res.length()) {
    m_state->m_zones.push_back(MSKTextInternal::TextZone());
    MSKTextInternal::TextZone &zone = m_state->m_zones.back();
    zone.m_id = int(m_state->m_zones.size())-1;
    zone.m_type = header ? MSKTextInternal::TextZone::Header :
                  MSKTextInternal::TextZone::Footer;
    zone.m_text = res;
  }
  return res;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////
void MSKText::send(MSKTextInternal::TextZone &zone, Vec2i limit)
{
  int numZones = int(zone.m_zonesList.size());
  if (numZones == 0 && zone.m_text.length()) {
    sendString(zone.m_text);
    zone.m_isSent = true;
    return;
  }
  bool isMain = false;
  Vec2i notePos(-1,-1);
  if (limit[0] < 0) {
    limit = Vec2i(0,numZones);
    isMain = zone.isMain();
    // find the notes in the text zones
    std::map<int, Vec2i>::const_iterator noteIt;
    for (noteIt=zone.m_footnoteMap.begin(); noteIt != zone.m_footnoteMap.end(); noteIt++) {
      if (notePos[0]==-1) {
        notePos = noteIt->second;
        continue;
      }
      if (notePos[0] > noteIt->second[0])
        notePos[0] = noteIt->second[0];
      if (notePos[1] < noteIt->second[1])
        notePos[1] = noteIt->second[1];
    }
  }

  for (int i = limit[0]; i < limit[1]; i++) {
    if (i == notePos[0]) {
      i = notePos[1]-1;
      continue;
    }
    if (isMain && zone.m_pagesPosition.find(i) != zone.m_pagesPosition.end()) {
      MSKTextInternal::Font actFont;
      actFont.m_font = m_state->m_font;
      m_mainParser->newPage(++m_state->m_actualPage, zone.m_pagesPosition[i]);
      setProperty(actFont);
    }
    MSKTextInternal::LineZone &z = zone.m_zonesList[(size_t)i];
    if (z.m_type & 0x80) {
      MSKTextInternal::Paragraph parag;
      if (readParagraph(z, parag))
        setProperty(parag);
    } else
      sendText(z, zone.m_id);
  }
  zone.m_isSent = true;
}

void MSKText::sendNote(int zoneId, int noteId)
{
  if (zoneId < 0 || zoneId >= int(m_state->m_zones.size())) {
    if (m_listener) m_listener->insertCharacter(' ');
    MWAW_DEBUG_MSG(("MSKText::sendNote: unknown zone %d\n", zoneId));
    return;
  }
  MSKTextInternal::TextZone &zone=m_state->m_zones[(size_t)zoneId];
  std::map<int, Vec2i>::const_iterator noteIt = zone.m_footnoteMap.find(noteId);
  if (noteIt==zone.m_footnoteMap.end()) {
    MWAW_DEBUG_MSG(("MSKText::sendNote: unknown note %d-%d\n", zoneId, noteId));
    if (m_listener) m_listener->insertCharacter(' ');
  } else
    send(zone, noteIt->second);
}

void MSKText::sendZone(int zoneId)
{
  if (zoneId < 0 || zoneId >= int(m_state->m_zones.size())) {
    MWAW_DEBUG_MSG(("MSKText::sendZone: unknown zone %d\n", zoneId));
    return;
  }
  send(m_state->m_zones[(size_t)zoneId]);
}


void MSKText::flushExtra()
{
  for (size_t i = 0; i < m_state->m_zones.size(); i++) {
    if (m_state->m_zones[i].m_isSent)
      continue;
    send(m_state->m_zones[i]);
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
