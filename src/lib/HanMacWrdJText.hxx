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
 * Parser to HanMac Word-J text document
 *
 */
#ifndef HAN_MAC_WRD_J_TEXT
#  define HAN_MAC_WRD_J_TEXT

#include <map>
#include <vector>

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"

namespace HanMacWrdJTextInternal
{
struct Paragraph;
class SubDocument;
struct TextZone;
struct State;
}

class HanMacWrdJParser;

/** \brief the main class to read the text part of HanMac Word-J file
 *
 *
 *
 */
class HanMacWrdJText
{
  friend class HanMacWrdJTextInternal::SubDocument;
  friend class HanMacWrdJParser;
public:
  //! constructor
  HanMacWrdJText(HanMacWrdJParser &parser);
  //! destructor
  virtual ~HanMacWrdJText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:
  //! send the main text zone
  bool sendMainText();
  //! send a text zone
  bool sendText(long id, long cPos, bool asGraphic=false);
  //! check if we can send a textzone as graphic
  bool canSendTextAsGraphic(long id, long cPos);
  //! send a text zone
  bool sendText(HanMacWrdJTextInternal::TextZone const &zone, long cPos, bool asGraphic);
  //! check if we can send a textzone has graphic
  bool canSendTextAsGraphic(HanMacWrdJTextInternal::TextZone const &zone, long cPos);
  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //! compute the number of pages present in a zone
  int computeNumPages(HanMacWrdJTextInternal::TextZone const &zone);
  //! returns the list of zoneId which corresponds to the token
  std::vector<long> getTokenIdList() const;
  //! update the text zone type with map id->type
  void updateTextZoneTypes(std::map<long,int> const &idTypeMap);
  /** update the footnote text zone id and the list of first char position */
  void updateFootnoteInformations(long const &textZId, std::vector<long> const &fPosList);

  //
  // intermediate level
  //

  /** try to read the fonts name zone (type 15)*/
  bool readFontNames(MWAWEntry const &entry);
  /** try to read the fonts zone (type 0)*/
  bool readFonts(MWAWEntry const &entry);
  /** try to read the font ( reading up to endPos if endPos is defined ) */
  bool readFont(MWAWFont &font, long endPos=-1);
  /** try to read the paragraphs zone (type 1)*/
  bool readParagraphs(MWAWEntry const &entry);
  /** try to read a paragraph  ( reading up to endPos if endPos is defined ) */
  bool readParagraph(HanMacWrdJTextInternal::Paragraph &para, long endPos=-1);
  /** try to read the style zone (type 2) */
  bool readStyles(MWAWEntry const &entry);
  /** try to read the list of textzones ( type 4) */
  bool readTextZonesList(MWAWEntry const &entry);
  /** try to read a text zone ( type 5 ) */
  bool readTextZone(MWAWEntry const &entry, int actZone);
  /** try to read the token in the text zone */
  bool readTextToken(long endPos, HanMacWrdJTextInternal::TextZone &zone);
  /** try to read the different sections*/
  bool readSections(MWAWEntry const &entry);
  /** try to read the footnote position*/
  bool readFtnPos(MWAWEntry const &entry);

  //
  // low level
  //

private:
  HanMacWrdJText(HanMacWrdJText const &orig);
  HanMacWrdJText &operator=(HanMacWrdJText const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<HanMacWrdJTextInternal::State> m_state;

  //! the main parser;
  HanMacWrdJParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
