/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libwpd
 * Copyright (C) 2002 William Lachance (wrlach@gmail.com)
 * Copyright (C) 2002 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2006 Fridrich Strba (fridrich.strba@bluewin.ch)
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

#ifndef WPXPAGE_H
#define WPXPAGE_H
#include "DMWAWFileStructure.hxx"
#include <vector>
#include "DMWAWTable.hxx"
#include "libmwaw_libwpd.hxx"
#include "DMWAWSubDocument.hxx"

// intermediate page representation class: for internal use only (by the high-level content/styles listeners). should not be exported.

class DMWAWHeaderFooter
{
public:
  DMWAWHeaderFooter(const DMWAWHeaderFooterType headerFooterType, const DMWAWHeaderFooterOccurence occurence,
                    const uint8_t internalType, const DMWAWSubDocument *subDocument, DMWAWTableList tableList);
  DMWAWHeaderFooter(const DMWAWHeaderFooterType headerFooterType, const DMWAWHeaderFooterOccurence occurence,
                    const uint8_t internalType, const DMWAWSubDocument *subDocument);
  DMWAWHeaderFooter(const DMWAWHeaderFooter &headerFooter);
  ~DMWAWHeaderFooter();
  DMWAWHeaderFooter &operator=(const DMWAWHeaderFooter &headerFooter);
  DMWAWHeaderFooterType getType() const {
    return m_type;
  }
  DMWAWHeaderFooterOccurence getOccurence() const {
    return m_occurence;
  }
  uint8_t getInternalType() const {
    return m_internalType;
  }
  const DMWAWSubDocument *getSubDocument() const {
    return m_subDocument;
  }
  DMWAWTableList getTableList() const {
    return m_tableList;
  }

private:
  DMWAWHeaderFooterType m_type;
  DMWAWHeaderFooterOccurence m_occurence;
  uint8_t m_internalType; // for suppression
  const DMWAWSubDocument *m_subDocument;  // for the actual text
  DMWAWTableList m_tableList;
};

class DMWAWPageSpan
{
public:
  DMWAWPageSpan();
  DMWAWPageSpan(const DMWAWPageSpan &page, double paragraphMarginLeft, double paragraphMarginRight);
  DMWAWPageSpan(const DMWAWPageSpan &page);
  virtual ~DMWAWPageSpan();

  bool getPageNumberSuppression() const {
    return m_isPageNumberSuppressed;
  }
  bool getHeaderFooterSuppression(const uint8_t headerFooterType) const {
    if (headerFooterType <= DMWAW_FOOTER_B) return m_isHeaderFooterSuppressed[headerFooterType];
    return false;
  }
  double getFormLength() const {
    return m_formLength;
  }
  double getFormWidth() const {
    return m_formWidth;
  }
  DMWAWFormOrientation getFormOrientation() const {
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
  DMWAWPageNumberPosition getPageNumberPosition() const {
    return m_pageNumberPosition;
  }
  bool getPageNumberOverriden() const {
    return m_isPageNumberOverridden;
  }
  int getPageNumberOverride() const {
    return m_pageNumberOverride;
  }
  DMWAWNumberingType getPageNumberingType() const {
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
  const std::vector<DMWAWHeaderFooter> & getHeaderFooterList() const {
    return m_headerFooterList;
  }

  void setHeaderFooter(const DMWAWHeaderFooterType type, const uint8_t headerFooterType, const DMWAWHeaderFooterOccurence occurence,
                       const DMWAWSubDocument *subDocument, DMWAWTableList tableList);
  void setPageNumberSuppression(const bool suppress) {
    m_isPageNumberSuppressed = suppress;
  }
  void setHeadFooterSuppression(const uint8_t headerFooterType, const bool suppress) {
    m_isHeaderFooterSuppressed[headerFooterType] = suppress;
  }
  void setFormLength(const double formLength) {
    m_formLength = formLength;
  }
  void setFormWidth(const double formWidth) {
    m_formWidth = formWidth;
  }
  void setFormOrientation(const DMWAWFormOrientation formOrientation) {
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
  void setPageNumberPosition(const DMWAWPageNumberPosition pageNumberPosition) {
    m_pageNumberPosition = pageNumberPosition;
  }
  void setPageNumber(const int pageNumberOverride) {
    m_pageNumberOverride = pageNumberOverride;
    m_isPageNumberOverridden = true;
  }
  void setPageNumberingType(const DMWAWNumberingType pageNumberingType) {
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

protected:
  void _removeHeaderFooter(DMWAWHeaderFooterType type, DMWAWHeaderFooterOccurence occurence);
  bool _containsHeaderFooter(DMWAWHeaderFooterType type, DMWAWHeaderFooterOccurence occurence);

private:
  bool m_isHeaderFooterSuppressed[DMWAW_NUM_HEADER_FOOTER_TYPES];
  bool m_isPageNumberSuppressed;
  double m_formLength, m_formWidth;
  DMWAWFormOrientation m_formOrientation;
  double m_marginLeft, m_marginRight;
  double m_marginTop, m_marginBottom;
  DMWAWPageNumberPosition m_pageNumberPosition;
  bool m_isPageNumberOverridden;
  int m_pageNumberOverride;
  DMWAWNumberingType m_pageNumberingType;
  WPXString m_pageNumberingFontName;
  double m_pageNumberingFontSize;
  std::vector<DMWAWHeaderFooter> m_headerFooterList;

  int m_pageSpan;
};

bool operator==(const DMWAWPageSpan &, const DMWAWPageSpan &);

#endif /* WPXPAGE_H */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
