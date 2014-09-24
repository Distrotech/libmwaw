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

#ifndef MS_WKS4_TEXT
#  define MS_WKS4_TEXT

#include <vector>

#include "MWAWEntry.hxx"
#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

namespace MsWks4TextInternal
{
struct Font;
struct Paragraph;
struct State;
}

class MsWksDocument;
class MsWks4Zone;

/** The class which parses text zones in a mac MS Works document v4
 *
 * This class must be associated with a MsWks4Zone. It reads the entries:
 * - TEXT : the text strings
 * - FONT : the fonts name
 * - FDPC, BTEC : the fonts properties
 * - FDPP, BTEP : the paragraph properties
 * - FTNT : the footnote definition
 * - PGD : the page break (only parsed)
 * - TOKN : the field properties (pagenumber, date, ...)
 */
class MsWks4Text
{
  friend class MsWks4Zone;
protected:
  struct DataFOD;
  /** callback when a new attribute is found in an FDPP/FDPC entry
   *
   * \param input defines the input
   * \param endPos the last position in the input
   * \return true and filled id if this attribute can be parsed
   * \note mess can be filled to add a message in debugFile */
  typedef bool (MsWks4Text::* FDPParser)(MWAWInputStreamPtr &input, long endPos,
                                         int &id, std::string &mess);
public:
  //! constructor
  MsWks4Text(MsWksDocument &document);

  //! destructor
  ~MsWks4Text();

  //! sets the default font
  void setDefault(MWAWFont &font);

  //! returns the number of pages
  int numPages() const;

  //! sends the data which have not been sent: actually do nothing
  void flushExtra(MWAWInputStreamPtr /*input*/) {}

protected:
  /** finds and parses all structures which correspond to the text
   *
   * More precisely the TEXT, FONT, FDPC/FDPP, BTEC/BTEP, FTNT, PGD, TOKN entries */
  bool readStructures(MWAWInputStreamPtr input, bool mainOle);

  //! reads a text section and send it to the listener
  bool readText(MWAWInputStreamPtr input, MWAWEntry const &entry, bool mainOle);

  //! sends the text which corresponds to footnote \a id to the listner
  bool readFootNote(MWAWInputStreamPtr input, int id);

  //----------------------------------------
  // PLC parsing, setting
  //----------------------------------------
  /** definition of the plc data parser (low level)
   *
   * \param endPos the end of the properties' definition,
   * \param bot defined the begin of the text zone corresponding to these properties
   * \param eot defined the end of the text zone corresponding to these properties
   * \param id the number of this properties
   * \param mess a string which can be filled to indicate unparsed data */
  typedef bool (MsWks4Text::* DataParser)
  (MWAWInputStreamPtr input, long endPos,  long bot, long eot, int id, std::string &mess);

  /** reads a PLC (Pointer List Composant ?) in zone entry
   *
   * \param input the file's input
   * \param entry the zone which contains the plc
   * \param textPtrs lists of offset in text zones where properties changes
   * \param listValues lists of properties values (filled only if values are simple types: int, ..)
   * \param parser the parser to use to read the values */
  bool readPLC(MWAWInputStreamPtr input, MWAWEntry const &entry,
               std::vector<long> &textPtrs, std::vector<long> &listValues,
               DataParser parser = &MsWks4Text::defDataParser);
  /** reads a PLC (Pointer List Composant ?) in zone entry
   *
   * \param input the file's input
   * \param entry the zone which contains the plc
   * \param textPtrs lists of offset in text zones where properties changes
   * \param listValues lists of properties values (filled only if values are simple types: int, ..)
   */
  bool readSimplePLC(MWAWInputStreamPtr &input, MWAWEntry const &entry,
                     std::vector<long> &textPtrs,
                     std::vector<long> &listValues)
  {
    return readPLC(input, entry, textPtrs, listValues);
  }

  //! the default parser (does nothing)
  bool defDataParser(MWAWInputStreamPtr input, long endPos,
                     long bot, long eot, int id, std::string &mess);

  //! reads the font names entry : FONT
  bool readFontNames(MWAWInputStreamPtr input, MWAWEntry const &entry);

