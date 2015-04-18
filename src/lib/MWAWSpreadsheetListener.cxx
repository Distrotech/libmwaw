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

/** \file MWAWSpreadsheetListener.cxx
 * Implements MWAWSpreadsheetListener: the libmwaw spreadsheet processor listener
 *
 * \note this class is the only class which does the interface with
 * the librevenge::RVNGSpreadsheetInterface
 */

#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <time.h>

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"

#include "MWAWCell.hxx"
#include "MWAWChart.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWGraphicListener.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWGraphicShape.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWList.hxx"
#include "MWAWPageSpan.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWParser.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSection.hxx"
#include "MWAWSubDocument.hxx"
#include "MWAWTable.hxx"

#include "MWAWSpreadsheetListener.hxx"

//! Internal and low level namespace to define the states of MWAWSpreadsheetListener
namespace MWAWSpreadsheetListenerInternal
{
//! a enum to define basic break bit
enum { PageBreakBit=0x1, ColumnBreakBit=0x2 };
//! a class to store the document state of a MWAWSpreadsheetListener
struct DocumentState {
  //! constructor
  DocumentState(std::vector<MWAWPageSpan> const &pageList) :
    m_pageList(pageList), m_pageSpan(), m_metaData(), m_footNoteNumber(0), m_smallPictureNumber(0),
    m_isDocumentStarted(false), m_isSheetOpened(false), m_isSheetRowOpened(false),
    m_sentListMarkers(), m_numberingIdMap(),
    m_subDocuments()
  {
  }
  //! destructor
  ~DocumentState()
  {
  }

  //! the pages definition
  std::vector<MWAWPageSpan> m_pageList;
  //! the current page span
  MWAWPageSpan m_pageSpan;
  //! the document meta data
  librevenge::RVNGPropertyList m_metaData;

  int m_footNoteNumber /** footnote number*/;

  int m_smallPictureNumber /** number of small picture */;
  bool m_isDocumentStarted /** a flag to know if the document is open */;
  bool m_isSheetOpened /** a flag to know if a sheet is open */;
  bool m_isSheetRowOpened /** a flag to know if a row is open */;
  /// the list of marker corresponding to sent list
  std::vector<int> m_sentListMarkers;
  /** a map cell's format to id */
  std::map<MWAWCell::Format,int,MWAWCell::CompareFormat> m_numberingIdMap;
  std::vector<MWAWSubDocumentPtr> m_subDocuments; /** list of document actually open */

private:
  DocumentState(const DocumentState &);
  DocumentState &operator=(const DocumentState &);
};

/** the state of a MWAWSpreadsheetListener */
struct State {
  //! constructor
  State();
  //! destructor
  ~State() { }
  //! returns true if we are in a text zone
  bool canWriteText() const
  {
    if (m_isSheetCellOpened || m_isHeaderFooterOpened) return true;
    return m_isTextboxOpened || m_isTableCellOpened || m_isNote;
  }

  //! a buffer to stored the text
  librevenge::RVNGString m_textBuffer;
  //! the number of tabs to add
  int m_numDeferredTabs;

  //! the font
  MWAWFont m_font;
  //! the paragraph
  MWAWParagraph m_paragraph;

  shared_ptr<MWAWList> m_list;

  bool m_isPageSpanOpened;
  bool m_isHeaderFooterOpened /** a flag to know if the header footer is started */;
  bool m_isFrameOpened;
  bool m_isTextboxOpened;

  bool m_isHeaderFooterWithoutParagraph;

  bool m_isSpanOpened;
  bool m_isParagraphOpened;
  bool m_isListElementOpened;

  bool m_firstParagraphInPageSpan;

  bool m_isSheetColumnOpened;
  bool m_isSheetCellOpened;

  bool m_isTableOpened;
  bool m_isTableRowOpened;
  bool m_isTableColumnOpened;
  bool m_isTableCellOpened;

  unsigned m_currentPage;
  int m_numPagesRemainingInSpan;
  int m_currentPageNumber;

  std::vector<bool> m_listOrderedLevels; //! a stack used to know what is open

  bool m_inSubDocument;

  bool m_isNote;
  bool m_inLink;
  libmwaw::SubDocumentType m_subDocumentType;

private:
  State(const State &);
  State &operator=(const State &);
};

State::State() :
  m_textBuffer(""), m_numDeferredTabs(0),

  m_font(20,12), // default time 12

  m_paragraph(),

  m_list(),

  m_isPageSpanOpened(false), m_isHeaderFooterOpened(false),
  m_isFrameOpened(false), m_isTextboxOpened(false),
  m_isHeaderFooterWithoutParagraph(false),

  m_isSpanOpened(false), m_isParagraphOpened(false), m_isListElementOpened(false),

  m_firstParagraphInPageSpan(true),

  m_isSheetColumnOpened(false),
  m_isSheetCellOpened(false),

  m_isTableOpened(false), m_isTableRowOpened(false), m_isTableColumnOpened(false),
  m_isTableCellOpened(false),

  m_currentPage(0), m_numPagesRemainingInSpan(0), m_currentPageNumber(1),

  m_listOrderedLevels(),

  m_inSubDocument(false),
  m_isNote(false), m_inLink(false),
  m_subDocumentType(libmwaw::DOC_NONE)
{
}
}

MWAWSpreadsheetListener::MWAWSpreadsheetListener(MWAWParserState &parserState, std::vector<MWAWPageSpan> const &pageList, librevenge::RVNGSpreadsheetInterface *documentInterface) : MWAWListener(),
  m_ds(new MWAWSpreadsheetListenerInternal::DocumentState(pageList)), m_ps(new MWAWSpreadsheetListenerInternal::State), m_psStack(),
  m_parserState(parserState), m_documentInterface(documentInterface)
{
}

MWAWSpreadsheetListener::MWAWSpreadsheetListener(MWAWParserState &parserState, MWAWBox2f const &box, librevenge::RVNGSpreadsheetInterface *documentInterface) : MWAWListener(),
  m_ds(), m_ps(new MWAWSpreadsheetListenerInternal::State), m_psStack(), m_parserState(parserState), m_documentInterface(documentInterface)
{
  MWAWPageSpan pageSpan;
  pageSpan.setMargins(0);
  pageSpan.setPageSpan(1);
  pageSpan.setFormWidth(box.size().x()/72.);
  pageSpan.setFormLength(box.size().y()/72.);
  m_ds.reset(new MWAWSpreadsheetListenerInternal::DocumentState(std::vector<MWAWPageSpan>(1, pageSpan)));
}


MWAWSpreadsheetListener::~MWAWSpreadsheetListener()
{
}

///////////////////
// text data
///////////////////
bool MWAWSpreadsheetListener::canWriteText() const
{
  return m_ps->canWriteText();
}

void MWAWSpreadsheetListener::insertChar(uint8_t character)
{
  if (!m_ps->canWriteText()) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertChar: called outside a text zone\n"));
    return;
  }
  if (character >= 0x80) {
    MWAWSpreadsheetListener::insertUnicode(character);
    return;
  }
  _flushDeferredTabs();
  if (!m_ps->m_isSpanOpened) _openSpan();
  m_ps->m_textBuffer.append((char) character);
}

void MWAWSpreadsheetListener::insertCharacter(unsigned char c)
{
  if (!m_ps->canWriteText()) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertCharacter: called outside a text zone\n"));
    return;
  }
  int unicode = m_parserState.m_fontConverter->unicode(m_ps->m_font.id(), c);
  if (unicode == -1) {
    if (c < 0x20) {
      MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertCharacter: Find odd char %x\n", (unsigned int)c));
    }
    else
      MWAWSpreadsheetListener::insertChar((uint8_t) c);
  }
  else
    MWAWSpreadsheetListener::insertUnicode((uint32_t) unicode);
}

