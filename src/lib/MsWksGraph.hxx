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
 * Parser to Microsoft Works text document ( graphic part )
 *
 */
#ifndef MS_WKS_MWAW_GRAPH
#  define MS_WKS_MWAW_GRAPH

#include <list>
#include <string>
#include <vector>

#include "MWAWPosition.hxx"

#include "MWAWEntry.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWParser.hxx"

namespace MsWksGraphInternal
{
struct GroupZone;
struct TextBox;
struct Zone;

struct State;

class SubDocument;
}

class MsWksParser;
class MsWks3Parser;
class MsWks4Zone;
class MsWksTable;

/** \brief the main class to read the graphic of a Microsoft Works file */
class MsWksGraph
{
  friend class MsWks3Parser;
  friend class MsWks4Zone;
  friend class MsWksTable;
  friend class MsWksGraphInternal::SubDocument;
public:
  struct Style;
  //! constructor
  MsWksGraph(MsWksParser &parser);
  //! destructor
  virtual ~MsWksGraph();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages(int zoneId) const;

  /** send a zone (textbox, ...).

   \note if pos.size() is not defined, this function will retrieve the
   position, .. using the corresponding zone's data */
  void send(int id, MWAWPosition const &pos);

  /** send all the picture corresponding to a zone */
  void sendAll(int zoneId, bool mainZone);

  //! small struct used which picture need to be send
  struct SendData {
    //! constructor
    SendData() : m_type(RBDR), m_id(-1), m_anchor(MWAWPosition::Char), m_page(-1), m_size()
    {
    }
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
  void sendObjects(SendData const &what);

  /** try to update positions knowing pages and lines height */
  void computePositions(int zoneId, std::vector<int> &linesHeight, std::vector<int> &pagesHeight);

protected:
  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // Intermediate level
  //

  //! read the picture header
  bool readPictHeader(MsWksGraphInternal::Zone &pict);
  //! read the gradient structure ( v4)
  bool readGradient(Style &style);
  /** checks if the next zone is a v1 picture and returns a zone id ( or -1).*/
  int getEntryPictureV1(int zoneId, MWAWEntry &zone, bool autoSend=true);

  /** checks if the next zone is a v2 picture and returns a zone id ( or -1) */
  int getEntryPicture(int zoneId, MWAWEntry &zone, bool autoSend=true, int order=-1000);

  // version 4 file

  /** reads the RBDR or a RBIL zone: a zone which seems to regroup all pages pictures */
  bool readRB(MWAWInputStreamPtr input, MWAWEntry const &entry);


  /** reads a Pict zone: a zone which seems to code in v4 : header/footer picture */
  bool readPictureV4(MWAWInputStreamPtr input, MWAWEntry const &entry);

  //! try to read a text zone
  bool readText(MsWksGraphInternal::TextBox &textBox);
  //! try to send a text box zone v1-3
  void sendTextBox(int zId);
  /** check the text box link v4 */
  void checkTextBoxLinks(int zId);

  // interface function

  /** returns the graphic style of the zone defined by zoneId */
  bool getZoneGraphicStyle(int zoneId, MWAWGraphicStyle &style) const;
  /** returns the position of the zone defined by zoneId */
  bool getZonePosition(int zoneId, MWAWPosition::AnchorTo anchor, MWAWPosition &pos) const;

  //! ask m_mainParser to send a frame text(v4)
  void sendFrameText(MWAWEntry const &entry, std::string const &frame);

  //! try to  a table zone
  void sendTable(int id);

  //! try to send a chart
  void sendChart(int zoneId);

  //
  // low level
  //
  /** try to read the group data*/
  shared_ptr<MsWksGraphInternal::GroupZone> readGroup(MsWksGraphInternal::Zone &group);
  /** try to send a group */
  void sendGroup(int zoneId, MWAWPosition const &pos);
  /** try to send a group elements by elemenys*/
  void sendGroupChild(int zoneId, MWAWPosition const &pos);
  /** returns true if we can create a graphic for the whole group */
  bool canCreateGraphic(MsWksGraphInternal::GroupZone const &group) const;
  /** send the group as a graphic zone */
  void sendGroup(MsWksGraphInternal::GroupZone const &group, MWAWGraphicListenerPtr &listener) const;
  //! reads the textbox font
  bool readFont(MWAWFont &font);

public:
  //! Internal: the graphic style of MsWksGraph
  struct Style : public MWAWGraphicStyle {
    //! constructor
    Style() : MWAWGraphicStyle(), m_baseLineColor(MWAWColor::black()), m_baseSurfaceColor(MWAWColor::white())
    {
      m_fillRuleEvenOdd=true;
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Style const &st)
    {
      o << static_cast<MWAWGraphicStyle const &>(st);
      if (st.m_baseLineColor != st.m_lineColor)
        o << "lineColor[base]=" << st.m_baseLineColor << ",";
      if (st.m_baseSurfaceColor != st.m_surfaceColor)
        o << "surfaceColor[base]=" << st.m_baseSurfaceColor << ",";

      return o;
    }

    //! the line color
    MWAWColor m_baseLineColor;
    //! the 2D surface color
    MWAWColor m_baseSurfaceColor;
  };

private:
  MsWksGraph(MsWksGraph const &orig);
  MsWksGraph &operator=(MsWksGraph const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<MsWksGraphInternal::State> m_state;

  //! the main parser;
  MsWksParser *m_mainParser;

  //! the table manager
  shared_ptr<MsWksTable> m_tableParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
