/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
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
 * For further information visit http://libmwaw.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#ifndef MWAW_CONTENT_LISTENER_H
#  define MWAW_CONTENT_LISTENER_H

#include <string>
#include <vector>

#include <libwpd/WPXString.h>

#include "MWAWInputStream.hxx"

#include "libmwaw_internal.hxx"

#include "DMWAWContentListener.hxx"

class MWAWCell;
class MWAWList;
class MWAWPosition;
class MWAWSubDocument;
typedef shared_ptr<MWAWSubDocument> MWAWSubDocumentPtr;

class MWAWPageSpan;
/** \brief an interne MWAWContentListener which adds some function to a basic DMWAWContentListener
 *
 * This class is mainly based on some specialised contentlistener libwpd */
class MWAWContentListener : public DMWAWContentListener
{
  friend class MWAWPageSpan;
  //! the parent type
  typedef DMWAWContentListener parent;
protected:
  struct ParsingState;
public:
  //! access to the constructor
  static shared_ptr<MWAWContentListener> create(std::list<MWAWPageSpan> &pageList,
      WPXDocumentInterface *documentInterface);
  //! a virtual destructor
  virtual ~MWAWContentListener();


  //! starts the document
  using parent::startDocument;
  //! ends the document
  using parent::endDocument;

  //! inserts a break : page, ..
  using parent::insertBreak;

  // justification and line spacing
  using parent::justificationChange;
  using parent::lineSpacingChange;

  //! sets the documents language (the simplified locale name)
  void setDocumentLanguage(std::string const &locale);

  //! returns true if a section is opened
  bool isSectionOpened() const;
  //! open a section if possible
  bool openSection(std::vector<int> colsWidth=std::vector<int>(), WPXUnit unit=WPX_POINT);

  //! close a section
  bool closeSection();

  //! returns true if a paragraph is opened
  bool isParagraphOpened() const;

  //! sets the text attributes
  void setTextAttribute(const uint32_t attribute);
  //! sets the text fonts
  void setTextFont(const WPXString &fontName);
  //! sets the font size
  void setFontSize(const uint16_t fontSize);
  //! sets the font color
  void setFontColor(int const (&col) [3]);
  //! sets the font color
  void setFontColor(Vec3i const &col);
  //! sets the text language (the simplified locale name)
  void setTextLanguage(std::string const &locale);

  //! sets the first paragraph text indent. \warning unit are given in inches
  void setParagraphTextIndent(double margin);
  /** sets the paragraph margin.
   *
   * \param margin is given in inches
   * \param pos in MWAW_LEFT, MWAW_RIGHT, MWAW_TOP, MWAW_BOTTOM
   */
  void setParagraphMargin(double margin, int pos, WPXUnit unit=WPX_INCH);
  /** sets the tabulations.
   *
   * \param tabStops the tabulations
   * \param maxW if given, decals the tabulations which are too near of the right border
   */
  void setTabs(const std::vector<MWAWTabStop> &tabStops, double maxW = -1.);
  /** indicates that the paragraph has a basic border (ie. a black line)
   *
   * \param which = libmwaw::LeftBorderBit, ...
   * \param flag sets to true
   */
  void setParagraphBorder(int which, bool flag);
  //! indicates that the paragraph has a basic borders (ie. 4 black lines)
  void setParagraphBorder(bool flag);

  /** function to set the actual list
   *
   * \note set the listid if not set
   */
  void setCurrentList(shared_ptr<MWAWList> list);

  /** returns the current list */
  shared_ptr<MWAWList> getCurrentList() const;

  /** function to create an unordered list
   *
   * \warning minimal implementation : this part must be complety rewritten ...*/
  void setCurrentListLevel(int level);

  //! adds a basic character, ..
  void insertCharacter(uint8_t character);
  /** adds an unicode character
   *
   * by convention if \a character=0xfffd(undef), no character is added */
  void insertUnicode(uint32_t character);
  //! adds a unicode string
  void insertUnicodeString(WPXString const &str);
  //! internal function used to add an unicode character to a string
  static void appendUnicode(uint32_t val, WPXString &buffer);

  //! adds an end of line
  void insertEOL(bool softBreak=false);
  //! adds a tabulation
  void insertTab();

  /** Defines some basic type for field */
  enum FieldType { None, PageNumber, Date, Time, Title, Link, Database };
  //! adds a field type
  void insertField(FieldType type);
  //! insert a date/time field with given format (see strftime)
  void insertDateTimeField(char const *format);

  /** adds note
   *
   * \warning checks if this does allow recursive insertion */
  void insertNote(const NoteType noteType,
                  MWAWSubDocumentPtr &subDocument);
  /** adds comment
   *
   * \warning checks if this does allow recursive insertion */
  void insertComment(MWAWSubDocumentPtr &subDocument);

  /** open a frame */
  bool openFrame(MWAWPosition const &pos, WPXPropertyList extras=WPXPropertyList());
  /** close a frame */
  void closeFrame();

