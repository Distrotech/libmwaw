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
 * For further information visit http://libwps.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

/*
 * Parser to WriteNow text document
 *
 */
#ifndef FW_MWAW_TEXT
#  define FW_MWAW_TEXT

#include <list>
#include <string>
#include <vector>

#include "libmwaw_tools.hxx"

#include "DMWAWPageSpan.hxx"

#include "TMWAWPosition.hxx"

#include "IMWAWEntry.hxx"
#include "IMWAWContentListener.hxx"
#include "IMWAWSubDocument.hxx"

#include "TMWAWDebug.hxx"
#include "TMWAWInputStream.hxx"

#include "IMWAWParser.hxx"

typedef class MWAWContentListener FWContentListener;
typedef shared_ptr<FWContentListener> FWContentListenerPtr;

namespace MWAWStruct
{
class Font;
}

namespace MWAWTools
{
class Convertissor;
typedef shared_ptr<Convertissor> ConvertissorPtr;
}

namespace FWTextInternal
{
struct Font;
struct Ruler;

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
  FWText(TMWAWInputStreamPtr ip, FWParser &parser, MWAWTools::ConvertissorPtr &convertissor);
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
            MWAWStruct::Font &font);

  //! check if a zone is a style zone, if so read it...
  bool readStyle(shared_ptr<FWEntry> zone);

  //! sort the different zones, finding the main zone, ...
  void sortZones();

  //
  // low level
  //

  /*
   * \param font the font's properties
   * \param force if false, we only sent differences from the actual font */
  void setProperty(MWAWStruct::Font const &font,
                   MWAWStruct::Font &previousFont,
                   bool force = false);

  /* send the ruler properties */
  void setProperty(FWTextInternal::Ruler const &para);

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
  TMWAWInputStreamPtr m_input;

  //! the listener
  FWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWTools::ConvertissorPtr m_convertissor;

  //! the state
  shared_ptr<FWTextInternal::State> m_state;

  //! the main parser;
  FWParser *m_mainParser;

};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
