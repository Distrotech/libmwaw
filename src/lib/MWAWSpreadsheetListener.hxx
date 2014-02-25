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

/** \file MWAWSpreadsheetListener.hxx
 * Defines MWAWSpreadsheetListener: the libmwaw spreadsheet processor listener
 *
 * \note this class is the only class which does the interface with
 * the librevenge::RVNGSpreadsheetInterface
 */
#ifndef MWAW_SPREADSHEET_LISTENER_H
#define MWAW_SPREADSHEET_LISTENER_H

#include <vector>

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"

#include "MWAWListener.hxx"

class MWAWCell;
class MWAWCellContent;
class MWAWChart;
class MWAWGraphicStyle;
class MWAWGraphicShape;
class MWAWTable;

namespace MWAWSpreadsheetListenerInternal
{
struct DocumentState;
struct State;
}

/** This class contents the main functions needed to create a spreadsheet processing Document */
class MWAWSpreadsheetListener : public MWAWListener
{
public:
  /** constructor */
  MWAWSpreadsheetListener(MWAWParserState &parserState, std::vector<MWAWPageSpan> const &pageList, librevenge::RVNGSpreadsheetInterface *documentInterface);
  /** destructor */
  virtual ~MWAWSpreadsheetListener();

  /** returns the listener type */
  Type getType() const
  {
    return Spreadsheet;
  }

  /** sets the documents language */
  void setDocumentLanguage(std::string locale);

  /** starts the document */
  void startDocument();
  /** ends the document */
  void endDocument(bool sendDelayedSubDoc=true);
  /** returns true if a document is opened */
  bool isDocumentStarted() const;

  /** function called to add a subdocument */
  void handleSubDocument(MWAWSubDocumentPtr subDocument, libmwaw::SubDocumentType subDocumentType);
  /** returns try if a subdocument is open  */
  bool isSubDocumentOpened(libmwaw::SubDocumentType &subdocType) const;

  /** returns true if we can add text data */
  bool canWriteText() const;

  // ------ page --------
  /** returns true if a page is opened */
  bool isPageSpanOpened() const;
  /** returns the current page span

  \note this forces the opening of a new page if no page is opened.*/
  MWAWPageSpan const &getPageSpan();

  // ------ header/footer --------
  /** insert a header */
  bool insertHeader(MWAWSubDocumentPtr subDocument, librevenge::RVNGPropertyList const &extras);
  /** insert a footer */
  bool insertFooter(MWAWSubDocumentPtr subDocument, librevenge::RVNGPropertyList const &extras);
  /** returns true if the header/footer is open */
  bool isHeaderFooterOpened() const;

  // ------- sheet -----------------
  /** open a sheet*/
  void openSheet(std::vector<float> const &colWidth, librevenge::RVNGUnit unit, std::string const &name="");
  /** closes this sheet */
  void closeSheet();
  /** open a row with given height ( if h < 0.0, set min-row-height = -h )*/
  void openSheetRow(float h, librevenge::RVNGUnit unit);
  /** closes this row */
  void closeSheetRow();
  /** open a cell */
  void openSheetCell(MWAWCell const &cell, MWAWCellContent const &content);
  /** close a cell */
  void closeSheetCell();

  // ------- chart -----------------
  /** adds a chart in given position */
  void insertChart(MWAWPosition const &pos, MWAWChart &chart,
                   MWAWGraphicStyle const &style=MWAWGraphicStyle::emptyStyle());

  // ------ text data -----------

  //! adds a basic character, ..
  void insertChar(uint8_t character);
  /** insert a character using the font converter to find the utf8
      character */
  void insertCharacter(unsigned char c);
  /** insert a character using the font converter to find the utf8
      character and if needed, input to read extra character.

      \return the number of extra character read
   */
  int insertCharacter(unsigned char c, MWAWInputStreamPtr &input, long endPos=-1);
  /** adds an unicode character.
   *  By convention if \a character=0xfffd(undef), no character is added */
  void insertUnicode(uint32_t character);
  //! adds a unicode string
  void insertUnicodeString(librevenge::RVNGString const &str);

  //! adds a tab
  void insertTab();
  //! adds an end of line ( by default an hard one)
  void insertEOL(bool softBreak=false);

  // ------ text format -----------
  //! sets the font
  void setFont(MWAWFont const &font);
  //! returns the actual font
  MWAWFont const &getFont() const;

  // ------ paragraph format -----------
  //! returns true if a paragraph or a list is opened
  bool isParagraphOpened() const;
  //! sets the paragraph
  void setParagraph(MWAWParagraph const &paragraph);
  //! returns the actual paragraph
  MWAWParagraph const &getParagraph() const;

