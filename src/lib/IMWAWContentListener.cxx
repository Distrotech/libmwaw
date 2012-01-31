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

#include "DMWAWSubDocument.hxx"

#include "libmwaw_internal.hxx"
#include "TMWAWPosition.hxx"

#include "IMWAWCell.hxx"
#include "IMWAWList.hxx"
#include "IMWAWSubDocument.hxx"

#include "IMWAWContentListener.hxx"

#include <iostream>
#include <cmath>

IMWAWContentListener::ParsingState::ParsingState() :
  m_textBuffer(), m_numDeferredTabs(0),
  m_footNoteNumber(0), m_endNoteNumber(0), m_numNestedNotes(0),
  m_border(0), m_language(""),  m_parag_language(""),
  m_doc_language("en"), m_isFrameOpened(false), m_list(), m_listOrderedLevels() {}

IMWAWContentListener::ParsingState::ParsingState
(IMWAWContentListener::ParsingState const &orig)  :
  m_textBuffer(), m_numDeferredTabs(0),
  m_footNoteNumber(0), m_endNoteNumber(0), m_numNestedNotes(0),
  m_border(0), m_language(""),  m_parag_language(""),
  m_doc_language("en"), m_isFrameOpened(false), m_list(), m_listOrderedLevels(0)
{
  *this = orig;
}

IMWAWContentListener::ParsingState::~ParsingState() {}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
IMWAWContentListener::IMWAWContentListener(std::list<DMWAWPageSpan> &pageList, WPXDocumentInterface *documentInterface) :
  DMWAWContentListener(pageList, documentInterface), m_parseState(0L), m_actualListId(0),
  m_subDocuments(), m_listDocuments()
{
  // adjust the first paragraph margin

  m_ps->m_paragraphMarginTop = m_ps->m_paragraphMarginBottom = 0.0;
  if (pageList.begin() != pageList.end())
    m_ps->m_pageMarginLeft = pageList.begin()->getMarginLeft();
}

IMWAWContentListenerPtr IMWAWContentListener::create(std::list<DMWAWPageSpan> &pageList,
    WPXDocumentInterface *documentInterface)
{
  shared_ptr<IMWAWContentListener> res(new IMWAWContentListener(pageList, documentInterface));
  res->_init();
  return res;
}

void IMWAWContentListener::_init()
{
  if (!m_parseState) m_parseState = _newParsingState();

  *m_ps->m_fontName = "Times New Roman";
  m_ps->m_fontSize = 12.0;

}

IMWAWContentListener::~IMWAWContentListener()
{
  delete m_parseState;
}

bool IMWAWContentListener::isParagraphOpened() const
{
  return m_ps->m_isParagraphOpened;
}

bool IMWAWContentListener::isSectionOpened() const
{
  return m_ps->m_isSectionOpened;
}

bool IMWAWContentListener::openSection(std::vector<int> colsWidth, WPXUnit unit)
{
  if (m_ps->m_isSectionOpened) {
    MWAW_DEBUG_MSG(("IMWAWContentListener::openSection: a section is already opened\n"));
    return false;
  }

  if (m_ps->m_isTableOpened || (m_ps->m_inSubDocument && m_ps->m_subDocumentType != DMWAW_SUBDOCUMENT_TEXT_BOX)) {
    MWAW_DEBUG_MSG(("IMWAWContentListener::openSection: impossible to open a section\n"));
    return false;
  }

  int numCols = colsWidth.size();
  if (numCols <= 1) {
    m_ps->m_textColumns.resize(0);
    m_ps->m_numColumns=1;
  } else {
    float factor = 1.0;
    switch(unit) {
    case WPX_POINT:
      factor = 1/72.;
      break;
    case WPX_TWIP:
      factor = 1/1440.;
      break;
    case WPX_INCH:
      break;
    default:
      MWAW_DEBUG_MSG(("IMWAWContentListener::openSection: unknonwn unit\n"));
      break;
    }
    m_ps->m_textColumns.resize(numCols);
    m_ps->m_numColumns=numCols;
    for (int col = 0; col < numCols; col++) {
      DMWAWColumnDefinition column;
      column.m_width = factor*colsWidth[col];
      m_ps->m_textColumns[col] = column;
    }
  }
  _openSection();
  return true;
}

bool IMWAWContentListener::closeSection()
{
  if (!m_ps->m_isSectionOpened) {
    MWAW_DEBUG_MSG(("IMWAWContentListener::closeSection: no section are already opened\n"));
    return false;
  }

  if (m_ps->m_isTableOpened || (m_ps->m_inSubDocument && m_ps->m_subDocumentType != DMWAW_SUBDOCUMENT_TEXT_BOX)) {
    MWAW_DEBUG_MSG(("IMWAWContentListener::closeSection: impossible to close a section\n"));
    return false;
  }

  _closeSection();
  return true;
}

void IMWAWContentListener::setDocumentLanguage(std::string const &locale)
{
  if (!locale.length()) return;
  if (parsingState()) m_parseState->m_doc_language = locale;
  m_metaData.insert("libwpd:language", locale.c_str());
}

void IMWAWContentListener::_flushDeferredTabs()
{
  if (isUndoOn() || m_parseState->m_numDeferredTabs == 0) return;

  // CHECKME: the tab are not underline even if the underline bit is set
  uint32_t oldTextAttributes = m_ps->m_textAttributeBits;
  uint32_t newAttributes = oldTextAttributes & (~DMWAW_UNDERLINE_BIT);
  if (oldTextAttributes != newAttributes) setTextAttribute(newAttributes);
  if (!m_ps->m_isSpanOpened) _openSpan();
  for (; m_parseState->m_numDeferredTabs > 0; m_parseState->m_numDeferredTabs--)
    m_documentInterface->insertTab();
  if (oldTextAttributes != newAttributes) setTextAttribute(oldTextAttributes);
}


