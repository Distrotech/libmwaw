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

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWPageSpan.hxx"

/** Internal: the structures of a MWAWPageSpan */
namespace MWAWPageSpanInternal
{
////////////////////////////////////////
//! Internal: the subdocument of a MWParser
class SubDocument : public MWAWSubDocument
{
public:
  //! constructor
  SubDocument(MWAWHeaderFooter const &headerFooter) :
    MWAWSubDocument(0, MWAWInputStreamPtr(), MWAWEntry()), m_headerFooter(headerFooter) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const {
    if (MWAWSubDocument::operator!=(doc)) return true;
    SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
    if (!sDoc) return true;
    if (m_headerFooter != sDoc->m_headerFooter) return true;
    return false;
  }

  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  //! the header footer
  MWAWHeaderFooter const &m_headerFooter;
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("MWAWPageSpanInternal::SubDocument::parse: no listener\n"));
    return;
  }
  if (m_headerFooter.m_pageNumberPosition >= MWAWHeaderFooter::TopLeft &&
      m_headerFooter.m_pageNumberPosition <= MWAWHeaderFooter::TopRight)
    m_headerFooter.insertPageNumberParagraph(listener.get());
  if (m_headerFooter.m_subDocument)
    m_headerFooter.m_subDocument->parse(listener, type);
  if (m_headerFooter.m_pageNumberPosition >= MWAWHeaderFooter::BottomLeft &&
      m_headerFooter.m_pageNumberPosition <= MWAWHeaderFooter::BottomRight)
    m_headerFooter.insertPageNumberParagraph(listener.get());
}
}

// ----------------- MWAWHeaderFooter ------------------------
MWAWHeaderFooter::MWAWHeaderFooter(MWAWHeaderFooter::Type const type, MWAWHeaderFooter::Occurence const occurence) :
  m_type(type), m_occurence(occurence), m_height(0),
  m_pageNumberPosition(MWAWHeaderFooter::None), m_pageNumberType(libmwaw::ARABIC),
  m_pageNumberFont(20,12), m_subDocument()
{
}

MWAWHeaderFooter::~MWAWHeaderFooter()
{
}

bool MWAWHeaderFooter::operator==(MWAWHeaderFooter const &hf) const
{
  if (&hf==this) return true;
  if (m_type != hf.m_type)
    return false;
  if (m_type == UNDEF)
    return true;
  if (m_occurence != hf.m_occurence)
    return false;
  if (m_height < hf.m_height || m_height > hf.m_height)
    return false;
  if (m_pageNumberPosition != hf.m_pageNumberPosition ||
      m_pageNumberType != hf.m_pageNumberType ||
      m_pageNumberFont != hf.m_pageNumberFont)
    return false;
  if (!m_subDocument)
    return !hf.m_subDocument;
  if (*m_subDocument.get() != hf.m_subDocument)
    return false;
  return true;
}

// send data to the listener
void MWAWHeaderFooter::send(MWAWContentListener *listener) const
{
  if (m_type == UNDEF)
    return;
  if (!listener) {
    MWAW_DEBUG_MSG(("MWAWHeaderFooter::send: called without listener\n"));
    return;
  }
  librevenge::RVNGPropertyList propList;
  switch (m_occurence) {
  case ODD:
    propList.insert("librevenge:occurence", "odd");
    break;
  case EVEN:
    propList.insert("librevenge:occurence", "even");
    break;
  case ALL:
    propList.insert("librevenge:occurence", "all");
    break;
  case NEVER:
  default:
    break;
  }
  if (m_pageNumberPosition!=None) {
    shared_ptr<MWAWPageSpanInternal::SubDocument> doc
    (new MWAWPageSpanInternal::SubDocument(*this));
    if (m_type == HEADER)
      listener->insertHeader(doc,propList);
    else
      listener->insertFooter(doc,propList);
    return;
  }
  if (m_type == HEADER)
    listener->insertHeader(m_subDocument,propList);
  else
    listener->insertFooter(m_subDocument,propList);
}