int MWAWSpreadsheetListener::insertCharacter(unsigned char c, MWAWInputStreamPtr &input, long endPos)
{
  if (!m_ps->canWriteText()) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertCharacter: called outside a text zone\n"));
    return 0;
  }
  if (!input || !m_parserState.m_fontConverter) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertCharacter: input or font converter does not exist!!!!\n"));
    return 0;
  }
  long debPos=input->tell();
  int fId = m_ps->m_font.id();
  int unicode = endPos==debPos ?
                m_parserState.m_fontConverter->unicode(fId, c) :
                m_parserState.m_fontConverter->unicode(fId, c, input);

  long pos=input->tell();
  if (endPos > 0 && pos > endPos) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertCharacter: problem reading a character\n"));
    pos = debPos;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    unicode = m_parserState.m_fontConverter->unicode(fId, c);
  }
  if (unicode == -1) {
    if (c < 0x20) {
      MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertCharacter: Find odd char %x\n", (unsigned int)c));
    }
    else
      MWAWSpreadsheetListener::insertChar((uint8_t) c);
  }
  else
    MWAWSpreadsheetListener::insertUnicode((uint32_t) unicode);

  return int(pos-debPos);
}

void MWAWSpreadsheetListener::insertUnicode(uint32_t val)
{
  if (!m_ps->canWriteText()) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertUnicode: called outside a text zone\n"));
    return;
  }

  // undef character, we skip it
  if (val == 0xfffd) return;

  _flushDeferredTabs();
  if (!m_ps->m_isSpanOpened) _openSpan();
  libmwaw::appendUnicode(val, m_ps->m_textBuffer);
}

void MWAWSpreadsheetListener::insertUnicodeString(librevenge::RVNGString const &str)
{
  if (!m_ps->canWriteText()) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertUnicodeString: called outside a text zone\n"));
    return;
  }

  _flushDeferredTabs();
  if (!m_ps->m_isSpanOpened) _openSpan();
  m_ps->m_textBuffer.append(str);
}

void MWAWSpreadsheetListener::insertEOL(bool soft)
{
  if (!m_ps->canWriteText()) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertEOL: called outside a text zone\n"));
    return;
  }

  if (!m_ps->m_isParagraphOpened && !m_ps->m_isListElementOpened)
    _openSpan();
  _flushDeferredTabs();

  if (soft) {
    if (m_ps->m_isSpanOpened)
      _flushText();
    m_documentInterface->insertLineBreak();
  }
  else if (m_ps->m_isParagraphOpened)
    _closeParagraph();

  // sub/superscript must not survive a new line
  m_ps->m_font.set(MWAWFont::Script());
}

void MWAWSpreadsheetListener::insertTab()
{
  if (!m_ps->canWriteText()) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertTab: called outside a text zone\n"));
    return;
  }

  if (!m_ps->m_isParagraphOpened) {
    m_ps->m_numDeferredTabs++;
    return;
  }
  if (m_ps->m_isSpanOpened) _flushText();
  m_ps->m_numDeferredTabs++;
  _flushDeferredTabs();
}

void MWAWSpreadsheetListener::insertBreak(MWAWSpreadsheetListener::BreakType)
{
  MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertBreak: make not sense\n"));
  return;
}

///////////////////
// font/paragraph function
///////////////////
void MWAWSpreadsheetListener::setFont(MWAWFont const &font)
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

MWAWFont const &MWAWSpreadsheetListener::getFont() const
{
  return m_ps->m_font;
}

bool MWAWSpreadsheetListener::isParagraphOpened() const
{
  return m_ps->m_isParagraphOpened;
}

void MWAWSpreadsheetListener::setParagraph(MWAWParagraph const &para)
{
  if (para==m_ps->m_paragraph) return;

  m_ps->m_paragraph=para;
}

MWAWParagraph const &MWAWSpreadsheetListener::getParagraph() const
{
  return m_ps->m_paragraph;
}

///////////////////
// field/link :
///////////////////
void MWAWSpreadsheetListener::insertField(MWAWField const &field)
{
  if (!m_ps->canWriteText()) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertField: called outside a text zone\n"));
    return;
  }

  switch (field.m_type) {
  case MWAWField::None:
    break;
  case MWAWField::PageCount:
  case MWAWField::PageNumber:
  case MWAWField::Title: {
    _flushDeferredTabs();
    _flushText();
    _openSpan();
    librevenge::RVNGPropertyList propList;
    if (field.m_type==MWAWField::Title) {
      propList.insert("librevenge:field-type", "text:title");
      m_documentInterface->insertField(propList);
    }
    else {
      propList.insert("style:num-format", libmwaw::numberingTypeToString(field.m_numberingType).c_str());
      if (field.m_type == MWAWField::PageNumber) {
        propList.insert("librevenge:field-type", "text:page-number");
        m_documentInterface->insertField(propList);
      }
      else {
        propList.insert("librevenge:field-type", "text:page-count");
        m_documentInterface->insertField(propList);
      }
    }
    break;
  }
  case MWAWField::Database:
    if (field.m_data.length())
      MWAWSpreadsheetListener::insertUnicodeString(field.m_data.c_str());
    else
      MWAWSpreadsheetListener::insertUnicodeString("#DATAFIELD#");
    break;
  case MWAWField::Date:
  case MWAWField::Time: {
    std::string format(field.m_DTFormat);
    if (format.length()==0) {
      if (field.m_type==MWAWField::Date)
        format="%m/%d/%y";
      else
        format="%I:%M:%S %p";
    }
    time_t now = time(0L);
    struct tm timeinfo;
    if (localtime_r(&now, &timeinfo)) {
      char buf[256];
      strftime(buf, 256, format.c_str(), &timeinfo);
      MWAWSpreadsheetListener::insertUnicodeString(librevenge::RVNGString(buf));
    }
    break;
  }
  default:
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertField: must not be called with type=%d\n", int(field.m_type)));
    break;
  }
}

void MWAWSpreadsheetListener::openLink(MWAWLink const &link)
{
  if (!m_ps->canWriteText()) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openLink: called outside a text zone\n"));
    return;
  }
  if (m_ps->m_inLink) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener:openLink: a link is already opened\n"));
    return;
  }
  if (!m_ps->m_isSpanOpened) _openSpan();
  librevenge::RVNGPropertyList propList;
  link.addTo(propList);
  m_documentInterface->openLink(propList);
  _pushParsingState();
  m_ps->m_inLink=true;
// we do not want any close open paragraph in a link
  m_ps->m_isParagraphOpened=true;
}

void MWAWSpreadsheetListener::closeLink()
{
  if (!m_ps->m_inLink) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener:closeLink: can not close a link\n"));
    return;
  }
  if (m_ps->m_isSpanOpened) _closeSpan();
  m_documentInterface->closeLink();
  _popParsingState();
}

///////////////////
// document
///////////////////
void MWAWSpreadsheetListener::setDocumentLanguage(std::string locale)
{
  if (!locale.length()) return;
  m_ds->m_metaData.insert("librevenge:language", locale.c_str());
}

bool MWAWSpreadsheetListener::isDocumentStarted() const
{
  return m_ds->m_isDocumentStarted;
}

