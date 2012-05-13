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

#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"

#include "MWAWCell.hxx"
#include "MWAWTable.hxx"

#include "MWAWStruct.hxx"
#include "MWAWTools.hxx"
#include "MWAWContentListener.hxx"

#include "FWText.hxx"

#include "FWParser.hxx"

/** Internal: the structures of a FWText */
namespace FWTextInternal
{
/** Internal: class to store a ColumnInfo */
struct ColumnInfo {
  ColumnInfo() : m_column(0), m_box(), m_beginPos(1) {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, ColumnInfo const &c) {
    if (c.m_column > 0) o << "col=" << c.m_column+1 << ",";
    o << c.m_box << ",";
    if (c.m_beginPos > 1) o << "textPos=" << c.m_beginPos << ",";
    return o;
  }
  //! the column number
  int m_column;
  //! the bdbox
  Box2i m_box;
  //! the first data
  int m_beginPos;
};

struct PageInfo {
  PageInfo() : m_page(-1), m_columns() {
  }

  //! returns true if the page has same color position
  bool isSimilar(PageInfo const &p) const {
    int numColumns = m_columns.size();
    if (numColumns != int(p.m_columns.size()))
      return false;
    for (int c = 0; c < numColumns; c++) {
      if (m_columns[c].m_box[0].x() != p.m_columns[c].m_box[0].x())
        return false;
      if (m_columns[c].m_box[1].x() != p.m_columns[c].m_box[1].x())
        return false;
    }
    return true;
  }

  //! the pages
  int m_page;
  //! the columns
  std::vector<ColumnInfo> m_columns;
};

/** Internal: class to store a text zone */
struct Zone {
  Zone() : m_zone(), m_box(), m_begin(-1), m_end(-1), m_main(false), m_pagesInfo(), m_extra("") {
    for (int i = 0; i < 2; i++) m_flags[i] = 0;
    for (int i = 0; i < 2; i++) m_pages[i] = 0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Zone const &z) {
    if (z.m_main) o << "Main,";
    if (z.m_zone) o << *z.m_zone << ",";
    if (z.m_pages[0]) o << "firstP=" << z.m_pages[0] << ",";
    if (z.m_pages[1] && z.m_pages[1] != z.m_pages[0])
      o << "lastP=" << z.m_pages[1] << ",";
    o << "Box=" << z.m_box << ",";
    for (int i = 0; i < 2; i++) {
      if (z.m_flags[i])
        o << "fl" << i << "=" << z.m_flags[i] << ",";
    }
    o << "sz=" << std::hex << z.m_end-z.m_begin << std::dec << ",";
    if (z.m_extra.length())
      o << "extra=[" << z.m_extra << "],";
    return o;
  }
  //! return the col/page break
  std::vector<int> getBreaksPosition() const {
    int numPages = m_pagesInfo.size();
    int prevPos = 0;
    std::vector<int> res;
    for (int p = 0; p < numPages; p++) {
      PageInfo const &page = m_pagesInfo[p];
      for (int c = 0; c < int(page.m_columns.size()); c++) {
        int pos = page.m_columns[c].m_beginPos;
        if (pos < prevPos) {
          MWAW_DEBUG_MSG(("FWTextInternal::Zone:;getBreaksPosition pos go back\n"));
          p = numPages;
          break;
        }
        res.push_back(pos);
        prevPos = pos;
      }
    }
    return res;
  }
  //! the main zone
  shared_ptr<FWEntry> m_zone;
  //! the bdbox
  Box2f m_box;

  //! the beginning of the text data
  long m_begin;
  //! the end of the text data
  long m_end;
  //! true if the zone has no header
  bool m_main;
  //! the zone flags, header|footer, normal|extra
  int m_flags[2];
  //! the pages
  int m_pages[2];
  //! the pages info
  std::vector<PageInfo> m_pagesInfo;
  //! the extra data ( for debugging )
  std::string m_extra;
};

/** Internal: class to store the paragraph properties */
struct Ruler {
  //! Constructor
  Ruler() : m_justify (DMWAW_PARAGRAPH_JUSTIFICATION_LEFT),
    m_interline(1.0), m_interlinePercent(true), m_tabs(),
    m_error("") {
    for(int c = 0; c < 2; c++) // default value
      m_margins[c] = 0.0;
    m_margins[2] = -1.0;
    for(int c = 0; c < 2; c++) // default value
      m_spacings[c] = 0.0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Ruler const &ind) {
    if (ind.m_justify) {
      o << "Just=";
      switch(ind.m_justify) {
      case DMWAW_PARAGRAPH_JUSTIFICATION_LEFT:
        o << "left";
        break;
      case DMWAW_PARAGRAPH_JUSTIFICATION_CENTER:
        o << "centered";
        break;
      case DMWAW_PARAGRAPH_JUSTIFICATION_RIGHT:
        o << "right";
        break;
      case DMWAW_PARAGRAPH_JUSTIFICATION_FULL:
        o << "full";
        break;
      default:
        o << "#just=" << ind.m_justify << ", ";
        break;
      }
      o << ", ";
    }
    if (ind.m_margins[0]) o << "firstLPos=" << ind.m_margins[0] << ", ";
    if (ind.m_margins[1]) o << "leftPos=" << ind.m_margins[1] << ", ";
    if (ind.m_margins[2]>=0.0) o << "rightPos=" << ind.m_margins[2] << ", ";
    if (ind.m_interline > 0.0) {
      o << "interline=" << ind.m_interline;
      if (ind.m_interlinePercent)
        o << "%,";
      else
        o << "pt,";
    }
    if (ind.m_spacings[0]) o << "topSpace=" << ind.m_spacings[0] << ",";
    if (ind.m_spacings[1]) o << "bottomSpace=" << ind.m_spacings[0] << ",";
    if (ind.m_tabs.size()) {
      libmwaw::internal::printTabs(o, ind.m_tabs);
      o << ",";
    }
    if (ind.m_error.length()) o << ind.m_error << ",";
    return o;
  }

  /** the margins in inches
   *
   * 0: first line left, 1: left, 2: right (from right)
   */
  float m_margins[3];

