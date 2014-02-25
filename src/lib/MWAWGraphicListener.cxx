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
#include <time.h>

#include <librevenge/librevenge.h>

#include "libmwaw/libmwaw.hxx"
#include "libmwaw_internal.hxx"

#include "MWAWCell.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWGraphicEncoder.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWGraphicShape.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWList.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWParser.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSection.hxx"
#include "MWAWSubDocument.hxx"
#include "MWAWTable.hxx"

#include "MWAWGraphicListener.hxx"

//! Internal and low level namespace to define the states of MWAWGraphicListener
namespace MWAWGraphicListenerInternal
{
/** the global graphic state of MWAWGraphicListener */
struct GraphicState {
  //! constructor
  GraphicState(std::vector<MWAWPageSpan> const &pageList) :
    m_pageList(pageList), m_metaData(), m_isDocumentStarted(false), m_isHeaderFooterStarted(false), m_sentListMarkers(), m_subDocuments()
  {
  }
  //! destructor
  ~GraphicState()
  {
  }
  //! the pages definition
  std::vector<MWAWPageSpan> m_pageList;
  //! the document meta data
  librevenge::RVNGPropertyList m_metaData;
  bool m_isDocumentStarted /** a flag to know if the document is open */, m_isHeaderFooterStarted /** a flag to know if the header footer is started */;
  /// the list of marker corresponding to sent list
  std::vector<int> m_sentListMarkers;
  //! the list of actual subdocument
  std::vector<MWAWSubDocumentPtr> m_subDocuments;
};

/** the state of a MWAWGraphicListener */
struct State {
  //! constructor
  State();
  //! destructor
  ~State() { }

//! returns true if we are in a text zone, ie. either in a textbox or a table cell
  bool isInTextZone() const
  {
    return m_isTextBoxOpened || m_isTableCellOpened;
  }
  //! the origin position
  Vec2f m_origin;
  //! a buffer to stored the text
  librevenge::RVNGString m_textBuffer;

  //! the font
  MWAWFont m_font;
  //! the paragraph
  MWAWParagraph m_paragraph;
//! true if a page is open
  bool m_isPageSpanOpened;
//! the list of list
  shared_ptr<MWAWList> m_list;

  bool m_isTextBoxOpened;
  bool m_isFrameOpened;

  bool m_isSpanOpened;
  bool m_isParagraphOpened;
  bool m_isListElementOpened;

  bool m_firstParagraphInPageSpan;

  std::vector<bool> m_listOrderedLevels; //! a stack used to know what is open

  bool m_isTableOpened;
  bool m_isTableRowOpened;
  bool m_isTableColumnOpened;
  bool m_isTableCellOpened;

  MWAWPageSpan m_pageSpan;
  unsigned m_currentPage;
  int m_numPagesRemainingInSpan;
  int m_currentPageNumber;

  bool m_inLink;
  bool m_inSubDocument;
  libmwaw::SubDocumentType m_subDocumentType;

private:
  State(const State &);
  State &operator=(const State &);
};

State::State() : m_origin(0,0),
  m_textBuffer(""), m_font(20,12)/* default time 12 */, m_paragraph(), m_isPageSpanOpened(false), m_list(),
  m_isTextBoxOpened(false), m_isFrameOpened(false),
  m_isSpanOpened(false), m_isParagraphOpened(false), m_isListElementOpened(false),
  m_firstParagraphInPageSpan(true), m_listOrderedLevels(),
  m_isTableOpened(false), m_isTableRowOpened(false), m_isTableColumnOpened(false),
  m_isTableCellOpened(false),
  m_pageSpan(), m_currentPage(0), m_numPagesRemainingInSpan(0), m_currentPageNumber(1),
  m_inLink(false), m_inSubDocument(false), m_subDocumentType(libmwaw::DOC_NONE)
{
}
}

MWAWGraphicListener::MWAWGraphicListener(MWAWParserState &parserState, std::vector<MWAWPageSpan> const &pageList, librevenge::RVNGDrawingInterface *documentInterface) : MWAWBasicListener(),
  m_ds(new MWAWGraphicListenerInternal::GraphicState(pageList)), m_ps(new MWAWGraphicListenerInternal::State),
  m_psStack(), m_parserState(parserState), m_documentInterface(documentInterface)
{
  MWAW_DEBUG_MSG(("MWAWGraphicListener::MWAWGraphicListener: general constructor is not implemented\n"));
}