void IMWAWContentListener::insertCharacter(uint8_t character)
{
  if (isUndoOn()) return;
  if (character >= 0x80) {
    insertUnicode(character);
    return;
  }
  _flushDeferredTabs ();
  if (!m_ps->m_isSpanOpened) _openSpan();
  m_parseState->m_textBuffer.append(character);
}

void IMWAWContentListener::insertUnicode(uint32_t val)
{
  if (isUndoOn()) return;

  // undef character, we skip it
  if (val == 0xfffd) return;

  _flushDeferredTabs ();
  if (!m_ps->m_isSpanOpened) _openSpan();
  appendUnicode(val, m_parseState->m_textBuffer);
}

void IMWAWContentListener::insertUnicodeString(WPXString const &str)
{
  if (isUndoOn()) return;

  _flushDeferredTabs ();
  if (!m_ps->m_isSpanOpened) _openSpan();
  m_parseState->m_textBuffer.append(str);
}

void IMWAWContentListener::appendUnicode(uint32_t val, WPXString &buffer)
{
  uint8_t first;
  int len;
  if (val < 0x80) {
    first = 0;
    len = 1;
  } else if (val < 0x800) {
    first = 0xc0;
    len = 2;
  } else if (val < 0x10000) {
    first = 0xe0;
    len = 3;
  } else if (val < 0x200000) {
    first = 0xf0;
    len = 4;
  } else if (val < 0x4000000) {
    first = 0xf8;
    len = 5;
  } else {
    first = 0xfc;
    len = 6;
  }

  uint8_t outbuf[6] = { 0, 0, 0, 0, 0, 0 };
  int i;
  for (i = len - 1; i > 0; --i) {
    outbuf[i] = (val & 0x3f) | 0x80;
    val >>= 6;
  }
  outbuf[0] = val | first;
  for (i = 0; i < len; i++) buffer.append(outbuf[i]);
}

void IMWAWContentListener::insertEOL()
{
  if (isUndoOn()) return;

  if (!m_ps->m_isParagraphOpened && !m_ps->m_isListElementOpened) _openSpan();
  for (; m_parseState->m_numDeferredTabs > 0; m_parseState->m_numDeferredTabs--)
    m_documentInterface->insertTab();

  if (m_ps->m_isParagraphOpened) _closeParagraph();
  if (m_ps->m_isListElementOpened) _closeListElement();

  // sub/superscript must not survive a new line
  if (m_ps->m_textAttributeBits & (DMWAW_SUBSCRIPT_BIT | DMWAW_SUPERSCRIPT_BIT | DMWAW_SUBSCRIPT100_BIT | DMWAW_SUPERSCRIPT100_BIT))
    m_ps->m_textAttributeBits &= ~(DMWAW_SUBSCRIPT_BIT | DMWAW_SUPERSCRIPT_BIT | DMWAW_SUBSCRIPT100_BIT | DMWAW_SUPERSCRIPT100_BIT);
}

void IMWAWContentListener::insertTab()
{
  if (isUndoOn()) return;

  if (!m_ps->m_isParagraphOpened) {
    m_parseState->m_numDeferredTabs++;
    return;
  }

  // CHECKME: the tab are not underline even if the underline bit is set
  uint32_t oldTextAttributes = m_ps->m_textAttributeBits;
  uint32_t newAttributes = oldTextAttributes & (~DMWAW_UNDERLINE_BIT);

  if (oldTextAttributes != newAttributes) setTextAttribute(newAttributes);

  if (!m_ps->m_isSpanOpened) _openSpan();
  else _flushText();

  m_documentInterface->insertTab();

  if (oldTextAttributes != newAttributes)
    setTextAttribute(oldTextAttributes);
}

void IMWAWContentListener::_flushText()
{
  if (m_parseState->m_textBuffer.len() == 0) return;

  // when some many ' ' follows each other, call insertSpace
  WPXString tmpText;
  int numConsecutiveSpaces = 0;
  WPXString::Iter i(m_parseState->m_textBuffer);
  for (i.rewind(); i.next();) {
    if (*(i()) == 0x20) // this test is compatible with unicode format
      numConsecutiveSpaces++;
    else
      numConsecutiveSpaces = 0;

    if (numConsecutiveSpaces > 1) {
      if (tmpText.len() > 0) {
        m_documentInterface->insertText(tmpText);
        tmpText.clear();
      }
      m_documentInterface->insertSpace();
    } else
      tmpText.append(i());
  }
  m_documentInterface->insertText(tmpText);
  m_parseState->m_textBuffer.clear();
}

///////////////////
//
// font/character/
//
///////////////////
void IMWAWContentListener::setTextAttribute(const uint32_t attribute)
{
  if (attribute == m_ps->m_textAttributeBits) return;
  _closeSpan();

  m_ps->m_textAttributeBits = attribute;
}

void IMWAWContentListener::setTextFont(const WPXString &fontName)
{
  if (fontName == *(m_ps->m_fontName)) return;
  _closeSpan();
  // FIXME verify that fontName does not contain bad characters,
  //       if so, pass a unicode string
  *m_ps->m_fontName = fontName;
}