void MWAWHeaderFooter::insertPageNumberParagraph(MWAWContentListener *listener) const
{
  MWAWParagraph para;
  para.m_justify = MWAWParagraph::JustificationCenter;
  switch (m_pageNumberPosition) {
  case TopLeft:
  case BottomLeft:
    para.m_justify = MWAWParagraph::JustificationLeft;
    break;
  case TopRight:
  case BottomRight:
    para.m_justify = MWAWParagraph::JustificationRight;
    break;
  case TopCenter:
  case BottomCenter:
  case None:
    break;
  default:
    MWAW_DEBUG_MSG(("MWAWHeaderFooter::insertPageNumberParagraph: unexpected value\n"));
    break;
  }
  listener->setParagraph(para);
  listener->setFont(m_pageNumberFont);
  if (listener->isParagraphOpened())
    listener->insertEOL();

  MWAWField field(MWAWField::PageNumber);
  field.m_numberingType=m_pageNumberType;
  listener->insertField(field);
}

// ----------------- MWAWPageSpan ------------------------
MWAWPageSpan::MWAWPageSpan() :
  m_formLength(11.0), m_formWidth(8.5), m_formOrientation(MWAWPageSpan::PORTRAIT),
  m_backgroundColor(MWAWColor::white()),
  m_pageNumber(-1),
  m_headerFooterList(),
  m_pageSpan(1)
{
  for (int i = 0; i < 4; i++) m_margins[i] = 1.0;
}

MWAWPageSpan::~MWAWPageSpan()
{
}

void MWAWPageSpan::setHeaderFooter(MWAWHeaderFooter const &hF)
{
  MWAWHeaderFooter::Type const type=hF.m_type;
  switch (hF.m_occurence) {
  case MWAWHeaderFooter::NEVER:
    removeHeaderFooter(type, MWAWHeaderFooter::ALL);
  case MWAWHeaderFooter::ALL:
    removeHeaderFooter(type, MWAWHeaderFooter::ODD);
    removeHeaderFooter(type, MWAWHeaderFooter::EVEN);
    break;
  case MWAWHeaderFooter::ODD:
    removeHeaderFooter(type, MWAWHeaderFooter::ALL);
    break;
  case MWAWHeaderFooter::EVEN:
    removeHeaderFooter(type, MWAWHeaderFooter::ALL);
    break;
  default:
    break;
  }
  int pos = getHeaderFooterPosition(hF.m_type, hF.m_occurence);
  if (pos != -1)
    m_headerFooterList[size_t(pos)]=hF;

  bool containsHFLeft = containsHeaderFooter(type, MWAWHeaderFooter::ODD);
  bool containsHFRight = containsHeaderFooter(type, MWAWHeaderFooter::EVEN);

  if (containsHFLeft && !containsHFRight) {
    MWAW_DEBUG_MSG(("Inserting dummy header right\n"));
    MWAWHeaderFooter dummy(type, MWAWHeaderFooter::EVEN);
    pos = getHeaderFooterPosition(type, MWAWHeaderFooter::EVEN);
    if (pos != -1)
      m_headerFooterList[size_t(pos)]=MWAWHeaderFooter(type, MWAWHeaderFooter::EVEN);
  } else if (!containsHFLeft && containsHFRight) {
    MWAW_DEBUG_MSG(("Inserting dummy header left\n"));
    pos = getHeaderFooterPosition(type, MWAWHeaderFooter::ODD);
    if (pos != -1)
      m_headerFooterList[size_t(pos)]=MWAWHeaderFooter(type, MWAWHeaderFooter::ODD);
  }
}

void MWAWPageSpan::checkMargins()
{
  if (m_margins[libmwaw::Left]+m_margins[libmwaw::Right] > 0.95*m_formWidth) {
    MWAW_DEBUG_MSG(("MWAWPageSpan::checkMargins: left/right margins seems bad\n"));
    m_margins[libmwaw::Left] = m_margins[libmwaw::Right] = 0.05*m_formWidth;
  }
  if (m_margins[libmwaw::Top]+m_margins[libmwaw::Bottom] > 0.95*m_formLength) {
    MWAW_DEBUG_MSG(("MWAWPageSpan::checkMargins: top/bottom margins seems bad\n"));
    m_margins[libmwaw::Top] = m_margins[libmwaw::Bottom] = 0.05*m_formLength;
  }
}

void MWAWPageSpan::sendHeaderFooters(MWAWContentListener *listener) const
{
  if (!listener) {
    MWAW_DEBUG_MSG(("MWAWPageSpan::sendHeaderFooters: no listener\n"));
    return;
  }

  for (size_t i = 0; i < m_headerFooterList.size(); i++) {
    MWAWHeaderFooter const &hf = m_headerFooterList[i];
    if (!hf.isDefined()) continue;
    hf.send(listener);
  }
}

