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

#ifndef MWAW_CONTENT_LISTENER_H
#define MWAW_CONTENT_LISTENER_H

#include <vector>

#include <libwpd/libwpd.h>

#include "libmwaw_internal.hxx"

#include "MWAWFont.hxx"

class WPXBinaryData;
class WPXDocumentInterface;
class WPXString;
class WPXPropertyListVector;

class MWAWCell;
class MWAWFontConverter;
class MWAWList;
class MWAWPageSpan;
class MWAWPosition;
class MWAWSubDocument;
struct MWAWTabStop;

typedef shared_ptr<MWAWSubDocument> MWAWSubDocumentPtr;

struct MWAWDocumentParsingState {
  MWAWDocumentParsingState(std::vector<MWAWPageSpan> const &pageList);
  ~MWAWDocumentParsingState();

  std::vector<MWAWPageSpan> m_pageList;
  WPXPropertyList m_metaData;

  int m_footNoteNumber /** footnote number*/, m_endNoteNumber /** endnote number*/;
  int m_newListId; // a new free id

  bool m_isDocumentStarted, m_isHeaderFooterStarted;
  std::vector<MWAWSubDocumentPtr> m_subDocuments; /** list of document actually open */

private:
  MWAWDocumentParsingState(const MWAWDocumentParsingState &);
  MWAWDocumentParsingState &operator=(const MWAWDocumentParsingState &);
};

struct MWAWContentParsingState {
  //! constructor
  MWAWContentParsingState();
  //! destructor
  ~MWAWContentParsingState();

  //! functions used to know if the paragraph has some borders
  bool hasParagraphBorders() const;
  //! functions used to know if the paragraph has different borders
  bool hasParagraphDifferentBorders() const;

  //! a buffer to stored the text
  WPXString m_textBuffer;
  //! the number of tabs to add
  int m_numDeferredTabs;

  //! the font
  MWAWFont m_font;
  //! the text language
  std::string m_textLanguage;

  //! true if pararagraph add a column break
  bool m_isParagraphColumnBreak;
  //! true if pararagraph add a page break
  bool m_isParagraphPageBreak;
  //! the paragraph justification ( left, center, ... )
  libmwaw::Justification m_paragraphJustification;
  //! the paragraph interline value
  double m_paragraphLineSpacing;
  //! the paragraph interline unit ( point, percent, ...)
  WPXUnit m_paragraphLineSpacingUnit;
  //! the paragraph interline type (fixed, at least )
  libmwaw::LineSpacing m_paragraphLineSpacingType;
  //! the paragraph background color
  MWAWColor m_paragraphBackgroundColor;
  //! the paragraph borders
  std::vector<MWAWBorder> m_paragraphBorders;

  shared_ptr<MWAWList> m_list;
  uint8_t m_currentListLevel;

  bool m_isPageSpanOpened;
  bool m_isSectionOpened;
  bool m_isFrameOpened;
  bool m_isPageSpanBreakDeferred;
  bool m_isHeaderFooterWithoutParagraph;

  bool m_isSpanOpened;
  bool m_isParagraphOpened;
  bool m_isListElementOpened;

  bool m_firstParagraphInPageSpan;

  std::vector<unsigned int> m_numRowsToSkip;
  bool m_isTableOpened;
  bool m_isTableRowOpened;
  bool m_isTableColumnOpened;
  bool m_isTableCellOpened;

  unsigned m_currentPage;
  int m_numPagesRemainingInSpan;
  int m_currentPageNumber;

  bool m_sectionAttributesChanged;
  int m_numColumns;
  std::vector < MWAWColumnDefinition > m_textColumns;
  bool m_isTextColumnWithoutParagraph;

  double m_pageFormLength;
  double m_pageFormWidth;
  bool m_pageFormOrientationIsPortrait;

  double m_pageMarginLeft;
  double m_pageMarginRight;
  double m_pageMarginTop;
  double m_pageMarginBottom;

  double m_sectionMarginLeft;  // In multicolumn sections, the above two will be rather interpreted
  double m_sectionMarginRight; // as section margin change
  double m_sectionMarginTop;
  double m_sectionMarginBottom;
  double m_paragraphMarginLeft;  // resulting paragraph margin that is one of the paragraph
  double m_paragraphMarginRight; // properties
  double m_paragraphMarginTop;
  WPXUnit m_paragraphMarginTopUnit;
  double m_paragraphMarginBottom;
  WPXUnit m_paragraphMarginBottomUnit;
  double m_leftMarginByPageMarginChange;  // part of the margin due to the PAGE margin change
  double m_rightMarginByPageMarginChange; // inside a page that already has content.
  double m_leftMarginByParagraphMarginChange;  // part of the margin due to the PARAGRAPH
  double m_rightMarginByParagraphMarginChange; // margin change (in WP6)
  double m_leftMarginByTabs;  // part of the margin due to the LEFT or LEFT/RIGHT Indent; the
  double m_rightMarginByTabs; // only part of the margin that is reset at the end of a paragraph