  //! reads a font properties
  bool readFont(MWAWInputStreamPtr &input, long endPos,
                int &id, std::string &mess);

  /** sends a paragraph properties to the listener */
  void setProperty(MsWks4TextInternal::Paragraph const &tabs);
  //! reads a paragraph properties
  bool readParagraph(MWAWInputStreamPtr &input, long endPos,
                     int &id, std::string &mess);

  //! parses the footnote position : FTNT
  bool ftntDataParser(MWAWInputStreamPtr input, long endPos,
                      long bot, long eot, int id, std::string &mess);

  //! parses the object position : EOBJ
  bool eobjDataParser(MWAWInputStreamPtr input, long endPos,
                      long bot, long eot, int id, std::string &mess);

  /** parses the field properties entries : TOKN.
   *
   * \note the read data are not used to create the document */
  bool toknDataParser(MWAWInputStreamPtr input, long endPos,
                      long bot, long eot, int id, std::string &mess);

  /** parses the pagebreak positin entries : PGD
   *
   * \note the read data are not used to create the document */
  bool pgdDataParser(MWAWInputStreamPtr input, long endPos,
                     long , long, int id, std::string &mess);

  //! sends to the listener the text which corresponds to noteId
  void flushNote(int noteId);

protected:
  /** function which takes two sorted list of attribute (by text position).
      \return a list of attribute */
  std::vector<DataFOD> mergeSortedLists
  (std::vector<DataFOD> const &lst1, std::vector<DataFOD> const &lst2) const;

  /** parses a FDPP or a FDPC entry (which contains a list of ATTR_TEXT/ATTR_PARAG
   * with their definition ) and adds found data in listFODs */
  bool readFDP(MWAWInputStreamPtr &input, MWAWEntry const &entry,
               std::vector<DataFOD> &fods, FDPParser parser);

  /** Fills the vector of (FDPCs/FDPPs) paragraph/characters strutures
   *
   * Uses the entry BTEC/BTEP : the normal ways, and calls readSimplePLC on each entry to check that the parsing is correct
   * \param input the file input
   * \param which set to 0 for paragraphs structures and to 1 for characters structures
   */
  bool findFDPStructures(MWAWInputStreamPtr &input, int which);
  /** Fills the vector of (FDPCs/FDPPs) paragraph/characters strutures,
   * a function to call when the normal ways fails
   *
   * Uses all entries FDPCs/FDPPs and calls readSimplePLC on each entry to check that the parsing is correct.
   * \param input the file input
   * \param which set to 0 for paragraphs structures and to 1 for characters structures
   */
  bool findFDPStructuresByHand(MWAWInputStreamPtr &input, int which);

protected:
  //! structure which retrieves data information which correspond to a text position
  struct DataFOD {
    /** different type which can be associated to a text position
     *
     * ATTR_TEXT: all text attributes (font, size, ...)
     * ATTR_PARAG: all paragraph attributes (margin, tabs, ...)
     * ATTR_PLC: other attribute (note, fields ... )
     */
    enum Type { ATTR_TEXT, ATTR_PARAG, ATTR_PLC, ATTR_UNKN };

    //! the constructor
    DataFOD() : m_type(ATTR_UNKN), m_pos(-1), m_defPos(0), m_id(-1) {}

    //! the type of the attribute
    Type m_type;
    //! the offset position of the text modified by this attribute
    long m_pos;
    //! the offset position of the definition of the attribute in the file
    long m_defPos;
    //! an identificator (which must be unique by category)
    int m_id;
  };

private:
  MsWks4Text(MsWks4Text const &orig);
  MsWks4Text &operator=(MsWks4Text const &orig);
protected:
  //! the main parser;
  MWAWParser *m_mainParser;
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the main document
  MsWksDocument &m_document;

  //! an entry which corresponds to the complete text zone
  MWAWEntry m_textPositions;

  //! the internal state
  mutable shared_ptr<MsWks4TextInternal::State> m_state;

  //! the list of a FOD
  std::vector<DataFOD> m_FODsList;

  //! the list of FDPC entries
  std::vector<MWAWEntry const *> m_FDPCs;
  //! the list of FDPP entries
  std::vector<MWAWEntry const *> m_FDPPs;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
