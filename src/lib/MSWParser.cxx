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
 * For further information visit http://libmwaw.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

#include <libwpd/WPXString.h>

#include "TMWAWPosition.hxx"
#include "TMWAWPictMac.hxx"
#include "TMWAWPrint.hxx"

#include "IMWAWHeader.hxx"

#include "MWAWStruct.hxx"
#include "MWAWTools.hxx"
#include "MWAWContentListener.hxx"

#include "MSWParser.hxx"

/** Internal: the structures of a MSWParser */
namespace MSWParserInternal
{
////////////////////////////////////////
//! Internal: the state of a MSWParser
struct State {
  //! constructor
  State() : m_version(-1), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0) {
  }

  //! the file version
  int m_version;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MSWParser
class SubDocument : public IMWAWSubDocument
{
public:
  SubDocument(MSWParser &pars, TMWAWInputStreamPtr input, int id) :
    IMWAWSubDocument(&pars, input, IMWAWEntry()), m_id(id) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(IMWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(IMWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(IMWAWContentListenerPtr &listener, DMWAWSubDocumentType type);

protected:
  //! the subdocument file position
  int m_id;
};

void SubDocument::parse(IMWAWContentListenerPtr &listener, DMWAWSubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  MSWContentListener *listen = dynamic_cast<MSWContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
    return;
  }

  assert(m_parser);

  long pos = m_input->tell();
  //  reinterpret_cast<MSWParser *>(m_parser)->send(m_id);
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(IMWAWSubDocument const &doc) const
{
  if (IMWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_id != sDoc->m_id) return true;
  return false;
}
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MSWParser::MSWParser(TMWAWInputStreamPtr input, IMWAWHeader * header) :
  IMWAWParser(input, header), m_listener(), m_convertissor(), m_state(),
  m_pageSpan(), m_listSubDocuments(),
  m_asciiFile(), m_asciiName("")
{
  init();
}

MSWParser::~MSWParser()
{
  if (m_listener.get()) m_listener->endDocument();
}

void MSWParser::init()
{
  m_convertissor.reset(new MWAWTools::Convertissor);
  m_listener.reset();
  m_asciiName = "main-1";

  m_state.reset(new MSWParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  m_pageSpan.setMarginTop(0.1);
  m_pageSpan.setMarginBottom(0.1);
  m_pageSpan.setMarginLeft(0.1);
  m_pageSpan.setMarginRight(0.1);
}

void MSWParser::setListener(MSWContentListenerPtr listen)
{
  m_listener = listen;
}

int MSWParser::version() const
{
  return m_state->m_version;
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
float MSWParser::pageHeight() const
{
  return m_pageSpan.getFormLength()-m_pageSpan.getMarginTop()-m_pageSpan.getMarginBottom()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0;
}

float MSWParser::pageWidth() const
{
  return m_pageSpan.getFormWidth()-m_pageSpan.getMarginLeft()-m_pageSpan.getMarginRight();
}

////////////////////////////////////////////////////////////
// new page and color
////////////////////////////////////////////////////////////
void MSWParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!m_listener || m_state->m_actPage == 1)
      continue;
    m_listener->insertBreak(DMWAW_PAGE_BREAK);
  }
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MSWParser::parse(WPXDocumentInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw_libwpd::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    ascii().addPos(getInput()->tell());
    ascii().addNote("_");

    ok = createZones();
    if (ok) {
      createDocument(docInterface);
    }

    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("MSWParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  if (!ok) throw(libmwaw_libwpd::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MSWParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (m_listener) {
    MWAW_DEBUG_MSG(("MSWParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  std::list<DMWAWPageSpan> pageList;
  DMWAWPageSpan ps(m_pageSpan);

  int numPage = 1;
  m_state->m_numPages = numPage;

  for (int i = 0; i <= m_state->m_numPages; i++) pageList.push_back(ps);

  //
  MSWContentListenerPtr listen =
    MSWContentListener::create(pageList, documentInterface, m_convertissor);
  setListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// try to find the different zone
////////////////////////////////////////////////////////////
bool MSWParser::createZones()
{
  return false;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MSWParser::checkHeader(IMWAWHeader *header, bool strict)
{
  *m_state = MSWParserInternal::State();

  TMWAWInputStreamPtr input = getInput();

  libmwaw_tools::DebugStream f;
  int headerSize=80;
  input->seek(headerSize,WPX_SEEK_SET);
  if (int(input->tell()) != headerSize) {
    MWAW_DEBUG_MSG(("MSWParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0, WPX_SEEK_SET);
  if (input->readULong(4) != 0xfe370023L)
    return false;

  int val = input->readLong(2);
  if (strict && val > 3)
    return false;
  f << "FileHeader: f0=" << val << ",";

  // ok, we can finish initialization
  if (header) {
    header->setMajorVersion(m_state->m_version);
    header->setType(IMWAWDocument::MSWORD);
  }

  //
  input->seek(headerSize, WPX_SEEK_SET);

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());
  ascii().addPos(headerSize);

  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool MSWParser::readPrintInfo()
{
  TMWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw_tools::DebugStream f;
  // print info
  libmwaw_tools_mac::PrintInfo info;
  if (!info.read(input)) return false;
  f << "Entries(PrintInfo):"<< info;

  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  // define margin from print info
  Vec2i lTopMargin= -1 * info.paper().pos(0);
  Vec2i rBotMargin=info.paper().size() - info.page().size();

  // move margin left | top
  int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
  int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
  lTopMargin -= Vec2i(decalX, decalY);
  rBotMargin += Vec2i(decalX, decalY);

  // decrease right | bottom
  int rightMarg = rBotMargin.x() -50;
  if (rightMarg < 0) rightMarg=0;
  int botMarg = rBotMargin.y() -50;
  if (botMarg < 0) botMarg=0;

  m_pageSpan.setMarginTop(lTopMargin.y()/72.0);
  m_pageSpan.setMarginBottom(botMarg/72.0);
  m_pageSpan.setMarginLeft(lTopMargin.x()/72.0);
  m_pageSpan.setMarginRight(rightMarg/72.0);
  m_pageSpan.setFormLength(paperSize.y()/72.);
  m_pageSpan.setFormWidth(paperSize.x()/72.);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+0x78, WPX_SEEK_SET);
  if (long(input->tell()) != pos+0x78) {
    MWAW_DEBUG_MSG(("MSWParser::readPrintInfo: file is too short\n"));
    return false;
  }
  ascii().addPos(input->tell());

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
