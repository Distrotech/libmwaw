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
 * Parser to GreatWorks document
 *
 */
#ifndef GREAT_WKS_TEXT
#  define GREAT_WKS_TEXT

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"

namespace GreatWksTextInternal
{
struct State;
struct Token;
struct Zone;
}

class GreatWksParser;
class GreatWksSSParser;

/** \brief the main class to read the text part of GreatWorks Text file
 *
 *
 *
 */
class GreatWksText
{
  friend class GreatWksParser;
  friend class GreatWksSSParser;
public:
  //! constructor
  GreatWksText(MWAWParser &parser);
  //! destructor
  virtual ~GreatWksText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:
  //! finds the different objects zones
  bool createZones(int expectedHF);
  //! send a main zone
  bool sendMainText();
  //! return the number of header/footer zones
  int numHFZones() const;
  //! try to send the i^th header/footer
  bool sendHF(int id);
  //! check if a textbox can be send in a graphic zone, ie. does not contains any graphic
  bool canSendTextBoxAsGraphic(MWAWEntry const &entry);
  //! try to send the textbox text
  bool sendTextbox(MWAWEntry const &entry, MWAWListenerPtr listener=MWAWListenerPtr());
  //! sends the data which have not yet been sent to the listener
  void flushExtra();
  //! read the final font id corresponding to a file id
  int getFontId(int fileId) const;

  //
  // intermediate level
  //

  //! try to read the font names zone
  bool readFontNames();
  //! try to read a zone ( textheader+fonts+rulers)
  bool readZone(GreatWksTextInternal::Zone &zone);
  //! try to read the end of a zone ( line + frame position )
  bool readZonePositions(GreatWksTextInternal::Zone &zone);
  //! try to send a zone
  bool sendZone(GreatWksTextInternal::Zone const &zone, MWAWListenerPtr listener=MWAWListenerPtr());
  //! try to send simplified textbox zone
  bool sendSimpleTextbox(MWAWEntry const &entry, MWAWListenerPtr listener=MWAWListenerPtr());
  //! try to read a font
  bool readFont(MWAWFont &font);
  //! try to read a ruler
  bool readRuler(MWAWParagraph &para);
  //! try to read a token
  bool readToken(GreatWksTextInternal::Token &token, long &nChar);

  //! heuristic function used to find the next zone
  bool findNextZone();

  /** a struct used to defined the different callback */
  struct Callback {
    /** callback used to send a page break */
    typedef void (MWAWParser::* NewPage)(int page);
    //! callback used to send a picture
    typedef bool (MWAWParser::* SendPicture)(MWAWEntry const &entry, MWAWPosition pos);
    //! callback used to return the main section
    typedef MWAWSection(MWAWParser::* GetMainSection)() const;

    /** constructor */
    Callback() : m_newPage(0), m_sendPicture(0), m_mainSection(0) { }
    /** copy constructor */
    Callback(Callback const &orig) : m_newPage(0), m_sendPicture(0), m_mainSection(0)
    {
      *this=orig;
    }
    /** copy operator */
    Callback &operator=(Callback const &orig)
    {
      m_newPage=orig.m_newPage;
      m_sendPicture=orig.m_sendPicture;
      m_mainSection=orig.m_mainSection;
      return *this;
    }
    /** the new page callback */
    NewPage m_newPage;
    /** the send picture callback */
    SendPicture m_sendPicture;
    /** the get main section callback */
    GetMainSection m_mainSection;
  };
  //! set the callback
  void setCallback(Callback const &callback)
  {
    m_callback=callback;
  }
private:
  GreatWksText(GreatWksText const &orig);
  GreatWksText &operator=(GreatWksText const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<GreatWksTextInternal::State> m_state;

  //! the main parser;
  MWAWParser *m_mainParser;

  //! the different callbacks
  Callback m_callback;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
