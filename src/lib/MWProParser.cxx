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

#include <libwpd/WPXBinaryData.h>
#include <libwpd/WPXString.h>

#include "TMWAWPosition.hxx"
#include "TMWAWPictMac.hxx"
#include "TMWAWPrint.hxx"

#include "IMWAWHeader.hxx"

#include "MWAWStruct.hxx"
#include "MWAWTools.hxx"
#include "MWAWContentListener.hxx"

#include "MWProParser.hxx"
#include "MWProStructures.hxx"

// set this flag to true to create an ascii file of the original
#define DEBUG_RECONSTRUCT 0

/** Internal: the structures of a MWProParser */
namespace MWProParserInternal
{
////////////////////////////////////////
//! Internal: a struct used to store a zone
struct Zone {
  Zone() : m_type(-1), m_blockId(0), m_data(), m_input(), m_asciiFile(), m_parsed(false) {
  }
  ~Zone() {
    ascii().reset();
  }

  //! returns the debug file
  libmwaw_tools::DebugFile &ascii() {
    return m_asciiFile;
  }

  //! the type : 0(text), 1(graphic)
  int m_type;

  //! the first block id
  int m_blockId;

  //! the storage
  WPXBinaryData m_data;

  //! the main input
  TMWAWInputStreamPtr m_input;

  //! the debug file
  libmwaw_tools::DebugFile m_asciiFile;

  //! true if the zone is sended
  bool m_parsed;
};

//! Internal: a struct used to store a text zone
struct TextZoneData {
  TextZoneData() : m_type(-1), m_length(0), m_id(0) {
  }
  friend std::ostream &operator<<(std::ostream &o, TextZoneData const &tData) {
    switch(tData.m_type) {
    case 0:
      o << "C" << tData.m_id << ",";
      break;
    case 1:
      o << "P" << tData.m_id << ",";
      break;
    default:
      o << "type=" << tData.m_type << ",id=" << tData.m_id << ",";
      break;
    }
    o << "nC=" << tData.m_length << ",";
    return o;
  }
  //! the type
  int m_type;
  //! the text length
  int m_length;
  //! an id
  int m_id;
};

//! Internal: a struct used to store a text zone
struct Token {
  Token() : m_type(-1), m_length(0), m_blockId(-1) {
    for (int i = 0; i < 3; i++) m_flags[i] = 0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Token const &tkn) {
    o << "nC=" << tkn.m_length << ",";
    switch(tkn.m_type) {
    case 1:
      o << "pagenumber,";
      break;
    case 2:
      o << "footnote(pos),";
      break;
    case 3:
      o << "footnote(content),";
      break;
    case 4:
      o << "figure,";
      break;
    case 5:
      o << "hyphen,";
      break;
    case 6:
      o << "date,";
      break;
    case 7:
      o << "time,";
      break;
    case 8:
      o << "title,";
      break;
    case 9:
      o << "revision,";
      break;
    case 10:
      o << "sectionnumber,";
      break;
    default:
      o << "#type=" << tkn.m_type << ",";
    }
    if (tkn.m_blockId >= 0) o << "blockId=" << tkn.m_blockId << ",";
    for (int i = 0; i < 3; i++) {
      if (tkn.m_flags[i]) o << "flags=" << std::hex << tkn.m_flags[i] << ",";
    }
    return o;
  }
  //! the type
  int m_type;
  //! the text length
  int m_length;
  //! the block id
  int m_blockId;
  //! some flags
  int m_flags[3];
};

//! Internal: a struct used to store a text zone
struct TextZone {
  TextZone() : m_textLength(0), m_entries(), m_tokens(), m_parsed(false) {
  }

  //! the text length
  int m_textLength;

  //! the list of entries
  std::vector<IMWAWEntry> m_entries;

  //! two vector list of id ( charIds, paragraphIds)
  std::vector<TextZoneData> m_ids[2];

  //! the tokens list
  std::vector<Token> m_tokens;

  //! true if the zone is sended
  bool m_parsed;
};


////////////////////////////////////////
//! Internal: the state of a MWProParser
struct State {
  //! constructor
  State() : m_version(-1), m_blocksMap(), m_dataMap(), m_textMap(),
    m_blocksCallByTokens(), m_actPage(0), m_numPages(0),
    m_headerHeight(0), m_footerHeight(0) {
  }

  //! the file version
  int m_version;

  //! the list of retrieved block : block -> new address
  std::map<int,long> m_blocksMap;

  //! the list of blockId->data zone
  std::map<int, shared_ptr<Zone> > m_dataMap;

  //! the list of blockId->text zone
  std::map<int, shared_ptr<TextZone> > m_textMap;

  //! the list of blockId called by tokens
  std::vector<int> m_blocksCallByTokens;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MWProParser
class SubDocument : public IMWAWSubDocument
{
public:
  SubDocument(MWProParser &pars, TMWAWInputStreamPtr input, int zoneId) :
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
  //! the subdocument id
  int m_id;
};

void SubDocument::parse(IMWAWContentListenerPtr &listener, DMWAWSubDocumentType /*type*/)
{
  if (m_id == -3) return; // empty block
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  MWProContentListener *listen = dynamic_cast<MWProContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
    return;
  }
  assert(m_parser);