void MWAWPageSpan::getPageProperty(librevenge::RVNGPropertyList &propList) const
{
  propList.insert("librevenge:num-pages", getPageSpan());

  propList.insert("fo:page-height", getFormLength());
  propList.insert("fo:page-width", getFormWidth());
  if (getFormOrientation() == LANDSCAPE)
    propList.insert("style:print-orientation", "landscape");
  else
    propList.insert("style:print-orientation", "portrait");
  propList.insert("fo:margin-left", getMarginLeft());
  propList.insert("fo:margin-right", getMarginRight());
  propList.insert("fo:margin-top", getMarginTop());
  propList.insert("fo:margin-bottom", getMarginBottom());
  if (!m_backgroundColor.isWhite())
    propList.insert("fo:background-color", m_backgroundColor.str().c_str());
}


bool MWAWPageSpan::operator==(shared_ptr<MWAWPageSpan> const &page2) const
{
  if (!page2) return false;
  if (page2.get() == this) return true;
  if (m_formLength < page2->m_formLength || m_formLength > page2->m_formLength ||
      m_formWidth < page2->m_formWidth || m_formWidth > page2->m_formWidth ||
      m_formOrientation != page2->m_formOrientation)
    return false;
  if (getMarginLeft() < page2->getMarginLeft() || getMarginLeft() > page2->getMarginLeft() ||
      getMarginRight() < page2->getMarginRight() || getMarginRight() > page2->getMarginRight() ||
      getMarginTop() < page2->getMarginTop() || getMarginTop() > page2->getMarginTop() ||
      getMarginBottom() < page2->getMarginBottom() || getMarginBottom() > page2->getMarginBottom())
    return false;
  if (backgroundColor() != page2->backgroundColor())
    return false;

  if (getPageNumber() != page2->getPageNumber())
    return false;

  size_t numHF = m_headerFooterList.size();
  size_t numHF2 = page2->m_headerFooterList.size();
  for (size_t i = numHF; i < numHF2; i++) {
    if (page2->m_headerFooterList[i].isDefined())
      return false;
  }
  for (size_t i = numHF2; i < numHF; i++) {
    if (m_headerFooterList[i].isDefined())
      return false;
  }
  if (numHF2 < numHF) numHF = numHF2;
  for (size_t i = 0; i < numHF; i++) {
    if (m_headerFooterList[i] != page2->m_headerFooterList[i])
      return false;
  }
  MWAW_DEBUG_MSG(("WordPerfect: MWAWPageSpan == comparison finished, found no differences\n"));

  return true;
}

// -------------- manage header footer list ------------------
void MWAWPageSpan::removeHeaderFooter(MWAWHeaderFooter::Type type, MWAWHeaderFooter::Occurence occurence)
{
  int pos = getHeaderFooterPosition(type, occurence);
  if (pos == -1) return;
  m_headerFooterList[size_t(pos)]=MWAWHeaderFooter();
}

bool MWAWPageSpan::containsHeaderFooter(MWAWHeaderFooter::Type type, MWAWHeaderFooter::Occurence occurence)
{
  int pos = getHeaderFooterPosition(type, occurence);
  if (pos == -1 || !m_headerFooterList[size_t(pos)].isDefined()) return false;
  return true;
}

int MWAWPageSpan::getHeaderFooterPosition(MWAWHeaderFooter::Type type, MWAWHeaderFooter::Occurence occurence)
{
  int typePos = 0, occurencePos = 0;
  switch(type) {
  case MWAWHeaderFooter::HEADER:
    typePos = 0;
    break;
  case MWAWHeaderFooter::FOOTER:
    typePos = 1;
    break;
  case MWAWHeaderFooter::UNDEF:
  default:
    MWAW_DEBUG_MSG(("MWAWPageSpan::getVectorPosition: unknown type\n"));
    return -1;
  }
  switch(occurence) {
  case MWAWHeaderFooter::ALL:
    occurencePos = 0;
    break;
  case MWAWHeaderFooter::ODD:
    occurencePos = 1;
    break;
  case MWAWHeaderFooter::EVEN:
    occurencePos = 2;
    break;
  case MWAWHeaderFooter::NEVER:
  default:
    MWAW_DEBUG_MSG(("MWAWPageSpan::getVectorPosition: unknown occurence\n"));
    return -1;
  }
  size_t res = size_t(typePos*3+occurencePos);
  if (res >= m_headerFooterList.size())
    m_headerFooterList.resize(res+1);
  return int(res);
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