MWAWGraphicListener::MWAWGraphicListener(MWAWParserState &parserState, Box2f const &box, librevenge::RVNGDrawingInterface *documentInterface) : MWAWBasicListener(),
  m_ds(), m_ps(new MWAWGraphicListenerInternal::State), m_psStack(), m_parserState(parserState), m_documentInterface(documentInterface)
{
  MWAWPageSpan pageSpan;
  pageSpan.setMargins(0);
  pageSpan.setPageSpan(1);
  pageSpan.setFormWidth(box.size().x()/72.);
  pageSpan.setFormLength(box.size().y()/72.);
  m_ds.reset(new MWAWGraphicListenerInternal::GraphicState(std::vector<MWAWPageSpan>(1, pageSpan)));
  m_ps->m_origin=box[0];
}

MWAWGraphicListener::~MWAWGraphicListener()
{
}

///////////////////
// text data
///////////////////
void MWAWGraphicListener::insertChar(uint8_t character)
{
  if (!m_ps->isInTextZone()) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertCharacter: called outside a text zone\n"));
    return;
  }
  if (character >= 0x80) {
    MWAWGraphicListener::insertUnicode(character);
    return;
  }
  if (!m_ps->m_isSpanOpened) _openSpan();
  m_ps->m_textBuffer.append((char) character);
}

void MWAWGraphicListener::insertCharacter(unsigned char c)
{
  if (!m_ps->isInTextZone()) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertCharacter: called outside a text zone\n"));
    return;
  }
  int unicode = m_parserState.m_fontConverter->unicode(m_ps->m_font.id(), c);
  if (unicode == -1) {
    if (c < 0x20) {
      MWAW_DEBUG_MSG(("MWAWGraphicListener::insertCharacter: Find odd char %x\n", int(c)));
    }
    else
      MWAWGraphicListener::insertChar((uint8_t) c);
  }
  else
    MWAWGraphicListener::insertUnicode((uint32_t) unicode);
}

int MWAWGraphicListener::insertCharacter(unsigned char c, MWAWInputStreamPtr &input, long endPos)
{
  if (!m_ps->isInTextZone()) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertCharacter: called outside a text zone\n"));
    return 0;
  }
  if (!input || !m_parserState.m_fontConverter) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertCharacter: input or font converter does not exist!!!!\n"));
    return 0;
  }
  long debPos=input->tell();
  int fId = m_ps->m_font.id();
  int unicode = endPos==debPos ?
                m_parserState.m_fontConverter->unicode(fId, c) :
                m_parserState.m_fontConverter->unicode(fId, c, input);

  long pos=input->tell();
  if (endPos > 0 && pos > endPos) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertCharacter: problem reading a character\n"));
    pos = debPos;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    unicode = m_parserState.m_fontConverter->unicode(fId, c);
  }
  if (unicode == -1) {
    if (c < 0x20) {
      MWAW_DEBUG_MSG(("MWAWGraphicListener::insertCharacter: Find odd char %x\n", int(c)));
    }
    else
      MWAWGraphicListener::insertChar((uint8_t) c);
  }
  else
    MWAWGraphicListener::insertUnicode((uint32_t) unicode);

  return int(pos-debPos);
}

void MWAWGraphicListener::insertUnicode(uint32_t val)
{
  if (!m_ps->isInTextZone()) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertUnicode: called outside a text zone\n"));
    return;
  }
  // undef character, we skip it
  if (val == 0xfffd) return;

  if (!m_ps->m_isSpanOpened) _openSpan();
  libmwaw::appendUnicode(val, m_ps->m_textBuffer);
}

void MWAWGraphicListener::insertUnicodeString(librevenge::RVNGString const &str)
{
  if (!m_ps->isInTextZone()) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertUnicodeString: called outside a text zone\n"));
    return;
  }
  if (!m_ps->m_isSpanOpened) _openSpan();
  m_ps->m_textBuffer.append(str);
}

void MWAWGraphicListener::insertEOL(bool soft)
{
  if (!m_ps->isInTextZone()) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertEOL: called outside a text zone\n"));
    return;
  }
  if (!m_ps->m_isParagraphOpened && !m_ps->m_isListElementOpened)
    _openSpan();
  if (soft) {
    _flushText();
    m_documentInterface->insertLineBreak();
  }
  else if (m_ps->m_isParagraphOpened)
    _closeParagraph();

  // sub/superscript must not survive a new line
  m_ps->m_font.set(MWAWFont::Script());
}

void MWAWGraphicListener::insertTab()
{
  if (!m_ps->isInTextZone()) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertTab: called outside a text zone\n"));
    return;
  }
  if (!m_ps->m_isSpanOpened) _openSpan();
  _flushText();
  m_documentInterface->insertTab();
}

///////////////////
// font/paragraph function
///////////////////
void MWAWGraphicListener::setFont(MWAWFont const &font)
{
  if (!m_ps->isInTextZone()) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::setFont: called outside a text zone\n"));
    return;
  }
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

