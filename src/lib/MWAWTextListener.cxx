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

/** \file MWAWTextListener.cxx
 * Implements MWAWTextListener: the libmwaw word processor listener
 *
 * \note this class is the only class which does the interface with
 * the librevenge::RVNGTextInterface
 */

#include <cstring>
#include <iomanip>
#include <sstream>
#include <time.h>

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"

#include "MWAWCell.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWGraphicEncoder.hxx"
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

#include "MWAWTextListener.hxx"

//! Internal and low level namespace to define the states of MWAWTextListener
namespace MWAWTextListenerInternal
{
//! a enum to define basic break bit
enum { PageBreakBit=0x1, ColumnBreakBit=0x2 };
//! a class to store the document state of a MWAWTextListener
struct DocumentState {
  //! constructor
  DocumentState(std::vector<MWAWPageSpan> const &pageList) :
    m_pageList(pageList), m_pageSpan(), m_metaData(), m_footNoteNumber(0), m_endNoteNumber(0), m_smallPictureNumber(0),
    m_isDocumentStarted(false), m_isHeaderFooterStarted(false), m_sentListMarkers(), m_subDocuments()
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

  int m_footNoteNumber /** footnote number*/, m_endNoteNumber /** endnote number*/;

  int m_smallPictureNumber /** number of small picture */;
  bool m_isDocumentStarted /** a flag to know if the document is open */, m_isHeaderFooterStarted /** a flag to know if the header footer is started */;
  /// the list of marker corresponding to sent list
  std::vector<int> m_sentListMarkers;
  std::vector<MWAWSubDocumentPtr> m_subDocuments; /** list of document actually open */

private:
  DocumentState(const DocumentState &);
  DocumentState &operator=(const DocumentState &);
};

/** the state of a MWAWTextListener */
struct State {
  //! constructor
  State();
  //! destructor
  ~State() { }

  //! a buffer to stored the text
  librevenge::RVNGString m_textBuffer;
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

  bool m_isTableOpened;
  bool m_isTableRowOpened;
  bool m_isTableColumnOpened;
  bool m_isTableCellOpened;

  unsigned m_currentPage;
  int m_numPagesRemainingInSpan;
  int m_currentPageNumber;

  bool m_sectionAttributesChanged;
  //! the section
  MWAWSection m_section;

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

  m_paragraph(), m_paragraphNeedBreak(0),

  m_list(),

  m_isPageSpanOpened(false), m_isSectionOpened(false), m_isFrameOpened(false),
  m_isPageSpanBreakDeferred(false),
  m_isHeaderFooterWithoutParagraph(false),

  m_isSpanOpened(false), m_isParagraphOpened(false), m_isListElementOpened(false),

  m_firstParagraphInPageSpan(true),

  m_isTableOpened(false), m_isTableRowOpened(false), m_isTableColumnOpened(false),
  m_isTableCellOpened(false),

  m_currentPage(0), m_numPagesRemainingInSpan(0), m_currentPageNumber(1),

  m_sectionAttributesChanged(false),
  m_section(),

  m_listOrderedLevels(),

