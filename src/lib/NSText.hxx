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

/*
 * Parser to Nisus text document
 *
 */
#ifndef NS_TEXT
#  define NS_TEXT

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"

#include "NSStruct.hxx"

class MWAWInputStream;
typedef shared_ptr<MWAWInputStream> MWAWInputStreamPtr;

class MWAWContentListener;
typedef class MWAWContentListener NSContentListener;
typedef shared_ptr<NSContentListener> NSContentListenerPtr;

class MWAWEntry;

class MWAWFont;
class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

class MWAWSubDocument;

namespace NSTextInternal
{
class SubDocument;
struct Paragraph;
struct State;
}

class NSParser;

/** \brief the main class to read the text part of writenow file
 *
 *
 *
 */
class NSText
{
  friend class NSTextInternal::SubDocument;
  friend class NSParser;
public:
  //! constructor
  NSText(MWAWInputStreamPtr ip, NSParser &parser, MWAWFontConverterPtr &convertissor);
  //! destructor
  virtual ~NSText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:

  //! sets the listener in this class and in the helper classes
  void setListener(NSContentListenerPtr listen) {
    m_listener = listen;
  }

  //! finds the different text zones
  bool createZones();

  //! return an header subdocument
  shared_ptr<MWAWSubDocument> getHeader(int page);
  //! return a footer subdocument
  shared_ptr<MWAWSubDocument> getFooter(int page);

  //! send a main zone
  bool sendMainText();

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  /** read a text entry.
     \note entry.id() must correspond to the zone id.
     \note while the main text is in the data fork, the footnote/header footer is in a ??TX rsrc.*/
  bool sendText(MWAWEntry entry, NSStruct::Position fPos=NSStruct::Position());

  /** try to send the ith footnote */
  bool sendFootnote(int footnoteId);

  /** try to send the ith header footer */
  bool sendHeaderFooter(int hfId);

  //
  // intermediate level
  //

  /** compute the positions */
  void computePositions();

  /* sends a font property to the listener
   * \param font the font's properties */
  void setProperty(MWAWFont const &font, MWAWFont &previousFont);
  /** sends a paragraph property to the listener */
  void setProperty(NSTextInternal::Paragraph const &ruler, int width);

  //! read the list of fonts
  bool readFontsList(MWAWEntry const &entry);
  /** read the header/footer main entry */
  bool readHeaderFooter(MWAWEntry const &entry);
  /** read the footnote main entry */
  bool readFootnotes(MWAWEntry const &entry);

  //! read the FTAB/STYL resource: a list of fonts
  bool readFonts(MWAWEntry const &entry);
  //! read the FRMT resource: a list of filepos -> fontId
  bool readPosToFont(MWAWEntry const &entry, NSStruct::ZoneType zoneId);

  //! read the RULE resource: a list of paragraphs
  bool readParagraphs(MWAWEntry const &entry, NSStruct::ZoneType zoneId);

  //
  // low level
  //

  //! find the file pos which correspond to a pos
  long findFilePos(NSStruct::ZoneType zoneId, NSStruct::Position const &pos);
private:
  NSText(NSText const &orig);
  NSText &operator=(NSText const &orig);

protected:
  //
  // data
  //
  //! the input
  MWAWInputStreamPtr m_input;

  //! the listener
  NSContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<NSTextInternal::State> m_state;

  //! the main parser;
  NSParser *m_mainParser;

};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