void IMWAWContentListener::setFontColor(int const (&col) [3])
{
  if (m_ps->m_fontColor->m_r == col[0] &&
      m_ps->m_fontColor->m_g == col[1] &&
      m_ps->m_fontColor->m_b == col[2]) return;
  _closeSpan();
  m_ps->m_fontColor->m_r = col[0];
  m_ps->m_fontColor->m_g = col[1];
  m_ps->m_fontColor->m_b = col[2];
}

void IMWAWContentListener::setFontColor(Vec3i const &col)
{
  if (m_ps->m_fontColor->m_r == col[0] &&
      m_ps->m_fontColor->m_g == col[1] &&
      m_ps->m_fontColor->m_b == col[2]) return;
  _closeSpan();
  m_ps->m_fontColor->m_r = col[0];
  m_ps->m_fontColor->m_g = col[1];
  m_ps->m_fontColor->m_b = col[2];
}

void IMWAWContentListener::setFontSize(const uint16_t fontSize)
{
  float fSize = fontSize;
  if (m_ps->m_fontSize==fSize) return;

  _closeSpan();
  m_ps->m_fontSize=fSize;
}

void IMWAWContentListener::setTextLanguage(std::string const &locale)
{
  if (!parsingState()) return;
  std::string lang;
  if (m_parseState->m_doc_language==locale &&
      m_parseState->m_parag_language=="")
    lang = "";
  else
    lang = locale;
  if (m_parseState->m_language!=lang) _closeSpan();
  m_parseState->m_language = lang;
}

///////////////////
//
// tabs/...
//
///////////////////
void IMWAWContentListener::setParagraphTextIndent(double margin)
{
  m_ps->m_listReferencePosition = margin;
}

void IMWAWContentListener::setParagraphMargin(double margin, int pos, WPXUnit unit)
{
  switch(unit) {
  case WPX_POINT:
    if (pos == DMWAW_LEFT || pos == DMWAW_RIGHT)
      margin/=72.;
    break;
  case WPX_TWIP:
    if (pos == DMWAW_LEFT || pos == DMWAW_RIGHT)
      margin/=1440.;
    break;
  case WPX_PERCENT:
    MWAW_DEBUG_MSG(("IMWAWContentListener::setParagraphMargin: unit can not be percent\n"));
    return;
  case WPX_GENERIC:
    MWAW_DEBUG_MSG(("IMWAWContentListener::setParagraphMargin: unit can not be generic\n"));
    return;
  default:
    break;
  }
  switch(pos) {
  case DMWAW_LEFT:
    m_ps->m_leftMarginByParagraphMarginChange = margin;
    m_ps->m_paragraphMarginLeft = m_ps->m_leftMarginByPageMarginChange
                                  + m_ps->m_leftMarginByParagraphMarginChange
                                  + m_ps->m_leftMarginByTabs;
    break;
  case DMWAW_RIGHT:
    m_ps->m_rightMarginByParagraphMarginChange = margin;
    m_ps->m_paragraphMarginRight = m_ps->m_rightMarginByPageMarginChange
                                   + m_ps->m_rightMarginByParagraphMarginChange
                                   + m_ps->m_rightMarginByTabs;
    break;
  case DMWAW_TOP:
    m_ps->m_paragraphMarginTop = margin;
    m_ps->m_paragraphMarginTopUnit = unit;
    break;
  case DMWAW_BOTTOM:
    m_ps->m_paragraphMarginBottom = margin;
    m_ps->m_paragraphMarginBottomUnit = unit;
    break;
  default:
    MWAW_DEBUG_MSG(("IMWAWContentListener::setParagraphMargin: unknown pos"));
  }
}

void IMWAWContentListener::setTabs(const std::vector<DMWAWTabStop> &tabStops, double maxW)
{
  if (isUndoOn()) return;

  m_ps->m_isTabPositionRelative = true;

  if (maxW <= 0) {
    m_ps->m_tabStops = tabStops;
    return;
  }

  std::vector<DMWAWTabStop> tabs = tabStops;
  for (int p = 0; p < int(tabs.size()); p++) {
    double pos = tabs[p].m_position;
    if (pos > maxW-10./72. && tabs[p].m_alignment != RIGHT)
      tabs[p].m_position = maxW-10./72.;
  }
  m_ps->m_tabStops = tabs;
}

void IMWAWContentListener::setParagraphBorder(int which, bool flag)
{
  if (!parsingState()) return;
  if (flag) parsingState()->m_border |= which;
  else parsingState()->m_border &= (~which);
}

void IMWAWContentListener::setParagraphBorder(bool flag)
{
  if (!parsingState()) return;
  parsingState()->m_border = flag ? 0xf : 0;
}

void IMWAWContentListener::_appendParagraphProperties(WPXPropertyList &propList, const bool isListElement)
{
  DMWAWContentListener::_appendParagraphProperties(propList, isListElement);
  if (!parsingState()) return;
  if (parsingState()->m_border) {
    int border = parsingState()->m_border;
    if (border == 0xF) {
      propList.insert("fo:border", "0.03cm solid #000000");
      return;
    }
    if (border & DMWAW_TABLE_CELL_LEFT_BORDER_OFF)
      propList.insert("fo:border-left", "0.03cm solid #000000");
    if (border & DMWAW_TABLE_CELL_RIGHT_BORDER_OFF)
      propList.insert("fo:border-right", "0.03cm solid #000000");
    if (border & DMWAW_TABLE_CELL_TOP_BORDER_OFF)
      propList.insert("fo:border-top", "0.03cm solid #000000");
    if (border & DMWAW_TABLE_CELL_BOTTOM_BORDER_OFF)
      propList.insert("fo:border-bottom", "0.03cm solid #000000");
  }
  if (parsingState()->m_language.length())
    propList.insert("fo:language", parsingState()->m_language.c_str());
  parsingState()->m_parag_language = parsingState()->m_language;
}