  // ------- fields ----------------
  //! adds a field type
  void insertField(MWAWField const &field);

  // ------- link ----------------
  //! open a link
  void openLink(MWAWLink const &link);
  //! close a link
  void closeLink();

  // ------- subdocument -----------------
  /** insert a note */
  void insertNote(MWAWNote const &note, MWAWSubDocumentPtr &subDocument);

  /** adds comment */
  void insertComment(MWAWSubDocumentPtr &subDocument);

  /** adds a picture in given position */
  void insertPicture(MWAWPosition const &pos, const librevenge::RVNGBinaryData &binaryData,
                     std::string type="image/pict", MWAWGraphicStyle const &style=MWAWGraphicStyle::emptyStyle());
  /** adds a shape picture in given position */
  void insertPicture(MWAWPosition const &pos, MWAWGraphicShape const &shape,
                     MWAWGraphicStyle const &style);
  /** adds a textbox in given position */
  void insertTextBox(MWAWPosition const &pos, MWAWSubDocumentPtr subDocument,
                     librevenge::RVNGPropertyList frameExtras=librevenge::RVNGPropertyList(),
                     librevenge::RVNGPropertyList textboxExtras=librevenge::RVNGPropertyList());

  // ------- table -----------------
  /** adds a table in given position */
  void insertTable(MWAWPosition const &pos, MWAWTable &table, MWAWGraphicStyle const &style=MWAWGraphicStyle::emptyStyle());
  /** open a table */
  void openTable(MWAWTable const &table);
  /** closes this table */
  void closeTable();
  /** open a row with given height ( if h < 0.0, set min-row-height = -h )*/
  void openTableRow(float h, librevenge::RVNGUnit unit, bool headerRow=false);
  /** closes this row */
  void closeTableRow();
  /** open a cell */
  void openTableCell(MWAWCell const &cell);
  /** close a cell */
  void closeTableCell();
  /** add empty cell */
  void addEmptyTableCell(Vec2i const &pos, Vec2i span=Vec2i(1,1));

  // ------- section ---------------
  /** returns true if we can add open a section, add page break, ... */
  bool canOpenSectionAddBreak() const
  {
    return false;
  }
  //! returns true if a section is opened
  bool isSectionOpened() const
  {
    return false;
  }
  //! returns the actual section
  MWAWSection const &getSection() const;
  //! open a section if possible
  bool openSection(MWAWSection const &section);
  //! close a section
  bool closeSection();
  //! inserts a break type: ColumBreak, PageBreak, ..
  void insertBreak(BreakType breakType);

protected:
  //! does open a new page (low level)
  void _openPageSpan(bool sendHeaderFooters=true);
  //! does close a page (low level)
  void _closePageSpan();

  void _startSubDocument();
  void _endSubDocument();

  void _handleFrameParameters(librevenge::RVNGPropertyList &propList, MWAWPosition const &pos);
  bool openFrame(MWAWPosition const &pos, librevenge::RVNGPropertyList extras);
  /** tries to open a frame */
  bool openFrame(MWAWPosition const &pos, MWAWGraphicStyle const &style=MWAWGraphicStyle::emptyStyle());
  void closeFrame();


  void _openParagraph();
  void _closeParagraph();
  void _resetParagraphState(const bool isListElement=false);

  /** open a list level */
  void _openListElement();
  /** close a list level */
  void _closeListElement();
  /** update the list so that it corresponds to the actual level */
  void _changeList();
  /** low level: find a list id which corresponds to actual list and a change of level.

  \note called when the list id is not set
  */
  int _getListId() const;

  void _openSpan();
  void _closeSpan();

  void _flushText();
  void _flushDeferredTabs();

  /** creates a new parsing state (copy of the actual state)
   *
   * \return the old one */
  shared_ptr<MWAWSpreadsheetListenerInternal::State> _pushParsingState();
  //! resets the previous parsing state
  void _popParsingState();

protected:
  //! the main parse state
  shared_ptr<MWAWSpreadsheetListenerInternal::DocumentState> m_ds;
  //! the actual local parse state
  shared_ptr<MWAWSpreadsheetListenerInternal::State> m_ps;
  //! stack of local state
  std::vector<shared_ptr<MWAWSpreadsheetListenerInternal::State> > m_psStack;
  //! the parser state
  MWAWParserState &m_parserState;
  //! the document interface
  librevenge::RVNGSpreadsheetInterface *m_documentInterface;

private:
  //! copy constructor (unimplemented)
  MWAWSpreadsheetListener(const MWAWSpreadsheetListener &);
  //! operator= (unimplemented)
  MWAWSpreadsheetListener &operator=(const MWAWSpreadsheetListener &);
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
