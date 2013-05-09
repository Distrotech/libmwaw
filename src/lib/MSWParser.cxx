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
#include <map>
#include <set>
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSubDocument.hxx"

#include "MSWParser.hxx"

#include "MSWText.hxx"

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
    res.setId(m_id);
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
  MWAWEntry m_pos;

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
//! Internal: the picture of a MSWParser
struct Picture {
  struct Zone;
  Picture() : m_dim(), m_picturesList(), m_flag(0) {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Picture const &pict) {
    o << "dim=" << pict.m_dim << ",";
    if (pict.m_flag) o << "f0=" << std::hex << pict.m_flag << std::dec << ",";
    return o;
  }

  //! the dimension
  Box2i m_dim;
  //! the list of picture
  std::vector<Zone> m_picturesList;
  //! an unknown flag
  int m_flag;

  // ! a small zone
  struct Zone {
    Zone() : m_pos(), m_dim() {
      for (int i = 0; i < 3; i++) m_flags[i] = 0;
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Zone const &pict) {
      o << "dim=" << pict.m_dim << ",";
      if (pict.m_flags[0] != 8) o << "f0=" << pict.m_flags[0] << ",";
      if (pict.m_flags[1]) o << "f1=" << pict.m_flags[1] << ",";
      if (pict.m_flags[2] != 1) o << "f2=" << pict.m_flags[2] << ","; // or 0
      return o;
    }
    //! the position in file
    MWAWEntry m_pos;
    //! the dimension
    Box2i m_dim;
    //! three unknown flags
    int m_flags[3];
  };

};

////////////////////////////////////////
//! Internal: the state of a MSWParser
struct State {
  //! constructor
  State() : m_eof(-1), m_bot(-1), m_eot(-1), m_endNote(false),
    m_picturesMap(), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0),
    m_headersId(), m_footersId() {
  }

  //! end of file
  long m_eof;
  //! the begin of the text
  long m_bot;
  //! end of the text
  long m_eot;
  //! a flag to know if we must place the note at the end or in the foot part
  bool m_endNote;
  //! the map filePos -> Picture
  std::map<long, Picture> m_picturesMap;

  //! the list of object ( mainZone, other zone)
  std::vector<Object> m_objectList[2];

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
  /** the list of header id which corresponds to each page */
  std::vector<int> m_headersId;
  /** the list of footer id which corresponds to each page */
  std::vector<int> m_footersId;

};

////////////////////////////////////////
//! Internal: the subdocument of a MSWParser
class SubDocument : public MWAWSubDocument
{
public:
  //! constructor for footnote, comment
  SubDocument(MSWParser &pars, MWAWInputStreamPtr input, int id, libmwaw::SubDocumentType type) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_id(id), m_type(type), m_pictFPos(-1), m_pictCPos(-1)  {}
  //! constructor for header/footer
  SubDocument(MSWParser &pars, MWAWInputStreamPtr input, MWAWEntry entry, libmwaw::SubDocumentType type) :
    MWAWSubDocument(&pars, input, entry), m_id(-1), m_type(type), m_pictFPos(-1), m_pictCPos(-1)  {}
  //! constructor for picture
  SubDocument(MSWParser &pars, MWAWInputStreamPtr input, long fPos, int cPos) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_id(-1), m_type(libmwaw::DOC_NONE), m_pictFPos(fPos), m_pictCPos(cPos) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  //! the subdocument id
  int m_id;
  //! the subdocument type
  libmwaw::SubDocumentType m_type;
  //! the picture file position
  long m_pictFPos;
  //! the picture char position
  int m_pictCPos;
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_parser);

  long pos = m_input->tell();
  if (m_type == libmwaw::DOC_NONE && m_pictCPos >= 0 && m_pictFPos > 0)
    reinterpret_cast<MSWParser *>(m_parser)->sendPicture(m_pictFPos, m_pictCPos, MWAWPosition::Frame);
  else if (m_type == libmwaw::DOC_HEADER_FOOTER)
    reinterpret_cast<MSWParser *>(m_parser)->send(m_zone);
  else
    reinterpret_cast<MSWParser *>(m_parser)->send(m_id, type);
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_type != sDoc->m_type) return true;
  if (m_pictFPos != sDoc->m_pictFPos) return true;
  if (m_pictCPos != sDoc->m_pictCPos) return true;
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
MSWParser::MSWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_state(), m_entryMap(), m_textParser()
{
  init();
}

MSWParser::~MSWParser()
{
}

void MSWParser::init()
{
  resetListener();
  setAsciiName("main-1");

  m_state.reset(new MSWParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);

  m_textParser.reset(new MSWText(*this));
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
    if (!getListener() || m_state->m_actPage == 1)
      continue;
    getListener()->insertBreak(MWAWContentListener::PageBreak);
  }
}