MWAWFont const &MWAWGraphicListener::getFont() const
{
  return m_ps->m_font;
}

bool MWAWGraphicListener::isParagraphOpened() const
{
  return m_ps->m_isParagraphOpened;
}

void MWAWGraphicListener::setParagraph(MWAWParagraph const &para)
{
  if (!m_ps->isInTextZone()) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::setParagraph: called outside a text zone\n"));
    return;
  }
  if (para==m_ps->m_paragraph) return;

  m_ps->m_paragraph=para;
}

MWAWParagraph const &MWAWGraphicListener::getParagraph() const
{
  return m_ps->m_paragraph;
}

///////////////////
// field/link :
///////////////////
void MWAWGraphicListener::insertField(MWAWField const &field)
{
  if (!m_ps->isInTextZone()) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::setParagraph: called outside a text zone\n"));
    return;
  }
  switch (field.m_type) {
  case MWAWField::None:
    break;
  case MWAWField::PageCount:
  case MWAWField::PageNumber:
  case MWAWField::Title: {
    _flushText();
    _openSpan();
    librevenge::RVNGPropertyList propList;
    if (field.m_type==MWAWField::Title)
      propList.insert("librevenge:field-type", "text:title");
    else {
      propList.insert("style:num-format", libmwaw::numberingTypeToString(field.m_numberingType).c_str());
      if (field.m_type == MWAWField::PageNumber)
        propList.insert("librevenge:field-type", "text:page-number");
      else
        propList.insert("librevenge:field-type", "text:page-count");
    }
    m_documentInterface->insertField(propList);
    break;
  }
  case MWAWField::Database:
    if (field.m_data.length())
      MWAWGraphicListener::insertUnicodeString(field.m_data.c_str());
    else
      MWAWGraphicListener::insertUnicodeString("#DATAFIELD#");
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
      MWAWGraphicListener::insertUnicodeString(librevenge::RVNGString(buf));
    }
    break;
  }
  default:
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertField: must not be called with type=%d\n", int(field.m_type)));
    break;
  }
}

void MWAWGraphicListener::openLink(MWAWLink const &link)
{
  if (!m_ps->isInTextZone()) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::openLink: called outside a textbox\n"));
    return;
  }
  if (m_ps->m_inLink) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::openLink: called inside a link\n"));
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

void MWAWGraphicListener::closeLink()
{
  if (!m_ps->m_inLink) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::closeLink: closed outside a link\n"));
    return;
  }
  m_documentInterface->closeLink();
  _popParsingState();
}

///////////////////
// document
///////////////////
void MWAWGraphicListener::startDocument()
{
  if (m_ds->m_isDocumentStarted) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::startDocument: the document is already started\n"));
    return;
  }
  m_ds->m_isDocumentStarted=true;
  m_documentInterface->startDocument(librevenge::RVNGPropertyList());
  m_documentInterface->setDocumentMetaData(m_ds->m_metaData);
}

void MWAWGraphicListener::endDocument(bool /*delayed*/)
{
  if (m_ps->m_isPageSpanOpened)
    _closePageSpan();

  if (!m_ds->m_isDocumentStarted) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::endDocument: the document is not started\n"));
    return;
  }
  m_documentInterface->endDocument();
  m_ds->m_isDocumentStarted=true;
  *m_ds=MWAWGraphicListenerInternal::GraphicState(std::vector<MWAWPageSpan>());
}

///////////////////
// document
///////////////////
void MWAWGraphicListener::setDocumentLanguage(std::string locale)
{
  if (!locale.length()) return;
  m_ds->m_metaData.insert("librevenge:language", locale.c_str());
}

bool MWAWGraphicListener::isDocumentStarted() const
{
  return m_ds->m_isDocumentStarted;
}

bool MWAWGraphicListener::canWriteText() const
{
  return m_ps->m_isPageSpanOpened && m_ps->isInTextZone();
}

///////////////////
// page
///////////////////
bool MWAWGraphicListener::isPageSpanOpened() const
{
  return m_ps->m_isPageSpanOpened;
}

MWAWPageSpan const &MWAWGraphicListener::getPageSpan()
{
  if (!m_ps->m_isPageSpanOpened)
    _openPageSpan();
  return m_ps->m_pageSpan;
}

