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
#include <cstring>
#include <iomanip>
#include <sstream>

#include <libwpd/WPXDocumentInterface.h>
#include <libwpd/WPXProperty.h>

#include "libmwaw_internal.hxx"

#include "MWAWCell.hxx"
#include "MWAWFont.hxx"
#include "MWAWList.hxx"
#include "MWAWPageSpan.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWContentListener.hxx"

MWAWDocumentParsingState::MWAWDocumentParsingState(std::vector<MWAWPageSpan> const &pageList) :
  m_pageList(pageList),
  m_metaData(),
  m_footNoteNumber(0), m_endNoteNumber(0), m_newListId(0),
  m_isDocumentStarted(false), m_isHeaderFooterStarted(false), m_subDocuments()
{
}

MWAWDocumentParsingState::~MWAWDocumentParsingState()
{
}

MWAWContentParsingState::MWAWContentParsingState() :
  m_textBuffer(""), m_numDeferredTabs(0),

  m_textAttributeBits(0),	m_fontSize(12.0), m_fontName("Times New Roman"),
  m_fontColor(0), m_textLanguage("UNSET"),

  m_isParagraphColumnBreak(false), m_isParagraphPageBreak(false),

  m_paragraphJustification(libmwaw::JustificationLeft),
  m_paragraphLineSpacing(1.0), m_paragraphLineSpacingUnit(WPX_PERCENT),
  m_paragraphBorders(),

  m_list(), m_currentListLevel(0),

  m_isPageSpanOpened(false), m_isSectionOpened(false), m_isFrameOpened(false),
  m_isPageSpanBreakDeferred(false),
  m_isHeaderFooterWithoutParagraph(false),

  m_isSpanOpened(false), m_isParagraphOpened(false), m_isListElementOpened(false),

  m_firstParagraphInPageSpan(true),

  m_numRowsToSkip(),
  m_isTableOpened(false), m_isTableRowOpened(false), m_isTableColumnOpened(false),
  m_isTableCellOpened(false),

  m_currentPage(0), m_numPagesRemainingInSpan(0), m_currentPageNumber(1),

  m_sectionAttributesChanged(false),
  m_numColumns(1),
  m_textColumns(),
  m_isTextColumnWithoutParagraph(false),

  m_pageFormLength(11.0),
  m_pageFormWidth(8.5f),
  m_pageFormOrientationIsPortrait(true),

  m_pageMarginLeft(1.0),
  m_pageMarginRight(1.0),
  m_pageMarginTop(1.0),
  m_pageMarginBottom(1.0),

  m_sectionMarginLeft(0.0),
  m_sectionMarginRight(0.0),
  m_sectionMarginTop(0.0),
  m_sectionMarginBottom(0.0),

  m_paragraphMarginLeft(0.0),
  m_paragraphMarginRight(0.0),
  m_paragraphMarginTop(0.0), m_paragraphMarginTopUnit(WPX_INCH),
  m_paragraphMarginBottom(0.0), m_paragraphMarginBottomUnit(WPX_INCH),

  m_leftMarginByPageMarginChange(0.0),
  m_rightMarginByPageMarginChange(0.0),
  m_leftMarginByParagraphMarginChange(0.0),
  m_rightMarginByParagraphMarginChange(0.0),
  m_leftMarginByTabs(0.0),
  m_rightMarginByTabs(0.0),

  m_paragraphTextIndent(0.0),
  m_textIndentByParagraphIndentChange(0.0),
  m_textIndentByTabs(0.0),

  m_listReferencePosition(0.0), m_listBeginPosition(0.0), m_listOrderedLevels(),

  m_alignmentCharacter('.'),
  m_tabStops(),
  m_isTabPositionRelative(false),

  m_inSubDocument(false),
  m_isNote(false),
  m_subDocumentType(libmwaw::DOC_NONE)
{
}

MWAWContentParsingState::~MWAWContentParsingState()
{
}

bool MWAWContentParsingState::hasParagraphBorders() const
{
  for (size_t i = 0; i < m_paragraphBorders.size(); i++) {
    if (m_paragraphBorders[i].m_style != MWAWBorder::None)
      return true;
  }
  return false;
}

bool MWAWContentParsingState::hasParagraphDifferentBorders() const
{
  if (!hasParagraphBorders()) return false;
  if (m_paragraphBorders.size() < 4) return true;
  for (size_t i = 1; i < m_paragraphBorders.size(); i++) {
    if (m_paragraphBorders[i] != m_paragraphBorders[0])
      return true;
  }
  return false;
}

MWAWContentListener::MWAWContentListener(std::vector<MWAWPageSpan> const &pageList, WPXDocumentInterface *documentInterface) :
  m_ds(new MWAWDocumentParsingState(pageList)), m_ps(new MWAWContentParsingState), m_psStack(),
  m_documentInterface(documentInterface)
{
  _updatePageSpanDependent(true);
  _recomputeParagraphPositions();
}

MWAWContentListener::~MWAWContentListener()
{
}


///////////////////
// text data
///////////////////
void MWAWContentListener::insertCharacter(uint8_t character)
{
  if (character >= 0x80) {
    insertUnicode(character);
    return;
  }
  _flushDeferredTabs ();
  if (!m_ps->m_isSpanOpened) _openSpan();
  m_ps->m_textBuffer.append((char) character);
}

void MWAWContentListener::insertUnicode(uint32_t val)
{
  // undef character, we skip it
  if (val == 0xfffd) return;

  _flushDeferredTabs ();
  if (!m_ps->m_isSpanOpened) _openSpan();
  appendUnicode(val, m_ps->m_textBuffer);
}

void MWAWContentListener::insertUnicodeString(WPXString const &str)
{
  _flushDeferredTabs ();
  if (!m_ps->m_isSpanOpened) _openSpan();
  m_ps->m_textBuffer.append(str);
}

void MWAWContentListener::appendUnicode(uint32_t val, WPXString &buffer)
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
    outbuf[i] = uint8_t((val & 0x3f) | 0x80);
    val >>= 6;
  }
  outbuf[0] = uint8_t(val | first);
  for (i = 0; i < len; i++) buffer.append((char)outbuf[i]);
}

void MWAWContentListener::insertEOL(bool soft)
{
  if (!m_ps->m_isParagraphOpened && !m_ps->m_isListElementOpened)
    _openSpan();
  _flushDeferredTabs();

  if (soft) {
    if (m_ps->m_isSpanOpened)
      _flushText();
    m_documentInterface->insertLineBreak();
  } else if (m_ps->m_isParagraphOpened)
    _closeParagraph();

  // sub/superscript must not survive a new line
  static const uint32_t s_subsuperBits =
    MWAW_SUBSCRIPT_BIT | MWAW_SUPERSCRIPT_BIT |
    MWAW_SUBSCRIPT100_BIT | MWAW_SUPERSCRIPT100_BIT;
  if (m_ps->m_textAttributeBits & s_subsuperBits)
    m_ps->m_textAttributeBits &= ~s_subsuperBits;
}

void MWAWContentListener::insertTab()
{
  if (!m_ps->m_isParagraphOpened) {
    m_ps->m_numDeferredTabs++;
    return;
  }
  if (m_ps->m_isSpanOpened) _flushText();
  m_ps->m_numDeferredTabs++;
  _flushDeferredTabs();
}

void MWAWContentListener::insertBreak(const uint8_t breakType)
{
  switch (breakType) {
  case MWAW_COLUMN_BREAK:
    if (!m_ps->m_isPageSpanOpened && !m_ps->m_inSubDocument)
      _openSpan();
    if (m_ps->m_isParagraphOpened)
      _closeParagraph();
    m_ps->m_isParagraphColumnBreak = true;
    m_ps->m_isTextColumnWithoutParagraph = true;
    break;
  case MWAW_PAGE_BREAK:
    if (!m_ps->m_isPageSpanOpened && !m_ps->m_inSubDocument)
      _openSpan();
    if (m_ps->m_isParagraphOpened)
      _closeParagraph();
    m_ps->m_isParagraphPageBreak = true;
    break;
    // TODO: (.. line break?)
  default:
    break;
  }

  if (m_ps->m_inSubDocument)
    return;

  switch (breakType) {
  case MWAW_PAGE_BREAK:
  case MWAW_SOFT_PAGE_BREAK:
    if (m_ps->m_numPagesRemainingInSpan > 0)
      m_ps->m_numPagesRemainingInSpan--;
    else {
      if (!m_ps->m_isTableOpened && !m_ps->m_isParagraphOpened && !m_ps->m_isListElementOpened)
        _closePageSpan();
      else
        m_ps->m_isPageSpanBreakDeferred = true;
    }
    m_ps->m_currentPageNumber++;
    break;
  default:
    break;
  }
}