bool MSWParser::getColor(int id, MWAWColor &col) const
{
  switch(id) {
  case 0:
    col=MWAWColor(0,0,0);
    break; // black
  case 1:
    col=MWAWColor(0,0,255);
    break; // blue
  case 2:
    col=MWAWColor(0, 255,255);
    break; // cyan
  case 3:
    col=MWAWColor(0,255,0);
    break; // green
  case 4:
    col=MWAWColor(255,0,255);
    break; // magenta
  case 5:
    col=MWAWColor(255,0,0);
    break; // red
  case 6:
    col=MWAWColor(255,255,0);
    break; // yellow
  case 7:
    col=MWAWColor(255,255,255);
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

  MWAWInputStreamPtr input = getInput();
  long actPos = input->tell();
  input->seek(pos, WPX_SEEK_SET);
  bool ok = long(input->tell()) == pos;
  if (ok) m_state->m_eof = pos;
  input->seek(actPos, WPX_SEEK_SET);
  return ok;
}

void MSWParser::sendFootnote(int id)
{
  if (!getListener()) return;

  MWAWSubDocumentPtr subdoc(new MSWParserInternal::SubDocument(*this, getInput(), id, libmwaw::DOC_NOTE));
  getListener()->insertNote
  (MWAWNote(m_state->m_endNote ? MWAWNote::EndNote : MWAWNote::FootNote), subdoc);
}

void MSWParser::sendFieldComment(int id)
{
  if (!getListener()) return;

  MWAWSubDocumentPtr subdoc(new MSWParserInternal::SubDocument(*this, getInput(), id, libmwaw::DOC_COMMENT_ANNOTATION));
  getListener()->insertComment(subdoc);
}

void MSWParser::send(MWAWEntry const &entry)
{
  m_textParser->sendText(entry, false);
}

void MSWParser::send(int id, libmwaw::SubDocumentType type)
{
  switch(type) {
  case libmwaw::DOC_COMMENT_ANNOTATION:
    m_textParser->sendFieldComment(id);
    break;
  case libmwaw::DOC_NOTE:
    m_textParser->sendFootnote(id);
    break;
  case libmwaw::DOC_NONE:
  case libmwaw::DOC_HEADER_FOOTER:
  case libmwaw::DOC_TABLE:
  case libmwaw::DOC_TEXT_BOX:
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

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
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

  resetListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MSWParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getListener()) {
    MWAW_DEBUG_MSG(("MSWParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  MWAWPageSpan ps(getPageSpan());
  MWAWEntry entry = m_textParser->getHeader();
  if (entry.valid()) {
    MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
    header.m_subDocument.reset
    (new MSWParserInternal::SubDocument(*this, getInput(), entry, libmwaw::DOC_HEADER_FOOTER));
    ps.setHeaderFooter(header);
  }
  entry = m_textParser->getFooter();
  if (entry.valid()) {
    MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    footer.m_subDocument.reset
    (new MSWParserInternal::SubDocument(*this, getInput(), entry, libmwaw::DOC_HEADER_FOOTER));
    ps.setHeaderFooter(footer);
  }
  int numPage = 1;
  if (m_textParser->numPages() > numPage)
    numPage = m_textParser->numPages();
  m_state->m_numPages = numPage;

  ps.setPageSpan(m_state->m_numPages+1);
  std::vector<MWAWPageSpan> pageList(1,ps);
  //
  MWAWContentListenerPtr listen(new MWAWContentListener(*getParserState(), pageList, documentInterface));
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
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (pos != m_state->m_bot) {
    ascii().addPos(pos);
    ascii().addNote("_");
  }
  libmwaw::DebugStream f;
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

  readObjects();

  bool ok = m_textParser->createZones(m_state->m_bot);

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

  for (it=m_entryMap.begin(); it!=m_entryMap.end(); ++it) {
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
  MWAWInputStreamPtr input = getInput();
  int const vers = version();
  getInput()->seek(vers <= 3 ? 30 : 64, WPX_SEEK_SET);
  int numData = vers <= 3 ? 15: 20;
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
      break;
    case 8:
      readEntry("HeaderFooter");
      break;
    case 9:
      readEntry("CharList", 0);
      break;
    case 10:
      readEntry("ParagList", 1);
      break;
    case 12:
      readEntry("FontIds");
      break;
    case 13: // checkme: is it ok also for v3 file ?
      readEntry("PrintInfo");
      break;
    case 14:
      readEntry(vers <= 3 ? "TextStruct" : "ParaInfo");
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

  if (vers <= 3) return true;
  long pos = input->tell();
  libmwaw::DebugStream f;
  f << "Entries(ListZoneData)[0]:";
  for (int i = 0; i < 2; i++) // two small int
    f << "f" << i << "=" << input->readLong(2) << ",";

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  if (vers <= 4) return true;

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
bool MSWParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MSWParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;

  libmwaw::DebugStream f;
  int headerSize=64;
  if (!isFilePos(0x88)) {
    MWAW_DEBUG_MSG(("MSWParser::checkHeader: file is too short\n"));
    return false;
  }
  long pos = 0;
  input->seek(pos, WPX_SEEK_SET);
  int val = (int) input->readULong(2);
  switch (val) {
  case 0xfe34:
    switch (input->readULong(2)) {
    case 0x0:
      headerSize = 30;
      setVersion(3);
      break;
    default:
      return false;
    }
    break;
  case 0xfe37:
    switch (input->readULong(2)) {
    case 0x1c:
      setVersion(4);
      break;
    case 0x23:
      setVersion(5);
      break;
    default:
      return false;
    }
    break;
  default:
    return false;
  }

  int const vers = version();
  f << "FileHeader:";
  val = (int) input->readULong(1); // v1: ab other 0 ?
  if (val) f << "f0=" << val << ",";
  for (int i = 1; i < 3; i++) { // always 0
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  if (vers > 3) {
    // find 4, 8, c, 24, 2c
    val = (int) input->readLong(2);
    if (val)
      f << "unkn=" << std::hex << val << std::dec << ",";
    // 0,0,0x19,0
    for (int i = 4; i < 8; i++) {
      val = (int) input->readLong(1);
      if (val) f << "f" << i << "=" << val << ",";
    }
  }

  for (int i = 0; i < 5; i++) { // always 0 ?
    val = (int) input->readLong(1);
    if (val) f << "g" << i << "=" << val << ",";
  }

  m_state->m_bot = vers <= 3 ? 0x100 : (long) input->readULong(4);
  m_state->m_eot = (long) input->readULong(4);
  f << "text=" << std::hex << m_state->m_bot << "<->" << m_state->m_eot << ",";
  if (m_state->m_bot > m_state->m_eot) {
    f << "#text,";
    if (0x100 <= m_state->m_eot) {
      MWAW_DEBUG_MSG(("MSWParser::checkHeader: problem with text position: reset begin to default\n"));
      m_state->m_bot = 0x100;
    } else {
      MWAW_DEBUG_MSG(("MSWParser::checkHeader: problem with text position: reset to empty\n"));
      m_state->m_bot = m_state->m_eot = 0x100;
    }
  }

  if (vers <= 3) { // always 0
    for (int i = 0; i < 6; i++) {
      val = (int) input->readLong(2);
      if (val) f << "h" << i << "=" << val << ",";
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    if (!readHeaderEndV3())
      return false;
    if (header)
      header->reset(MWAWDocument::MSWORD, vers);
    return true;
  }

  long endOfData = (long) input->readULong(4);
  f << "eof=" << std::hex << endOfData << std::dec << ",";
  if (endOfData < 100 || !isFilePos(endOfData)) {
    MWAW_DEBUG_MSG(("MSWParser::checkHeader: end of file pos is too small\n"));
    if (endOfData < m_state->m_eot || strict)
      return false;
    f << "#endOfData,";
  }
  ascii().addPos(endOfData);
  ascii().addNote("Entries(End)");

  val = (int) input->readLong(4); // always 0 ?
  if (val) f << "unkn2=" << val << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  if (!m_textParser->readHeaderTextLength())
    return false;

  pos = input->tell();
  f.str("");
  f << "FileHeader[A]:";
  for (int i = 0; i < 8; i++) {
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }

  // ok, we can finish initialization
  if (header)
    header->reset(MWAWDocument::MSWORD, vers);

  if (long(input->tell()) != headerSize)
    ascii().addDelimiter(input->tell(), '|');

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// try to the end of the header
////////////////////////////////////////////////////////////
bool MSWParser::readHeaderEndV3()
{
  if (!isFilePos(0xb8))
    return false;
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  input->seek(0x78, WPX_SEEK_SET);
  long pos = input->tell();
  long val = input->readLong(4); // normally 0x100
  if (val != 0x100)
    f << "FileHeader[A]:" << std::hex << val << std::dec << ",";
  else
    f << "_";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  if (!m_textParser->readHeaderTextLength())
    return false;
  pos = input->tell();
  f << "FileHeader[B]:";
  for (int i = 0; i < 18; i++) { // always 0 ?
    val = input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  float dim[6]; // H, W+margin T, L, B, R
  for (int i = 0; i < 6; i++)
    dim[i] = float(input->readLong(2))/1440.0f;

  f << "page=" << dim[1] << "x" << dim[0] << ",";
  f << "margins=" << dim[3] << "x" << dim[2] << "-" << dim[5] << "x" << dim[4] << ",";
  bool dimOk = true;
  if (dim[0]>0 && dim[1]>0) {
    for (int i = 2; i < 6; i++)
      if (dim[i] < 0) dimOk = false;
    if (2*(dim[3]+dim[5]) > dim[1] || 2*(dim[2]+dim[4]) > dim[0]) dimOk = false;
    if (!dimOk) {
      f << "###";
      MWAW_DEBUG_MSG(("MSWParser::readHeaderEndV3: page dimensions seem bad\n"));
    } else {
      getPageSpan().setMarginTop(dim[2]);
      getPageSpan().setMarginBottom(dim[4]);
      getPageSpan().setMarginLeft(dim[3]);
      getPageSpan().setMarginRight(dim[5]);
      getPageSpan().setFormLength(dim[0]);
      getPageSpan().setFormWidth(dim[1]);
    }
  } else
    dimOk = false;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = input->tell();
  f.str("");
  f << "FileHeader[C]:";
  val = input->readLong(2); // always 0 ?
  if (val)
    f << "margins[binding]=" << float(val)/1440.f << ",";
  val = input->readLong(2);
  f << "defTabs=" << float(val)/1440.f << ",";
  int flags = (int) input->readULong(1);
  if (flags & 0x80) // page vis a vis
    f << "facingpage,";
  if (flags & 0x40) // ligne creuse
    f << "defTabs[emptyline],";
  switch((flags>>1) & 0x3) {
  case 0:
    if (dimOk) m_state->m_endNote = true;
    f << "endnote,";
    break;
  case 1:
    f << "footnote,";
    break;
  case 2:
    f << "footnote[undertext],";
    break;
  default:
    f << "#notepos=3,";
    break;
  }
  if (flags&1) {
    f << "landscape,";
    if (dimOk)
      getPageSpan().setFormOrientation(MWAWPageSpan::LANDSCAPE);
  }
  flags &= 0x38;
  if (flags)
    f << "#flags=" << std::hex << flags << std::dec << ",";
  flags = (int) input->readULong(1);
  if (flags) // always 1
    f << "fl1=" << std::hex << flags << std::dec << ",";
  char const *(wh[]) = { "note", "line", "page" };
  for (int i = 0; i < 3; i++) {
    val = (long) input->readULong(2);
    if (val == 1) continue;
    if (val & 0x8000)
      f << wh[i] << "[firstNumber]=" << (val&0x7FFF) << "[auto],";
    else
      f << wh[i] << "[firstNumber]=" << val << ",";
  }
  for (int i = 0; i < 2; i++) { // first flags often 0x40, second?
    flags = (int) input->readULong(1);
    if (flags) // always 1
      f << "fl" << 2+i << "=" << std::hex << flags << std::dec << ",";
  }
  for (int i = 0; i < 13; i++) { // always 0?
    val = input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = input->tell();
  f.str("");
  f << "FileHeader[D]:";
  int sz = (int) input->readULong(1);
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  if (sz > 31) {
    f << "###";
    MWAW_DEBUG_MSG(("MSWParser::readHeaderEndV3: next filename seems bad\n"));
  } else {
    std::string fName("");
    for (int i = 0; i < sz; i++)
      fName += (char) input->readULong(1);
    f << "nextFile=" << fName;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(0x100, WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// try to read an entry
////////////////////////////////////////////////////////////
MSWEntry MSWParser::readEntry(std::string type, int id)
{
  MWAWInputStreamPtr input = getInput();
  MSWEntry entry;
  entry.setType(type);
  entry.setId(id);
  long pos = input->tell();
  libmwaw::DebugStream f;

  long debPos = (long) input->readULong(4);
  long sz = (long) input->readULong(2);
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
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "DocumentInfo:";

  float dim[2];
  for (int i = 0; i < 2; i++) dim[i] =  float(input->readLong(2))/1440.f;
  f << "dim?=" << dim[1] << "x" << dim[0] << ",";

  float margin[4];
  f << ",marg=["; // top, left?, bottom, right?
  for (int i = 0; i < 4; i++) {
    margin[i] = float(input->readLong(2))/1440.f;
    f << margin[i] << ",";
    if (margin[i] < 0.0) margin[i] *= -1.0f;
  }
  f << "],";

  if (dim[0] > margin[0]+margin[2] && dim[1] > margin[1]+margin[3]) {
    getPageSpan().setMarginTop(margin[0]);
    getPageSpan().setMarginLeft(margin[1]);
    /* decrease a little the right/bottom margin to allow fonts discrepancy*/
    getPageSpan().setMarginBottom((margin[2]< 0.5) ? 0.0 : margin[2]-0.5);
    getPageSpan().setMarginRight((margin[3]< 0.5) ? 0.0 : margin[3]-0.5);

    getPageSpan().setFormLength(dim[0]);
    getPageSpan().setFormWidth(dim[1]);
  } else {
    MWAW_DEBUG_MSG(("MSWParser::readDocumentInfo: the page dimensions seems odd\n"));
  }

  int val = (int) input->readLong(2); // always 0 ?
  if (val) f << "unkn=" << val << ",";
  val = (int) input->readLong(2); // 0x2c5 or 0x2d0?
  f << "f0=" << val << ",";
  for (int i = 0; i < 4; i++) { //[a|12|40|42|4a|52|54|d2],0,0|80,1
    val = (int) input->readULong(1);
    if (val) f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  val = (int) input->readLong(2); // always 1 ?
  if (val != 1) f << "f1=" << val << ",";
  // a small number between 0 and 77
  f << "f2=" << (int) input->readLong(2) << ",";
  for (int i = 0; i < 4; i++) { //[0|2|40|42|44|46|48|58],0|64,0|10|80,[0|2|5]
    val = (int) input->readULong(1);
    if (val) f << "flA" << i << "=" << std::hex << val << std::dec << ",";
  }
  val = (int) input->readLong(2); // always 0 ?
  if (val != 1) f << "f3=" << val << ",";
  val = (int) input->readLong(2); // 0, 48, 50
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
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
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
    val = (int) input->readULong(1);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  // 0 or 1, followed by 0
  for (int i = 2; i < 4; i++) {
    val = (int) input->readLong(1);
    if (val) f << "f" << i << "=" << val << ",";
  }
  long ptr = (long) input->readULong(4); // a text ptr ( often near to textLength )
  f << "textPos[sel?]=" << std::hex << ptr << std::dec << ",";
  val  = (int) input->readULong(4); // almost always ptr
  if (val != ptr)
    f << "textPos1=" << std::hex << val << std::dec << ",";
  // a small int between 6 and b
  val = (int) input->readLong(2);
  if (val) f << "f4=" << val << ",";

  for (int i = 5; i < 7; i++) { // 0,0 or 3,5 or 8000, 8000
    val = (int) input->readULong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  val  = (int) input->readULong(4); // almost always ptr
  if (val != ptr)
    f << "textPos2=" << std::hex << val << std::dec << ",";
  /* g0=[0,1,5,c], g1=[0,1,3,4] */
  for (int i = 0; i < 2; i++) {
    val = (int) input->readLong(2);
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

  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Printer:";
  int sz = (int) input->readULong(2);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("MSWParser::readPrinter: the zone seems to short\n"));
    return false;
  }
  int strSz = (int) input->readULong(1);
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
    int val = (int) input->readLong(2);
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

  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "DocSum:";
  int sz = (int) input->readULong(2);
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

    sz = (int) input->readULong(1);
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

  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << entry;
  int sz = (int) input->readULong(2);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("MSWParser::readStringsZone: the zone seems to short\n"));
    return false;
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  int id = 0;
  while (long(input->tell()) != entry.end()) {
    pos = input->tell();
    int strSz = (int) input->readULong(1);
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
  MWAWInputStreamPtr input = getInput();

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

    if (entry.id() < 0 || entry.id() > 1) {
      MWAW_DEBUG_MSG(("MSWParser::readObjects: unexpected entry id: %d\n", entry.id()));
      continue;
    }
    std::vector<MSWParserInternal::Object> &listObject = m_state->m_objectList[entry.id()];
    size_t numObjects = listObject.size();
    if (list.size() != numObjects) {
      MWAW_DEBUG_MSG(("MSWParser::readObjects: unexpected number of name\n"));
      if (list.size() < numObjects) numObjects = list.size();
    }
    for (size_t i = 0; i < numObjects; i++)
      listObject[i].m_name = list[i];
  }

  for (int st = 0; st < 2; st++) {
    std::vector<MSWParserInternal::Object> &listObject = m_state->m_objectList[st];

    for (size_t i = 0; i < listObject.size(); i++)
      readObject(listObject[i]);
  }
  return true;
}

bool MSWParser::readObjectList(MSWEntry &entry)
{
  if (entry.id() < 0 || entry.id() > 1) {
    MWAW_DEBUG_MSG(("MSWParser::readObjectList: unexpected entry id: %d\n", entry.id()));
    return false;
  }
  std::vector<MSWParserInternal::Object> &listObject = m_state->m_objectList[entry.id()];
  listObject.resize(0);
  if (entry.length() < 4 || (entry.length()%18) != 4) {
    MWAW_DEBUG_MSG(("MSWParser::readObjectList: the zone size seems odd\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "ObjectList[" << entry.id() << "]:";
  int N=int(entry.length()/18);

  std::multimap<long, MSWText::PLC> &plcMap=m_textParser->getTextPLCMap();
  MSWText::PLC plc(MSWText::PLC::Object);
  std::vector<long> textPos; // checkme
  textPos.resize((size_t)N+1);
  f << "[";
  for (int i = 0; i < N+1; i++) {
    long tPos = (long) input->readULong(4);
    textPos[(size_t)i] = tPos;
    f << std::hex << tPos << std::dec << ",";
    if (i == N)
      break;
    plc.m_id = i;
    plcMap.insert(std::multimap<long, MSWText::PLC>::value_type(tPos,plc));
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  int val;

  for (int i = 0; i < N; i++) {
    MSWParserInternal::Object object;
    object.m_textPos = textPos[(size_t)i];
    pos = input->tell();
    f.str("");
    object.m_id = (int) input->readLong(2);
    // id0=<small number>:[8|48], id1: <small number>:60->normal, :7c?, 0->annotation ?
    for (int st = 0; st < 2; st++) {
      object.m_ids[st] = (int) input->readLong(2);
      object.m_idsFlag[st] = (int) input->readULong(1);
    }

    object.m_pos.setBegin((long) input->readULong(4));
    val = (int) input->readLong(2); // always 0 ?
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
  if (entry.id() < 0 || entry.id() > 1) {
    MWAW_DEBUG_MSG(("MSWParser::readObjectFlags: unexpected entry id: %d\n", entry.id()));
    return false;
  }
  std::vector<MSWParserInternal::Object> &listObject = m_state->m_objectList[entry.id()];
  int numObject = (int) listObject.size();
  if (entry.length() < 4 || (entry.length()%6) != 4) {
    MWAW_DEBUG_MSG(("MSWParser::readObjectFlags: the zone size seems odd\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "ObjectFlags[" << entry.id() << "]:";
  int N=int(entry.length()/6);
  if (N != numObject) {
    MWAW_DEBUG_MSG(("MSWParser::readObjectFlags: unexpected number of object\n"));
  }

  f << "[";
  for (int i = 0; i < N+1; i++) {
    long textPos = (long) input->readULong(4);
    if (i < numObject && textPos != listObject[(size_t)i].m_textPos && textPos != listObject[(size_t)i].m_textPos+1)
      f << "#";
    f << std::hex << textPos << std::dec << ",";
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    int fl[2];
    for (int st = 0; st < 2; st++) fl[st] = (int) input->readULong(1);
    f.str("");
    f << "ObjectFlags-" << i << ":";
    if (i < numObject) {
      for (int st = 0; st < 2; st++) listObject[(size_t)i].m_flags[st] = fl[st];
      f << "Obj" << listObject[(size_t)i].m_id << ",";
    }
    // indentical to ObjectList id0[low] ?
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
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;

  long pos = obj.m_pos.begin(), beginPos = pos;
  if (!pos) return false;

  input->seek(pos, WPX_SEEK_SET);
  int sz = (int) input->readULong(4);

  f << "Entries(ObjectData):Obj" << obj.m_id << ",";
  if (!isFilePos(pos+sz) || sz < 6) {
    MWAW_DEBUG_MSG(("MSWParser::readObject: pb finding object data sz\n"));
    f << "#";
    ascii().addPos(beginPos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  obj.m_pos.setLength(sz);
  long endPos = obj.m_pos.end();
  ascii().addPos(endPos);
  ascii().addNote("_");

  int fSz = (int) input->readULong(2);
  if (fSz < 0 || fSz+6 > sz) {
    MWAW_DEBUG_MSG(("MSWParser::readObject: pb reading the name\n"));
    f << "#";
    ascii().addPos(beginPos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  MSWEntry fileEntry = obj.getEntry();
  fileEntry.setParsed(true);
  m_entryMap.insert
  (std::multimap<std::string, MSWEntry>::value_type
   (fileEntry.type(), fileEntry));

  long zoneEnd = pos+6+fSz;
  std::string name(""); // first equation, second "" or Equation Word?
  while(long(input->tell()) != zoneEnd) {
    int c = (int) input->readULong(1);
    if (c == 0) {
      if (name.length()) f << name << ",";
      name = "";
      continue;
    }
    name += char(c);
  }
  if (name.length()) f << name << ",";

  pos = input->tell();
  // Equation Word? : often contains not other data
  if (pos==endPos) {
    ascii().addPos(beginPos);
    ascii().addNote(f.str().c_str());
    return true;
  }

  // 0 or a small size c for annotation an equivalent of file type?
  fSz = (int) input->readULong(1);
  if (pos+fSz+1 > endPos) {
    MWAW_DEBUG_MSG(("MSWParser::readObject: pb reading the second field zone\n"));
    f << "#fSz=" << fSz;
    ascii().addPos(beginPos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int val;
  bool isAnnotation=false;
  if (fSz==12) { // possible annotation
    f << "type=[";
    for (int i = 0; i < 4; i++) { // f0=f1=f2=0, f3=0x100
      val = (int) input->readLong(2);
      if (val) f << "g0=" << std::hex << val << std::dec << ",";
    }
    std::string type("");
    for (int i = 0; i < 4; i++)
      type += (char) input->readULong(1);
    f << type << "],";
    isAnnotation=type=="ANOT";
  } else if (fSz) {
    f << "##data2[sz]=" << fSz << ",";
    ascii().addDelimiter(input->tell(),'|');
    input->seek(pos+fSz+1, WPX_SEEK_SET);
    ascii().addDelimiter(input->tell(),'|');
  }
  if (isAnnotation) {
    MWAW_DEBUG_MSG(("MSWParser::readObject: find some annotations, not implemented\n"));
  }
  pos = input->tell();
  if (pos+2>endPos) {
    if (pos!= endPos)
      f << "###";
    ascii().addPos(beginPos);
    ascii().addNote(f.str().c_str());
    return true;
  }
  val = (int) input->readLong(2);
  if (val) f << "#f0=" << val << ",";

  pos = input->tell();
  if (pos+4>endPos) {
    if (pos!= endPos)
      f << "##";
    ascii().addPos(beginPos);
    ascii().addNote(f.str().c_str());
    return true;
  }
  long dataSz = (long) input->readULong(4);
  pos = input->tell();
  if (pos+dataSz > endPos) {
    MWAW_DEBUG_MSG(("MSWParser::readObject: pb reading the last field size zone\n"));
    f << "#fSz[last]=" << dataSz;
    ascii().addPos(beginPos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  if (isAnnotation && dataSz>9) {
    f << "annot=[";
    for (int i = 0; i < 3; i++) { // h0=1|2, h1,h2: big numbers
      val = (int) input->readULong(2);
      if (val)
        f << "h" << i << "=" << std::hex << val << std::dec << ",";
    }
    fSz = (int) input->readULong(1);
    bool ok=true;
    if (fSz+7 > dataSz) {
      MWAW_DEBUG_MSG(("MSWParser::readObject: can not read the annotation string\n"));
      f << "###";
      ok = false;
    } else {
      std::string annotation("");
      for (int i = 0; i < fSz; i++)
        annotation += (char) input->readULong(1);
      if (annotation.length())
        f << "annot[inText]=" << annotation << ",";
    }

    if (ok) {
      val = (int) input->readULong(1); // always 0
      if (val)
        f << "h3=" << std::hex << val << std::dec << ",";
      fSz = (int) input->readULong(1);
    }
    if (!ok) {
    } else if (fSz+9 > dataSz) {
      MWAW_DEBUG_MSG(("MSWParser::readObject: can not read the annotation comment\n"));
      f << "###";
    } else {
      std::string annotation("");
      for (int i = 0; i < fSz; i++)
        annotation += (char) input->readULong(1);
      if (annotation.length())
        f << "annot[comment]=" << annotation << ",";
    }
  } else if (dataSz)
    ascii().addDelimiter(pos, '|');
  input->seek(pos+dataSz, WPX_SEEK_SET);

  pos = input->tell();
  ascii().addPos(beginPos);
  ascii().addNote(f.str().c_str());
  if (pos != endPos)
    ascii().addDelimiter(pos, '#');

  return true;
}

////////////////////////////////////////////////////////////
// check if a zone is a picture/read a picture
////////////////////////////////////////////////////////////
bool MSWParser::checkPicturePos(long pos, int type)
{
  if (pos < 0x100 || !isFilePos(pos))
    return false;

  MWAWInputStreamPtr input = getInput();
  input->seek(pos, WPX_SEEK_SET);
  long sz = (long) input->readULong(4);
  long endPos = pos+sz;
  if (sz < 14 || !isFilePos(sz+pos)) return false;
  int num = (int) input->readLong(1);
  if (num < 0 || num > 4) return false;
  input->seek(pos+14, WPX_SEEK_SET);
  for (int n = 0; n < num; n++) {
    long actPos = input->tell();
    long pSz = (long) input->readULong(4);
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
  entry.setPictType(type);
  entry.setId(id++);
  m_entryMap.insert
  (std::multimap<std::string, MSWEntry>::value_type(entry.type(), entry));

  return true;
}

bool MSWParser::readPicture(MSWEntry &entry)
{
  if (m_state->m_picturesMap.find(entry.begin())!=m_state->m_picturesMap.end())
    return true;
  if (entry.length() < 30 && entry.length() != 14) {
    MWAW_DEBUG_MSG(("MSWParser::readPicture: the zone seems too short\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(Picture)[" << entry.pictType() << "-" << entry.id() << "]:";
  long sz = (long) input->readULong(4);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("MSWParser::readPicture: the zone size seems too big\n"));
    return false;
  }
  int N = (int) input->readULong(1);
  f << "N=" << N << ",";
  MSWParserInternal::Picture pict;
  pict.m_flag = (int) input->readULong(1); // find 0 or 0x80
  int dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = (int) input->readLong(2);
  pict.m_dim=Box2i(Vec2i(dim[1],dim[0]), Vec2i(dim[3],dim[2]));
  f << pict;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int n=0; n < N; n++) {
    MSWParserInternal::Picture::Zone zone;
    pos = input->tell();
    f.str("");
    f << "Picture-" << n << "[" << entry.pictType() << "-" << entry.id() << "]:";
    sz = (long) input->readULong(4);
    if (sz < 16 || sz+pos > entry.end()) {
      MWAW_DEBUG_MSG(("MSWParser::readPicture: pb with the picture size\n"));
      f << "#";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return false;
    }
    for (int i = 0; i < 3; i++)
      zone.m_flags[i] = (int) input->readULong((i==2) ? 2 : 1);
    for (int i = 0; i < 4; i++)
      dim[i] = (int) input->readLong(2);
    zone.m_dim=Box2i(Vec2i(dim[1],dim[0]), Vec2i(dim[3],dim[2]));
    zone.m_pos.setBegin(pos+16);
    zone.m_pos.setLength(sz-16);
    f << zone;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    if (sz <= 16)
      continue;
    pict.m_picturesList.push_back(zone);
#ifdef DEBUG_WITH_FILES
    ascii().skipZone(pos+16, pos+sz-1);
    WPXBinaryData file;
    input->seek(pos+16, WPX_SEEK_SET);
    input->readDataBlock(sz-16, file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f2;
    f2 << "PICT-" << ++pictName << ".pct";
    libmwaw::Debug::dumpFile(file, f2.str().c_str());
#endif

    input->seek(pos+sz, WPX_SEEK_SET);
  }
  m_state->m_picturesMap[entry.begin()]=pict;
  pos = input->tell();
  if (pos != entry.end())
    ascii().addDelimiter(pos, '|');
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

void MSWParser::sendPicture(long fPos, int cPos, MWAWPosition::AnchorTo anchor)
{
  if (!getListener()) {
    MWAW_DEBUG_MSG(("MSWParser::sendPicture: listener is not set\n"));
    return;
  }
  if (m_state->m_picturesMap.find(fPos)==m_state->m_picturesMap.end()) {
    MWAW_DEBUG_MSG(("MSWParser::sendPicture: can not find picture for pos %lx\n", fPos));
    return;
  }
  MSWParserInternal::Picture const &pict= m_state->m_picturesMap.find(fPos)->second;
  MWAWInputStreamPtr input = getInput();
  if (pict.m_picturesList.size()!=1 &&
      (anchor == MWAWPosition::Char || anchor == MWAWPosition::CharBaseLine)) {
    shared_ptr<MSWParserInternal::SubDocument> subdoc
    (new MSWParserInternal::SubDocument(*this, input, fPos, cPos));
    MWAWPosition pictPos(pict.m_dim.min(), pict.m_dim.size(), WPX_POINT);;
    pictPos.setRelativePosition(MWAWPosition::Char,
                                MWAWPosition::XLeft, MWAWPosition::YTop);
    pictPos.m_wrapping =  MWAWPosition::WBackground;
    getListener()->insertTextBox(pictPos, subdoc);
    return;
  }
  long actPos = input->tell();
  MWAWPosition basicPos(Vec2f(0.,0.), Vec2f(100.,100.), WPX_POINT);
  if (anchor != MWAWPosition::Page && anchor != MWAWPosition::Frame) {
    basicPos.setRelativePosition(anchor, MWAWPosition::XLeft, MWAWPosition::YCenter);
    basicPos.m_wrapping =  MWAWPosition::WBackground;
  } else
    basicPos.setRelativePosition(anchor);


  std::string pictType;
  WPXBinaryData data;
  Box2f naturalBox;
  for (size_t p = 0; p < pict.m_picturesList.size(); p++) {
    MSWParserInternal::Picture::Zone const &zone=pict.m_picturesList[p];
    if (!zone.m_pos.valid()) continue;
    MWAWPosition pos(basicPos);
    pos.setOrigin(pos.origin()+(Vec2f)zone.m_dim.min());
    pos.setSize(zone.m_dim.size());

    input->seek(zone.m_pos.begin(), WPX_SEEK_SET);
    MWAWPict::ReadResult res = MWAWPictData::check(input, (int)zone.m_pos.length(), naturalBox);
    if (res == MWAWPict::MWAW_R_BAD) {
      MWAW_DEBUG_MSG(("MSWParser::sendPicture: can not find the picture %d\n", int(p)));
      continue;
    }

    input->seek(zone.m_pos.begin(), WPX_SEEK_SET);
    shared_ptr<MWAWPict> thePict(MWAWPictData::get(input, (int)zone.m_pos.length()));
    if (!thePict) continue;
    thePict->getBinary(data,pictType);
    if (data.size())
      getListener()->insertPicture(pos, data, pictType);
  }
  input->seek(actPos, WPX_SEEK_SET);
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
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  // print info
  libmwaw::PrinterInfo info;
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

  getPageSpan().setFormOrientation(MWAWPageSpan::PORTRAIT);
  getPageSpan().setMarginTop(topMargin/72.0);
  getPageSpan().setMarginBottom(botMarg/72.0);
  getPageSpan().setMarginLeft(leftMargin/72.0);
  getPageSpan().setMarginRight(rightMarg/72.0);
  getPageSpan().setFormLength(paperSize.y()/72.);
  getPageSpan().setFormWidth(paperSize.x()/72.);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  if (long(input->tell()) != entry.end())
    ascii().addDelimiter(input->tell(), '|');

  ascii().addPos(entry.end());
  ascii().addNote("_");

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