void MWAWSpreadsheetListener::startDocument()
{
  if (m_ds->m_isDocumentStarted) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::startDocument: the document is already started\n"));
    return;
  }

  m_documentInterface->startDocument(librevenge::RVNGPropertyList());
  m_ds->m_isDocumentStarted = true;

  m_documentInterface->setDocumentMetaData(m_ds->m_metaData);
}

void MWAWSpreadsheetListener::endDocument(bool sendDelayedSubDoc)
{
  if (!m_ds->m_isDocumentStarted) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::endDocument: the document is not started\n"));
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
  if (m_ds->m_isSheetOpened)
    closeSheet();

  _closePageSpan();
  m_documentInterface->endDocument();
  m_ds->m_isDocumentStarted = false;
}

///////////////////
// page
///////////////////
bool MWAWSpreadsheetListener::isPageSpanOpened() const
{
  return m_ps->m_isPageSpanOpened;
}

MWAWPageSpan const &MWAWSpreadsheetListener::getPageSpan()
{
  if (!m_ps->m_isPageSpanOpened)
    _openPageSpan();
  return m_ds->m_pageSpan;
}


void MWAWSpreadsheetListener::_openPageSpan(bool sendHeaderFooters)
{
  if (m_ps->m_isPageSpanOpened)
    return;

  if (!m_ds->m_isDocumentStarted)
    startDocument();

  if (m_ds->m_pageList.size()==0) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::_openPageSpan: can not find any page\n"));
    throw libmwaw::ParseException();
  }
  unsigned actPage = 0;
  std::vector<MWAWPageSpan>::iterator it = m_ds->m_pageList.begin();
  ++m_ps->m_currentPage;
  while (true) {
    actPage+=(unsigned)it->getPageSpan();
    if (actPage >= m_ps->m_currentPage)
      break;
    if (++it == m_ds->m_pageList.end()) {
      MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::_openPageSpan: can not find current page, use the previous one\n"));
      --it;
      break;
    }
  }
  MWAWPageSpan &currentPage = *it;

  librevenge::RVNGPropertyList propList;
  currentPage.getPageProperty(propList);
  propList.insert("librevenge:is-last-page-span", ++it==m_ds->m_pageList.end());

  if (!m_ps->m_isPageSpanOpened)
    m_documentInterface->openPageSpan(propList);

  m_ps->m_isPageSpanOpened = true;
  m_ds->m_pageSpan = currentPage;

  // we insert the header footer
  if (sendHeaderFooters)
    currentPage.sendHeaderFooters(this);

  // first paragraph in span (necessary for resetting page number)
  m_ps->m_firstParagraphInPageSpan = true;
  m_ps->m_numPagesRemainingInSpan = (currentPage.getPageSpan() - 1);
}

void MWAWSpreadsheetListener::_closePageSpan()
{
  if (!m_ps->m_isPageSpanOpened)
    return;

  m_documentInterface->closePageSpan();
  m_ps->m_isPageSpanOpened = false;
}

///////////////////
// header/footer
///////////////////
bool MWAWSpreadsheetListener::isHeaderFooterOpened() const
{
  return m_ps->m_isHeaderFooterOpened;
}

bool MWAWSpreadsheetListener::insertHeader(MWAWSubDocumentPtr subDocument, librevenge::RVNGPropertyList const &extras)
{
  if (m_ps->m_isHeaderFooterOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertHeader: Oops a header/footer is already opened\n"));
    return false;
  }
  librevenge::RVNGPropertyList propList(extras);
  m_documentInterface->openHeader(propList);
  handleSubDocument(subDocument, libmwaw::DOC_HEADER_FOOTER);
  m_documentInterface->closeHeader();
  return true;
}

bool MWAWSpreadsheetListener::insertFooter(MWAWSubDocumentPtr subDocument, librevenge::RVNGPropertyList const &extras)
{
  if (m_ps->m_isHeaderFooterOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertFooter: Oops a header/footer is already opened\n"));
    return false;
  }
  librevenge::RVNGPropertyList propList(extras);
  m_documentInterface->openFooter(propList);
  handleSubDocument(subDocument, libmwaw::DOC_HEADER_FOOTER);
  m_documentInterface->closeFooter();
  return true;
}

///////////////////
// section
///////////////////
MWAWSection const &MWAWSpreadsheetListener::getSection() const
{
  MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::getSection: make no sense\n"));
  static MWAWSection const badSection;
  return badSection;
}

bool MWAWSpreadsheetListener::openSection(MWAWSection const &)
{
  MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openSection: make no sense\n"));
  return false;
}

bool MWAWSpreadsheetListener::closeSection()
{
  MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::closeSection: make no sense\n"));
  return false;
}

///////////////////
// paragraph
///////////////////
void MWAWSpreadsheetListener::_openParagraph()
{
  if (!m_ps->canWriteText())
    return;

  if (m_ps->m_isParagraphOpened || m_ps->m_isListElementOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::_openParagraph: a paragraph (or a list) is already opened"));
    return;
  }

  librevenge::RVNGPropertyList propList;
  m_ps->m_paragraph.addTo(propList, false);
  if (!m_ps->m_isParagraphOpened)
    m_documentInterface->openParagraph(propList);

  _resetParagraphState();
  m_ps->m_firstParagraphInPageSpan = false;
}

void MWAWSpreadsheetListener::_closeParagraph()
{
  // we can not close a paragraph in a link
  if (m_ps->m_inLink)
    return;
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
}

void MWAWSpreadsheetListener::_resetParagraphState(const bool isListElement)
{
  m_ps->m_isListElementOpened = isListElement;
  m_ps->m_isParagraphOpened = true;
  m_ps->m_isHeaderFooterWithoutParagraph = false;
}

///////////////////
// list
///////////////////
void MWAWSpreadsheetListener::_openListElement()
{
  if (!m_ps->canWriteText())
    return;

  if (m_ps->m_isParagraphOpened || m_ps->m_isListElementOpened)
    return;

  librevenge::RVNGPropertyList propList;
  m_ps->m_paragraph.addTo(propList, false);
  // check if we must change the start value
  int startValue=m_ps->m_paragraph.m_listStartValue.get();
  if (startValue > 0 && m_ps->m_list && m_ps->m_list->getStartValueForNextElement() != startValue) {
    propList.insert("text:start-value", startValue);
    m_ps->m_list->setStartValueForNextElement(startValue);
  }

  if (m_ps->m_list) m_ps->m_list->openElement();
  m_documentInterface->openListElement(propList);
  _resetParagraphState(true);
}

void MWAWSpreadsheetListener::_closeListElement()
{
  if (m_ps->m_isListElementOpened) {
    if (m_ps->m_isSpanOpened)
      _closeSpan();

    if (m_ps->m_list) m_ps->m_list->closeElement();
    m_documentInterface->closeListElement();
  }

  m_ps->m_isListElementOpened = m_ps->m_isParagraphOpened = false;
}

int MWAWSpreadsheetListener::_getListId() const
{
  size_t newLevel= (size_t) m_ps->m_paragraph.m_listLevelIndex.get();
  if (newLevel == 0) return -1;
  int newListId = m_ps->m_paragraph.m_listId.get();
  if (newListId > 0) return newListId;
  static bool first = true;
  if (first) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::_getListId: the list id is not set, try to find a new one\n"));
    first = false;
  }
  shared_ptr<MWAWList> list=m_parserState.m_listManager->getNewList
                            (m_ps->m_list, int(newLevel), *m_ps->m_paragraph.m_listLevel);
  if (!list) return -1;
  return list->getId();
}