void IMWAWContentListener::_appendExtraSpanProperties(WPXPropertyList &propList)
{
  DMWAWContentListener::_appendExtraSpanProperties(propList);
  if (!parsingState()) return;
  if (parsingState()->m_language != parsingState()->m_parag_language)
    propList.insert("fo:language", parsingState()->m_language.c_str());
}

///////////////////
//
// field :
//
///////////////////
#include <time.h>
void IMWAWContentListener::insertField(IMWAWContentListener::FieldType type)
{
  switch(type) {
  case None:
    break;
  case PageNumber: {
    _flushText();
    _openSpan();
    WPXPropertyList propList;
    propList.insert("style:num-format", libmwaw_libwpd::_numberingTypeToString(ARABIC));
    m_documentInterface->insertField(WPXString("text:page-number"), propList);
    // Checkme WP6ContentListener.cpp finish by resetting the state:
    // _parseState->m_styleStateSequence.setCurrentState(m_parseState->m_styleStateSequence.getPreviousState());
    break;
  }
  case Database: {
    WPXString tmp("#DATAFIELD#");
    insertUnicodeString(tmp);
    break;
  }
  case Title: {
    WPXString tmp("#TITLE#");
    insertUnicodeString(tmp);
    break;
  }
  case Date: {
    time_t now = time ( 0L );
    struct tm timeinfo = *(localtime ( &now));
    char buf[256];
    strftime(buf, 256, "%m/%d/%y", &timeinfo);
    WPXString tmp(buf);
    insertUnicodeString(tmp);
    break;
  }
  case Time: {
    time_t now = time ( 0L );
    struct tm timeinfo = *(localtime ( &now));
    char buf[256];
    strftime(buf, 256, "%I:%M:%S %p", &timeinfo);
    WPXString tmp(buf);
    insertUnicodeString(tmp);
    break;
  }
  default:
    MWAW_DEBUG_MSG(("IMWAWContentListener::insertField: must not be called with type=%d\n", int(type)));
    break;
  }
}

///////////////////
//
// Frame/Text picture
//
///////////////////

