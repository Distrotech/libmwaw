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
 * Parser to Microsoft Works text document ( graphic part )
 *
 */
#ifndef MSK_MWAW_GRAPH
#  define MSK_MWAW_GRAPH

#include <list>
#include <string>
#include <vector>

#include "DMWAWPageSpan.hxx"

#include "TMWAWPosition.hxx"

#include "IMWAWEntry.hxx"
#include "IMWAWContentListener.hxx"
#include "IMWAWSubDocument.hxx"

#include "TMWAWDebug.hxx"
#include "TMWAWInputStream.hxx"

#include "IMWAWParser.hxx"

typedef class MWAWContentListener MSKContentListener;
typedef shared_ptr<MSKContentListener> MSKContentListenerPtr;

namespace MWAWStruct
{
class Font;
}

namespace MWAWTools
{
class Convertissor;
typedef shared_ptr<Convertissor> ConvertissorPtr;
}

namespace libmwaw_tools
{
class PictData;
}

namespace MSKGraphInternal
{
struct Font;
struct Zone;
struct DataPict;
struct GroupZone;
struct TextBox;
struct State;
}

class MSKParser;

/** \brief the main class to read the text part of Claris Works file
 *
 *
 *
 */
class MSKGraph
{
  friend class MSKParser;

public:
  //! constructor
  MSKGraph(TMWAWInputStreamPtr ip, MSKParser &parser, MWAWTools::ConvertissorPtr &convertissor);
  //! destructor
  virtual ~MSKGraph();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

  /** send a zone (textbox, ...) */
  void send(int id, bool local=true);

  /** send page */
  void sendAll();

  /** try to update positions knowing pages and lines height */
  void computePositions(std::vector<int> &linesHeight, std::vector<int> &pagesHeight);

protected:

  //! sets the listener in this class and in the helper classes
  void setListener(MSKContentListenerPtr listen) {
    m_listener = listen;
  }

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // Intermediate level
  //

  //! read the picture header
  bool readPictHeader(MSKGraphInternal::Zone &pict);

  /** checks if the next zone is a v1 picture and returns a zone
      id. If not, returns -1.
   */
  int getEntryPictureV1(IMWAWEntry &zone);

  /** checks if the next zone is a v2 picture and returns a zone
      id. If not, returns -1.
   */
  int getEntryPicture(IMWAWEntry &zone);


  //! try to read a text zone
  bool readText(MSKGraphInternal::TextBox &textBox);
  /** send a textbox to the listener */
  void send(MSKGraphInternal::TextBox &textBox);

  //
  // low level
  //
  /** try to read the group data*/
  shared_ptr<MSKGraphInternal::GroupZone> readGroup(MSKGraphInternal::Zone &group);
  //! reads the textbox font
  bool readFont(MSKGraphInternal::Font &font);
  //! send the font properties
  void setProperty(MSKGraphInternal::Font const &font);

  //! returns the debug file
  libmwaw_tools::DebugFile &ascii() {
    return m_asciiFile;
  }

private:
  MSKGraph(MSKGraph const &orig);
  MSKGraph &operator=(MSKGraph const &orig);

protected:
  //
  // data
  //
  //! the input
  TMWAWInputStreamPtr m_input;

  //! the listener
  MSKContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWTools::ConvertissorPtr m_convertissor;

  //! the state
  shared_ptr<MSKGraphInternal::State> m_state;

  //! the main parser;
  MSKParser *m_mainParser;

  //! the debug file
  libmwaw_tools::DebugFile &m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