void MWAWSpreadsheetListener::_changeList()
{
  if (!m_ps->canWriteText())
    return;

  if (m_ps->m_isParagraphOpened)
    _closeParagraph();

  size_t actualLevel = m_ps->m_listOrderedLevels.size();
  size_t newLevel= (size_t) m_ps->m_paragraph.m_listLevelIndex.get();
  int newListId = newLevel>0 ? _getListId() : -1;
  bool changeList = newLevel &&
                    (m_ps->m_list && m_ps->m_list->getId()!=newListId);
  size_t minLevel = changeList ? 0 : newLevel;
  while (actualLevel > minLevel) {
    if (m_ps->m_listOrderedLevels[--actualLevel])
      m_documentInterface->closeOrderedListLevel();
    else
      m_documentInterface->closeUnorderedListLevel();
  }

  if (newLevel) {
    shared_ptr<MWAWList> theList;

    theList=m_parserState.m_listManager->getList(newListId);
    if (!theList) {
      MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::_changeList: can not find any list\n"));
      m_ps->m_listOrderedLevels.resize(actualLevel);
      return;
    }
    m_parserState.m_listManager->needToSend(newListId, m_ds->m_sentListMarkers);
    m_ps->m_list = theList;
    m_ps->m_list->setLevel((int)newLevel);
  }

  m_ps->m_listOrderedLevels.resize(newLevel, false);
  if (actualLevel == newLevel) return;

  for (size_t i=actualLevel+1; i<= newLevel; i++) {
    bool ordered = m_ps->m_list->isNumeric(int(i));
    m_ps->m_listOrderedLevels[i-1] = ordered;

    librevenge::RVNGPropertyList level;
    m_ps->m_list->addTo(int(i), level);
    if (ordered)
      m_documentInterface->openOrderedListLevel(level);
    else
      m_documentInterface->openUnorderedListLevel(level);
  }
}

///////////////////
// span
///////////////////
void MWAWSpreadsheetListener::_openSpan()
{
  if (m_ps->m_isSpanOpened || !m_ps->canWriteText())
    return;

  if (!m_ps->m_isParagraphOpened && !m_ps->m_isListElementOpened) {
    _changeList();
    if (*m_ps->m_paragraph.m_listLevelIndex == 0)
      _openParagraph();
    else
      _openListElement();
  }

  librevenge::RVNGPropertyList propList;
  m_ps->m_font.addTo(propList, m_parserState.m_fontConverter);

  m_documentInterface->openSpan(propList);

  m_ps->m_isSpanOpened = true;
}

void MWAWSpreadsheetListener::_closeSpan()
{
  // better not to close a link...
  if (!m_ps->m_isSpanOpened)
    return;

  _flushText();
  m_documentInterface->closeSpan();
  m_ps->m_isSpanOpened = false;
}

///////////////////
// text (send data)
///////////////////
void MWAWSpreadsheetListener::_flushDeferredTabs()
{
  if (m_ps->m_numDeferredTabs == 0 || !m_ps->canWriteText())
    return;
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

void MWAWSpreadsheetListener::_flushText()
{
  if (m_ps->m_textBuffer.len() == 0  || !m_ps->canWriteText()) return;

  // when some many ' ' follows each other, call insertSpace
  librevenge::RVNGString tmpText;
  int numConsecutiveSpaces = 0;
  librevenge::RVNGString::Iter i(m_ps->m_textBuffer);
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
    }
    else
      tmpText.append(i());
  }
  m_documentInterface->insertText(tmpText);
  m_ps->m_textBuffer.clear();
}

///////////////////
// Note/Comment/picture/textbox
///////////////////
void MWAWSpreadsheetListener::insertNote(MWAWNote const &note, MWAWSubDocumentPtr &subDocument)
{
  if (m_ps->m_isNote) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertNote try to insert a note recursively (ignored)\n"));
    return;
  }
  if (!canWriteText()) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertNote called outside a text zone (ignored)\n"));
    return;
  }
  m_ps->m_isNote = true;
  if (m_ps->m_isHeaderFooterOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertNote try to insert a note in a header/footer\n"));
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
  }
  else {
    if (!m_ps->m_isParagraphOpened)
      _openParagraph();
    else {
      _flushText();
      _closeSpan();
    }

    librevenge::RVNGPropertyList propList;
    if (note.m_label.len())
      propList.insert("text:label", librevenge::RVNGPropertyFactory::newStringProp(note.m_label));
    if (note.m_type == MWAWNote::FootNote) {
      if (note.m_number >= 0)
        m_ds->m_footNoteNumber = note.m_number;
      else
        m_ds->m_footNoteNumber++;
      propList.insert("librevenge:number", m_ds->m_footNoteNumber);
      m_documentInterface->openFootnote(propList);
      handleSubDocument(subDocument, libmwaw::DOC_NOTE);
      m_documentInterface->closeFootnote();
    }
    else {
      MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertNote try to insert a unexpected note\n"));
    }
  }
  m_ps->m_isNote = false;
}

void MWAWSpreadsheetListener::insertComment(MWAWSubDocumentPtr &subDocument)
{
  if (m_ps->m_isNote) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertComment try to insert a comment in a note (ignored)\n"));
    return;
  }
  if (!canWriteText()) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertComment called outside a text zone (ignored)\n"));
    return;
  }

  if (!m_ps->m_isSheetCellOpened) {
    if (!m_ps->m_isParagraphOpened)
      _openParagraph();
    else {
      _flushText();
      _closeSpan();
    }
  }
  else if (m_ps->m_isParagraphOpened)
    _closeParagraph();

  librevenge::RVNGPropertyList propList;
  m_documentInterface->openComment(propList);

  m_ps->m_isNote = true;
  handleSubDocument(subDocument, libmwaw::DOC_COMMENT_ANNOTATION);

  m_documentInterface->closeComment();
  m_ps->m_isNote = false;
}

void MWAWSpreadsheetListener::insertTextBox
(MWAWPosition const &pos, MWAWSubDocumentPtr subDocument, MWAWGraphicStyle const &frameStyle)
{
  if (!m_ds->m_isSheetOpened || m_ds->m_isSheetRowOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertTextBox insert a textbox outside a sheet is not implemented\n"));
    return;
  }
  if (!openFrame(pos, frameStyle)) return;

  librevenge::RVNGPropertyList propList;
  if (!frameStyle.m_frameNextName.empty())
    propList.insert("librevenge:next-frame-name",frameStyle.m_frameNextName.c_str());
  m_documentInterface->openTextBox(propList);
  handleSubDocument(subDocument, libmwaw::DOC_TEXT_BOX);
  m_documentInterface->closeTextBox();

  closeFrame();
}