void IMWAWContentListener::_handleFrameParameters
( WPXPropertyList &propList, TMWAWPosition const &pos)
{
  Vec2f origin = pos.origin();
  WPXUnit unit = pos.unit();
  float inchFactor=pos.getInvUnitScale(WPX_INCH);
  float pointFactor = pos.getInvUnitScale(WPX_POINT);

  propList.insert("svg:width", double(pos.size()[0]), unit);
  propList.insert("svg:height", double(pos.size()[1]), unit);
  if (pos.naturalSize().x() > 4*pointFactor && pos.naturalSize().y() > 4*pointFactor) {
    propList.insert("libwpd:naturalWidth", pos.naturalSize().x(), pos.unit());
    propList.insert("libwpd:naturalHeight", pos.naturalSize().y(), pos.unit());
  }

  double newPosition;

  if ( pos.m_wrapping ==  TMWAWPosition::WDynamic)
    propList.insert( "style:wrap", "dynamic" );
  else if ( pos.m_wrapping ==  TMWAWPosition::WRunThrough)
    propList.insert( "style:wrap", "run-through" );
  else
    propList.insert( "style:wrap", "none" );

  if (pos.m_anchorTo == TMWAWPosition::Paragraph) {
    propList.insert("text:anchor-type", "paragraph");
    propList.insert("style:vertical-rel", "paragraph" );
    propList.insert("style:horizontal-rel", "paragraph");
    double w = m_ps->m_pageFormWidth - m_ps->m_pageMarginLeft
               - m_ps->m_pageMarginRight - m_ps->m_sectionMarginLeft
               - m_ps->m_sectionMarginRight - m_ps->m_paragraphMarginLeft
               - m_ps->m_paragraphMarginRight;
    w *= inchFactor;
    switch ( pos.m_xPos) {
    case TMWAWPosition::XRight:
      if (origin[0] == 0.0) {
        propList.insert("style:horizontal-pos", "right");
        break;
      }
      propList.insert( "style:horizontal-pos", "from-left");
      propList.insert( "svg:x", double(origin[0] - pos.size()[0] + w), unit);
      break;
    case TMWAWPosition::XCenter:
      if (origin[0] == 0.0) {
        propList.insert("style:horizontal-pos", "center");
        break;
      }
      propList.insert( "style:horizontal-pos", "from-left");
      propList.insert( "svg:x", double(origin[0] - pos.size()[0]/2.0 + w/2.0), unit);
      break;
    case TMWAWPosition::XLeft:
    default:
      if (origin[0] == 0.0)
        propList.insert("style:horizontal-pos", "left");
      else {
        propList.insert( "style:horizontal-pos", "from-left");
        propList.insert( "svg:x", double(origin[0]), unit);
      }
      break;
    }

    if (origin[1] == 0.0)
      propList.insert( "style:vertical-pos", "top" );
    else {
      propList.insert( "style:vertical-pos", "from-top" );
      propList.insert( "svg:y", double(origin[1]), unit);
    }

    return;
  }
  if ( pos.m_anchorTo == TMWAWPosition::Page ) {
    // Page position seems to do not use the page margin...
    propList.insert("text:anchor-type", "page");
    if (pos.page() > 0) propList.insert("text:anchor-page-number", pos.page());
    double  w = m_ps->m_pageFormWidth;
    double h = m_ps->m_pageFormLength;
    w *= inchFactor;
    h *= inchFactor;

    propList.insert("style:vertical-rel", "page" );
    propList.insert("style:horizontal-rel", "page" );
    switch ( pos.m_yPos) {
    case TMWAWPosition::YFull:
      propList.insert("svg:height", double(h), unit);
    case TMWAWPosition::YTop:
      if ( origin[1] == 0.0) {
        propList.insert("style:vertical-pos", "top" );
        break;
      }
      propList.insert("style:vertical-pos", "from-top" );
      newPosition = origin[1];
      if (newPosition > h -pos.size()[1])
        newPosition = h - pos.size()[1];
      propList.insert("svg:y", double(newPosition), unit);
      break;
    case TMWAWPosition::YCenter:
      if (origin[1] == 0.0) {
        propList.insert("style:vertical-pos", "middle" );
        break;
      }
      propList.insert("style:vertical-pos", "from-top" );
      newPosition = (h - pos.size()[1])/2.0;
      if (newPosition > h -pos.size()[1]) newPosition = h - pos.size()[1];
      propList.insert("svg:y", double(newPosition), unit);
      break;
    case TMWAWPosition::YBottom:
      if (origin[1] == 0.0) {
        propList.insert("style:vertical-pos", "bottom" );
        break;
      }
      propList.insert("style:vertical-pos", "from-top" );
      newPosition = h - pos.size()[1]-origin[1];
      if (newPosition > h -pos.size()[1]) newPosition = h -pos.size()[1];
      else if (newPosition < 0) newPosition = 0;
      propList.insert("svg:y", double(newPosition), unit);
      break;
    }

    switch ( pos.m_xPos ) {
    case TMWAWPosition::XFull:
      propList.insert("svg:width", double(w), unit);
    case TMWAWPosition::XLeft:
      if ( origin[0] == 0.0 )
        propList.insert( "style:horizontal-pos", "left");
      else {
        propList.insert( "style:horizontal-pos", "from-left");
        propList.insert( "svg:x", double(origin[0]), unit);
      }
      break;
    case TMWAWPosition::XRight:
      if ( origin[0] == 0.0 )
        propList.insert( "style:horizontal-pos", "right");
      else {
        propList.insert( "style:horizontal-pos", "from-left");
        propList.insert( "svg:x",double( w - pos.size()[0] + origin[0]), unit);
      }
      break;
    case TMWAWPosition::XCenter:
      if ( origin[0] == 0.0 )
        propList.insert( "style:horizontal-pos", "center" );
      else {
        propList.insert( "style:horizontal-pos", "from-left");
        propList.insert( "svg:x", double((w - pos.size()[0])/2. + origin[0]), unit);
      }
      break;
    }
    return;
  }
  if ( pos.m_anchorTo != TMWAWPosition::Char &&
       pos.m_anchorTo != TMWAWPosition::CharBaseLine) return;

  propList.insert("text:anchor-type", "as-char");
  if ( pos.m_anchorTo == TMWAWPosition::CharBaseLine)
    propList.insert( "style:vertical-rel", "baseline" );
  else
    propList.insert( "style:vertical-rel", "line" );
  switch ( pos.m_yPos ) {
  case TMWAWPosition::YFull:
  case TMWAWPosition::YTop:
    if ( origin[1] == 0.0 )
      propList.insert( "style:vertical-pos", "top" );
    else {
      propList.insert( "style:vertical-pos", "from-top" );
      propList.insert( "svg:y", double(origin[1]), unit);
    }
    break;
  case TMWAWPosition::YCenter:
    if ( origin[1] == 0.0 )
      propList.insert( "style:vertical-pos", "middle" );
    else {
      propList.insert( "style:vertical-pos", "from-top" );
      propList.insert( "svg:y", double(origin[1] - pos.size()[1]/2.0), unit);
    }
    break;
  case TMWAWPosition::YBottom:
  default:
    if ( origin[1] == 0.0 )
      propList.insert( "style:vertical-pos", "bottom" );
    else {
      propList.insert( "style:vertical-pos", "from-top" );
      propList.insert( "svg:y", double(origin[1] - pos.size()[1]), unit);
    }
    break;
  }
}

bool IMWAWContentListener::openFrame(TMWAWPosition const &pos)
{
  if (isUndoOn()) return true;
  if (m_ps->m_isTableOpened && !m_ps->m_isTableCellOpened) {
    MWAW_DEBUG_MSG(("IMWAWContentListener::openFrame: called in table but cell is not opened\n"));
    return false;
  }
  if (m_parseState->m_isFrameOpened) {
    MWAW_DEBUG_MSG(("IMWAWContentListener::openFrame: called but a frame is already opened\n"));
    return false;
  }

  switch(pos.m_anchorTo) {
  case TMWAWPosition::Page:
    if (!m_ps->m_inSubDocument && !m_ps->m_isSectionOpened)
      openSection();
    break;
  case TMWAWPosition::Paragraph:
    if (m_ps->m_isParagraphOpened)
      _flushText();
    else
      _openParagraph();
    break;
  case TMWAWPosition::CharBaseLine:
  case TMWAWPosition::Char:
    if (m_ps->m_isSpanOpened)
      _flushText();
    else
      _openSpan();
    break;
  default:
    MWAW_DEBUG_MSG(("IMWAWContentListener::openFrame: can not determine the anchor\n"));
    return false;
  }

  WPXPropertyList propList;
  _handleFrameParameters(propList, pos);
  m_documentInterface->openFrame(propList);

  m_parseState->m_isFrameOpened = true;
  return true;
}