void MWAWGraphicListener::_openPageSpan(bool /*sendHeaderFooters*/)
{
  if (m_ps->m_isPageSpanOpened)
    return;

  if (!m_ds->m_isDocumentStarted)
    startDocument();

  if (m_ds->m_pageList.size()==0) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::_openPageSpan: can not find any page\n"));
    throw libmwaw::ParseException();
  }
  unsigned actPage = 0;
  std::vector<MWAWPageSpan>::iterator it = m_ds->m_pageList.begin();
  while (actPage < m_ps->m_currentPage) {
    actPage+=(unsigned)it++->getPageSpan();
    if (it == m_ds->m_pageList.end()) {
      MWAW_DEBUG_MSG(("MWAWGraphicListener::_openPageSpan: can not find current page\n"));
      throw libmwaw::ParseException();
    }
  }

  MWAWPageSpan &currentPage = *it;
  librevenge::RVNGPropertyList propList;
  currentPage.getPageProperty(propList);
  propList.insert("librevenge:is-last-page-span", bool(m_ps->m_currentPage + 1 == m_ds->m_pageList.size()));
  // now add data for embedded graph
  propList.insert("svg:x",m_ps->m_origin.x(), librevenge::RVNG_POINT);
  propList.insert("svg:y",m_ps->m_origin.y(), librevenge::RVNG_POINT);
  propList.insert("svg:width",72.*currentPage.getFormWidth(), librevenge::RVNG_POINT);
  propList.insert("svg:height",72.*currentPage.getFormLength(), librevenge::RVNG_POINT);
  propList.insert("librevenge:enforce-frame",true);

  if (!m_ps->m_isPageSpanOpened)
    m_documentInterface->startPage(propList);
  m_ps->m_isPageSpanOpened = true;
  m_ps->m_pageSpan = currentPage;

#if 0
  // fixme

  // we insert the header footer
  if (sendHeaderFooters)
    currentPage.sendHeaderFooters(this);
#endif

  // first paragraph in span (necessary for resetting page number)
  m_ps->m_firstParagraphInPageSpan = true;
  m_ps->m_numPagesRemainingInSpan = (currentPage.getPageSpan() - 1);
  m_ps->m_currentPage++;
}

void MWAWGraphicListener::_closePageSpan()
{
  if (!m_ps->m_isPageSpanOpened)
    return;

  m_ps->m_isPageSpanOpened = false;
  if (m_ps->m_inSubDocument) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::endDocument: we are in a sub document\n"));
    _endSubDocument();
    _popParsingState();
  }
  if (m_ps->m_isTableOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::_closePageSpan: we are in a table zone\n"));
    closeTable();
  }
  if (m_ps->isInTextZone()) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::_closePageSpan: we are in a text zone\n"));
    if (m_ps->m_isParagraphOpened)
      _closeParagraph();
    m_ps->m_paragraph.m_listLevelIndex = 0;
    _changeList(); // flush the list exterior
  }
  m_documentInterface->endPage();
}

///////////////////
// paragraph
///////////////////
void MWAWGraphicListener::_openParagraph()
{
  if (m_ps->m_isTableOpened && !m_ps->m_isTableCellOpened)
    return;
  if (!m_ps->isInTextZone()) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::_openParagraph: called outsize a text zone\n"));
    return;
  }
  if (m_ps->m_isParagraphOpened || m_ps->m_isListElementOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::_openParagraph: a paragraph (or a list) is already opened"));
    return;
  }

  librevenge::RVNGPropertyList propList;
  m_ps->m_paragraph.addTo(propList, m_ps->m_isTableCellOpened);
  m_documentInterface->openParagraph(propList);

  _resetParagraphState();
  m_ps->m_firstParagraphInPageSpan = false;
}

void MWAWGraphicListener::_closeParagraph()
{
  if (!m_ps->isInTextZone()) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::_closeParagraph: called outsize a text zone\n"));
    return;
  }
  if (m_ps->m_inLink) return;
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

void MWAWGraphicListener::_resetParagraphState(const bool isListElement)
{
  m_ps->m_isListElementOpened = isListElement;
  m_ps->m_isParagraphOpened = true;
}

///////////////////
// list
///////////////////
void MWAWGraphicListener::_openListElement()
{
  if (m_ps->m_isTableOpened && !m_ps->m_isTableCellOpened)
    return;
  if (!m_ps->isInTextZone()) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::_openListElement: called outsize a text zone\n"));
    return;
  }
  if (m_ps->m_isParagraphOpened || m_ps->m_isListElementOpened)
    return;

  librevenge::RVNGPropertyList propList;
  m_ps->m_paragraph.addTo(propList,m_ps->m_isTableOpened);

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

void MWAWGraphicListener::_closeListElement()
{
  if (m_ps->m_isListElementOpened) {
    if (m_ps->m_isSpanOpened)
      _closeSpan();

    if (m_ps->m_list) m_ps->m_list->closeElement();
    m_documentInterface->closeListElement();
  }

  m_ps->m_isListElementOpened = m_ps->m_isParagraphOpened = false;
}