void MWAWSpreadsheetListener::insertPicture
(MWAWPosition const &pos, MWAWGraphicShape const &shape, MWAWGraphicStyle const &style)
{
  if (!m_ds->m_isSheetOpened || m_ds->m_isSheetRowOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertPicture insert a picture outside a sheet is not implemented\n"));
    return;
  }
  // sanity check: avoid to send to many small pict
  float factor=pos.getScaleFactor(pos.unit(), librevenge::RVNG_POINT);
  if (pos.size()[0]*factor <= 8 && pos.size()[1]*factor <= 8 && m_ds->m_smallPictureNumber++ > 200) {
    static bool first = true;
    if (first) {
      first = false;
      MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertPicture: find too much small pictures, skip them from now\n"));
    }
    return;
  }

  // now check that the anchor is coherent with the actual state
  switch (pos.m_anchorTo) {
  case MWAWPosition::Page:
    break;
  case MWAWPosition::Paragraph:
    if (m_ps->m_isParagraphOpened)
      _flushText();
    else
      _openParagraph();
    break;
  case MWAWPosition::Unknown:
  default:
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertPicture: UNKNOWN position, insert as char position\n"));
  // fallthrough intended
  case MWAWPosition::CharBaseLine:
  case MWAWPosition::Char:
    if (m_ps->m_isSpanOpened)
      _flushText();
    else
      _openSpan();
    break;
  case MWAWPosition::Frame:
    break;
  }

  librevenge::RVNGPropertyList shapePList;
  _handleFrameParameters(shapePList, pos);
  shapePList.remove("svg:x");
  shapePList.remove("svg:y");

  librevenge::RVNGPropertyList list;
  style.addTo(list, shape.getType()==MWAWGraphicShape::Line);

  MWAWVec2f decal = factor*pos.origin();
  switch (shape.addTo(decal, style.hasSurface(), shapePList)) {
  case MWAWGraphicShape::C_Ellipse:
    m_documentInterface->defineGraphicStyle(list);
    m_documentInterface->drawEllipse(shapePList);
    break;
  case MWAWGraphicShape::C_Path:
    m_documentInterface->defineGraphicStyle(list);
    m_documentInterface->drawPath(shapePList);
    break;
  case MWAWGraphicShape::C_Polyline:
    m_documentInterface->defineGraphicStyle(list);
    m_documentInterface->drawPolyline(shapePList);
    break;
  case MWAWGraphicShape::C_Polygon:
    m_documentInterface->defineGraphicStyle(list);
    m_documentInterface->drawPolygon(shapePList);
    break;
  case MWAWGraphicShape::C_Rectangle:
    m_documentInterface->defineGraphicStyle(list);
    m_documentInterface->drawRectangle(shapePList);
    break;
  case MWAWGraphicShape::C_Bad:
    break;
  default:
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertPicture: unexpected shape\n"));
    break;
  }
}

void MWAWSpreadsheetListener::insertPicture
(MWAWPosition const &pos, const librevenge::RVNGBinaryData &binaryData, std::string type,
 MWAWGraphicStyle const &style)
{
  if (!m_ds->m_isSheetOpened || m_ds->m_isSheetRowOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertPicture insert a picture outside a sheet is not implemented\n"));
    return;
  }
  // sanity check: avoid to send to many small pict
  float factor=pos.getScaleFactor(pos.unit(), librevenge::RVNG_POINT);
  if (pos.size()[0]*factor <= 8 && pos.size()[1]*factor <= 8 && m_ds->m_smallPictureNumber++ > 200) {
    static bool first = true;
    if (first) {
      first = false;
      MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertPicture: find too much small pictures, skip them from now\n"));
    }
    return;
  }
  if (!openFrame(pos, style)) return;

  librevenge::RVNGPropertyList propList;
  propList.insert("librevenge:mime-type", type.c_str());
  propList.insert("office:binary-data", binaryData);
  m_documentInterface->insertBinaryObject(propList);

  closeFrame();
}

///////////////////
// frame
///////////////////
bool MWAWSpreadsheetListener::openFrame(MWAWPosition const &pos, MWAWGraphicStyle const &style)
{
  if (!m_ds->m_isSheetOpened || m_ds->m_isSheetRowOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openFrame insert a frame outside a sheet is not implemented\n"));
    return false;
  }
  if (m_ps->m_isFrameOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openFrame: called but a frame is already opened\n"));
    return false;
  }
  MWAWPosition fPos(pos);
  switch (pos.m_anchorTo) {
  case MWAWPosition::Page:
    break;
  case MWAWPosition::Paragraph:
    if (m_ps->m_isParagraphOpened)
      _flushText();
    else
      _openParagraph();
    break;
  case MWAWPosition::Unknown:
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openFrame: UNKNOWN position, insert as char position\n"));
  // fallthrough intended
  case MWAWPosition::CharBaseLine:
  case MWAWPosition::Char:
    if (m_ps->m_isSpanOpened)
      _flushText();
    else
      _openSpan();
    break;
  case MWAWPosition::Frame:
    if (!m_ds->m_subDocuments.size()) {
      MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openFrame: can not determine the frame\n"));
      return false;
    }
    if (m_ps->m_subDocumentType==libmwaw::DOC_HEADER_FOOTER) {
      MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openFrame: called with Frame position in header footer, switch to paragraph\n"));
      if (m_ps->m_isParagraphOpened)
        _flushText();
      else
        _openParagraph();
      fPos.m_anchorTo=MWAWPosition::Paragraph;
    }
    break;
  default:
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openFrame: can not determine the anchor\n"));
    return false;
  }

  librevenge::RVNGPropertyList propList;
  style.addFrameTo(propList);
  if (!propList["draw:fill"])
    propList.insert("draw:fill","none");
  _handleFrameParameters(propList, fPos);
  m_documentInterface->openFrame(propList);

  m_ps->m_isFrameOpened = true;
  return true;
}

void MWAWSpreadsheetListener::closeFrame()
{
  if (!m_ps->m_isFrameOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::closeFrame: called but no frame is already opened\n"));
    return;
  }
  m_documentInterface->closeFrame();
  m_ps->m_isFrameOpened = false;
}

