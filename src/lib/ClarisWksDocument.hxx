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
 * Document data used by the Claris Works parser
 *
 */
#ifndef CLARIS_WKS_DOCUMENT
#  define CLARIS_WKS_DOCUMENT

#include <vector>

#include "libmwaw_internal.hxx"

#include "MWAWPosition.hxx"

class MWAWSection;

namespace ClarisWksDocumentInternal
{
struct State;
}

class ClarisWksDatabase;
class ClarisWksGraph;
class ClarisWksParser;
class ClarisWksStyleManager;
class ClarisWksPresentation;
class ClarisWksSpreadsheet;
class ClarisWksTable;
class ClarisWksText;

namespace ClarisWksStruct
{
struct DSET;
}
//! main document information used to create a ClarisWorks file
class ClarisWksDocument
{
public:
  friend class ClarisWksParser;
  //! constructor
  ClarisWksDocument(MWAWParser &parser);
  //! virtual destructor
  ~ClarisWksDocument();

  //! returns the number of expected pages ( accross pages x down page)
  Vec2i getDocumentPages() const;
  //! returns the page height, ie. paper size less margin (in inches) less header/footer size
  double getTextHeight() const;
  //! returns the page left top point ( in inches)
  Vec2f getPageLeftTop() const;
  //! returns the header/footer id
  void getHeaderFooterId(int &headerId, int &footerId) const;
  //! set the header/footer id
  void setHeaderFooterId(int &id, bool header);

  //! returns the main parser
  MWAWParser &getMainParser()
  {
    return *m_parser;
  }
  //! returns the style manager
  shared_ptr<ClarisWksStyleManager> getStyleManager()
  {
    return m_styleManager;
  }
  //! returns the database parser
  shared_ptr<ClarisWksDatabase> getDatabaseParser()
  {
    return m_databaseParser;
  }
  //! returns the graph parser
  shared_ptr<ClarisWksGraph> getGraphParser()
  {
    return m_graphParser;
  }
  //! returns the presentation parser
  shared_ptr<ClarisWksPresentation> getPresentationParser()
  {
    return m_presentationParser;
  }
  //! returns the spreadsheet parser
  shared_ptr<ClarisWksSpreadsheet> getSpreadsheetParser()
  {
    return m_spreadsheetParser;
  }
  //! returns the table parser
  shared_ptr<ClarisWksTable> getTableParser()
  {
    return m_tableParser;
  }
  //! returns the text parser
  shared_ptr<ClarisWksText> getTextParser()
  {
    return m_textParser;
  }

  //! reads the document header
  bool readDocHeader();
  //! reads the document info part ( end of the header)
  bool readDocInfo();
  //! read the print info zone
  bool readPrintInfo();
  /* read the document summary */
  bool readDSUM(MWAWEntry const &entry, bool inHeader);

  //! try to read a structured zone
  bool readStructZone(char const *zoneName, bool hasEntete);
  /** try to read a int structured zone
      where \a fSz to the int size: 1(int8), 2(int16), 4(int32) */
  bool readStructIntZone(char const *zoneName, bool hasEntete, int fSz, std::vector<int> &res);

  /** small fonction used to check unusual endian ordering of a list of int16_t, int32_t*/
  void checkOrdering(std::vector<int16_t> &vec16, std::vector<int32_t> &vec32) const;

  //! returns the main document section
  MWAWSection getMainSection() const;
  //! return the zone corresponding to an id ( low level)
  shared_ptr<ClarisWksStruct::DSET> getZone(int zId) const;
  /** send a page break */
  void newPage(int page);
  //! indicates that a zone is parser
  void forceParsed(int zoneId);
  //! check if we can send a zone as a graphic
  bool canSendZoneAsGraphic(int number) const;
  //! try to send a zone
  bool sendZone(int zoneId, bool asGraphic, MWAWPosition pos=MWAWPosition());
  /** ask the main parser to create a document to send a footnote */
  void sendFootnote(int zoneId);

protected:
  //! the state
  shared_ptr<ClarisWksDocumentInternal::State> m_state;
public:
  //! the parser state
  shared_ptr<MWAWParserState> m_parserState;

protected:
  //! the main parser
  MWAWParser *m_parser;
  //! the style manager
  shared_ptr<ClarisWksStyleManager> m_styleManager;

  //! the database parser
  shared_ptr<ClarisWksDatabase> m_databaseParser;
  //! the graph parser
  shared_ptr<ClarisWksGraph> m_graphParser;
  //! the spreadsheet parser
  shared_ptr<ClarisWksPresentation> m_presentationParser;
  //! the spreadsheet parser
  shared_ptr<ClarisWksSpreadsheet> m_spreadsheetParser;
  //! the table parser
  shared_ptr<ClarisWksTable> m_tableParser;
  //! the text parser
  shared_ptr<ClarisWksText> m_textParser;

  //
  // the callback
  //

  //! callback used to check if we can send a zone as a graphic
  typedef bool (MWAWParser::* CanSendZoneAsGraphic)(int number) const;
  //! callback used to send a forceParsed
  typedef void (MWAWParser::* ForceParsed)(int zoneId);
  //! callback used to return a zone
  typedef shared_ptr<ClarisWksStruct::DSET> (MWAWParser::* GetZone)(int zId) const;
  /** callback used to send a page break */
  typedef void (MWAWParser::* NewPage)(int page);
  //! callback used to send a footnote
  typedef void (MWAWParser::* SendFootnote)(int zoneId);
  //! callback used to send a zone
  typedef bool (MWAWParser::* SendZone)(int zoneId, bool asGraphic, MWAWPosition pos);

  /** callback to check if we can send a zone as graphic */
  CanSendZoneAsGraphic m_canSendZoneAsGraphic;
  /** the force parsed callback */
  ForceParsed m_forceParsed;
  /** the callback used to return a zone*/
  GetZone m_getZone;
  /** the new page callback */
  NewPage m_newPage;
  /** the send footnote callback */
  SendFootnote m_sendFootnote;
  /** the send zone callback */
  SendZone m_sendZone;

private:
  ClarisWksDocument(ClarisWksDocument const &orig);
  ClarisWksDocument operator=(ClarisWksDocument const &orig);
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