  //! paragraph justification : DMWAW_PARAGRAPH_JUSTIFICATION*
  int m_justify;
  /** the line height */
  float m_interline;
  /** true if the interline is percent*/
  int m_interlinePercent;
  /** the top/bottom spacing in inches */
  float m_spacings[2];
  //! the tabulations
  std::vector<DMWAWTabStop> m_tabs;
  /** the errors */
  std::string m_error;
};

////////////////////////////////////////
//! Internal: the state of a FWText
struct State {
  //! constructor
  State() : m_version(-1), m_entryMap(), m_textFileIdMap(),
    m_mainZones(), m_numPages(1), m_actualPage(0), m_font(-1, 0, 0) {
  }

  //! return the zone id ( if found or -1)
  int getZoneId(int textId) const {
    std::map<int,int>::const_iterator it = m_textFileIdMap.find(textId);
    if (it == m_textFileIdMap.end()) {
      MWAW_DEBUG_MSG(("FWTextInternal::State::getZoneId can not find %d\n", textId));
      return -1;
    }
    return it->second;
  }

  //! the file version
  mutable int m_version;

  //! zoneId -> entry
  std::multimap<int, shared_ptr<Zone> > m_entryMap;

  //! the correspondance id
  std::map<int,int> m_textFileIdMap;

  //! the main zone index
  std::vector<int> m_mainZones;

  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
  //! the actual font
  MWAWStruct::Font m_font;

};
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
FWText::FWText
(MWAWInputStreamPtr ip, FWParser &parser, MWAWTools::ConvertissorPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new FWTextInternal::State),
  m_mainParser(&parser)
{
}

FWText::~FWText()
{ }

int FWText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_mainParser->version();
  return m_state->m_version;
}

