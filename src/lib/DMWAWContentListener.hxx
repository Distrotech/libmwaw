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
 *   - gestion of  MWAW_ALL_CAPS_BIT/MWAW_EMBOSS_BIT/MWAW_ENGRAVE_BIT bits
 *   - change in DMWAWContentListener::justificationChange
 *
 * the other file WPX... are only basic copy of WPX needed because
 *    they are not exported
 */

#ifndef WPXCONTENTLISTENER_H
#define WPXCONTENTLISTENER_H

#include <libwpd/WPXPropertyListVector.h>
#include <libwpd/WPXPropertyList.h>
#include "libmwaw_internal.hxx"
#include "MWAWSubDocument.hxx"
#include "MWAWPageSpan.hxx"
#include <libwpd/WPXDocumentInterface.h>
#include <vector>
#include <list>
#include <set>
class WPXString;

struct RGBSColor {
  RGBSColor(uint8_t r, uint8_t g, uint8_t b, uint8_t s);
  RGBSColor(uint16_t red, uint16_t green, uint16_t blue); // Construct
  // RBBSColor from double precision RGB color used by WP3.x for Mac
  RGBSColor(); // initializes all values to 0
  uint8_t m_r;
  uint8_t m_g;
  uint8_t m_b;
  uint8_t m_s;
};

typedef struct _DMWAWContentParsingState DMWAWContentParsingState;
struct _DMWAWContentParsingState {
  _DMWAWContentParsingState();
  ~_DMWAWContentParsingState();

  uint32_t m_textAttributeBits;
  double m_fontSize;
  WPXString *m_fontName;
  RGBSColor *m_fontColor;
  RGBSColor *m_highlightColor;

  bool m_isParagraphColumnBreak;
  bool m_isParagraphPageBreak;
  libmwaw::Justification m_paragraphJustification;
  libmwaw::Justification m_tempParagraphJustification; // TODO: remove this one after the tabs are properly implemented
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
  std::vector < MWAWColumnDefinition > m_textColumns;
  bool m_isTextColumnWithoutParagraph;

  double m_pageFormLength;
  double m_pageFormWidth;
  MWAWPageSpan::FormOrientation m_pageFormOrientation;

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
  std::vector<MWAWTabStop> m_tabStops;
  bool m_isTabPositionRelative;

  std::set<MWAWSubDocumentPtr> m_subDocuments;

  bool m_inSubDocument;
  bool m_isNote;
  libmwaw::SubDocumentType m_subDocumentType;

private:
  _DMWAWContentParsingState(const _DMWAWContentParsingState &);
  _DMWAWContentParsingState &operator=(const _DMWAWContentParsingState &);
};

class DMWAWContentListener
{
public:
  enum NoteType { FOOTNOTE, ENDNOTE };
protected:
  DMWAWContentListener(std::list<MWAWPageSpan> &pageList, WPXDocumentInterface *documentInterface);
  virtual ~DMWAWContentListener();

  void startDocument();
  void startSubDocument();
  void endDocument();
  void endSubDocument();
  void handleSubDocument(MWAWSubDocumentPtr &subDocument, libmwaw::SubDocumentType subDocumentType);
  void insertBreak(const uint8_t breakType);

  // OSNOLA: allows to change the unit
  void lineSpacingChange(const double lineSpacing, WPXUnit unit=WPX_PERCENT);
  // force a break if there is a justification change
  void justificationChange(libmwaw::Justification justification, bool force=false);

  DMWAWContentParsingState *m_ps; // parse state
  WPXDocumentInterface *m_documentInterface;
  WPXPropertyList m_metaData;
  std::list<MWAWPageSpan> m_pageList;
  bool m_isUndoOn;

  virtual void _handleSubDocument(MWAWSubDocumentPtr &subDocument, libmwaw::SubDocumentType subDocumentType) = 0;
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
  void _appendJustification(WPXPropertyList &propList, libmwaw::Justification justification);
  void _resetParagraphState(const bool isListElement=false);
  virtual void _openParagraph();
  void _closeParagraph();

  void _openListElement();
  void _closeListElement();

  void _openSpan();
  void _closeSpan();

  void _closeTable();
  void _closeTableRow();
  void _closeTableCell();

  double _movePositionToFirstColumn(double position);

  double _getNextTabStop() const;
  double _getPreviousTabStop() const;

  void _insertText(const WPXString &textBuffer);

  void _insertBreakIfNecessary(WPXPropertyList &propList);

  uint32_t _mapNonUnicodeCharacter(uint32_t character);

  bool isUndoOn() {
    return m_isUndoOn;
  }
  void setUndoOn(bool isOn) {
    m_isUndoOn = isOn;
  }

private:
  DMWAWContentListener(const DMWAWContentListener &);
  DMWAWContentListener &operator=(const DMWAWContentListener &);
  WPXString _colorToString(const RGBSColor *color);
  WPXString _mergeColorsToString(const RGBSColor *fgColor, const RGBSColor *bgColor);
  uint32_t _mapSymbolFontCharacter(uint32_t character);
  uint32_t _mapDingbatsFontCharacter(uint32_t character);
};

#endif /* WPXCONTENTLISTENER_H */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