  m_inSubDocument(false),
  m_isNote(false), m_inLink(false),
  m_subDocumentType(libmwaw::DOC_NONE)
{
}
}

MWAWTextListener::MWAWTextListener(MWAWParserState &parserState, std::vector<MWAWPageSpan> const &pageList, librevenge::RVNGTextInterface *documentInterface) : MWAWListener(),
  m_ds(new MWAWTextListenerInternal::DocumentState(pageList)), m_ps(new MWAWTextListenerInternal::State), m_psStack(),
  m_parserState(parserState), m_documentInterface(documentInterface)
{
}

MWAWTextListener::~MWAWTextListener()
{
}

///////////////////
// text data
///////////////////
void MWAWTextListener::insertChar(uint8_t character)
{
  if (character >= 0x80) {
    MWAWTextListener::insertUnicode(character);
    return;
  }
  _flushDeferredTabs();
  if (!m_ps->m_isSpanOpened) _openSpan();
  m_ps->m_textBuffer.append((char) character);
}

void MWAWTextListener::insertCharacter(unsigned char c)
{
  int unicode = m_parserState.m_fontConverter->unicode(m_ps->m_font.id(), c);
  if (unicode == -1) {
    if (c < 0x20) {
      MWAW_DEBUG_MSG(("MWAWTextListener::insertCharacter: Find odd char %x\n", (unsigned int)c));
    }
    else
      MWAWTextListener::insertChar((uint8_t) c);
  }
  else
    MWAWTextListener::insertUnicode((uint32_t) unicode);
}

int MWAWTextListener::insertCharacter(unsigned char c, MWAWInputStreamPtr &input, long endPos)
{
  if (!input || !m_parserState.m_fontConverter) {
    MWAW_DEBUG_MSG(("MWAWTextListener::insertCharacter: input or font converter does not exist!!!!\n"));
    return 0;
  }
  long debPos=input->tell();
  int fId = m_ps->m_font.id();
  int unicode = endPos==debPos ?
                m_parserState.m_fontConverter->unicode(fId, c) :
                m_parserState.m_fontConverter->unicode(fId, c, input);

  long pos=input->tell();
  if (endPos > 0 && pos > endPos) {
    MWAW_DEBUG_MSG(("MWAWTextListener::insertCharacter: problem reading a character\n"));
    pos = debPos;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    unicode = m_parserState.m_fontConverter->unicode(fId, c);
  }
  if (unicode == -1) {
    if (c < 0x20) {
      MWAW_DEBUG_MSG(("MWAWTextListener::insertCharacter: Find odd char %x\n", (unsigned int)c));
    }
    else
      MWAWTextListener::insertChar((uint8_t) c);
  }
  else
    MWAWTextListener::insertUnicode((uint32_t) unicode);

  return int(pos-debPos);
}

void MWAWTextListener::insertUnicode(uint32_t val)
{
  // undef character, we skip it
  if (val == 0xfffd) return;

  _flushDeferredTabs();
  if (!m_ps->m_isSpanOpened) _openSpan();
  libmwaw::appendUnicode(val, m_ps->m_textBuffer);
}

void MWAWTextListener::insertUnicodeString(librevenge::RVNGString const &str)
{
  _flushDeferredTabs();
  if (!m_ps->m_isSpanOpened) _openSpan();
  m_ps->m_textBuffer.append(str);
}

void MWAWTextListener::insertEOL(bool soft)
{
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

void MWAWTextListener::insertTab()
{
  if (!m_ps->m_isParagraphOpened) {
    m_ps->m_numDeferredTabs++;
    return;
  }
  if (m_ps->m_isSpanOpened) _flushText();
  m_ps->m_numDeferredTabs++;
  _flushDeferredTabs();
}

void MWAWTextListener::insertBreak(MWAWTextListener::BreakType breakType)
{
  switch (breakType) {
  case ColumnBreak:
    if (!m_ps->m_isPageSpanOpened && !m_ps->m_inSubDocument)
      _openSpan();
    if (m_ps->m_isParagraphOpened)
      _closeParagraph();
    m_ps->m_paragraphNeedBreak |= MWAWTextListenerInternal::ColumnBreakBit;
    break;
  case PageBreak:
    if (!m_ps->m_isPageSpanOpened && !m_ps->m_inSubDocument)
      _openSpan();
    if (m_ps->m_isParagraphOpened)
      _closeParagraph();
    m_ps->m_paragraphNeedBreak |= MWAWTextListenerInternal::PageBreakBit;
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

void MWAWTextListener::_insertBreakIfNecessary(librevenge::RVNGPropertyList &propList)
{
  if (!m_ps->m_paragraphNeedBreak)
    return;

  if ((m_ps->m_paragraphNeedBreak&MWAWTextListenerInternal::PageBreakBit) ||
      m_ps->m_section.numColumns() <= 1) {
    if (m_ps->m_inSubDocument) {
      MWAW_DEBUG_MSG(("MWAWTextListener::_insertBreakIfNecessary: can not add page break in subdocument\n"));
    }
    else
      propList.insert("fo:break-before", "page");
  }
  else if (m_ps->m_paragraphNeedBreak&MWAWTextListenerInternal::ColumnBreakBit)
    propList.insert("fo:break-before", "column");
  m_ps->m_paragraphNeedBreak=0;
}

///////////////////
// font/paragraph function
///////////////////
void MWAWTextListener::setFont(MWAWFont const &font)
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

MWAWFont const &MWAWTextListener::getFont() const
{
  return m_ps->m_font;
}

bool MWAWTextListener::isParagraphOpened() const
{
  return m_ps->m_isParagraphOpened;
}

void MWAWTextListener::setParagraph(MWAWParagraph const &para)
{
  if (para==m_ps->m_paragraph) return;

  m_ps->m_paragraph=para;
}

MWAWParagraph const &MWAWTextListener::getParagraph() const
{
  return m_ps->m_paragraph;
}

///////////////////
// field/link :
///////////////////
void MWAWTextListener::insertField(MWAWField const &field)
{
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
      MWAWTextListener::insertUnicodeString(field.m_data.c_str());
    else
      MWAWTextListener::insertUnicodeString("#DATAFIELD#");
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
      MWAWTextListener::insertUnicodeString(librevenge::RVNGString(buf));
    }
    break;
  }
  default:
    MWAW_DEBUG_MSG(("MWAWTextListener::insertField: must not be called with type=%d\n", int(field.m_type)));
    break;
  }
}

void MWAWTextListener::openLink(MWAWLink const &link)
{
  if (m_ps->m_inLink) {
    MWAW_DEBUG_MSG(("MWAWTextListener:openLink: a link is already opened\n"));
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

void MWAWTextListener::closeLink()
{
  if (!m_ps->m_inLink) {
    MWAW_DEBUG_MSG(("MWAWTextListener:closeLink: can not close a link\n"));
    return;
  }
  if (m_ps->m_isSpanOpened) _closeSpan();
  m_documentInterface->closeLink();
  _popParsingState();
}

///////////////////
// document
///////////////////
void MWAWTextListener::setDocumentLanguage(std::string locale)
{
  if (!locale.length()) return;
  m_ds->m_metaData.insert("librevenge:language", locale.c_str());
}

bool MWAWTextListener::isDocumentStarted() const
{
  return m_ds->m_isDocumentStarted;
}

void MWAWTextListener::startDocument()
{
  if (m_ds->m_isDocumentStarted) {
    MWAW_DEBUG_MSG(("MWAWTextListener::startDocument: the document is already started\n"));
    return;
  }

  m_documentInterface->startDocument(librevenge::RVNGPropertyList());
  m_ds->m_isDocumentStarted = true;

  m_documentInterface->setDocumentMetaData(m_ds->m_metaData);
}

void MWAWTextListener::endDocument(bool sendDelayedSubDoc)
{
  if (!m_ds->m_isDocumentStarted) {
    MWAW_DEBUG_MSG(("MWAWTextListener::endDocument: the document is not started\n"));
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
bool MWAWTextListener::isPageSpanOpened() const
{
  return m_ps->m_isPageSpanOpened;
}

MWAWPageSpan const &MWAWTextListener::getPageSpan()
{
  if (!m_ps->m_isPageSpanOpened)
    _openPageSpan();
  return m_ds->m_pageSpan;
}


void MWAWTextListener::_openPageSpan(bool sendHeaderFooters)
{
  if (m_ps->m_isPageSpanOpened)
    return;

  if (!m_ds->m_isDocumentStarted)
    startDocument();

  if (m_ds->m_pageList.size()==0) {
    MWAW_DEBUG_MSG(("MWAWTextListener::_openPageSpan: can not find any page\n"));
    throw libmwaw::ParseException();
  }
  unsigned actPage = 0;
  std::vector<MWAWPageSpan>::iterator it = m_ds->m_pageList.begin();
  ++m_ps->m_currentPage;
  while (true) {
    actPage+=(unsigned)it->getPageSpan();
    if (actPage>=m_ps->m_currentPage) break;
    if (++it == m_ds->m_pageList.end()) {
      MWAW_DEBUG_MSG(("MWAWTextListener::_openPageSpan: can not find current page, use last page\n"));
      --it;
      break;
    }
  }
  MWAWPageSpan &currentPage = *it;

  librevenge::RVNGPropertyList propList;
  currentPage.getPageProperty(propList);
  propList.insert("librevenge:is-last-page-span", ++it == m_ds->m_pageList.end());

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

void MWAWTextListener::_closePageSpan()
{
  if (!m_ps->m_isPageSpanOpened)
    return;

  if (m_ps->m_isSectionOpened)
    _closeSection();

  m_documentInterface->closePageSpan();
  m_ps->m_isPageSpanOpened = m_ps->m_isPageSpanBreakDeferred = false;
}

///////////////////
// header/footer
///////////////////
bool MWAWTextListener::isHeaderFooterOpened() const
{
  return m_ds->m_isHeaderFooterStarted;
}

bool MWAWTextListener::insertHeader(MWAWSubDocumentPtr subDocument, librevenge::RVNGPropertyList const &extras)
{
  if (m_ds->m_isHeaderFooterStarted) {
    MWAW_DEBUG_MSG(("MWAWTextListener::insertHeader: Oops a header/footer is already opened\n"));
    return false;
  }
  librevenge::RVNGPropertyList propList(extras);
  m_documentInterface->openHeader(propList);
  handleSubDocument(subDocument, libmwaw::DOC_HEADER_FOOTER);
  m_documentInterface->closeHeader();
  return true;
}

bool MWAWTextListener::insertFooter(MWAWSubDocumentPtr subDocument, librevenge::RVNGPropertyList const &extras)
{
  if (m_ds->m_isHeaderFooterStarted) {
    MWAW_DEBUG_MSG(("MWAWTextListener::insertFooter: Oops a header/footer is already opened\n"));
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
bool MWAWTextListener::isSectionOpened() const
{
  return m_ps->m_isSectionOpened;
}

MWAWSection const &MWAWTextListener::getSection() const
{
  return m_ps->m_section;
}

bool MWAWTextListener::canOpenSectionAddBreak() const
{
  return !m_ps->m_isTableOpened && (!m_ps->m_inSubDocument || m_ps->m_subDocumentType == libmwaw::DOC_TEXT_BOX);
}

bool MWAWTextListener::openSection(MWAWSection const &section)
{
  if (m_ps->m_isSectionOpened) {
    MWAW_DEBUG_MSG(("MWAWTextListener::openSection: a section is already opened\n"));
    return false;
  }

  if (m_ps->m_isTableOpened || (m_ps->m_inSubDocument && m_ps->m_subDocumentType != libmwaw::DOC_TEXT_BOX)) {
    MWAW_DEBUG_MSG(("MWAWTextListener::openSection: impossible to open a section\n"));
    return false;
  }

  m_ps->m_section=section;
  _openSection();
  return true;
}

bool MWAWTextListener::closeSection()
{
  if (!m_ps->m_isSectionOpened) {
    MWAW_DEBUG_MSG(("MWAWTextListener::closeSection: no section are already opened\n"));
    return false;
  }

  if (m_ps->m_isTableOpened || (m_ps->m_inSubDocument && m_ps->m_subDocumentType != libmwaw::DOC_TEXT_BOX)) {
    MWAW_DEBUG_MSG(("MWAWTextListener::closeSection: impossible to close a section\n"));
    return false;
  }
  _closeSection();
  return true;
}

void MWAWTextListener::_openSection()
{
  if (m_ps->m_isSectionOpened) {
    MWAW_DEBUG_MSG(("MWAWTextListener::_openSection: a section is already opened\n"));
    return;
  }

  if (!m_ps->m_isPageSpanOpened)
    _openPageSpan();

  librevenge::RVNGPropertyList propList;
  m_ps->m_section.addTo(propList);

  librevenge::RVNGPropertyListVector columns;
  m_ps->m_section.addColumnsTo(columns);
  if (columns.count())
    propList.insert("style:columns", columns);
  m_documentInterface->openSection(propList);

  m_ps->m_sectionAttributesChanged = false;
  m_ps->m_isSectionOpened = true;
}

void MWAWTextListener::_closeSection()
{
  if (!m_ps->m_isSectionOpened ||m_ps->m_isTableOpened)
    return;

  if (m_ps->m_isParagraphOpened)
    _closeParagraph();
  m_ps->m_paragraph.m_listLevelIndex=0;
  _changeList();

  m_documentInterface->closeSection();

  m_ps->m_section = MWAWSection();
  m_ps->m_sectionAttributesChanged = false;
  m_ps->m_isSectionOpened = false;
}

///////////////////
// paragraph
///////////////////
void MWAWTextListener::_openParagraph()
{
  if (m_ps->m_isTableOpened && !m_ps->m_isTableCellOpened)
    return;

  if (m_ps->m_isParagraphOpened || m_ps->m_isListElementOpened) {
    MWAW_DEBUG_MSG(("MWAWTextListener::_openParagraph: a paragraph (or a list) is already opened"));
    return;
  }
  if (!m_ps->m_isTableOpened && (!m_ps->m_inSubDocument || m_ps->m_subDocumentType == libmwaw::DOC_TEXT_BOX)) {
    if (m_ps->m_sectionAttributesChanged)
      _closeSection();

    if (!m_ps->m_isSectionOpened)
      _openSection();
  }

  librevenge::RVNGPropertyList propList;
  _appendParagraphProperties(propList);
  if (!m_ps->m_isParagraphOpened)
    m_documentInterface->openParagraph(propList);

  _resetParagraphState();
  m_ps->m_firstParagraphInPageSpan = false;
}

void MWAWTextListener::_closeParagraph()
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

  if (!m_ps->m_isTableOpened && m_ps->m_isPageSpanBreakDeferred && !m_ps->m_inSubDocument)
    _closePageSpan();
}

void MWAWTextListener::_resetParagraphState(const bool isListElement)
{
  m_ps->m_paragraphNeedBreak = 0;
  m_ps->m_isListElementOpened = isListElement;
  m_ps->m_isParagraphOpened = true;
  m_ps->m_isHeaderFooterWithoutParagraph = false;
}

void MWAWTextListener::_appendParagraphProperties(librevenge::RVNGPropertyList &propList, const bool /*isListElement*/)
{
  m_ps->m_paragraph.addTo(propList,m_ps->m_isTableOpened);

  if (!m_ps->m_inSubDocument && m_ps->m_firstParagraphInPageSpan && m_ds->m_pageSpan.getPageNumber() >= 0)
    propList.insert("style:page-number", m_ds->m_pageSpan.getPageNumber());

  _insertBreakIfNecessary(propList);
}

///////////////////
// list
///////////////////
void MWAWTextListener::_openListElement()
{
  if (m_ps->m_isTableOpened && !m_ps->m_isTableCellOpened)
    return;

  if (m_ps->m_isParagraphOpened || m_ps->m_isListElementOpened)
    return;

  if (!m_ps->m_isTableOpened && (!m_ps->m_inSubDocument || m_ps->m_subDocumentType == libmwaw::DOC_TEXT_BOX)) {
    if (m_ps->m_sectionAttributesChanged)
      _closeSection();

    if (!m_ps->m_isSectionOpened)
      _openSection();
  }

  librevenge::RVNGPropertyList propList;
  _appendParagraphProperties(propList, true);
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

void MWAWTextListener::_closeListElement()
{
  if (m_ps->m_isListElementOpened) {
    if (m_ps->m_isSpanOpened)
      _closeSpan();

    if (m_ps->m_list) m_ps->m_list->closeElement();
    m_documentInterface->closeListElement();
  }

  m_ps->m_isListElementOpened = m_ps->m_isParagraphOpened = false;

  if (!m_ps->m_isTableOpened && m_ps->m_isPageSpanBreakDeferred && !m_ps->m_inSubDocument)
    _closePageSpan();
}

int MWAWTextListener::_getListId() const
{
  size_t newLevel= (size_t) m_ps->m_paragraph.m_listLevelIndex.get();
  if (newLevel == 0) return -1;
  int newListId = m_ps->m_paragraph.m_listId.get();
  if (newListId > 0) return newListId;
  static bool first = true;
  if (first) {
    MWAW_DEBUG_MSG(("MWAWTextListener::_getListId: the list id is not set, try to find a new one\n"));
    first = false;
  }
  shared_ptr<MWAWList> list=m_parserState.m_listManager->getNewList
                            (m_ps->m_list, int(newLevel), *m_ps->m_paragraph.m_listLevel);
  if (!list) return -1;
  return list->getId();
}

void MWAWTextListener::_changeList()
{
  if (m_ps->m_isParagraphOpened)
    _closeParagraph();

  size_t actualLevel = m_ps->m_listOrderedLevels.size();
  size_t newLevel= (size_t) m_ps->m_paragraph.m_listLevelIndex.get();

  if (!m_ps->m_isSectionOpened && newLevel && !m_ps->m_isTableOpened &&
      (!m_ps->m_inSubDocument || m_ps->m_subDocumentType == libmwaw::DOC_TEXT_BOX))
    _openSection();

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
      MWAW_DEBUG_MSG(("MWAWTextListener::_changeList: can not find any list\n"));
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
void MWAWTextListener::_openSpan()
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

  librevenge::RVNGPropertyList propList;
  m_ps->m_font.addTo(propList, m_parserState.m_fontConverter);

  m_documentInterface->openSpan(propList);

  m_ps->m_isSpanOpened = true;
}

void MWAWTextListener::_closeSpan()
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
void MWAWTextListener::_flushDeferredTabs()
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

void MWAWTextListener::_flushText()
{
  if (m_ps->m_textBuffer.len() == 0) return;

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
void MWAWTextListener::insertNote(MWAWNote const &note, MWAWSubDocumentPtr &subDocument)
{
  if (m_ps->m_isNote) {
    MWAW_DEBUG_MSG(("MWAWTextListener::insertNote try to insert a note recursively (ignored)\n"));
    return;
  }

  m_ps->m_isNote = true;
  if (m_ds->m_isHeaderFooterStarted) {
    MWAW_DEBUG_MSG(("MWAWTextListener::insertNote try to insert a note in a header/footer\n"));
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
      if (note.m_number >= 0)
        m_ds->m_endNoteNumber = note.m_number;
      else
        m_ds->m_endNoteNumber++;
      propList.insert("librevenge:number", m_ds->m_endNoteNumber);
      m_documentInterface->openEndnote(propList);
      handleSubDocument(subDocument, libmwaw::DOC_NOTE);
      m_documentInterface->closeEndnote();
    }
  }
  m_ps->m_isNote = false;
}

void MWAWTextListener::insertComment(MWAWSubDocumentPtr &subDocument)
{
  if (m_ps->m_isNote) {
    MWAW_DEBUG_MSG(("MWAWTextListener::insertComment try to insert a note recursively (ignored)\n"));
    return;
  }

  if (!m_ps->m_isParagraphOpened)
    _openParagraph();
  else {
    _flushText();
    _closeSpan();
  }

  librevenge::RVNGPropertyList propList;
  m_documentInterface->openComment(propList);

  m_ps->m_isNote = true;
  handleSubDocument(subDocument, libmwaw::DOC_COMMENT_ANNOTATION);

  m_documentInterface->closeComment();
  m_ps->m_isNote = false;
}

void MWAWTextListener::insertTextBox
(MWAWPosition const &pos, MWAWSubDocumentPtr subDocument, MWAWGraphicStyle const &frameStyle)
{
  if (!openFrame(pos, frameStyle)) return;

  librevenge::RVNGPropertyList propList;
  if (!frameStyle.m_frameNextName.empty())
    propList.insert("librevenge:next-frame-name",frameStyle.m_frameNextName.c_str());
  m_documentInterface->openTextBox(propList);
  handleSubDocument(subDocument, libmwaw::DOC_TEXT_BOX);
  m_documentInterface->closeTextBox();

  closeFrame();
}

void MWAWTextListener::insertPicture
(MWAWPosition const &pos, MWAWGraphicShape const &shape, MWAWGraphicStyle const &style)
{
  // sanity check: avoid to send to many small pict
  float factor=pos.getScaleFactor(pos.unit(), librevenge::RVNG_POINT);
  if (pos.size()[0]*factor <= 8 && pos.size()[1]*factor <= 8 && m_ds->m_smallPictureNumber++ > 200) {
    static bool first = true;
    if (first) {
      first = false;
      MWAW_DEBUG_MSG(("MWAWTextListener::insertPicture: find too much small pictures, skip them from now\n"));
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
    MWAW_DEBUG_MSG(("MWAWTextListener::insertPicture: UNKNOWN position, insert as char position\n"));
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
  case MWAWGraphicShape::C_Path: {
    // odt seems to have some problem displaying path so
    // first create the picture, reset origin (if it is bad)
    MWAWBox2f bdbox = shape.getBdBox(style,true);
    MWAWGraphicEncoder graphicEncoder;
    MWAWGraphicListener graphicListener(m_parserState, MWAWBox2f(MWAWVec2f(0,0),bdbox.size()), &graphicEncoder);
    graphicListener.startDocument();
    MWAWPosition pathPos(-1.f*bdbox[0],bdbox.size(),librevenge::RVNG_POINT);
    pathPos.m_anchorTo=MWAWPosition::Page;
    graphicListener.insertPicture(pathPos, shape, style);
    graphicListener.endDocument();

    librevenge::RVNGBinaryData data;
    std::string mime;
    if (!graphicEncoder.getBinaryResult(data,mime) || !openFrame(pos))
      break;
    librevenge::RVNGPropertyList propList;
    propList.insert("librevenge:mime-type", mime.c_str());
    propList.insert("office:binary-data", data);
    m_documentInterface->insertBinaryObject(propList);
    closeFrame();
    break;
  }
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
    MWAW_DEBUG_MSG(("MWAWTextListener::insertPicture: unexpected shape\n"));
    break;
  }
}

void MWAWTextListener::insertPicture
(MWAWPosition const &pos, const librevenge::RVNGBinaryData &binaryData, std::string type,
 MWAWGraphicStyle const &style)
{
  // sanity check: avoid to send to many small pict
  float factor=pos.getScaleFactor(pos.unit(), librevenge::RVNG_POINT);
  if (pos.size()[0]*factor <= 8 && pos.size()[1]*factor <= 8 && m_ds->m_smallPictureNumber++ > 200) {
    static bool first = true;
    if (first) {
      first = false;
      MWAW_DEBUG_MSG(("MWAWTextListener::insertPicture: find too much small pictures, skip them from now\n"));
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
bool MWAWTextListener::openFrame(MWAWPosition const &pos, MWAWGraphicStyle const &style)
{
  if (m_ps->m_isTableOpened && !m_ps->m_isTableCellOpened) {
    MWAW_DEBUG_MSG(("MWAWTextListener::openFrame: called in table but cell is not opened\n"));
    return false;
  }
  if (m_ps->m_isFrameOpened) {
    MWAW_DEBUG_MSG(("MWAWTextListener::openFrame: called but a frame is already opened\n"));
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
    MWAW_DEBUG_MSG(("MWAWTextListener::openFrame: UNKNOWN position, insert as char position\n"));
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
      MWAW_DEBUG_MSG(("MWAWTextListener::openFrame: can not determine the frame\n"));
      return false;
    }
    if (m_ps->m_subDocumentType==libmwaw::DOC_HEADER_FOOTER) {
      MWAW_DEBUG_MSG(("MWAWTextListener::openFrame: called with Frame position in header footer, switch to paragraph\n"));
      if (m_ps->m_isParagraphOpened)
        _flushText();
      else
        _openParagraph();
      fPos.m_anchorTo=MWAWPosition::Paragraph;
    }
    break;
  default:
    MWAW_DEBUG_MSG(("MWAWTextListener::openFrame: can not determine the anchor\n"));
    return false;
  }

  librevenge::RVNGPropertyList propList;
  style.addFrameTo(propList);
  _handleFrameParameters(propList, fPos);
  m_documentInterface->openFrame(propList);

  m_ps->m_isFrameOpened = true;
  return true;
}

void MWAWTextListener::closeFrame()
{
  if (!m_ps->m_isFrameOpened) {
    MWAW_DEBUG_MSG(("MWAWTextListener::closeFrame: called but no frame is already opened\n"));
    return;
  }
  m_documentInterface->closeFrame();
  m_ps->m_isFrameOpened = false;
}

void MWAWTextListener::_handleFrameParameters
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
void MWAWTextListener::handleSubDocument(MWAWSubDocumentPtr subDocument, libmwaw::SubDocumentType subDocumentType)
{
  _pushParsingState();
  _startSubDocument();
  m_ps->m_subDocumentType = subDocumentType;

  m_ps->m_isPageSpanOpened = true;
  m_ps->m_list.reset();

  switch (subDocumentType) {
  case libmwaw::DOC_TEXT_BOX:
    m_ds->m_pageSpan.setMargins(0.0);
    m_ps->m_sectionAttributesChanged = true;
    break;
  case libmwaw::DOC_HEADER_FOOTER:
    m_ps->m_isHeaderFooterWithoutParagraph = true;
    m_ds->m_isHeaderFooterStarted = true;
    break;
  case libmwaw::DOC_NONE:
  case libmwaw::DOC_CHART:
  case libmwaw::DOC_CHART_ZONE:
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
      MWAW_DEBUG_MSG(("MWAWTextListener::handleSubDocument: recursif call, stop...\n"));
      sendDoc = false;
      break;
    }
  }
  if (sendDoc) {
    if (subDocument) {
      m_ds->m_subDocuments.push_back(subDocument);
      shared_ptr<MWAWListener> listen(this, MWAW_shared_ptr_noop_deleter<MWAWTextListener>());
      try {
        subDocument->parse(listen, subDocumentType);
      }
      catch (...) {
        MWAW_DEBUG_MSG(("Works: MWAWTextListener::handleSubDocument exception catched \n"));
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
  case libmwaw::DOC_CHART:
  case libmwaw::DOC_CHART_ZONE:
  case libmwaw::DOC_NOTE:
  case libmwaw::DOC_SHEET:
  case libmwaw::DOC_TABLE:
  case libmwaw::DOC_COMMENT_ANNOTATION:
  case libmwaw::DOC_GRAPHIC_GROUP:
  default:
    break;
  }
  _endSubDocument();
  _popParsingState();
}

bool MWAWTextListener::isSubDocumentOpened(libmwaw::SubDocumentType &subdocType) const
{
  if (!m_ps->m_inSubDocument)
    return false;
  subdocType = m_ps->m_subDocumentType;
  return true;
}

void MWAWTextListener::_startSubDocument()
{
  m_ds->m_isDocumentStarted = true;
  m_ps->m_inSubDocument = true;
}

void MWAWTextListener::_endSubDocument()
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
void MWAWTextListener::openTable(MWAWTable const &table)
{
  if (m_ps->m_isTableOpened) {
    MWAW_DEBUG_MSG(("MWAWTextListener::openTable: called with m_isTableOpened=true\n"));
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

void MWAWTextListener::closeTable()
{
  if (!m_ps->m_isTableOpened) {
    MWAW_DEBUG_MSG(("MWAWTextListener::closeTable: called with m_isTableOpened=false\n"));
    return;
  }

  m_ps->m_isTableOpened = false;
  _endSubDocument();
  m_documentInterface->closeTable();

  _popParsingState();
}

void MWAWTextListener::openTableRow(float h, librevenge::RVNGUnit unit, bool headerRow)
{
  if (m_ps->m_isTableRowOpened) {
    MWAW_DEBUG_MSG(("MWAWTextListener::openTableRow: called with m_isTableRowOpened=true\n"));
    return;
  }
  if (!m_ps->m_isTableOpened) {
    MWAW_DEBUG_MSG(("MWAWTextListener::openTableRow: called with m_isTableOpened=false\n"));
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

void MWAWTextListener::closeTableRow()
{
  if (!m_ps->m_isTableRowOpened) {
    MWAW_DEBUG_MSG(("MWAWTextListener::openTableRow: called with m_isTableRowOpened=false\n"));
    return;
  }
  m_ps->m_isTableRowOpened = false;
  m_documentInterface->closeTableRow();
}

void MWAWTextListener::addEmptyTableCell(MWAWVec2i const &pos, MWAWVec2i span)
{
  if (!m_ps->m_isTableRowOpened) {
    MWAW_DEBUG_MSG(("MWAWTextListener::addEmptyTableCell: called with m_isTableRowOpened=false\n"));
    return;
  }
  if (m_ps->m_isTableCellOpened) {
    MWAW_DEBUG_MSG(("MWAWTextListener::addEmptyTableCell: called with m_isTableCellOpened=true\n"));
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

void MWAWTextListener::openTableCell(MWAWCell const &cell)
{
  if (!m_ps->m_isTableRowOpened) {
    MWAW_DEBUG_MSG(("MWAWTextListener::openTableCell: called with m_isTableRowOpened=false\n"));
    return;
  }
  if (m_ps->m_isTableCellOpened) {
    MWAW_DEBUG_MSG(("MWAWTextListener::openTableCell: called with m_isTableCellOpened=true\n"));
    closeTableCell();
  }

  librevenge::RVNGPropertyList propList;
  cell.addTo(propList, m_parserState.m_fontConverter);
  m_ps->m_isTableCellOpened = true;
  m_documentInterface->openTableCell(propList);
}

void MWAWTextListener::closeTableCell()
{
  if (!m_ps->m_isTableCellOpened) {
    MWAW_DEBUG_MSG(("MWAWTextListener::closeTableCell: called with m_isTableCellOpened=false\n"));
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
shared_ptr<MWAWTextListenerInternal::State> MWAWTextListener::_pushParsingState()
{
  shared_ptr<MWAWTextListenerInternal::State> actual = m_ps;
  m_psStack.push_back(actual);
  m_ps.reset(new MWAWTextListenerInternal::State);

  m_ps->m_isNote = actual->m_isNote;

  return actual;
}

void MWAWTextListener::_popParsingState()
{
  if (m_psStack.size()==0) {
    MWAW_DEBUG_MSG(("MWAWTextListener::_popParsingState: psStack is empty()\n"));
    throw libmwaw::ParseException();
  }
  m_ps = m_psStack.back();
  m_psStack.pop_back();
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