void MWAWSpreadsheetListener::_handleFrameParameters
(librevenge::RVNGPropertyList &propList, MWAWPosition const &pos)
{
  MWAWVec2f origin = pos.origin();
  librevenge::RVNGUnit unit = pos.unit();
  float inchFactor=pos.getInvUnitScale(librevenge::RVNG_INCH);
  float pointFactor = pos.getInvUnitScale(librevenge::RVNG_POINT);

  if (pos.size()[0]>0)
    propList.insert("svg:width", double(pos.size()[0]), unit);
  else if (pos.size()[0]<0)
    propList.insert("fo:min-width", double(-pos.size()[0]), unit);
  if (pos.size()[1]>0)
    propList.insert("svg:height", double(pos.size()[1]), unit);
  else if (pos.size()[1]<0)
    propList.insert("fo:min-height", double(-pos.size()[1]), unit);
  if (pos.order() > 0)
    propList.insert("draw:z-index", pos.order());
  if (pos.naturalSize().x() > 4*pointFactor && pos.naturalSize().y() > 4*pointFactor) {
    propList.insert("librevenge:naturalWidth", pos.naturalSize().x(), pos.unit());
    propList.insert("librevenge:naturalHeight", pos.naturalSize().y(), pos.unit());
  }
  MWAWVec2f TLClip = (1.f/pointFactor)*pos.leftTopClipping();
  MWAWVec2f RBClip = (1.f/pointFactor)*pos.rightBottomClipping();
  if (TLClip[0] > 0 || TLClip[1] > 0 || RBClip[0] > 0 || RBClip[1] > 0) {
    // in ODF1.2 we need to separate the value with ,
    std::stringstream s;
    s << "rect(" << TLClip[1] << "pt " << RBClip[0] << "pt "
      <<  RBClip[1] << "pt " << TLClip[0] << "pt)";
    propList.insert("fo:clip", s.str().c_str());
  }

  if (pos.m_wrapping ==  MWAWPosition::WDynamic)
    propList.insert("style:wrap", "dynamic");
  else if (pos.m_wrapping ==  MWAWPosition::WBackground) {
    propList.insert("style:wrap", "run-through");
    propList.insert("style:run-through", "background");
  }
  else if (pos.m_wrapping ==  MWAWPosition::WForeground) {
    propList.insert("style:wrap", "run-through");
    propList.insert("style:run-through", "foreground");
  }
  else if (pos.m_wrapping ==  MWAWPosition::WRunThrough)
    propList.insert("style:wrap", "run-through");
  else
    propList.insert("style:wrap", "none");

  if (pos.m_anchorTo == MWAWPosition::Paragraph ||
      pos.m_anchorTo == MWAWPosition::Frame) {
    std::string what= pos.m_anchorTo == MWAWPosition::Paragraph ?
                      "paragraph" : "frame";
    propList.insert("text:anchor-type", what.c_str());
    propList.insert("style:vertical-rel", what.c_str());
    propList.insert("style:horizontal-rel", what.c_str());
    double w = m_ds->m_pageSpan.getPageWidth() - m_ps->m_paragraph.getMarginsWidth();
    w *= inchFactor;
    switch (pos.m_xPos) {
    case MWAWPosition::XRight:
      if (origin[0] < 0.0 || origin[0] > 0.0) {
        propList.insert("style:horizontal-pos", "from-left");
        propList.insert("svg:x", double(origin[0] - pos.size()[0] + w), unit);
      }
      else
        propList.insert("style:horizontal-pos", "right");
      break;
    case MWAWPosition::XCenter:
      if (origin[0] < 0.0 || origin[0] > 0.0) {
        propList.insert("style:horizontal-pos", "from-left");
        propList.insert("svg:x", double(origin[0] - pos.size()[0]/2.0 + w/2.0), unit);
      }
      else
        propList.insert("style:horizontal-pos", "center");
      break;
    case MWAWPosition::XLeft:
    case MWAWPosition::XFull:
    default:
      if (origin[0] < 0.0 || origin[0] > 0.0) {
        propList.insert("style:horizontal-pos", "from-left");
        propList.insert("svg:x", double(origin[0]), unit);
      }
      else
        propList.insert("style:horizontal-pos", "left");
      break;
    }

    if (origin[1] < 0.0 || origin[1] > 0.0) {
      propList.insert("style:vertical-pos", "from-top");
      propList.insert("svg:y", double(origin[1]), unit);
    }
    else
      propList.insert("style:vertical-pos", "top");
    return;
  }

  if (pos.m_anchorTo == MWAWPosition::Page) {
    // Page position seems to do not use the page margin...
    propList.insert("text:anchor-type", "page");
    if (pos.page() > 0) propList.insert("text:anchor-page-number", pos.page());
    double w = m_ds->m_pageSpan.getFormWidth();
    double h = m_ds->m_pageSpan.getFormLength();
    w *= inchFactor;
    h *= inchFactor;

    propList.insert("style:vertical-rel", "page");
    propList.insert("style:horizontal-rel", "page");
    double newPosition;
    switch (pos.m_yPos) {
    case MWAWPosition::YFull:
      propList.insert("svg:height", double(h), unit);
    // fallthrough intended
    case MWAWPosition::YTop:
      if (origin[1] < 0.0 || origin[1] > 0.0) {
        propList.insert("style:vertical-pos", "from-top");
        newPosition = origin[1];
        if (newPosition > h -pos.size()[1])
          newPosition = h - pos.size()[1];
        propList.insert("svg:y", double(newPosition), unit);
      }
      else
        propList.insert("style:vertical-pos", "top");
      break;
    case MWAWPosition::YCenter:
      if (origin[1] < 0.0 || origin[1] > 0.0) {
        propList.insert("style:vertical-pos", "from-top");
        newPosition = (h - pos.size()[1])/2.0;
        if (newPosition > h -pos.size()[1]) newPosition = h - pos.size()[1];
        propList.insert("svg:y", double(newPosition), unit);
      }
      else
        propList.insert("style:vertical-pos", "middle");
      break;
    case MWAWPosition::YBottom:
      if (origin[1] < 0.0 || origin[1] > 0.0) {
        propList.insert("style:vertical-pos", "from-top");
        newPosition = h - pos.size()[1]-origin[1];
        if (newPosition > h -pos.size()[1]) newPosition = h -pos.size()[1];
        else if (newPosition < 0) newPosition = 0;
        propList.insert("svg:y", double(newPosition), unit);
      }
      else
        propList.insert("style:vertical-pos", "bottom");
      break;
    default:
      break;
    }

    switch (pos.m_xPos) {
    case MWAWPosition::XFull:
      propList.insert("svg:width", double(w), unit);
    // fallthrough intended
    case MWAWPosition::XLeft:
      if (origin[0] < 0.0 || origin[0] > 0.0) {
        propList.insert("style:horizontal-pos", "from-left");
        propList.insert("svg:x", double(origin[0]), unit);
      }
      else
        propList.insert("style:horizontal-pos", "left");
      break;
    case MWAWPosition::XRight:
      if (origin[0] < 0.0 || origin[0] > 0.0) {
        propList.insert("style:horizontal-pos", "from-left");
        propList.insert("svg:x",double(w - pos.size()[0] + origin[0]), unit);
      }
      else
        propList.insert("style:horizontal-pos", "right");
      break;
    case MWAWPosition::XCenter:
      if (origin[0] < 0.0 || origin[0] > 0.0) {
        propList.insert("style:horizontal-pos", "from-left");
        propList.insert("svg:x", double((w - pos.size()[0])/2. + origin[0]), unit);
      }
      else
        propList.insert("style:horizontal-pos", "center");
      break;
    default:
      break;
    }
    return;
  }
  if (pos.m_anchorTo != MWAWPosition::Char &&
      pos.m_anchorTo != MWAWPosition::CharBaseLine &&
      pos.m_anchorTo != MWAWPosition::Unknown) return;

  propList.insert("text:anchor-type", "as-char");
  if (pos.m_anchorTo == MWAWPosition::CharBaseLine)
    propList.insert("style:vertical-rel", "baseline");
  else
    propList.insert("style:vertical-rel", "line");
  switch (pos.m_yPos) {
  case MWAWPosition::YFull:
  case MWAWPosition::YTop:
    if (origin[1] < 0.0 || origin[1] > 0.0) {
      propList.insert("style:vertical-pos", "from-top");
      propList.insert("svg:y", double(origin[1]), unit);
    }
    else
      propList.insert("style:vertical-pos", "top");
    break;
  case MWAWPosition::YCenter:
    if (origin[1] < 0.0 || origin[1] > 0.0) {
      propList.insert("style:vertical-pos", "from-top");
      propList.insert("svg:y", double(origin[1] - pos.size()[1]/2.0), unit);
    }
    else
      propList.insert("style:vertical-pos", "middle");
    break;
  case MWAWPosition::YBottom:
  default:
    if (origin[1] < 0.0 || origin[1] > 0.0) {
      propList.insert("style:vertical-pos", "from-top");
      propList.insert("svg:y", double(origin[1] - pos.size()[1]), unit);
    }
    else
      propList.insert("style:vertical-pos", "bottom");
    break;
  }
}

