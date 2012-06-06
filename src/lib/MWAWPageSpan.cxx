/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2002 William Lachance (william.lachance@sympatico.ca)
 * Copyright (C) 2002 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2006-2007 Fridrich Strba (fridrich.strba@bluewin.ch)
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
 * For further information visit http://libwpd.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */
#include <libwpd/WPXDocumentInterface.h>
#include <libwpd/WPXProperty.h>

#include "libmwaw_internal.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWPageSpan.hxx"

namespace MWAWPageSpanInternal
{
// intermediate page representation class: for internal use only (by the high-level content/styles listeners). should not be exported.
class HeaderFooter
{
public:
  HeaderFooter(const MWAWPageSpan::HeaderFooterType headerFooterType, const MWAWPageSpan::HeaderFooterOccurence occurence, MWAWSubDocumentPtr &subDoc)  :
    m_type(headerFooterType), m_occurence(occurence), m_subDocument(subDoc) {
  }

  HeaderFooter(const HeaderFooter &headerFooter) :
    m_type(headerFooter.getType()), m_occurence(headerFooter.getOccurence()), m_subDocument(headerFooter.m_subDocument) {
  }

  ~HeaderFooter() {
  }

  MWAWPageSpan::HeaderFooterType getType() const {
    return m_type;
  }
  MWAWPageSpan::HeaderFooterOccurence getOccurence() const {
    return m_occurence;
  }
  MWAWSubDocumentPtr &getSubDocument() {
    return m_subDocument;
  }
  bool operator==(shared_ptr<HeaderFooter> const &headerFooter) const;
  bool operator!=(shared_ptr<HeaderFooter> const &headerFooter) const {
    return !operator==(headerFooter);
  }
private:
  MWAWPageSpan::HeaderFooterType m_type;
  MWAWPageSpan::HeaderFooterOccurence m_occurence;
  MWAWSubDocumentPtr m_subDocument;
};

bool HeaderFooter::operator==(shared_ptr<HeaderFooter> const &hF) const
{
  if (!hF) return false;
  if (m_type != hF.get()->m_type)
    return false;
  if (m_occurence != hF.get()->m_occurence)
    return false;
  if (!m_subDocument)
    return !hF.get()->m_subDocument;
  if (*m_subDocument.get() != hF.get()->m_subDocument)
    return false;
  return true;
}
}

// ----------------- MWAWPageSpan ------------------------
MWAWPageSpan::MWAWPageSpan() :
  m_formLength(11.0),
  m_formWidth(8.5f),
  m_formOrientation(MWAWPageSpan::PORTRAIT),
  m_marginLeft(1.0),
  m_marginRight(1.0),
  m_marginTop(1.0),
  m_marginBottom(1.0),
  m_pageNumberPosition(None),
  m_pageNumber(-1),
  m_pageNumberingType(libmwaw::ARABIC),
  m_pageNumberingFontName("Times New Roman"),
  m_pageNumberingFontSize(12.0),
  m_headerFooterList(),
  m_pageSpan(1)
{
}

MWAWPageSpan::MWAWPageSpan(const MWAWPageSpan &page) :
  m_formLength(page.getFormLength()),
  m_formWidth(page.getFormWidth()),
  m_formOrientation(page.getFormOrientation()),
  m_marginLeft(page.getMarginLeft()),
  m_marginRight(page.getMarginRight()),
  m_marginTop(page.getMarginTop()),
  m_marginBottom(page.getMarginBottom()),
  m_pageNumberPosition(page.getPageNumberPosition()),
  m_pageNumber(page.getPageNumber()),
  m_pageNumberingType(page.getPageNumberingType()),
  m_pageNumberingFontName(page.getPageNumberingFontName()),
  m_pageNumberingFontSize(page.getPageNumberingFontSize()),
  m_headerFooterList(page.getHeaderFooterList()),
  m_pageSpan(page.getPageSpan())
{
}

MWAWPageSpan::~MWAWPageSpan()
{
}

