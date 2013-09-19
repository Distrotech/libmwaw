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

#ifndef MWAW_GRAPHIC_LISTENER_H
#define MWAW_GRAPHIC_LISTENER_H

#include <vector>

#include <libwpd/libwpd.h>

#include "libmwaw_internal.hxx"

#include "MWAWGraphicStyle.hxx"

#include "MWAWListener.hxx"

class MWAWGraphicShape;

namespace MWAWGraphicListenerInternal
{
struct GraphicState;
struct State;
}

/** This class contains the minimal code needed to write a Graphic sub document.

    \note it will be probably be enhanced in some near/far futur...
    All units are specicified in WPX_POINT
 */
class MWAWGraphicListener : public MWAWListener
{
public:
  /** constructor */
  MWAWGraphicListener(MWAWParserState &parserState);
  /** destructor */
  virtual ~MWAWGraphicListener();

  /** starts a new graphic */
  void startGraphic(Box2f const &bdbox);
  /** ends the actual graphic and fill the final WPXBinaryData, ... */
  bool endGraphic(WPXBinaryData &data, std::string &mimeType);
  /** returns true if a document is opened */
  bool isDocumentStarted() const;

  /** function called to add a subdocument */
  void handleSubDocument(Vec2f const &orig, MWAWSubDocumentPtr subDocument, libmwaw::SubDocumentType subDocumentType);
  /** returns try if a subdocument is open  */
  bool isSubDocumentOpened(libmwaw::SubDocumentType &subdocType) const;

  // ------ general information --------
  /** returns true if a text zone is opened */
  bool canWriteText() const;
  /** returns the graphic bdbox.*/
  Box2f const &getGraphicBdBox();

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

  // ------- subdocument -----------------
  /** adds a picture in given position */
  void insertPicture(Box2f const &bdbox, MWAWGraphicStyle const &style,
                     const WPXBinaryData &binaryData, std::string type="image/pict");
  /** adds a shape picture in given position */
  void insertPicture(Box2f const &bdbox, MWAWGraphicShape const &shape,
                     MWAWGraphicStyle const &style);
  /** adds a textbox in given position */
  void insertTextBox(Box2f const &bdbox, MWAWSubDocumentPtr subDocument, MWAWGraphicStyle const &style);

  // ------- section ---------------
  /** returns true if we can add open a section, add page break, ... */
  bool canOpenSectionAddBreak() const {
    return false;
  }
  //! returns true if a section is opened
  bool isSectionOpened() const {
    return false;
  }
  //! returns the actual section
  MWAWSection const &getSection() const;
  //! open a section if possible
  bool openSection(MWAWSection const &section);
  //! close a section
  bool closeSection() {
    return false;
  }
  //! inserts a break type: ColumBreak, PageBreak, ..
  void insertBreak(BreakType breakType);

protected:
  void _startSubDocument();
  void _endSubDocument();

  void _handleFrameParameters(WPXPropertyList &propList, Box2f const &pos, MWAWGraphicStyle const &style);
  bool openFrame();
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

  /** creates a new parsing state (copy of the actual state)
   *
   * \return the old one */
  shared_ptr<MWAWGraphicListenerInternal::State> _pushParsingState();
  //! resets the previous parsing state
  void _popParsingState();

protected:
  //! the actual global state
  shared_ptr<MWAWGraphicListenerInternal::GraphicState> m_gs;
  //! the actual local parse state
  shared_ptr<MWAWGraphicListenerInternal::State> m_ps;
  //! stack of local state
  std::vector<shared_ptr<MWAWGraphicListenerInternal::State> > m_psStack;
  //! the parser state
  MWAWParserState &m_parserState;

private:
  MWAWGraphicListener(const MWAWGraphicListener &);
  MWAWGraphicListener &operator=(const MWAWGraphicListener &);
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