  /** adds a picture in given position
   *
   * \warning the position unit must be WPX_POINT, WPX_INCH or WPX_TWIP... */
  void insertPicture(MWAWPosition const &pos, const WPXBinaryData &binaryData,
                     std::string type="image/pict",
                     WPXPropertyList frameExtras=WPXPropertyList());
  /** adds a textbox in given position
   *
   * \warning the position unit must be WPX_POINT, WPX_INCH or WPX_TWIP... */
  void insertTextBox(MWAWPosition const &pos, MWAWSubDocumentPtr subDocument,
                     WPXPropertyList frameExtras=WPXPropertyList());

  /** open a table*/
  void openTable(std::vector<int> const &colWidth, WPXUnit unit);
  void openTable(std::vector<float> const &colWidth, WPXUnit unit);
  /** closes this table */
  void closeTable();

  /** open a row with given height*/
  void openTableRow(float h, WPXUnit unit, bool headerRow=false);
  /** closes this row */
  void closeTableRow();

  /** low level function to define a cell.
      \param cell the cell position, alignement, ...
      \param extras to be used to pass extra data, for instance spreadsheet data

  \note openTableRow, .. must call before*/
  void openTableCell(MWAWCell const &cell, WPXPropertyList const &extras);
  /** low level function to define a cell */
  void closeTableCell();

protected:
  //! protected constructor \sa create
  MWAWContentListener(std::list<MWAWPageSpan> &pageList,
                      WPXDocumentInterface *documentInterface);
  //! function called automatically after creation to initialize the data
  void _init();

  //! virtual function called to create the parsing state
  virtual ParsingState *_newParsingState() const {
    return new ParsingState;
  }
  //! returns the actual parsing state
  ParsingState *parsingState() {
    return m_parseState;
  }

  /** creates a new parsing state (copy of the actual state)
   *
   * \return the old one */
  ParsingState *_pushParsingState() {
    ParsingState *oldState = m_parseState;
    m_parseState = _newParsingState();
    m_parseState->m_numNestedNotes = oldState->m_numNestedNotes;
    return oldState;
  }
  //! resets the previous parsing state (obtained from pushParsingState)
  void _popParsingState(ParsingState *oldState) {
    oldState->m_numNestedNotes = m_parseState->m_numNestedNotes;
    delete m_parseState;
    m_parseState = oldState;

  }

  //! flushes the text
  void _flushText();
  //! flushes the deferred tabs
  void _flushDeferredTabs();

  //! adds the gestion of border or language
  void _appendParagraphProperties(WPXPropertyList &propList, const bool isListElement=false);
  //! inserts some language property if needed
  void _appendExtraSpanProperties(WPXPropertyList &propList);
  //! closes the previous item (if needed) and open the new one
  void _changeList();
  //! function called to handle a sub document
  void _handleSubDocument(MWAWSubDocumentPtr &subDocument,
                          libmwaw::SubDocumentType subDocumentType);
  /** retrieve properties to open a new frame
   *
   * \param propList the properties list
   * \param pos the frame position
   * \warning the position unit must be WPX_POINT, WPX_INCH or WPX_TWIP...
   */
  void _handleFrameParameters( WPXPropertyList &propList, MWAWPosition const &pos);

  //! sets list properties when the list changes
  void _handleListChange();

protected:
  /** the MWAWContentListener's state */
  struct ParsingState {
    //! basic constructor
    ParsingState();
    //! the copy constructor
    ParsingState(ParsingState const &orig);
    //! virtual destructor
    virtual ~ParsingState();

    //! the text to flush
    WPXString m_textBuffer;

    //! the tabs which need to be flushed
    int m_numDeferredTabs;
    int m_footNoteNumber /** footnote number*/, m_endNoteNumber /** end number*/, m_numNestedNotes /** number of nested notes */;

    //! a flag used to add a ''basic'' border to a paragraph
    int m_border;


    std::string m_language/** the language (simplified locale name)*/,
        m_parag_language /** the actual paragraph language */,
        m_doc_language /** the document language (simplified locale name)*/;
    bool m_isFrameOpened; /** true if a frame is opened */
    //! the list
    shared_ptr<MWAWList> m_list;
    //! a stack used to know what is open
    std::vector<bool> m_listOrderedLevels;
  };

private:
  //! protected copy constructor. Must not be called.
  MWAWContentListener(const MWAWContentListener &);
  //! protected copy operator. Must not be called.
  MWAWContentListener &operator=(const MWAWContentListener &);

  //! the actual state
  ParsingState *m_parseState;

  //! the actual list id
  int m_actualListId;

  //! a list of actual subdocument smart pointer to forbide eroneous destruction
  std::vector<MWAWSubDocumentPtr>  m_subDocuments;

};

typedef shared_ptr<MWAWContentListener> MWAWContentListenerPtr;
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