int MWAWGraphicListener::_getListId() const
{
  size_t newLevel= (size_t) m_ps->m_paragraph.m_listLevelIndex.get();
  if (newLevel == 0) return -1;
  int newListId = m_ps->m_paragraph.m_listId.get();
  if (newListId > 0) return newListId;
  static bool first = true;
  if (first) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::_getListId: the list id is not set, try to find a new one\n"));
    first = false;
  }
  shared_ptr<MWAWList> list=m_parserState.m_listManager->getNewList
                            (m_ps->m_list, int(newLevel), *m_ps->m_paragraph.m_listLevel);
  if (!list) return -1;
  return list->getId();
}

void MWAWGraphicListener::_changeList()
{
  if (!m_ps->isInTextZone()) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::_changeList: called outsize a text zone\n"));
    return;
  }
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
      MWAW_DEBUG_MSG(("MWAWGraphicListener::_changeList: can not find any list\n"));
      m_ps->m_listOrderedLevels.resize(actualLevel);
      return;
    }
    m_parserState.m_listManager->needToSend(newListId, m_ds->m_sentListMarkers);
    m_ps->m_list = theList;
    m_ps->m_list->setLevel((int)newLevel);
  }

  m_ps->m_listOrderedLevels.resize(newLevel, false);
  if (actualLevel == newLevel) return;

  librevenge::RVNGPropertyList propList;
  propList.insert("librevenge:list-id", m_ps->m_list->getId());
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
void MWAWGraphicListener::_openSpan()
{
  if (m_ps->m_isTableOpened && !m_ps->m_isTableCellOpened)
    return;
  if (!m_ps->isInTextZone()) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::_openSpan: called outsize a text zone\n"));
    return;
  }
  if (m_ps->m_isSpanOpened)
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

void MWAWGraphicListener::_closeSpan()
{
  if (m_ps->m_isTableOpened && !m_ps->m_isTableCellOpened)
    return;
  if (!m_ps->isInTextZone()) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::_closeSpan: called outsize a text zone\n"));
    return;
  }
  if (!m_ps->m_isSpanOpened)
    return;

  _flushText();
  m_documentInterface->closeSpan();
  m_ps->m_isSpanOpened = false;
}

///////////////////
// text (send data)
///////////////////
void MWAWGraphicListener::_flushText()
{
  if (m_ps->m_textBuffer.len() == 0) return;

  // when some many ' ' follows each other, call insertSpace
  librevenge::RVNGString tmpText("");
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
// header/footer
///////////////////
bool MWAWGraphicListener::isHeaderFooterOpened() const
{
  return m_ds->m_isHeaderFooterStarted;
}

bool MWAWGraphicListener::insertHeader(MWAWSubDocumentPtr subDocument, librevenge::RVNGPropertyList const &extras)
{
  if (m_ds->m_isHeaderFooterStarted) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertHeader: Oops a header/footer is already opened\n"));
    return false;
  }
  // we do not have any header interface, so mimick it by creating a textbox
  if (!openFrame())
    return false;
  librevenge::RVNGPropertyList propList(extras);
  Box2f bdbox(Vec2f(20,20), Vec2f(-20,-10)); // fixme
  _handleFrameParameters(propList, bdbox, MWAWGraphicStyle::emptyStyle());

  m_documentInterface->startTextObject(propList);
  handleSubDocument(bdbox[0], subDocument, libmwaw::DOC_HEADER_FOOTER);
  m_documentInterface->endTextObject();
  closeFrame();
  return true;
}

bool MWAWGraphicListener::insertFooter(MWAWSubDocumentPtr subDocument, librevenge::RVNGPropertyList const &extras)
{
  if (m_ds->m_isHeaderFooterStarted) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertFooter: Oops a header/footer is already opened\n"));
    return false;
  }
  MWAW_DEBUG_MSG(("MWAWGraphicListener::insertFooter: inserting footer is very experimental\n"));

  // we do not have any header interface, so mimick it by creating a textbox
  if (!openFrame())
    return false;
  librevenge::RVNGPropertyList propList(extras);
  MWAWPageSpan page(getPageSpan()); // fixme
  Box2f bdbox(Vec2f(20,72.f*float(page.getFormLength())-40.f), Vec2f(-20,-10));
  _handleFrameParameters(propList, bdbox, MWAWGraphicStyle::emptyStyle());

  m_documentInterface->startTextObject(propList);
  handleSubDocument(bdbox[0], subDocument, libmwaw::DOC_HEADER_FOOTER);
  m_documentInterface->endTextObject();
  closeFrame();
  return true;
}

///////////////////
// section
///////////////////
MWAWSection const &MWAWGraphicListener::getSection() const
{
  MWAW_DEBUG_MSG(("MWAWGraphicListener::getSection: must not be called\n"));
  static MWAWSection s_section;
  return s_section;
}