void MWAWContentListener::_insertBreakIfNecessary(WPXPropertyList &propList)
{
  if (m_ps->m_isParagraphPageBreak && !m_ps->m_inSubDocument) {
    // no hard page-breaks in subdocuments
    propList.insert("fo:break-before", "page");
    m_ps->m_isParagraphPageBreak = false;
  } else if (m_ps->m_isParagraphColumnBreak) {
    if (m_ps->m_numColumns > 1)
      propList.insert("fo:break-before", "column");
    else
      propList.insert("fo:break-before", "page");
  }
}

///////////////////
// font/character format
///////////////////
void MWAWContentListener::setFontAttributes(const uint32_t attribute)
{
  if (attribute == m_ps->m_textAttributeBits) return;
  _closeSpan();

  m_ps->m_textAttributeBits = attribute;
}

void MWAWContentListener::setTextFont(const WPXString &fontName)
{
  if (fontName == m_ps->m_fontName) return;
  _closeSpan();
  // FIXME verify that fontName does not contain bad characters,
  //       if so, pass a unicode string
  m_ps->m_fontName = fontName;
}

void MWAWContentListener::setFontSize(const uint16_t fontSize)
{
  float fSize = fontSize;
  if (m_ps->m_fontSize >= fSize && m_ps->m_fontSize <= fSize) return;

  _closeSpan();
  m_ps->m_fontSize=fSize;
}

void MWAWContentListener::setTextColor(const uint32_t rgb)
{
  if (m_ps->m_fontColor==rgb) return;
  _closeSpan();
  m_ps->m_fontColor = rgb;
}

void MWAWContentListener::setTextLanguage(std::string const &locale)
{
  if (m_ps->m_textLanguage==locale) return;
  _closeSpan();
  m_ps->m_textLanguage=locale;
}

///////////////////
// paragraph tabs, tabs/...
///////////////////
bool MWAWContentListener::isParagraphOpened() const
{
  return m_ps->m_isParagraphOpened;
}

void MWAWContentListener::setParagraphLineSpacing(const double lineSpacing, WPXUnit unit)
{
  m_ps->m_paragraphLineSpacing = lineSpacing;
  m_ps->m_paragraphLineSpacingUnit = unit;
}

void MWAWContentListener::setParagraphJustification(libmwaw::Justification justification, bool force)
{
  if (justification == m_ps->m_paragraphJustification) return;

  if (force) {
    if (m_ps->m_isParagraphOpened)
      _closeParagraph();

    m_ps->m_currentListLevel = 0;
  }
  m_ps->m_paragraphJustification = justification;
}

void MWAWContentListener::setParagraphTextIndent(double margin, WPXUnit unit)
{
  float scale=MWAWPosition::getScaleFactor(unit, WPX_INCH);
  m_ps->m_textIndentByParagraphIndentChange = scale*margin;
  _recomputeParagraphPositions();
}

void MWAWContentListener::setParagraphMargin(double margin, int pos, WPXUnit unit)
{
  margin*=MWAWPosition::getScaleFactor(unit, WPX_INCH);
  switch(pos) {
  case MWAW_LEFT:
    m_ps->m_leftMarginByParagraphMarginChange = margin;
    _recomputeParagraphPositions();
    break;
  case MWAW_RIGHT:
    m_ps->m_rightMarginByParagraphMarginChange = margin;
    _recomputeParagraphPositions();
    break;
  case MWAW_TOP:
    m_ps->m_paragraphMarginTop = margin;
    break;
  case MWAW_BOTTOM:
    m_ps->m_paragraphMarginBottom = margin;
    break;
  default:
    MWAW_DEBUG_MSG(("MWAWContentListener::setParagraphMargin: unknown pos"));
  }
}

void MWAWContentListener::setTabs(const std::vector<MWAWTabStop> &tabStops)
{
  m_ps->m_isTabPositionRelative = true;
  m_ps->m_tabStops = tabStops;
}

void MWAWContentListener::resetParagraphBorders()
{
  m_ps->m_paragraphBorders.resize(0);
}

void MWAWContentListener::setParagraphBorder(int wh, MWAWBorder const &border)
{
  int const allBits = MWAWBorder::LeftBit|MWAWBorder::RightBit|MWAWBorder::TopBit|MWAWBorder::BottomBit|MWAWBorder::HMiddleBit|MWAWBorder::VMiddleBit;
  if (wh & (~allBits)) {
    MWAW_DEBUG_MSG(("MWAWContentListener::setParagraphBorder: unknown borders\n"));
    return;
  }
  if (m_ps->m_paragraphBorders.size() < 4) {
    MWAWBorder emptyBorder;
    emptyBorder.m_style = MWAWBorder::None;
    m_ps->m_paragraphBorders.resize(4, emptyBorder);
  }
  if (wh & MWAWBorder::LeftBit) m_ps->m_paragraphBorders[MWAWBorder::Left] = border;
  if (wh & MWAWBorder::RightBit) m_ps->m_paragraphBorders[MWAWBorder::Right] = border;
  if (wh & MWAWBorder::TopBit) m_ps->m_paragraphBorders[MWAWBorder::Top] = border;
  if (wh & MWAWBorder::BottomBit) m_ps->m_paragraphBorders[MWAWBorder::Bottom] = border;
  static bool first = true;
  if ((wh & (MWAWBorder::HMiddleBit|MWAWBorder::VMiddleBit)) && first) {
    first = false;
    MWAW_DEBUG_MSG(("MWAWContentListener::setParagraphBorder: set hmiddle or vmiddle is not implemented\n"));
  }
}

///////////////////
// List: Minimal implementation
///////////////////
void MWAWContentListener::setCurrentListLevel(int level)
{
  m_ps->m_currentListLevel = uint8_t(level);
  // to be compatible with MWAWContentListerner
  if (level)
    m_ps->m_listBeginPosition =
      m_ps->m_paragraphMarginLeft + m_ps->m_paragraphTextIndent;
  else
    m_ps->m_listBeginPosition = 0;
}
void MWAWContentListener::setCurrentList(shared_ptr<MWAWList> list)
{
  m_ps->m_list=list;
  if (list && list->getId() <= 0 && list->numLevels())
    list->setId(++m_ds->m_newListId);
}
shared_ptr<MWAWList> MWAWContentListener::getCurrentList() const
{
  return m_ps->m_list;
}

