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
 * Parser to HanMac Word text document
 *
 */
#ifndef HMWK_TEXT
#  define HMWK_TEXT

#include <map>

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"

namespace HMWKTextInternal
{
struct Paragraph;
struct Token;
struct State;
}

struct HMWKZone;
class HMWKParser;

/** \brief the main class to read the text part of HanMac Word file
 *
 *
 *
 */
class HMWKText
{
  friend class HMWKParser;
public:
  //! constructor
  HMWKText(HMWKParser &parser);
  //! destructor
  virtual ~HMWKText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:
  //! send the main text zone
  bool sendMainText();
  //! send a text zone
  bool sendText(long id, long subId, bool asGraphic=false);
  //! check if we can send a textzone as graphic
  bool canSendTextAsGraphic(long id, long subId);
  //! sends the data which have not yet been sent to the listener
  void flushExtra();
  //! update the text zone type with map id->type
  void updateTextZoneTypes(std::map<long,int> const &idTypeMap);
  //! returns the list of zoneId which corresponds to the token
  std::vector<long> const &getTokenIdList() const;
  //! return sthe header/footer id
  void getHeaderFooterId(long &headerId, long &footerId) const;

  //
  // intermediate level
  //

  /** try to read a text zone (type 1)*/
  bool readTextZone(shared_ptr<HMWKZone> zone);
  /** try to read the fonts name zone (type 5)*/
  bool readFontNames(shared_ptr<HMWKZone> zone);
  /** try to read the style zone (type 3) */
  bool readStyles(shared_ptr<HMWKZone> zone);
  /** try to read a section info zone (type 4)*/
  bool readSections(shared_ptr<HMWKZone> zone);

  /** try to send a text zone (type 1)*/
  bool sendText(HMWKZone &zone, bool asGraphic=false);
  /** check if we can send a textzone has graphic */
  bool canSendTextAsGraphic(HMWKZone &zone);

  //
  // low level
  //
  /** try to read a font in a text zone */
  bool readFont(HMWKZone &zone, MWAWFont &font);

  /** try to read a paragraph in a text zone */
  bool readParagraph(HMWKZone &zone, HMWKTextInternal::Paragraph &para);
  /** send the ruler properties */
  void setProperty(HMWKTextInternal::Paragraph const &para, float width);

  /** try to read an token in a text zone */
  bool readToken(HMWKZone &zone, HMWKTextInternal::Token &token);

private:
  HMWKText(HMWKText const &orig);
  HMWKText &operator=(HMWKText const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<HMWKTextInternal::State> m_state;

  //! the main parser;
  HMWKParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
