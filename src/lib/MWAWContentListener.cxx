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

#include <cstring>
#include <iomanip>
#include <sstream>

#include <libwpd/libwpd.h>

#include "libmwaw_internal.hxx"

#include "MWAWCell.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWList.hxx"
#include "MWAWPageSpan.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWContentListener.hxx"

//! Internal and low level namespace to define the states of MWAWContentListener
namespace MWAWContentListenerInternal
{
//! a enum to define basic break bit
enum { PageBreakBit=0x1, ColumnBreakBit=0x2 };
//! a class to store the document state of a MWAWContentListener
struct DocumentState {
  //! constructor
  DocumentState(std::vector<MWAWPageSpan> const &pageList) :
    m_pageList(pageList), m_metaData(), m_footNoteNumber(0), m_endNoteNumber(0), m_newListId(0),
    m_isDocumentStarted(false), m_isHeaderFooterStarted(false), m_subDocuments() {
  }
  //! destructor
  ~DocumentState() {
  }

  //! the pages definition
  std::vector<MWAWPageSpan> m_pageList;
  //! the document meta data
  WPXPropertyList m_metaData;

  int m_footNoteNumber /** footnote number*/, m_endNoteNumber /** endnote number*/;
  int m_newListId; /** a new free id */

  bool m_isDocumentStarted /** a flag to know if the document is open */, m_isHeaderFooterStarted /** a flag to know if the header footer is started */;
  std::vector<MWAWSubDocumentPtr> m_subDocuments; /** list of document actually open */

private:
  DocumentState(const DocumentState &);
  DocumentState &operator=(const DocumentState &);
};

/** the state of a MWAWContentListener */
struct State {
  //! constructor
  State();
  //! destructor
  ~State() { }

  //! a buffer to stored the text
  WPXString m_textBuffer;
  //! the number of tabs to add
  int m_numDeferredTabs;

  //! the font
  MWAWFont m_font;
  //! the paragraph
  MWAWParagraph m_paragraph;
  //! a sequence of bit used to know if we need page/column break
  int m_paragraphNeedBreak;

  shared_ptr<MWAWList> m_list;

  bool m_isPageSpanOpened;
  bool m_isSectionOpened;
  bool m_isFrameOpened;
  bool m_isPageSpanBreakDeferred;
  bool m_isHeaderFooterWithoutParagraph;

  bool m_isSpanOpened;
  bool m_isParagraphOpened;
  bool m_isListElementOpened;

  bool m_firstParagraphInPageSpan;

  std::vector<unsigned int> m_numRowsToSkip;
  bool m_isTableOpened;
  bool m_isTableRowOpened;
  bool m_isTableColumnOpened;
  bool m_isTableCellOpened;

  unsigned m_currentPage;
  int m_numPagesRemainingInSpan;
  int m_currentPageNumber;

  bool m_sectionAttributesChanged;
  int m_numColumns;
  std::vector < MWAWColumnDefinition > m_textColumns;
  bool m_isTextColumnWithoutParagraph;

  double m_pageFormLength;
  double m_pageFormWidth;
  bool m_pageFormOrientationIsPortrait;

  double m_pageMarginLeft;
  double m_pageMarginRight;
  double m_pageMarginTop;
  double m_pageMarginBottom;

  std::vector<bool> m_listOrderedLevels; //! a stack used to know what is open

  bool m_inSubDocument;

  bool m_isNote;
  libmwaw::SubDocumentType m_subDocumentType;

private:
  State(const State &);
  State &operator=(const State &);
};

State::State() :
  m_textBuffer(""), m_numDeferredTabs(0),

  m_font(20,12), // default time 12

  m_paragraph(), m_paragraphNeedBreak(0),

  m_list(),

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

  m_listOrderedLevels(),

  m_inSubDocument(false),
  m_isNote(false),
  m_subDocumentType(libmwaw::DOC_NONE)
{
}
}