int FWText::numPages() const
{
  return m_state->m_numPages;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////
void FWText::send(shared_ptr<FWTextInternal::Zone> zone, int numChar,
                  MWAWStruct::Font &font)
{
  if (!m_listener) return;
  MWAWInputStreamPtr input = zone->m_zone->m_input;
  long pos = input->tell();
  long endPos = pos+numChar;
  bool nextIsChar = false;
  bool fontSet = false;
  int val;
  for (int i = 0; i < numChar; i++) {
    long actPos = input->tell();
    if (actPos >= endPos)
      break;
    val = input->readULong(1);
    bool done = false;
    if (nextIsChar)
      nextIsChar = false;
    else {
      done = true;
      int fFlags = font.flags();
      int id;
      switch(val) {
      case 0:
        val=' ';
        done = false;
        break; // space
      case 0x80:
        break; // often found by pair consecutively : selection ?
      case 0x81:
        break; // often found by pair around a " " a ","...
      case 0x83:
        fFlags ^= DMWAW_BOLD_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x84:
        fFlags ^= DMWAW_ITALICS_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x86:
        fFlags ^= DMWAW_OUTLINE_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x87:
        fFlags ^= DMWAW_SHADOW_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x88:
        fFlags ^= DMWAW_SMALL_CAPS_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
        //case 0x89: f << "[color]"; break;
      case 0x8a:
        fFlags ^= DMWAW_SUPERSCRIPT100_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x8b:
        fFlags ^= DMWAW_SUBSCRIPT100_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x8c:
        fFlags ^= DMWAW_STRIKEOUT_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x90:
        break; // condensed
      case 0x91:
        break; // extended

      case 0x85:
      case 0x8e: // word underline
      case 0x92: // dotted underline
        fFlags ^= DMWAW_UNDERLINE_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x8f:
        fFlags ^= DMWAW_DOUBLE_UNDERLINE_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x93:
        fFlags ^= DMWAW_OVERLINE_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x94:
        fFlags ^= DMWAW_ALL_CAPS_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x95:
        fFlags ^= DMWAW_SMALL_CAPS_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0xa7:
        val = ' ';
        done = false;
        break; // appear for instance in note
      case 0xac:
        val = ' ';
        done = false;
        break; // item separator
      case 0xae:
        val=0x2011;
        done = false;
        break; // insecable-
      case 0xb2:
        if (actPos+1 > endPos) break;
        font.setSize(input->readULong(1));
        fontSet=false;
        break;
      case 0xb3:
        nextIsChar = true;
        break;
      case 0xc1:
        if (actPos+2 > endPos) break;
        font.setId(input->readULong(2));
        fontSet = false;
        break;
      case 0xc7: // item id
        if (actPos+2 > endPos) break;
        id = input->readULong(2); // fixme
        break;
      case 0xcb: // space/and or color
        if (actPos+2 > endPos) break;
        id = input->readULong(2); // fixme
        break;
      case 0xd2:
      case 0xd3:
      case 0xd5: {
        if (actPos+2 > endPos) break;
        id = input->readULong(2);
        int fId = m_state->getZoneId(id);
        if (fId != -1) {
          switch(val) {
          case 0xd2:
            m_mainParser->sendText(fId, MWAW_SUBDOCUMENT_COMMENT_ANNOTATION);
            break;
          case 0xd3:
            m_mainParser->sendText(fId, MWAW_SUBDOCUMENT_NOTE, FOOTNOTE);
            break;
          case 0xd5:
            m_mainParser->sendText(fId, MWAW_SUBDOCUMENT_NOTE, ENDNOTE);
            break;
          }
        }
        break;
      }
      case 0xa0: // potential hypen break
        if (actPos+1==endPos) {
          val = '-';
          done = false;
        }
        break;
      case 0x98:
        val=' ';
        done = false;
        break; // fixme in a table ?
      case 0x96: // field end
      case 0x9d: // justication
      case 0x9e:
      case 0x9f:
      case 0xa8:
        break; // some parenthesis system ?
      case 0xab:
        break; // some parenthesis system ?
        break; // ok
      case 0xc6: // ruler id
        if (actPos+2 > endPos) break;
        input->seek(2, WPX_SEEK_CUR);
        break; // ok
      case 0xe8: // justification
        if (actPos+4 > endPos) break;
        input->seek(4, WPX_SEEK_CUR);
        break; // ok

      case 0x8d: // unknown
      case 0x97:
      case 0x9a:
      case 0x9b:
      case 0x9c:
      case 0xa1:
      case 0xa9:
      case 0xaa:
        break;
      case 0xca: // unknown
      case 0xcd:
      case 0xd0: // header
      case 0xd1: // footer ?
      case 0xd4: // contents
      case 0xd6: // biblio
      case 0xd7: // entry
      case 0xd9:
      case 0xda:
      case 0xdc:
      case 0xe1:
      case 0xe2:
      case 0xe4:
      case 0xe5: // contents/index data
        if (actPos+2 > endPos) break;
        input->seek(2, WPX_SEEK_CUR);
        break;
      case 0xe9:
        if (actPos+4 > endPos) break;
        input->seek(4, WPX_SEEK_CUR);
        break;
      default:
        done = false;
        break;
      }
    }
    if (done) continue;
    if (!fontSet) {
      setProperty(font, m_state->m_font, true);
      fontSet = true;
    }
    if (val >= 256)
      m_listener->insertUnicode(val);
    else {
      int unicode = m_convertissor->getUnicode (font, val);
      if (unicode != -1)
        m_listener->insertUnicode(unicode);
      else if (val > 0x1f)
        m_listener->insertCharacter(val);
    }
  }
}

bool FWText::send(shared_ptr<FWTextInternal::Zone> zone)
{
  MWAWInputStreamPtr input = zone->m_zone->m_input;
  libmwaw::DebugFile &ascii = zone->m_zone->getAsciiFile();
  libmwaw::DebugStream f;

  zone->m_zone->setParsed(true);

  long pos = zone->m_begin;
  input->seek(pos, WPX_SEEK_SET);
  int val, num=1;
  MWAWStruct::Font font(3,12);
  FWTextInternal::Ruler ruler;
  bool rulerSent = true;

  std::vector<int> listBreaks = zone->getBreaksPosition();
  int numBreaks = listBreaks.size();
  int numPages = zone->m_pagesInfo.size();
  int actBreak = numBreaks, actBreakPos = -1;
  if (numBreaks) {
    actBreak = 0;
    actBreakPos = listBreaks[actBreak];
  }
  int actPage = 0, actCol = 0, numCol=1;
  while(1) {
    pos = input->tell();
    int type = input->readULong(2);
    int lengthSz = 1;
    if (type & 0x8000)
      lengthSz = 2;
    f.str("");
    f << "TextData-a[" << num << "]:";
    bool sendData = false;
    while (num==actBreakPos) {
      if (num != 1) sendData = true;
      if (actCol < numCol-1 && numCol > 1) {
        m_listener->insertBreak(DMWAW_COLUMN_BREAK);
        actCol++;
      } else if (actPage >= numPages) {
        MWAW_DEBUG_MSG(("FWText::send can not find the page information\n"));
      } else {
        FWTextInternal::PageInfo const &page = zone->m_pagesInfo[actPage];
        if (sendData) {
          if (zone->m_main)
            m_mainParser->newPage(++m_state->m_actualPage);
          else if (numCol > 1)
            m_listener->insertBreak(DMWAW_COLUMN_BREAK);
        }
        actCol = 0;

        if (!actPage || !page.isSimilar(zone->m_pagesInfo[actPage-1])) {
          if (m_listener->isSectionOpened())
            m_listener->closeSection();

          int numC = page.m_columns.size();

          if (numC<=1 && sendData) m_listener->openSection();
          else {
            std::vector<int> colSize;
            colSize.resize(numC);
            for (int i = 0; i < numC; i++) colSize[i] = page.m_columns[i].m_box.size().x();
            m_listener->openSection(colSize, WPX_POINT);
          }
          numCol = numC;
        }

        actPage++;
      }

      if (num != 1) f << "break,";
      sendData = true;
      // update numbreaks
      if (++actBreak < numBreaks)
        actBreakPos = listBreaks[actBreak];
      else
        actBreakPos = -1;
    }
    num++;
    int numChar = input->readULong(lengthSz);
    if ((lengthSz==1 && (numChar & 0x80)) ||
        long(input->tell()+numChar) > zone->m_end) {
      input->seek(pos, WPX_SEEK_SET);
      break;
    }

    if (type & 0x4000) {
      f << "f0=[";
      val = input->readLong(1); // always 0 ?
      if (val) f << "unkn=" << val << ",";
      f << "N=" << input->readLong(2) << ","; // small number
      val = input->readLong(1); // almost always 0, but find 1 and 0x49
      if (val) f << "unkn1=" << val << ",";
      val = input->readLong(2); // 0 or -1
      if (val == -1) f << "*,";
      else if (val) f << "unkn2=" << val << ",";
      val = input->readLong(1); // small number between 20 and -20?
      if (val) f << "N1=" << val << ",";
      val = input->readLong(1); // almost always 0, but find 80 ff, 66 cc
      if (val) f << "unkn3=" << std::hex << val << std::dec << ",";
      val = input->readLong(2); // check me
      if (val) f << "textId=" << val << ",";
      f << "w=" << input->readLong(2) << ",";
      f << "],";
    }
    if (type & 0x2000) {
      // small number between 1 and 4
      f << "f1=" << input->readLong(1) << ",";
    }
    if (type & 0x1000) {
      // small number between 1 and 2
      f << "f2=" << input->readLong(1) << ",";
    }
    if (type & 0x800) {
      // small number between 1 and 2
      f << "f3=" << input->readLong(1) << ",";
    }
    if (type & 0x400) {
      // small number  1
      f << "f4=" << input->readLong(1) << ",";
    }
    if (type & 0x200) {
      // small int between 0 and 0x4f : ident ?
      f << "f5=" << input->readLong(2) << ",";
    }
    if (type & 0x100) {
      // small int between 0 and 0xb0 : ident ?
      f << "f6=" << input->readLong(2) << ",";
    }
    if (type & 0x80) {
      // small int between 0 and 0x5b : ident ?
      f << "f7=" << input->readLong(2) << ",";
    }
    if (type & 0x40) {
      // small int between 0 and 0xcf : ident ?
      f << "f8=" << input->readLong(2) << ",";
    }
    if (type & 0x20) {
      /* first small number:
         0|10|80|81, 0|1|2|4|10,
         last small number 0|1|...|7|e|10|11|20|21|23|40|41
            &1  line continue ?
            &2  new page ?
      */
      f << "f9=[";
      for (int i = 0; i < 4; i++) {
        val = input->readULong(1);
        if (val) f << std::hex << val << std::dec << ",";
        else f << "_,";
      }
      f << "],";
    }
    if (type & 0x10) {
      f << "font=[";
      // 0x3385 or 2|3|10|14|..
      int fId = input->readLong(2);
      int fSz = input->readULong(2);
      font.setId(fId);
      font.setSize(fSz);
      f << "id=" << fId << ",";
      f << "sz=" << fSz << ",";
      // a small number ( often 1 but can be negative )
      f << "unkn0=" << input->readLong(2) << ",";
      val = input->readULong(2); // often 0, but also 3333, 8000, b6b0, cccd, e666 : gray color ?
      if (val) f << "unkn1=" << std::hex << val << std::dec << ",";
      f << "],";
    }
    if (type & 0x8) { // font flag ?
      val = input->readULong(2);
      f << "fa=" << std::hex << val << std::dec << ";";
    }
    if (type & 0x4) {
      MWAW_DEBUG_MSG(("FWText::send: find unknown size flags!!!!\n"));
      f << "[#fl&4]";
      // we do not know the size of this field, let try with 2
      input->seek(2, WPX_SEEK_CUR);
    }
    if (type & 0x2) { // 0 or 2
      val = input->readULong(2);
      f << "fb=" << val << ";";
    }
    if (type & 0x1) { // small number between 1 and 1b
      val = input->readLong(2);
      f << "fc=" << val << ";";
    }

    if (numChar)
      ascii.addDelimiter(input->tell(),'|');
    bool nextIsChar = false;
    long debPos = input->tell();
    long lastPos = debPos+numChar;
    for (int i = 0; i < numChar; i++) {
      long actPos = input->tell();
      if (actPos >= lastPos)
        break;
      val = input->readULong(1);
      if (nextIsChar) {
        if (val < 0x80)
          f << "#" << int(val);
        else
          f << char(val);
        nextIsChar = false;
        continue;
      }
      nextIsChar = false;
      switch(val) {
        // -------- relative to ruler
      case 0x9d:
      case 0x9e:
      case 0x9f:
        ruler.m_interline = (1.0+(val-0x9d)/2.0);
        ruler.m_interlinePercent = true;
        rulerSent = false;
        f << "[just=" << ruler.m_interline << "%]";
        break;
      case 0xe8:
        if (actPos+4 > lastPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not find justify!!!!\n"));
          f << "##";
        } else { // negatif : pt, positif %?
          int just = input->readLong(2);
          if (just < 0) {
            ruler.m_interline = -just;
            ruler.m_interlinePercent = false;
            rulerSent = false;
          }
          f << "[just=" << input->readLong(2) << ",";
          f << "typ?=" << input->readLong(2) << "]";
        }
        break;
      case 0xc6:
        if (actPos+2 > lastPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not find ruler!!!!\n"));
          f << "##";
        } else
          f << "[rulerId=" << input->readLong(2) << "]";
        break;

        // -------- relative to char style
      case 0x83:
        f << "[b]";
        break; // bold
      case 0x84:
        f << "[it]";
        break; // italique
      case 0x85:
        f << "[under]";
        break; // underline
      case 0x86:
        f << "[outline]";
        break;
      case 0x87:
        f << "[shadow]";
        break;
      case 0x88:
        f << "[smallcap]";
        break;
      case 0x89:
        f << "[color]";
        break;
      case 0x8a:
        f << "[sup]";
        break; // superscript
      case 0x8b:
        f << "[sub]";
        break; // subscript
      case 0x8c:
        f << "[strikethrough]";
        break;
      case 0x8e:
        f << "[under:word]";
        break;
      case 0x8f:
        f << "[under:double]";
        break;
      case 0x90:
        f << "[condensed]";
        break;
      case 0x91:
        f << "[extended]";
        break;
      case 0x92:
        f << "[under:dotted]";
        break;
      case 0x93:
        f << "[overbar]";
        break;
      case 0x94:
        f << "[uppercase]";
        break;
      case 0x95:
        f << "[lowercase]";
        break;
      case 0xb2:
        if (actPos+1 > lastPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not read font size!!!!\n"));
          f << "##";
        } else
          f << "[fs=" << input->readULong(1) << "]";
        break;
      case 0xc1:
        if (actPos+2 > lastPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not read font id!!!!\n"));
          f << "##";
        } else
          f << "[fId=" << input->readULong(2) << "]";
        break;

        // -------- relative to char
      case 0:
        f << " ";
        break; // space
      case 0x80:
        break; // often found by pair consecutively : selection ?
      case 0x81:
        break; // often found by pair around a " " a ","...
      case 0x98:
        f << "\\n";
        break; // in a table ?
      case 0xa0:
        break; // hyphen
      case 0xae:
        f << "-";
        break; // insecable
      case 0xb3:
        nextIsChar = true;
        break;

      case 0xa7:
        f << "[sep]";
        break; // appear for instance in note
      case 0xa8:
        f << "]";
        break; // some parenthesis system ?
      case 0xab:
        f << "[";
        break; // some parenthesis system ?
      case 0xac:
        f << "[ItemSep]";
        break;
      case 0xc7:
        if (actPos+2 > lastPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not find item id!!!!\n"));
          f << "##";
        } else
          f << "[itemId=" << input->readLong(2) << "]";
        break;
      case 0xcb:
        if (actPos+2 > lastPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not after/bef/col space!!!!\n"));
          f << "##";
        } else
          f << "[modId=" << input->readLong(2) << "]";
        break;
      case 0xd0:
      case 0xd1:
      case 0xd2:
      case 0xd3:
      case 0xd5:
        if (actPos+2 > lastPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not read note id!!!!\n"));
          f << "##";
          break;
        }
        switch (val) {
        case 0xd0:
          f << "[headerId=" << input->readULong(2) << "]";
          break;
        case 0xd1:
          f << "[footerId=" << input->readULong(2) << "]";
          break;
        case 0xd2:
          f << "[noteId=" << input->readULong(2) << "]";
          break;
        case 0xd3:
          f << "[footnoteId=" << input->readULong(2) << "]";
          break;
        case 0xd5:
          f << "[endnoteId=" << input->readULong(2) << "]";
          break;
        default:
          break;
        }
        break;
      case 0x8d:
      case 0x97:
      case 0x9a:
      case 0x9b:
      case 0x9c:
      case 0xa1:
      case 0xa9:
      case 0xaa:
        f << "[f" << std::hex << val << std::dec << "]";
        break;
      case 0xca:
      case 0xcd:
      case 0xd4: // contents
      case 0xd6: // biblio
      case 0xd7: // entry
      case 0xd9:
      case 0xda:
      case 0xdc:
      case 0xe1:
      case 0xe2:
      case 0xe5: // contents/index data
        if (actPos+2 > lastPos) {
          MWAW_DEBUG_MSG(("FWText::send: can find %x data!!!!\n", val));
          f << "##";
        } else
          f << "[f" << std::hex << val << std::dec << "=" << input->readULong(2) << "]";
        break;
      case 0xe9:
        if (actPos+4 > lastPos) {
          MWAW_DEBUG_MSG(("FWText::send: can find %x data!!!!\n", val));
          f << "##";
        } else {
          f << "[f" << std::hex << val << "=" << input->readULong(2);
          f << "<->" << input->readULong(2) << std::dec << "]";
        }
        break;
      case 0xe4:
        if (actPos+2 > lastPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not find field number!!!!\n"));
          f << "##";
        } else {
          f << "[FNum=" << input->readLong(2) << "]";
        }
        break;
      case 0x96:
        f << "[FEnd]";
        break;
      default:
        if (val < 0x20 || val >= 0x80)
          f << "[#" << std::hex << val << std::dec << "]";
        else
          f << char(val);
        break;
      }
    }
    ascii.addPos(pos);
    ascii.addNote(f.str().c_str());

    if (!rulerSent) {
      setProperty(ruler);
      rulerSent = true;
    }
    if (m_listener) {
      input->seek(debPos, WPX_SEEK_SET);
      send(zone, numChar, font);
      m_listener->insertEOL();
      input->seek(lastPos, WPX_SEEK_SET);
    }
    if (long(input->tell()) >= zone->m_end)
      break;
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the text data
bool FWText::readTextData(shared_ptr<FWEntry> zone)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &ascii = zone->getAsciiFile();
  int vers = version();

  long pos = zone->begin();
  input->seek(pos, WPX_SEEK_SET);
  int header = input->readULong(2);
  if (header&0xFC00) return false;

  shared_ptr<FWTextInternal::Zone> text(new FWTextInternal::Zone);
  text->m_zone = zone;

  int val;
  if (header) { // attachement
    text->m_main = false;
    for (int i = 0; i < 2; i++) {
      val = input->readLong(2);
      if (val <= 0 || val > 2) return false;
    }
    val = input->readLong(vers==1 ? 1 : 2);
    if (val < 0 || val > 4) return false;
    if (vers==2 && val== 0) return false;
    val = input->readLong(2);
    if (val) return false;
  } else { // main
    text->m_main = true;
    for (int i = 0; i < 2; i++) {
      val = input->readLong(2);
      if (val <= 0 || val >= 0x200) return false;

    }
    for (int i = 0; i < 2; i++) {
      val = input->readULong(1);
      if (val>1) return false;
    }
    val = input->readLong(2);
    if (val < 1 || val > 5)
      return false;
  }
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  if (header) {
    //find 8, 1c, 71, 76, 8a, 99, 9d, c6, c9, ce, f3, f4
    f << "N0=" << input->readLong(2) << ",";
    for (int i = 0; i < 2; i++) { // f0=f1=1 ? f2=1/2
      val = input->readLong(2);
      if (val != 1) f << "f" << i << "=" << val << ",";
    }
    val = input->readLong(vers==1 ? 1 : 2);
    if (val != 1) f << "f2=" << val << ",";
    if (vers == 1) {
      ascii.addDelimiter(input->tell(),'|');
      input->seek(59, WPX_SEEK_CUR);
      ascii.addDelimiter(input->tell(),'|');
    }
  }
  val = input->readLong(2); // always 0 ?
  if (val) f << "f3=" << val << ",";

  /* N1=small number between 1 an 1b, N2 between 1 and 5e */
  for (int i = 0; i < 2; i++) {
    val = input->readLong(2);
    if (val)
      f << "N" << i+1 << "=" << std::hex << val << std::dec << ",";
  }
  // two flag header/footer : 0|1, normal/other: 0|1?
  for (int i = 0; i < 2; i++) {
    text->m_flags[i] = input->readLong(1);
    if (text->m_flags[i])
      f << "fl" << i << "=" << text->m_flags[i] << ",";
  }
  val = input->readLong(2); // small number 1/2
  if (val!=1) f << "f4=" << val << ",";

  // between 1 and 202 ( very similar to dim[2]
  int N = input->readLong(2);
  // two flags 0|ce|fa, 0
  for (int i = 3; i < 5; i++) {
    val = input->readLong(1);
    if (val)
      f << "fl" << i << "=" << val << ",";
  }
  // between 8 and 58
  f << "N3=" << input->readLong(2) << ",";
  int dim[4]; // box with almost always y1=y0+1?
  for (int i = 0; i < 4; i++)
    dim[i]=input->readLong(2);
  if (N != dim[2]) f << "N=" << N << ",";

  text->m_box=Box2f(Vec2f(dim[0],dim[1]),Vec2f(dim[2],dim[3]));
  text->m_pages[1] = input->readLong(2);
  if (text->m_pages[1] == 16000) text->m_pages[1] = 0;
  text->m_pages[0] = input->readLong(2);
  if (text->m_pages[0] == 16000) text->m_pages[0] = text->m_pages[1];

  for (int i = 0; i < 2; i++) { // 1, 2 or 16000=infty?
    val = input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }

  text->m_extra  = f.str();
  f.str("");
  f << "Entries(TextData):" << *text;
  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());
  if (long(input->tell()) >= zone->end())
    return false;

  text->m_begin = input->tell();
  while(1) {
    pos = input->tell();
    if (pos+2 >= zone->end()) break;
    int type = input->readULong(2);
    int lengthSz = 1;
    if (type & 0x8000)
      lengthSz = 2;
    int numChar = input->readULong(lengthSz);
    if ((lengthSz==1 && (numChar & 0x80)) ||
        long(input->tell()+numChar) > zone->end()) {
      input->seek(pos, WPX_SEEK_SET);
      break;
    }

    int sz = 0;
    if (type & 0x4000) sz+=12;
    if (type & 0x2000) sz+=1;
    if (type & 0x1000) sz+=1;
    if (type & 0x800) sz+=1;
    if (type & 0x400) sz+=1;
    if (type & 0x200) sz+=2;
    if (type & 0x100) sz+=2;
    if (type & 0x80) sz+=2;
    if (type & 0x40) sz+=2;
    if (type & 0x20) sz+=4;
    if (type & 0x10) sz+=8;
    if (type & 0x8) sz+=2;
    if (type & 0x4) {
      MWAW_DEBUG_MSG(("FWText::readTextData: find unknown size flags!!!!\n"));
      sz+=2;
    }
    if (type & 0x2) sz+=2;
    if (type & 0x1) sz+=2;
    if (numChar || sz)
      input->seek(numChar+sz, WPX_SEEK_CUR);
  }
  pos = text->m_end = input->tell();
  // ok, we can insert the data
  std::multimap<int, shared_ptr<FWTextInternal::Zone> >::iterator it =
    m_state->m_entryMap.find(zone->id());
  if (it != m_state->m_entryMap.end()) {
    MWAW_DEBUG_MSG(("FWText::readTextData: entry %d already exists\n", zone->id()));
  }
  m_state->m_entryMap.insert
  ( std::multimap<int, shared_ptr<FWTextInternal::Zone> >::value_type(zone->id(), text));

  f.str("");
  f << "TextData-b:";
  val = input->readULong(2);
  if (val || input->readULong(1) != 0xFF) {
    f << "##";
    ascii.addPos(pos);
    ascii.addNote(f.str().c_str());
    return true;
  }

  f << "numCols=" << input->readLong(2) << ",";
  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());
  int actPage = 0;
  while (1) {
    pos = input->tell();
    if (pos >= zone->end()) break;
    int val = input->readULong(2);
    int sz = 0;
    if ((val%50)==30) {
      int num = val/50;
      sz = 32+num*52;

      input->seek(9, WPX_SEEK_CUR);

      if (pos+sz > zone->end()) {
        input->seek(pos, WPX_SEEK_SET);
        break;
      }

      FWTextInternal::PageInfo page;
      page.m_page = actPage++;

      ascii.addPos(pos);
      ascii.addNote("TextData-c[ColH]");

      pos+=32;
      input->seek(pos, WPX_SEEK_SET);
      for (int i = 0; i < num; i++) {
        pos = input->tell();
        f.str("");
        f << "TextData-c[ColDef]:";
        FWTextInternal::ColumnInfo col;
        col.m_column = i;
        col.m_beginPos = input->readLong(2);
        int dim[4];
        for (int j = 0; j < 4; j++) dim[j] = input->readLong(2);
        col.m_box = Box2i(Vec2i(dim[0],dim[2]), Vec2i(dim[1], dim[3]));
        f << col;
        page.m_columns.push_back(col);

        ascii.addDelimiter(input->tell(),'|');
        ascii.addPos(pos);
        ascii.addNote(f.str().c_str());
        input->seek(pos+52, WPX_SEEK_SET);
      }
      text->m_pagesInfo.push_back(page);
      continue;
    }

    // ok, probably link to a col field, but...
    // let try to correct
    int high = (val>>8);
    if (high==0x21)
      sz = 42;
    else if (high==0x61||high==0x63) {
      input->seek(12, WPX_SEEK_CUR);
      int numData = input->readULong(2);
      if (!numData) break;
      sz = 26+9*numData;
    } else if (high==0xe1) {
      input->seek(14, WPX_SEEK_CUR);
      int numData = input->readULong(2);
      if (!numData) break;
      sz = 30+9*numData;
    } else if (high==0 && (val%50)!=30)
      sz=26;
    if (sz == 0 || pos+sz > zone->end()) {
      input->seek(pos, WPX_SEEK_SET);
      break;
    }

    ascii.addPos(pos);
    ascii.addNote("TextData-d:");
    input->seek(pos+sz, WPX_SEEK_SET);
  }

  if (input->tell() < zone->end()) {
    ascii.addPos(input->tell());
    ascii.addNote("TextData-d:");
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the style data
bool FWText::readStyle(shared_ptr<FWEntry> zone)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &ascii = zone->getAsciiFile();
  libmwaw::DebugStream f;
  //  int vers = version();

  long pos = zone->begin();
  input->seek(pos, WPX_SEEK_SET);
  zone->setParsed(true);
  f << "Entries(Style)|" << *zone << ":";
  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());

  int num = 0;
  while (1) {
    pos = input->tell();
    num++;
    if (pos+4 >= zone->end()) break;
    if (readParagraph(zone)) continue;
    if (readColumns(zone)) continue;
    if (readCorrespondance(zone)) continue;
    if (readStyleName(zone)) continue;

    int sz = 0;
    int val = input->readULong(2);
    f.str("");
    f << "Style-" << num-1 << ":";
    bool done = false;
    switch (val) {
    case 0: {
      sz = input->readULong(2);
      if (sz && sz < 100 && (sz%2)==0)
        sz += 4;
      else
        sz = 0;
      break;
    }
    case 4:
      if (input->readULong(2)) break;
      sz = 6;
      break;
    case 0x39:
      sz=2;
      break;
    default:
      break;
    }
    if (done) continue;
    if (pos+sz > zone->end())
      break;

    if (!sz) {
      input->seek(pos+2, WPX_SEEK_SET);
      int nextVal = input->readULong(2);
      if (nextVal == 0xFFFF) // color
        sz = 10;
      else if (nextVal == 0 && (val & 0xF00F) ==  0x800a)
        sz = 133;
      if (!sz || pos+sz > zone->end()) {
        sz = 0;
        input->seek(-2, WPX_SEEK_CUR);
      }
    }
    if (sz) {
      ascii.addPos(pos);
      ascii.addNote(f.str().c_str());
      input->seek(pos+sz, WPX_SEEK_SET);
      continue;
    }

    int numZero =0;
    bool ok = false;
    while (1) {
      if (long(input->tell())+0x1e > zone->end())
        break;
      int val = input->readULong(1);
      if (val == 0) {
        numZero++;
        continue;
      }
      int actNumZero = numZero;
      numZero = 0;
      if ((val & 0xF8)==0x18 && actNumZero) {
        long actPos = input->tell();
        input->seek(-2, WPX_SEEK_CUR);
        ok = readCorrespondance(zone, true);
        if (ok) break;
        input->seek(actPos, WPX_SEEK_SET);
      }
      if (val==0x46 && actNumZero) {
        long actPos = input->tell();
        input->seek(-2, WPX_SEEK_CUR);
        ok = readStyleName(zone);
        if (ok) break;
        input->seek(actPos, WPX_SEEK_SET);
      }
      if (actNumZero < 3 || val < 24) continue;
      input->seek(1,WPX_SEEK_CUR);
      if (input->readULong(1) > 2) {
        input->seek(-2,WPX_SEEK_CUR);
        continue;
      }
      input->seek(-6, WPX_SEEK_CUR);
      long actPos = input->tell();
      ok = readParagraph(zone);
      if (ok) break;
      input->seek(actPos, WPX_SEEK_SET);
      ok = readColumns(zone);
      if (ok) break;
      input->seek(actPos+4, WPX_SEEK_SET);
    }
    if (!ok) break;
    ascii.addDelimiter(pos,'#');
  }
  if (pos != zone->end()) {
    ascii.addPos(pos);
    ascii.addNote("Entries(Style)#");
  }

  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the paragraph data
bool FWText::readParagraph(shared_ptr<FWEntry> zone)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &ascii = zone->getAsciiFile();
  libmwaw::DebugStream f;
  int vers = version();
  const int dataSz = vers==1 ? 14 : 10;
  const int headerSz = vers==1 ? 24 : 30;
  long pos = input->tell();
  input->seek(pos, WPX_SEEK_SET);
  long sz = input->readULong(4);
  if (sz<24 || pos+4+sz > zone->end()) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  input->seek(vers==1 ? 23: 22, WPX_SEEK_CUR);
  int N = input->readULong(1);
  if (headerSz+dataSz *N != sz) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  f.str("");
  f << "Entries(Tabs):N=" << N << ",";
  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());
  if (vers==2)
    input->seek(pos+4+30, WPX_SEEK_SET);
  int val;
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << "Tabs-" << i << ":";
    val = input->readULong(1);
    switch((val>>5) & 3) {
    case 0:
      break;
    case 1:
      f << "center,";
      break;
    case 2:
      f << "right,";
      break;
    case 3:
    default:
      f << "decimal,";
      break;
    }
    if (val & 0x80) f << "tableLimit,";
    // if (val & 0x1F) f << "#type=" << std::hex << (val & 0x1F) << std::dec << ",";
    val = input->readULong(1);
    if (val != 0x2e) f << "#f0=" <<  std::hex << val << std::dec << ",";
    f << "pos=" << input->readLong(4)/65536. << ",";
    val = input->readLong(2);
    if (val) f << "repeat=" << val/256.;

    val = input->readULong(1);
    switch(val) {
    case 0x20:
      break;
    case 0x1:
      f << "leader=...,";
      break;
    default:
      f << "leader=" << char(val) << ",";
      break;
    }
    val = input->readULong(1);
    if (val) f << "#f2=" <<  std::hex << val << std::dec << ",";
    ascii.addPos(pos);
    ascii.addNote(f.str().c_str());
    input->seek(pos+dataSz, WPX_SEEK_SET);
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the column data
bool FWText::readColumns(shared_ptr<FWEntry> zone)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &ascii = zone->getAsciiFile();
  libmwaw::DebugStream f;

  long pos = input->tell();
  input->seek(pos, WPX_SEEK_SET);
  long sz = input->readULong(4);
  if (sz<34 || pos+4+sz > zone->end()) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  input->seek(13, WPX_SEEK_CUR);
  int N = input->readULong(1);
  if (24+10*N != sz) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  f.str("");
  f << "Entries(Columns):" << N;
  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());
  input->seek(pos+4+24, WPX_SEEK_SET);
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << "Columns-" << i << ":";
    ascii.addPos(pos);
    ascii.addNote(f.str().c_str());
    input->seek(pos+10, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read an unknown zone
bool FWText::readStyleName(shared_ptr<FWEntry> zone)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &ascii = zone->getAsciiFile();
  libmwaw::DebugStream f;
  f << "Entries(StylName):";

  long pos = input->tell();
  if (pos+72 > zone->end()) return false;

  int val = input->readULong(2);
  if (val != 0x46 || input->readULong(4)) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  int sz = input->readULong(1);
  if (sz == 0 || sz >= 32) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  std::string str("");
  for (int i = 0; i < sz; i++) {
    char c= input->readLong(1);
    if (!c) {
      input->seek(pos, WPX_SEEK_SET);
      return false;
    }
    str += c;
  }
  f << str << ",";
  input->seek(pos+38, WPX_SEEK_SET);

  ascii.addDelimiter(input->tell(),'|');
  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());
  input->seek(pos+72, WPX_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
// read the correspondance data
bool FWText::readCorrespondance(shared_ptr<FWEntry> zone, bool extraCheck)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &ascii = zone->getAsciiFile();
  libmwaw::DebugStream f;
  f << "Entries(Correspondance):";

  long pos = input->tell();
  int val = input->readULong(1);
  if ((extraCheck & val) || (val&0xfb)) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  if (val) f << "#type" << std::hex << val << std::dec << ",";
  int type = input->readULong(1);
  if (!(type >= 0x18 && type <=0x1f)) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  int sz = 73;
  f << "type=" << std::hex << type << std::dec << ",";
  val = input->readULong(2);
  if (val && extraCheck) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  if (val)
    f << "#f0=" << val << ",";
  val = input->readULong(1); // 0, 6 or 0x10, 0x1e
  if (val) f << "f1=" << std::hex << val << std::dec << ",";
  val = input->readLong(1); // 0 or  0x1 or -10
  if (val != 1) f << "f2=" << val << ",";
  int N = input->readLong(2);
  if (N) // can be a big number, but some time 0, 1, 3, 4, ...
    f << "N0=" << N << ",";
  // small number between 1 and 0x1f
  val = input->readLong(2);
  if (val) f << "N1=" << val << ",";

  val = input->readLong(1); // 0, 1, 2, -1, -2
  if (val) f << "f3=" << val << ",";
  val = input->readULong(1); // 12, 1f, 22, 23, 25, 2d, 32, 60, 62, 66, 67, ...
  if (val) f << "f4=" << std::hex << val << std::dec << ",";

  // small number, g0, g2 often negative
  for (int i = 0; i < 4; i++) {
    val = input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }

  int numError = 0;
  val = input->readLong(2); // alway -2
  if (val != -2) {
    if (extraCheck || val > 0 || val < -2) {
      input->seek(pos, WPX_SEEK_SET);
      return false;
    }
    f << "#g4=" << val << ",";
    numError++;
  }
  for (int i = 0; i < 3; i++) {
    // first a small number < 3e1, g6,g7 almost always 0 expected one time g6=-1a9
    val = input->readLong(4);
    if (!val) continue;
    if (i==2) {
      if (extraCheck || numError) {
        input->seek(pos, WPX_SEEK_SET);
        return false;
      }
      numError++;
    }
    f << "g" << i+5 << "=" << val << ",";
  }
  int fileId = input->readULong(2);
  int textId = input->readULong(2);
  std::map<int,int>::iterator it = m_state->m_textFileIdMap.find(textId);
  if (it != m_state->m_textFileIdMap.end()) {
    MWAW_DEBUG_MSG(("FWText::readCorrespondance: id %d already exists\n", textId));
    f << "#";
  } else
    m_state->m_textFileIdMap[textId] = fileId;
  f << "id=" << textId << "->" << fileId << ",";

  ascii.addDelimiter(input->tell(),'|');
  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());
  input->seek(pos+sz, WPX_SEEK_SET);
  return true;
}
////////////////////////////////////////////////////////////
// send char/ruler property
void FWText::setProperty(MWAWStruct::Font const &font,
                         MWAWStruct::Font &previousFont,
                         bool force)
{
  if (!m_listener) return;
  font.sendTo(m_listener.get(), m_convertissor, previousFont, force);
}

