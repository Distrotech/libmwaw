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

/* Inspired of TN-012-Disk-Based-MW-Format.txt */

#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include <libwpd/WPXString.h>


#include "MWAWContentListener.hxx"
#include "MWAWHeader.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPrinter.hxx"

#include "MSKGraph.hxx"
#include "MSKText.hxx"

#include "MSKParser.hxx"

/** Internal: the structures of a MSKParser */
namespace MSKParserInternal
{
//! Internal: a zone of a MSKParser ( main, header, footer )
struct Zone {
  //! the different type
  enum Type { MAIN, HEADER, FOOTER, NONE };
  //! the constructor
  Zone(Type type=NONE, int zoneId=-1) : m_type(type), m_zoneId(zoneId), m_textId(-1) {}
  //! the zone type
  Type m_type;
  //! the parser zone id
  int m_zoneId;
  //! the text internal id
  int m_textId;
};

////////////////////////////////////////
//! Internal: the state of a MSKParser
struct State {
  //! constructor
  State() : m_version(-1), m_docType(MWAWDocument::K_TEXT), m_zoneMap(), m_eof(-1), m_actPage(0), m_numPages(0),
    m_headerText(""), m_footerText(""), m_hasHeader(false), m_hasFooter(false),
    m_headerHeight(0), m_footerHeight(0) {
  }

