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

#include "MWAWGraphicListener.hxx"

//! Internal and low level namespace to define the states of MWAWGraphicListener
namespace MWAWGraphicListenerInternal
{
/** the global graphic state of MWAWGraphicListener */
struct GraphicState {
  //! constructor
  GraphicState() : m_box(), m_sentListMarkers(), m_subDocuments(), m_interface()
  {
  }
  //! destructor
  ~GraphicState()
  {
  }
  //! clear the different data
  void clear()
  {
    m_box=Box2f();
    m_sentListMarkers.clear();
    m_subDocuments.clear();
  }
  /** the graphic bdbox */
  Box2f m_box;
  /// the list of marker corresponding to sent list
  std::vector<int> m_sentListMarkers;
  //! the list of actual subdocument
  std::vector<MWAWSubDocumentPtr> m_subDocuments;
  /// the property handler
  shared_ptr<librevenge::RVNGDrawingInterface> m_interface;
};

/** the state of a MWAWGraphicListener */
struct State {
  //! constructor
  State();
  //! destructor
  ~State() { }

  //! the origin position
  Vec2f m_origin;
  //! a buffer to stored the text
  librevenge::RVNGString m_textBuffer;

  //! the font
  MWAWFont m_font;
  //! the paragraph
  MWAWParagraph m_paragraph;

  shared_ptr<MWAWList> m_list;

  bool m_isGraphicStarted;
  bool m_isTextZoneOpened;
  bool m_isFrameOpened;

  bool m_isSpanOpened;
  bool m_isParagraphOpened;
  bool m_isListElementOpened;

  bool m_firstParagraphInPageSpan;

  std::vector<bool> m_listOrderedLevels; //! a stack used to know what is open

  bool m_inLink;
  bool m_inSubDocument;
  libmwaw::SubDocumentType m_subDocumentType;

private:
  State(const State &);
  State &operator=(const State &);
};

State::State() : m_origin(0,0),
  m_textBuffer(""), m_font(20,12)/* default time 12 */, m_paragraph(), m_list(),
  m_isGraphicStarted(false), m_isTextZoneOpened(false), m_isFrameOpened(false),
  m_isSpanOpened(false), m_isParagraphOpened(false), m_isListElementOpened(false),
  m_firstParagraphInPageSpan(true), m_listOrderedLevels(),
  m_inLink(false), m_inSubDocument(false), m_subDocumentType(libmwaw::DOC_NONE)
{
}
}

MWAWGraphicListener::MWAWGraphicListener(MWAWParserState &parserState, std::vector<MWAWPageSpan> const &, librevenge::RVNGDrawingInterface *documentInterface) : MWAWBasicListener(),
  m_gs(new MWAWGraphicListenerInternal::GraphicState), m_ps(new MWAWGraphicListenerInternal::State), m_psStack(), m_parserState(parserState)
{
  m_gs->m_interface = shared_ptr<librevenge::RVNGDrawingInterface>(documentInterface, MWAW_shared_ptr_noop_deleter<librevenge::RVNGDrawingInterface>());
}

MWAWGraphicListener::MWAWGraphicListener(MWAWParserState &parserState) : MWAWBasicListener(),
  m_gs(), m_ps(new MWAWGraphicListenerInternal::State), m_psStack(), m_parserState(parserState)
{
}

MWAWGraphicListener::~MWAWGraphicListener()
{
}

