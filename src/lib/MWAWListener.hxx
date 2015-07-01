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

#ifndef MWAW_LISTENER_H
#define MWAW_LISTENER_H

#include <vector>

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"

#include "MWAWGraphicStyle.hxx"

class MWAWCell;
class MWAWTable;

/** This class contains a virtual interface to all listener */
class MWAWListener
{
public:
  //! destructor
  virtual ~MWAWListener() {}

  //! the listener type
  enum Type { Graphic, Presentation, Spreadsheet, Text };
  /** the different break type */
  enum BreakType { PageBreak=0, SoftPageBreak, ColumnBreak };

  //------- generic accessor ---
  /** returns the listener type */
  virtual Type getType() const = 0;
  /** returns true if we can add text data */
  virtual bool canWriteText() const =0;

  // ------ main document -------
  /** sets the documents language */
  virtual void setDocumentLanguage(std::string locale) = 0;
  /** starts the document */
  virtual void startDocument() = 0;
  /** returns true if a document is opened */
  virtual bool isDocumentStarted() const =0;
  /** ends the document */
  virtual void endDocument(bool sendDelayedSubDoc=true) = 0;

  // ------ page --------
  /** returns true if a page is opened */
  virtual bool isPageSpanOpened() const = 0;
  /** returns the current page span

  \note this forces the opening of a new page if no page is opened.*/
  virtual MWAWPageSpan const &getPageSpan() = 0;

  // ------ header/footer --------
  /** insert a header (interaction with MWAWPageSpan which fills the parameters for openHeader) */
  virtual bool insertHeader(MWAWSubDocumentPtr subDocument, librevenge::RVNGPropertyList const &extras) = 0;
  /** insert a footer (interaction with MWAWPageSpan which fills the parameters for openFooter) */
  virtual bool insertFooter(MWAWSubDocumentPtr subDocument, librevenge::RVNGPropertyList const &extras) = 0;
  /** returns true if the header/footer is open */
  virtual bool isHeaderFooterOpened() const = 0;

  // ------ text data -----------
  //! adds a basic character, ..
  virtual void insertChar(uint8_t character)=0;
  /** insert a character using the font converter to find the utf8
      character */
  virtual void insertCharacter(unsigned char c)=0;
  /** insert a character using the font converter to find the utf8
      character and if needed, input to read extra character.

      \return the number of extra character read
   */
  virtual int insertCharacter(unsigned char c, MWAWInputStreamPtr &input, long endPos=-1)=0;
  /** adds an unicode character.
   *  By convention if \a character=0xfffd(undef), no character is added */
  virtual void insertUnicode(uint32_t character)=0;
  //! adds a unicode string
  virtual void insertUnicodeString(librevenge::RVNGString const &str)=0;

  //! adds a tab
  virtual void insertTab()=0;
  //! adds an end of line ( by default an hard one)
  virtual void insertEOL(bool softBreak=false)=0;

  // ------ text format -----------
  //! sets the font
  virtual void setFont(MWAWFont const &font)=0;
  //! returns the actual font
  virtual MWAWFont const &getFont() const=0;

  // ------ paragraph format -----------
  //! returns true if a paragraph or a list is opened
  virtual bool isParagraphOpened() const=0;
  //! sets the paragraph
  virtual void setParagraph(MWAWParagraph const &paragraph)=0;
  //! returns the actual paragraph
  virtual MWAWParagraph const &getParagraph() const=0;

  // ------- fields ----------------
  //! adds a field type
  virtual void insertField(MWAWField const &field)=0;

  // ------- link ----------------

  //! open a link
  virtual void openLink(MWAWLink const &link)=0;
  //! close a link
  virtual void closeLink()=0;

  // ------- table -----------------
  /** open a table*/
  virtual void openTable(MWAWTable const &table) = 0;
  /** closes this table */
  virtual void closeTable() = 0;
  /** open a row with given height ( if h < 0.0, set min-row-height = -h )*/
  virtual void openTableRow(float h, librevenge::RVNGUnit unit, bool headerRow=false) = 0;
  /** closes this row */
  virtual void closeTableRow() = 0;
  /** open a cell */
  virtual void openTableCell(MWAWCell const &cell) = 0;
  /** close a cell */
  virtual void closeTableCell() = 0;
  /** add empty cell */
  virtual void addEmptyTableCell(MWAWVec2i const &pos, MWAWVec2i span=MWAWVec2i(1,1)) = 0;

  // ------- section ---------------
  /** returns true if we can add open a section, add page break, ... */
  virtual bool canOpenSectionAddBreak() const =0;
  //! returns true if a section is opened
  virtual bool isSectionOpened() const=0;
  //! returns the actual section
  virtual MWAWSection const &getSection() const=0;
  //! open a section if possible
  virtual bool openSection(MWAWSection const &section)=0;
  //! close a section
  virtual bool closeSection()=0;
  //! inserts a break type: ColumBreak, PageBreak, ..
  virtual void insertBreak(BreakType breakType)=0;

  // ------- subdocument ---------------
  /** insert a note */
  virtual void insertNote(MWAWNote const &note, MWAWSubDocumentPtr &subDocument)=0;
  /** adds comment */
  virtual void insertComment(MWAWSubDocumentPtr &subDocument) = 0;
  /** adds a picture in given position */
  virtual void insertPicture(MWAWPosition const &pos, const librevenge::RVNGBinaryData &binaryData,
                             std::string type="image/pict", MWAWGraphicStyle const &style=MWAWGraphicStyle::emptyStyle()) = 0;
  /** adds a shape picture in given position */
  virtual void insertPicture(MWAWPosition const &pos, MWAWGraphicShape const &shape,
                             MWAWGraphicStyle const &style) = 0;
  /** adds a textbox in given position */
  virtual void insertTextBox(MWAWPosition const &pos, MWAWSubDocumentPtr subDocument,
                             MWAWGraphicStyle const &frameStyle=MWAWGraphicStyle::emptyStyle()) = 0;
  /** low level: tries to open a frame */
  virtual bool openFrame(MWAWPosition const &pos, MWAWGraphicStyle const &style=MWAWGraphicStyle::emptyStyle()) = 0;
  /** low level: tries to close the last opened frame */
  virtual void closeFrame() = 0;
  /** low level: tries to open a group */
  virtual bool openGroup(MWAWPosition const &pos) = 0;
  /** low level: tries to close the last opened group */
  virtual void closeGroup() = 0;
  /** low level: function called to add a subdocument */
  virtual void handleSubDocument(MWAWSubDocumentPtr subDocument, libmwaw::SubDocumentType subDocumentType) = 0;
  /** returns true if a subdocument is open  */
  virtual bool isSubDocumentOpened(libmwaw::SubDocumentType &subdocType) const = 0;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