  long pos = m_input->tell();
  MWProParser *parser = reinterpret_cast<MWProParser *>(m_parser);
  if (parser->m_structures.get())
    parser->m_structures->send(m_id);
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
MWProParser::MWProParser(TMWAWInputStreamPtr input, IMWAWHeader * header) :
  IMWAWParser(input, header), m_listener(), m_convertissor(), m_state(),
  m_structures(), m_pageSpan(), m_listSubDocuments(),
  m_asciiFile(), m_asciiName("")
{
  init();
}

MWProParser::~MWProParser()
{
  if (m_listener.get()) m_listener->endDocument();
}

void MWProParser::init()
{
  m_convertissor.reset(new MWAWTools::Convertissor);
  m_listener.reset();
  m_asciiName = "main-1";

  m_state.reset(new MWProParserInternal::State);
  m_structures.reset(new MWProStructures(*this));

  // reduce the margin (in case, the page is not defined)
  m_pageSpan.setMarginTop(0.1);
  m_pageSpan.setMarginBottom(0.1);
  m_pageSpan.setMarginLeft(0.1);
  m_pageSpan.setMarginRight(0.1);
}

void MWProParser::setListener(MWProContentListenerPtr listen)
{
  m_listener = listen;
  m_structures->setListener(listen);
}

int MWProParser::version() const
{
  return m_state->m_version;
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
float MWProParser::pageHeight() const
{
  return m_pageSpan.getFormLength()-m_pageSpan.getMarginTop()-m_pageSpan.getMarginBottom()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0;
}

float MWProParser::pageWidth() const
{
  return m_pageSpan.getFormWidth()-m_pageSpan.getMarginLeft()-m_pageSpan.getMarginRight();
}


////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void MWProParser::newPage(int number, bool softBreak)
{
  if (number <= m_state->m_actPage) return;
  if (number > m_state->m_numPages) {
    MWAW_DEBUG_MSG(("MWProParser::newPage: can not create new page\n"));
    return;
  }

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

std::vector<int> const &MWProParser::getBlocksCalledByToken() const
{
  return m_state->m_blocksCallByTokens;
}


////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MWProParser::parse(WPXDocumentInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw_libwpd::ParseException());
  bool ok = true;
  try {
    m_state->m_blocksMap.clear();

    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);

    ok = createZones();
#if DEBUG_RECONSTRUCT && defined(DEBUG_WITH_FILES)
    saveOriginal(getInput());
#endif
    if (ok) {
      createDocument(docInterface);
      if (m_structures) {
        m_structures->sendMainZone();
        m_structures->flushExtra();
      }
    }

    std::vector<int> freeList;
    if (getFreeZoneList(2, freeList) && freeList.size() > 1) {
      for (int i = 1; i < int(freeList.size()); i++) {
        ascii().addPos(freeList[i]*0x100);
        ascii().addNote("Entries(Free)");
      }
    }
#ifdef DEBUG
    checkUnparsed();
#endif

    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("MWProParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  if (!ok) throw(libmwaw_libwpd::ParseException());
}

////////////////////////////////////////////////////////////
// returns a data zone which corresponds to a zone id
////////////////////////////////////////////////////////////
bool MWProParser::getZoneData(WPXBinaryData &data, int blockId)
{
  data.clear();
  if (blockId < 1) {
    MWAW_DEBUG_MSG(("MWProParser::getZoneData: block %d is invalid\n", blockId));
    return false;
  }
  TMWAWInputStreamPtr input = getInput();
  input->seek((blockId-1)*0x100, WPX_SEEK_SET);

  unsigned long read;
  int first=blockId-1, last=blockId-1;
  while (!input->atEOS()) {
    bool ok = true;
    for(int i=first; i<= last; i++) {
      if (m_state->m_blocksMap.find(i) != m_state->m_blocksMap.end()) {
        MWAW_DEBUG_MSG(("MWProParser::getZoneData: block %d already seems\n", i));
        ok = false;
        break;
      }
      m_state->m_blocksMap[i] = data.size()+(i-first)*0x100;
    }
    if (!ok) break;
    long endPos = (last+1)*0x100 - 4;
    long pos = input->tell();
    input->seek(endPos, WPX_SEEK_SET);
    long limit = input->tell();
    if (limit <= pos) break;
    input->seek(pos, WPX_SEEK_SET);

    const unsigned char *dt = input->read(limit-pos, read);
    data.append(dt, read);
    ascii().skipZone(first*0x100, (last+1)*0x100-1);

    if (long(read) != limit-pos) {
      MWAW_DEBUG_MSG(("MWProParser::getZoneData: can not read all data\n"));
      break;
    }
    if (limit < endPos)
      break;
    input->seek(limit, WPX_SEEK_SET);

    int act = last;
    long val = input->readLong(4);
    if (val == 0) break;
    if (val < 0)
      first = (-val)-1;
    else
      first = val-1;
    last = first;

    if (first != act+1) {
      input->seek(first*0x100, WPX_SEEK_SET);
      if (long(input->tell()) != first*0x100) {
        MWAW_DEBUG_MSG(("MWProParser::getZoneData: can not go to %d block\n", first));
        break;
      }
    }
    if (val < 0) {
      int num = input->readULong(4);
      last = first+(num-1);
    }
    if (last-first > 2) {
      pos = input->tell();
      input->seek((last-1)*0x100, WPX_SEEK_SET);
      if (long(input->tell()) != (last-1)*0x100) {
        MWAW_DEBUG_MSG(("MWProParser::getZoneData: num %d if odd\n", last));
        last = input->tell();
        last = (last>>8)+1;  // set last to the last block
      }
      input->seek(pos, WPX_SEEK_SET);
    }
  }
  return data.size() != 0;
}

////////////////////////////////////////////////////////////
// return the chain list of block ( used to get free blocks)
////////////////////////////////////////////////////////////
bool MWProParser::getFreeZoneList(int blockId, std::vector<int> &blockLists)
{
  blockLists.clear();
  if (blockId < 1) {
    MWAW_DEBUG_MSG(("MWProParser::getFreeZoneList: block %d is invalid\n", blockId));
    return false;
  }
  TMWAWInputStreamPtr input = getInput();

  int first=blockId-1, last=blockId-1;
  while (1) {
    bool ok = true;
    for(int i=first; i<= last; i++) {
      if (m_state->m_blocksMap.find(i) != m_state->m_blocksMap.end()) {
        MWAW_DEBUG_MSG(("MWProParser::getFreeZoneList: block %d already seems\n", i));
        ok = false;
        break;
      }
      blockLists.push_back(i);
      m_state->m_blocksMap[i] = 0;
    }
    if (!ok) break;
    long endPos = (last+1)*0x100 - 4;
    input->seek(endPos, WPX_SEEK_SET);
    if (long(input->tell()) != endPos) break;


    int act = last;
    long val = input->readLong(4);
    if (val == 0) break;
    if (val < 0)
      first = (-val)-1;
    else
      first = val-1;
    last = first;

    if (val < 0) {
      if (first != act+1) {
        input->seek(first*0x100, WPX_SEEK_SET);
        if (long(input->tell()) != first*0x100) {
          MWAW_DEBUG_MSG(("MWProParser::getFreeZoneList: can not go to %d block\n", first));
          break;
        }
      }

      int num = input->readULong(4);
      last = first+(num-1);
    }
  }
  return blockLists.size() != 0;
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MWProParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (m_listener) {
    MWAW_DEBUG_MSG(("MWProParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;
  int numPages = m_structures ? m_structures->numPages() : 0;
  if (numPages <= 0) numPages = 1;
  m_state->m_numPages = numPages;

  // create the page list
  std::list<DMWAWPageSpan> pageList;


  DMWAWTableList tableList;
  int actHeaderId = 0, actFooterId = 0;
  shared_ptr<MWProParserInternal::SubDocument> headerSubdoc, footerSubdoc;
  for (int i = 0; i < m_state->m_numPages; i++) {
    int headerId =  m_structures->getHeaderId(i+1);
    if (headerId != actHeaderId) {
      actHeaderId = headerId;
      if (actHeaderId == 0)
        headerSubdoc.reset();
      else {
        headerSubdoc.reset
        (new MWProParserInternal::SubDocument(*this, getInput(), headerId));
        m_listSubDocuments.push_back(headerSubdoc);
      }
    }
    int footerId =  m_structures->getFooterId(i+1);
    if (footerId != actFooterId) {
      actFooterId = footerId;
      if (actFooterId == 0)
        footerSubdoc.reset();
      else {
        footerSubdoc.reset
        (new MWProParserInternal::SubDocument(*this, getInput(), footerId));
        m_listSubDocuments.push_back(footerSubdoc);
      }
    }

    DMWAWPageSpan ps(m_pageSpan);
    if (headerSubdoc)
      ps.setHeaderFooter(HEADER, 0, ALL, headerSubdoc.get(), tableList);
    if (footerSubdoc)
      ps.setHeaderFooter(FOOTER, 0, ALL, footerSubdoc.get(), tableList);
    pageList.push_back(ps);
  }

  //
  MWProContentListenerPtr listen =
    MWProContentListener::create(pageList, documentInterface, m_convertissor);
  setListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool MWProParser::createZones()
{
  TMWAWInputStreamPtr input = getInput();
  long pos = input->tell();

  if (!readPrintInfo()) {
    // bad sign, but we can try to recover
    ascii().addPos(pos);
    ascii().addNote("###PrintInfo");
    input->seek(pos+0x78, WPX_SEEK_SET);
  }

  pos = input->tell();
  if (!readDocHeader()) {
    ascii().addPos(pos);
    ascii().addNote("##Entries(Data0)");
  }

  // ok now ask the structure manager to retrieve its data
  return m_structures->createZones();
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MWProParser::checkHeader(IMWAWHeader *header, bool strict)
{
  *m_state = MWProParserInternal::State();

  TMWAWInputStreamPtr input = getInput();

  libmwaw_tools::DebugStream f;
  int headerSize=4;
  input->seek(headerSize+0x78,WPX_SEEK_SET);
  if (int(input->tell()) != headerSize+0x78) {
    MWAW_DEBUG_MSG(("MWProParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,WPX_SEEK_SET);

  int vers = input->readULong(2);
  int val = input->readULong(2);

  f << "FileHeader:";
  switch (vers) {
  case 4:
    vers = 1;
    if (val != 4) {
#ifdef DEBUG
      if (strict || val < 3 || val > 5)
        return false;
      f << "#unk=" << val << ",";
#else
      return false;
#endif
    }
    break;
  default:
    MWAW_DEBUG_MSG(("MWProParser::checkHeader: unknown version\n"));
    return false;
  }
  m_state->m_version = vers;
  f << "vers=" << vers << ",";
  if (strict) {
    if (!readPrintInfo())
      return false;
    input->seek(0xdd, WPX_SEEK_SET);
    // "MP" seems always in this position
    if (input->readULong(2) != 0x4d50)
      return false;
  }


  // ok, we can finish initialization
  if (header) {
    header->setMajorVersion(m_state->m_version);
    header->setType(IMWAWDocument::MWPRO);
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
bool MWProParser::readPrintInfo()
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
  Vec2i rBotMargin=info.paper().pos(1) - info.page().pos(1);

  // move margin left | top
  int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
  int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
  lTopMargin -= Vec2i(decalX, decalY);
  rBotMargin += Vec2i(decalX, decalY);

  // decrease right | bottom
  int rightMarg = rBotMargin.x() -10;
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
    MWAW_DEBUG_MSG(("MWProParser::readPrintInfo: file is too short\n"));
    return false;
  }
  ascii().addPos(input->tell());

  return true;
}

////////////////////////////////////////////////////////////
// read the document header
////////////////////////////////////////////////////////////
bool MWProParser::readDocHeader()
{
  TMWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw_tools::DebugStream f;

  f << "Entries(Data0):";
  long val = input->readLong(1); // always 0 ?
  if (val) f << "unkn=" << val << ",";
  int N=input->readLong(2); // find 2, a, 9e, 1a
  f << "N?=" << N << ",";
  N = input->readLong(1); // almost always 0, find one time 6 ?
  if (N) f << "N1?=" << N << ",";
  val = input->readLong(2); // almost always 0x622, find also 0 and 12
  f << "f0=" << std::hex << val << std::dec << ",";
  val = input->readLong(1); // always 0 ?
  if (val) f << "unkn1=" << val << ",";
  N = input->readLong(2);
  f << "N2?=" << N << ",";
  val = input->readLong(1); // almost always 1 ( find one time 2)
  f << "f1=" << val << ",";
  int const defVal[] = { 0x64, 0/*small number between 1 and 8*/, 0x24 };
  for (int i = 0; i < 3; i++) {
    val = input->readLong(2);
    if (val != defVal[i])
      f << "f" << i+2 << "=" << val << ",";
  }
  for (int i = 5; i < 10; i++) { // always 0 ?
    val = input->readLong(1);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  val = input->readLong(2); // always 480 ?
  if (val != 0x480) f << "f10=" << val << ",";
  val = input->readULong(1); // always 0 ?
  if (val) f << "f11=" << val << ",";
  float dim[6];
  for (int i = 0; i < 6; i++)
    dim[i] = input->readLong(4)/65356.;
  bool ok = true;
  for (int i = 0; i < 6; i++) {
    if (dim[i] < 0) ok = false;
  }
  if (ok) ok = dim[0] > dim[2]+dim[3] && dim[1] > dim[4]+dim[5];

  if (ok) {
    m_pageSpan.setMarginTop(dim[2]/72.0);
    m_pageSpan.setMarginLeft(dim[4]/72.0);
    /* decrease a little the right/bottom margin to allow fonts discrepancy*/
    m_pageSpan.setMarginBottom((dim[3]<36.0) ? 0.0 : dim[3]/72.0-0.5);
    m_pageSpan.setMarginRight((dim[5]<18.0) ? 0.0 : dim[5]/72.0-0.25);
    m_pageSpan.setFormLength(dim[0]/72.);
    m_pageSpan.setFormWidth(dim[1]/72.);
  } else {
    MWAW_DEBUG_MSG(("MWProParser::readDocHeader: find odd page dimensions, ignored\n"));
    f << "#";
  }
  f << "dim=" << dim[1] << "x" << dim[0] << ",";
  f << "margins=["; // top, bottom, left, right
  for (int i = 2; i < 6; i++) f << dim[i] << ",";
  f << "],";

  ascii().addDelimiter(input->tell(), '|');
  /** then find
      000000fd0000000000018200000100002f00
      44[40|80] followed by something like a7c3ec07|a7c4c3c6 : 2 ptrs ?
      6f6600000000000000080009000105050506010401
   */
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(pos+97, WPX_SEEK_SET);
  pos = input->tell();
  f.str("");
  f << "Data0-A:";
  val = input->readULong(2);
  if (val != 0x4d50) // MP
    f << "#keyWord=" << std::hex << val <<std::dec;
  //always 4, 4, 6 ?
  for (int i = 0; i < 3; i++) {
    val = input->readLong(1);
    if ((i==2 && val!=6) || (i < 2 && val != 4))
      f << "f" << i << "=" << val << ",";
  }
  for (int i = 3; i < 9; i++) { // always 0 ?
    val = input->readLong(2);
    if (val) f << "f"  << i << "=" << val << ",";
  }
  // some dim ?
  f << "dim=[";
  for (int i = 0; i < 4; i++)
    f << input->readLong(2) << ",";
  f << "],";
  // always 0x48 0x48
  for (int i = 0; i < 2; i++) {
    val = input->readLong(2);
    if (val != 0x48) f << "g"  << i << "=" << val << ",";
  }
  // always 0 ?
  for (int i = 2; i < 42; i++) {
    val = input->readLong(2);
    if (val) f << "g"  << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // then junk ? (ie. find a string portion, a list of 0...),
  pos = input->tell();
  f.str("");
  f << "Data0-B:";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // interesting data seems to begin again in 0x200...
  input->seek(0x200, WPX_SEEK_SET);
  ascii().addPos(input->tell());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// try to parse a data zone
////////////////////////////////////////////////////////////
bool MWProParser::parseDataZone(int blockId, int type)
{
  if (m_state->m_dataMap.find(blockId) != m_state->m_dataMap.end())
    return true;
  if (blockId < 1) {
    MWAW_DEBUG_MSG(("MWProParser::parseDataZone: block %d seems bad\n", blockId));
    return false;
  }
  if (m_state->m_blocksMap.find(blockId-1) != m_state->m_blocksMap.end()) {
    MWAW_DEBUG_MSG(("MWProParser::parseDataZone: block %d is already parsed\n", blockId));
    return false;
  }

  shared_ptr<MWProParserInternal::Zone> zone(new MWProParserInternal::Zone);
  zone->m_blockId = blockId;
  zone->m_type = type;

  if (!getZoneData(zone->m_data, blockId))
    return false;
  WPXInputStream *dataInput =
    const_cast<WPXInputStream *>(zone->m_data.getDataStream());
  if (!dataInput) {
    MWAW_DEBUG_MSG(("MWProParser::parseDataZone: can not find my input\n"));
    return false;
  }

  zone->m_input.reset(new TMWAWInputStream(dataInput, false));
  zone->m_input->setResponsable(false);

  zone->m_asciiFile.setStream(zone->m_input);
  std::stringstream s;
  s << "DataZone" << std::hex << blockId << std::dec;
  zone->m_asciiFile.open(s.str());
  m_state->m_dataMap[blockId] = zone;

  // ok init is done
  if (type == 0)
    parseTextZone(zone);
  else if (type == 1)
    ;
  else {
    libmwaw_tools::DebugStream f;
    f << "Entries(DataZone):type" << type;
    zone->m_asciiFile.addPos(0);
    zone->m_asciiFile.addNote(f.str().c_str());
  }
  return true;
}

bool MWProParser::parseTextZone(shared_ptr<MWProParserInternal::Zone> zone)
{
  if (!zone) return false;
  if (zone->m_type != 0) {
    MWAW_DEBUG_MSG(("MWProParser::parseTextZone: not a picture date\n"));
    return false;
  }

  TMWAWInputStreamPtr input = zone->m_input;
  TMWAWInputStreamPtr fileInput = getInput();
  libmwaw_tools::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw_tools::DebugStream f;

  shared_ptr<MWProParserInternal::TextZone> text(new MWProParserInternal::TextZone);

  long pos = 0;
  input->seek(pos, WPX_SEEK_SET);
  f << "Entries(TextZone):";
  text->m_textLength = input->readLong(4);
  f << "textLength=" << text->m_textLength << ",";

  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  if (!readTextEntries(zone, text->m_entries, text->m_textLength))
    return false;
  m_state->m_textMap[zone->m_blockId] = text;

  for (int i = 0; i < int(text->m_entries.size()); i++) {
    IMWAWEntry &entry = text->m_entries[i];
    fileInput->seek(entry.begin(), WPX_SEEK_SET);
    if (long(fileInput->tell()) != entry.begin()) {
      MWAW_DEBUG_MSG(("MWProParser::parseTextZone: bad block id for block %d\n", i));
      entry.setBegin(-1);
    }
  }
  for (int i = 0; i < 2; i++) {
    if (!readTextIds(zone, text->m_ids[i], text->m_textLength, i))
      return true;
  }

  if (!readTextTokens(zone, text->m_tokens, text->m_textLength))
    return true;

  asciiFile.addPos(input->tell());
  asciiFile.addNote("TextZone(end)");

  return true;
}

bool MWProParser::readTextEntries(shared_ptr<MWProParserInternal::Zone> zone,
                                  std::vector<IMWAWEntry> &res, int textLength)
{
  res.resize(0);
  TMWAWInputStreamPtr input = zone->m_input;
  libmwaw_tools::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw_tools::DebugStream f;
  long pos = input->tell();

  int val = input->readULong(2);
  int sz = input->readULong(2);
  if ((sz%6) != 0) {
    MWAW_DEBUG_MSG(("MWProParser::readTextEntries: find an odd size\n"));
    return false;
  }
  long endPos = pos+sz+4;

  int numElt = sz/6;
  f << "TextZone:entry(header),N=" << numElt << ",";
  if (val) f << "unkn=" << val << ",";
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  int remainLength = textLength;
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();
    f.str("");
    f << "TextZone-" << i<<":entry,";
    int unkn = input->readLong(2);
    if (unkn) f << "unkn=" << unkn << ",";
    int bl = input->readLong(2);
    f << "block=" << std::hex << bl << std::dec << ",";
    int nChar = input->readULong(2);
    f << "blockSz=" << nChar;

    if (nChar > remainLength || nChar > 256) {
      MWAW_DEBUG_MSG(("MWProParser::readTextEntries: bad size for block %d\n", i));
      input->seek(pos, WPX_SEEK_SET);
      break;
    }
    remainLength -= nChar;
    bool ok = bl >= 3 && m_state->m_blocksMap.find(bl-1) == m_state->m_blocksMap.end();
    if (!ok) {
      MWAW_DEBUG_MSG(("MWProParser::readTextEntries: bad block id for block %d\n", i));
      input->seek(pos, WPX_SEEK_SET);
      break;
    }

    m_state->m_blocksMap[bl-1] = 0;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    if (nChar==0) continue;

    IMWAWEntry entry;
    entry.setTextId(unkn);
    entry.setBegin((bl-1)*0x100);
    entry.setLength(nChar);
    res.push_back(entry);
  }

  if (remainLength) {
    MWAW_DEBUG_MSG(("MWProParser::readTextEntries: can not find %d characters\n", remainLength));
    asciiFile.addPos(input->tell());
    asciiFile.addNote("TextEntry-#");
  }

  input->seek(endPos, WPX_SEEK_SET);
  return long(input->tell() == endPos) && res.size() != 0;
}

bool MWProParser::readTextIds(shared_ptr<MWProParserInternal::Zone> zone,
                              std::vector<MWProParserInternal::TextZoneData> &res,
                              int textLength, int type)
{
  res.resize(0);
  TMWAWInputStreamPtr input = zone->m_input;
  libmwaw_tools::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw_tools::DebugStream f;
  long pos = input->tell();

  int val = input->readULong(2);
  int sz = input->readULong(2);
  if (sz == 0) {
    asciiFile.addPos(pos);
    asciiFile.addNote("_");
    return true;
  }

  if ((sz%6) != 0) {
    MWAW_DEBUG_MSG(("MWProParser::readTextIds: find an odd size\n"));
    return false;
  }
  long endPos = pos+sz+4;

  int numElt = sz/6;
  f << "TextZone:type=" << type << "(header),N=" << numElt << ",";
  if (val) f << "unkn=" << val << ",";
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  int remainLength = textLength;
  for (int i = 0; i < numElt; i++) {
    MWProParserInternal::TextZoneData data;
    data.m_type = type;
    pos = input->tell();
    data.m_id = input->readLong(2);
    int nChar = data.m_length = input->readULong(4);
    f.str("");
    f << "TextZone-" << i<< ":" << data;

    if (nChar > remainLength) {
      MWAW_DEBUG_MSG(("MWProParser::readTextIds: bad size for block %d\n", i));
      input->seek(pos, WPX_SEEK_SET);
      break;
    }
    remainLength -= nChar;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    if (nChar==0) continue;

    res.push_back(data);
  }

  if (remainLength) {
    MWAW_DEBUG_MSG(("MWProParser::readTextIds: can not find %d characters\n", remainLength));
    asciiFile.addPos(input->tell());
    asciiFile.addNote("TextZone:id-#");
  }

  input->seek(endPos, WPX_SEEK_SET);
  return long(input->tell() == endPos) && res.size() != 0;
}

bool MWProParser::readTextTokens(shared_ptr<MWProParserInternal::Zone> zone,
                                 std::vector<MWProParserInternal::Token> &res,
                                 int textLength)
{
  res.resize(0);
  TMWAWInputStreamPtr input = zone->m_input;
  libmwaw_tools::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw_tools::DebugStream f;
  long pos = input->tell();

  int val = input->readULong(2);
  int sz = input->readULong(2);
  if (sz == 0) {
    asciiFile.addPos(pos);
    asciiFile.addNote("_");
    return true;
  }

  if ((sz%10) != 0) {
    MWAW_DEBUG_MSG(("MWProParser::readTextTokens: find an odd size\n"));
    return false;
  }
  long endPos = pos+sz+4;

  int numElt = sz/10;
  f << "TextZone:token(header),N=" << numElt << ",";
  if (val) f << "unkn=" << val << ",";
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  int remainLength = textLength;
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();

    MWProParserInternal::Token data;
    data.m_type = input->readULong(1);
    data.m_flags[0] = input->readULong(1);
    int nChar = data.m_length = input->readULong(4);
    for (int j = 1; j < 3; j++) data.m_flags[j] = input->readULong(1);
    data.m_blockId = input->readLong(2);
    f.str("");
    f << "TextZone-" << i<< ":token," << data;
    if (nChar > remainLength) {
      MWAW_DEBUG_MSG(("MWProParser::readTextTokens: bad size for block %d\n", i));
      input->seek(pos, WPX_SEEK_SET);
      break;
    }
    remainLength -= nChar;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    res.push_back(data);
    if (data.m_blockId && (data.m_type == 2 || data.m_type == 4))
      m_state->m_blocksCallByTokens.push_back(data.m_blockId);
  }


  input->seek(endPos, WPX_SEEK_SET);
  return long(input->tell() == endPos) && res.size() != 0;
}

////////////////////////////////////////////////////////////
// try to send a empty zone
////////////////////////////////////////////////////////////
bool MWProParser::sendEmptyFrameZone(TMWAWPosition const &pos,
                                     WPXPropertyList extras)
{
  shared_ptr<MWProParserInternal::SubDocument> subdoc
  (new MWProParserInternal::SubDocument(*this, getInput(), -3));
  m_listSubDocuments.push_back(subdoc);
  if (m_listener)
    m_listener->insertTextBox(pos, subdoc, extras);
  return true;
}

////////////////////////////////////////////////////////////
// try to send a text
////////////////////////////////////////////////////////////
bool MWProParser::sendTextZone(int blockId, bool mainZone)
{
  std::map<int, shared_ptr<MWProParserInternal::TextZone> >::iterator it;
  it = m_state->m_textMap.find(blockId);
  if (it == m_state->m_textMap.end()) {
    MWAW_DEBUG_MSG(("MWProParser::sendTextZone: can not find text zone\n"));
    return false;
  }
  sendText(it->second, mainZone);
  return true;
}

bool MWProParser::sendTextBoxZone(int blockId, TMWAWPosition const &pos,
                                  WPXPropertyList extras)
{
  shared_ptr<MWProParserInternal::SubDocument> subdoc
  (new MWProParserInternal::SubDocument(*this, getInput(), blockId));
  m_listSubDocuments.push_back(subdoc);
  if (m_listener)
    m_listener->insertTextBox(pos, subdoc, extras);
  return true;
}

namespace MWProParserInternal
{
struct DataPosition {
  DataPosition(int type=-1, int id=-1, long pos=0) : m_type(type), m_id(id), m_pos(pos) {}
  int m_type;
  int m_id;
  long m_pos;
  struct Compare {
    //! comparaison function
    bool operator()(DataPosition const &p1, DataPosition const &p2) const {
      long diff = p1.m_pos - p2.m_pos;
      if (diff) return (diff < 0);
      diff = p1.m_type - p2.m_type;
      if (diff) return (diff < 0);
      diff = p1.m_id - p2.m_id;
      return (diff < 0);
    }
  };
};
}

bool MWProParser::sendText(shared_ptr<MWProParserInternal::TextZone> zone, bool mainZone)
{
  if (!zone->m_entries.size()) {
    MWAW_DEBUG_MSG(("MWProParser::sendText: can not find the text entries\n"));
    return false;
  }

  MWProStructuresListenerState listenerState(m_structures, mainZone);
  MWProParserInternal::DataPosition::Compare compareFunction;
  std::set<MWProParserInternal::DataPosition, MWProParserInternal::DataPosition::Compare>
  set(compareFunction);
  long cPos = 0;
  for (int i = 0; i < int(zone->m_entries.size()); i++) {
    set.insert(MWProParserInternal::DataPosition(3, i, cPos));
    cPos += zone->m_entries[i].length();
  }
  set.insert(MWProParserInternal::DataPosition(4, 0, cPos));
  cPos = 0;
  for (int i = 0; i < int(zone->m_tokens.size()); i++) {
    cPos += zone->m_tokens[i].m_length;
    set.insert(MWProParserInternal::DataPosition(2, i, cPos));
  }
  for (int id = 0; id < 2; id++) {
    cPos = 0;
    for (int i = 0; i < int(zone->m_ids[id].size()); i++) {
      set.insert(MWProParserInternal::DataPosition(1-id, i, cPos));
      cPos += zone->m_ids[id][i].m_length;
    }
  }
  std::vector<int> pageBreaks=listenerState.getPageBreaksPos();
  for (int i = 0; i < int(pageBreaks.size()); i++)
    set.insert(MWProParserInternal::DataPosition(-1, i, pageBreaks[i]));

  TMWAWInputStreamPtr input = getInput();
  long pos = zone->m_entries[0].begin();
  long asciiPos = pos;
  if (pos > 0)
    input->seek(pos, WPX_SEEK_SET);

  libmwaw_tools::DebugStream f, f2;
  cPos = 0;
  std::set<MWProParserInternal::DataPosition,
      MWProParserInternal::DataPosition::Compare>::const_iterator it;
  bool first = true;
  for (it = set.begin(); it != set.end(); it++) {
    MWProParserInternal::DataPosition const &data = *it;
    long oldPos = pos;
    if (data.m_pos < cPos) {
      MWAW_DEBUG_MSG(("MWProParser::sendText: position go backward, stop...\n"));
      break;
    }
    if (data.m_pos != cPos) {
      if (pos > 0) {
        std::string text("");
        for (int i = cPos; i < data.m_pos; i++) {
          char ch = input->readULong(1);
          if (!ch)
            text+= "#";
          else {
            listenerState.sendChar(ch);
            if (ch > 0 && ch < 20 && ch != 0xd && ch != 0x9) text+="#";
            text+=ch;
          }
        }
        f << "'" << text << "'";
      }

      if (pos > 0 && f.str().length()) {
        f2.str("");
        f2 << "Entries(TextContent):" << f.str();
        f.str("");
        ascii().addPos(asciiPos);
        ascii().addNote(f2.str().c_str());
        pos += (data.m_pos-cPos);
      }

      cPos = data.m_pos;
    }
    switch (data.m_type) {
    case -1:
      listenerState.insertSoftPageBreak();
      break;
    case 4:
    case 3:
      if (pos > 0 && (pos&0xFF))
        ascii().addDelimiter(pos,'|');
      if (data.m_type == 3) {
        pos = zone->m_entries[data.m_id].begin();
        if (pos > 0)
          input->seek(pos, WPX_SEEK_SET);
      }
      break;
    case 2: {
      // save the position because we read some extra data ( footnote, table, textbox)
      long actPos = input->tell();
      switch (zone->m_tokens[data.m_id].m_type) {
      case 1:
        if (m_listener) m_listener->insertField(IMWAWContentListener::PageNumber);
        break;
      case 2:
        if (listenerState.isSent(zone->m_tokens[data.m_id].m_blockId)) {
          MWAW_DEBUG_MSG(("MWProParser::sendText: footnote is already sent...\n"));
        } else {
          IMWAWSubDocumentPtr subdoc(new MWProParserInternal::SubDocument(*this, getInput(), zone->m_tokens[data.m_id].m_blockId));
          m_listener->insertNote(FOOTNOTE, subdoc);
        }
        break;
      case 3:
        break; // footnote content, ok
      case 4:
        listenerState.send(zone->m_tokens[data.m_id].m_blockId);
        listenerState.resendAll();
        break;
      case 5:
        break; // hyphen ok
      case 6:
        if (m_listener) m_listener->insertField(IMWAWContentListener::Date);
        break;
      case 7:
        if (m_listener) m_listener->insertField(IMWAWContentListener::Time);
        break;
      case 8:
        if (m_listener) m_listener->insertField(IMWAWContentListener::Title);
        break;
      case 9:
        if (m_listener) m_listener->insertUnicodeString("#REVISION#");
        break;
      case 10:
        if (m_listener) {
          int numSection = listenerState.numSection()+1;
          std::stringstream s;
          s << numSection;
          m_listener->insertUnicodeString(s.str().c_str());
        }
        break;
      default:
        break;
      }
      f << "token[" << zone->m_tokens[data.m_id] << "],";
      input->seek(actPos, WPX_SEEK_SET);
      break;
    }
    case 1:
      if (m_structures) {
        listenerState.sendFont(zone->m_ids[0][data.m_id].m_id, first);
        first = false;
        f << "[" << listenerState.getFontDebugString(zone->m_ids[0][data.m_id].m_id) << "],";
      } else
        f << "[" << zone->m_ids[0][data.m_id] << "],";
      break;
    case 0:
      if (m_structures) {
        listenerState.sendParagraph(zone->m_ids[1][data.m_id].m_id);
        f << "[" << listenerState.getParagraphDebugString(zone->m_ids[1][data.m_id].m_id) << "],";
      } else
        f << "[" << zone->m_ids[1][data.m_id] << "],";
      break;
    default: {
      static bool firstError = true;
      if (firstError) {
        MWAW_DEBUG_MSG(("MWProParser::sendText: find unexpected data type...\n"));
        firstError = false;
      }
      f << "#";
      break;
    }

    }
    if (pos >= 0 && pos != oldPos)
      asciiPos = pos;
  }

  return true;
}


////////////////////////////////////////////////////////////
// try to send a picture
////////////////////////////////////////////////////////////
bool MWProParser::sendPictureZone(int blockId, TMWAWPosition const &pictPos,
                                  WPXPropertyList extras)
{
  std::map<int, shared_ptr<MWProParserInternal::Zone> >::iterator it;
  it = m_state->m_dataMap.find(blockId);
  if (it == m_state->m_dataMap.end()) {
    MWAW_DEBUG_MSG(("MWProParser::sendPictureZone: can not find picture zone\n"));
    return false;
  }
  sendPicture(it->second, pictPos, extras);
  return true;
}

bool MWProParser::sendPicture(shared_ptr<MWProParserInternal::Zone> zone,
                              TMWAWPosition pictPos,
                              WPXPropertyList const &extras)
{
  if (!zone) return false;
  if (zone->m_type != 1) {
    MWAW_DEBUG_MSG(("MWProParser::sendPicture: not a picture date\n"));
    return false;
  }

  zone->m_parsed = true;

  // ok init is done
  TMWAWInputStreamPtr input = zone->m_input;
  libmwaw_tools::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw_tools::DebugStream f;

  f << "Entries(PICT),";
  asciiFile.addPos(0);
  asciiFile.addNote(f.str().c_str());

  input->seek(0, WPX_SEEK_SET);
  long pictSize = input->readULong(4);
  if (pictSize < 10 || pictSize > long(zone->m_data.size())) {
    MWAW_DEBUG_MSG(("MWProParser::sendPicture: oops a pb with pictSize\n"));
    asciiFile.addPos(4);
    asciiFile.addNote("#PICT");
    return false;
  }
  shared_ptr<libmwaw_tools::Pict> pict
  (libmwaw_tools::PictData::get(input, pictSize));
  if (!pict) {
    // sometimes this just fails because the pictSize is not correct
    input->seek(14, WPX_SEEK_SET);
    if (input->readULong(2) == 0x1101) { // try to force the size to be ok
      WPXBinaryData data;
      input->seek(0, WPX_SEEK_SET);
      input->readDataBlock(4+pictSize, data);
      unsigned char *dataPtr=const_cast<unsigned char *>(data.getDataBuffer());

      dataPtr[4]=dataPtr[2];
      dataPtr[5]=dataPtr[3];

      WPXInputStream *dataInput =
        const_cast<WPXInputStream *>(data.getDataStream());
      TMWAWInputStreamPtr input(new TMWAWInputStream(dataInput, false));
      input->setResponsable(false);

      input->seek(4, WPX_SEEK_SET);
      pict.reset(libmwaw_tools::PictData::get(input, pictSize));
    }
  }

#ifdef DEBUG_WITH_FILES
  asciiFile.skipZone(4, 4+pictSize-1);
  WPXBinaryData file;
  input->seek(4, WPX_SEEK_SET);
  input->readDataBlock(pictSize, file);
  static int volatile pictName = 0;
  f.str("");
  f << "PICT-" << ++pictName;
  libmwaw_tools::Debug::dumpFile(file, f.str().c_str());
  asciiFile.addPos(4+pictSize);
  asciiFile.addNote("PICT(end)");
#endif

  if (!pict) { // ok, we can not do anything except sending the data...
    MWAW_DEBUG_MSG(("MWProParser::parseDataZone: no sure this is a picture\n"));
    if (pictPos.size().x() <= 0 || pictPos.size().y() <= 0)
      pictPos=TMWAWPosition(Vec2f(0,0),Vec2f(100.,100.), WPX_POINT);
    if (m_listener) {
      WPXBinaryData data;
      input->seek(4, WPX_SEEK_SET);
      input->readDataBlock(pictSize, data);
      m_listener->insertPicture(pictPos, data, "image/pict", extras);
    }
    return true;
  }

  if (pictPos.size().x() <= 0 || pictPos.size().y() <= 0) {
    pictPos.setOrigin(Vec2f(0,0));
    pictPos.setSize(pict->getBdBox().size());
    pictPos.setUnit(WPX_POINT);
  }
  if (pict->getBdBox().size().x() > 0 && pict->getBdBox().size().y() > 0)
    pictPos.setNaturalSize(pict->getBdBox().size());

  if (m_listener) {
    WPXBinaryData data;
    std::string type;
    if (pict->getBinary(data,type))
      m_listener->insertPicture(pictPos, data, type, extras);
  }
  return true;
}

////////////////////////////////////////////////////////////
// some debug functions
////////////////////////////////////////////////////////////

#ifdef DEBUG
void MWProParser::saveOriginal(TMWAWInputStreamPtr input)
{
  libmwaw_tools::DebugStream f;

  libmwaw_tools::DebugFile orig;
  orig.setStream(input);
  orig.open("orig");
  int bl = 0;
  while(1) {
    long pos = bl*0x100;
    input->seek(pos, WPX_SEEK_SET);
    if (long(input->tell()) != pos)
      break;
    f.str("");
    int val = 0;
    if (bl) {
      input->seek(-4, WPX_SEEK_CUR);
      val = input->readLong(4);
      int next = val > 0 ? val : -val;
      if (next > 0 && next < 0x1000) {
        f << "next=" << std::hex << (next-1)*0x100 << std::dec << ",";
        if (val < 0)
          f << "N=?" << input->readLong(4) << ",";
        orig.addPos(pos-4);
        orig.addNote(f.str().c_str());
      }
    }
    orig.addPos(input->tell());

    if (m_state->m_blocksMap.find(bl) == m_state->m_blocksMap.end())
      orig.addNote("unparsed*");
    else {
      f.str("");
      f << std::hex << "(" << m_state->m_blocksMap.find(bl)->second << ")"
        << std::dec;
      orig.addNote(f.str().c_str());
    }
    bl++;
  }
  orig.reset();
}
#endif

void MWProParser::checkUnparsed()
{
  TMWAWInputStreamPtr input = getInput();
  libmwaw_tools::DebugStream f;

  long pos;
  std::stringstream notParsed;
  for (int bl = 3; bl < 1000; bl++) {
    if (m_state->m_blocksMap.find(bl) != m_state->m_blocksMap.end())
      continue;

    pos = bl*0x100;
    input->seek(pos, WPX_SEEK_SET);
    if (input->atEOS()) break;
    notParsed << std::hex <<  bl << std::dec << ",";

    // normaly there must remains only text entry...
    f.str("");
    f << "Entries(Unparsed):";

    std::string text("");
    bool findZero = false;
    for (int c = 0; c < 256; c++) {
      char ch = input->readULong(1);
      if (!ch) {
        if (findZero) {
          input->seek(-1, WPX_SEEK_CUR);
          break;
        }
        findZero = true;
        continue;
      }
      if (findZero) {
        text += "#";
        findZero = false;
      }
      text+=ch;
    }
    f << text;
    if (long(input->tell()) != pos+256)
      ascii().addDelimiter(input->tell(),'|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  if (notParsed.str().size()) {
    MWAW_DEBUG_MSG(("MWProParser::checkUnparsed: not parsed %s\n", notParsed.str().c_str()));
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