///////////////////
// field :
///////////////////
#include <time.h>
void MWAWContentListener::insertField(MWAWContentListener::FieldType type)
{
  switch(type) {
  case None:
    break;
  case PageNumber: {
    _flushText();
    _openSpan();
    WPXPropertyList propList;
    propList.insert("style:num-format", libmwaw::numberingTypeToString(libmwaw::ARABIC).c_str());
    m_documentInterface->insertField(WPXString("text:page-number"), propList);
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
  case Date:
    insertDateTimeField("%m/%d/%y");
    break;
  case Time:
    insertDateTimeField("%I:%M:%S %p");
    break;
  case Link:
  default:
    MWAW_DEBUG_MSG(("MWAWContentListener::insertField: must not be called with type=%d\n", int(type)));
    break;
  }
}

void MWAWContentListener::insertDateTimeField(char const *format)

{
  time_t now = time ( 0L );
  struct tm timeinfo = *(localtime ( &now));
  char buf[256];
  strftime(buf, 256, format, &timeinfo);
  WPXString tmp(buf);
  insertUnicodeString(tmp);
}

///////////////////
// section
///////////////////
bool MWAWContentListener::isSectionOpened() const
{
  return m_ps->m_isSectionOpened;
}

int MWAWContentListener::getSectionNumColumns() const
{
  return m_ps->m_numColumns;
}

bool MWAWContentListener::openSection(std::vector<int> colsWidth, WPXUnit unit)
{
  if (m_ps->m_isSectionOpened) {
    MWAW_DEBUG_MSG(("MWAWContentListener::openSection: a section is already opened\n"));
    return false;
  }

  if (m_ps->m_isTableOpened || (m_ps->m_inSubDocument && m_ps->m_subDocumentType != libmwaw::DOC_TEXT_BOX)) {
    MWAW_DEBUG_MSG(("MWAWContentListener::openSection: impossible to open a section\n"));
    return false;
  }

  size_t numCols = colsWidth.size();
  if (numCols <= 1) {
    m_ps->m_textColumns.resize(0);
    m_ps->m_numColumns=1;
  } else {
    float factor = 1.0;
    switch(unit) {
    case WPX_POINT:
    case WPX_TWIP:
      factor = MWAWPosition::getScaleFactor(unit, WPX_INCH);
    case WPX_INCH:
      break;
    case WPX_PERCENT:
    case WPX_GENERIC:
    default:
      MWAW_DEBUG_MSG(("MWAWContentListener::openSection: unknown unit\n"));
      return false;
    }
    m_ps->m_textColumns.resize(numCols);
    m_ps->m_numColumns=int(numCols);
    for (size_t col = 0; col < numCols; col++) {
      MWAWColumnDefinition column;
      column.m_width = factor*float(colsWidth[col]);
      m_ps->m_textColumns[col] = column;
    }
  }
  _openSection();
  return true;
}

bool MWAWContentListener::closeSection()
{
  if (!m_ps->m_isSectionOpened) {
    MWAW_DEBUG_MSG(("MWAWContentListener::closeSection: no section are already opened\n"));
    return false;
  }

  if (m_ps->m_isTableOpened || (m_ps->m_inSubDocument && m_ps->m_subDocumentType != libmwaw::DOC_TEXT_BOX)) {
    MWAW_DEBUG_MSG(("MWAWContentListener::closeSection: impossible to close a section\n"));
    return false;
  }

  _closeSection();
  return true;
}

///////////////////
// document
///////////////////
void MWAWContentListener::setDocumentLanguage(std::string locale)
{
  if (!locale.length()) return;
  m_ds->m_metaData.insert("libwpd:language", locale.c_str());
}

void MWAWContentListener::startDocument()
{
  if (m_ds->m_isDocumentStarted) {
    MWAW_DEBUG_MSG(("MWAWContentListener::startDocument: the document is already started\n"));
    return;
  }

  // FIXME: this is stupid, we should store a property list filled with the relevant metadata
  // and then pass that directly..
  m_documentInterface->setDocumentMetaData(m_ds->m_metaData);

  m_documentInterface->startDocument();
  m_ds->m_isDocumentStarted = true;
}

void MWAWContentListener::endDocument()
{
  if (!m_ds->m_isDocumentStarted) {
    MWAW_DEBUG_MSG(("MWAWContentListener::startDocument: the document is not started\n"));
    return;
  }

  if (!m_ps->m_isPageSpanOpened)
    _openSpan();

  if (m_ps->m_isTableOpened)
    closeTable();
  if (m_ps->m_isParagraphOpened)
    _closeParagraph();

  m_ps->m_currentListLevel = 0;
  _changeList(); // flush the list exterior

  // close the document nice and tight
  _closeSection();
  _closePageSpan();
  m_documentInterface->endDocument();
  m_ds->m_isDocumentStarted = false;
}

///////////////////
// page
///////////////////
void MWAWContentListener::_openPageSpan()
{
  if (m_ps->m_isPageSpanOpened)
    return;

  if (!m_ds->m_isDocumentStarted)
    startDocument();

  if (m_ds->m_pageList.size()==0) {
    MWAW_DEBUG_MSG(("MWAWContentListener::_openPageSpan: can not find any page\n"));
    throw libmwaw::ParseException();
  }
  unsigned actPage = 0;
  std::vector<MWAWPageSpan>::iterator it = m_ds->m_pageList.begin();
  while(actPage < m_ps->m_currentPage) {
    actPage+=(unsigned)it->getPageSpan();
    it++;
    if (it == m_ds->m_pageList.end()) {
      MWAW_DEBUG_MSG(("MWAWContentListener::_openPageSpan: can not find current page\n"));
      throw libmwaw::ParseException();
    }
  }
  MWAWPageSpan &currentPage = *it;

  WPXPropertyList propList;
  currentPage.getPageProperty(propList);
  propList.insert("libwpd:is-last-page-span", ((m_ps->m_currentPage + 1 == m_ds->m_pageList.size()) ? true : false));

  if (!m_ps->m_isPageSpanOpened)
    m_documentInterface->openPageSpan(propList);

  m_ps->m_isPageSpanOpened = true;

  _updatePageSpanDependent(false);
  m_ps->m_pageFormLength = currentPage.getFormLength();
  m_ps->m_pageFormWidth = currentPage.getFormWidth();
  m_ps->m_pageMarginLeft = currentPage.getMarginLeft();
  m_ps->m_pageMarginRight = currentPage.getMarginRight();
  m_ps->m_pageFormOrientationIsPortrait =
    currentPage.getFormOrientation() == MWAWPageSpan::PORTRAIT;
  m_ps->m_pageMarginTop = currentPage.getMarginTop();
  m_ps->m_pageMarginBottom = currentPage.getMarginBottom();

  _updatePageSpanDependent(true);
  _recomputeParagraphPositions();
  // we insert the header footer
  currentPage.sendHeaderFooters(this, m_documentInterface);

  // first paragraph in span (necessary for resetting page number)
  m_ps->m_firstParagraphInPageSpan = true;
  m_ps->m_numPagesRemainingInSpan = (currentPage.getPageSpan() - 1);
  m_ps->m_currentPage++;
}

void MWAWContentListener::_closePageSpan()
{
  if (!m_ps->m_isPageSpanOpened)
    return;

  if (m_ps->m_isSectionOpened)
    _closeSection();

  m_documentInterface->closePageSpan();
  m_ps->m_isPageSpanOpened = m_ps->m_isPageSpanBreakDeferred = false;
}

void MWAWContentListener::_updatePageSpanDependent(bool set)
{
  double deltaRight = set ? -m_ps->m_pageMarginRight : m_ps->m_pageMarginRight;
  double deltaLeft = set ? -m_ps->m_pageMarginLeft : m_ps->m_pageMarginLeft;
  if (m_ps->m_sectionMarginLeft < 0 || m_ps->m_sectionMarginLeft > 0)
    m_ps->m_sectionMarginLeft += deltaLeft;
  if (m_ps->m_sectionMarginRight < 0 || m_ps->m_sectionMarginRight > 0)
    m_ps->m_sectionMarginRight += deltaRight;
  m_ps->m_listReferencePosition += deltaLeft;
  m_ps->m_listBeginPosition += deltaLeft;
}

void MWAWContentListener::_recomputeParagraphPositions()
{
  m_ps->m_paragraphMarginLeft = m_ps->m_leftMarginByPageMarginChange
                                + m_ps->m_leftMarginByParagraphMarginChange + m_ps->m_leftMarginByTabs;
  m_ps->m_paragraphMarginRight = m_ps->m_rightMarginByPageMarginChange
                                 + m_ps->m_rightMarginByParagraphMarginChange + m_ps->m_rightMarginByTabs;
  m_ps->m_paragraphTextIndent = m_ps->m_textIndentByParagraphIndentChange + m_ps->m_textIndentByTabs;
  m_ps->m_listBeginPosition = m_ps->m_paragraphMarginLeft + m_ps->m_paragraphTextIndent;
  m_ps->m_listReferencePosition = m_ps->m_paragraphMarginLeft + m_ps->m_paragraphTextIndent;
}

///////////////////
// section
///////////////////
void MWAWContentListener::_openSection()
{
  if (m_ps->m_isSectionOpened) {
    MWAW_DEBUG_MSG(("MWAWContentListener::_openSection: a section is already opened\n"));
    return;
  }

  if (!m_ps->m_isPageSpanOpened)
    _openPageSpan();

  WPXPropertyList propList;
  propList.insert("fo:margin-left", m_ps->m_sectionMarginLeft);
  propList.insert("fo:margin-right", m_ps->m_sectionMarginRight);
  if (m_ps->m_numColumns > 1)
    propList.insert("text:dont-balance-text-columns", false);
  if (m_ps->m_sectionMarginTop > 0 || m_ps->m_sectionMarginTop < 0)
    propList.insert("libwpd:margin-top", m_ps->m_sectionMarginTop);
  if (m_ps->m_sectionMarginBottom > 0 || m_ps->m_sectionMarginBottom < 0)
    propList.insert("libwpd:margin-bottom", m_ps->m_sectionMarginBottom);

  WPXPropertyListVector columns;
  for (size_t i = 0; i < m_ps->m_textColumns.size(); i++) {
    MWAWColumnDefinition const &col = m_ps->m_textColumns[i];
    WPXPropertyList column;
    // The "style:rel-width" is expressed in twips (1440 twips per inch) and includes the left and right Gutter
    column.insert("style:rel-width", col.m_width * 1440.0, WPX_TWIP);
    column.insert("fo:start-indent", col.m_leftGutter);
    column.insert("fo:end-indent", col.m_rightGutter);
    columns.append(column);
  }
  m_documentInterface->openSection(propList, columns);

  m_ps->m_sectionAttributesChanged = false;
  m_ps->m_isSectionOpened = true;
}

void MWAWContentListener::_closeSection()
{
  if (!m_ps->m_isSectionOpened ||m_ps->m_isTableOpened)
    return;

  if (m_ps->m_isParagraphOpened)
    _closeParagraph();
  _changeList();

  m_documentInterface->closeSection();

  m_ps->m_sectionAttributesChanged = false;
  m_ps->m_isSectionOpened = false;
}

///////////////////
// paragraph
///////////////////
void MWAWContentListener::_openParagraph()
{
  if (m_ps->m_isTableOpened && !m_ps->m_isTableCellOpened)
    return;

  if (m_ps->m_isParagraphOpened || m_ps->m_isListElementOpened) {
    MWAW_DEBUG_MSG(("MWAWContentListener::_openParagraph: a paragraph (or a list) is already opened"));
    return;
  }
  if (!m_ps->m_isTableOpened && (!m_ps->m_inSubDocument || m_ps->m_subDocumentType == libmwaw::DOC_TEXT_BOX)) {
    if (m_ps->m_sectionAttributesChanged)
      _closeSection();

    if (!m_ps->m_isSectionOpened)
      _openSection();
  }

  WPXPropertyListVector tabStops;
  _getTabStops(tabStops);

  WPXPropertyList propList;
  _appendParagraphProperties(propList);

  if (!m_ps->m_isParagraphOpened)
    m_documentInterface->openParagraph(propList, tabStops);

  _resetParagraphState();
  m_ps->m_firstParagraphInPageSpan = false;
}

void MWAWContentListener::_closeParagraph()
{
  if (m_ps->m_isListElementOpened) {
    _closeListElement();
    return;
  }

  if (m_ps->m_isParagraphOpened) {
    if (m_ps->m_isSpanOpened)
      _closeSpan();

    m_documentInterface->closeParagraph();
  }

  m_ps->m_isParagraphOpened = false;
  m_ps->m_currentListLevel = 0;

  if (!m_ps->m_isTableOpened && m_ps->m_isPageSpanBreakDeferred && !m_ps->m_inSubDocument)
    _closePageSpan();
}

void MWAWContentListener::_resetParagraphState(const bool isListElement)
{
  m_ps->m_isParagraphColumnBreak = false;
  m_ps->m_isParagraphPageBreak = false;
  if (isListElement) {
    m_ps->m_isListElementOpened = true;
    m_ps->m_isParagraphOpened = true;
  } else {
    m_ps->m_isListElementOpened = false;
    m_ps->m_isParagraphOpened = true;
  }
  m_ps->m_leftMarginByTabs = 0.0;
  m_ps->m_rightMarginByTabs = 0.0;
  m_ps->m_textIndentByTabs = 0.0;
  m_ps->m_isTextColumnWithoutParagraph = false;
  m_ps->m_isHeaderFooterWithoutParagraph = false;
  _recomputeParagraphPositions();
}

void MWAWContentListener::_appendJustification(WPXPropertyList &propList, libmwaw::Justification justification)
{
  switch (justification) {
  case libmwaw::JustificationLeft:
    // doesn't require a paragraph prop - it is the default
    propList.insert("fo:text-align", "left");
    break;
  case libmwaw::JustificationCenter:
    propList.insert("fo:text-align", "center");
    break;
  case libmwaw::JustificationRight:
    propList.insert("fo:text-align", "end");
    break;
  case libmwaw::JustificationFull:
    propList.insert("fo:text-align", "justify");
    break;
  case libmwaw::JustificationFullAllLines:
    propList.insert("fo:text-align", "justify");
    propList.insert("fo:text-align-last", "justify");
    break;
  default:
    break;
  }
}

void MWAWContentListener::_appendParagraphProperties(WPXPropertyList &propList, const bool isListElement)
{
  _appendJustification(propList, m_ps->m_paragraphJustification);

  if (!m_ps->m_isTableOpened) {
    // these properties are not appropriate when a table is opened..
    if (isListElement) {
      propList.insert("fo:margin-left", (m_ps->m_listBeginPosition - m_ps->m_paragraphTextIndent));
      propList.insert("fo:text-indent", m_ps->m_paragraphTextIndent);
    } else {
      propList.insert("fo:margin-left", m_ps->m_paragraphMarginLeft);
      propList.insert("fo:text-indent", m_ps->m_listReferencePosition - m_ps->m_paragraphMarginLeft);
    }
    propList.insert("fo:margin-right", m_ps->m_paragraphMarginRight);
    if (m_ps->hasParagraphBorders()) {
      bool setAll = !m_ps->hasParagraphDifferentBorders();
      for (size_t w = 0; w < m_ps->m_paragraphBorders.size(); w++) {
        MWAWBorder const &border = m_ps->m_paragraphBorders[w];
        std::string property = border.getPropertyValue();
        if (property.length() == 0) continue;
        if (setAll == 0xF) {
          propList.insert("fo:border", property.c_str());
          break;
        }
        switch(w) {
        case MWAWBorder::Left:
          propList.insert("fo:border-left", property.c_str());
          break;
        case MWAWBorder::Right:
          propList.insert("fo:border-right", property.c_str());
          break;
        case MWAWBorder::Top:
          propList.insert("fo:border-top", property.c_str());
          break;
        case MWAWBorder::Bottom:
          propList.insert("fo:border-bottom", property.c_str());
          break;
        default:
          MWAW_DEBUG_MSG(("MWAWContentListener::_appendParagraphProperties: can not send %d border\n",int(w)));
          break;
        }
      }
    }
  }
  propList.insert("fo:margin-top", m_ps->m_paragraphMarginTop, m_ps->m_paragraphMarginBottomUnit);
  propList.insert("fo:margin-bottom", m_ps->m_paragraphMarginBottom, m_ps->m_paragraphMarginBottomUnit);
  propList.insert("fo:line-height", m_ps->m_paragraphLineSpacing, m_ps->m_paragraphLineSpacingUnit);


  if (!m_ps->m_inSubDocument && m_ps->m_firstParagraphInPageSpan) {
    unsigned actPage = 1;
    std::vector<MWAWPageSpan>::const_iterator it = m_ds->m_pageList.begin();
    while(actPage < m_ps->m_currentPage) {
      if (it == m_ds->m_pageList.end())
        break;

      actPage+=(unsigned)it->getPageSpan();
      it++;
    }
    MWAWPageSpan const &currentPage = *it;
    if (currentPage.getPageNumber() >= 0)
      propList.insert("style:page-number", currentPage.getPageNumber());
  }

  _insertBreakIfNecessary(propList);
}

void MWAWContentListener::_getTabStops(WPXPropertyListVector &tabStops)
{
  double decalX = m_ps->m_isTabPositionRelative ? -m_ps->m_leftMarginByTabs :
                  -m_ps->m_paragraphMarginLeft-m_ps->m_sectionMarginLeft-m_ps->m_pageMarginLeft;
  for (size_t i=0; i<m_ps->m_tabStops.size(); i++)
    m_ps->m_tabStops[i].addTo(tabStops, decalX);
}

///////////////////
// list
///////////////////
void MWAWContentListener::_openListElement()
{
  if (m_ps->m_isTableOpened && !m_ps->m_isTableCellOpened)
    return;

  if (!m_ps->m_isParagraphOpened && !m_ps->m_isListElementOpened) {
    if (!m_ps->m_isTableOpened && (!m_ps->m_inSubDocument || m_ps->m_subDocumentType == libmwaw::DOC_TEXT_BOX)) {
      if (m_ps->m_sectionAttributesChanged)
        _closeSection();

      if (!m_ps->m_isSectionOpened)
        _openSection();
    }

    WPXPropertyList propList;
    _appendParagraphProperties(propList, true);

    WPXPropertyListVector tabStops;
    _getTabStops(tabStops);

    if (!m_ps->m_isListElementOpened)
      m_documentInterface->openListElement(propList, tabStops);
    _resetParagraphState(true);
  }
}

void MWAWContentListener::_closeListElement()
{
  if (m_ps->m_isListElementOpened) {
    if (m_ps->m_isSpanOpened)
      _closeSpan();

    m_documentInterface->closeListElement();
  }

  m_ps->m_isListElementOpened = m_ps->m_isParagraphOpened = false;
  m_ps->m_currentListLevel = 0;

  if (!m_ps->m_isTableOpened && m_ps->m_isPageSpanBreakDeferred && !m_ps->m_inSubDocument)
    _closePageSpan();
}

void MWAWContentListener::_changeList()
{
  if (m_ps->m_isParagraphOpened)
    _closeParagraph();

  if (!m_ps->m_isSectionOpened && !m_ps->m_inSubDocument && !m_ps->m_isTableOpened)
    _openSection();

  // FIXME: even if nobody really care, if we close an ordered or an unordered
  //      elements, we must keep the previous to close this part...
  size_t actualListLevel = m_ps->m_listOrderedLevels.size();
  for (size_t i=actualListLevel; i > m_ps->m_currentListLevel; i--) {
    if (m_ps->m_listOrderedLevels[i-1])
      m_documentInterface->closeOrderedListLevel();
    else
      m_documentInterface->closeUnorderedListLevel();
  }

  WPXPropertyList propList2;
  if (m_ps->m_currentListLevel) {
    if (!m_ps->m_list.get()) {
      MWAW_DEBUG_MSG(("MWAWContentListener::_handleListChange: can not find any list\n"));
      return;
    }
    m_ps->m_list->setLevel(m_ps->m_currentListLevel);
    m_ps->m_list->openElement();

    if (m_ps->m_list->mustSendLevel(m_ps->m_currentListLevel)) {
      if (actualListLevel == m_ps->m_currentListLevel) {
        if (m_ps->m_listOrderedLevels[actualListLevel-1])
          m_documentInterface->closeOrderedListLevel();
        else
          m_documentInterface->closeUnorderedListLevel();
        actualListLevel--;
      }
      if (m_ps->m_currentListLevel==1) {
        // we must change the listID for writerperfect
        int prevId;
        if ((prevId=m_ps->m_list->getPreviousId()) > 0)
          m_ps->m_list->setId(prevId);
        else
          m_ps->m_list->setId(++m_ds->m_newListId);
      }
      m_ps->m_list->sendTo(*m_documentInterface, m_ps->m_currentListLevel);
    }

    propList2.insert("libwpd:id", m_ps->m_list->getId());
    m_ps->m_list->closeElement();
  }

  if (actualListLevel == m_ps->m_currentListLevel) return;

  m_ps->m_listOrderedLevels.resize(m_ps->m_currentListLevel, false);
  for (size_t i=actualListLevel+1; i<= m_ps->m_currentListLevel; i++) {
    if (m_ps->m_list->isNumeric(int(i))) {
      m_ps->m_listOrderedLevels[i-1] = true;
      m_documentInterface->openOrderedListLevel(propList2);
    } else {
      m_ps->m_listOrderedLevels[i-1] = false;
      m_documentInterface->openUnorderedListLevel(propList2);
    }
  }
}

///////////////////
// span
///////////////////
void MWAWContentListener::_openSpan()
{
  if (m_ps->m_isSpanOpened)
    return;

  if (m_ps->m_isTableOpened && !m_ps->m_isTableCellOpened)
    return;

  if (!m_ps->m_isParagraphOpened && !m_ps->m_isListElementOpened) {
    _changeList();
    if (m_ps->m_currentListLevel == 0)
      _openParagraph();
    else
      _openListElement();
  }

  uint32_t attributeBits = m_ps->m_textAttributeBits;
  double fontSizeChange = 1.0;
  switch (attributeBits& 0x0000001f) {
  case 0x01:  // Extra large
    fontSizeChange = 2.0;
    break;
  case 0x02: // Very large
    fontSizeChange = 1.5f;
    break;
  case 0x04: // Large
    fontSizeChange = 1.2f;
    break;
  case 0x08: // Small print
    fontSizeChange = 0.8f;
    break;
  case 0x10: // Fine print
    fontSizeChange = 0.6f;
    break;
  default: // Normal
    fontSizeChange = 1.0;
    break;
  }

  WPXPropertyList propList;
  if (attributeBits & MWAW_SUPERSCRIPT_BIT)
    propList.insert("style:text-position", "super 58.0%");
  else if (attributeBits & MWAW_SUBSCRIPT_BIT)
    propList.insert("style:text-position", "sub 58.0%");
  else if (attributeBits & MWAW_SUPERSCRIPT100_BIT)
    propList.insert("style:text-position", "20 100");
  else if (attributeBits & MWAW_SUBSCRIPT100_BIT)
    propList.insert("style:text-position", "-20 100");

  if (attributeBits & MWAW_ITALICS_BIT)
    propList.insert("fo:font-style", "italic");
  if (attributeBits & MWAW_BOLD_BIT)
    propList.insert("fo:font-weight", "bold");
  if (attributeBits & MWAW_STRIKEOUT_BIT)
    propList.insert("style:text-line-through-type", "single");
  if (attributeBits & MWAW_DOUBLE_UNDERLINE_BIT)
    propList.insert("style:text-underline-type", "double");
  else if (attributeBits & MWAW_UNDERLINE_BIT)
    propList.insert("style:text-underline-type", "single");
  if (attributeBits & MWAW_OVERLINE_BIT)
    propList.insert("style:text-overline-type", "single");
  if (attributeBits & MWAW_OUTLINE_BIT)
    propList.insert("style:text-outline", "true");
  if (attributeBits & MWAW_SMALL_CAPS_BIT)
    propList.insert("fo:font-variant", "small-caps");
  if (attributeBits & MWAW_BLINK_BIT)
    propList.insert("style:text-blinking", "true");
  if (attributeBits & MWAW_SHADOW_BIT)
    propList.insert("fo:text-shadow", "1pt 1pt");
  if (attributeBits & MWAW_HIDDEN_BIT)
    propList.insert("text:display", "none");
  if (attributeBits & MWAW_ALL_CAPS_BIT)
    propList.insert("fo:text-transform", "uppercase");
  if (attributeBits & MWAW_EMBOSS_BIT)
    propList.insert("style:font-relief", "embossed");
  else if (attributeBits & MWAW_ENGRAVE_BIT)
    propList.insert("style:font-relief", "engraved");

  if (m_ps->m_fontName.len())
    propList.insert("style:font-name", m_ps->m_fontName.cstr());

  propList.insert("fo:font-size", fontSizeChange*m_ps->m_fontSize, WPX_POINT);

  char color[20];
  sprintf(color,"#%06x",m_ps->m_fontColor);
  propList.insert("fo:color", color);

  if (m_ps->m_textLanguage != "UNSET")
    _addLanguage(m_ps->m_textLanguage, propList);

  m_documentInterface->openSpan(propList);

  m_ps->m_isSpanOpened = true;
}

void MWAWContentListener::_closeSpan()
{
  if (!m_ps->m_isSpanOpened)
    return;

  _flushText();
  m_documentInterface->closeSpan();
  m_ps->m_isSpanOpened = false;
}

///////////////////
// text (send data)
///////////////////
void MWAWContentListener::_flushDeferredTabs()
{
  if (m_ps->m_numDeferredTabs == 0) return;

  // CHECKME: the tab are not underline even if the underline bit is set
  uint32_t oldTextAttributes = m_ps->m_textAttributeBits;
  static const uint32_t s_underoverlineBits =
    MWAW_UNDERLINE_BIT | MWAW_DOUBLE_UNDERLINE_BIT | MWAW_OVERLINE_BIT;
  uint32_t newAttributes = oldTextAttributes & (~s_underoverlineBits);
  if (oldTextAttributes != newAttributes) setFontAttributes(newAttributes);
  if (!m_ps->m_isSpanOpened) _openSpan();
  for (; m_ps->m_numDeferredTabs > 0; m_ps->m_numDeferredTabs--)
    m_documentInterface->insertTab();
  if (oldTextAttributes != newAttributes) setFontAttributes(oldTextAttributes);
}

void MWAWContentListener::_flushText()
{
  if (m_ps->m_textBuffer.len() == 0) return;

  // when some many ' ' follows each other, call insertSpace
  WPXString tmpText;
  int numConsecutiveSpaces = 0;
  WPXString::Iter i(m_ps->m_textBuffer);
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
  m_ps->m_textBuffer.clear();
}

///////////////////
// Note/Comment/picture/textbox
///////////////////
void MWAWContentListener::insertNote(const NoteType noteType, MWAWSubDocumentPtr &subDocument)
{
  if (m_ps->m_isNote) {
    MWAW_DEBUG_MSG(("MWAWContentListener::insertNote try to insert a note recursively (ingnored)\n"));
    return;
  }
  WPXString label("");
  insertLabelNote(noteType, label, subDocument);
}

void MWAWContentListener::insertLabelNote(const NoteType noteType, WPXString const &label, MWAWSubDocumentPtr &subDocument)
{
  if (m_ps->m_isNote) {
    MWAW_DEBUG_MSG(("MWAWContentListener::insertLabelNote try to insert a note recursively (ingnored)\n"));
    return;
  }

  m_ps->m_isNote = true;
  if (m_ds->m_isHeaderFooterStarted) {
    MWAW_DEBUG_MSG(("MWAWContentListener::insertLabelNote try to insert a note in a header/footer\n"));
    /** Must not happen excepted in corrupted document, so we do the minimum.
    	Note that we have no choice, either we begin by closing the paragraph,
    	... or we reprogram handleSubDocument.
    */
    if (m_ps->m_isParagraphOpened)
      _closeParagraph();
    int prevListLevel = m_ps->m_currentListLevel;
    m_ps->m_currentListLevel = 0;
    _changeList(); // flush the list exterior
    handleSubDocument(subDocument, libmwaw::DOC_NOTE);
    m_ps->m_currentListLevel = (uint8_t)prevListLevel;
  } else {
    if (!m_ps->m_isParagraphOpened)
      _openParagraph();
    else {
      _flushText();
      _closeSpan();
    }

    WPXPropertyList propList;
    if (label.len())
      propList.insert("text:label", label);
    if (noteType == FOOTNOTE) {
      propList.insert("libwpd:number", ++(m_ds->m_footNoteNumber));
      m_documentInterface->openFootnote(propList);
    } else {
      propList.insert("libwpd:number", ++(m_ds->m_endNoteNumber));
      m_documentInterface->openEndnote(propList);
    }

    handleSubDocument(subDocument, libmwaw::DOC_NOTE);

    if (noteType == FOOTNOTE)
      m_documentInterface->closeFootnote();
    else
      m_documentInterface->closeEndnote();
  }
  m_ps->m_isNote = false;
}

void MWAWContentListener::insertComment(MWAWSubDocumentPtr &subDocument)
{
  if (m_ps->m_isNote) {
    MWAW_DEBUG_MSG(("MWAWContentListener::insertComment try to insert a note recursively (ingnored)"));
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
  handleSubDocument(subDocument, libmwaw::DOC_COMMENT_ANNOTATION);

  m_documentInterface->closeComment();
  m_ps->m_isNote = false;
}

void MWAWContentListener::insertTextBox
(MWAWPosition const &pos, MWAWSubDocumentPtr subDocument, WPXPropertyList frameExtras, WPXPropertyList textboxExtras)
{
  if (!openFrame(pos, frameExtras)) return;

  WPXPropertyList propList(textboxExtras);
  m_documentInterface->openTextBox(propList);
  handleSubDocument(subDocument, libmwaw::DOC_TEXT_BOX);
  m_documentInterface->closeTextBox();

  closeFrame();
}

void MWAWContentListener::insertPicture
(MWAWPosition const &pos, const WPXBinaryData &binaryData, std::string type,
 WPXPropertyList frameExtras)
{
  if (!openFrame(pos, frameExtras)) return;

  WPXPropertyList propList;
  propList.insert("libwpd:mimetype", type.c_str());
  m_documentInterface->insertBinaryObject(propList, binaryData);

  closeFrame();
}

///////////////////
// frame
///////////////////
bool MWAWContentListener::openFrame(MWAWPosition const &pos, WPXPropertyList extras)
{
  if (m_ps->m_isTableOpened && !m_ps->m_isTableCellOpened) {
    MWAW_DEBUG_MSG(("MWAWContentListener::openFrame: called in table but cell is not opened\n"));
    return false;
  }
  if (m_ps->m_isFrameOpened) {
    MWAW_DEBUG_MSG(("MWAWContentListener::openFrame: called but a frame is already opened\n"));
    return false;
  }

  switch(pos.m_anchorTo) {
  case MWAWPosition::Page:
    break;
  case MWAWPosition::Paragraph:
    if (m_ps->m_isParagraphOpened)
      _flushText();
    else
      _openParagraph();
    break;
  case MWAWPosition::CharBaseLine:
  case MWAWPosition::Char:
    if (m_ps->m_isSpanOpened)
      _flushText();
    else
      _openSpan();
    break;
  case MWAWPosition::Frame:
    if (!m_ds->m_subDocuments.size()) {
      MWAW_DEBUG_MSG(("MWAWContentListener::openFrame: can not determine the frame\n"));
      return false;
    }
    break;
  default:
    MWAW_DEBUG_MSG(("MWAWContentListener::openFrame: can not determine the anchor\n"));
    return false;
  }

  WPXPropertyList propList(extras);
  _handleFrameParameters(propList, pos);
  m_documentInterface->openFrame(propList);

  m_ps->m_isFrameOpened = true;
  return true;
}

void MWAWContentListener::closeFrame()
{
  if (!m_ps->m_isFrameOpened) {
    MWAW_DEBUG_MSG(("MWAWContentListener::closeFrame: called but no frame is already opened\n"));
    return;
  }
  m_documentInterface->closeFrame();
  m_ps->m_isFrameOpened = false;
}

void MWAWContentListener::_handleFrameParameters
( WPXPropertyList &propList, MWAWPosition const &pos)
{
  Vec2f origin = pos.origin();
  WPXUnit unit = pos.unit();
  float inchFactor=pos.getInvUnitScale(WPX_INCH);
  float pointFactor = pos.getInvUnitScale(WPX_POINT);

  propList.insert("svg:width", double(pos.size()[0]), unit);
  propList.insert("svg:height", double(pos.size()[1]), unit);
  if (pos.order() > 0)
    propList.insert("draw:z-index", pos.order());
  if (pos.naturalSize().x() > 4*pointFactor && pos.naturalSize().y() > 4*pointFactor) {
    propList.insert("libwpd:naturalWidth", pos.naturalSize().x(), pos.unit());
    propList.insert("libwpd:naturalHeight", pos.naturalSize().y(), pos.unit());
  }

  double newPosition;

  if ( pos.m_wrapping ==  MWAWPosition::WDynamic)
    propList.insert( "style:wrap", "dynamic" );
  else if ( pos.m_wrapping ==  MWAWPosition::WRunThrough) {
    propList.insert( "style:wrap", "run-through" );
    propList.insert( "style:run-through", "background" );
  } else
    propList.insert( "style:wrap", "none" );

  if (pos.m_anchorTo == MWAWPosition::Paragraph ||
      pos.m_anchorTo == MWAWPosition::Frame) {
    std::string what= pos.m_anchorTo == MWAWPosition::Paragraph ?
                      "paragraph" : "frame";
    propList.insert("text:anchor-type", what.c_str());
    propList.insert("style:vertical-rel", what.c_str());
    propList.insert("style:horizontal-rel", what.c_str());
    double w = m_ps->m_pageFormWidth - m_ps->m_pageMarginLeft
               - m_ps->m_pageMarginRight - m_ps->m_sectionMarginLeft
               - m_ps->m_sectionMarginRight - m_ps->m_paragraphMarginLeft
               - m_ps->m_paragraphMarginRight;
    w *= inchFactor;
    switch ( pos.m_xPos) {
    case MWAWPosition::XRight:
      if (origin[0] < 0.0 || origin[0] > 0.0) {
        propList.insert( "style:horizontal-pos", "from-left");
        propList.insert( "svg:x", double(origin[0] - pos.size()[0] + w), unit);
      } else
        propList.insert("style:horizontal-pos", "right");
      break;
    case MWAWPosition::XCenter:
      if (origin[0] < 0.0 || origin[0] > 0.0) {
        propList.insert( "style:horizontal-pos", "from-left");
        propList.insert( "svg:x", double(origin[0] - pos.size()[0]/2.0 + w/2.0), unit);
      } else
        propList.insert("style:horizontal-pos", "center");
      break;
    case MWAWPosition::XLeft:
    case MWAWPosition::XFull:
    default:
      if (origin[0] < 0.0 || origin[0] > 0.0) {
        propList.insert( "style:horizontal-pos", "from-left");
        propList.insert( "svg:x", double(origin[0]), unit);
      } else
        propList.insert("style:horizontal-pos", "left");
      break;
    }

    if (origin[1] < 0.0 || origin[1] > 0.0) {
      propList.insert( "style:vertical-pos", "from-top" );
      propList.insert( "svg:y", double(origin[1]), unit);
    } else
      propList.insert( "style:vertical-pos", "top" );
    return;
  }

  if ( pos.m_anchorTo == MWAWPosition::Page ) {
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
    case MWAWPosition::YFull:
      propList.insert("svg:height", double(h), unit);
    case MWAWPosition::YTop:
      if (origin[1] < 0.0 || origin[1] > 0.0) {
        propList.insert("style:vertical-pos", "from-top" );
        newPosition = origin[1];
        if (newPosition > h -pos.size()[1])
          newPosition = h - pos.size()[1];
        propList.insert("svg:y", double(newPosition), unit);
      } else
        propList.insert("style:vertical-pos", "top" );
      break;
    case MWAWPosition::YCenter:
      if (origin[1] < 0.0 || origin[1] > 0.0) {
        propList.insert("style:vertical-pos", "from-top" );
        newPosition = (h - pos.size()[1])/2.0;
        if (newPosition > h -pos.size()[1]) newPosition = h - pos.size()[1];
        propList.insert("svg:y", double(newPosition), unit);
      } else
        propList.insert("style:vertical-pos", "middle" );
      break;
    case MWAWPosition::YBottom:
      if (origin[1] < 0.0 || origin[1] > 0.0) {
        propList.insert("style:vertical-pos", "from-top" );
        newPosition = h - pos.size()[1]-origin[1];
        if (newPosition > h -pos.size()[1]) newPosition = h -pos.size()[1];
        else if (newPosition < 0) newPosition = 0;
        propList.insert("svg:y", double(newPosition), unit);
      } else
        propList.insert("style:vertical-pos", "bottom" );
      break;
    default:
      break;
    }

    switch ( pos.m_xPos ) {
    case MWAWPosition::XFull:
      propList.insert("svg:width", double(w), unit);
    case MWAWPosition::XLeft:
      if ( origin[0] < 0.0 || origin[0] > 0.0 ) {
        propList.insert( "style:horizontal-pos", "from-left");
        propList.insert( "svg:x", double(origin[0]), unit);
      } else
        propList.insert( "style:horizontal-pos", "left");
      break;
    case MWAWPosition::XRight:
      if ( origin[0] < 0.0 || origin[0] > 0.0 ) {
        propList.insert( "style:horizontal-pos", "from-left");
        propList.insert( "svg:x",double( w - pos.size()[0] + origin[0]), unit);
      } else
        propList.insert( "style:horizontal-pos", "right");
      break;
    case MWAWPosition::XCenter:
      if ( origin[0] < 0.0 || origin[0] > 0.0 ) {
        propList.insert( "style:horizontal-pos", "from-left");
        propList.insert( "svg:x", double((w - pos.size()[0])/2. + origin[0]), unit);
      } else
        propList.insert( "style:horizontal-pos", "center" );
      break;
    default:
      break;
    }
    return;
  }
  if ( pos.m_anchorTo != MWAWPosition::Char &&
       pos.m_anchorTo != MWAWPosition::CharBaseLine) return;

  propList.insert("text:anchor-type", "as-char");
  if ( pos.m_anchorTo == MWAWPosition::CharBaseLine)
    propList.insert( "style:vertical-rel", "baseline" );
  else
    propList.insert( "style:vertical-rel", "line" );
  switch ( pos.m_yPos ) {
  case MWAWPosition::YFull:
  case MWAWPosition::YTop:
    if ( origin[1] < 0.0 || origin[1] > 0.0) {
      propList.insert( "style:vertical-pos", "from-top" );
      propList.insert( "svg:y", double(origin[1]), unit);
    } else
      propList.insert( "style:vertical-pos", "top" );
    break;
  case MWAWPosition::YCenter:
    if ( origin[1] < 0.0 || origin[1] > 0.0) {
      propList.insert( "style:vertical-pos", "from-top" );
      propList.insert( "svg:y", double(origin[1] - pos.size()[1]/2.0), unit);
    } else
      propList.insert( "style:vertical-pos", "middle" );
    break;
  case MWAWPosition::YBottom:
  default:
    if ( origin[1] < 0.0 || origin[1] > 0.0) {
      propList.insert( "style:vertical-pos", "from-top" );
      propList.insert( "svg:y", double(origin[1] - pos.size()[1]), unit);
    } else
      propList.insert( "style:vertical-pos", "bottom" );
    break;
  }
}

///////////////////
// subdocument
///////////////////
void MWAWContentListener::handleSubDocument(MWAWSubDocumentPtr subDocument, libmwaw::SubDocumentType subDocumentType)
{
  _pushParsingState();
  _startSubDocument();
  m_ps->m_subDocumentType = subDocumentType;

  m_ps->m_isPageSpanOpened = true;
  m_ps->m_list.reset();

  switch(subDocumentType) {
  case libmwaw::DOC_TEXT_BOX:
    m_ps->m_pageMarginLeft = m_ps->m_pageMarginRight =
                               m_ps->m_pageMarginTop = m_ps->m_pageMarginBottom = 0.0;
    m_ps->m_sectionAttributesChanged = true;
    break;
  case libmwaw::DOC_HEADER_FOOTER:
    m_ps->m_isHeaderFooterWithoutParagraph = true;
    m_ds->m_isHeaderFooterStarted = true;
    break;
  case libmwaw::DOC_NONE:
  case libmwaw::DOC_NOTE:
  case libmwaw::DOC_TABLE:
  case libmwaw::DOC_COMMENT_ANNOTATION:
  default:
    break;
  }

  // Check whether the document is calling itself
  bool sendDoc = true;
  for (size_t i = 0; i < m_ds->m_subDocuments.size(); i++) {
    if (!subDocument)
      break;
    if (subDocument == m_ds->m_subDocuments[i]) {
      MWAW_DEBUG_MSG(("MWAWContentListener::handleSubDocument: recursif call, stop...\n"));
      sendDoc = false;
      break;
    }
  }
  if (sendDoc) {
    if (subDocument) {
      m_ds->m_subDocuments.push_back(subDocument);
      shared_ptr<MWAWContentListener> listen(this, MWAW_shared_ptr_noop_deleter<MWAWContentListener>());
      try {
        subDocument->parse(listen, subDocumentType);
      } catch(...) {
        MWAW_DEBUG_MSG(("Works: MWAWContentListener::handleSubDocument exception catched \n"));
      }
      m_ds->m_subDocuments.pop_back();
    }
    if (m_ps->m_isHeaderFooterWithoutParagraph)
      _openSpan();
  }

  switch (m_ps->m_subDocumentType) {
  case libmwaw::DOC_TEXT_BOX:
    _closeSection();
    break;
  case libmwaw::DOC_HEADER_FOOTER:
    m_ds->m_isHeaderFooterStarted = false;
  case libmwaw::DOC_NONE:
  case libmwaw::DOC_NOTE:
  case libmwaw::DOC_TABLE:
  case libmwaw::DOC_COMMENT_ANNOTATION:
  default:
    break;
  }
  _endSubDocument();
  _popParsingState();
}

bool MWAWContentListener::isHeaderFooterOpened() const
{
  return m_ds->m_isHeaderFooterStarted;
}

void MWAWContentListener::_startSubDocument()
{
  m_ds->m_isDocumentStarted = true;
  m_ps->m_inSubDocument = true;
}

void MWAWContentListener::_endSubDocument()
{
  if (m_ps->m_isTableOpened)
    closeTable();
  if (m_ps->m_isParagraphOpened)
    _closeParagraph();

  m_ps->m_currentListLevel = 0;
  _changeList(); // flush the list exterior
}

///////////////////
// table
///////////////////
void MWAWContentListener::openTable(std::vector<float> const &colWidth, WPXUnit unit)
{
  if (m_ps->m_isTableOpened) {
    MWAW_DEBUG_MSG(("MWAWContentListener::openTable: called with m_isTableOpened=true\n"));
    return;
  }

  if (m_ps->m_isParagraphOpened)
    _closeParagraph();

  _pushParsingState();
  _startSubDocument();
  m_ps->m_subDocumentType = libmwaw::DOC_TABLE;

  WPXPropertyList propList;
  propList.insert("table:align", "left");
  propList.insert("fo:margin-left", 0.0);

  float tableWidth = 0;
  WPXPropertyListVector columns;

  size_t nCols = colWidth.size();
  for (size_t c = 0; c < nCols; c++) {
    WPXPropertyList column;
    column.insert("style:column-width", colWidth[c], unit);
    columns.append(column);

    tableWidth += colWidth[c];
  }
  propList.insert("style:width", tableWidth, unit);
  m_documentInterface->openTable(propList, columns);
  m_ps->m_isTableOpened = true;
}

void MWAWContentListener::closeTable()
{
  if (!m_ps->m_isTableOpened) {
    MWAW_DEBUG_MSG(("MWAWContentListener::closeTable: called with m_isTableOpened=false\n"));
    return;
  }

  m_ps->m_isTableOpened = false;
  _endSubDocument();
  m_documentInterface->closeTable();

  _popParsingState();
}

void MWAWContentListener::openTableRow(float h, WPXUnit unit, bool headerRow)
{
  if (m_ps->m_isTableRowOpened) {
    MWAW_DEBUG_MSG(("MWAWContentListener::openTableRow: called with m_isTableRowOpened=true\n"));
    return;
  }
  if (!m_ps->m_isTableOpened) {
    MWAW_DEBUG_MSG(("MWAWContentListener::openTableRow: called with m_isTableOpened=false\n"));
    return;
  }
  WPXPropertyList propList;
  propList.insert("libwpd:is-header-row", headerRow);

  propList.insert("style:row-height", h, unit);
  m_documentInterface->openTableRow(propList);
  m_ps->m_isTableRowOpened = true;
}

void MWAWContentListener::closeTableRow()
{
  if (!m_ps->m_isTableRowOpened) {
    MWAW_DEBUG_MSG(("MWAWContentListener::openTableRow: called with m_isTableRowOpened=false\n"));
    return;
  }
  m_ps->m_isTableRowOpened = false;
  m_documentInterface->closeTableRow();
}

void MWAWContentListener::openTableCell(MWAWCell const &cell, WPXPropertyList const &extras)
{
  if (!m_ps->m_isTableRowOpened) {
    MWAW_DEBUG_MSG(("MWAWContentListener::openTableCell: called with m_isTableRowOpened=false\n"));
    return;
  }
  if (m_ps->m_isTableCellOpened) {
    MWAW_DEBUG_MSG(("MWAWContentListener::openTableCell: called with m_isTableCellOpened=true\n"));
    closeTableCell();
  }

  WPXPropertyList propList(extras);

  if (extras["office:value-type"]) {
    std::stringstream f;
    switch (cell.format()) {
    case MWAWCell::F_NUMBER:
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
    case MWAWCell::F_DATE:
      if (strcmp(extras["office:value-type"]->getStr().cstr(), "date") == 0) {
        f << "Date" << cell.subformat();
        propList.insert("style:data-style-name", f.str().c_str());
      }
      break;
    case MWAWCell::F_TIME:
      if (strcmp(extras["office:value-type"]->getStr().cstr(), "time") == 0) {
        f << "Time" << cell.subformat();
        propList.insert("style:data-style-name", f.str().c_str());
      }
      break;
    case MWAWCell::F_TEXT:
    case MWAWCell::F_UNKNOWN:
    default:
      break;
    }
  }

  propList.insert("libwpd:column", cell.position()[0]);
  propList.insert("libwpd:row", cell.position()[1]);

  propList.insert("table:number-columns-spanned", cell.numSpannedCells()[0]);
  propList.insert("table:number-rows-spanned", cell.numSpannedCells()[1]);

  std::vector<MWAWBorder> const &borders = cell.borders();
  for (size_t c = 0; c < borders.size(); c++) {
    std::string property = borders[c].getPropertyValue();
    if (property.length() == 0) continue;
    switch(c) {
    case MWAWBorder::Left:
      propList.insert("fo:border-left", property.c_str());
      break;
    case MWAWBorder::Right:
      propList.insert("fo:border-right", property.c_str());
      break;
    case MWAWBorder::Top:
      propList.insert("fo:border-top", property.c_str());
      break;
    case MWAWBorder::Bottom:
      propList.insert("fo:border-bottom", property.c_str());
      break;
    default:
      MWAW_DEBUG_MSG(("MWAWContentListener::openTableCell: can not send %d border\n",int(c)));
      break;
    }
  }
  if (cell.isProtected())
    propList.insert("style:cell-protect","protected");
  // alignement
  switch(cell.hAlignement()) {
  case MWAWCell::HALIGN_LEFT:
    propList.insert("fo:text-align", "first");
    propList.insert("style:text-align-source", "fix");
    break;
  case MWAWCell::HALIGN_CENTER:
    propList.insert("fo:text-align", "center");
    propList.insert("style:text-align-source", "fix");
    break;
  case MWAWCell::HALIGN_RIGHT:
    propList.insert("fo:text-align", "end");
    propList.insert("style:text-align-source", "fix");
    break;
  case MWAWCell::HALIGN_DEFAULT:
    break; // default
  case MWAWCell::HALIGN_FULL:
  default:
    MWAW_DEBUG_MSG(("MWAWContentListener::openTableCell: called with unknown align=%d\n", cell.hAlignement()));
  }

  m_ps->m_isTableCellOpened = true;
  m_documentInterface->openTableCell(propList);
}

void MWAWContentListener::closeTableCell()
{
  if (!m_ps->m_isTableCellOpened) {
    MWAW_DEBUG_MSG(("MWAWContentListener::closeTableCell: called with m_isTableCellOpened=false\n"));
    return;
  }

  _closeParagraph();
  m_ps->m_currentListLevel = 0;
  _changeList(); // flush the list exterior

  m_ps->m_isTableCellOpened = false;
  m_documentInterface->closeTableCell();
}

///////////////////
// others
///////////////////
void MWAWContentListener::_addLanguage(std::string const &lang, WPXPropertyList &propList)
{
  if (lang.length()) {
    std::string language(lang);
    std::string country("none");
    if (lang.length() > 3 && lang[2]=='_') {
      country=lang.substr(3);
      language=lang.substr(0,2);
    }
    propList.insert("fo:language", language.c_str());
    propList.insert("fo:country", country.c_str());
  } else {
    propList.insert("fo:language", "none");
    propList.insert("fo:country", "none");
  }
}

// ---------- state stack ------------------
shared_ptr<MWAWContentParsingState> MWAWContentListener::_pushParsingState()
{
  shared_ptr<MWAWContentParsingState> actual = m_ps;
  m_psStack.push_back(actual);
  m_ps.reset(new MWAWContentParsingState);

  // BEGIN: copy page properties into the new parsing state
  m_ps->m_pageFormLength = actual->m_pageFormLength;
  m_ps->m_pageFormWidth = actual->m_pageFormWidth;
  m_ps->m_pageFormOrientationIsPortrait =	actual->m_pageFormOrientationIsPortrait;
  m_ps->m_pageMarginLeft = actual->m_pageMarginLeft;
  m_ps->m_pageMarginRight = actual->m_pageMarginRight;
  m_ps->m_pageMarginTop = actual->m_pageMarginTop;
  m_ps->m_pageMarginBottom = actual->m_pageMarginBottom;

  m_ps->m_isNote = actual->m_isNote;

  return actual;
}

void MWAWContentListener::_popParsingState()
{
  if (m_psStack.size()==0) {
    MWAW_DEBUG_MSG(("MWAWContentListener::_popParsingState: psStack is empty()\n"));
    throw libmwaw::ParseException();
  }
  m_ps = m_psStack.back();
  m_psStack.pop_back();
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