void IMWAWContentListener::closeFrame()
{
  if (isUndoOn()) return;
  if (!m_parseState->m_isFrameOpened) {
    MWAW_DEBUG_MSG(("IMWAWContentListener::closeFrame: called but no frame is already opened\n"));
    return;
  }
  m_documentInterface->closeFrame();
  m_parseState->m_isFrameOpened = false;
}

void IMWAWContentListener::insertTextBox(TMWAWPosition const &pos,
    IMWAWSubDocumentPtr &subDocument)
{
  if (isUndoOn()) return;

  if (!openFrame(pos)) return;

  WPXPropertyList propList;
  m_documentInterface->openTextBox(propList);

  if (subDocument.get()) {
    DMWAWTableList tableList;
    m_listDocuments.push_back(subDocument);
    handleSubDocument(subDocument.get(), DMWAW_SUBDOCUMENT_TEXT_BOX, tableList, 0);
  }
  m_documentInterface->closeTextBox();

  closeFrame();
}

void IMWAWContentListener::insertPicture
(TMWAWPosition const &pos, const WPXBinaryData &binaryData, std::string type)
{
  if (isUndoOn()) return;

  if (!openFrame(pos)) return;

  WPXPropertyList propList;
  propList.insert("libwpd:mimetype", type.c_str());
  m_documentInterface->insertBinaryObject(propList, binaryData);

  closeFrame();
}

///////////////////
//
// Note
//
///////////////////
void IMWAWContentListener::insertNote(const DMWAWNoteType noteType, IMWAWSubDocumentPtr &subDocument)
{
  if (isUndoOn()) return;

  if (m_ps->m_isNote) {
    MWAW_DEBUG_MSG(("IMWAWContentListener::insertNote try to insert a note recursively (ingnored)"));
    return;
  }

  if (!m_ps->m_isParagraphOpened)
    _openParagraph();
  else {
    _flushText();
    _closeSpan();
  }

  m_ps->m_isNote = true;

  WPXPropertyList propList;

  if (noteType == FOOTNOTE) {
    propList.insert("libwpd:number", ++(m_parseState->m_footNoteNumber));
    m_documentInterface->openFootnote(propList);
  } else {
    propList.insert("libwpd:number", ++(m_parseState->m_endNoteNumber));
    m_documentInterface->openEndnote(propList);
  }

  DMWAWTableList tableList;
  if (subDocument.get()) m_listDocuments.push_back(subDocument);

  handleSubDocument(subDocument.get(), DMWAW_SUBDOCUMENT_NOTE, tableList, 0);

  if (noteType == FOOTNOTE)
    m_documentInterface->closeFootnote();
  else
    m_documentInterface->closeEndnote();
  m_ps->m_isNote = false;
}

void IMWAWContentListener::insertComment(IMWAWSubDocumentPtr &subDocument)
{
  if (isUndoOn()) return;

  if (m_ps->m_isNote) {
    MWAW_DEBUG_MSG(("IMWAWContentListener::insertComment try to insert a note recursively (ingnored)"));
    return;
  }

  if (!m_ps->m_isParagraphOpened)
    _openParagraph();
  else {
    _flushText();
    _closeSpan();
  }

  WPXPropertyList propList;
  m_documentInterface->openComment(propList);

  m_ps->m_isNote = true;

  DMWAWTableList tableList;
  if (subDocument.get()) m_listDocuments.push_back(subDocument);
  handleSubDocument(subDocument.get(), DMWAW_SUBDOCUMENT_COMMENT_ANNOTATION, tableList, 0);


  m_documentInterface->closeComment();
  m_ps->m_isNote = false;
}

///////////////////
//
// document handle
//
///////////////////

void IMWAWContentListener::_handleSubDocument
(const DMWAWSubDocument *subDocument,
 DMWAWSubDocumentType subDocumentType,
 DMWAWTableList /*tableList*/, int /*nextTableIndice*/)
{
  if (isUndoOn()) return;

  if (!subDocument) return;

  IMWAWSubDocument const *doc =
    dynamic_cast<IMWAWSubDocument const *>(subDocument);
  if (!doc) {
    MWAW_DEBUG_MSG(("Works: IMWAWContentListener::_handleSubDocument unknown type of document\n"));
    return;
  }
  int numSubDocument = m_subDocuments.size();
  for (int i = 0; i < numSubDocument; i++) {
    if (*doc != *(m_subDocuments[i])) continue;
    MWAW_DEBUG_MSG(("Works: IMWAWContentListener::_handleSubDocument recursive call\n"));
    return;
  }

  // save our old parsing state on our "stack"
  ParsingState *oldParseState = _pushParsingState();
  m_subDocuments.push_back(doc);
  IMWAWContentListenerPtr listen(this, MWAW_shared_ptr_noop_deleter<IMWAWContentListener>());
  // do not loose oldParseState and try to continue
  try {
    const_cast<IMWAWSubDocument *>(doc)->parse(listen, subDocumentType);
  } catch(...) {
    MWAW_DEBUG_MSG(("Works: IMWAWContentListener::_handleSubDocument exception catched \n"));
  }
  m_subDocuments.resize(numSubDocument);

  // Close the sub-document properly
  if (m_ps->m_isTableOpened)
    _closeTable();
  if (m_ps->m_isParagraphOpened)
    _closeParagraph();
  if (m_ps->m_isListElementOpened)
    _closeListElement();

  m_ps->m_currentListLevel = 0;
  _changeList();

  _popParsingState(oldParseState);
}