void MWAWPageSpan::setHeaderFooter(const HeaderFooterType type, const HeaderFooterOccurence occurence,
                                   MWAWSubDocumentPtr &subDocument)
{
  MWAWPageSpanInternal::HeaderFooter headerFooter(type, occurence, subDocument);
  switch (occurence) {
  case NEVER:
    _removeHeaderFooter(type, ALL);
  case ALL:
    _removeHeaderFooter(type, ODD);
    _removeHeaderFooter(type, EVEN);
    break;
  case ODD:
    _removeHeaderFooter(type, ALL);
    break;
  case EVEN:
    _removeHeaderFooter(type, ALL);
    break;
  default:
    break;
  }

  _setHeaderFooter(type, occurence, subDocument);

  bool containsHFLeft = _containsHeaderFooter(type, ODD);
  bool containsHFRight = _containsHeaderFooter(type, EVEN);

  //MWAW_DEBUG_MSG(("Contains HFL: %i HFR: %i\n", containsHFLeft, containsHFRight));
  if (containsHFLeft && !containsHFRight) {
    MWAW_DEBUG_MSG(("Inserting dummy header right\n"));
    MWAWSubDocumentPtr dummyDoc;
    _setHeaderFooter(type, EVEN, dummyDoc);
  } else if (!containsHFLeft && containsHFRight) {
    MWAW_DEBUG_MSG(("Inserting dummy header left\n"));
    MWAWSubDocumentPtr dummyDoc;
    _setHeaderFooter(type, ODD, dummyDoc);
  }
}

void MWAWPageSpan::sendHeaderFooters(MWAWContentListener *listener,
                                     WPXDocumentInterface *documentInterface)
{
  if (!listener || !documentInterface) {
    MWAW_DEBUG_MSG(("MWAWPageSpan::sendHeaderFooters: no listener or document interface\n"));
    return;
  }

  bool pageNumberInserted = false;
  for (size_t i = 0; i < m_headerFooterList.size(); i++) {
    MWAWPageSpanInternal::HeaderFooterPtr &hf = m_headerFooterList[i];
    if (!hf) continue;

    WPXPropertyList propList;
    switch (hf->getOccurence()) {
    case MWAWPageSpan::ODD:
      propList.insert("libwpd:occurence", "odd");
      break;
    case MWAWPageSpan::EVEN:
      propList.insert("libwpd:occurence", "even");
      break;
    case MWAWPageSpan::ALL:
      propList.insert("libwpd:occurence", "all");
      break;
    case MWAWPageSpan::NEVER:
    default:
      break;
    }
    bool isHeader = hf->getType() == MWAWPageSpan::HEADER;
    if (isHeader)
      documentInterface->openHeader(propList);
    else
      documentInterface->openFooter(propList);
    if (isHeader && m_pageNumberPosition >= TopLeft &&
        m_pageNumberPosition <= TopInsideLeftAndRight) {
      pageNumberInserted = true;
      _insertPageNumberParagraph(documentInterface);
    }
    listener->handleSubDocument(hf->getSubDocument(), libmwaw::DOC_HEADER_FOOTER);
    if (!isHeader && m_pageNumberPosition >= BottomLeft &&
        m_pageNumberPosition <= BottomInsideLeftAndRight) {
      pageNumberInserted = true;
      _insertPageNumberParagraph(documentInterface);
    }
    if (isHeader)
      documentInterface->closeHeader();
    else
      documentInterface->closeFooter();

    MWAW_DEBUG_MSG(("Header Footer Element: type: %i occurence: %i\n",
                    hf->getType(), hf->getOccurence()));
  }

  if (!pageNumberInserted) {
    WPXPropertyList propList;
    propList.insert("libwpd:occurence", "all");
    if (m_pageNumberPosition >= TopLeft &&
        m_pageNumberPosition <= TopInsideLeftAndRight) {
      documentInterface->openHeader(propList);
      _insertPageNumberParagraph(documentInterface);
      documentInterface->closeHeader();
    } else if (m_pageNumberPosition >= BottomLeft &&
               m_pageNumberPosition <= BottomInsideLeftAndRight) {
      documentInterface->openFooter(propList);
      _insertPageNumberParagraph(documentInterface);
      documentInterface->closeFooter();
    }
  }
}