bool MWAWGraphicListener::openSection(MWAWSection const &)
{
  MWAW_DEBUG_MSG(("MWAWGraphicListener::openSection: must not be called\n"));
  return false;
}

void MWAWGraphicListener::insertBreak(BreakType)
{
  MWAW_DEBUG_MSG(("MWAWGraphicListener::insertBreak: must not be called\n"));
}

///////////////////
// picture/textbox
///////////////////

void MWAWGraphicListener::insertPicture
(Box2f const &bdbox, MWAWGraphicShape const &shape, MWAWGraphicStyle const &style)
{
  if (!m_ds->m_isDocumentStarted) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertPicture: the document is not started\n"));
    return;
  }
  if (!m_ps->m_isPageSpanOpened)
    _openPageSpan();
  if (m_ps->m_isFrameOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertPicture: a frame is already open\n"));
    return;
  }

  librevenge::RVNGPropertyList list, shapePList;
  style.addTo(list, shape.getType()==MWAWGraphicShape::Line);
  m_documentInterface->setStyle(list);
  switch (shape.addTo(bdbox[0]-m_ps->m_origin, style.hasSurface(), shapePList)) {
  case MWAWGraphicShape::C_Ellipse:
    m_documentInterface->drawEllipse(shapePList);
    break;
  case MWAWGraphicShape::C_Path:
    m_documentInterface->drawPath(shapePList);
    break;
  case MWAWGraphicShape::C_Polyline:
    m_documentInterface->drawPolyline(shapePList);
    break;
  case MWAWGraphicShape::C_Polygon:
    m_documentInterface->drawPolygon(shapePList);
    break;
  case MWAWGraphicShape::C_Rectangle:
    m_documentInterface->drawRectangle(shapePList);
    break;
  case MWAWGraphicShape::C_Bad:
    break;
  default:
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertPicture: unexpected shape\n"));
    break;
  }
}

void MWAWGraphicListener::insertPicture
(Box2f const &bdbox, MWAWGraphicStyle const &style, const librevenge::RVNGBinaryData &binaryData, std::string type)
{
  if (!m_ds->m_isDocumentStarted) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertPicture: the document is not started\n"));
    return;
  }
  if (m_ps->m_isFrameOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertPicture: a frame is already open\n"));
    return;
  }
  if (!m_ps->m_isPageSpanOpened)
    _openPageSpan();
  librevenge::RVNGPropertyList list;
  style.addTo(list);
  m_documentInterface->setStyle(list);

  list.clear();
  Vec2f pt=bdbox[0]-m_ps->m_origin;
  list.insert("svg:x",pt.x(), librevenge::RVNG_POINT);
  list.insert("svg:y",pt.y(), librevenge::RVNG_POINT);
  pt=bdbox.size();
  list.insert("svg:width",pt.x(), librevenge::RVNG_POINT);
  list.insert("svg:height",pt.y(), librevenge::RVNG_POINT);
  list.insert("librevenge:mime-type", type.c_str());
  list.insert("office:binary-data", binaryData);
  m_documentInterface->drawGraphicObject(list);
}

void MWAWGraphicListener::insertTextBox
(Box2f const &bdbox, MWAWSubDocumentPtr subDocument, MWAWGraphicStyle const &style)
{
  if (!m_ds->m_isDocumentStarted) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertTextBox: the document is not started\n"));
    return;
  }
  if (!m_ps->m_isPageSpanOpened)
    _openPageSpan();
  if (!openFrame())
    return;
  librevenge::RVNGPropertyList propList;
  _handleFrameParameters(propList, bdbox, style);
  float rotate = style.m_rotate;
  // flip does not works on text, so we ignore it...
  if (style.m_flip[0]&&style.m_flip[1]) rotate += 180.f;
  if (rotate<0||rotate>0) {
    propList.insert("librevenge:rotate", rotate);
    Vec2f size=bdbox.size();
    if (size[0]<0) size[0]=-size[0];
    if (size[1]<0) size[1]=-size[1];
    Vec2f center=bdbox[0]-m_ps->m_origin+0.5f*size;
    propList.insert("librevenge:rotate-cx",center[0], librevenge::RVNG_POINT);
    propList.insert("librevenge:rotate-cy",center[1], librevenge::RVNG_POINT);
  }
  m_documentInterface->startTextObject(propList);
  handleSubDocument(bdbox[0], subDocument, libmwaw::DOC_TEXT_BOX);
  m_documentInterface->endTextObject();
  closeFrame();
}

void MWAWGraphicListener::insertGroup(Box2f const &bdbox, MWAWSubDocumentPtr subDocument)
{
  if (!m_ds->m_isDocumentStarted || m_ps->isInTextZone()) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertGroup: can not insert a group\n"));
    return;
  }
  if (!m_ps->m_isPageSpanOpened)
    _openPageSpan();
  handleSubDocument(bdbox[0], subDocument, libmwaw::DOC_GRAPHIC_GROUP);
}