///////////////////
// subdocument
///////////////////
void MWAWSpreadsheetListener::handleSubDocument(MWAWSubDocumentPtr subDocument, libmwaw::SubDocumentType subDocumentType)
{
  _pushParsingState();
  _startSubDocument();
  m_ps->m_subDocumentType = subDocumentType;

  m_ps->m_isPageSpanOpened = true;
  m_ps->m_list.reset();

  switch (subDocumentType) {
  case libmwaw::DOC_TEXT_BOX:
    m_ps->m_isTextboxOpened = true;
    m_ds->m_pageSpan.setMargins(0.0);
    break;
  case libmwaw::DOC_HEADER_FOOTER:
    m_ps->m_isHeaderFooterWithoutParagraph = true;
    m_ps->m_isHeaderFooterOpened = true;
    break;
  case libmwaw::DOC_CHART_ZONE:
    m_ps->m_isTextboxOpened = true;
    break;
  case libmwaw::DOC_NONE:
  case libmwaw::DOC_CHART:
  case libmwaw::DOC_NOTE:
  case libmwaw::DOC_SHEET:
  case libmwaw::DOC_TABLE:
  case libmwaw::DOC_COMMENT_ANNOTATION:
  case libmwaw::DOC_GRAPHIC_GROUP:
  default:
    break;
  }

  // Check whether the document is calling itself
  bool sendDoc = true;
  for (size_t i = 0; i < m_ds->m_subDocuments.size(); i++) {
    if (!subDocument)
      break;
    if (subDocument == m_ds->m_subDocuments[i]) {
      MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::handleSubDocument: recursif call, stop...\n"));
      sendDoc = false;
      break;
    }
  }
  if (sendDoc) {
    if (subDocument) {
      m_ds->m_subDocuments.push_back(subDocument);
      shared_ptr<MWAWListener> listen(this, MWAW_shared_ptr_noop_deleter<MWAWSpreadsheetListener>());
      try {
        subDocument->parse(listen, subDocumentType);
      }
      catch (...) {
        MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::handleSubDocument exception catched \n"));
      }
      m_ds->m_subDocuments.pop_back();
    }
    if (m_ps->m_isHeaderFooterWithoutParagraph)
      _openSpan();
  }

  _endSubDocument();
  _popParsingState();
}

bool MWAWSpreadsheetListener::isSubDocumentOpened(libmwaw::SubDocumentType &subdocType) const
{
  if (!m_ps->m_inSubDocument)
    return false;
  subdocType = m_ps->m_subDocumentType;
  return true;
}

void MWAWSpreadsheetListener::_startSubDocument()
{
  m_ds->m_isDocumentStarted = true;
  m_ps->m_inSubDocument = true;
}

void MWAWSpreadsheetListener::_endSubDocument()
{
  if (m_ps->m_isTableOpened)
    closeTable();
  if (m_ps->m_isSpanOpened)
    _closeSpan();
  if (m_ps->m_isParagraphOpened)
    _closeParagraph();

  m_ps->m_paragraph.m_listLevelIndex=0;
  _changeList(); // flush the list exterior
}

///////////////////
// sheet
///////////////////
void MWAWSpreadsheetListener::openSheet(std::vector<float> const &colWidth, librevenge::RVNGUnit unit, std::string const &name)
{
  if (m_ds->m_isSheetOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openSheet: called with m_isSheetOpened=true\n"));
    return;
  }
  if (!m_ps->m_isPageSpanOpened)
    _openPageSpan();
  if (m_ps->m_isParagraphOpened)
    _closeParagraph();

  _pushParsingState();
  _startSubDocument();
  m_ps->m_subDocumentType = libmwaw::DOC_SHEET;
  m_ps->m_isPageSpanOpened = true;

  librevenge::RVNGPropertyList propList;
  librevenge::RVNGPropertyListVector columns;
  size_t nCols = colWidth.size();
  for (size_t c = 0; c < nCols; c++) {
    librevenge::RVNGPropertyList column;
    column.insert("style:column-width", colWidth[c], unit);
    columns.append(column);
  }
  propList.insert("librevenge:columns", columns);
  if (!name.empty())
    propList.insert("librevenge:sheet-name", name.c_str());
  m_documentInterface->openSheet(propList);
  m_ds->m_isSheetOpened = true;
}

void MWAWSpreadsheetListener::closeSheet()
{
  if (!m_ds->m_isSheetOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::closeSheet: called with m_isSheetOpened=false\n"));
    return;
  }

  m_ds->m_isSheetOpened = false;
  m_documentInterface->closeSheet();
  _endSubDocument();
  _popParsingState();
}

void MWAWSpreadsheetListener::openSheetRow(float h, librevenge::RVNGUnit unit)
{
  if (m_ds->m_isSheetRowOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openSheetRow: called with m_isSheetRowOpened=true\n"));
    return;
  }
  if (!m_ds->m_isSheetOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openSheetRow: called with m_isSheetOpened=false\n"));
    return;
  }
  librevenge::RVNGPropertyList propList;
  if (h > 0)
    propList.insert("style:row-height", h, unit);
  else if (h < 0)
    propList.insert("style:min-row-height", -h, unit);
  m_documentInterface->openSheetRow(propList);
  m_ds->m_isSheetRowOpened = true;
}

void MWAWSpreadsheetListener::closeSheetRow()
{
  if (!m_ds->m_isSheetRowOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openSheetRow: called with m_isSheetRowOpened=false\n"));
    return;
  }
  m_ds->m_isSheetRowOpened = false;
  m_documentInterface->closeSheetRow();
}

void MWAWSpreadsheetListener::openSheetCell(MWAWCell const &cell, MWAWCellContent const &content)
{
  if (!m_ds->m_isSheetRowOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openSheetCell: called with m_isSheetRowOpened=false\n"));
    return;
  }
  if (m_ps->m_isSheetCellOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openSheetCell: called with m_isSheetCellOpened=true\n"));
    closeSheetCell();
  }

  librevenge::RVNGPropertyList propList;
  cell.addTo(propList, m_parserState.m_fontConverter);
  MWAWCell::Format const &format=cell.getFormat();
  if (!format.hasBasicFormat()) {
    int numberingId=-1;
    std::stringstream name;
    if (m_ds->m_numberingIdMap.find(format)!=m_ds->m_numberingIdMap.end()) {
      numberingId=m_ds->m_numberingIdMap.find(format)->second;
      name << "Numbering" << numberingId;
    }
    else {
      numberingId=(int) m_ds->m_numberingIdMap.size();
      name << "Numbering" << numberingId;

      librevenge::RVNGPropertyList numList;
      if (format.getNumberingProperties(numList)) {
        numList.insert("librevenge:name", name.str().c_str());
        m_documentInterface->defineSheetNumberingStyle(numList);
        m_ds->m_numberingIdMap[format]=numberingId;
      }
      else
        numberingId=-1;
    }
    if (numberingId>=0)
      propList.insert("librevenge:numbering-name", name.str().c_str());
  }
  // formula
  if (content.m_formula.size()) {
    librevenge::RVNGPropertyListVector formulaVect;
    for (size_t i=0; i < content.m_formula.size(); ++i)
      formulaVect.append(content.m_formula[i].getPropertyList(*m_parserState.m_fontConverter, m_ps->m_font.id()));
    propList.insert("librevenge:formula", formulaVect);
  }
  bool hasFormula=!content.m_formula.empty();
  if (content.isValueSet() || hasFormula) {
    bool hasValue=content.isValueSet();
    if (hasFormula && (content.m_value >= 0 && content.m_value <= 0))
      hasValue=false;
    switch (format.m_format) {
    case MWAWCell::F_TEXT:
      if (!hasValue) break;
      propList.insert("librevenge:value-type", format.getValueType().c_str());
      propList.insert("librevenge:value", content.m_value, librevenge::RVNG_GENERIC);
      break;
    case MWAWCell::F_NUMBER:
      propList.insert("librevenge:value-type", format.getValueType().c_str());
      if (!hasValue) break;
      propList.insert("librevenge:value", content.m_value, librevenge::RVNG_GENERIC);
      break;
    case MWAWCell::F_BOOLEAN:
      propList.insert("librevenge:value-type", "boolean");
      if (!hasValue) break;
      propList.insert("librevenge:value", content.m_value, librevenge::RVNG_GENERIC);
      break;
    case MWAWCell::F_DATE: {
      propList.insert("librevenge:value-type", "date");
      if (!hasValue) break;
      int Y=0, M=0, D=0;
      if (!MWAWCellContent::double2Date(content.m_value, Y, M, D)) break;
      propList.insert("librevenge:year", Y);
      propList.insert("librevenge:month", M);
      propList.insert("librevenge:day", D);
      break;
    }
    case MWAWCell::F_TIME: {
      propList.insert("librevenge:value-type", "time");
      if (!hasValue) break;
      int H=0, M=0, S=0;
      if (!MWAWCellContent::double2Time(std::fmod(content.m_value,1.),H,M,S))
        break;
      propList.insert("librevenge:hours", H);
      propList.insert("librevenge:minutes", M);
      propList.insert("librevenge:seconds", S);
      break;
    }
    case MWAWCell::F_UNKNOWN:
      if (!hasValue) break;
      propList.insert("librevenge:value-type", format.getValueType().c_str());
      propList.insert("librevenge:value", content.m_value, librevenge::RVNG_GENERIC);
      break;
    default:
      break;
    }
  }

  m_ps->m_isSheetCellOpened = true;
  m_documentInterface->openSheetCell(propList);
}