  double m_paragraphTextIndent; // resulting first line indent that is one of the paragraph properties
  double m_textIndentByParagraphIndentChange; // part of the indent due to the PARAGRAPH indent (WP6???)
  double m_textIndentByTabs; // part of the indent due to the "Back Tab" or "Left Tab"

  double m_listReferencePosition; // position from the left page margin of the list number/bullet
  double m_listBeginPosition; // position from the left page margin of the beginning of the list
  std::vector<bool> m_listOrderedLevels; //! a stack used to know what is open

  uint16_t m_alignmentCharacter;
  std::vector<MWAWTabStop> m_tabStops;
  bool m_isTabPositionRelative;

  bool m_inSubDocument;

  bool m_isNote;
  libmwaw::SubDocumentType m_subDocumentType;

private:
  MWAWContentParsingState(const MWAWContentParsingState &);
  MWAWContentParsingState &operator=(const MWAWContentParsingState &);
};

class MWAWContentListener
{
public:
  /** constructor */
  MWAWContentListener(shared_ptr<MWAWFontConverter> fontConverter, std::vector<MWAWPageSpan> const &pageList, WPXDocumentInterface *documentInterface);
  /** destructor */
  virtual ~MWAWContentListener();

  /** sets the documents language */
  void setDocumentLanguage(std::string locale);

  /** starts the document */
  void startDocument();
  /** ends the document */
  void endDocument();

  /** function called to add a subdocument */
  void handleSubDocument(MWAWSubDocumentPtr subDocument, libmwaw::SubDocumentType subDocumentType);
  /** returns try if a subdocument is open  */
  bool isSubDocumentOpened(libmwaw::SubDocumentType &subdocType) const;
  /** returns true if the header/footer is open */
  bool isHeaderFooterOpened() const;

  // ------ text data -----------

  //! adds a basic character, ..
  void insertCharacter(uint8_t character);
  /** adds an unicode character.
   *  By convention if \a character=0xfffd(undef), no character is added */
  void insertUnicode(uint32_t character);
  //! adds a unicode string
  void insertUnicodeString(WPXString const &str);
  //! adds an unicode character to a string ( with correct encoding ).
  static void appendUnicode(uint32_t val, WPXString &buffer);

  //! adds a tab
  void insertTab();
  //! adds an end of line ( by default an hard one)
  void insertEOL(bool softBreak=false);
  //! inserts a break type: ColumBreak, PageBreak, ..
  void insertBreak(const uint8_t breakType);

  // ------ text format -----------
  //! sets the font
  void setFont(MWAWFont const &font);
  //! returns the actual font
  MWAWFont const &getFont() const;
  //! sets the font language
  void setTextLanguage(std::string const &locale);

  // ------ paragraph format -----------
  //! returns true if a paragraph or a list is opened
  bool isParagraphOpened() const;
  void setParagraphLineSpacing(const double lineSpacing, WPXUnit unit=WPX_PERCENT, libmwaw::LineSpacing type=libmwaw::Fixed);
  /** Define the paragraph justification. You can set force=true to
      force a break if there is a justification change. */
  void setParagraphJustification(libmwaw::Justification justification, bool force=false);
  /** sets the first paragraph text indent.
      \note: the first character will appear at paragraphLeftmargin + paragraphTextIndent*/
  void setParagraphTextIndent(double margin, WPXUnit unit=WPX_INCH);
  /** sets the paragraph margin.
   * \note pos must be MWAW_LEFT, MWAW_RIGHT, MWAW_TOP, MWAW_BOTTOM
   */
  void setParagraphMargin(double margin, int pos, WPXUnit unit=WPX_INCH);
  /** sets the tabulations.
   * \param tabStops the tabulations
   */
  void setTabs(const std::vector<MWAWTabStop> &tabStops);
  /** sets the paragraph background color */
  void setParagraphBackgroundColor(MWAWColor const color=MWAWColor::white());
  /** reset the paragraph to have no border */
  void resetParagraphBorders();
  /** set a paragraph has one or several border:
   * \param which LeftBit | RightBit | ...
   * \param border the border
   */
  void setParagraphBorder(int which, MWAWBorder const &border);

