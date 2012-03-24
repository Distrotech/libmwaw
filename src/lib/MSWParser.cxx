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
#include <set>
#include <sstream>

#include <libwpd/WPXString.h>

#include "TMWAWPosition.hxx"
#include "TMWAWPictMac.hxx"
#include "TMWAWPrint.hxx"

#include "IMWAWHeader.hxx"

#include "MWAWStruct.hxx"
#include "MWAWTools.hxx"
#include "MWAWContentListener.hxx"

#include "MSWText.hxx"

#include "MSWParser.hxx"

/** Internal: the structures of a MSWParser */
namespace MSWParserInternal
{
////////////////////////////////////////
//! Internal: the object of MSWParser
struct Object {
  Object() : m_textPos(-1), m_pos(), m_name(""), m_id(-1), m_extra("") {
    for (int i = 0; i < 2; i++) {
      m_ids[i] = -1;
      m_idsFlag[i] = 0;
    }
    for (int i = 0; i < 2; i++) m_flags[i] = 0;
  }

  MSWEntry getEntry() const {
    MSWEntry res;
    res.setBegin(m_pos.begin());
    res.setEnd(m_pos.end());
    res.setType("ObjectData");
    res.m_id = m_id;
    return res;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Object const &obj) {
    if (obj.m_textPos >= 0)
      o << std::hex << "textPos?=" << obj.m_textPos << std::dec << ",";
    if (obj.m_id >= 0) o << "Obj" << obj.m_id << ",";
    if (obj.m_name.length()) o << obj.m_name << ",";
    for (int st = 0; st < 2; st++) {
      if (obj.m_ids[st] == -1 && obj.m_idsFlag[st] == 0) continue;
      o << "id" << st << "=" << obj.m_ids[st];
      if (obj.m_idsFlag[st]) o << ":" << std::hex << obj.m_idsFlag[st] << std::dec << ",";
    }
    for (int st = 0; st < 2; st++) {
      if (obj.m_flags[st])
        o << "fl" << st << "=" << std::hex << obj.m_flags[st] << std::dec << ",";
    }

    if (obj.m_extra.length()) o << "extras=[" << obj.m_extra << "],";
    return o;
  }
  //! the text position
  long m_textPos;

  //! the object entry
  IMWAWEntry m_pos;

  //! the object name
  std::string m_name;

  //! the id
  int m_id;

  //! some others id?
  int m_ids[2];

  //! some flags link to m_ids
  int m_idsFlag[2];

  //! some flags
  int m_flags[2];

  //! some extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the state of a MSWParser
struct State {
  //! constructor
  State() : m_version(-1), m_eof(-1), m_bot(-1), m_eot(-1),
    m_textLength(0), m_footnoteLength(0), m_headerfooterLength(0),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0) {
  }

  //! returns the total text size
  long getTotalTextSize() const {
    return m_textLength + m_footnoteLength + m_headerfooterLength;
  }

  //! the file version
  int m_version;

  //! end of file
  long m_eof;
  //! the begin of the text
  long m_bot;
  //! end of the text
  long m_eot;
  //! the total textlength
  long m_textLength;
  //! the size of the footnote data
  long m_footnoteLength;
  //! the size of the header/footer data
  long m_headerfooterLength;

  //! the list of object ( mainZone, other zone)
  std::vector<Object> m_objectList[2];

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;

};

////////////////////////////////////////
//! Internal: the subdocument of a MSWParser
class SubDocument : public IMWAWSubDocument
{
public:
  SubDocument(MSWParser &pars, TMWAWInputStreamPtr input, int id, DMWAWSubDocumentType type) :
    IMWAWSubDocument(&pars, input, IMWAWEntry()), m_id(id), m_type(type) {}

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
  //! the subdocument id
  int m_id;
  //! the subdocument type
  DMWAWSubDocumentType m_type;
};

void SubDocument::parse(IMWAWContentListenerPtr &listener, DMWAWSubDocumentType type)
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
  reinterpret_cast<MSWParser *>(m_parser)->send(m_id, type);
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(IMWAWSubDocument const &doc) const
{
  if (IMWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_type != sDoc->m_type) return true;
  return false;
}
}

////////////////////////////////////////////////////////////
// MSWEntry
////////////////////////////////////////////////////////////
std::ostream &operator<<(std::ostream &o, MSWEntry const &entry)
{
  if (entry.type().length()) {
    o << entry.type();
    if (entry.m_id >= 0) o << "[" << entry.m_id << "]";
    o << "=";
  }
  return o;
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MSWParser::MSWParser(TMWAWInputStreamPtr input, IMWAWHeader * header) :
  IMWAWParser(input, header), m_listener(), m_convertissor(), m_state(),
  m_entryMap(), m_pageSpan(), m_textParser(), m_listSubDocuments(),
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

  m_textParser.reset(new MSWText(getInput(), *this, m_convertissor));
}

