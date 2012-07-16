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

#include "MWAWPosition.hxx"

#include "MWAWEntry.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

typedef class MWAWContentListener MSKContentListener;
typedef shared_ptr<MSKContentListener> MSKContentListenerPtr;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

namespace MSKGraphInternal
{
struct Font;
struct Zone;
struct DataPict;
struct Table;
struct GroupZone;
struct TextBox;
struct State;
class SubDocument;
}

class MSKParser;
class MSK3Parser;
class MSK4Zone;

/** \brief the main class to read the text part of Claris Works file
 *
 *
 *
 */
class MSKGraph
{
  friend class MSK3Parser;
  friend class MSK4Zone;
  friend class MSKGraphInternal::SubDocument;
public:
  //! constructor
  MSKGraph(MWAWInputStreamPtr ip, MSKParser &parser, MWAWFontConverterPtr &convertissor);
  //! destructor
  virtual ~MSKGraph();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages(int zoneId) const;

  /** send a zone (textbox, ...) */
  void send(int id, MWAWPosition::AnchorTo anchor);

  /** send all the picture corresponding to a zone */
  void sendAll(int zoneId, bool mainZone);

  struct SendData {
    // constructor
    SendData();
    /** the type */
    enum Type { RBDR, RBIL, ALL } m_type;
    /** the rbil id */
    int m_id;
    /** the anchor */
    MWAWPosition::AnchorTo m_anchor;
    /** the page ( used if anchor==page) */
    int m_page;
    /** the size of the data ( used by rbil ) */
    Vec2i m_size;
  };
  /** sends all the object of a page, frame, ...  */
  void sendObjects(SendData const what);

  /** try to update positions knowing pages and lines height */
  void computePositions(int zoneId, std::vector<int> &linesHeight, std::vector<int> &pagesHeight);

protected:
  //! reinitializes all data
  void reset(MSKParser &parser, MWAWFontConverterPtr &convertissor);

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
  int getEntryPictureV1(int zoneId, MWAWEntry &zone);

  /** checks if the next zone is a v2 picture and returns a zone
      id. If not, returns -1.
   */
  int getEntryPicture(int zoneId, MWAWEntry &zone);

  // version 4 file

  /** reads the RBDR or a RBIL zone: a zone which seems to regroup all pages pictures */
  bool readRB(MWAWInputStreamPtr input, MWAWEntry const &entry);


  /** reads a Pict zone: a zone which seems to code in v4 : header/footer picture */
  bool readPictureV4(MWAWInputStreamPtr input, MWAWEntry const &entry);

  //! try to read a text zone
  bool readText(MSKGraphInternal::TextBox &textBox);
  /** send a textbox to the listener */
  void sendTextBox(int id);
  /** check the text box link */
  void checkTextBoxLinks(int zId);

  //! ask m_mainParser to send a frame text(v4)
  void sendFrameText(MWAWEntry const &entry, std::string const &frame);

  //! try to read a table zone
  bool readTable(MSKGraphInternal::Table &table);
  //! try to  a table zone
  void sendTable(int id);

  //! try to read a chart (very incomplete)
  bool readChart(MSKGraphInternal::Zone &zone);

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
  libmwaw::DebugFile &ascii() {
    return *m_asciiFile;
  }

private:
  MSKGraph(MSKGraph const &orig);
  MSKGraph &operator=(MSKGraph const &orig);

protected:
  //
  // data
  //
  //! the input
  MWAWInputStreamPtr m_input;

  //! the listener
  MSKContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<MSKGraphInternal::State> m_state;

  //! the main parser;
  MSKParser *m_mainParser;

  //! the debug file
  libmwaw::DebugFile *m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