///////////////////
// text data
///////////////////
void MWAWGraphicListener::insertChar(uint8_t character)
{
  if (!m_ps->m_isTextZoneOpened) {
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
  if (!m_ps->m_isTextZoneOpened) {
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
  if (!m_ps->m_isTextZoneOpened) {
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
  if (!m_ps->m_isTextZoneOpened) {
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
  if (!m_ps->m_isTextZoneOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertUnicodeString: called outside a text zone\n"));
    return;
  }
  if (!m_ps->m_isSpanOpened) _openSpan();
  m_ps->m_textBuffer.append(str);
}

void MWAWGraphicListener::insertEOL(bool soft)
{
  if (!m_ps->m_isTextZoneOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertEOL: called outside a text zone\n"));
    return;
  }
  if (!m_ps->m_isParagraphOpened && !m_ps->m_isListElementOpened)
    _openSpan();
  if (soft) {
    _flushText();
    m_gs->m_interface->insertLineBreak();
  }
  else if (m_ps->m_isParagraphOpened)
    _closeParagraph();

  // sub/superscript must not survive a new line
  m_ps->m_font.set(MWAWFont::Script());
}

void MWAWGraphicListener::insertTab()
{
  if (!m_ps->m_isTextZoneOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertTab: called outside a text zone\n"));
    return;
  }
  if (!m_ps->m_isSpanOpened) _openSpan();
  _flushText();
  m_gs->m_interface->insertTab();
}

///////////////////
// font/paragraph function
///////////////////
void MWAWGraphicListener::setFont(MWAWFont const &font)
{
  if (!m_ps->m_isTextZoneOpened) {
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
  if (!m_ps->m_isTextZoneOpened) {
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
  if (!m_ps->m_isTextZoneOpened) {
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
    m_gs->m_interface->insertField(propList);
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
  if (!m_ps->m_isTextZoneOpened) {
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
  m_gs->m_interface->openLink(propList);
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
  m_gs->m_interface->closeLink();
  _popParsingState();
}

///////////////////
// document
///////////////////
void MWAWGraphicListener::startGraphic(Box2f const &bdbox)
{
  if (m_ps->m_isGraphicStarted) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::startGraphic: the graphic is already started\n"));
    return;
  }
  if (m_gs && m_gs->m_interface)
    m_gs->clear();
  else {
    m_gs.reset(new MWAWGraphicListenerInternal::GraphicState);
    m_gs->m_interface.reset(new MWAWGraphicEncoder);
  }
  m_gs->m_box=bdbox;
  m_ps->m_isGraphicStarted = true;
  m_ps->m_origin=bdbox[0];

  m_gs->m_interface->startDocument(librevenge::RVNGPropertyList());
  librevenge::RVNGPropertyList list;
  list.insert("svg:x",bdbox[0].x(), librevenge::RVNG_POINT);
  list.insert("svg:y",bdbox[0].y(), librevenge::RVNG_POINT);
  list.insert("svg:width",bdbox.size().x(), librevenge::RVNG_POINT);
  list.insert("svg:height",bdbox.size().y(), librevenge::RVNG_POINT);
  list.insert("librevenge:enforce-frame",true);
  m_gs->m_interface->startPage(list);
}

void MWAWGraphicListener::endGraphic()
{
  if (!m_ps->m_isGraphicStarted) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::endGraphic: the graphic is not started\n"));
    return;
  }
  if (m_ps->m_inSubDocument) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::endGraphic: we are in a sub document\n"));
    return;
  }

  if (m_ps->m_isTextZoneOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::endGraphic: we are in a text zone\n"));
    if (m_ps->m_isParagraphOpened)
      _closeParagraph();
    m_ps->m_paragraph.m_listLevelIndex = 0;
    _changeList(); // flush the list exterior
  }
  m_gs->m_interface->endPage();
  m_gs->m_interface->endDocument();
  m_ps->m_isGraphicStarted = false;
  m_gs.reset();
}

bool MWAWGraphicListener::endGraphic(librevenge::RVNGBinaryData &data, std::string &mimeType)
{
  if (!m_ps->m_isGraphicStarted) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::endGraphic: the graphic is not started\n"));
    return false;
  }
  if (m_ps->m_inSubDocument) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::endGraphic: we are in a sub document\n"));
    return false;
  }

  if (m_ps->m_isTextZoneOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::endGraphic: we are in a text zone\n"));
    if (m_ps->m_isParagraphOpened)
      _closeParagraph();
    m_ps->m_paragraph.m_listLevelIndex = 0;
    _changeList(); // flush the list exterior
  }
  m_gs->m_interface->endPage();
  m_gs->m_interface->endDocument();
  MWAWGraphicEncoder *interface=dynamic_cast<MWAWGraphicEncoder *>(m_gs->m_interface.get());
  if (!interface) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::endGraphic: oops the interface is not a property handler\n"));
    m_ps->m_isGraphicStarted = false;
    m_gs.reset();
    return false;
  }
  bool ok=interface->getBinaryResult(data, mimeType);
  m_ps->m_isGraphicStarted = false;
  m_gs.reset();
  return ok;
}

///////////////////
// document
///////////////////
bool MWAWGraphicListener::isDocumentStarted() const
{
  return m_ps->m_isGraphicStarted;
}

bool MWAWGraphicListener::canWriteText() const
{
  return m_ps->m_isGraphicStarted && m_ps->m_isTextZoneOpened;
}

Box2f const &MWAWGraphicListener::getGraphicBdBox()
{
  return m_gs->m_box;
}

///////////////////
// paragraph
///////////////////
void MWAWGraphicListener::_openParagraph()
{
  if (!m_ps->m_isTextZoneOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::_openParagraph: called outsize a text zone\n"));
    return;
  }
  if (m_ps->m_isParagraphOpened || m_ps->m_isListElementOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::_openParagraph: a paragraph (or a list) is already opened"));
    return;
  }

  librevenge::RVNGPropertyList propList;
  m_ps->m_paragraph.addTo(propList, false);
  m_gs->m_interface->openParagraph(propList);

  _resetParagraphState();
  m_ps->m_firstParagraphInPageSpan = false;
}

void MWAWGraphicListener::_closeParagraph()
{
  if (!m_ps->m_isTextZoneOpened) {
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
    m_gs->m_interface->closeParagraph();
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
  if (!m_ps->m_isTextZoneOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::_openListElement: called outsize a text zone\n"));
    return;
  }
  if (m_ps->m_isParagraphOpened || m_ps->m_isListElementOpened)
    return;

  librevenge::RVNGPropertyList propList;
  m_ps->m_paragraph.addTo(propList,false);

  // check if we must change the start value
  int startValue=m_ps->m_paragraph.m_listStartValue.get();
  if (startValue > 0 && m_ps->m_list && m_ps->m_list->getStartValueForNextElement() != startValue) {
    propList.insert("text:start-value", startValue);
    m_ps->m_list->setStartValueForNextElement(startValue);
  }

  if (m_ps->m_list) m_ps->m_list->openElement();
  m_gs->m_interface->openListElement(propList);
  _resetParagraphState(true);
}

void MWAWGraphicListener::_closeListElement()
{
  if (m_ps->m_isListElementOpened) {
    if (m_ps->m_isSpanOpened)
      _closeSpan();

    if (m_ps->m_list) m_ps->m_list->closeElement();
    m_gs->m_interface->closeListElement();
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
  if (!m_ps->m_isTextZoneOpened) {
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
      m_gs->m_interface->closeOrderedListLevel();
    else
      m_gs->m_interface->closeUnorderedListLevel();
  }

  if (newLevel) {
    shared_ptr<MWAWList> theList;

    theList=m_parserState.m_listManager->getList(newListId);
    if (!theList) {
      MWAW_DEBUG_MSG(("MWAWGraphicListener::_changeList: can not find any list\n"));
      m_ps->m_listOrderedLevels.resize(actualLevel);
      return;
    }
    m_parserState.m_listManager->needToSend(newListId, m_gs->m_sentListMarkers);
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
      m_gs->m_interface->openOrderedListLevel(level);
    else
      m_gs->m_interface->openUnorderedListLevel(level);
  }
}

///////////////////
// span
///////////////////
void MWAWGraphicListener::_openSpan()
{
  if (!m_ps->m_isTextZoneOpened) {
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

  m_gs->m_interface->openSpan(propList);

  m_ps->m_isSpanOpened = true;
}

void MWAWGraphicListener::_closeSpan()
{
  if (!m_ps->m_isTextZoneOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::_closeSpan: called outsize a text zone\n"));
    return;
  }
  if (!m_ps->m_isSpanOpened)
    return;

  _flushText();
  m_gs->m_interface->closeSpan();
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
        m_gs->m_interface->insertText(tmpText);
        tmpText.clear();
      }
      m_gs->m_interface->insertSpace();
    }
    else
      tmpText.append(i());
  }
  m_gs->m_interface->insertText(tmpText);
  m_ps->m_textBuffer.clear();
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
  if (!m_ps->m_isGraphicStarted) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertPicture: the graphic is not started\n"));
    return;
  }
  if (m_ps->m_isFrameOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertPicture: a frame is already open\n"));
    return;
  }

  librevenge::RVNGPropertyList list, shapePList;
  style.addTo(list, shape.getType()==MWAWGraphicShape::Line);
  m_gs->m_interface->setStyle(list);
  switch (shape.addTo(bdbox[0]-m_ps->m_origin, style.hasSurface(), shapePList)) {
  case MWAWGraphicShape::C_Ellipse:
    m_gs->m_interface->drawEllipse(shapePList);
    break;
  case MWAWGraphicShape::C_Path:
    m_gs->m_interface->drawPath(shapePList);
    break;
  case MWAWGraphicShape::C_Polyline:
    m_gs->m_interface->drawPolyline(shapePList);
    break;
  case MWAWGraphicShape::C_Polygon:
    m_gs->m_interface->drawPolygon(shapePList);
    break;
  case MWAWGraphicShape::C_Rectangle:
    m_gs->m_interface->drawRectangle(shapePList);
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
  if (!m_ps->m_isGraphicStarted) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertPicture: the graphic is not started\n"));
    return;
  }
  if (m_ps->m_isFrameOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertPicture: a frame is already open\n"));
    return;
  }
  librevenge::RVNGPropertyList list;
  style.addTo(list);
  m_gs->m_interface->setStyle(list);

  list.clear();
  Vec2f pt=bdbox[0]-m_ps->m_origin;
  list.insert("svg:x",pt.x(), librevenge::RVNG_POINT);
  list.insert("svg:y",pt.y(), librevenge::RVNG_POINT);
  pt=bdbox.size();
  list.insert("svg:width",pt.x(), librevenge::RVNG_POINT);
  list.insert("svg:height",pt.y(), librevenge::RVNG_POINT);
  list.insert("librevenge:mime-type", type.c_str());
  list.insert("office:binary-data", binaryData);
  m_gs->m_interface->drawGraphicObject(list);
}

void MWAWGraphicListener::insertTextBox
(Box2f const &bdbox, MWAWSubDocumentPtr subDocument, MWAWGraphicStyle const &style)
{
  if (!m_ps->m_isGraphicStarted) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertTextBox: the graphic is not started\n"));
    return;
  }
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
  m_gs->m_interface->startTextObject(propList);
  handleSubDocument(bdbox[0], subDocument, libmwaw::DOC_TEXT_BOX);
  m_gs->m_interface->endTextObject();
  closeFrame();
}