  // ------ list format -----------
  /** function to set the actual list */
  void setCurrentList(shared_ptr<MWAWList> list);
  /** returns the current list */
  shared_ptr<MWAWList> getCurrentList() const;
  /** function to set the level of the current list
   * \warning minimal implementation...*/
  void setCurrentListLevel(int level);

  // ------- fields ----------------
  /** Defines some basic type for field */
  enum FieldType { None, PageCount, PageNumber, Date, Time, Title, Link, Database };
  //! adds a field type
  void insertField(FieldType type);
  //! insert a date/time field with given format (see strftime)
  void insertDateTimeField(char const *format);

  // ------- subdocument -----------------
  /** defines the footnote type */
  enum NoteType { FOOTNOTE, ENDNOTE };
  /** adds note */
  void insertNote(const NoteType noteType, MWAWSubDocumentPtr &subDocument);
  /** adds a label note */
  void insertLabelNote(const NoteType noteType, WPXString const &label, MWAWSubDocumentPtr &subDocument);
  /** set the next note number */
  void resetNoteNumber(const NoteType noteType, int number);

  /** adds comment */
  void insertComment(MWAWSubDocumentPtr &subDocument);

  /** adds a picture in given position */
  void insertPicture(MWAWPosition const &pos, const WPXBinaryData &binaryData,
                     std::string type="image/pict",
                     WPXPropertyList frameExtras=WPXPropertyList());
  /** adds a textbox in given position */
  void insertTextBox(MWAWPosition const &pos, MWAWSubDocumentPtr subDocument,
                     WPXPropertyList frameExtras=WPXPropertyList(),
                     WPXPropertyList textboxExtras=WPXPropertyList());

  // ------- table -----------------
  /** open a table*/
  void openTable(std::vector<float> const &colWidth, WPXUnit unit);
  /** closes this table */
  void closeTable();
  /** open a row with given height ( if h < 0.0, set min-row-height = -h )*/
  void openTableRow(float h, WPXUnit unit, bool headerRow=false);
  /** closes this row */
  void closeTableRow();
  /** low level function to define a cell.
  	\param cell the cell position, alignement, ...
  	\param extras to be used to pass extra data, for instance spreadsheet data*/
  void openTableCell(MWAWCell const &cell, WPXPropertyList const &extras);
  /** close a cell */
  void closeTableCell();
  /** add empty cell */
  void addEmptyTableCell(Vec2i const &pos, Vec2i span=Vec2i(1,1));

  // ------- section ---------------
  //! returns true if a section is opened
  bool isSectionOpened() const;
  //! returns the actual number of columns ( or 1 if no section is opened )
  int getSectionNumColumns() const;
  //! open a section if possible
  bool openSection(std::vector<int> colsWidth=std::vector<int>(), WPXUnit unit=WPX_POINT);
  //! close a section
  bool closeSection();

protected:
  void _openSection();
  void _closeSection();

  void _openPageSpan();
  void _closePageSpan();
  void _updatePageSpanDependent(bool set);
  void _recomputeParagraphPositions();

  void _startSubDocument();
  void _endSubDocument();

  void _handleFrameParameters( WPXPropertyList &propList, MWAWPosition const &pos);
  bool openFrame(MWAWPosition const &pos, WPXPropertyList extras=WPXPropertyList());
  void closeFrame();


  void _openParagraph();
  void _closeParagraph();
  void _appendParagraphProperties(WPXPropertyList &propList, const bool isListElement=false);
  void _getTabStops(WPXPropertyListVector &tabStops);
  void _appendJustification(WPXPropertyList &propList, libmwaw::Justification justification);
  void _resetParagraphState(const bool isListElement=false);

  void _openListElement();
  void _closeListElement();
  void _changeList();

  void _openSpan();
  void _closeSpan();

  void _flushText();
  void _flushDeferredTabs();

  void _insertBreakIfNecessary(WPXPropertyList &propList);

  static void _addLanguage(std::string const &locale, WPXPropertyList &propList);

  /** creates a new parsing state (copy of the actual state)
   *
   * \return the old one */
  shared_ptr<MWAWContentParsingState> _pushParsingState();
  //! resets the previous parsing state
  void _popParsingState();

protected:
  shared_ptr<MWAWDocumentParsingState> m_ds; // main parse state
  shared_ptr<MWAWContentParsingState> m_ps; // parse state
  std::vector<shared_ptr<MWAWContentParsingState> > m_psStack;
  shared_ptr<MWAWFontConverter> m_fontConverter; // the font convertor
  WPXDocumentInterface *m_documentInterface;

private:
  MWAWContentListener(const MWAWContentListener &);
  MWAWContentListener &operator=(const MWAWContentListener &);
};

typedef shared_ptr<MWAWContentListener> MWAWContentListenerPtr;

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