void FWText::setProperty(FWTextInternal::Ruler const &para)
{
  if (!m_listener) return;
  m_listener->justificationChange(para.m_justify);

  if (para.m_interline > 0.0)
    m_listener->lineSpacingChange(para.m_interline,
                                  para.m_interlinePercent ? WPX_PERCENT :  WPX_POINT);
  else
    m_listener->lineSpacingChange(1.0, WPX_PERCENT);

  m_listener->setParagraphTextIndent(para.m_margins[0]+para.m_margins[1]);
  m_listener->setParagraphMargin(para.m_margins[1], DMWAW_LEFT);
  if (para.m_margins[2] >= 0.0)
    m_listener->setParagraphMargin(para.m_margins[2], DMWAW_RIGHT);

  m_listener->setParagraphMargin(para.m_spacings[0]/72., DMWAW_TOP);
  m_listener->setParagraphMargin(para.m_spacings[1]/72., DMWAW_BOTTOM);
  m_listener->setTabs(para.m_tabs);
}

////////////////////////////////////////////////////////////
//! send data to the listener
bool FWText::sendMainText()
{
  int numZones = m_state->m_mainZones.size();
  if (!numZones) {
    MWAW_DEBUG_MSG(("FWText::sendMainText: can not find main zone\n"));
    return false;
  }
  if (!m_listener) return true;
  for (int i = 0; i < numZones; i++) {
    std::multimap<int, shared_ptr<FWTextInternal::Zone> >::iterator it;
    it = m_state->m_entryMap.find(m_state->m_mainZones[i]);
    if (it == m_state->m_entryMap.end() || !it->second) {
      MWAW_DEBUG_MSG(("FWText::sendMainText: can not find main zone: internal problem\n"));
      continue;
    }
    m_mainParser->newPage(++m_state->m_actualPage);
    send(it->second);
  }
  return true;
}