///////////////////
//
// Minimal implementation
//
///////////////////
void IMWAWContentListener::setCurrentListLevel(int level)
{
  m_ps->m_currentListLevel = level;
  // to be compatible with DMWAWContentListerner
  if (level)
    m_ps->m_listBeginPosition =
      m_ps->m_paragraphMarginLeft + m_ps->m_paragraphTextIndent;
  else
    m_ps->m_listBeginPosition = 0;
}
void IMWAWContentListener::setCurrentList(shared_ptr<IMWAWList> list)
{
  m_parseState->m_list=list;
  if (list && list->getId() <= 0)
    list->setId(++m_actualListId);
}
shared_ptr<IMWAWList> IMWAWContentListener::getCurrentList() const
{
  return m_parseState->m_list;
}


void IMWAWContentListener::_changeList()
{
  if (m_ps->m_isParagraphOpened)
    _closeParagraph();
  if (m_ps->m_isListElementOpened)
    _closeListElement();
  _handleListChange();
}


// basic model of unordered list
void IMWAWContentListener::_handleListChange()
{
  if (!m_ps->m_isSectionOpened && !m_ps->m_inSubDocument && !m_ps->m_isTableOpened)
    _openSection();

  // FIXME: even if nobody really care, if we close an ordered or an unordered
  //      elements, we must keep the previous to close this part...
  int actualListLevel = m_parseState->m_listOrderedLevels.size();
  for (int i=actualListLevel; i > m_ps->m_currentListLevel; i--) {
    if (m_parseState->m_listOrderedLevels[i-1])
      m_documentInterface->closeOrderedListLevel();
    else
      m_documentInterface->closeUnorderedListLevel();
  }

  WPXPropertyList propList2;
  if (m_ps->m_currentListLevel) {
    if (!m_parseState->m_list.get()) {
      MWAW_DEBUG_MSG(("IMWAWContentListener::_handleListChange: can not find any list\n"));
      return;
    }
    m_parseState->m_list->setLevel(m_ps->m_currentListLevel);
    m_parseState->m_list->openElement();

    bool mustResend =  m_parseState->m_list->mustSendLevel
                       (m_ps->m_currentListLevel,
                        m_ps->m_paragraphMarginLeft + m_ps->m_paragraphTextIndent, m_ps->m_listBeginPosition);

    if (mustResend) {
      if (actualListLevel == m_ps->m_currentListLevel) {
        if (m_parseState->m_listOrderedLevels[actualListLevel-1])
          m_documentInterface->closeOrderedListLevel();
        else
          m_documentInterface->closeUnorderedListLevel();
        actualListLevel--;
      }
      if (m_ps->m_currentListLevel==1) {
        // we must change the listID for writerperfect
        int prevId;
        if ((prevId=m_parseState->m_list->getPreviousId()) > 0)
          m_parseState->m_list->setId(prevId);
        else
          m_parseState->m_list->setId(++m_actualListId);
      }
      m_parseState->m_list->sendTo(*m_documentInterface, m_ps->m_currentListLevel,
                                   m_ps->m_paragraphMarginLeft + m_ps->m_paragraphTextIndent,
                                   m_ps->m_listBeginPosition);
    }

    propList2.insert("libwpd:id", m_parseState->m_list->getId());
    m_parseState->m_list->closeElement();
  }

  if (actualListLevel == m_ps->m_currentListLevel) return;

  m_parseState->m_listOrderedLevels.resize(m_ps->m_currentListLevel, false);
  for (int i=actualListLevel+1; i<= m_ps->m_currentListLevel; i++) {
    if (m_parseState->m_list->isNumeric(i)) {
      m_parseState->m_listOrderedLevels[i-1] = true;
      m_documentInterface->openOrderedListLevel(propList2);
    } else {
      m_parseState->m_listOrderedLevels[i-1] = false;
      m_documentInterface->openUnorderedListLevel(propList2);
    }
  }
}

////////////////////////////////////////////////////////////
// Table gestion
////////////////////////////////////////////////////////////

void IMWAWContentListener::openTable(std::vector<float> const &colWidth, WPXUnit unit)
{
  if (m_ps->m_isTableOpened) {
    MWAW_DEBUG_MSG(("IMWAWContentListener::openTable: called with m_isTableOpened=true\n"));
    return;
  }

  if (m_ps->m_isParagraphOpened)
    _closeParagraph();

  WPXPropertyList propList;
  propList.insert("table:align", "left");
  propList.insert("fo:margin-left", 0.0);

  float tableWidth = 0;
  WPXPropertyListVector columns;

  int nCols = colWidth.size();
  for (int c = 0; c < nCols; c++) {
    WPXPropertyList column;
    column.insert("style:column-width", colWidth[c], unit);
    columns.append(column);

    tableWidth += colWidth[c];
  }
  propList.insert("style:width", tableWidth, unit);
  m_documentInterface->openTable(propList, columns);
  m_ps->m_isTableOpened = true;

}

void IMWAWContentListener::openTable(std::vector<int> const &colWidth, WPXUnit unit)
{
  if (m_ps->m_isTableOpened) {
    MWAW_DEBUG_MSG(("IMWAWContentListener::openTable: called with m_isTableOpened=true\n"));
    return;
  }

  WPXPropertyList propList;
  propList.insert("table:align", "left");
  propList.insert("fo:margin-left", 0.0);

  long tableWidth = 0;
  WPXPropertyListVector columns;

  int nCols = colWidth.size();
  for (int c = 0; c < nCols; c++) {
    WPXPropertyList column;
    column.insert("style:column-width", colWidth[c], unit);
    columns.append(column);

    tableWidth += colWidth[c];
  }
  propList.insert("style:width", tableWidth, unit);
  m_documentInterface->openTable(propList, columns);
  m_ps->m_isTableOpened = true;

}

