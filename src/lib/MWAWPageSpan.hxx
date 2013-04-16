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

/** A structure used to defined the page properties */
class MWAWPageSpan
{
  friend class MWAWContentListener;
public:
  /** the page orientation */
  enum FormOrientation { PORTRAIT, LANDSCAPE };
  /** the header/footer type */
  enum HeaderFooterType { HEADER, FOOTER };
  /** the header/footer occurence in the file */
  enum HeaderFooterOccurence { ODD, EVEN, ALL, NEVER };

  /** a fixed page number position */
  enum PageNumberPosition { None = 0, TopLeft, TopCenter, TopRight, TopLeftAndRight, TopInsideLeftAndRight,
                            BottomLeft, BottomCenter, BottomRight, BottomLeftAndRight, BottomInsideLeftAndRight
                          };
public:
  //! constructor
  MWAWPageSpan();
  //! destructor
  virtual ~MWAWPageSpan();

  //! returns the page length
  double getFormLength() const {
    return m_formLength;
  }
  //! returns the page width
  double getFormWidth() const {
    return m_formWidth;
  }
  //! returns the page orientation
  FormOrientation getFormOrientation() const {
    return m_formOrientation;
  }
  //! returns the left margin
  double getMarginLeft() const {
    return m_margins[libmwaw::Left];
  }
  //! returns the right margin
  double getMarginRight() const {
    return m_margins[libmwaw::Right];
  }
  //! returns the top margin
  double getMarginTop() const {
    return m_margins[libmwaw::Top];
  }
  //! returns the bottom margin
  double getMarginBottom() const {
    return m_margins[libmwaw::Bottom];
  }
  //! returns the page length (form width without margin )
  double getPageLength() const {
    return m_formLength-m_margins[libmwaw::Top]-m_margins[libmwaw::Bottom];
  }
  //! returns the page width (form width without margin )
  double getPageWidth() const {
    return m_formWidth-m_margins[libmwaw::Left]-m_margins[libmwaw::Right];
  }
  //! returns the background color
  MWAWColor backgroundColor() const {
    return m_backgroundColor;
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

  //! add a header/footer on some page
  void setHeaderFooter(const HeaderFooterType type, const HeaderFooterOccurence occurence,
                       MWAWSubDocumentPtr &subDocument);
  //! set the total page length
  void setFormLength(const double formLength) {
    m_formLength = formLength;
  }
  //! set the total page width
  void setFormWidth(const double formWidth) {
    m_formWidth = formWidth;
  }
  //! set the form orientation
  void setFormOrientation(const FormOrientation formOrientation) {
    m_formOrientation = formOrientation;
  }
  //! set the page left margin
  void setMarginLeft(const double marginLeft) {
    m_margins[libmwaw::Left] = (marginLeft > 0) ? marginLeft : 0.01;
  }
  //! set the page right margin
  void setMarginRight(const double marginRight) {
    m_margins[libmwaw::Right] = (marginRight > 0) ? marginRight : 0.01;
  }
  //! set the page top margin
  void setMarginTop(const double marginTop) {
    m_margins[libmwaw::Top] =(marginTop > 0) ? marginTop : 0.01;
  }
  //! set the page bottom margin
  void setMarginBottom(const double marginBottom) {
    m_margins[libmwaw::Bottom] = (marginBottom > 0) ? marginBottom : 0.01;
  }
  //! set all the margins
  void setMargins(double margin, int wh=libmwaw::LeftBit|libmwaw::RightBit|libmwaw::TopBit|libmwaw::BottomBit) {
    if (margin <= 0.0) margin = 0.01;
    if (wh&libmwaw::LeftBit)
      m_margins[libmwaw::Left]=margin;
    if (wh&libmwaw::RightBit)
      m_margins[libmwaw::Right]=margin;
    if (wh&libmwaw::TopBit)
      m_margins[libmwaw::Top]=margin;
    if (wh&libmwaw::BottomBit)
      m_margins[libmwaw::Bottom]=margin;
  }
  //! check if the page margins are consistent with the page dimension, if not update them
  void checkMargins();

  //! set the background color
  void setBackgroundColor(MWAWColor color=MWAWColor::white()) {
    m_backgroundColor=color;
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
  /** the form orientation */
  FormOrientation m_formOrientation;
  /** the margins: libmwaw::Left, ... */
  double m_margins[4];
  /** the page background color: default white */
  MWAWColor m_backgroundColor;
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