  //! return a zone
  Zone get(Zone::Type type) {
    Zone res;
    if (m_zoneMap.find(int(type)) != m_zoneMap.end())
      res = m_zoneMap[int(type)];
    return res;
  }
  //! returns true if this is a text document (hack for MSWorks 4.0 Draw)
  bool IsTextDoc() const {
    return m_docType == MWAWDocument::K_TEXT;
  }
  //! the file version
  int m_version;
  //! the type of document
  MWAWDocument::DocumentKind m_docType;
  //! the list of zone
  std::map<int, Zone> m_zoneMap;
  //! the last known file position
  long m_eof;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  std::string m_headerText /**header string v1-2*/, m_footerText /**footer string v1-2*/;
  bool m_hasHeader /** true if there is a header v3*/, m_hasFooter /** true if there is a footer v3*/;
  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MSKParser
class SubDocument : public MWAWSubDocument
{
public:
  enum Type { Zone, Graphic, Text };
  SubDocument(MSKParser &pars, MWAWInputStreamPtr input, Type type,
              int zoneId, int noteId=-1) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_type(type), m_id(zoneId), m_noteId(noteId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! returns the subdocument \a id
  int getId() const {
    return m_id;
  }
  //! sets the subdocument \a id
  void setId(int vid) {
    m_id = vid;
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  /** the type */
  Type m_type;
  /** the subdocument id*/
  int m_id;
  /** the note id */
  int m_noteId;
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  MSKContentListener *listen = dynamic_cast<MSKContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
    return;
  }
  assert(m_parser);

  long pos = m_input->tell();
  MSKParser *parser = reinterpret_cast<MSKParser *>(m_parser);
  switch(m_type) {
  case Text:
    parser->sendText(m_id, m_noteId);
    break;
  case Graphic:
    parser->sendGraphic(m_id);
    break;
  case Zone:
    parser->sendZone(m_id);
    break;
  }
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_type != sDoc->m_type) return true;
  if (m_noteId != sDoc->m_noteId) return true;
  return false;
}
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MSKParser::MSKParser(MWAWInputStreamPtr input, MWAWHeader *header) :
  MWAWParser(input, header), m_listener(), m_convertissor(), m_state(),
  m_pageSpan(), m_listZones(), m_graphParser(), m_textParser(), m_asciiFile(), m_asciiName("")
{
  init();
}

MSKParser::~MSKParser()
{
  if (m_listener.get()) m_listener->endDocument();
}

void MSKParser::init()
{
  m_convertissor.reset(new MWAWFontConverter);
  m_listener.reset();
  m_asciiName = "main-1";

  m_state.reset(new MSKParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  m_pageSpan.setMarginTop(0.1);
  m_pageSpan.setMarginBottom(0.1);
  m_pageSpan.setMarginLeft(0.1);
  m_pageSpan.setMarginRight(0.1);

  m_graphParser.reset(new MSKGraph(getInput(), *this, m_convertissor));
  m_textParser.reset(new MSKText(getInput(), *this, m_convertissor));
}

void MSKParser::setListener(MSKContentListenerPtr listen)
{
  m_listener = listen;
  m_graphParser->setListener(listen);
  m_textParser->setListener(listen);
}

int MSKParser::version() const
{
  return m_state->m_version;
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
float MSKParser::pageHeight() const
{
  return m_pageSpan.getFormLength()-m_pageSpan.getMarginTop()-m_pageSpan.getMarginBottom()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0;
}

float MSKParser::pageWidth() const
{
  return m_pageSpan.getFormWidth()-m_pageSpan.getMarginLeft()-m_pageSpan.getMarginRight();
}

Vec2f MSKParser::getPageTopLeft() const
{
  return Vec2f(m_pageSpan.getMarginLeft(),
               m_pageSpan.getMarginTop()+m_state->m_headerHeight/72.0);
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void MSKParser::newPage(int number, bool softBreak)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!m_listener || m_state->m_actPage == 1)
      continue;
    if (softBreak)
      m_listener->insertBreak(MWAW_SOFT_PAGE_BREAK);
    else
      m_listener->insertBreak(MWAW_PAGE_BREAK);
  }
}

bool MSKParser::getColor(int id, Vec3uc &col) const
{
  switch(version()) {
  case 2:
    switch(id) {
    case 1:
      col=Vec3uc(0,0,0);
      return true;
    case 2:
      col=Vec3uc(255,255,255);
      return true;
    case 3:
      col=Vec3uc(255,0,0);
      return true;
    case 4:
      col=Vec3uc(0,255,0);
      return true;
    case 5:
      col=Vec3uc(0,0,255);
      return true;
    case 6:
      col=Vec3uc(0,255,255);
      return true;
    case 7:
      col=Vec3uc(255,0,255);
      return true;
    case 8:
      col=Vec3uc(255,255,0);
      return true;
    default:
      break;
    }
    break;
  case 3: {
    static std::vector<Vec3uc> palette;
    if (palette.size()==0) {
      palette.resize(256);
      int ind=0;
      for (int k = 0; k < 6; k++) {
        for (int j = 0; j < 6; j++) {
          for (int i = 0; i < 6; i++, ind++) {
            if (j==5 && i==2) break;
            palette[ind]=Vec3uc(255-51*i, 255-51*k, 255-51*j);
          }
        }
      }

      // the last 2 lines
      for (int r = 0; r < 2; r++) {
        // the black, red, green, blue zone of 5*2
        for (int c = 0; c < 4; c++) {
          for (int i = 0; i < 5; i++, ind++) {
            int val = 17*r+51*i;
            if (c == 0) {
              palette[ind]=Vec3uc(val, val, val);
              continue;
            }
            int col[3]= {0,0,0};
            col[c-1]=val;
            palette[ind]=Vec3uc(col[0],col[1],col[2]);
          }
        }
        // last part of j==5, i=2..5
        for (int k = r; k < 6; k+=2) {
          for (int i = 2; i < 6; i++, ind++) palette[ind]=Vec3uc(255-51*i, 255-51*k, 255-51*5);
        }
      }
    }
    if (id >= 0 && id <= 255) {
      col=palette[id];
      return true;
    }
    break;
  }
  default:
    break;
  }
  static bool first = true;
  if (first) {
    MWAW_DEBUG_MSG(("MSWParser::getColor: unknown color=%d\n", id));
    first = false;
  }
  return false;
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MSKParser::parse(WPXDocumentInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      sendZone(MSKParserInternal::Zone::MAIN);
      m_textParser->flushExtra();
      m_graphParser->flushExtra();
      if (m_listener) m_listener->endDocument();
      m_listener.reset();
    }
    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("MSKParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  if (!ok) throw(libmwaw::ParseException());
}

bool MSKParser::checkIfPositionValid(long pos)
{
  if (pos <= m_state->m_eof)
    return true;
  MWAWInputStreamPtr input = getInput();
  long actPos = input->tell();
  input->seek(pos, WPX_SEEK_SET);
  bool ok = long(input->tell())==pos;
  if (ok) m_state->m_eof = pos;

  input->seek(actPos, WPX_SEEK_SET);
  return ok;
}

void MSKParser::sendText(int id, int noteId)
{
  if (noteId < 0)
    m_textParser->sendZone(id);
  else
    m_textParser->sendNote(id, noteId);
}

void MSKParser::sendGraphic(int id)
{
  m_graphParser->send(id, MWAWPosition::Char);
}

void MSKParser::sendZone(int zoneType)
{
  if (!m_listener) return;
  MSKParserInternal::Zone zone=m_state->get(MSKParserInternal::Zone::Type(zoneType));
  if (zone.m_zoneId >= 0)
    m_graphParser->sendAll(zone.m_zoneId, zoneType==MSKParserInternal::Zone::MAIN);
  if (zone.m_textId >= 0)
    m_textParser->sendZone(zone.m_textId);
}

bool MSKParser::sendFootNote(int zoneId, int noteId)
{
  MWAWSubDocumentPtr subdoc
  (new MSKParserInternal::SubDocument(*this, getInput(), MSKParserInternal::SubDocument::Text, zoneId, noteId));
  if (m_listener)
    m_listener->insertNote(MWAWContentListener::FOOTNOTE, subdoc);
  return true;
}

bool  MSKParser::sendTextBox(int id, MWAWPosition const &pos, WPXPropertyList &extras)
{
  shared_ptr<MSKParserInternal::SubDocument> subdoc
  (new MSKParserInternal::SubDocument(*this, getInput(), MSKParserInternal::SubDocument::Graphic, id));
  if (m_listener)
    m_listener->insertTextBox(pos, subdoc, extras);
  return true;
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MSKParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (m_listener) {
    MWAW_DEBUG_MSG(("MSKParser::createDocument: listener already exist\n"));
    return;
  }

  int vers = version();

  MSKParserInternal::Zone mainZone=m_state->get(MSKParserInternal::Zone::MAIN);
  // update the page
  int numPage = 1;
  if (mainZone.m_textId >= 0 && m_textParser->numPages(mainZone.m_textId) > numPage)
    numPage = m_textParser->numPages(mainZone.m_textId);
  if (mainZone.m_zoneId >= 0 && m_graphParser->numPages(mainZone.m_zoneId) > numPage)
    numPage = m_graphParser->numPages(mainZone.m_zoneId);
  m_state->m_numPages = numPage;
  m_state->m_actPage = 0;

  // create the page list
  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan ps(m_pageSpan);
  int id = m_textParser->getHeader();
  if (id >= 0) {
    if (vers <= 2) m_state->m_headerHeight = 12;
    shared_ptr<MWAWSubDocument> subdoc
    (new MSKParserInternal::SubDocument
     (*this, getInput(), MSKParserInternal::SubDocument::Text, id));
    ps.setHeaderFooter(MWAWPageSpan::HEADER, MWAWPageSpan::ALL, subdoc);
  } else if (m_state->get(MSKParserInternal::Zone::HEADER).m_zoneId >= 0) {
    shared_ptr<MWAWSubDocument> subdoc
    (new MSKParserInternal::SubDocument
     (*this, getInput(), MSKParserInternal::SubDocument::Zone, int(MSKParserInternal::Zone::HEADER)));
    ps.setHeaderFooter(MWAWPageSpan::HEADER, MWAWPageSpan::ALL, subdoc);
  }
  id = m_textParser->getFooter();
  if (id >= 0) {
    if (vers <= 2) m_state->m_footerHeight = 12;
    shared_ptr<MWAWSubDocument> subdoc
    (new MSKParserInternal::SubDocument
     (*this, getInput(), MSKParserInternal::SubDocument::Text, id));
    ps.setHeaderFooter(MWAWPageSpan::FOOTER, MWAWPageSpan::ALL, subdoc);
  } else if (m_state->get(MSKParserInternal::Zone::FOOTER).m_zoneId >= 0) {
    shared_ptr<MWAWSubDocument> subdoc
    (new MSKParserInternal::SubDocument
     (*this, getInput(), MSKParserInternal::SubDocument::Zone, int(MSKParserInternal::Zone::FOOTER)));
    ps.setHeaderFooter(MWAWPageSpan::FOOTER, MWAWPageSpan::ALL, subdoc);
  }

  for (int i = 0; i <= m_state->m_numPages; i++) pageList.push_back(ps);

  //
  MSKContentListenerPtr listen(new MSKContentListener(pageList, documentInterface));
  setListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool MSKParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();

  if (version()>=3) {
    bool ok = true;
    if (m_state->m_hasHeader)
      ok = readGroupHeaderInfo(true,99);
    if (ok) pos = input->tell();
    else input->seek(pos, WPX_SEEK_SET);
    if (ok && m_state->m_hasFooter)
      ok = readGroupHeaderInfo(false,99);
    if (ok) pos = input->tell();
    else input->seek(pos, WPX_SEEK_SET);
  }

  MSKParserInternal::Zone::Type const type = MSKParserInternal::Zone::MAIN;
  MSKParserInternal::Zone newZone(type, m_state->m_zoneMap.size());
  m_state->m_zoneMap.insert(std::map<int,MSKParserInternal::Zone>::value_type(int(type),newZone));
  MSKParserInternal::Zone &mainZone = m_state->m_zoneMap.find(int(type))->second;
  while (!input->atEOS()) {
    pos = input->tell();
    if (!readZone(mainZone)) {
      input->seek(pos, WPX_SEEK_SET);
      break;
    }
  }

  mainZone.m_textId = m_textParser->createZones(-1, true);

  pos = input->tell();

  if (!input->atEOS())
    ascii().addPos(input->tell());
  ascii().addNote("Entries(End)");
  ascii().addPos(pos+100);
  ascii().addNote("_");

  // ok, prepare the data
  m_state->m_numPages = 1;
  std::vector<int> linesH, pagesH;
  if (m_textParser->getLinesPagesHeight(mainZone.m_textId, linesH, pagesH))
    m_graphParser->computePositions(mainZone.m_zoneId, linesH, pagesH);

  return true;
}


////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read a generic zone
////////////////////////////////////////////////////////////
bool MSKParser::readZone(MSKParserInternal::Zone &zone)
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();

  MWAWEntry pict;
  int val = input->readLong(1);
  input->seek(-1, WPX_SEEK_CUR);
  switch(val) {
  case 0: {
    MWAWEntry pict;
    int pictId = m_graphParser->getEntryPicture(zone.m_zoneId, pict);
    if (pictId >= 0) {
      input->seek(pict.end(), WPX_SEEK_SET);
      return true;
    }
    break;
  }
  case 1: {
    MWAWEntry pict;
    int pictId = m_graphParser->getEntryPictureV1(zone.m_zoneId, pict);
    if (pictId >= 0) {
      input->seek(pict.end(), WPX_SEEK_SET);
      return true;
    }
    break;
  }
  case 2:
    if (readDocumentInfo())
      return true;
    break;
  case 3: {
    MWAWEntry group;
    if (readGroup(zone, group, 2))
      return true;
    break;
  }
  default:
    break;
  }

  input->seek(pos, WPX_SEEK_SET);
  return false;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MSKParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MSKParserInternal::State();
  MWAWInputStreamPtr input = getInput();

  int numError = 0, val;

  const int headerSize = 0x20;

  libmwaw::DebugStream f;

  input->seek(0,WPX_SEEK_SET);

  m_state->m_hasHeader = m_state->m_hasFooter = false;
  int vers = input->readULong(4);
  switch (vers) {
  case 11:
#ifndef DEBUG
    return false;
#else
    m_state->m_version = 4;
    break; // no-text Works4 file have classic header
#endif
  case 9:
    m_state->m_version = 3;
    break;
  case 8:
    m_state->m_version = 2;
    break;
  case 4:
    m_state->m_version = 1;
    break;
  default:
    if (strict) return false;

    MWAW_DEBUG_MSG(("MSKParser::checkHeader: find unknown version 0x%x\n", vers));
    // must we stop in this case, or can we continue ?
    if (vers < 0 || vers > 14) {
      MWAW_DEBUG_MSG(("MSKParser::checkHeader: version too big, we stop\n"));
      return false;
    }
    m_state->m_version = (vers < 4) ? 1 : (vers < 8) ? 2 : (vers < 11) ? 3 : 4;
  }
  if (input->seek(headerSize,WPX_SEEK_SET) != 0 || input->atEOS())
    return false;

  if (input->seek(12,WPX_SEEK_SET) != 0) return false;

  for (int i = 0; i < 3; i++) {
    val = input->readLong(1);
    if (val < -10 || val > 10) {
      MWAW_DEBUG_MSG(("MSKParser::checkHeader: find odd val%d=0x%x: not implemented\n", i, val));
      numError++;
    }
  }
  input->seek(1,WPX_SEEK_CUR);
  int type = input->readLong(2);
  switch (type) {
    // Text document
  case 1:
    break;
  case 2:
    m_state->m_docType = MWAWDocument::K_DATABASE;
    break;
  case 3:
    m_state->m_docType = MWAWDocument::K_SPREADSHEET;
    break;
  case 12:
    m_state->m_docType = MWAWDocument::K_DRAW;
    break;
  default:
    MWAW_DEBUG_MSG(("MSKParser::checkHeader: find odd type=%d: not implemented\n", type));
    return false;
  }

#ifndef DEBUG
  // I have never seen this file, so...
  if (strict && m_state->m_version == 1 && m_state->m_docType != MWAWDocument::K_TEXT)
    return false;

  if (m_state->m_docType != MWAWDocument::K_TEXT)
    return false;

  if (m_state->m_version < 1 || m_state->m_version > 3)
    return false;
#endif

  // ok, we can finish initialization
  MWAWEntry headerZone;
  headerZone.setBegin(0);
  headerZone.setEnd(headerSize);
  headerZone.setType("FileHeader");
  m_listZones.push_back(headerZone);

  //
  input->seek(0,WPX_SEEK_SET);
  f << "FileHeader: ";
  f << "version= " << input->readULong(4);
  long dim[4];
  for (int i = 0; i < 4; i++) dim[i] = input->readLong(2);
  if (dim[2] <= dim[0] || dim[3] <= dim[1]) {
    MWAW_DEBUG_MSG(("MSKParser::checkHeader: find odd bdbox\n"));
    numError++;
  }
  f << ", windowdbdbox?=(";
  for (int i = 0; i < 4; i++) f << dim[i]<<",";
  f << "),";
  for (int i = 0; i < 4; i++) {
    val = input->readULong(1);
    if (!val) continue;
    f << "##v" << i << "=" << std::hex << val <<",";
  }
  type = input->readULong(2);
  f << std::dec;
  switch(type) {
  case 1:
    f << "doc,";
    break;
  case 2:
    f << "database,";
    break; // with ##v3=50
  case 3:
    f << "spreadsheet,";
    break; // with ##v2=5,##v3=6c
  case 12:
    f << "draw,";
    break;
  default:
    f << "###type=" << type << ",";
    break;
  }
  f << "numlines?=" << input->readLong(2) << ",";
  val = input->readLong(1); // 0, v2: 0, 4 or -4
  if (val)  f << "f0=" << val << ",";
  val = input->readLong(1); // almost always 1
  if (val != 1) f << "f1=" << val << ",";
  for (int i = 11; i < headerSize/2; i++) { // v1: 0, 0, v2: 0, 0|1
    int val = input->readULong(2);
    if (!val) continue;
    f << "f" << i << "=" << std::hex << val << std::dec;
    if ( m_state->IsTextDoc() && version() >= 3 && i == 12) {
      if (val & 0x100) {
        m_state->m_hasHeader = true;
        f << "(Head)";
      }
      if (val & 0x200) {
        m_state->m_hasFooter = true;
        f << "(Foot)";
      }
    }
    f << ",";
  }

  if (header)
    header->reset(MWAWDocument::MSWORKS, m_state->m_version,
                  m_state->m_docType);

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());
  ascii().addPos(headerSize);

