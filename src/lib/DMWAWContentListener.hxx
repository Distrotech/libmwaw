/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libwpd
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

/* OSNOLA :
 * this file and DMWAWContentListener.cpp differ only from
 * libwpd::DMWAWContentListener by
 *   - introduction of a new field m_paragraphLineSpacingUnit
 *   - gestion of  DMWAW_ALL_CAPS_BIT/DMWAW_EMBOSS_BIT/DMWAW_ENGRAVE_BIT bits
 *   - change in DMWAWContentListener::justificationChange
 *
 * the other file WPX... are only basic copy of WPX needed because
 *    they are not exported
 */

#ifndef WPXCONTENTLISTENER_H
#define WPXCONTENTLISTENER_H

#include "DMWAWTable.hxx"
#include <libwpd/WPXPropertyListVector.h>
#include <libwpd/WPXPropertyList.h>
#include "libmwaw_libwpd.hxx"
#include "DMWAWSubDocument.hxx"
#include "DMWAWPageSpan.hxx"
#include <libwpd/WPXDocumentInterface.h>
#include "DMWAWListener.hxx"
#include <vector>
#include <list>
#include <set>
class WPXString;

typedef struct _DMWAWTableDefinition DMWAWTableDefinition;
struct _DMWAWTableDefinition {
  _DMWAWTableDefinition() : m_positionBits(0), m_leftOffset(0.0), m_columns(), m_columnsProperties() {};
  uint8_t m_positionBits;
  double m_leftOffset;
  std::vector < DMWAWColumnDefinition > m_columns;
  std::vector < DMWAWColumnProperties > m_columnsProperties;
};

typedef struct _DMWAWContentParsingState DMWAWContentParsingState;
struct _DMWAWContentParsingState {
  _DMWAWContentParsingState();
  ~_DMWAWContentParsingState();

  uint32_t m_textAttributeBits;
  double m_fontSize;
  WPXString *m_fontName;
  libmwaw_libwpd::RGBSColor *m_fontColor;
  libmwaw_libwpd::RGBSColor *m_highlightColor;

  bool m_isParagraphColumnBreak;
  bool m_isParagraphPageBreak;
  uint8_t m_paragraphJustification;
  uint8_t m_tempParagraphJustification; // TODO: remove this one after the tabs are properly implemented
  double m_paragraphLineSpacing;
  WPXUnit m_paragraphLineSpacingUnit; // OSNOLA

  bool m_isDocumentStarted;
  bool m_isPageSpanOpened;
  bool m_isSectionOpened;
  bool m_isPageSpanBreakDeferred;
  bool m_isHeaderFooterWithoutParagraph;

  bool m_isSpanOpened;
  bool m_isParagraphOpened;
  bool m_isListElementOpened;

  bool m_firstParagraphInPageSpan;

  std::vector<unsigned int> m_numRowsToSkip;
  DMWAWTableDefinition m_tableDefinition;
  int m_currentTableCol;
  int m_currentTableRow;
  int m_currentTableCellNumberInRow;
  bool m_isTableOpened;
  bool m_isTableRowOpened;
  bool m_isTableColumnOpened;
  bool m_isTableCellOpened;
  bool m_wasHeaderRow;
  bool m_isCellWithoutParagraph;
  bool m_isRowWithoutCell;
  uint32_t m_cellAttributeBits;
  uint8_t m_paragraphJustificationBeforeTable;

  unsigned m_currentPage;
  int m_numPagesRemainingInSpan;
  int m_currentPageNumber;

  bool m_sectionAttributesChanged;
  int m_numColumns;
  std::vector < DMWAWColumnDefinition > m_textColumns;
  bool m_isTextColumnWithoutParagraph;

  double m_pageFormLength;
  double m_pageFormWidth;
  DMWAWFormOrientation m_pageFormOrientation;

  double m_pageMarginLeft;
  double m_pageMarginRight;
  double m_pageMarginTop;
  double m_pageMarginBottom;
  double m_paragraphMarginLeft;  // resulting paragraph margin that is one of the paragraph
  double m_paragraphMarginRight; // properties
  double m_paragraphMarginTop;
  WPXUnit m_paragraphMarginTopUnit; // OSNOLA
  double m_paragraphMarginBottom;
  WPXUnit m_paragraphMarginBottomUnit; // OSNOLA
  double m_leftMarginByPageMarginChange;  // part of the margin due to the PAGE margin change
  double m_rightMarginByPageMarginChange; // inside a page that already has content.
  double m_sectionMarginLeft;  // In multicolumn sections, the above two will be rather interpreted
  double m_sectionMarginRight; // as section margin change
  double m_leftMarginByParagraphMarginChange;  // part of the margin due to the PARAGRAPH
  double m_rightMarginByParagraphMarginChange; // margin change (in WP6)
  double m_leftMarginByTabs;  // part of the margin due to the LEFT or LEFT/RIGHT Indent; the
  double m_rightMarginByTabs; // only part of the margin that is reset at the end of a paragraph

