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
 * Parser to Microsoft Word text document
 *
 */
#ifndef MS_WRD_MWAW_TEXT
#  define MS_WRD_MWAW_TEXT

#include <map>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"

#include "MWAWEntry.hxx"

#include "MWAWDebug.hxx"

#include "MsWrdTextStyles.hxx"

namespace MsWrdTextInternal
{
struct State;
struct Table;
}

struct MsWrdEntry;
class MsWrdParser;
class MsWrdTextStyles;

/** \brief the main class to read the text part of Microsoft Word file */
class MsWrdText
{
  friend class MsWrdParser;
  friend class MsWrdTextStyles;
public:
  //! Internal: the plc
  struct PLC {
    enum Type { TextPosition, HeaderFooter, Page, Section, ParagraphInfo, Paragraph, Font, Footnote, FootnoteDef, Field, Object };
    PLC(Type type, int id=0) : m_type(type), m_id(id), m_extra("")
    {
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, PLC const &plc);
    //! a comparaison structure
    struct ltstr {
      bool operator()(PLC const &s1, PLC const &s2) const
      {
        if (s1.m_type != s2.m_type)
          return int(s1.m_type) < int(s2.m_type);
        if (s1.m_id != s2.m_id)
          return s1.m_id < s2.m_id;
        return false;
      }
    };
    //! the plc type
    Type m_type;
    //! the identificator
    int m_id;
    //! some extra data
    std::string m_extra;
  };
public:
  //! constructor
  MsWrdText(MsWrdParser &parser);
  //! destructor
  virtual ~MsWrdText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

  /** returns the header entry */
  MWAWEntry getHeader() const;

  /** returns the footer entry */
  MWAWEntry getFooter() const;
protected:
  //! returns the parser state
  shared_ptr<MWAWParserState> &getParserState()
  {
    return m_parserState;
  }

  //! send a main zone
  bool sendMainText();

  //! send a text zone
  bool sendText(MWAWEntry const &textEntry, bool mainZone, bool tableCell=false);
  //! try to open a section
  bool sendSection(int sectionId);
  //! reads the three different zone size
  bool readHeaderTextLength();

  //! finds the different zones
  bool createZones(long bot);

  //! read the text structure(some paragraph style+some text position?)
  bool readTextStruct(MsWrdEntry &entry);

  //! read the page limit ?
  bool readPageBreak(MsWrdEntry &entry);

  //! read the paragraph height info
  bool readParagraphInfo(MsWrdEntry entry);

  //! read the field data
  bool readFields(MsWrdEntry &entry, std::vector<long> const &fieldPos);

  //! send a field note to a listener
  bool sendFieldComment(int id);

  //! read the footnote pos in text + val
  bool readFootnotesPos(MsWrdEntry &entry, std::vector<long> const &noteDef);

  //! read the footnote data
  bool readFootnotesData(MsWrdEntry &entry);

  //! send a note to a listener
  bool sendFootnote(int id);

  //! read the font names
  bool readFontNames(MsWrdEntry &entry);

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //! try to send a table.
  bool sendTable(MsWrdTextInternal::Table const &table);

  // interface with MsWrdTextStyles

  //! returns the main text length
  long getMainTextLength() const;
  //! returns the text correspondance zone ( textpos, plc )
  std::multimap<long, MsWrdText::PLC> &getTextPLCMap();
  //! returns the file correspondance zone ( filepos, plc )
  std::multimap<long, MsWrdText::PLC> &getFilePLCMap();

  //
  // low level
  //

  //! prepare the data to be send
  void prepareData();

  //! cut the text in line/cell pos
  void prepareLines();
  //! convert the file position in character position and compute the paragraph limit
  void convertFilePLCPos();
  //! retrieve the paragraph properties
  void prepareParagraphProperties();
  //! retrieve the font properties
  void prepareFontProperties();

  //! find the table end position knowing the end cell/pos delimiter
  void prepareTableLimits();
  //! try to find a table which begin at position cPos, if so, update its data...
  bool updateTableBeginnningAt(long cPos, long &nextCPos);

  //! read a zone which consists in a list of int
  bool readLongZone(MsWrdEntry &entry, int sz, std::vector<long> &list);

private:
  MsWrdText(MsWrdText const &orig);
  MsWrdText &operator=(MsWrdText const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<MsWrdTextInternal::State> m_state;

  //! the style manager
  shared_ptr<MsWrdTextStyles> m_stylesManager;

  //! the main parser;
  MsWrdParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
