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

class WPXBinaryData;
class WPXDocumentInterface;
class WPXString;
class WPXPropertyListVector;

class MWAWCell;
class MWAWFont;
class MWAWInputStream;
class MWAWList;
class MWAWPageSpan;
class MWAWParagraph;
class MWAWParserState;
class MWAWPosition;
class MWAWSubDocument;

typedef shared_ptr<MWAWInputStream> MWAWInputStreamPtr;
typedef shared_ptr<MWAWSubDocument> MWAWSubDocumentPtr;

namespace MWAWContentListenerInternal
{
struct DocumentState;
struct State;
}

class MWAWContentListener
{
public:
  enum BreakType { PageBreak=0, SoftPageBreak, ColumnBreak };

  /** constructor */
  MWAWContentListener(MWAWParserState &parserState, std::vector<MWAWPageSpan> const &pageList, WPXDocumentInterface *documentInterface);
  /** destructor */
  virtual ~MWAWContentListener();

  /** sets the documents language */
  void setDocumentLanguage(std::string locale);

  /** starts the document */
  void startDocument();
  /** ends the document */
  void endDocument(bool sendDelayedSubDoc=true);

  /** function called to add a subdocument */
  void handleSubDocument(MWAWSubDocumentPtr subDocument, libmwaw::SubDocumentType subDocumentType);
  /** returns try if a subdocument is open  */
  bool isSubDocumentOpened(libmwaw::SubDocumentType &subdocType) const;
  /** returns true if the header/footer is open */
  bool isHeaderFooterOpened() const;

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
  void insertUnicodeString(WPXString const &str);
  //! adds an unicode character to a string ( with correct encoding ).
  static void appendUnicode(uint32_t val, WPXString &buffer);

  //! adds a tab
  void insertTab();
  //! adds an end of line ( by default an hard one)
  void insertEOL(bool softBreak=false);
  //! inserts a break type: ColumBreak, PageBreak, ..
  void insertBreak(BreakType breakType);

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

  // ------ list format -----------
  /** function to set the actual list */
  void setList(shared_ptr<MWAWList> list);

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

  void _openPageSpan(bool sendHeaderFooters=true);
  void _closePageSpan();

  void _startSubDocument();
  void _endSubDocument();

  void _handleFrameParameters( WPXPropertyList &propList, MWAWPosition const &pos);
  bool openFrame(MWAWPosition const &pos, WPXPropertyList extras=WPXPropertyList());
  void closeFrame();


  void _openParagraph();
  void _closeParagraph();
  void _appendParagraphProperties(WPXPropertyList &propList, const bool isListElement=false);
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

  void _insertBreakIfNecessary(WPXPropertyList &propList);

  /** creates a new parsing state (copy of the actual state)
   *
   * \return the old one */
  shared_ptr<MWAWContentListenerInternal::State> _pushParsingState();
  //! resets the previous parsing state
  void _popParsingState();

protected:
  //! the main parse state
  shared_ptr<MWAWContentListenerInternal::DocumentState> m_ds;
  //! the actual local parse state
  shared_ptr<MWAWContentListenerInternal::State> m_ps;
  //! stack of local state
  std::vector<shared_ptr<MWAWContentListenerInternal::State> > m_psStack;
  //! the parser state
  MWAWParserState &m_parserState;
  //! the document interface
  WPXDocumentInterface *m_documentInterface;

private:
  MWAWContentListener(const MWAWContentListener &);
  MWAWContentListener &operator=(const MWAWContentListener &);
};

typedef shared_ptr<MWAWContentListener> MWAWContentListenerPtr;

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