  input->seek(headerSize,WPX_SEEK_SET);
  return strict ? (numError==0) : (numError < 3);
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool MSKParser::readDocumentInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;

  if (input->readLong(1) != 2)
    return false;

  int vers = version();
  int docId = input->readULong(1);
  int docExtra = input->readULong(1);
  int flag = input->readULong(1);
  long sz = input->readULong(2);
  long endPos = pos+6+sz;
  if (!checkIfPositionValid(endPos))
    return false;

  int expectedSz = vers<=2 ? 0x15e : 0x9a;
  if (sz < expectedSz) {
    if (sz < 0x78+8) {
      MWAW_DEBUG_MSG(("MSKParser::readDocumentInfo: size is too short\n"));
      return false;
    }
    MWAW_DEBUG_MSG(("MSKParser::readDocumentInfo: size is too short: try to continue\n"));
  }

  f << "Entries(DocInfo):";
  if (docId) f << "id=0x"<< std::hex << docId << ",";
  if (docExtra) f << "unk=" << docExtra << ","; // in v3: find 3, 7, 1x
  if (flag) f << "fl=" << flag << ","; // in v3: find 80, 84, e0
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  if (!readPrintInfo()) {
    input->seek(endPos, WPX_SEEK_SET);
    return true;
  }

