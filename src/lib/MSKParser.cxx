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

#include "TMWAWPosition.hxx"
#include "TMWAWPictMac.hxx"
#include "TMWAWPrint.hxx"

#include "IMWAWHeader.hxx"

#include "MWAWStruct.hxx"
#include "MWAWTools.hxx"
#include "MWAWContentListener.hxx"

#include "MSKGraph.hxx"
#include "MSKText.hxx"

#include "MSKParser.hxx"

/** Internal: the structures of a MSKParser */
namespace MSKParserInternal
{
////////////////////////////////////////
//! Internal: the state of a MSKParser
struct State {
  //! constructor
  State() : m_version(-1), m_docType(IMWAWDocument::K_TEXT), m_eof(-1), m_actPage(0), m_numPages(0),
    m_headerText(""), m_footerText(""), m_hasHeader(false), m_hasFooter(false),
    m_headerHeight(0), m_footerHeight(0) {
  }

  //! returns true if this is a text document (hack for MSWorks 4.0 Draw)
  bool IsTextDoc() const {
    return m_docType == IMWAWDocument::K_TEXT;
  }
  //! the file version
  int m_version;
  //! the type of document
  IMWAWDocument::DocumentKind m_docType;
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
class SubDocument : public IMWAWSubDocument
{
public:
  SubDocument(MSKParser &pars, TMWAWInputStreamPtr input, int zoneId) :
    IMWAWSubDocument(&pars, input, IMWAWEntry()), m_id(zoneId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(IMWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(IMWAWSubDocument const &doc) const {
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
  void parse(IMWAWContentListenerPtr &listener, DMWAWSubDocumentType type);

protected:
  /** the subdocument id

  \note m_id >= 0: correspond to the text parser data, while m_id < 0
  corresponds to a graph parser zone (ie. a textbox) */
  int m_id;
};

void SubDocument::parse(IMWAWContentListenerPtr &listener, DMWAWSubDocumentType /*type*/)
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
  reinterpret_cast<MSKParser *>(m_parser)->send(m_id);
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
MSKParser::MSKParser(TMWAWInputStreamPtr input, IMWAWHeader * header) :
  IMWAWParser(input, header), m_listener(), m_convertissor(), m_state(),
  m_pageSpan(), m_listZones(), m_graphParser(), m_textParser(), m_listSubDocuments(), m_asciiFile(), m_asciiName("")
{
  init();
}

MSKParser::~MSKParser()
{
  if (m_listener.get()) m_listener->endDocument();
}

void MSKParser::init()
{
  m_convertissor.reset(new MWAWTools::Convertissor);
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
      m_listener->insertBreak(DMWAW_SOFT_PAGE_BREAK);
    else
      m_listener->insertBreak(DMWAW_PAGE_BREAK);
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

  if (!checkHeader(0L))  throw(libmwaw_libwpd::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      m_textParser->sendZone(0);
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

  if (!ok) throw(libmwaw_libwpd::ParseException());
}

bool MSKParser::checkIfPositionValid(long pos)
{
  if (pos <= m_state->m_eof)
    return true;
  TMWAWInputStreamPtr input = getInput();
  long actPos = input->tell();
  input->seek(pos, WPX_SEEK_SET);
  bool ok = long(input->tell())==pos;
  if (ok) m_state->m_eof = pos;

  input->seek(actPos, WPX_SEEK_SET);
  return ok;
}

void MSKParser::send(int id)
{
  if (id < 0) {
    MWAW_DEBUG_MSG(("MSKParser::send: do not know how to send %d zone\n", id));
  } else
    m_textParser->sendZone(id);
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

  // update the page
  int numPage = 1;
  if (m_textParser->numPages() > numPage)
    numPage = m_textParser->numPages();
  if (m_graphParser->numPages() > numPage)
    numPage = m_graphParser->numPages();
  m_state->m_numPages = numPage;
  m_state->m_actPage = 0;

  // create the page list
  std::list<DMWAWPageSpan> pageList;
  DMWAWPageSpan ps(m_pageSpan);
  int id = m_textParser->getHeader();
  if (id >= 0) {
    DMWAWTableList tableList;
    shared_ptr<MSKParserInternal::SubDocument> subdoc
    (new MSKParserInternal::SubDocument(*this, getInput(), id));
    m_listSubDocuments.push_back(subdoc);
    ps.setHeaderFooter(HEADER, 0, ALL, subdoc.get(), tableList);
  }

  id = m_textParser->getFooter();
  if (id >= 0) {
    DMWAWTableList tableList;
    shared_ptr<MSKParserInternal::SubDocument> subdoc
    (new MSKParserInternal::SubDocument(*this, getInput(), id));
    m_listSubDocuments.push_back(subdoc);
    ps.setHeaderFooter(FOOTER, 0, ALL, subdoc.get(), tableList);
  }

  for (int i = 0; i <= m_state->m_numPages; i++) pageList.push_back(ps);

  //
  MSKContentListenerPtr listen =
    MSKContentListener::create(pageList, documentInterface, m_convertissor);
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
  TMWAWInputStreamPtr input = getInput();
  long pos = input->tell();

  IMWAWEntry pict;
  while (!input->atEOS()) {
    pos = input->tell();
    int val = input->readLong(1);
    input->seek(-1, WPX_SEEK_CUR);
    bool ok = false;
    switch(val) {
    case 0: {
      IMWAWEntry pict;
      int pictId = m_graphParser->getEntryPicture(pict);
      if ((ok=(pictId >= 0)))
        input->seek(pict.end(), WPX_SEEK_SET);
      break;
    }
    case 1: {
      IMWAWEntry pict;
      int pictId = m_graphParser->getEntryPictureV1(pict);
      if ((ok=(pictId >= 0)))
        input->seek(pict.end(), WPX_SEEK_SET);
      break;
    }
    case 2:
      ok = readDocumentInfo();
      break;
    case 3: {
      IMWAWEntry group;
      ok = readGroup(group, 2);
      break;
    }
    default:
      break;
    }
    if (ok) continue;
    input->seek(pos, WPX_SEEK_SET);
    break;
  }

  pos = input->tell();
  bool ok = m_textParser->createZones();
  if (ok)
    pos = input->tell();
  input->seek(pos, WPX_SEEK_SET);

  if (!input->atEOS()) {
    ascii().addPos(pos);
    ascii().addNote("Entries(End)");
    ascii().addPos(pos+100);
    ascii().addNote("_");
  }

  // ok, we can find calculate the number of pages and the header and the footer height
  m_state->m_numPages = 1;

  return ok;
}


////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MSKParser::checkHeader(IMWAWHeader *header, bool strict)
{
  *m_state = MSKParserInternal::State();
  TMWAWInputStreamPtr input = getInput();

  int numError = 0, val;

  const int headerSize = 0x20;

  libmwaw_tools::DebugStream f;

  input->seek(0,WPX_SEEK_SET);

  m_state->m_hasHeader = m_state->m_hasFooter = false;
  int vers = input->readULong(4);
  switch (vers) {
  case 11:
    m_state->m_version = 4;
    break; // no-text Works4 file have classic header
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
    m_state->m_docType = IMWAWDocument::K_DATABASE;
    break;
  case 3:
    m_state->m_docType = IMWAWDocument::K_SPREADSHEET;
    break;
  case 12:
    m_state->m_docType = IMWAWDocument::K_DRAW;
    break;
  default:
    MWAW_DEBUG_MSG(("MSKParser::checkHeader: find odd type=%d: not implemented\n", type));
    return false;
  }

#ifndef DEBUG
  // I have never seen this file, so...
  if (strict && m_state->m_version == 1 && m_state->m_docType != IMWAWDocument::K_TEXT)
    return false;

  if (m_state->m_docType != IMWAWDocument::K_TEXT)
    return false;
#endif

  // ok, we can finish initialization
  IMWAWEntry headerZone;
  headerZone.setBegin(0);
  headerZone.setEnd(headerSize);
  headerZone.setType("FileHeader");
  m_listZones.push_back(headerZone);

  if (header) {
    header->setMajorVersion(m_state->m_version);
    header->setKind(m_state->m_docType);
  }

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
  TMWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw_tools::DebugStream f;

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
    if (val) f << "g" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(endPos, WPX_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool MSKParser::readGroup(IMWAWEntry &zone, int check)
{
  zone = IMWAWEntry();
  TMWAWInputStreamPtr input=getInput();
  if (input->atEOS()) return false;

  long pos = input->tell();
  if (input->readULong(1) != 3) return false;

  libmwaw_tools::DebugStream f;
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

  zone.setBegin(pos);
  zone.setLength(size);
  zone.setType("GroupHeader");

  if (!checkIfPositionValid(zone.end())) {
    if (!checkIfPositionValid(pos+blockSize)) {
      MWAW_DEBUG_MSG(("MSKParser::readGroup: can not determine group %d size \n", docId));
      return false;
    }
    zone.setLength(blockSize);
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
    MWAW_DEBUG_MSG(("MSKParser::getNextEntryGroup: find a different zone id 0x%x: not implemented\n", docId));
  else
    m_state->m_docId = docId;
#endif
  IMWAWEntry pictZone;
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    if (m_graphParser->getEntryPicture(pictZone) >= 0)
      continue;
    MWAW_DEBUG_MSG(("MSKParser::getNextEntryGroup: can not find the end of group \n"));
    input->seek(pos, WPX_SEEK_SET);
    break;
  }
  if (input->tell() < zone.end()) {
    ascii().addPos(input->tell());
    ascii().addNote("Entries(GroupData)");
    input->seek(zone.end(), WPX_SEEK_SET);
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool MSKParser::readPrintInfo()
{
  TMWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw_tools::DebugStream f;
  // print info
  libmwaw_tools_mac::PrintInfo info;
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
