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
 * Parser to FullWrite text document
 *
 */
#ifndef FW_TEXT
#  define FW_TEXT

#include "libmwaw_internal.hxx"


#include "MWAWDebug.hxx"

class MWAWInputStream;
typedef shared_ptr<MWAWInputStream> MWAWInputStreamPtr;

class MWAWContentListener;
typedef class MWAWContentListener FWContentListener;
typedef shared_ptr<FWContentListener> FWContentListenerPtr;

class MWAWFont;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

namespace FWTextInternal
{
struct Font;
struct Paragraph;

struct Zone;

struct State;
}

struct FWEntry;
struct FWEntryManager;

class FWParser;

/** \brief the main class to read the text part of writenow file
 *
 *
 *
 */
class FWText
{
  friend class FWParser;
public:
  //! constructor
  FWText(MWAWInputStreamPtr ip, FWParser &parser, MWAWFontConverterPtr &convertissor);
  //! destructor
  virtual ~FWText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:

  //! sets the listener in this class and in the helper classes
  void setListener(FWContentListenerPtr listen) {
    m_listener = listen;
  }

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //! send a main zone
  bool sendMainText();

  //! send a id zone
  bool send(int zId);

  //
  // intermediate level
  //

  //! check if a zone is a text zone, if so read it...
  bool readTextData(shared_ptr<FWEntry> zone);

  //! send the text
  bool send(shared_ptr<FWTextInternal::Zone> zone);

  //! send a simple line
  void send(shared_ptr<FWTextInternal::Zone> zone, int numChar,
            MWAWFont &font);

  //! check if a zone is a style zone, if so read it...
  bool readStyle(shared_ptr<FWEntry> zone);

  //! sort the different zones, finding the main zone, ...
  void sortZones();

  //
  // low level
  //

  /* send the character properties
   * \param font the font's properties */
  void setProperty(MWAWFont const &font, MWAWFont &previousFont);

  /* send the ruler properties */
  void setProperty(FWTextInternal::Paragraph const &para);

  //! check if the input of the zone points to a paragraph zone, ...
  bool readParagraph(shared_ptr<FWEntry> zone);

  //! check if the input of the zone points to the columns definition, ...
  bool readColumns(shared_ptr<FWEntry> zone);

  //! check if the input of the zone points to the correspondance definition, ...
  bool readCorrespondance(shared_ptr<FWEntry> zone, bool extraCheck=false);
  //! check if the input of the zone points to a custom style, ...
  bool readStyleName(shared_ptr<FWEntry> zone);

private:
  FWText(FWText const &orig);
  FWText &operator=(FWText const &orig);

protected:
  //
  // data
  //
  //! the input
  MWAWInputStreamPtr m_input;

  //! the listener
  FWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<FWTextInternal::State> m_state;

  //! the main parser;
  FWParser *m_mainParser;

};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