  pos = input->tell();
  if (sz < 0x9a) {
    input->seek(endPos, WPX_SEEK_SET);
    return true;
  }
  pos = input->tell();
  f.str("");
  f << "DocInfo-1:";
  int val = input->readLong(2);
  if ((val & 0x0400) && vers >= 3) {
    f << "titlepage,";
    val &= 0xFBFF;
  }
  if (val) f << "unkn=" << val << ",";
  if (vers <= 2) {
    for (int wh = 0; wh < 2; wh++) {
      long debPos = input->tell();
      std::string name(wh==0 ? "header" : "footer");
      std::string text = m_textParser->readHeaderFooterString(wh==0);
      if (text.size()) f << name << "="<< text << ",";

      int remain = debPos+100 - input->tell();
      for (int i = 0; i < remain; i++) {
        unsigned char c = input->readULong(1);
        if (c == 0) continue;
        f << std::dec << "f"<< i << "=" << (int) c << ",";
      }
    }
    f << "defFid=" << input->readULong(2) << ",";
    f << "defFsz=" << input->readULong(2)/2 << ",";
    val = input->readULong(2); // 0 or 8
    if (val) f << "#unkn=" << val << ",";
    int dim[2];
    for (int i = 0; i < 2; i++) dim[i] = input->readULong(2);
    f << "dim=" << dim[0] << "x" << dim[1] << ",";
    /* followed by 0 (v1) or 0|0x21|0* (v2)*/
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    pos = input->tell();
    f.str("");
    f << "DocInfo-2:";
  }

