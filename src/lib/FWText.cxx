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

#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWParagraph.hxx"

#include "FWParser.hxx"

#include "FWText.hxx"

/** Internal: the structures of a FWText */
namespace FWTextInternal
{
/** Internal: class to store the LineHeader */
struct LineHeader {
  /** Constructor */
  LineHeader() : m_numChar(0), m_font(), m_fontSet(false), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, LineHeader const &line) {
    o << "numC=" << line.m_numChar << ",";
    if (line.m_fontSet)
      o << "font=[fId=" << line.m_font.id()
        << ",fSize=" << line.m_font.size() << "],";
    o << line.m_extra;
    return o;
  }
  /** the number of char */
  int m_numChar;
  /** the font */
  MWAWFont m_font;
  /** a flag to know if the font is set */
  bool m_fontSet;
  /** extra data */
  std::string m_extra;
};
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
    size_t numColumns = m_columns.size();
    if (numColumns != p.m_columns.size())
      return false;
    for (size_t c = 0; c < numColumns; c++) {
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
    if (z.m_end!=z.m_begin)
      o << "sz=" << std::hex << z.m_end-z.m_begin << std::dec << ",";
    if (z.m_extra.length())
      o << "extra=[" << z.m_extra << "],";
    return o;
  }
  //! return the col/page break
  std::vector<int> getBreaksPosition() const {
    size_t numPages = m_pagesInfo.size();
    int prevPos = 0;
    std::vector<int> res;
    for (size_t p = 0; p < numPages; p++) {
      PageInfo const &page = m_pagesInfo[p];
      for (size_t c = 0; c < page.m_columns.size(); c++) {
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
struct Paragraph : public MWAWParagraph {
  //! Constructor
  Paragraph() : MWAWParagraph(), m_align(0),
    m_interSpacing(1.), m_interSpacingUnit(WPX_PERCENT), m_isTable(false), m_isSent(false) { }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Paragraph const &ind) {
    if (ind.m_isTable) o << "table,";
    if (ind.m_align) o << "align=" << ind.m_align << ",";
    o << reinterpret_cast<MWAWParagraph const &>(ind);
    return o;
  }

  //! returns true if this is a table
  bool isTable() const {
    return m_isTable;
  }

  //! set the align type
  void setAlign(int align) {
    m_align = align;
    m_isSent = false;
  }
  //! set the interline spacing
  void setInterlineSpacing(double spacing, WPXUnit unit) {
    m_interSpacing = spacing;
    m_interSpacingUnit = unit;
  }

  //! update the paragraph data from a ruler
  void updateFromRuler(Paragraph const &ruler) {
    MWAWParagraph::operator=(ruler);
    m_isTable = ruler.m_isTable;
    m_isSent = false;
  }
  //! update the paragraph data to be sent to a listener
  void update() {
    if ( m_interSpacing>0) {
      m_spacings[0] = m_interSpacing;
      m_spacingsInterlineUnit = m_interSpacingUnit;
    }
    switch(m_align) {
    case 0:
      m_justify = libmwaw::JustificationLeft;
      break;
    case 1:
      m_justify = libmwaw::JustificationCenter;
      break;
    case 2:
      m_justify = libmwaw::JustificationFull;
      break;
    case 3:
      m_justify = libmwaw::JustificationRight;
      break;
    default:
      break;
    }
  }

  //! the align value
  int m_align;
  //! the spacing
  double m_interSpacing;
  //! the spacing unit
  WPXUnit m_interSpacingUnit;
  //! a flag to know if this is a table
  bool m_isTable;
  //! a flag to know if the parser is send or not
  mutable bool m_isSent;
};

////////////////////////////////////////
//! Internal: the state of a FWText
struct State {
  //! constructor
  State() : m_version(-1), m_entryMap(), m_paragraphMap(), m_paragraphModSet(),
    m_mainZones(), m_numPages(1), m_actualPage(0), m_font(-1, 0, 0) {
  }

  //! the file version
  mutable int m_version;

  //! zoneId -> entry
  std::multimap<int, shared_ptr<Zone> > m_entryMap;

  //! rulerId -> ruler
  std::map<int, Paragraph> m_paragraphMap;
  //! the list of seen paragraph mod id ( changme)
  std::set<int> m_paragraphModSet;

  //! the main zone index
  std::vector<int> m_mainZones;

  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
  //! the actual font
  MWAWFont m_font;

};
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
FWText::FWText
(MWAWInputStreamPtr ip, FWParser &parser, MWAWFontConverterPtr &convert) :
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
                  MWAWFont &font)
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
    val = (int)input->readULong(1);
    bool done = false;
    if (nextIsChar)
      nextIsChar = false;
    else {
      done = true;
      uint32_t fFlags = font.flags();
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
        fFlags ^= MWAW_BOLD_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x84:
        fFlags ^= MWAW_ITALICS_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x86:
        fFlags ^= MWAW_OUTLINE_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x87:
        fFlags ^= MWAW_SHADOW_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x88:
        fFlags ^= MWAW_SMALL_CAPS_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x89: // change color
        break;
      case 0x8a:
        fFlags ^= MWAW_SUPERSCRIPT100_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x8b:
        fFlags ^= MWAW_SUBSCRIPT100_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x8c:
        fFlags ^= MWAW_STRIKEOUT_BIT;
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
        fFlags ^= MWAW_UNDERLINE_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x8f:
        fFlags ^= MWAW_DOUBLE_UNDERLINE_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x93:
        fFlags ^= MWAW_OVERLINE_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x94:
        fFlags ^= MWAW_ALL_CAPS_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x95:
        fFlags ^= MWAW_SMALL_CAPS_BIT;
        font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0xa7: // fixme appear also as tabs separator
        m_listener->insertTab();
        break;
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
        font.setSize((int)input->readULong(1));
        fontSet=false;
        break;
      case 0xb3:
        nextIsChar = true;
        break;
      case 0xc1:
        if (actPos+2 > endPos) break;
        font.setId((int)input->readULong(2));
        fontSet = false;
        break;
      case 0xc7: // item id
        if (actPos+2 > endPos) break;
        id = (int)input->readULong(2); // fixme
        break;
      case 0xcb: // space/and or color
        if (actPos+2 > endPos) break;
        id = (int)input->readULong(2); // fixme
        break;
      case 0xd2:
      case 0xd3:
      case 0xd5:
      case 0xdc: {
        if (actPos+2 > endPos) break;
        id = (int)input->readULong(2);
        switch(val) {
        case 0xd2:
          m_mainParser->sendText(id, libmwaw::DOC_COMMENT_ANNOTATION);
          break;
        case 0xd3:
          m_mainParser->sendText(id, libmwaw::DOC_NOTE, MWAWContentListener::FOOTNOTE);
          break;
        case 0xd5:
          m_mainParser->sendText(id, libmwaw::DOC_NOTE, MWAWContentListener::ENDNOTE);
          break;
        case 0xdc:
          m_mainParser->sendGraphic(id);
          break;
        default:
          break;
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
      case 0x9b: // align
      case 0x9c:
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
      case 0xe5: // contents/index data
        if (actPos+2 > endPos) break;
        input->seek(2, WPX_SEEK_CUR);
        break;
      case 0xe1:
      case 0xe2:
      case 0xe4: {
        if (actPos+2 > endPos) break;
        int docId = int(input->readULong(2));
        switch(val) {
        case 0xe1: // reference field
          break;
        case 0xe2:
          m_mainParser->sendReference(docId);
          break;
        case 0xe4:
          m_mainParser->sendVariable(docId);
          break;
        default:
          break;
        }
        break;
      }
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
      setProperty(font, m_state->m_font);
      fontSet = true;
    }
    if (val >= 256)
      m_listener->insertUnicode((uint32_t) val);
    else {
      int unicode = m_convertissor->unicode(font.id(), (unsigned char) val);
      if (unicode != -1)
        m_listener->insertUnicode((uint32_t) unicode);
      else if (val > 0x1f)
        m_listener->insertCharacter((unsigned char) val);
    }
  }
}

bool FWText::readLineHeader(shared_ptr<FWTextInternal::Zone> zone, FWTextInternal::LineHeader &lHeader)
{
  lHeader = FWTextInternal::LineHeader();

  MWAWInputStreamPtr input = zone->m_zone->m_input;
  libmwaw::DebugStream f;
  long pos = input->tell();

  int type = (int)input->readULong(2);
  int lengthSz = 1;
  if (type & 0x8000)
    lengthSz = 2;

  lHeader.m_numChar = (int)input->readULong(lengthSz);
  if ((lengthSz==1 && (lHeader.m_numChar & 0x80)) ||
      pos+2+lHeader.m_numChar > zone->m_end) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  int val;
  if (type & 0x4000) {
    f << "f0=[";
    val = (int)input->readLong(1); // always 0 ?
    if (val) f << "unkn=" << val << ",";
    f << "N=" << (int)input->readLong(2) << ","; // small number
    val = (int)input->readLong(1); // almost always 0, but find 1 and 0x49
    if (val) f << "unkn1=" << val << ",";
    val = (int)input->readLong(2); // 0 or -1
    if (val == -1) f << "*,";
    else if (val) f << "unkn2=" << val << ",";
    val = (int)input->readLong(1); // small number between 20 and -20?
    if (val) f << "N1=" << val << ",";
    val = (int)input->readLong(1); // almost always 0, but find 80 ff, 66 cc
    if (val) f << "unkn3=" << std::hex << val << std::dec << ",";
    val = (int)input->readLong(2); // check me
    if (val) f << "textId=" << val << ",";
    f << "w=" << (int)input->readLong(2) << ",";
    f << "],";
  }
  if (type & 0x2000) {
    // small number between 1 and 4
    f << "f1=" << (int)input->readLong(1) << ",";
  }
  if (type & 0x1000) {
    // small number between 1 and 2
    f << "f2=" << (int)input->readLong(1) << ",";
  }
  if (type & 0x800) {
    // small number between 1 and 2
    f << "f3=" << (int)input->readLong(1) << ",";
  }
  if (type & 0x400) {
    // small number  1
    f << "f4=" << (int)input->readLong(1) << ",";
  }
  if (type & 0x200) {
    // small int between 0 and 0x4f : ident ?
    f << "f5=" << (int)input->readLong(2) << ",";
  }
  if (type & 0x100) {
    // small int between 0 and 0xb0 : ident ?
    f << "f6=" << (int)input->readLong(2) << ",";
  }
  if (type & 0x80) {
    // small int between 0 and 0x5b : ident ?
    f << "f7=" << (int)input->readLong(2) << ",";
  }
  if (type & 0x40) {
    // small int between 0 and 0xcf : ident ?
    f << "f8=" << (int)input->readLong(2) << ",";
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
      val = (int)input->readULong(1);
      if (val) f << std::hex << val << std::dec << ",";
      else f << "_,";
    }
    f << "],";
  }
  if (type & 0x10) {
    int fId = (int)input->readLong(2);
    int fSz = (int)input->readULong(2);
    lHeader.m_fontSet = true;
    lHeader.m_font.setId(fId);
    lHeader.m_font.setSize(fSz);
    f << "id=" << fId << ",";
    f << "sz=" << fSz << ",";
    // a small number ( often 1 but can be negative )
    f << "fontUnkn0=" << (int)input->readLong(2) << ",";
    val = (int)input->readULong(2); // often 0, but also 3333, 8000, b6b0, cccd, e666 : gray color ?
    if (val) f << "fontUnkn1=" << std::hex << val << std::dec << ",";
  }
  if (type & 0x8) { // font flag ?
    val = (int)input->readULong(2);
    f << "fa=" << std::hex << val << std::dec << ",";
  }
  if (type & 0x4) {
    MWAW_DEBUG_MSG(("FWText::readLineHeader: find unknown size flags!!!!\n"));
    f << "[#fl&4]";
    // we do not know the size of this field, let try with 2
    input->seek(2, WPX_SEEK_CUR);
  }
  if (type & 0x2) { // 0 or 2
    val = (int)input->readULong(2);
    f << "fb=" << val << ",";
  }
  if (type & 0x1) { // small number between 1 and 1b
    val = (int)input->readLong(2);
    f << "fc=" << val << ",";
  }
  lHeader.m_extra = f.str();
  return true;
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
  MWAWFont font(3,12);
  FWTextInternal::Paragraph ruler;

  std::vector<int> listBreaks = zone->getBreaksPosition();
  int numBreaks = int(listBreaks.size());
  int nPages = int(zone->m_pagesInfo.size());
  int actBreak = numBreaks, actBreakPos = -1;
  if (numBreaks) {
    actBreak = 0;
    actBreakPos = listBreaks[size_t(actBreak)];
  }
  int actPage = 0, actCol = 0, numCol=1;
  MWAWParagraph().send(m_listener);
  while(1) {
    pos = input->tell();
    bool sendData = false;
    f.str("");
    f << "TextData-a[" << num << "]:";
    while (num==actBreakPos) {
      if (num != 1) sendData = true;
      if (actCol < numCol-1 && numCol > 1) {
        m_listener->insertBreak(MWAW_COLUMN_BREAK);
        actCol++;
      } else if (actPage >= nPages) {
        MWAW_DEBUG_MSG(("FWText::send can not find the page information\n"));
      } else {
        FWTextInternal::PageInfo const &page = zone->m_pagesInfo[size_t(actPage)];
        if (sendData) {
          if (zone->m_main)
            m_mainParser->newPage(++m_state->m_actualPage);
          else if (numCol > 1)
            m_listener->insertBreak(MWAW_COLUMN_BREAK);
        }
        actCol = 0;

        if (!actPage || !page.isSimilar(zone->m_pagesInfo[size_t(actPage-1)])) {
          size_t numC = page.m_columns.size();
          libmwaw::SubDocumentType subdocType;
          if (m_listener->isSubDocumentOpened(subdocType) && numC <=1
              && subdocType != libmwaw::DOC_TEXT_BOX)
            ;
          else {
            if (m_listener->isSectionOpened())
              m_listener->closeSection();

            if (numC<=1 && sendData) m_listener->openSection();
            else {
              std::vector<int> colSize;
              colSize.resize(numC);
              for (size_t i = 0; i < numC; i++) colSize[i] = page.m_columns[i].m_box.size().x();
              m_listener->openSection(colSize, WPX_POINT);
            }
            numCol = int(numC);
          }
        }

        actPage++;
      }

      if (num != 1) f << "break,";
      sendData = true;
      // update numbreaks
      if (++actBreak < numBreaks)
        actBreakPos = listBreaks[(size_t)actBreak];
      else
        actBreakPos = -1;
    }
    num++;
    FWTextInternal::LineHeader lHeader;
    if (!readLineHeader(zone, lHeader)) {
      input->seek(pos, WPX_SEEK_SET);
      break;
    }
    f << lHeader;
    if (lHeader.m_fontSet) {
      font.setId(lHeader.m_font.id());
      font.setSize(lHeader.m_font.size());
    }
    if (lHeader.m_numChar)
      ascii.addDelimiter(input->tell(),'|');
    bool nextIsChar = false;
    long debPos = input->tell();
    long lastPos = debPos+lHeader.m_numChar;
    for (int i = 0; i < lHeader.m_numChar; i++) {
      long actPos = input->tell();
      if (actPos >= lastPos)
        break;
      val = (int)input->readULong(1);
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
      case 0x9b:
        ruler.setAlign(ruler.m_align^1);
        f << "[align=" << ruler.m_align << "]";
        break;
      case 0x9c:
        ruler.setAlign(ruler.m_align^2);
        f << "[align=" << ruler.m_align << "]";
        break;
      case 0x9d:
      case 0x9e:
      case 0x9f:
        ruler.setInterlineSpacing(1.0+(val-0x9d)/2.0, WPX_PERCENT);
        f << "[just=" << 100*ruler.m_interSpacing << "%]";
        break;
      case 0xc6: {
        if (actPos+2 > lastPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not find ruler!!!!\n"));
          f << "##";
          break;
        }
        int pId = (int) input->readULong(2);
        f << "[rulerId=" << pId << "]";
        std::map<int,FWTextInternal::Paragraph>::iterator it=
          m_state->m_paragraphMap.find(pId);
        if (it==m_state->m_paragraphMap.end()) {
          MWAW_DEBUG_MSG(("FWText::send: can not find paragraph with id=%d\n", pId));
          break;
        }
        ruler.updateFromRuler(it->second);
        break;
      }
      case 0xe8:
        if (actPos+4 > lastPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not find justify!!!!\n"));
          f << "##";
        } else { // negatif : pt, positif %?
          int just = (int)input->readLong(2);
          if (just < 0)
            ruler.setInterlineSpacing(-just, WPX_POINT);
          f << "[just=" << just << ",";
          f << "typ?=" << (int)input->readLong(2) << "]";
        }
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
          f << "[fs=" << (int)input->readULong(1) << "]";
        break;
      case 0xc1:
        if (actPos+2 > lastPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not read font id!!!!\n"));
          f << "##";
        } else
          f << "[fId=" << (int)input->readULong(2) << "]";
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
        f << "[tabs]";
        break;
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
          f << "[itemId=" << (int)input->readLong(2) << "]";
        break;
      case 0xcb:
        if (actPos+2 > lastPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not after/bef/col space!!!!\n"));
          f << "##";
        } else {
          int mId = (int)input->readLong(2);
          if (m_state->m_paragraphModSet.find(mId) == m_state->m_paragraphModSet.end()) {
            MWAW_DEBUG_MSG(("FWText::send: find unknown modId %d\n", mId));
            f << "[#modId=" << mId << "]";
          } else
            f << "[modId=" << mId << "]";
        }
        break;
      case 0xd0:
      case 0xd1:
      case 0xd2:
      case 0xd3:
      case 0xd5:
      case 0xdc:
      case 0xe1:
      case 0xe2:
        if (actPos+2 > lastPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not read note id!!!!\n"));
          f << "##";
          break;
        }
        switch (val) {
        case 0xd0:
          f << "[headerId=" << (int)input->readULong(2) << "]";
          break;
        case 0xd1:
          f << "[footerId=" << (int)input->readULong(2) << "]";
          break;
        case 0xd2:
          f << "[noteId=" << (int)input->readULong(2) << "]";
          break;
        case 0xd3:
          f << "[footnoteId=" << (int)input->readULong(2) << "]";
          break;
        case 0xd5:
          f << "[endnoteId=" << (int)input->readULong(2) << "]";
          break;
        case 0xdc:
          f << "[graphId=" << (int)input->readULong(2) << "]";
          break;
        case 0xe1:
          f << "[refDefId=" << (int)input->readULong(2) << "]";
          break;
        case 0xe2:
          f << "[refId=" << (int)input->readULong(2) << "]";
          break;
        default:
          break;
        }
        break;
      case 0x8d:
      case 0x97:
      case 0x9a:
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
      case 0xe5: // contents/index data
        if (actPos+2 > lastPos) {
          MWAW_DEBUG_MSG(("FWText::send: can find %x data!!!!\n", val));
          f << "##";
        } else
          f << "[f" << std::hex << val << std::dec << "=" << (int)input->readULong(2) << "]";
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
          MWAW_DEBUG_MSG(("FWText::send: can not find variable number!!!!\n"));
          f << "##";
        } else {
          f << "[variableId=" << (int)input->readLong(2) << "]";
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

    if (!ruler.m_isSent) {
      ruler.update();
      setProperty(ruler);
    }
    if (m_listener) {
      input->seek(debPos, WPX_SEEK_SET);
      send(zone, lHeader.m_numChar, font);
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

  bool knownZone = (zone->m_type >= 0xa && zone->m_type <= 0x14) || (zone->m_type == 0x18);

  int header = (int)input->readULong(2);
  bool hasHeader = false;
  if (!knownZone && (header&0xFC00)) return false;

  shared_ptr<FWTextInternal::Zone> text(new FWTextInternal::Zone);
  text->m_main = (zone->m_type==0xa);
  text->m_zone = zone;

  int val;
  if (!header) { // maybe the main zone
    bool ok = true;
    // find f0 in [1,270], f1 in [1,d2]
    for (int i = 0; i < 2 && ok ; i++) {
      val = (int)input->readLong(2);
      if (val <= 0 || (!i && val >= 0x800) || (i && val >= 0x200))
        ok = false;
    }
    for (int i = 0; i < 2 && ok; i++) {
      val = (int)input->readULong(1);
      if (val>1) ok=false;
    }
    val = ok ? (int)input->readLong(2) : 0;
    if (val < 1 || val > 20)
      ok=false;
    if (!ok) { // force to try as attachment
      header = 1;
      input->seek(pos+2, WPX_SEEK_SET);
    } else if (!knownZone)
      text->m_main = true;
  }
  if (header) { // attachement
    hasHeader = true;
    for (int i = 0; i < 2; i++) {
      val = (int)input->readLong(2);
      if (val <= 0 || val > 2) return false;
    }
    val = (int)input->readLong(vers==1 ? 1 : 2);
    if (val < 0 || val > 4) return false;
    if (vers==2) {
      if (val== 0) return false;
      val = (int)input->readLong(2);
      if (val) return false;
    }
    if (!knownZone)
      text->m_main = false;
  }
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  if (hasHeader) {
    //find 8, 1c, 71, 76, 8a, 99, 9d, c6, c9, ce, f3, f4
    f << "N0=" << (int)input->readLong(2) << ",";
    for (int i = 0; i < 2; i++) { // f0=f1=1 ? f2=1/2
      val = (int)input->readLong(2);
      if (val != 1) f << "f" << i << "=" << val << ",";
    }
    val = (int)input->readLong(vers==1 ? 1 : 2);
    if (val != 1) f << "f2=" << val << ",";
    if (vers == 1) {
      ascii.addDelimiter(input->tell(),'|');
      input->seek(59, WPX_SEEK_CUR);
      ascii.addDelimiter(input->tell(),'|');
    }
  }
  val = (int)input->readLong(2); // always 0 ?
  if (val) f << "f3=" << val << ",";

  /* N[0] related to number line ? N[1] between 1 and 5e */
  int N[2];
  for (int i = 0; i < 2; i++) {
    N[i] = (int)input->readLong(2);
    if (N[i])
      f << "N" << i+1 << "=" << std::hex << N[i] << std::dec << ",";
  }
  // two flag header/footer : 0|1, normal/other: 0|1?
  for (int i = 0; i < 2; i++) {
    text->m_flags[i] = (int)input->readLong(1);
    if (text->m_flags[i])
      f << "fl" << i << "=" << text->m_flags[i] << ",";
  }
  val = (int)input->readLong(2); // small number 1/2
  if (val!=1) f << "f4=" << val << ",";

  // between 1 and 202 ( very similar to dim[2]
  int dimUnk = (int)input->readLong(2);
  // two flags 0|ce|fa, 0
  for (int i = 3; i < 5; i++) {
    val = (int)input->readLong(1);
    if (val)
      f << "fl" << i << "=" << val << ",";
  }
  // between 8 and 58
  f << "N3=" << (int)input->readLong(2) << ",";
  int dim[4]; // box with almost always y1=y0+1?
  for (int i = 0; i < 4; i++)
    dim[i]=(int)input->readLong(2);
  if (dimUnk != dim[2]) f << "dimUnk=" << dimUnk << ",";

  text->m_box=Box2f(Vec2f(float(dim[0]),float(dim[1])),Vec2f(float(dim[2]),float(dim[3])));
  text->m_pages[1] = (int)input->readLong(2);
  if (text->m_pages[1] == 16000) text->m_pages[1] = 0;
  text->m_pages[0] = (int)input->readLong(2);
  if (text->m_pages[0] == 16000) text->m_pages[0] = text->m_pages[1];

  for (int i = 0; i < 2; i++) { // 1, 2 or 16000=infty?
    val = (int)input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }

  text->m_extra  = f.str();
  f.str("");
  f << "Entries(TextData):";
  if (zone->m_type >= 0)
    f << "zType=" << std::hex << zone->m_type << std::dec << ",";
  f << *text;
  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());
  if (long(input->tell()) >= zone->end())
    return false;

  text->m_begin = input->tell();
#if DEBUGII
  int numLines=0;
#endif
  while(1) {
    pos = input->tell();
    if (pos+2 >= zone->end()) break;
    int type = (int)input->readULong(2);
    int lengthSz = 1;
    if (type & 0x8000)
      lengthSz = 2;
    int numChar = (int)input->readULong(lengthSz);
    if ((lengthSz==1 && (numChar & 0x80)) ||
        long(input->tell()+numChar) > zone->end()) {
      input->seek(pos, WPX_SEEK_SET);
      break;
    }
#if DEBUGII
    numLines++;
#endif
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
#if DEBUGII
  std::cout << "FIND:N=" << numLines << " [" << N[0] << "]\n";
#endif
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
  val = (int)input->readULong(2);
  if (val || (int)input->readULong(1) != 0xFF) {
    f << "##";
    ascii.addPos(pos);
    ascii.addNote(f.str().c_str());
    return true;
  }

  f << "numCols=" << (int)input->readLong(2) << ",";
  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());
  int actPage = 0;
  while (1) {
    pos = input->tell();
    if (pos >= zone->end()) break;
    val = (int)input->readULong(2);
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
        col.m_beginPos = (int)input->readLong(2);
        for (int j = 0; j < 4; j++) dim[j] = (int)input->readLong(2);
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
      int numData = (int)input->readULong(2);
      if (!numData) break;
      sz = 26+9*numData;
    } else if (high==0xe1) {
      input->seek(14, WPX_SEEK_CUR);
      int numData = (int)input->readULong(2);
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
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the paragraph data
bool FWText::readParagraphTabs(shared_ptr<FWEntry> zone, int id)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &ascii = zone->getAsciiFile();
  libmwaw::DebugStream f;
  int const vers = version();
  const int dataSz = vers==1 ? 14 : 10;
  const int headerSz = vers==1 ? 24 : 30;
  long pos = input->tell();
  input->seek(pos, WPX_SEEK_SET);
  long sz = (long) input->readULong(4);
  if (sz<24 || pos+4+sz > zone->end()) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  f.str("");
  f << "Entries(Tabs):";
  int val;
  for (int i = 0; i < 2; i++) { // find f0 in 0 c0, f1=0|2
    val = (int)input->readULong(1);
    if (val) f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  for (int i = 0; i < 2; i++) {  // find f0=0|1|2: align ?, f1=0-7
    val = (int)input->readULong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  float dim[2];
  for (int i = 0; i < 2; i++) // dim0: height?, dim1:width
    dim[i] = (float)(input->readULong(2))/72.f;
  f << "dim?=" << dim[1] << "x" << dim[0] << ",";
  float margins[3];
  char const *(margNames[]) = { "left", "right", "first" };
  for (int i = 0; i < 3; i++) {
    margins[i] = (float)(input->readLong(4))/65536.f/72.0f;
    if (margins[i] < 0 || margins[i] > 0)
      f << "margins[" << margNames[i] << "]=" << margins[i] << ",";
  }
  FWTextInternal::Paragraph para;
  para.m_margins[0] = margins[2]-margins[0];
  para.m_margins[1] = margins[0];
  if (margins[1] < dim[1])
    para.m_margins[2]=dim[1]-margins[1];
  ascii.addDelimiter(input->tell(), '|');

  input->seek(vers==1 ? pos+27 : pos+26, WPX_SEEK_SET);
  int N = (int)input->readULong(1);
  if (headerSz+dataSz *N != sz) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  f << "N=" << N << ",";
  if (vers==2) {
    for (int i=0; i < 3; i++) {
      val = (int)input->readULong(1);
      if (val) f << "g" << i << "=" << val << ",";
    }
    if (input->readULong(4)) {
      para.m_isTable = true;
      f << "isTable,";
    }
    input->seek(pos+4+30, WPX_SEEK_SET);
  }
  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << "Tabs-" << i << ":";
    MWAWTabStop tab;
    val = (int)input->readULong(1);
    switch((val>>5) & 3) {
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
    default:
      break;
    }
    if (val & 0x80) f << "tableLimit,";
    // if (val & 0x1F) f << "#type=" << std::hex << (val & 0x1F) << std::dec << ",";
    val = (int)input->readULong(1);
    if (val != 0x2e) f << "#f0=" <<  std::hex << val << std::dec << ",";
    tab.m_position = float(input->readLong(4))/65536.f/72.f;
    val = (int)input->readLong(2);
    if (val) f << "repeat=" << val/256.;

    val = (int)input->readULong(1);
    switch(val) {
    case 0x20:
      break;
    case 0x1: // fixme: . very closed
      tab.m_leaderCharacter = '.';
      break;
    default:
      tab.m_leaderCharacter = uint16_t(val);
      break;
    }
    val = (int)input->readULong(1);
    if (val) f << "#f2=" <<  std::hex << val << std::dec << ",";
    f << tab;
    para.m_tabs->push_back(tab);
    ascii.addPos(pos);
    ascii.addNote(f.str().c_str());
    input->seek(pos+dataSz, WPX_SEEK_SET);
  }
  if (para.m_isTable) {
    pos = input->tell();
    sz = input->readLong(4);
    if (sz==0x24 && pos+4+sz <= zone->end()) {
      input->seek(0x24, WPX_SEEK_CUR);
      ascii.addPos(pos);
      ascii.addNote("Tabs-end");
    } else {
      MWAW_DEBUG_MSG(("FWText::readParagraphTabs: can not find table data\n"));
      input->seek(pos, WPX_SEEK_SET);
    }
  }
  if (id < 0) ;
  else if (m_state->m_paragraphMap.find(id) == m_state->m_paragraphMap.end())
    m_state->m_paragraphMap.insert
    (std::map<int,FWTextInternal::Paragraph>::value_type(id,para));
  else {
    MWAW_DEBUG_MSG(("FWText::readParagraphTabs: id %d already exists\n", id));
  }
  return true;
}

bool FWText::readParagraphMod(shared_ptr<FWEntry> zone, int id)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &ascii = zone->getAsciiFile();
  libmwaw::DebugStream f;

  long pos = input->tell();
  input->seek(pos, WPX_SEEK_SET);
  if (pos+10 > zone->end()) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  f.str("");
  f << "Entries(ParaMod):";
  f << "[" << std::hex;
  int val;
  /* f0 seems to be an font indexed color
     in v2: 0x6000=black, 0x8202(green), 830e(pure green)
   */
  /* f0=6000|801a|8202|830e|ffff, f1=0|ffff, f2=0|199a|ffff,
     f3=0|2|ffff,f4=0|1|2|3|199a|8000|ffff */
  for (int i = 0; i < 5; i++) {
    val = int(input->readULong(2));
    if (val==0xffff)
      f << "*,";
    else if (val == 0)
      f << "_,";
    else
      f << val << ",";
  }
  f << std::dec << "],";
  if (m_state->m_paragraphModSet.find(id) == m_state->m_paragraphModSet.end())
    m_state->m_paragraphModSet.insert(id);
  else {
    MWAW_DEBUG_MSG(("FWText::readParagraphMod: Oops id %d already find\n", id));
  }
  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());
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
  long sz = (long) input->readULong(4);
  if (sz<34 || pos+4+sz > zone->end()) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  input->seek(13, WPX_SEEK_CUR);
  int N = (int)input->readULong(1);
  if (24+10*N != sz) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  f.str("");
  f << "Entries(Columns):" << N;
  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());
  input->seek(pos+4+24, WPX_SEEK_SET);
  long val;
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << "Columns-" << i << ":";
    int dim[2];
    dim[0] = (int) input->readLong(2);
    val = (long) input->readULong(2);
    if (val) f << "f0=" << std::hex << val << std::dec << ",";
    dim[1] = (int) input->readLong(2);
    f << "pos=" << dim[0] << "<->" << dim[1] << ",";
    val = (long) input->readULong(2);
    if (val) f << "f1=" << std::hex << val << std::dec << ",";
    ascii.addDelimiter(input->tell(),'|');
    ascii.addPos(pos);
    ascii.addNote(f.str().c_str());
    input->seek(pos+10, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// send char/ruler property
void FWText::setProperty(MWAWFont const &font, MWAWFont &previousFont)
{
  if (!m_listener) return;
  font.sendTo(m_listener.get(), m_convertissor, previousFont);
}

void FWText::setProperty(FWTextInternal::Paragraph const &para)
{
  if (!m_listener) return;
  para.m_isSent = true;
  para.send(m_listener);
}

////////////////////////////////////////////////////////////
//! send data to the listener
bool FWText::sendMainText()
{
  size_t numZones = m_state->m_mainZones.size();
  if (!numZones) {
    MWAW_DEBUG_MSG(("FWText::sendMainText: can not find main zone\n"));
    return false;
  }
  if (!m_listener) return true;
  for (size_t i = 0; i < numZones; i++) {
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
  int numZones = 0, totalNumPages = 0;
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
      if (fPage < pagesLimits[size_t(2*pos)])
        break;
      if (fPage == pagesLimits[size_t(2*pos)] && lPage <= pagesLimits[size_t(2*pos+1)])
        break;
      pos++;
    }
    pagesLimits.resize(size_t(2*numZones+2));
    m_state->m_mainZones.resize(size_t(numZones+1));
    for (int i = numZones-1; i > pos-1; i--) {
      pagesLimits[size_t(2*i+2)]=pagesLimits[size_t(2*i)];
      pagesLimits[size_t(2*i+3)]=pagesLimits[size_t(2*i+1)];
      m_state->m_mainZones[size_t(i+1)]=m_state->m_mainZones[size_t(i)];
    }
    m_state->m_mainZones[size_t(pos)] = zone->m_zone->id();
    pagesLimits[size_t(2*pos)] = fPage;
    pagesLimits[size_t(2*pos+1)] = lPage;
    numZones++;
    int nPages = (lPage-fPage)+1;
    if (nPages < int(zone->m_pagesInfo.size())) {
      MWAW_DEBUG_MSG(("FWText::sortZones: pages limit seems odd!!!\n"));
      nPages = int(zone->m_pagesInfo.size());
    }
    totalNumPages += nPages;
  }
  m_state->m_numPages = totalNumPages;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