void MWAWGraphicListener::insertGroup(Box2f const &bdbox, MWAWSubDocumentPtr subDocument)
{
  if (!m_ps->m_isGraphicStarted || m_ps->m_isTextZoneOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::insertGroup: can not insert a group\n"));
    return;
  }
  handleSubDocument(bdbox[0], subDocument, libmwaw::DOC_GRAPHIC_GROUP);
}

///////////////////
// frame
///////////////////
bool MWAWGraphicListener::openFrame()
{
  if (!m_ps->m_isGraphicStarted) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::openFrame: the graphic is not started\n"));
    return false;
  }
  if (m_ps->m_isFrameOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::openFrame: called but a frame is already opened\n"));
    return false;
  }
  m_ps->m_isFrameOpened = true;
  return true;
}

void MWAWGraphicListener::closeFrame()
{
  if (!m_ps->m_isGraphicStarted)
    return;
  if (!m_ps->m_isFrameOpened) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::closeFrame: called but no frame is already opened\n"));
    return;
  }
  m_ps->m_isFrameOpened = false;
}

void MWAWGraphicListener::_handleFrameParameters(librevenge::RVNGPropertyList &list, Box2f const &bdbox, MWAWGraphicStyle const &style)
{
  if (!m_ps->m_isGraphicStarted)
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
    m_gs->m_interface->setStyle(rectList);
    rectList.clear();
    rectList.insert("svg:x",pt[0], librevenge::RVNG_POINT);
    rectList.insert("svg:y",pt[1], librevenge::RVNG_POINT);
    rectList.insert("svg:width",size.x()>0 ? size.x() : -size.x(), librevenge::RVNG_POINT);
    rectList.insert("svg:height",size.y()>0 ? size.y() : -size.y(), librevenge::RVNG_POINT);
    m_gs->m_interface->drawRectangle(rectList);

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
  if (!m_ps->m_isGraphicStarted) {
    MWAW_DEBUG_MSG(("MWAWGraphicListener::handleSubDocument: the graphic is not started\n"));
    return;
  }
  Vec2f actOrigin=m_ps->m_origin;
  _pushParsingState();
  m_ps->m_isGraphicStarted=true;
  m_ps->m_origin=actOrigin-orig;
  _startSubDocument();
  m_ps->m_subDocumentType = subDocumentType;

  m_ps->m_list.reset();
  if (subDocumentType==libmwaw::DOC_TEXT_BOX)
    m_ps->m_isTextZoneOpened=true;

  // Check whether the document is calling itself
  bool sendDoc = true;
  for (size_t i = 0; i < m_gs->m_subDocuments.size(); i++) {
    if (!subDocument)
      break;
    if (subDocument == m_gs->m_subDocuments[i]) {
      MWAW_DEBUG_MSG(("MWAWGraphicListener::handleSubDocument: recursif call, stop...\n"));
      sendDoc = false;
      break;
    }
  }
  if (sendDoc) {
    if (subDocument) {
      m_gs->m_subDocuments.push_back(subDocument);
      shared_ptr<MWAWGraphicListener> listen(this, MWAW_shared_ptr_noop_deleter<MWAWGraphicListener>());
      try {
        subDocument->parseGraphic(listen, subDocumentType);
      }
      catch (...) {
        MWAW_DEBUG_MSG(("Works: MWAWGraphicListener::handleSubDocument exception catched \n"));
      }
      m_gs->m_subDocuments.pop_back();
    }
  }

  _endSubDocument();
  _popParsingState();
}

bool MWAWGraphicListener::isSubDocumentOpened(libmwaw::SubDocumentType &subdocType) const
{
  if (!m_ps->m_isGraphicStarted || !m_ps->m_inSubDocument)
    return false;
  subdocType = m_ps->m_subDocumentType;
  return true;
}

void MWAWGraphicListener::_startSubDocument()
{
  if (!m_ps->m_isGraphicStarted) return;
  m_ps->m_inSubDocument = true;
}

void MWAWGraphicListener::_endSubDocument()
{
  if (!m_ps->m_isGraphicStarted || !m_ps->m_isTextZoneOpened) return;
  if (m_ps->m_isParagraphOpened)
    _closeParagraph();
  m_ps->m_paragraph.m_listLevelIndex=0;
  _changeList(); // flush the list exterior
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