  // last data ( normally 26)
  int numData = (endPos - input->tell())/2;
  for (int i = 0; i < numData; i++) {
    val = input->readLong(2);
    switch(i) {
    case 2:
      if (val!=1) f << "firstPageNumber=" << val << ",";
      break;
    case 3:
      if (val!=1) f << "firstNoteNumber=" << val << ",";
      break;
    default:
      if (val)
        f << "g" << i << "=" << val << ",";
      break;
    }
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(endPos, WPX_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
// read a basic zone info
////////////////////////////////////////////////////////////
bool MSKParser::readGroup(MSKParserInternal::Zone &zone, MWAWEntry &entry, int check)
{
  entry = MWAWEntry();
  MWAWInputStreamPtr input=getInput();
  if (input->atEOS()) return false;

  long pos = input->tell();
  if (input->readULong(1) != 3) return false;

  libmwaw::DebugStream f;
  int docId = input->readULong(1);
  int docExtra = input->readULong(1);
  int flag = input->readULong(1);
  long size = input->readULong(2)+6;

  int blockSize = version() <= 2 ? 340 : 360;
  if (size < blockSize) return false;

  f << "Entries(GroupHeader):";
  if (docId) f << "id=0x"<< std::hex << docId << std::dec << ",";
  if (docExtra) f << "unk=" << docExtra << ",";
  if (flag) f << "fl=" << flag << ",";
  if (size != blockSize)
    f << "end=" << std::hex << pos+size << std::dec << ",";

  entry.setBegin(pos);
  entry.setLength(size);
  entry.setType("GroupHeader");

  if (!checkIfPositionValid(entry.end())) {
    if (!checkIfPositionValid(pos+blockSize)) {
      MWAW_DEBUG_MSG(("MSKParser::readGroup: can not determine group %d size \n", docId));
      return false;
    }
    entry.setLength(blockSize);
  }

  if (check <= 0) return true;
  input->seek(pos+8, WPX_SEEK_SET);
  for (int i = 0; i < 52; i++) {
    int v = input->readLong(2);
    if (i < 8 && (v < -100 || v > 100)) return false;
    if (v) {
      f << "f" << i << "=";
      if (v > 0 && v < 1000)
        f << v;
      else
        f << std::hex << "X" << v << std::dec;
      f << ",";
    }
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = pos+blockSize;
  input->seek(pos, WPX_SEEK_SET);
  int N=input->readLong(2);

  f.str("");
  f << "GroupHeader:N=" << N << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

#if 0
  if (check < 99) return true;
  if (m_state->m_docId >= 0 && docId != m_state->m_docId)
    MWAW_DEBUG_MSG(("MSKParser::readGroup: find a different zone id 0x%x: not implemented\n", docId));
  else
    m_state->m_docId = docId;
#endif
  MWAWEntry pictZone;
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    if (m_graphParser->getEntryPicture(zone.m_zoneId, pictZone) >= 0)
      continue;
    MWAW_DEBUG_MSG(("MSKParser::readGroup: can not find the end of group \n"));
    input->seek(pos, WPX_SEEK_SET);
    break;
  }
  if (input->tell() < entry.end()) {
    ascii().addPos(input->tell());
    ascii().addNote("Entries(GroupData)");
    input->seek(entry.end(), WPX_SEEK_SET);
  }

  return true;
}

////////////////////////////////////////////////////////////
// read a header/footer zone info
////////////////////////////////////////////////////////////
bool MSKParser::readGroupHeaderInfo(bool header, int check)
{
  if (version() < 3) return false;

  MWAWInputStreamPtr input=getInput();
  long debPos = input->tell();

  long ptr = input->readULong(2);
  if (input->atEOS()) return false;
  if (ptr) {
    if (check == 49) return false;
    if (check == 99) {
      MWAW_DEBUG_MSG(("MSKParser::readGroupHeaderInfo: find ptr=0x%lx\n", ptr));
    }
  }

  libmwaw::DebugStream f;

  int size = input->readLong(2)+4;
  int realSize = 0x11;
  if (size < realSize) return false;
  if (input->readLong(2) != 0) return false;
  f << "Entries(GroupHInfo)";
  if (header)
    f << "[header]";
  else
    f << "[footer]";
  f << ": size=" << std::hex << size << std::dec << " BTXT";

  if (!checkIfPositionValid(debPos+size)) return false;

  input->seek(debPos+6, WPX_SEEK_SET);
  int N=input->readLong(2);
  f << ", N=" << N;
  int dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = input->readLong(2);

  Box2i box(Vec2i(dim[1], dim[0]), Vec2i(dim[3], dim[2]));
  if (box.size().x() < -2000 || box.size().y() < -2000 ||
      box.size().x() > 2000 || box.size().y() > 2000 ||
      box.min().x() < -200 || box.min().y() < -200) return false;
  if (check == 49 && box.size().x() == 0 &&  box.size().y() == 0) return false;
  f << ", BDBox =" << box;
  int val = input->readULong(1);
  if (val) f << ", flag=" << val;

  input->seek(debPos+size, WPX_SEEK_SET);
  if (check < 99) return true;
  if (header) m_state->m_headerHeight = box.size().y();
  else m_state->m_footerHeight = box.size().y();
  MSKParserInternal::Zone::Type type=
    header ? MSKParserInternal::Zone::HEADER : MSKParserInternal::Zone::FOOTER;
  MSKParserInternal::Zone zone(type, m_state->m_zoneMap.size());

  ascii().addPos(debPos);
  ascii().addNote(f.str().c_str());
  ascii().addPos(input->tell());

  input->seek(debPos+realSize, WPX_SEEK_SET);
  input->pushLimit(debPos+size);
  bool limitSet = true;
  for (int i = 0; i < N; i++) {
    long pos = input->tell();
    if (limitSet && pos==debPos+size) {
      limitSet = false;
      input->popLimit();
    }
    if (readZone(zone)) continue;
    input->seek(pos, WPX_SEEK_SET);
    zone.m_textId = m_textParser->createZones(N-i, false);
    if (zone.m_textId >= 0)
      break;
    MWAW_DEBUG_MSG(("MSKParser::readGroupHeaderInfo: can not find end of group\n"));
    input->seek(pos, WPX_SEEK_SET);
  }
  if (limitSet) input->popLimit();
  if (long(input->tell()) < debPos+size) {
    ascii().addPos(input->tell());
    ascii().addNote("GroupHInfo-II");

    input->seek(debPos+size, WPX_SEEK_SET);

    ascii().addPos(debPos + size);
    ascii().addNote("_");
  }
  //  m_graphParser->addDeltaToPositions(zone.m_zoneId, -1*box[0]);
  if (m_state->m_zoneMap.find(int(type)) != m_state->m_zoneMap.end()) {
    MWAW_DEBUG_MSG(("MSKParser::readGroupHeaderInfo: the zone already exists\n"));
  } else
    m_state->m_zoneMap.insert(std::map<int,MSKParserInternal::Zone>::value_type(int(type),zone));
  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool MSKParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;
  // print info
  libmwaw::PrinterInfo info;
  if (!checkIfPositionValid(pos+0x78+8) || !info.read(input)) return false;
  f << "Entries(PrintInfo):"<< info;

  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  // now read the margin
  int margin[4];
  int maxSize = paperSize.x() > paperSize.y() ? paperSize.x() : paperSize.y();
  f << ", margin=(";
  for (int i = 0; i < 4; i++) {
    margin[i] = int(72./120.*input->readLong(2));
    if (margin[i] < -maxSize || margin[i] > maxSize) return false;
    f << margin[i];
    if (i != 3) f << ", ";
  }
  f << ")";


  // define margin from print info
  Vec2i lTopMargin(margin[0],margin[1]), rBotMargin(margin[2],margin[3]);
  lTopMargin += paperSize - pageSize;

  int leftMargin = lTopMargin.x();
  int topMargin = lTopMargin.y();

  // decrease right | bottom
  int rightMarg = rBotMargin.x() -50;
  if (rightMarg < 0) {
    leftMargin -= (-rightMarg);
    if (leftMargin < 0) leftMargin=0;
    rightMarg=0;
  }
  int botMarg = rBotMargin.y() -50;
  if (botMarg < 0) {
    topMargin -= (-botMarg);
    if (topMargin < 0) topMargin=0;
    botMarg=0;
  }

  m_pageSpan.setMarginTop(topMargin/72.0);
  m_pageSpan.setMarginBottom(botMarg/72.0);
  m_pageSpan.setMarginLeft(leftMargin/72.0);
  m_pageSpan.setMarginRight(rightMarg/72.0);
  m_pageSpan.setFormLength(paperSize.y()/72.);
  m_pageSpan.setFormWidth(paperSize.x()/72.);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+0x78+8, WPX_SEEK_SET);

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
