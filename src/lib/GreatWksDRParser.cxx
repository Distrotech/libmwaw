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

#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWGraphicListener.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSection.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "GreatWksDocument.hxx"
#include "GreatWksGraph.hxx"
#include "GreatWksText.hxx"

#include "GreatWksDRParser.hxx"

/** Internal: the structures of a GreatWksDRParser */
namespace GreatWksDRParserInternal
{

////////////////////////////////////////
//! Internal: the state of a GreatWksDRParser
struct State {
  //! constructor
  State() : m_columnsWidth(), m_hasColSep(false), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
    for (int i=0; i<4; ++i)
      m_hfFlags[i]=false;
  }
  //! returns the number of expected header/footer zones
  int numHeaderFooters() const
  {
    int num=0;
    if (m_hfFlags[2]) num++; // header
    if (m_hfFlags[3]) num++; // footer
    if (m_hfFlags[1]) num*=2; // lf page
    return num;
  }

  //! returns a section
  MWAWSection getSection() const
  {
    MWAWSection sec;
    size_t numCols = m_columnsWidth.size()/2;
    if (numCols <= 1)
      return sec;
    sec.m_columns.resize(size_t(numCols));
    if (m_hasColSep)
      sec.m_columnSeparator=MWAWBorder();
    for (size_t c=0; c < numCols; c++) {
      double wSep=0;
      if (c)
        wSep += sec.m_columns[c].m_margins[libmwaw::Left]=
                  double(m_columnsWidth[2*c]-m_columnsWidth[2*c-1])/72./2.;
      if (c+1!=numCols)
        wSep+=sec.m_columns[c].m_margins[libmwaw::Right]=
                double(m_columnsWidth[2*c+2]-m_columnsWidth[2*c+1])/72./2.;
      sec.m_columns[c].m_width =
        double(m_columnsWidth[2*c+1]-m_columnsWidth[2*c])+72.*wSep;
      sec.m_columns[c].m_widthUnit = librevenge::RVNG_POINT;
    }
    return sec;
  }

  //! the columns dimension
  std::vector<double> m_columnsWidth;
  //! flags to define header/footer (titlePage, l/rPage, header, footer)
  bool m_hfFlags[4];
  //! true if columns have columns separator
  bool m_hasColSep;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
GreatWksDRParser::GreatWksDRParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWGraphicParser(input, rsrcParser, header), m_state(), m_document()
{
  init();
}

GreatWksDRParser::~GreatWksDRParser()
{
}

void GreatWksDRParser::init()
{
  resetGraphicListener();
  setAsciiName("main-1");

  m_state.reset(new GreatWksDRParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);

  m_document.reset(new GreatWksDocument(*this));
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void GreatWksDRParser::parse(librevenge::RVNGDrawingInterface *docInterface)
{
  assert(getInput().get() != 0);
  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = false;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      m_document->getGraphParser()->sendPageGraphics();
      m_document->getTextParser()->sendMainText();
#ifdef DEBUG
      m_document->getTextParser()->flushExtra();
#endif
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("GreatWksDRParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetGraphicListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void GreatWksDRParser::createDocument(librevenge::RVNGDrawingInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getGraphicListener()) {
    MWAW_DEBUG_MSG(("GreatWksDRParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  int numPages = 1;
  if (m_document->getGraphParser()->numPages() > numPages)
    numPages = m_document->getGraphParser()->numPages();
  if (m_document->getTextParser()->numPages() > numPages)
    numPages = m_document->getTextParser()->numPages();
  m_state->m_numPages = numPages;

  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan ps(getPageSpan());
  ps.setPageSpan(numPages);
  pageList.push_back(ps);
  MWAWGraphicListenerPtr listen(new MWAWGraphicListener(*getParserState(), pageList, documentInterface));
  setGraphicListener(listen);
  listen->startDocument();
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool GreatWksDRParser::createZones()
{
  m_document->readRSRCZones();
  MWAWInputStreamPtr input = getInput();
  long pos;
  ascii().addPos(40);
  ascii().addNote("Entries(GZoneHeader)");
  ascii().addDelimiter(68,'|');
  pos = 74;
  input->seek(74, librevenge::RVNG_SEEK_SET);
  if (!m_document->getTextParser()->readFontNames())
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  else
    pos = input->tell();

  bool ok=m_document->getGraphParser()->readGraphicZone();
  if (!input->isEnd()) {
    pos = input->tell();
    MWAW_DEBUG_MSG(("GreatWksDRParser::createZones: find some extra data\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Loose):");
    ascii().addPos(pos+200);
    ascii().addNote("_");
  }
  return ok;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool GreatWksDRParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = GreatWksDRParserInternal::State();
  if (!m_document->checkHeader(header,strict)) return false;
  return getParserState()->m_kind==MWAWDocument::MWAW_K_DRAW;
}



// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
