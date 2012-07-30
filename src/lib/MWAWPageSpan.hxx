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

#ifndef MWAWPAGESPAN_H
#define MWAWPAGESPAN_H
#include <vector>
#include "libmwaw_internal.hxx"

class WPXPropertyList;
class WPXDocumentInterface;
class WPXDocumentProperty;

class MWAWContentListener;

class MWAWSubDocument;
typedef shared_ptr<MWAWSubDocument> MWAWSubDocumentPtr;

namespace MWAWPageSpanInternal
{
class HeaderFooter;
typedef shared_ptr<HeaderFooter> HeaderFooterPtr;
}

class MWAWPageSpan
{
  friend class MWAWContentListener;
public:
  enum FormOrientation { PORTRAIT, LANDSCAPE };

  enum HeaderFooterType { HEADER, FOOTER };
  enum HeaderFooterOccurence { ODD, EVEN, ALL, NEVER };

  enum PageNumberPosition { None = 0, TopLeft, TopCenter, TopRight, TopLeftAndRight, TopInsideLeftAndRight,
                            BottomLeft, BottomCenter, BottomRight, BottomLeftAndRight, BottomInsideLeftAndRight
                          };
public:
  MWAWPageSpan();
  virtual ~MWAWPageSpan();

  double getFormLength() const {
    return m_formLength;
  }
  double getFormWidth() const {
    return m_formWidth;
  }
  FormOrientation getFormOrientation() const {
    return m_formOrientation;
  }
  double getMarginLeft() const {
    return m_marginLeft;
  }
  double getMarginRight() const {
    return m_marginRight;
  }
  double getMarginTop() const {
    return m_marginTop;
  }
  double getMarginBottom() const {
    return m_marginBottom;
  }
  PageNumberPosition getPageNumberPosition() const {
    return m_pageNumberPosition;
  }
  int getPageNumber() const {
    return m_pageNumber;
  }
  libmwaw::NumberingType getPageNumberingType() const {
    return m_pageNumberingType;
  }
  double getPageNumberingFontSize() const {
    return m_pageNumberingFontSize;
  }
  WPXString getPageNumberingFontName() const {
    return m_pageNumberingFontName;
  }
  int getPageSpan() const {
    return m_pageSpan;
  }
  const std::vector<MWAWPageSpanInternal::HeaderFooterPtr> & getHeaderFooterList() const {
    return m_headerFooterList;
  }

  void setHeaderFooter(const HeaderFooterType type, const HeaderFooterOccurence occurence,
                       MWAWSubDocumentPtr &subDocument);
  void setFormLength(const double formLength) {
    m_formLength = formLength;
  }
  void setFormWidth(const double formWidth) {
    m_formWidth = formWidth;
  }
  void setFormOrientation(const FormOrientation formOrientation) {
    m_formOrientation = formOrientation;
  }
  void setMarginLeft(const double marginLeft) {
    m_marginLeft = marginLeft;
  }
  void setMarginRight(const double marginRight) {
    m_marginRight = marginRight;
  }
  void setMarginTop(const double marginTop) {
    m_marginTop = marginTop;
  }
  void setMarginBottom(const double marginBottom) {
    m_marginBottom = marginBottom;
  }
  void setPageNumberPosition(const PageNumberPosition pageNumberPosition) {
    m_pageNumberPosition = pageNumberPosition;
  }
  void setPageNumber(const int pageNumber) {
    m_pageNumber = pageNumber;
  }
  void setPageNumberingType(const libmwaw::NumberingType pageNumberingType) {
    m_pageNumberingType = pageNumberingType;
  }
  void setPageNumberingFontSize(const double pageNumberingFontSize) {
    m_pageNumberingFontSize = pageNumberingFontSize;
  }
  void setPageNumberingFontName(const WPXString &pageNumberingFontName) {
    m_pageNumberingFontName = pageNumberingFontName;
  }
  void setPageSpan(const int pageSpan) {
    m_pageSpan = pageSpan;
  }

  bool operator==(shared_ptr<MWAWPageSpan> const &pageSpan) const;
  bool operator!=(shared_ptr<MWAWPageSpan> const &pageSpan) const {
    return !operator==(pageSpan);
  }
protected:
  // interface with MWAWContentListener
  void getPageProperty(WPXPropertyList &pList) const;
  void sendHeaderFooters(MWAWContentListener *listener,
                         WPXDocumentInterface *documentInterface);

protected:

  int _getHeaderFooterPosition(HeaderFooterType type, HeaderFooterOccurence occurence);
  void _setHeaderFooter(HeaderFooterType type, HeaderFooterOccurence occurence, MWAWSubDocumentPtr &doc);
  void _removeHeaderFooter(HeaderFooterType type, HeaderFooterOccurence occurence);
  bool _containsHeaderFooter(HeaderFooterType type, HeaderFooterOccurence occurence);

  void _insertPageNumberParagraph(WPXDocumentInterface *documentInterface);
private:
  double m_formLength, m_formWidth;
  FormOrientation m_formOrientation;
  double m_marginLeft, m_marginRight;
  double m_marginTop, m_marginBottom;
  PageNumberPosition m_pageNumberPosition;
  int m_pageNumber;
  libmwaw::NumberingType m_pageNumberingType;
  WPXString m_pageNumberingFontName;
  double m_pageNumberingFontSize;
  std::vector<MWAWPageSpanInternal::HeaderFooterPtr> m_headerFooterList;

  int m_pageSpan;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