///////////////////
// table
///////////////////
void MWAWGraphicListener::closeTable()
{
  if (!m_ps->m_isTableOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::closeTable: called with m_isTableOpened=false\n"));
    return;
  }

  m_ps->m_isTableOpened = false;
  _endSubDocument();
  m_documentInterface->endTableObject();

  _popParsingState();
}

void MWAWGraphicListener::openTableRow(float h, librevenge::RVNGUnit unit, bool headerRow)
{
  if (m_ps->m_isTableRowOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::openTableRow: called with m_isTableRowOpened=true\n"));
    return;
  }
  if (!m_ps->m_isTableOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::openTableRow: called with m_isTableOpened=false\n"));
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

void MWAWGraphicListener::closeTableRow()
{
  if (!m_ps->m_isTableRowOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::openTableRow: called with m_isTableRowOpened=false\n"));
    return;
  }
  m_ps->m_isTableRowOpened = false;
  m_documentInterface->closeTableRow();
}

void MWAWGraphicListener::addEmptyTableCell(Vec2i const &pos, Vec2i span)
{
  if (!m_ps->m_isTableRowOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::addEmptyTableCell: called with m_isTableRowOpened=false\n"));
    return;
  }
  if (m_ps->m_isTableCellOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::addEmptyTableCell: called with m_isTableCellOpened=true\n"));
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

void MWAWGraphicListener::openTableCell(MWAWCell const &cell)
{
  if (!m_ps->m_isTableRowOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::openTableCell: called with m_isTableRowOpened=false\n"));
    return;
  }
  if (m_ps->m_isTableCellOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::openTableCell: called with m_isTableCellOpened=true\n"));
    closeTableCell();
  }

  librevenge::RVNGPropertyList propList;
  cell.addTo(propList, m_parserState.m_fontConverter);
  m_ps->m_isTableCellOpened = true;
  m_documentInterface->openTableCell(propList);
}

void MWAWGraphicListener::closeTableCell()
{
  if (!m_ps->m_isTableCellOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::closeTableCell: called with m_isTableCellOpened=false\n"));
    return;
  }

  _closeParagraph();
  m_ps->m_paragraph.m_listLevelIndex=0;
  _changeList(); // flush the list exterior

  m_ps->m_isTableCellOpened = false;
  m_documentInterface->closeTableCell();
}

///////////////////
// frame
///////////////////
bool MWAWGraphicListener::openFrame()
{
  if (!m_ds->m_isDocumentStarted) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::openFrame: the document is not started\n"));
    return false;
  }
  if (m_ps->m_isTableOpened && !m_ps->m_isTableCellOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::openFrame: called in table but cell is not opened\n"));
    return false;
  }
  if (m_ps->m_isFrameOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::openFrame: called but a frame is already opened\n"));
    return false;
  }
  if (!m_ps->m_isPageSpanOpened)
    _openPageSpan();
  m_ps->m_isFrameOpened = true;
  return true;
}

void MWAWGraphicListener::closeFrame()
{
  if (!m_ps->m_isFrameOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::closeFrame: called but no frame is already opened\n"));
    return;
  }
  m_ps->m_isFrameOpened = false;
}

void MWAWGraphicListener::_handleFrameParameters(librevenge::RVNGPropertyList &list, Box2f const &bdbox, MWAWGraphicStyle const &style)
{
  if (!m_ds->m_isDocumentStarted)
    return;

  Vec2f size=bdbox.size();
  Vec2f pt=bdbox[0]-m_ps->m_origin;
  // checkme: do we still need to do that ?
  if (style.hasGradient(true)) {
    if (style.m_rotate<0 || style.m_rotate>0) {
      MWAW_DEBUG_MSG(("MWAWGraphicListener::_handleFrameParameters: rotation is not implemented\n"));
    }
    // ok, first send a background rectangle
    librevenge::RVNGPropertyList rectList;
    m_documentInterface->setStyle(rectList);
    rectList.clear();
    rectList.insert("svg:x",pt[0], librevenge::RVNG_POINT);
    rectList.insert("svg:y",pt[1], librevenge::RVNG_POINT);
    rectList.insert("svg:width",size.x()>0 ? size.x() : -size.x(), librevenge::RVNG_POINT);
    rectList.insert("svg:height",size.y()>0 ? size.y() : -size.y(), librevenge::RVNG_POINT);
    m_documentInterface->drawRectangle(rectList);

    list.insert("draw:stroke", "none");
    list.insert("draw:fill", "none");
  }
  else
    style.addTo(list);

  list.insert("svg:x",pt[0], librevenge::RVNG_POINT);
  list.insert("svg:y",pt[1], librevenge::RVNG_POINT);
  if (size.x()>0)
    list.insert("svg:width",size.x(), librevenge::RVNG_POINT);
  else if (size.x()<0)
    list.insert("fo:min-width",-size.x(), librevenge::RVNG_POINT);
  if (size.y()>0)
    list.insert("svg:height",size.y(), librevenge::RVNG_POINT);
  else if (size.y()<0)
    list.insert("fo:min-height",-size.y(), librevenge::RVNG_POINT);
  float const padding = 0; // fillme
  list.insert("fo:padding-top",padding, librevenge::RVNG_POINT);
  list.insert("fo:padding-bottom",padding, librevenge::RVNG_POINT);
  list.insert("fo:padding-left",padding, librevenge::RVNG_POINT);
  list.insert("fo:padding-right",padding, librevenge::RVNG_POINT);
}

///////////////////
// subdocument
///////////////////
void MWAWGraphicListener::handleSubDocument(Vec2f const &orig, MWAWSubDocumentPtr subDocument, libmwaw::SubDocumentType subDocumentType)
{
  if (!m_ds->m_isDocumentStarted) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::handleSubDocument: the graphic is not started\n"));
    return;
  }
  if (!m_ps->m_isPageSpanOpened)
    _openPageSpan();
  Vec2f actOrigin=m_ps->m_origin;
  _pushParsingState();
  m_ps->m_isPageSpanOpened=true;
  m_ps->m_origin=actOrigin-orig;
  _startSubDocument();
  m_ps->m_subDocumentType = subDocumentType;

  m_ps->m_list.reset();
  if (subDocumentType==libmwaw::DOC_TEXT_BOX)
    m_ps->m_isTextBoxOpened=true;
  else if (subDocumentType==libmwaw::DOC_HEADER_FOOTER) {
    m_ps->m_isTextBoxOpened=true;
    m_ds->m_isHeaderFooterStarted = true;
  }

  // Check whether the document is calling itself
  bool sendDoc = true;
  for (size_t i = 0; i < m_ds->m_subDocuments.size(); i++) {
    if (!subDocument)
      break;
    if (subDocument == m_ds->m_subDocuments[i]) {
      MWAW_DEBUG_MSG(("MWAWGraphicListener::handleSubDocument: recursif call, stop...\n"));
      sendDoc = false;
      break;
    }
  }
  if (sendDoc) {
    if (subDocument) {
      m_ds->m_subDocuments.push_back(subDocument);
      shared_ptr<MWAWGraphicListener> listen(this, MWAW_shared_ptr_noop_deleter<MWAWGraphicListener>());
      try {
        subDocument->parseGraphic(listen, subDocumentType);
      }
      catch (...) {
        MWAW_DEBUG_MSG(("Works: MWAWGraphicListener::handleSubDocument exception catched \n"));
      }
      m_ds->m_subDocuments.pop_back();
    }
  }

  _endSubDocument();
  _popParsingState();

  if (subDocumentType==libmwaw::DOC_HEADER_FOOTER)
    m_ds->m_isHeaderFooterStarted = false;
}

bool MWAWGraphicListener::isSubDocumentOpened(libmwaw::SubDocumentType &subdocType) const
{
  if (!m_ds->m_isDocumentStarted || !m_ps->m_inSubDocument)
    return false;
  subdocType = m_ps->m_subDocumentType;
  return true;
}

void MWAWGraphicListener::_startSubDocument()
{
  if (!m_ds->m_isDocumentStarted) return;
  m_ps->m_inSubDocument = true;
}

void MWAWGraphicListener::_endSubDocument()
{
  if (!m_ds->m_isDocumentStarted) return;
  if (m_ps->m_isTableOpened)
    closeTable();
  if (m_ps->m_isParagraphOpened)
    _closeParagraph();
  if (m_ps->isInTextZone()) {
    m_ps->m_paragraph.m_listLevelIndex=0;
    _changeList(); // flush the list exterior
  }
}

///////////////////
// others
///////////////////

// ---------- state stack ------------------
shared_ptr<MWAWGraphicListenerInternal::State> MWAWGraphicListener::_pushParsingState()
{
  shared_ptr<MWAWGraphicListenerInternal::State> actual = m_ps;
  m_psStack.push_back(actual);
  m_ps.reset(new MWAWGraphicListenerInternal::State);
  return actual;
}

void MWAWGraphicListener::_popParsingState()
{
  if (m_psStack.size()==0) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::_popParsingState: psStack is empty()\n"));
    throw libmwaw::ParseException();
  }
  m_ps = m_psStack.back();
  m_psStack.pop_back();
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