void MSWParser::setListener(MSWContentListenerPtr listen)
{
  m_listener = listen;
  m_textParser->setListener(listen);
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

bool MSWParser::getColor(int id, Vec3uc &col) const
{
  switch(id) {
  case 0:
    col=Vec3uc(0,0,0);
    break; // black
  case 1:
    col=Vec3uc(0,0,255);
    break; // blue
  case 2:
    col=Vec3uc(0, 255,255);
    break; // cyan
  case 3:
    col=Vec3uc(0,255,0);
    break; // green
  case 4:
    col=Vec3uc(255,0,255);
    break; // magenta
  case 5:
    col=Vec3uc(255,0,0);
    break; // red
  case 6:
    col=Vec3uc(255,255,0);
    break; // yellow
  case 7:
    col=Vec3uc(255,255,255);
    break; // white
  default:
    MWAW_DEBUG_MSG(("MSWParser::getColor: unknown color=%d\n", id));
    return false;
  }
  return true;
}

bool MSWParser::isFilePos(long pos)
{
  if (pos <= m_state->m_eof)
    return true;

  TMWAWInputStreamPtr input = getInput();
  long actPos = input->tell();
  input->seek(pos, WPX_SEEK_SET);
  bool ok = long(input->tell()) == pos;
  if (ok) m_state->m_eof = pos;
  input->seek(actPos, WPX_SEEK_SET);
  return ok;
}

void MSWParser::sendFootnote(int id)
{
  if (!m_listener) return;

  IMWAWSubDocumentPtr subdoc(new MSWParserInternal::SubDocument(*this, getInput(), id, DMWAW_SUBDOCUMENT_NOTE));
  m_listener->insertNote(FOOTNOTE, subdoc);
}

void MSWParser::sendFieldComment(int id)
{
  if (!m_listener) return;

  IMWAWSubDocumentPtr subdoc(new MSWParserInternal::SubDocument(*this, getInput(), id, DMWAW_SUBDOCUMENT_COMMENT_ANNOTATION));
  m_listener->insertComment(subdoc);
}

void MSWParser::send(int id, DMWAWSubDocumentType type)
{
  switch(type) {
  case DMWAW_SUBDOCUMENT_COMMENT_ANNOTATION:
    m_textParser->sendFieldComment(id);
    break;
  case DMWAW_SUBDOCUMENT_NOTE:
    m_textParser->sendFootnote(id);
    break;
  default:
    MWAW_DEBUG_MSG(("MSWParser::send: find unexpected type\n"));
    break;
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
      m_textParser->sendMainText();

      m_textParser->flushExtra();
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
  if (m_textParser->numPages() > numPage)
    numPage = m_textParser->numPages();
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
  if (!readZoneList()) return false;
  TMWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (pos != m_state->m_bot) {
    ascii().addPos(pos);
    ascii().addNote("_");
  }
  libmwaw_tools::DebugStream f;
  ascii().addPos(m_state->m_eot);
  ascii().addNote("_");

  std::multimap<std::string, MSWEntry>::iterator it;
  it = m_entryMap.find("PrintInfo");
  if (it != m_entryMap.end())
    readPrintInfo(it->second);

  it = m_entryMap.find("DocSum");
  if (it != m_entryMap.end())
    readDocSum(it->second);

  it = m_entryMap.find("Printer");
  if (it != m_entryMap.end())
    readPrinter(it->second);

  long textLength[3] = { m_state->m_textLength,
                         m_state->m_footnoteLength,
                         m_state->m_headerfooterLength
                       };
  bool ok = m_textParser->createZones(m_state->m_bot, textLength);

  readObjects();

  it = m_entryMap.find("DocumentInfo");
  if (it != m_entryMap.end())
    readDocumentInfo(it->second);

  it = m_entryMap.find("Zone17");
  if (it != m_entryMap.end())
    readZone17(it->second);

  it = m_entryMap.find("Picture");
  while (it != m_entryMap.end()) {
    if (!it->second.hasType("Picture")) break;
    MSWEntry &entry=it++->second;
    readPicture(entry);
  }

  for (it=m_entryMap.begin(); it!=m_entryMap.end(); it++) {
    MSWEntry const &entry = it->second;
    if (entry.isParsed()) continue;
    ascii().addPos(entry.begin());
    f.str("");
    f << entry;
    ascii().addNote(f.str().c_str());
    ascii().addPos(entry.end());
    ascii().addNote("_");

  }
  return ok;
}

////////////////////////////////////////////////////////////
// read the zone list
////////////////////////////////////////////////////////////
bool MSWParser::readZoneList()
{
  TMWAWInputStreamPtr input = getInput();

  int numData = version() <= 3 ? 15: 20;
  std::stringstream s;
  for (int i = 0; i < numData; i++) {
    switch(i) {
      // the first two zone are often simillar : even/odd header/footer ?
    case 0:
      readEntry("Styles", 0);
      break; // checkme: size is often invalid
    case 1:
      readEntry("Styles", 1);
      break;
    case 2:
      readEntry("FootnotePos");
      break;
    case 3:
      readEntry("FootnoteDef");
      break;
    case 4:
      readEntry("Section");
      break;
    case 5:
      readEntry("PageBreak");
      break;
    case 6:
      readEntry("FieldName");
      break;
    case 7:
      readEntry("FieldPos");
      break; // size ?
    case 8:
      readEntry("HeaderFooter");
      break; // size
    case 9:
      readEntry("CharList", 0);
      break;
    case 10:
      readEntry("ParagList", 1);
      break;
    case 12:
      readEntry("FontIds");
      break;
    case 13:
      readEntry("PrintInfo");
      break;
    case 14:
      readEntry("LineInfo");
      break;
    case 15:
      readEntry("DocumentInfo");
      break;
    case 16:
      readEntry("Printer");
      break;
    case 18:
      readEntry("TextStruct");
      break;
    case 19:
      readEntry("FootnoteData");
      break;
    default:
      s.str("");
      s << "Zone" << i;
      if (i < 4) s << "_";
      readEntry(s.str());
      break;
    }
  }

  long pos = input->tell();
  libmwaw_tools::DebugStream f;
  f << "Entries(ListZoneData)[0]:";
  for (int i = 0; i < 2; i++) // two small int
    f << "f" << i << "=" << input->readLong(2) << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  if (version() <= 4) return true;

  // main
  readEntry("ObjectName",0);
  readEntry("FontNames");
  readEntry("ObjectList",0);
  readEntry("ObjectFlags",0);
  readEntry("DocSum",0);
  for (int i = 25; i < 31; i++) {
    /* check me: Zone25, Zone26, Zone27: also some object name, list, flags ? */
    // header/footer
    if (i==28) readEntry("ObjectName",1);
    else if (i==29) readEntry("ObjectList",1);
    else if (i==30) readEntry("ObjectFlags",1);
    else {
      s.str("");
      s << "Zone" << i;
      readEntry(s.str());
    }
  }

  pos = input->tell();
  f.str("");
  f << "ListZoneData[1]:";

  long val = input->readLong(2);
  if (val) f << "unkn=" << val << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  if (input->atEOS()) {
    MWAW_DEBUG_MSG(("MSWParser::readZoneList: can not read list zone\n"));
    return false;
  }
  return true;
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
  int headerSize=64;
  input->seek(headerSize,WPX_SEEK_SET);
  if (int(input->tell()) != headerSize) {
    MWAW_DEBUG_MSG(("MSWParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0, WPX_SEEK_SET);
  int val = input->readULong(2);
  switch (val) {
  case 0xfe34:
    switch (input->readULong(2)) {
    case 0x0:
      headerSize = 30;
      m_state->m_version = 3;
#ifndef DEBUG
      return false;
#else
      break;
#endif
    default:
      return false;
    }
    break;
  case 0xfe37:
    switch (input->readULong(2)) {
    case 0x1c:
      m_state->m_version = 4;
      break;
    case 0x23:
      m_state->m_version = 5;
      break;
    default:
      return false;
    }
    break;
  default:
    return false;
  }

  f << "FileHeader:";
  val = input->readLong(1);
  if (val) f << "f0=" << val << ",";
  for (int i = 1; i < 3; i++) { // always 0
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  val = input->readLong(2); // v4-v5: find 4, 8, c, 24, 2c
  if (val)
    f << "unkn=" << std::hex << val << std::dec << ",";
  val = input->readLong(1); // always 0 ?
  if (val) f << "f4=" << val << ",";
  val = input->readLong(2); // always 0x19: for version 4, 5
  if (val!=0x19) f << "f5=" << val << ",";

  if (version() <= 3) {
    val = input->readLong(2);
    if (val) f << "f6=" << val << ",";
    m_state->m_bot = 0x100;
    m_state->m_eot = input->readULong(2);
    m_state->m_textLength = m_state->m_eot-0x100;

    for (int i = 0; i < 6; i++) { // always 0?
      val = input->readLong(2);
      if (val) f << "h" << i << "=" << val << ",";
    }
    input->seek(headerSize, WPX_SEEK_SET);
    ascii().addPos(0);
    ascii().addNote(f.str().c_str());
    return true;
  }

  for (int i = 0; i < 6; i++) { // always 0 ?
    val = input->readLong(1);
    if (val) f << "g" << i << "=" << val << ",";
  }
  if (val) f << "g7=" << val << ","; // always 0 ?
  m_state->m_bot =  input->readULong(4);
  m_state->m_eot = input->readULong(4);

  if (m_state->m_bot > m_state->m_eot) {
    f << "#text:" << std::hex << m_state->m_bot << "<->" << m_state->m_eot << ",";
    if (0x100 <= m_state->m_eot) {
      MWAW_DEBUG_MSG(("MSWParser::checkHeader: problem with text position: reset begin to default\n"));
      m_state->m_bot = 0x100;
    } else {
      MWAW_DEBUG_MSG(("MSWParser::checkHeader: problem with text position: reset to empty\n"));
      m_state->m_bot = m_state->m_eot = 0x100;
    }
  }

  long endOfData = input->readULong(4);
  if (endOfData < 100) {
    MWAW_DEBUG_MSG(("MSWParser::checkHeader: end of file pos is too small\n"));
    return false;
  }

  long actPos = input->tell();
  input->seek(endOfData, WPX_SEEK_SET);
  if (long(input->tell()) != endOfData) {
    if (strict) return false;
    endOfData = input->tell();
    if (endOfData < m_state->m_eot) {
      MWAW_DEBUG_MSG(("MSWParser::checkHeader: file seems too short, break...\n"));
      return false;
    }
    MWAW_DEBUG_MSG(("MSWParser::checkHeader: file seems too short, continue...\n"));
  }
  m_state->m_eof = endOfData;
  ascii().addPos(endOfData);
  ascii().addNote("Entries(End)");
  input->seek(actPos, WPX_SEEK_SET);
  val = input->readLong(4); // always 0 ?
  if (val) f << "unkn2=" << val << ",";

  // seen to regroup main textZone + ?
  m_state->m_textLength= input->readULong(4);
  f << "textLength=" << std::hex << m_state->m_textLength << std::dec << ",";
  m_state->m_footnoteLength = input->readULong(4);
  if (m_state->m_footnoteLength)
    f << "footnoteLength=" << std::hex << m_state->m_footnoteLength << std::dec << ",";
  m_state->m_headerfooterLength  = input->readULong(4);
  if (m_state->m_headerfooterLength)
    f << "headerFooterLength=" << std::hex << m_state->m_headerfooterLength << std::dec << ",";

  for (int i = 0; i < 8; i++) { // always 0 ?
    val = input->readLong(2);
    if (val) f << "h" << i << "=" << val << ",";
  }
  // ok, we can finish initialization
  if (header) {
    header->setMajorVersion(m_state->m_version);
    header->setType(IMWAWDocument::MSWORD);
  }

  if (long(input->tell()) != headerSize) {
    ascii().addDelimiter(input->tell(), '|');
    input->seek(headerSize, WPX_SEEK_SET);
  }

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// try to read an entry
////////////////////////////////////////////////////////////
MSWEntry MSWParser::readEntry(std::string type, int id)
{
  TMWAWInputStreamPtr input = getInput();
  MSWEntry entry;
  entry.setType(type);
  entry.m_id = id;
  long pos = input->tell();
  libmwaw_tools::DebugStream f;

  long debPos = input->readULong(4);
  long sz =  input->readULong(2);
  if (id >= 0) f << "Entries(" << type << ")[" << id << "]:";
  else f << "Entries(" << type << "):";
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return entry;
  }
  if (!isFilePos(debPos+sz)) {
    MWAW_DEBUG_MSG(("MSWParser::readEntry: problem reading entry: %s\n", type.c_str()));
    f << "#";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return entry;
  }

  entry.setBegin(debPos);
  entry.setLength(sz);
  m_entryMap.insert
  (std::multimap<std::string, MSWEntry>::value_type(type, entry));

  f << std::hex << debPos << "[" << sz << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return entry;
}

////////////////////////////////////////////////////////////
// read the document information
////////////////////////////////////////////////////////////
bool MSWParser::readDocumentInfo(MSWEntry &entry)
{
  if (entry.length() != 0x20) {
    MWAW_DEBUG_MSG(("MSWParser::readDocumentInfo: the zone size seems odd\n"));
    return false;
  }
  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "DocumentInfo:";

  float dim[2];
  for (int i = 0; i < 2; i++) dim[i] =  input->readLong(2)/1440.;
  f << "dim?=" << dim[1] << "x" << dim[0] << ",";

  float margin[4];
  f << ",marg=["; // top, left?, bottom, right?
  for (int i = 0; i < 4; i++) {
    margin[i] = input->readLong(2)/1440.;
    f << margin[i] << ",";
    if (margin[i] < 0.0) margin[i] *= -1.0;
  }
  f << "],";

  if (dim[0] > margin[0]+margin[2] && dim[1] > margin[1]+margin[3]) {
    m_pageSpan.setMarginTop(margin[0]);
    m_pageSpan.setMarginLeft(margin[1]);
    /* decrease a little the right/bottom margin to allow fonts discrepancy*/
    m_pageSpan.setMarginBottom((margin[2]< 0.5) ? 0.0 : margin[2]-0.5);
    m_pageSpan.setMarginRight((margin[3]< 0.5) ? 0.0 : margin[3]-0.5);

    m_pageSpan.setFormLength(dim[0]);
    m_pageSpan.setFormWidth(dim[1]);
  } else {
    MWAW_DEBUG_MSG(("MSWParser::readDocumentInfo: the page dimensions seems odd\n"));
  }

  int val = input->readLong(2); // always 0 ?
  if (val) f << "unkn=" << val << ",";
  val = input->readLong(2); // 0x2c5 or 0x2d0?
  f << "f0=" << val << ",";
  for (int i = 0; i < 4; i++) { //[a|12|40|42|4a|52|54|d2],0,0|80,1
    val = input->readULong(1);
    if (val) f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  val = input->readLong(2); // always 1 ?
  if (val != 1) f << "f1=" << val << ",";
  // a small number between 0 and 77
  f << "f2=" << input->readLong(2) << ",";
  for (int i = 0; i < 4; i++) { //[0|2|40|42|44|46|48|58],0|64,0|10|80,[0|2|5]
    val = input->readULong(1);
    if (val) f << "flA" << i << "=" << std::hex << val << std::dec << ",";
  }
  val = input->readLong(2); // always 0 ?
  if (val != 1) f << "f3=" << val << ",";
  val = input->readLong(2); // 0, 48, 50
  if (val) f << "f4=" << val << ",";

  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the zone 17
////////////////////////////////////////////////////////////
bool MSWParser::readZone17(MSWEntry &entry)
{
  if (entry.length() != 0x2a) {
    MWAW_DEBUG_MSG(("MSWParser::readZone17: the zone size seems odd\n"));
    return false;
  }
  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "Zone17:";
  if (version() < 5) {
    f << "bdbox?=[";
    for (int i = 0; i < 4; i++)
      f << input->readLong(2) << ",";
    f << "],";
    f << "bdbox2?=[";
    for (int i = 0; i < 4; i++)
      f << input->readLong(2) << ",";
    f << "],";
  }

  /*
    f0=0, 80, 82, 84, b0, b4, c2, c4, f0, f2 : type and ?
    f1=0|1|8|34|88 */
  int val;
  for (int i = 0; i < 2; i++) {
    val = input->readULong(1);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  // 0 or 1, followed by 0
  for (int i = 2; i < 4; i++) {
    val = input->readLong(1);
    if (val) f << "f" << i << "=" << val << ",";
  }
  long ptr = input->readULong(4); // a text ptr ( often near to textLength )
  if (ptr > m_state->m_textLength) f << "#";
  f << "textPos[sel?]=" << std::hex << ptr << std::dec << ",";
  val  = input->readULong(4); // almost always ptr
  if (val != ptr)
    f << "textPos1=" << std::hex << val << std::dec << ",";
  // a small int between 6 and b
  val = input->readLong(2);
  if (val) f << "f4=" << val << ",";

  for (int i = 5; i < 7; i++) { // 0,0 or 3,5 or 8000, 8000
    val = input->readULong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  val  = input->readULong(4); // almost always ptr
  if (val != ptr)
    f << "textPos2=" << std::hex << val << std::dec << ",";
  /* g0=[0,1,5,c], g1=[0,1,3,4] */
  for (int i = 0; i < 2; i++) {
    val = input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  if (version() == 5) {
    f << "bdbox?=[";
    for (int i = 0; i < 4; i++)
      f << input->readLong(2) << ",";
    f << "],";
    f << "bdbox2?=[";
    for (int i = 0; i < 4; i++)
      f << input->readLong(2) << ",";
    f << "],";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the printer name
////////////////////////////////////////////////////////////
bool MSWParser::readPrinter(MSWEntry &entry)
{
  if (entry.length() < 2) {
    MWAW_DEBUG_MSG(("MSWParser::readPrinter: the zone seems to short\n"));
    return false;
  }

  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "Printer:";
  int sz = input->readULong(2);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("MSWParser::readPrinter: the zone seems to short\n"));
    return false;
  }
  int strSz = input->readULong(1);
  if (strSz+2> sz) {
    MWAW_DEBUG_MSG(("MSWParser::readPrinter: name seems to big\n"));
    return false;
  }
  std::string name("");
  for (int i = 0; i < strSz; i++)
    name+=char(input->readLong(1));
  f << name << ",";
  int i= 0;
  while (long(input->tell())+2 <= entry.end()) { // almost always a,0,0
    int val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
    i++;
  }
  if (long(input->tell()) != entry.end())
    ascii().addDelimiter(input->tell(), '|');

  entry.setParsed(true);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the document summary
////////////////////////////////////////////////////////////
bool MSWParser::readDocSum(MSWEntry &entry)
{
  if (entry.length() < 8) {
    MWAW_DEBUG_MSG(("MSWParser::readDocSum: the zone seems to short\n"));
    return false;
  }

  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "DocSum:";
  int sz = input->readULong(2);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("MSWParser::readDocSum: the zone seems to short\n"));
    return false;
  }
  entry.setParsed(true);

  if (sz != entry.length()) f << "#";
  char const *(what[]) = { "title", "subject","author","version","keyword",
                           "author", "#unknown", "#unknown2"
                         };
  for (int i = 0; i < 8; i++) {
    long actPos = input->tell();
    if (actPos == entry.end()) break;

    int sz = input->readULong(1);
    if (sz == 0 || sz == 0xFF) continue;

    if (actPos+1+sz > entry.end()) {
      MWAW_DEBUG_MSG(("MSWParser::readDocSum: string %d to short...\n", i));
      f << "#";
      input->seek(actPos, WPX_SEEK_SET);
      break;
    }
    std::string s("");
    for (int j = 0; j < sz; j++) s += char(input->readULong(1));
    f << what[i] << "=" <<  s << ",";
  }

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  if (long(input->tell()) != entry.end())
    ascii().addDelimiter(input->tell(), '|');

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read  a list of strings zone
////////////////////////////////////////////////////////////
bool MSWParser::readStringsZone(MSWEntry &entry, std::vector<std::string> &list)
{
  list.resize(0);
  if (entry.length() < 2) {
    MWAW_DEBUG_MSG(("MSWParser::readStringsZone: the zone seems to short\n"));
    return false;
  }

  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << entry;
  int sz = input->readULong(2);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("MSWParser::readStringsZone: the zone seems to short\n"));
    return false;
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  int id = 0;
  while (long(input->tell()) != entry.end()) {
    pos = input->tell();
    int strSz = input->readULong(1);
    if (pos+strSz+1> entry.end()) {
      MWAW_DEBUG_MSG(("MSWParser::readStringsZone: a string seems to big\n"));
      f << "#";
      break;
    }
    std::string name("");
    for (int i = 0; i < strSz; i++)
      name+=char(input->readLong(1));
    list.push_back(name);
    f.str("");
    f << entry << "id" << id++ << "," << name << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  if (long(input->tell()) != entry.end()) {
    ascii().addPos(input->tell());
    f.str("");
    f << entry << "#";
    ascii().addNote(f.str().c_str());
  }

  entry.setParsed(true);

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the objects
////////////////////////////////////////////////////////////
bool MSWParser::readObjects()
{
  TMWAWInputStreamPtr input = getInput();

  std::multimap<std::string, MSWEntry>::iterator it;

  it = m_entryMap.find("ObjectList");
  while (it != m_entryMap.end()) {
    if (!it->second.hasType("ObjectList")) break;
    MSWEntry &entry=it++->second;
    readObjectList(entry);
  }

  it = m_entryMap.find("ObjectFlags");
  while (it != m_entryMap.end()) {
    if (!it->second.hasType("ObjectFlags")) break;
    MSWEntry &entry=it++->second;
    readObjectFlags(entry);
  }

  it = m_entryMap.find("ObjectName");
  while (it != m_entryMap.end()) {
    if (!it->second.hasType("ObjectName")) break;
    MSWEntry &entry=it++->second;
    std::vector<std::string> list;
    readStringsZone(entry, list);

    if (entry.m_id < 0 || entry.m_id > 1) {
      MWAW_DEBUG_MSG(("MSWParser::readObjects: unexpected entry id: %d\n", entry.m_id));
      continue;
    }
    std::vector<MSWParserInternal::Object> &listObject = m_state->m_objectList[entry.m_id];
    int numObjects = listObject.size();
    if (int(list.size()) != numObjects) {
      MWAW_DEBUG_MSG(("MSWParser::readObjects: unexpected number of name\n"));
      if (int(list.size()) < numObjects) numObjects = list.size();
    }
    for (int i = 0; i < numObjects; i++)
      listObject[i].m_name = list[i];
  }

  for (int st = 0; st < 2; st++) {
    std::vector<MSWParserInternal::Object> &listObject = m_state->m_objectList[st];

    for (int i = 0; i < int(listObject.size()); i++)
      readObject(listObject[i]);
  }
  return true;
}

bool MSWParser::readObjectList(MSWEntry &entry)
{
  if (entry.m_id < 0 || entry.m_id > 1) {
    MWAW_DEBUG_MSG(("MSWParser::readObjectList: unexpected entry id: %d\n", entry.m_id));
    return false;
  }
  std::vector<MSWParserInternal::Object> &listObject = m_state->m_objectList[entry.m_id];
  listObject.resize(0);
  if (entry.length() < 4 || (entry.length()%18) != 4) {
    MWAW_DEBUG_MSG(("MSWParser::readObjectList: the zone size seems odd\n"));
    return false;
  }
  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "ObjectList[" << entry.m_id << "]:";
  int N=entry.length()/18;
  std::vector<long> textPos; // checkme
  textPos.resize(N+1);
  f << "[";
  for (int i = 0; i < N+1; i++) {
    textPos[i] = input->readULong(4);
    f << std::hex << textPos[i] << std::dec << ",";
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  int val;

  for (int i = 0; i < N; i++) {
    MSWParserInternal::Object object;
    object.m_textPos = textPos[i];
    pos = input->tell();
    f.str("");
    object.m_id = input->readLong(2);
    for (int st = 0; st < 2; st++) {
      object.m_ids[st] = input->readLong(2); // small number -1, 0, 2, 3, 4
      object.m_idsFlag[st] = input->readULong(1); // 0, 48 .
    }

    object.m_pos.setBegin(input->readULong(4));
    val = input->readLong(2); // always 0 ?
    if (val) f << "#f1=" << val << ",";
    object.m_extra = f.str();
    f.str("");
    f << "ObjectList-" << i << ":" << object;
    if (!isFilePos(object.m_pos.begin())) {
      MWAW_DEBUG_MSG(("MSWParser::readObjectList: pb with ptr\n"));
      f << "#ptr=" << std::hex << object.m_pos.begin() << std::dec << ",";
      object.m_pos.setBegin(0);
    }

    listObject.push_back(object);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;

}

bool MSWParser::readObjectFlags(MSWEntry &entry)
{
  if (entry.m_id < 0 || entry.m_id > 1) {
    MWAW_DEBUG_MSG(("MSWParser::readObjectFlags: unexpected entry id: %d\n", entry.m_id));
    return false;
  }
  std::vector<MSWParserInternal::Object> &listObject = m_state->m_objectList[entry.m_id];
  int numObject = listObject.size();
  if (entry.length() < 4 || (entry.length()%6) != 4) {
    MWAW_DEBUG_MSG(("MSWParser::readObjectFlags: the zone size seems odd\n"));
    return false;
  }
  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "ObjectFlags[" << entry.m_id << "]:";
  int N=entry.length()/6;
  if (N != numObject) {
    MWAW_DEBUG_MSG(("MSWParser::readObjectFlags: unexpected number of object\n"));
  }

  f << "[";
  for (int i = 0; i < N+1; i++) {
    long textPos = input->readULong(4);
    if (i < numObject && textPos != listObject[i].m_textPos && textPos != listObject[i].m_textPos+1)
      f << "#";
    f << std::hex << textPos << std::dec << ",";
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    int fl[2];
    for (int st = 0; st < 2; st++) fl[st] = input->readULong(1);
    f.str("");
    f << "ObjectFlags-" << i << ":";
    if (i < numObject) {
      for (int st = 0; st < 2; st++) listObject[i].m_flags[st] = fl[st];
      f << "Obj" << listObject[i].m_id << ",";
    }
    if (fl[0] != 0x48) f << "fl0="  << std::hex << fl[0] << std::dec << ",";
    if (fl[1]) f << "fl1="  << std::hex << fl[1] << std::dec << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;

}

bool MSWParser::readObject(MSWParserInternal::Object &obj)
{
  TMWAWInputStreamPtr input = getInput();
  libmwaw_tools::DebugStream f;

  long pos = obj.m_pos.begin();
  if (!pos) return false;

  input->seek(pos, WPX_SEEK_SET);
  int sz = input->readULong(4);

  f << "Entries(ObjectData):Obj" << obj.m_id << ",";
  if (!isFilePos(pos+sz) || sz < 6) {
    MWAW_DEBUG_MSG(("MSWParser::readObjects: pb finding object data sz\n"));
    f << "#";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }

  int fSz = input->readULong(2);
  if (fSz < 2 || fSz+4 > sz) {
    MWAW_DEBUG_MSG(("MSWParser::readObjects: pb reading the name\n"));
    f << "#";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  obj.m_pos.setLength(sz);
  MSWEntry fileEntry = obj.getEntry();
  fileEntry.setParsed(true);
  m_entryMap.insert
  (std::multimap<std::string, MSWEntry>::value_type
   (fileEntry.type(), fileEntry));

  long endPos = pos+4+fSz;
  std::string name(""); // first equation, second "" or Equation Word?
  while(long(input->tell()) != endPos) {
    int c = input->readULong(1);
    if (c == 0) {
      if (name.length()) f << name << ",";
      name = "";
      continue;
    }
    name += char(c);
  }
  if (name.length()) f << name << ",";

  pos = input->tell();
  endPos = obj.m_pos.end();
  int val;
  if (pos+2 <= endPos) {
    val = input->readLong(2);
    if (val) f << "#unkn=" << val <<",";
  }
  // Equation Word? : often contains not other data
  long dataSz = 0;
  if (pos+9 <= endPos) {
    for (int i = 0; i < 3; i++) { // always 0
      val = input->readLong(1);
      if (val) f << "f" << i << "=" << val <<",";
    }
    dataSz = input->readULong(4);
  }
  pos = input->tell();
  if (dataSz && pos+dataSz != endPos) f << "#";
  ascii().addPos(obj.m_pos.begin());
  ascii().addNote(f.str().c_str());
  if (long(input->tell()) != obj.m_pos.end())
    ascii().addDelimiter(input->tell(), '|');

  ascii().addPos(obj.m_pos.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// check if a zone is a picture/read a picture
////////////////////////////////////////////////////////////
bool MSWParser::checkPicturePos(long pos, int type)
{
  if (pos < 0x100 || !isFilePos(pos))
    return false;

  TMWAWInputStreamPtr input = getInput();
  input->seek(pos, WPX_SEEK_SET);
  long sz = input->readULong(4);
  long endPos = pos+sz;
  if (sz < 14 || !isFilePos(sz+pos)) return false;
  int num = input->readLong(1);
  if (num < 0 || num > 4) return false;
  input->seek(pos+14, WPX_SEEK_SET);
  for (int n = 0; n < num; n++) {
    long actPos = input->tell();
    long pSz = input->readULong(4);
    if (pSz+actPos > endPos) return false;
    input->seek(pSz+actPos, WPX_SEEK_SET);
  }
  if (input->tell() != endPos)
    return false;

  static int id = 0;
  MSWEntry entry;
  entry.setBegin(pos);
  entry.setEnd(endPos);
  entry.setType("Picture");
  entry.setTextId(type);
  entry.m_id = id++;
  m_entryMap.insert
  (std::multimap<std::string, MSWEntry>::value_type(entry.type(), entry));

  return true;
}

bool MSWParser::readPicture(MSWEntry &entry)
{
  if (entry.length() < 30 && entry.length() != 14) {
    MWAW_DEBUG_MSG(("MSWParser::readPicture: the zone seems too short\n"));
    return false;
  }
  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "Entries(Picture)[" << entry.textId() << "-" << entry.m_id << "]:";
  long sz = input->readULong(4);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("MSWParser::readPicture: the zone size seems too big\n"));
    return false;
  }
  int N = input->readULong(1);
  f << "N=" << N << ",";
  int val = input->readULong(1); // find 0 or 0x80
  if (val) f << "fl=" << std::hex << val << std::dec << ",";
  int dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = input->readLong(2);
  f << "dim=[" << dim[1] << "x" << dim[0] << "," << dim[3] << "x" << dim[2] << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int n=0; n < N; n++) {
    pos = input->tell();
    f.str("");
    f << "Picture-" << n << "[" << entry.textId() << "-" << entry.m_id << "]:";
    sz = input->readULong(4);
    if (sz < 16 || sz+pos > entry.end()) {
      MWAW_DEBUG_MSG(("MSWParser::readPicture: pb with the picture size\n"));
      f << "#";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return false;
    }
    val = input->readULong(1); // always 8?
    if (val) f << "type?=" << val << ",";
    val = input->readLong(1); // always 0 ?
    if (val) f << "unkn=" << val << ",";
    val = input->readLong(2); // almost always 1 : two time 0?
    if (val) f << "id?=" << val << ",";
    for (int i = 0; i < 4; i++)
      dim[i] = input->readLong(2);
    f << "dim=[" << dim[1] << "x" << dim[0] << "," << dim[3] << "x" << dim[2] << "],";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
#ifdef DEBUG_WITH_FILES
    if (sz > 16) {
      ascii().skipZone(pos+16, pos+sz-1);
      WPXBinaryData file;
      input->seek(pos+16, WPX_SEEK_SET);
      input->readDataBlock(sz-16, file);
      static int volatile pictName = 0;
      libmwaw_tools::DebugStream f;
      f << "PICT-" << ++pictName << ".pct";
      libmwaw_tools::Debug::dumpFile(file, f.str().c_str());
    }
#endif
    input->seek(pos+sz, WPX_SEEK_SET);
  }

  pos = input->tell();
  if (pos != entry.end())
    ascii().addDelimiter(pos, '|');
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool MSWParser::readPrintInfo(MSWEntry &entry)
{
  if (entry.length() < 0x78) {
    MWAW_DEBUG_MSG(("MSWParser::readPrintInfo: the zone seems to short\n"));
    return false;
  }
  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  // print info
  libmwaw_tools_mac::PrintInfo info;
  if (!info.read(input)) return false;
  f << "PrintInfo:"<< info;

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

  if (long(input->tell()) != entry.end())
    ascii().addDelimiter(input->tell(), '|');

  ascii().addPos(entry.end());
  ascii().addNote("_");

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