void MWAWPageSpan::getPageProperty(WPXPropertyList &propList) const
{
  propList.insert("libwpd:num-pages", getPageSpan());

  propList.insert("fo:page-height", getFormLength());
  propList.insert("fo:page-width", getFormWidth());
  if (getFormOrientation() == MWAWPageSpan::MWAWPageSpan::LANDSCAPE)
    propList.insert("style:print-orientation", "landscape");
  else
    propList.insert("style:print-orientation", "portrait");
  propList.insert("fo:margin-left", getMarginLeft());
  propList.insert("fo:margin-right", getMarginRight());
  propList.insert("fo:margin-top", getMarginTop());
  propList.insert("fo:margin-bottom", getMarginBottom());
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

  if (getPageNumberPosition() != page2->getPageNumberPosition())
    return false;

  if (getPageNumber() != page2->getPageNumber())
    return false;

  if (getPageNumberingType() != page2->getPageNumberingType())
    return false;

  if (getPageNumberingFontName() != page2->getPageNumberingFontName() ||
      getPageNumberingFontSize() < page2->getPageNumberingFontSize() ||
      getPageNumberingFontSize() > page2->getPageNumberingFontSize())
    return false;

  size_t numHF = m_headerFooterList.size();
  size_t numHF2 = page2->m_headerFooterList.size();
  for (size_t i = numHF; i < numHF2; i++) {
    if (page2->m_headerFooterList[i])
      return false;
  }
  for (size_t i = numHF2; i < numHF; i++) {
    if (m_headerFooterList[i])
      return false;
  }
  if (numHF2 < numHF) numHF = numHF2;
  for (size_t i = 0; i < numHF; i++) {
    if (!m_headerFooterList[i]) {
      if (page2->m_headerFooterList[i])
        return false;
      continue;
    }
    if (!page2->m_headerFooterList[i])
      return false;
    if (*m_headerFooterList[i] != page2->m_headerFooterList[i])
      return false;
  }
  MWAW_DEBUG_MSG(("WordPerfect: MWAWPageSpan == comparison finished, found no differences\n"));

  return true;
}

void MWAWPageSpan::_insertPageNumberParagraph(WPXDocumentInterface *documentInterface)
{
  WPXPropertyList propList;
  switch (m_pageNumberPosition) {
  case TopLeft:
  case BottomLeft:
    // doesn't require a paragraph prop - it is the default
    propList.insert("fo:text-align", "left");
    break;
  case TopRight:
  case BottomRight:
    propList.insert("fo:text-align", "end");
    break;
  case TopCenter:
  case BottomCenter:
  case None:
    propList.insert("fo:text-align", "center");
    break;
  default:
  case TopLeftAndRight:
  case TopInsideLeftAndRight:
  case BottomLeftAndRight:
  case BottomInsideLeftAndRight:
    MWAW_DEBUG_MSG(("MWAWPageSpan::_insertPageNumberParagraph: unexpected value\n"));
    propList.insert("fo:text-align", "center");
    break;
  }

  documentInterface->openParagraph(propList, WPXPropertyListVector());

  propList.clear();
  propList.insert("style:font-name", m_pageNumberingFontName.cstr());
  propList.insert("fo:font-size", m_pageNumberingFontSize, WPX_POINT);
  documentInterface->openSpan(propList);


  propList.clear();
  propList.insert("style:num-format", libmwaw::numberingTypeToString(m_pageNumberingType).c_str());
  documentInterface->insertField("text:page-number", propList);

  propList.clear();
  documentInterface->closeSpan();

  documentInterface->closeParagraph();
}

// -------------- manage header footer list ------------------
void MWAWPageSpan::_setHeaderFooter(HeaderFooterType type, HeaderFooterOccurence occurence, MWAWSubDocumentPtr &doc)
{
  if (occurence == NEVER) return;

  int pos = _getHeaderFooterPosition(type, occurence);
  if (pos == -1) return;
  m_headerFooterList[size_t(pos)]=MWAWPageSpanInternal::HeaderFooterPtr(new MWAWPageSpanInternal::HeaderFooter(type, occurence, doc));
}

void MWAWPageSpan::_removeHeaderFooter(HeaderFooterType type, HeaderFooterOccurence occurence)
{
  int pos = _getHeaderFooterPosition(type, occurence);
  if (pos == -1) return;
  m_headerFooterList[size_t(pos)].reset();
}

bool MWAWPageSpan::_containsHeaderFooter(HeaderFooterType type, HeaderFooterOccurence occurence)
{
  int pos = _getHeaderFooterPosition(type, occurence);
  if (pos == -1 || ! m_headerFooterList[size_t(pos)]) return false;
  if (!m_headerFooterList[size_t(pos)]->getSubDocument()) return false;
  return true;
}

int MWAWPageSpan::_getHeaderFooterPosition(HeaderFooterType type, HeaderFooterOccurence occurence)
{
  int typePos = 0, occurencePos = 0;
  switch(type) {
  case HEADER:
    typePos = 0;
    break;
  case FOOTER:
    typePos = 1;
    break;
  default:
    MWAW_DEBUG_MSG(("MWAWPageSpan::getVectorPosition: unknown type\n"));
    return -1;
  }
  switch(occurence) {
  case ALL:
    occurencePos = 0;
    break;
  case ODD:
    occurencePos = 1;
    break;
  case EVEN:
    occurencePos = 2;
    break;
  case NEVER:
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