bool FWText::send(int zId)
{
  std::multimap<int, shared_ptr<FWTextInternal::Zone> >::iterator it;
  it = m_state->m_entryMap.find(zId);
  if (it == m_state->m_entryMap.end() || !it->second) {
    MWAW_DEBUG_MSG(("FWText::send: can not find zone: %d\n", zId));
    return false;
  }
  send(it->second);
  return true;
}

void FWText::flushExtra()
{
  std::multimap<int, shared_ptr<FWTextInternal::Zone> >::iterator it;
  for (it = m_state->m_entryMap.begin(); it != m_state->m_entryMap.end(); it++) {
    shared_ptr<FWTextInternal::Zone> zone = it->second;
    if (!zone || !zone->m_zone || zone->m_zone->isParsed())
      continue;
    send(zone);
  }
}

void FWText::sortZones()
{
  std::multimap<int, shared_ptr<FWTextInternal::Zone> >::iterator it;
  int numZones = 0, numPages = 0;
  std::vector<int> pagesLimits;
  for (it = m_state->m_entryMap.begin(); it != m_state->m_entryMap.end(); it++) {
    shared_ptr<FWTextInternal::Zone> zone = it->second;
    if (!zone || !zone->m_zone || !zone->m_main)
      continue;
    int fPage = zone->m_pages[0], lPage = zone->m_pages[1];
    if (lPage < fPage) {
      MWAW_DEBUG_MSG(("FWText::sortZones: last page is inferior to firstPage\n"));
      lPage = fPage;
    }
    int pos = 0;
    while (pos < numZones) {
      if (fPage < pagesLimits[2*pos])
        break;
      if (fPage == pagesLimits[2*pos] && lPage <= pagesLimits[2*pos+1])
        break;
      pos++;
    }
    pagesLimits.resize(2*numZones+2);
    m_state->m_mainZones.resize(numZones+1);
    for (int i = numZones-1; i >= pos; i--) {
      pagesLimits[2*i+2]=pagesLimits[2*i];
      pagesLimits[2*i+3]=pagesLimits[2*i+1];
      m_state->m_mainZones[i+1]=m_state->m_mainZones[i];
    }
    m_state->m_mainZones[pos] = zone->m_zone->id();
    pagesLimits[2*pos] = fPage;
    pagesLimits[2*pos+1] = lPage;
    numZones++;
    int nPages = (lPage-fPage)+1;
    if (nPages < int(zone->m_pagesInfo.size())) {
      MWAW_DEBUG_MSG(("FWText::sortZones: pages limit seems odd!!!\n"));
      nPages = int(zone->m_pagesInfo.size());
    }
    numPages += nPages;
  }
  m_state->m_numPages = numPages;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