MWAWContentListener::MWAWContentListener(shared_ptr<MWAWFontConverter> fontConverter, std::vector<MWAWPageSpan> const &pageList, WPXDocumentInterface *documentInterface) :
  m_ds(new MWAWContentListenerInternal::DocumentState(pageList)), m_ps(new MWAWContentListenerInternal::State), m_psStack(),
  m_fontConverter(fontConverter), m_documentInterface(documentInterface)
{
}

MWAWContentListener::~MWAWContentListener()
{
}


///////////////////
// text data
///////////////////
void MWAWContentListener::insertChar(uint8_t character)
{
  if (character >= 0x80) {
    insertUnicode(character);
    return;
  }
  _flushDeferredTabs ();
  if (!m_ps->m_isSpanOpened) _openSpan();
  m_ps->m_textBuffer.append((char) character);
}

void MWAWContentListener::insertCharacter(unsigned char c)
{
  int unicode = m_fontConverter->unicode (m_ps->m_font.id(), c);
  if (unicode == -1) {
    if (c < 0x20) {
      MWAW_DEBUG_MSG(("MWAWContentListener::insertCharacter: Find odd char %x\n", int(c)));
    } else
      insertChar((uint8_t) c);
  } else
    insertUnicode((uint32_t) unicode);
}

int MWAWContentListener::insertCharacter(unsigned char c, MWAWInputStreamPtr &input, long endPos)
{
  if (!input || !m_fontConverter) {
    MWAW_DEBUG_MSG(("MWAWContentListener::insertCharacter: input or font converter does not exist!!!!\n"));
    return 0;
  }
  long debPos=input->tell();
  int fId = m_ps->m_font.id();
  int unicode = endPos==debPos ? m_fontConverter->unicode (fId, c) :
                m_fontConverter->unicode (fId, c, input);

  long pos=input->tell();
  if (endPos > 0 && pos > endPos) {
    MWAW_DEBUG_MSG(("MWAWContentListener::insertCharacter: problem reading a character\n"));
    pos = debPos;
    input->seek(pos, WPX_SEEK_SET);
    unicode = m_fontConverter->unicode (fId, c);
  }
  if (unicode == -1) {
    if (c < 0x20) {
      MWAW_DEBUG_MSG(("MWAWContentListener::sendText: Find odd char %x\n", int(c)));
    } else
      insertChar((uint8_t) c);
  } else
    insertUnicode((uint32_t) unicode);

  return int(pos-debPos);
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
  m_ps->m_font.set(MWAWFont::Script());
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

void MWAWContentListener::insertBreak(MWAWContentListener::BreakType breakType)
{
  switch (breakType) {
  case ColumnBreak:
    if (!m_ps->m_isPageSpanOpened && !m_ps->m_inSubDocument)
      _openSpan();
    if (m_ps->m_isParagraphOpened)
      _closeParagraph();
    m_ps->m_paragraphNeedBreak |= MWAWContentListenerInternal::ColumnBreakBit;
    m_ps->m_isTextColumnWithoutParagraph = true;
    break;
  case PageBreak:
    if (!m_ps->m_isPageSpanOpened && !m_ps->m_inSubDocument)
      _openSpan();
    if (m_ps->m_isParagraphOpened)
      _closeParagraph();
    m_ps->m_paragraphNeedBreak |= MWAWContentListenerInternal::PageBreakBit;
    break;
  case SoftPageBreak:
  default:
    break;
  }

  if (m_ps->m_inSubDocument)
    return;

  switch (breakType) {
  case PageBreak:
  case SoftPageBreak:
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
  case ColumnBreak:
  default:
    break;
  }
}

void MWAWContentListener::_insertBreakIfNecessary(WPXPropertyList &propList)
{
  if (!m_ps->m_paragraphNeedBreak)
    return;

  if ((m_ps->m_paragraphNeedBreak&MWAWContentListenerInternal::PageBreakBit) ||
      m_ps->m_numColumns <= 1) {
    if (m_ps->m_inSubDocument) {
      MWAW_DEBUG_MSG(("MWAWContentListener::_insertBreakIfNecessary: can not add page break in subdocument\n"));
    } else
      propList.insert("fo:break-before", "page");
  } else if (m_ps->m_paragraphNeedBreak&MWAWContentListenerInternal::ColumnBreakBit)
    propList.insert("fo:break-before", "column");
  m_ps->m_paragraphNeedBreak=0;
}

///////////////////
// font/paragraph function
///////////////////
void MWAWContentListener::setFont(MWAWFont const &font)
{
  if (font == m_ps->m_font) return;

  // check if id and size are defined, if not used the previous fields
  MWAWFont finalFont(font);
  if (font.id() == -1)
    finalFont.setId(m_ps->m_font.id());
  if (font.size() <= 0)
    finalFont.setSize(m_ps->m_font.size());
  if (finalFont == m_ps->m_font) return;

  _closeSpan();
  m_ps->m_font = finalFont;
}

MWAWFont const &MWAWContentListener::getFont() const
{
  return m_ps->m_font;
}

bool MWAWContentListener::isParagraphOpened() const
{
  return m_ps->m_isParagraphOpened;
}

void MWAWContentListener::setParagraph(MWAWParagraph const &para)
{
  if (para==m_ps->m_paragraph) return;

  m_ps->m_paragraph=para;

  int listIndex=*para.m_listLevelIndex;
  if (listIndex <= 0) return;

  // FIXME: remove me when the list management is changed
  MWAWList::Level level = *para.m_listLevel;
  shared_ptr<MWAWList> list=m_ps->m_list;
  if (!list) {
    list.reset(new MWAWList);
    list->set(listIndex, level);
    setList(list);
  } else
    list->set(listIndex, level);
}

MWAWParagraph const &MWAWContentListener::getParagraph() const
{
  return m_ps->m_paragraph;
}

///////////////////
// List: Minimal implementation
///////////////////
void MWAWContentListener::setList(shared_ptr<MWAWList> list)
{
  m_ps->m_list=list;
  if (list && list->getId() <= 0 && list->numLevels())
    list->setId(++m_ds->m_newListId);
}

shared_ptr<MWAWList> MWAWContentListener::getList() const
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
  case PageCount:
  case PageNumber: {
    _flushText();
    _openSpan();
    WPXPropertyList propList;
    propList.insert("style:num-format", libmwaw::numberingTypeToString(libmwaw::ARABIC).c_str());
    if (type == PageNumber)
      m_documentInterface->insertField(WPXString("text:page-number"), propList);
    else
      m_documentInterface->insertField(WPXString("text:page-count"), propList);
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
  if (numCols <= 1)
    m_ps->m_textColumns.resize(0);
  else {
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

void MWAWContentListener::endDocument(bool sendDelayedSubDoc)
{
  if (!m_ds->m_isDocumentStarted) {
    MWAW_DEBUG_MSG(("MWAWContentListener::startDocument: the document is not started\n"));
    return;
  }

  if (!m_ps->m_isPageSpanOpened) {
    // we must call by hand openPageSpan to avoid sending any header/footer documents
    if (!sendDelayedSubDoc) _openPageSpan(false);
    _openSpan();
  }

  if (m_ps->m_isTableOpened)
    closeTable();
  if (m_ps->m_isParagraphOpened)
    _closeParagraph();

  m_ps->m_paragraph.m_listLevelIndex = 0;
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
void MWAWContentListener::_openPageSpan(bool sendHeaderFooters)
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

  m_ps->m_pageFormLength = currentPage.getFormLength();
  m_ps->m_pageFormWidth = currentPage.getFormWidth();
  m_ps->m_pageMarginLeft = currentPage.getMarginLeft();
  m_ps->m_pageMarginRight = currentPage.getMarginRight();
  m_ps->m_pageFormOrientationIsPortrait =
    currentPage.getFormOrientation() == MWAWPageSpan::PORTRAIT;
  m_ps->m_pageMarginTop = currentPage.getMarginTop();
  m_ps->m_pageMarginBottom = currentPage.getMarginBottom();

  // we insert the header footer
  if (sendHeaderFooters)
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

  m_ps->m_numColumns = int(m_ps->m_textColumns.size());

  WPXPropertyList propList;
  propList.insert("fo:margin-left", 0.0);
  propList.insert("fo:margin-right", 0.0);
  if (m_ps->m_numColumns > 1)
    propList.insert("text:dont-balance-text-columns", false);

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

  m_ps->m_numColumns = 1;
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

  WPXPropertyList propList;
  _appendParagraphProperties(propList);
  WPXPropertyListVector tabStops;
  m_ps->m_paragraph.addTabsTo(tabStops);

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
  m_ps->m_paragraph.m_listLevelIndex = 0;

  if (!m_ps->m_isTableOpened && m_ps->m_isPageSpanBreakDeferred && !m_ps->m_inSubDocument)
    _closePageSpan();
}

void MWAWContentListener::_resetParagraphState(const bool isListElement)
{
  m_ps->m_paragraphNeedBreak = 0;
  m_ps->m_isListElementOpened = isListElement;
  m_ps->m_isParagraphOpened = true;
  m_ps->m_isTextColumnWithoutParagraph = false;
  m_ps->m_isHeaderFooterWithoutParagraph = false;
}

void MWAWContentListener::_appendParagraphProperties(WPXPropertyList &propList, const bool /*isListElement*/)
{
  m_ps->m_paragraph.addTo(propList,m_ps->m_isTableOpened);

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
    m_ps->m_paragraph.addTabsTo(tabStops);

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
  m_ps->m_paragraph.m_listLevelIndex = 0;

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
  size_t newLevel= (size_t) m_ps->m_paragraph.m_listLevelIndex.get();
  for (size_t i=actualListLevel; i > newLevel; i--) {
    if (m_ps->m_listOrderedLevels[i-1])
      m_documentInterface->closeOrderedListLevel();
    else
      m_documentInterface->closeUnorderedListLevel();
  }

  WPXPropertyList propList;
  if (newLevel) {
    if (!m_ps->m_list.get()) {
      MWAW_DEBUG_MSG(("MWAWContentListener::_handleListChange: can not find any list\n"));
      return;
    }
    m_ps->m_list->setLevel((int)newLevel);
    m_ps->m_list->openElement();

    if (m_ps->m_list->mustSendLevel((int)newLevel)) {
      if (actualListLevel == newLevel) {
        if (m_ps->m_listOrderedLevels[actualListLevel-1])
          m_documentInterface->closeOrderedListLevel();
        else
          m_documentInterface->closeUnorderedListLevel();
        actualListLevel--;
      }
      if (newLevel==1) {
        // we must change the listID for writerperfect
        int prevId;
        if ((prevId=m_ps->m_list->getPreviousId()) > 0)
          m_ps->m_list->setId(prevId);
        else
          m_ps->m_list->setId(++m_ds->m_newListId);
      }
      m_ps->m_list->sendTo(*m_documentInterface, (int) newLevel);
    }

    propList.insert("libwpd:id", m_ps->m_list->getId());
    m_ps->m_list->closeElement();
  }

  if (actualListLevel == newLevel) return;

  m_ps->m_listOrderedLevels.resize(newLevel, false);
  for (size_t i=actualListLevel+1; i<= newLevel; i++) {
    bool ordered = m_ps->m_list->isNumeric(int(i));
    m_ps->m_listOrderedLevels[i-1] = ordered;
    if (ordered)
      m_documentInterface->openOrderedListLevel(propList);
    else
      m_documentInterface->openUnorderedListLevel(propList);
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
    if (*m_ps->m_paragraph.m_listLevelIndex == 0)
      _openParagraph();
    else
      _openListElement();
  }

  WPXPropertyList propList;
  m_ps->m_font.addTo(propList, m_fontConverter);

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
  if (!m_ps->m_font.hasDecorationLines()) {
    if (!m_ps->m_isSpanOpened) _openSpan();
    for (; m_ps->m_numDeferredTabs > 0; m_ps->m_numDeferredTabs--)
      m_documentInterface->insertTab();
    return;
  }

  MWAWFont oldFont(m_ps->m_font);
  m_ps->m_font.resetDecorationLines();
  _closeSpan();
  _openSpan();
  for (; m_ps->m_numDeferredTabs > 0; m_ps->m_numDeferredTabs--)
    m_documentInterface->insertTab();
  setFont(oldFont);
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
void MWAWContentListener::resetNoteNumber(const NoteType noteType, int number)
{
  if (noteType == FOOTNOTE)
    m_ds->m_footNoteNumber = number-1;
  else
    m_ds->m_endNoteNumber = number-1;
}

void MWAWContentListener::insertNote(const NoteType noteType, MWAWSubDocumentPtr &subDocument)
{
  if (m_ps->m_isNote) {
    MWAW_DEBUG_MSG(("MWAWContentListener::insertNote try to insert a note recursively (ignored)\n"));
    return;
  }
  WPXString label("");
  insertLabelNote(noteType, label, subDocument);
}

void MWAWContentListener::insertLabelNote(const NoteType noteType, WPXString const &label, MWAWSubDocumentPtr &subDocument)
{
  if (m_ps->m_isNote) {
    MWAW_DEBUG_MSG(("MWAWContentListener::insertLabelNote try to insert a note recursively (ignored)\n"));
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
    int prevListLevel = *m_ps->m_paragraph.m_listLevelIndex;
    m_ps->m_paragraph.m_listLevelIndex = 0;
    _changeList(); // flush the list exterior
    handleSubDocument(subDocument, libmwaw::DOC_NOTE);
    m_ps->m_paragraph.m_listLevelIndex = prevListLevel;
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
    MWAW_DEBUG_MSG(("MWAWContentListener::insertComment try to insert a note recursively (ignored)\n"));
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
  MWAWPosition fPos(pos);
  switch(pos.m_anchorTo) {
  case MWAWPosition::Page:
    break;
  case MWAWPosition::Paragraph:
    if (m_ps->m_isParagraphOpened)
      _flushText();
    else
      _openParagraph();
    break;
  case MWAWPosition::Unknown:
    MWAW_DEBUG_MSG(("MWAWContentListener::openFrame: UNKNOWN position, insert as char position\n"));
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
    if (m_ps->m_subDocumentType==libmwaw::DOC_HEADER_FOOTER) {
      MWAW_DEBUG_MSG(("MWAWContentListener::openFrame: called with Frame position in header footer, switch to paragraph\n"));
      if (m_ps->m_isParagraphOpened)
        _flushText();
      else
        _openParagraph();
      fPos.m_anchorTo=MWAWPosition::Paragraph;
    }
    break;
  default:
    MWAW_DEBUG_MSG(("MWAWContentListener::openFrame: can not determine the anchor\n"));
    return false;
  }

  WPXPropertyList propList(extras);
  _handleFrameParameters(propList, fPos);
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

  if (pos.size()[0]>0)
    propList.insert("svg:width", double(pos.size()[0]), unit);
  if (pos.size()[1]>0)
    propList.insert("svg:height", double(pos.size()[1]), unit);
  if (pos.order() > 0)
    propList.insert("draw:z-index", pos.order());
  if (pos.naturalSize().x() > 4*pointFactor && pos.naturalSize().y() > 4*pointFactor) {
    propList.insert("libwpd:naturalWidth", pos.naturalSize().x(), pos.unit());
    propList.insert("libwpd:naturalHeight", pos.naturalSize().y(), pos.unit());
  }
  Vec2f TLClip = (1.f/pointFactor)*pos.leftTopClipping();
  Vec2f RBClip = (1.f/pointFactor)*pos.rightBottomClipping();
  if (TLClip[0] > 0 || TLClip[1] > 0 || RBClip[0] > 0 || RBClip[1] > 0) {
    // in ODF1.2 we need to separate the value with ,
    std::stringstream s;
    s << "rect(" << TLClip[1] << "pt " << RBClip[0] << "pt "
      <<  RBClip[1] << "pt " << TLClip[0] << "pt)";
    propList.insert("fo:clip", s.str().c_str());
  }
  double newPosition;

  if ( pos.m_wrapping ==  MWAWPosition::WDynamic)
    propList.insert( "style:wrap", "dynamic" );
  else if ( pos.m_wrapping ==  MWAWPosition::WBackground) {
    propList.insert( "style:wrap", "run-through" );
    propList.insert( "style:run-through", "background" );
  } else if ( pos.m_wrapping ==  MWAWPosition::WRunThrough)
    propList.insert( "style:wrap", "run-through" );
  else
    propList.insert( "style:wrap", "none" );

  if (pos.m_anchorTo == MWAWPosition::Paragraph ||
      pos.m_anchorTo == MWAWPosition::Frame) {
    std::string what= pos.m_anchorTo == MWAWPosition::Paragraph ?
                      "paragraph" : "frame";
    propList.insert("text:anchor-type", what.c_str());
    propList.insert("style:vertical-rel", what.c_str());
    propList.insert("style:horizontal-rel", what.c_str());
    double w = m_ps->m_pageFormWidth - m_ps->m_pageMarginLeft
               - m_ps->m_pageMarginRight - m_ps->m_paragraph.getMarginsWidth();
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
       pos.m_anchorTo != MWAWPosition::CharBaseLine &&
       pos.m_anchorTo != MWAWPosition::Unknown) return;

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

bool MWAWContentListener::isSubDocumentOpened(libmwaw::SubDocumentType &subdocType) const
{
  if (!m_ps->m_inSubDocument)
    return false;
  subdocType = m_ps->m_subDocumentType;
  return true;
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

  m_ps->m_paragraph.m_listLevelIndex=0;
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

  if (h > 0)
    propList.insert("style:row-height", h, unit);
  else if (h < 0)
    propList.insert("style:min-row-height", -h, unit);
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

void MWAWContentListener::addEmptyTableCell(Vec2i const &pos, Vec2i span)
{
  if (!m_ps->m_isTableRowOpened) {
    MWAW_DEBUG_MSG(("MWAWContentListener::addEmptyTableCell: called with m_isTableRowOpened=false\n"));
    return;
  }
  if (m_ps->m_isTableCellOpened) {
    MWAW_DEBUG_MSG(("MWAWContentListener::addEmptyTableCell: called with m_isTableCellOpened=true\n"));
    closeTableCell();
  }
  WPXPropertyList propList;
  propList.insert("libwpd:column", pos[0]);
  propList.insert("libwpd:row", pos[1]);
  propList.insert("table:number-columns-spanned", span[0]);
  propList.insert("table:number-rows-spanned", span[1]);
  m_documentInterface->openTableCell(propList);
  m_documentInterface->closeTableCell();
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
  if (!cell.backgroundColor().isWhite())
    propList.insert("fo:background-color", cell.backgroundColor().str().c_str());
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
    MWAW_DEBUG_MSG(("MWAWContentListener::openTableCell: called with unknown halign=%d\n", cell.hAlignement()));
  }
  // alignement
  switch(cell.vAlignement()) {
  case MWAWCell::VALIGN_TOP:
    propList.insert("style:vertical-align", "top");
    break;
  case MWAWCell::VALIGN_CENTER:
    propList.insert("style:vertical-align", "middle");
    break;
  case MWAWCell::VALIGN_BOTTOM:
    propList.insert("style:vertical-align", "bottom");
    break;
  case MWAWCell::VALIGN_DEFAULT:
    break; // default
  default:
    MWAW_DEBUG_MSG(("MWAWContentListener::openTableCell: called with unknown valign=%d\n", cell.vAlignement()));
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
  m_ps->m_paragraph.m_listLevelIndex=0;
  _changeList(); // flush the list exterior

  m_ps->m_isTableCellOpened = false;
  m_documentInterface->closeTableCell();
}

///////////////////
// others
///////////////////

// ---------- state stack ------------------
shared_ptr<MWAWContentListenerInternal::State> MWAWContentListener::_pushParsingState()
{
  shared_ptr<MWAWContentListenerInternal::State> actual = m_ps;
  m_psStack.push_back(actual);
  m_ps.reset(new MWAWContentListenerInternal::State);

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