void MWAWSpreadsheetListener::closeSheetCell()
{
  if (!m_ps->m_isSheetCellOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::closeSheetCell: called with m_isSheetCellOpened=false\n"));
    return;
  }

  _closeParagraph();

  m_ps->m_isSheetCellOpened = false;
  m_documentInterface->closeSheetCell();
}

void MWAWSpreadsheetListener::insertTable
(MWAWPosition const &pos, MWAWTable &table, MWAWGraphicStyle const &style)
{
  if (!m_ds->m_isSheetOpened || m_ds->m_isSheetRowOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertTable insert a table outside a sheet is not implemented\n"));
    return;
  }
  if (!openFrame(pos, style)) return;

  _pushParsingState();
  _startSubDocument();
  m_ps->m_subDocumentType = libmwaw::DOC_TABLE;

  shared_ptr<MWAWListener> listen(this, MWAW_shared_ptr_noop_deleter<MWAWSpreadsheetListener>());
  try {
    table.sendTable(listen);
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertTable exception catched \n"));
  }
  _endSubDocument();
  _popParsingState();

  closeFrame();
}


///////////////////
// chart
///////////////////
void MWAWSpreadsheetListener::insertChart
(MWAWPosition const &pos, MWAWChart &chart, MWAWGraphicStyle const &style)
{
  if (!m_ds->m_isSheetOpened || m_ds->m_isSheetRowOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertChart outside a chart in a sheet is not implemented\n"));
    return;
  }
  if (!openFrame(pos, style)) return;

  _pushParsingState();
  _startSubDocument();
  m_ps->m_subDocumentType = libmwaw::DOC_CHART;

  shared_ptr<MWAWSpreadsheetListener> listen(this, MWAW_shared_ptr_noop_deleter<MWAWSpreadsheetListener>());
  try {
    chart.sendChart(listen, m_documentInterface);
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::insertChart exception catched \n"));
  }
  _endSubDocument();
  _popParsingState();

  closeFrame();
}

void MWAWSpreadsheetListener::openTable(MWAWTable const &table)
{
  if (m_ps->m_isFrameOpened || m_ps->m_isTableOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openTable: no frame is already open...\n"));
    return;
  }

  if (m_ps->m_isParagraphOpened)
    _closeParagraph();

  // default value: which can be redefined by table
  librevenge::RVNGPropertyList propList;
  propList.insert("table:align", "left");
  propList.insert("fo:margin-left", *m_ps->m_paragraph.m_margins[1], *m_ps->m_paragraph.m_marginsUnit);

  _pushParsingState();
  _startSubDocument();
  m_ps->m_subDocumentType = libmwaw::DOC_TABLE;

  table.addTablePropertiesTo(propList);
  m_documentInterface->openTable(propList);
  m_ps->m_isTableOpened = true;
}

void MWAWSpreadsheetListener::closeTable()
{
  if (!m_ps->m_isTableOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::closeTable: called with m_isTableOpened=false\n"));
    return;
  }

  m_ps->m_isTableOpened = false;
  _endSubDocument();
  m_documentInterface->closeTable();

  _popParsingState();
}

void MWAWSpreadsheetListener::openTableRow(float h, librevenge::RVNGUnit unit, bool headerRow)
{
  if (m_ps->m_isTableRowOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openTableRow: called with m_isTableRowOpened=true\n"));
    return;
  }
  if (!m_ps->m_isTableOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openTableRow: called with m_isTableOpened=false\n"));
    return;
  }
  librevenge::RVNGPropertyList propList;
  propList.insert("librevenge:is-header-row", headerRow);

  if (h > 0)
    propList.insert("style:row-height", h, unit);
  else if (h < 0)
    propList.insert("style:min-row-height", -h, unit);
  m_documentInterface->openTableRow(propList);
  m_ps->m_isTableRowOpened = true;
}

void MWAWSpreadsheetListener::closeTableRow()
{
  if (!m_ps->m_isTableRowOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openTableRow: called with m_isTableRowOpened=false\n"));
    return;
  }
  m_ps->m_isTableRowOpened = false;
  m_documentInterface->closeTableRow();
}

void MWAWSpreadsheetListener::addEmptyTableCell(MWAWVec2i const &pos, MWAWVec2i span)
{
  if (!m_ps->m_isTableRowOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::addEmptyTableCell: called with m_isTableRowOpened=false\n"));
    return;
  }
  if (m_ps->m_isTableCellOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::addEmptyTableCell: called with m_isTableCellOpened=true\n"));
    closeTableCell();
  }
  librevenge::RVNGPropertyList propList;
  propList.insert("librevenge:column", pos[0]);
  propList.insert("librevenge:row", pos[1]);
  propList.insert("table:number-columns-spanned", span[0]);
  propList.insert("table:number-rows-spanned", span[1]);
  m_documentInterface->openTableCell(propList);
  m_documentInterface->closeTableCell();
}

void MWAWSpreadsheetListener::openTableCell(MWAWCell const &cell)
{
  if (!m_ps->m_isTableRowOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openTableCell: called with m_isTableRowOpened=false\n"));
    return;
  }
  if (m_ps->m_isTableCellOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::openTableCell: called with m_isTableCellOpened=true\n"));
    closeTableCell();
  }

  librevenge::RVNGPropertyList propList;
  cell.addTo(propList, m_parserState.m_fontConverter);
  m_ps->m_isTableCellOpened = true;
  m_documentInterface->openTableCell(propList);
}

void MWAWSpreadsheetListener::closeTableCell()
{
  if (!m_ps->m_isTableCellOpened) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::closeTableCell: called with m_isTableCellOpened=false\n"));
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
shared_ptr<MWAWSpreadsheetListenerInternal::State> MWAWSpreadsheetListener::_pushParsingState()
{
  shared_ptr<MWAWSpreadsheetListenerInternal::State> actual = m_ps;
  m_psStack.push_back(actual);
  m_ps.reset(new MWAWSpreadsheetListenerInternal::State);

  m_ps->m_isNote = actual->m_isNote;

  return actual;
}

void MWAWSpreadsheetListener::_popParsingState()
{
  if (m_psStack.size()==0) {
    MWAW_DEBUG_MSG(("MWAWSpreadsheetListener::_popParsingState: psStack is empty()\n"));
    throw libmwaw::ParseException();
  }
  m_ps = m_psStack.back();
  m_psStack.pop_back();
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