void IMWAWContentListener::closeTable()
{
  if (!m_ps->m_isTableOpened) {
    MWAW_DEBUG_MSG(("IMWAWContentListener::closeTable: called with m_isTableOpened=false\n"));
    return;
  }
// close the table
  m_ps->m_isTableOpened = false;
  m_documentInterface->closeTable();
}
////////////////////////////////////////////////////////////
// Row gestion
////////////////////////////////////////////////////////////
void IMWAWContentListener::openTableRow(float h, WPXUnit unit, bool headerRow)
{
  if (m_ps->m_isTableRowOpened) {
    MWAW_DEBUG_MSG(("IMWAWContentListener::openTableRow: called with m_isTableRowOpened=true\n"));
    return;
  }
  if (!m_ps->m_isTableOpened) {
    MWAW_DEBUG_MSG(("IMWAWContentListener::openTableRow: called with m_isTableOpened=false\n"));
    return;
  }
  WPXPropertyList propList;
  propList.insert("libwpd:is-header-row", headerRow);

  propList.insert("style:row-height", h, unit);
  m_documentInterface->openTableRow(propList);
  m_ps->m_isTableRowOpened = true;
}

void IMWAWContentListener::closeTableRow()
{
  if (!m_ps->m_isTableRowOpened) {
    MWAW_DEBUG_MSG(("IMWAWContentListener::openTableRow: called with m_isTableRowOpened=false\n"));
    return;
  }
  m_ps->m_isTableRowOpened = false;
  m_documentInterface->closeTableRow();
}

////////////////////////////////////////////////////////////
// Cell gestion
////////////////////////////////////////////////////////////
void IMWAWContentListener::openTableCell(IMWAWCell const &cell, WPXPropertyList const &extras)
{
  if (!m_ps->m_isTableRowOpened) {
    MWAW_DEBUG_MSG(("IMWAWContentListener::openTableCell: called with m_isTableRowOpened=false\n"));
    return;
  }
  if (m_ps->m_isTableCellOpened) {
    MWAW_DEBUG_MSG(("IMWAWContentListener::openTableCell: called with m_isTableCellOpened=true\n"));
    closeTableCell();
  }

  WPXPropertyList propList(extras);

  if (extras["office:value-type"]) {
    std::stringstream f;
    switch (cell.format()) {
    case IMWAWCell::F_NUMBER:
      if (strcmp(extras["office:value-type"]->getStr().cstr(), "float") == 0
          && cell.subformat()) {
        f << "Numeric" << cell.subformat();
        propList.insert("style:data-style-name", f.str().c_str());
        switch (cell.subformat()) {
        case 3:
        case 6:
          propList.insert("office:value-type", "percentage");
          break;
        case 4:
        case 7:
          propList.insert("office:value-type", "currency");
          propList.insert("office:currency","USD"); // fixme set dollars
          break;
        default:
          break;
        }
      }
      break;
    case IMWAWCell::F_DATE:
      if (strcmp(extras["office:value-type"]->getStr().cstr(), "date") == 0) {
        f << "Date" << cell.subformat();
        propList.insert("style:data-style-name", f.str().c_str());
      }
      break;
    case IMWAWCell::F_TIME:
      if (strcmp(extras["office:value-type"]->getStr().cstr(), "time") == 0) {
        f << "Time" << cell.subformat();
        propList.insert("style:data-style-name", f.str().c_str());
      }
      break;
    default:
      break;
    }
  }

  propList.insert("libwpd:column", cell.position()[0]);
  propList.insert("libwpd:row", cell.position()[1]);

  propList.insert("table:number-columns-spanned", cell.numSpannedCells()[0]);
  propList.insert("table:number-rows-spanned", cell.numSpannedCells()[1]);

  int v = 1;
  static char const*bString[] = { "left", "right", "top", "bottom" };
  int border = cell.borders();
  for (int c = 0; c < 4; c++) {
    std::stringstream g;
    g << "fo:border-" << bString[c];
    if (border & v)
      propList.insert(g.str().c_str(), "0.01in solid #000000");
    v <<= 1;
  }
  if (cell.isProtected())
    propList.insert("style:cell-protect","protected");
  // alignement
  switch(cell.hAlignement()) {
  case IMWAWCell::HALIGN_LEFT:
    propList.insert("fo:text-align", "first");
    propList.insert("style:text-align-source", "fix");
    break;
  case IMWAWCell::HALIGN_CENTER:
    propList.insert("fo:text-align", "center");
    propList.insert("style:text-align-source", "fix");
    break;
  case IMWAWCell::HALIGN_RIGHT:
    propList.insert("fo:text-align", "end");
    propList.insert("style:text-align-source", "fix");
    break;
  case IMWAWCell::HALIGN_DEFAULT:
    break; // default
  default:
    MWAW_DEBUG_MSG(("IMWAWContentListener::openTableCell: called with unknown align=%d\n", cell.hAlignement()));
  }

  m_ps->m_isTableCellOpened = true;
  m_documentInterface->openTableCell(propList);
}

void IMWAWContentListener::closeTableCell()
{
  if (!m_ps->m_isTableCellOpened) {
    MWAW_DEBUG_MSG(("IMWAWContentListener::closeTableCell: called with m_isTableCellOpened=false\n"));
    return;
  }
  _closeParagraph();

  m_ps->m_isTableCellOpened = false;
  m_documentInterface->closeTableCell();
}


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