  double m_listReferencePosition; // position from the left page margin of the list number/bullet
  double m_listBeginPosition; // position from the left page margin of the beginning of the list

  double m_paragraphTextIndent; // resulting first line indent that is one of the paragraph properties
  double m_textIndentByParagraphIndentChange; // part of the indent due to the PARAGRAPH indent (WP6???)
  double m_textIndentByTabs; // part of the indent due to the "Back Tab" or "Left Tab"

  uint8_t m_currentListLevel;

  uint16_t m_alignmentCharacter;
  std::vector<DMWAWTabStop> m_tabStops;
  bool m_isTabPositionRelative;

  std::set <const DMWAWSubDocument *> m_subDocuments;

  bool m_inSubDocument;
  bool m_isNote;
  DMWAWSubDocumentType m_subDocumentType;

private:
  _DMWAWContentParsingState(const _DMWAWContentParsingState &);
  _DMWAWContentParsingState &operator=(const _DMWAWContentParsingState &);
};

class DMWAWContentListener : public DMWAWListener
{
protected:
  DMWAWContentListener(std::list<DMWAWPageSpan> &pageList, WPXDocumentInterface *documentInterface);
  virtual ~DMWAWContentListener();

  void startDocument();
  void startSubDocument();
  void endDocument();
  void endSubDocument();
  void handleSubDocument(const DMWAWSubDocument *subDocument, DMWAWSubDocumentType subDocumentType, DMWAWTableList tableList, int nextTableIndice);
  void insertBreak(const uint8_t breakType);

  // OSNOLA: allows to change the unit
  void lineSpacingChange(const double lineSpacing, WPXUnit unit=WPX_PERCENT);
  // force a break if there is a justification change
  void justificationChange(const uint8_t justification, bool force=false);

  DMWAWContentParsingState *m_ps; // parse state
  WPXDocumentInterface *m_documentInterface;
  WPXPropertyList m_metaData;

  virtual void _handleSubDocument(const DMWAWSubDocument *subDocument, DMWAWSubDocumentType subDocumentType, DMWAWTableList tableList, int nextTableIndice) = 0;
  virtual void _flushText() = 0;
  virtual void _changeList() = 0;

  void _openSection();
  void _closeSection();

  void _openPageSpan();
  void _closePageSpan();

// OSNOLA: adds virtual in order to define border
  virtual void _appendParagraphProperties(WPXPropertyList &propList, const bool isListElement=false);
  // OSNOLA: adds to insert some properties before openSpan
  virtual void _appendExtraSpanProperties(WPXPropertyList &) {}
  void _getTabStops(WPXPropertyListVector &tabStops);
  void _appendJustification(WPXPropertyList &propList, int justification);
  void _resetParagraphState(const bool isListElement=false);
  virtual void _openParagraph();
  void _closeParagraph();

  void _openListElement();
  void _closeListElement();

  void _openSpan();
  void _closeSpan();

  void _openTable();
  void _closeTable();
  void _openTableRow(const double height, const bool isMinimumHeight, const bool isHeaderRow);
  void _closeTableRow();
  void _openTableCell(const uint8_t colSpan, const uint8_t rowSpan, const uint8_t borderBits,
                      const libmwaw_libwpd::RGBSColor *cellFgColor, const libmwaw_libwpd::RGBSColor *cellBgColor,
                      const libmwaw_libwpd::RGBSColor *cellBorderColor,
                      const DMWAWVerticalAlignment cellVerticalAlignment);
  void _closeTableCell();

  double _movePositionToFirstColumn(double position);

  double _getNextTabStop() const;
  double _getPreviousTabStop() const;

  void _insertText(const WPXString &textBuffer);

  void _insertBreakIfNecessary(WPXPropertyList &propList);

  void _insertPageNumberParagraph(DMWAWPageNumberPosition position, DMWAWNumberingType type, WPXString fontName, double fontSize);

  uint32_t _mapNonUnicodeCharacter(uint32_t character);

private:
  DMWAWContentListener(const DMWAWContentListener &);
  DMWAWContentListener &operator=(const DMWAWContentListener &);
  WPXString _colorToString(const libmwaw_libwpd::RGBSColor *color);
  WPXString _mergeColorsToString(const libmwaw_libwpd::RGBSColor *fgColor, const libmwaw_libwpd::RGBSColor *bgColor);
  uint32_t _mapSymbolFontCharacter(uint32_t character);
  uint32_t _mapDingbatsFontCharacter(uint32_t character);
};

#endif /* WPXCONTENTLISTENER_H */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
