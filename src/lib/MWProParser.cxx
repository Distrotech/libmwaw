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
#include "MWAWHeader.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSubDocument.hxx"

#include "MWProStructures.hxx"

#include "MWProParser.hxx"

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
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

  //! the type : 0(text), 1(graphic)
  int m_type;

  //! the first block id
  int m_blockId;

  //! the storage
  WPXBinaryData m_data;

  //! the main input
  MWAWInputStreamPtr m_input;

  //! the debug file
  libmwaw::DebugFile m_asciiFile;

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
  Token() : m_type(-1), m_length(0), m_blockId(-1), m_box() {
    for (int i = 0; i < 4; i++) m_flags[i] = 0;
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
    for (int i = 0; i < 4; i++) {
      if (tkn.m_flags[i]) o << "fl" << i << "=" << std::hex << tkn.m_flags[i] << ",";
    }
    return o;
  }
  //! the type
  int m_type;
  //! the text length
  int m_length;
  //! the block id
  int m_blockId;
  //! the bdbox ( filled in MWII for figure)
  Box2f m_box;
  //! some flags
  int m_flags[4];
};

//! Internal: a struct used to store a text zone
struct TextZone {
  TextZone() : m_textLength(0), m_entries(), m_tokens(), m_parsed(false) {
  }

  //! the text length
  int m_textLength;

  //! the list of entries
  std::vector<MWAWEntry> m_entries;

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
  State() : m_blocksMap(), m_dataMap(), m_textMap(),
    m_blocksCallByTokens(), m_col(1), m_actPage(0), m_numPages(0),
    m_headerHeight(0), m_footerHeight(0) {
  }

  //! the list of retrieved block : block -> new address
  std::map<int,long> m_blocksMap;

  //! the list of blockId->data zone
  std::map<int, shared_ptr<Zone> > m_dataMap;

  //! the list of blockId->text zone
  std::map<int, shared_ptr<TextZone> > m_textMap;

  //! the list of blockId called by tokens
  std::vector<int> m_blocksCallByTokens;

  int m_col /** the number of columns in MWII */;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MWProParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MWProParser &pars, MWAWInputStreamPtr input, int zoneId) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

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
  //! the subdocument id
  int m_id;
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (m_id == -3) return; // empty block
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_parser);

  long pos = m_input->tell();
  MWProParser *parser = reinterpret_cast<MWProParser *>(m_parser);
  if (parser->m_structures.get())
    parser->m_structures->send(m_id);
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_id != sDoc->m_id) return true;
  return false;
}
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MWProParser::MWProParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_state(), m_structures()
{
  init();
}

MWProParser::~MWProParser()
{
}

void MWProParser::init()
{
  resetListener();
  setAsciiName("main-1");

  m_state.reset(new MWProParserInternal::State);
  m_structures.reset(new MWProStructures(*this));

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
int MWProParser::numColumns() const
{
  if (m_state->m_col <= 1) return 1;
  return m_state->m_col;
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
    if (!getListener() || m_state->m_actPage == 1)
      continue;
    if (softBreak)
      getListener()->insertBreak(MWAWContentListener::SoftPageBreak);
    else
      getListener()->insertBreak(MWAWContentListener::PageBreak);
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

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
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
      for (size_t i = 1; i < freeList.size(); i++) {
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

  resetListener();
  if (!ok) throw(libmwaw::ParseException());
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
  MWAWInputStreamPtr input = getInput();
  input->seek((blockId-1)*0x100, WPX_SEEK_SET);

  unsigned long read;
  long first=blockId-1, last=blockId-1;
  int const linkSz = version()<= 0 ? 2 : 4;
  while (!input->atEOS()) {
    bool ok = true;
    for(long i=first; i<= last; i++) {
      if (m_state->m_blocksMap.find((int)i) != m_state->m_blocksMap.end()) {
        MWAW_DEBUG_MSG(("MWProParser::getZoneData: block %ld already seems\n", i));
        ok = false;
        break;
      }
      m_state->m_blocksMap[(int)i] = (long)data.size()+(i-first)*0x100;
    }
    if (!ok) break;
    long endPos = (last+1)*0x100 - linkSz;
    long pos = input->tell();
    input->seek(endPos, WPX_SEEK_SET);
    long limit = input->tell();
    if (limit <= pos) break;
    input->seek(pos, WPX_SEEK_SET);

    const unsigned char *dt = input->read(size_t(limit-pos), read);
    data.append(dt, read);
    ascii().skipZone(first*0x100, (last+1)*0x100-1);

    if (long(read) != limit-pos) {
      MWAW_DEBUG_MSG(("MWProParser::getZoneData: can not read all data\n"));
      break;
    }
    if (limit < endPos)
      break;
    input->seek(limit, WPX_SEEK_SET);

    long act = last;
    long val = input->readLong(linkSz);
    if (val == 0) break;
    if (val < 0)
      first = -val-1;
    else
      first = val-1;
    last = first;

    if (first != act+1) {
      input->seek(first*0x100, WPX_SEEK_SET);
      if (long(input->tell()) != first*0x100) {
        MWAW_DEBUG_MSG(("MWProParser::getZoneData: can not go to %ld block\n", first));
        break;
      }
    }
    if (val < 0) {
      long num = (long) input->readULong(linkSz);
      last = first+(num-1);
    }
    if (last-first > 2) {
      pos = input->tell();
      input->seek((last-1)*0x100, WPX_SEEK_SET);
      if (long(input->tell()) != (last-1)*0x100) {
        MWAW_DEBUG_MSG(("MWProParser::getZoneData: num %ld if odd\n", last));
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
  MWAWInputStreamPtr input = getInput();

  long first=blockId-1, last=blockId-1;
  while (1) {
    bool ok = true;
    for(long i=first; i<= last; i++) {
      if (m_state->m_blocksMap.find((int)i) != m_state->m_blocksMap.end()) {
        MWAW_DEBUG_MSG(("MWProParser::getFreeZoneList: block %ld already seems\n", i));
        ok = false;
        break;
      }
      blockLists.push_back((int)i);
      m_state->m_blocksMap[(int)i] = 0;
    }
    if (!ok) break;
    long endPos = (last+1)*0x100 - 4;
    input->seek(endPos, WPX_SEEK_SET);
    if (long(input->tell()) != endPos) break;


    long act = last;
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
          MWAW_DEBUG_MSG(("MWProParser::getFreeZoneList: can not go to %ld block\n", first));
          break;
        }
      }

      long num = (long) input->readULong(4);
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
  if (getListener()) {
    MWAW_DEBUG_MSG(("MWProParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;
  int numPages = m_structures ? m_structures->numPages() : 0;
  if (numPages <= 0) numPages = 1;
  m_state->m_numPages = numPages;

  // create the page list
  std::vector<MWAWPageSpan> pageList;

  int actHeaderId = 0, actFooterId = 0;
  shared_ptr<MWProParserInternal::SubDocument> headerSubdoc, footerSubdoc;
  for (int i = 0; i < m_state->m_numPages; ) {
    int numSim[2]= {1,1};
    int headerId =  m_structures->getHeaderId(i+1, numSim[0]);
    if (headerId != actHeaderId) {
      actHeaderId = headerId;
      if (actHeaderId == 0)
        headerSubdoc.reset();
      else
        headerSubdoc.reset
        (new MWProParserInternal::SubDocument(*this, getInput(), headerId));
    }
    int footerId =  m_structures->getFooterId(i+1, numSim[1]);
    if (footerId != actFooterId) {
      actFooterId = footerId;
      if (actFooterId == 0)
        footerSubdoc.reset();
      else
        footerSubdoc.reset
        (new MWProParserInternal::SubDocument(*this, getInput(), footerId));
    }

    MWAWPageSpan ps(getPageSpan());
    if (headerSubdoc) {
      MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
      header.m_subDocument=headerSubdoc;
      ps.setHeaderFooter(header);
    }
    if (footerSubdoc) {
      MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
      footer.m_subDocument=footerSubdoc;
      ps.setHeaderFooter(footer);
    }
    if (numSim[1] < numSim[0]) numSim[0]=numSim[1];
    if (numSim[0] < 1) numSim[0]=1;
    ps.setPageSpan(numSim[0]);
    i+=numSim[0];
    pageList.push_back(ps);
  }

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
bool MWProParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
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
bool MWProParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MWProParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;

  libmwaw::DebugStream f;
  int headerSize=4;
  input->seek(headerSize+0x78,WPX_SEEK_SET);
  if (int(input->tell()) != headerSize+0x78) {
    MWAW_DEBUG_MSG(("MWProParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,WPX_SEEK_SET);

  int vers = (int) input->readULong(2);
  int val = (int) input->readULong(2);

  f << "FileHeader:";
  switch (vers) {
  case 0x2e:
    vers = 0;
    if (val != 0x2e)
      return false;
    break;
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
  setVersion(vers);
  f << "vers=" << vers << ",";
  if (strict) {
    if (!readPrintInfo())
      return false;
    if (vers) {
      input->seek(0xdd, WPX_SEEK_SET);
      // "MP" seems always in this position
      if (input->readULong(2) != 0x4d50)
        return false;
    }
  }


  // ok, we can finish initialization
  if (header)
    header->reset(MWAWDocument::MWAW_T_MACWRITEPRO, version());

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
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;
  // print info
  libmwaw::PrinterInfo info;
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

  getPageSpan().setMarginTop(lTopMargin.y()/72.0);
  getPageSpan().setMarginBottom(botMarg/72.0);
  getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
  getPageSpan().setMarginRight(rightMarg/72.0);
  getPageSpan().setFormLength(paperSize.y()/72.);
  getPageSpan().setFormWidth(paperSize.x()/72.);

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
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;

  f << "Entries(Data0):";
  long val;
  if (version()==0) {
    val = input->readLong(2); // always 0 ?
    if (val) f << "f0=" << val << ",";
    /* fl0=[2|6|82|86], fl1=[80|a0|a4], other 0|1|-1 */
    for (int i = 0; i < 9; i++) {
      val = (i<2) ? int(input->readULong(1)) : input->readLong(1);
      if (!val) continue;
      if (i < 2)
        f << "fl" << i << "=" << std::hex << val << std::dec << ",";
      else
        f << "fl" << i << "=" << val << ",";
    }
    val = input->readLong(2); // always 612 ?
    if (val != 0x612) f << "f1=" << val << ",";
    val = input->readLong(1); // always 1 ?
    if (val != 1) f << "f2=" << val << ",";
    val = input->readLong(2); // always 2 ?
    if (val != 2) f << "f3=" << val << ",";
    val = input->readLong(2); // always 12c ?
    if (val != 0x12c) f << "f4=" << val << ",";
    for (int i = 0; i < 4; i++) { // 0, 0, 3c, a small number
      val = input->readLong(2);
      if (val) f << "g" << i << "=" << val << ",";
    }
    /* then
      0009000020000000fd803333000600000000000120 |
      000c000020000000fd803333000600000000000180 |
      000c000020000000fd8033330006000000000001a0 |
      000c0000200e0000fd8033330006000000000001a0 |
      00240000200e0000fd8033330006000000000001a0

      and
      000001000000016f66000000000000000800090001000000
     */
  } else {
    val = input->readLong(1); // always 0 ?
    if (val) f << "unkn=" << val << ",";
    int N=(int) input->readLong(2); // find 2, a, 9e, 1a
    f << "N?=" << N << ",";
    N = (int) input->readLong(1); // almost always 0, find one time 6 ?
    if (N) f << "N1?=" << N << ",";
    val = (int) input->readLong(2); // almost always 0x622, find also 0 and 12
    f << "f0=" << std::hex << val << std::dec << ",";
    val = (int) input->readLong(1); // always 0 ?
    if (val) f << "unkn1=" << val << ",";
    N = (int) input->readLong(2);
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
    val = (long) input->readULong(1); // always 0 ?
    if (val) f << "f11=" << val << ",";
  }
  float dim[6];
  for (int i = 0; i < 6; i++)
    dim[i] = float(input->readLong(4))/65356.f;
  bool ok = true;
  for (int i = 0; i < 6; i++) {
    if (dim[i] < 0) ok = false;
  }
  if (ok) ok = dim[0] > dim[2]+dim[3] && dim[1] > dim[4]+dim[5];

  if (ok) {
    getPageSpan().setMarginTop(dim[2]/72.0);
    getPageSpan().setMarginLeft(dim[4]/72.0);
    /* decrease a little the right/bottom margin to allow fonts discrepancy*/
    getPageSpan().setMarginBottom((dim[3]<36.0) ? 0.0 : dim[3]/72.0-0.5);
    getPageSpan().setMarginRight((dim[5]<18.0) ? 0.0 : dim[5]/72.0-0.25);
    getPageSpan().setFormLength(dim[0]/72.);
    getPageSpan().setFormWidth(dim[1]/72.);
  } else {
    MWAW_DEBUG_MSG(("MWProParser::readDocHeader: find odd page dimensions, ignored\n"));
    f << "#";
  }
  f << "dim=" << dim[1] << "x" << dim[0] << ",";
  f << "margins=["; // top, bottom, left, right
  for (int i = 2; i < 6; i++) f << dim[i] << ",";
  f << "],";
  if (version()==0) {
    m_state->m_col = (int) input->readLong(2);
    if (m_state->m_col != 1) f << "col=" << m_state->m_col << ",";
  }

  ascii().addDelimiter(input->tell(), '|');
  /** then find
      000000fd0000000000018200000100002f00
      44[40|80] followed by something like a7c3ec07|a7c4c3c6 : 2 ptrs ?
      6f6600000000000000080009000105050506010401
   */
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  f.str("");
  f << "Data0-A:";
  if (version()==0) {
    input->seek(pos+120, WPX_SEEK_SET);
    pos = input->tell();
    for (int i = 0; i < 3; i++) { // f0=f1, f2=0
      val = (long) input->readULong(4);
      if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
    }
  } else {
    input->seek(pos+97, WPX_SEEK_SET);
    pos = input->tell();
    val = (long) input->readULong(2);
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
    val = (long) input->readULong(2);
    if (val) f << "g"  << i << "=" << std::hex << val << std::dec << ",";
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
  zone->m_input=MWAWInputStream::get(zone->m_data, false);
  if (!zone->m_input)
    return false;

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
    libmwaw::DebugStream f;
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

  MWAWInputStreamPtr input = zone->m_input;
  MWAWInputStreamPtr fileInput = getInput();
  libmwaw::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw::DebugStream f;

  shared_ptr<MWProParserInternal::TextZone> text(new MWProParserInternal::TextZone);

  long pos = 0;
  input->seek(pos, WPX_SEEK_SET);
  f << "Entries(TextZone):";
  text->m_textLength = (int)input->readLong(4);
  f << "textLength=" << text->m_textLength << ",";

  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  if (!readTextEntries(zone, text->m_entries, text->m_textLength))
    return false;
  m_state->m_textMap[zone->m_blockId] = text;

  for (size_t i = 0; i < text->m_entries.size(); i++) {
    MWAWEntry &entry = text->m_entries[i];
    fileInput->seek(entry.begin(), WPX_SEEK_SET);
    if (long(fileInput->tell()) != entry.begin()) {
      MWAW_DEBUG_MSG(("MWProParser::parseTextZone: bad block id for block %ld\n", long(i)));
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
                                  std::vector<MWAWEntry> &res, int textLength)
{
  res.resize(0);
  int vers = version();
  int expectedSize = vers == 0 ? 4 : 6;
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw::DebugStream f;
  long pos = input->tell();

  int val = (int) input->readULong(2);
  int sz = (int) input->readULong(2);
  if ((sz%expectedSize) != 0) {
    MWAW_DEBUG_MSG(("MWProParser::readTextEntries: find an odd size\n"));
    return false;
  }
  long endPos = pos+sz+4;

  int numElt = sz/expectedSize;
  f << "TextZone:entry(header),N=" << numElt << ",";
  if (val) f << "unkn=" << val << ",";
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  int remainLength = textLength;
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();
    f.str("");
    f << "TextZone-" << i<<":entry,";
    int unkn = 0;
    if (vers >= 1) {
      unkn = (int) input->readLong(2);
      if (unkn) f << "unkn=" << unkn << ",";
    }
    int bl = (int) input->readLong(2);
    f << "block=" << std::hex << bl << std::dec << ",";
    int nChar = (int) input->readULong(2);
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

    MWAWEntry entry;
    entry.setId(unkn);
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
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw::DebugStream f;
  long pos = input->tell();

  int val = (int) input->readULong(2);
  int sz = (int) input->readULong(2);
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

  long remainLength = textLength;
  for (int i = 0; i < numElt; i++) {
    MWProParserInternal::TextZoneData data;
    data.m_type = type;
    pos = input->tell();
    data.m_id = (int) input->readLong(2);
    long nChar = (long) input->readULong(4);
    data.m_length = (int) nChar;
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
    MWAW_DEBUG_MSG(("MWProParser::readTextIds: can not find %ld characters\n", remainLength));
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
  int vers = version();
  int expectedSz = vers==0 ? 8 : 10;
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw::DebugStream f;
  long pos = input->tell();

  int val = (int) input->readULong(2);
  if (val && vers == 0) {
    input->seek(pos, WPX_SEEK_SET);
    asciiFile.addPos(pos);
    asciiFile.addNote("_");
    return true;
  }
  long sz = (int) input->readULong(2);
  if (sz == 0) {
    asciiFile.addPos(pos);
    asciiFile.addNote("_");
    return true;
  }

  if ((sz%expectedSz) != 0) {
    MWAW_DEBUG_MSG(("MWProParser::readTextTokens: find an odd size\n"));
    return false;
  }
  long endPos = pos+sz+4;

  int numElt = int(sz/expectedSz);
  f << "TextZone:token(header),N=" << numElt << ",";
  if (val) f << "unkn=" << val << ",";
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  long remainLength = textLength;
  int numFootnotes = 0;
  std::vector<int> pictPos;
  for (int i = 0; i < numElt; i++) {
    f.str("");
    pos = input->tell();

    MWProParserInternal::Token data;
    data.m_type = (int) input->readULong(1);
    if (vers==0) { // check me
      switch(data.m_type) {
      case 2:  // page number
        data.m_type=1;
        break;
      case 3:  // footnote content
        break;
      case 4: // figure
        break;
      case 5: // footnote pos
        data.m_type=2;
        data.m_blockId = ++numFootnotes; // for MW2
        break;
      case 0x15: // Fixme: must find other date
      case 0x17: // date alpha
        data.m_type=6;
        break;
      case 0x1a: // time
        data.m_type=7;
        break;
      default:
        MWAW_DEBUG_MSG(("MWProParser::readTextTokens: unknown block type %d\n", data.m_type));
        f << "#type=" << data.m_type << ",";
        data.m_type = -1;
        break;
      }
    }
    data.m_flags[0] = (int) input->readULong(1);
    long nChar = (long) input->readULong(vers == 0 ? 2 : 4);
    data.m_length = (int) nChar;
    for (int j = 1; j < 3; j++) data.m_flags[j] = (int) input->readULong(1);
    if (vers == 0)
      data.m_flags[3] = (int) input->readULong(2);
    else
      data.m_blockId = (int) input->readULong(2);
    f << "TextZone-" << i<< ":token," << data;
    if (nChar > remainLength) {
      MWAW_DEBUG_MSG(("MWProParser::readTextTokens: bad size for block %d\n", i));
      input->seek(pos, WPX_SEEK_SET);
      break;
    }
    remainLength -= nChar;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    if (data.m_type == 4) pictPos.push_back((int)res.size());
    res.push_back(data);

    if (vers == 1 && data.m_blockId && (data.m_type == 2 || data.m_type == 4))
      m_state->m_blocksCallByTokens.push_back(data.m_blockId);
  }
  input->seek(endPos, WPX_SEEK_SET);
  if (vers == 0 && pictPos.size()) {
    size_t numPict = pictPos.size();
    // checkme always inverted ?
    for (size_t i = numPict; i > 0; i--) {
      MWProParserInternal::Token &token = res[(size_t) pictPos[i-1]];
      pos = input->tell();
      f.str("");
      f << "TextZone-pict" << i-1<< ":";
      val = (int) input->readLong(2);
      if (val) f << "unkn=" << val << ",";
      int blockId = (int) input->readULong(2);
      if (blockId) {
        token.m_blockId = blockId;
        f << "block=" << blockId << ",";
        parseDataZone(blockId,1);
      }
      sz = (long) input->readULong(4);
      f << "sz=" << std::hex << sz << std::dec << ",";
      int dim[4];
      for (int j = 0; j < 4; j++) dim[j] = (int) input->readLong(2);
      token.m_box = Box2f(Vec2f(float(dim[1]),float(dim[0])), Vec2f(float(dim[3]),float(dim[2])));
      f << "dim=[" << dim[1] << "x" << dim[0] << "-"
        << dim[3] << "x" << dim[2] << ",";
      for (int j = 0; j < 4; j++) dim[j] = (int) input->readLong(2);
      f << "dim2=[" << dim[1] << "x" << dim[0] << "-"
        << dim[3] << "x" << dim[2] << ",";
      // followed by junk ?
      ascii().addDelimiter(input->tell(),'|');
      input->seek(pos+62, WPX_SEEK_SET);
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
    }
  }

  return long(input->tell() == endPos) && res.size() != 0;
}

////////////////////////////////////////////////////////////
// try to send a empty zone
////////////////////////////////////////////////////////////
bool MWProParser::sendEmptyFrameZone(MWAWPosition const &pos,
                                     WPXPropertyList extras)
{
  shared_ptr<MWProParserInternal::SubDocument> subdoc
  (new MWProParserInternal::SubDocument(*this, getInput(), -3));
  if (getListener())
    getListener()->insertTextBox(pos, subdoc, extras);
  return true;
}

////////////////////////////////////////////////////////////
// try to send a text
////////////////////////////////////////////////////////////
int MWProParser::findNumHardBreaks(int blockId)
{
  std::map<int, shared_ptr<MWProParserInternal::TextZone> >::iterator it;
  it = m_state->m_textMap.find(blockId);
  if (it == m_state->m_textMap.end()) {
    MWAW_DEBUG_MSG(("MWProParser::findNumHardBreaks: can not find text zone\n"));
    return 0;
  }
  return findNumHardBreaks(it->second);
}

int MWProParser::findNumHardBreaks(shared_ptr<MWProParserInternal::TextZone> zone)
{
  if (!zone->m_entries.size()) return 0;
  int num = 0;
  MWAWInputStreamPtr input = getInput();
  for (size_t i = 0; i < zone->m_entries.size(); i++) {
    MWAWEntry const &entry = zone->m_entries[i];
    input->seek(entry.begin(), WPX_SEEK_SET);
    for (int j = 0; j < entry.length(); j++) {
      switch(input->readULong(1)) {
      case 0xc: // hard page
      case 0xb: // difficult to differentiate column/page break so...
        num++;
        break;
      default:
        break;
      }
    }
  }
  return num;
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

bool MWProParser::sendTextBoxZone(int blockId, MWAWPosition const &pos,
                                  WPXPropertyList extras)
{
  shared_ptr<MWProParserInternal::SubDocument> subdoc
  (new MWProParserInternal::SubDocument(*this, getInput(), blockId));
  if (getListener())
    getListener()->insertTextBox(pos, subdoc, extras);
  return true;
}

namespace MWProParserInternal
{
/** Internal and low level: structure used to sort the position of data */
struct DataPosition {
  //! constructor
  DataPosition(int type=-1, int id=-1, long pos=0) : m_type(type), m_id(id), m_pos(pos) {}
  //! the type
  int m_type;
  //! an id
  int m_id;
  //! the position
  long m_pos;
  //! the comparison structure
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
  int vers = version();
  MWProStructuresListenerState listenerState(m_structures, mainZone);
  MWProParserInternal::DataPosition::Compare compareFunction;
  std::set<MWProParserInternal::DataPosition, MWProParserInternal::DataPosition::Compare>
  set(compareFunction);
  long cPos = 0;
  for (size_t i = 0; i < zone->m_entries.size(); i++) {
    set.insert(MWProParserInternal::DataPosition(3, (int) i, cPos));
    cPos += zone->m_entries[i].length();
  }
  set.insert(MWProParserInternal::DataPosition(4, 0, cPos));
  cPos = 0;
  for (size_t i = 0; i < zone->m_tokens.size(); i++) {
    cPos += zone->m_tokens[i].m_length;
    set.insert(MWProParserInternal::DataPosition(2, (int) i, cPos));
  }
  for (int id = 0; id < 2; id++) {
    cPos = 0;
    for (size_t i = 0; i < zone->m_ids[id].size(); i++) {
      set.insert(MWProParserInternal::DataPosition(1-id, (int) i, cPos));
      cPos += zone->m_ids[id][i].m_length;
    }
  }
  std::vector<int> pageBreaks=listenerState.getPageBreaksPos();
  for (size_t i = 0; i < pageBreaks.size(); i++) {
    if (pageBreaks[i] >= zone->m_textLength) {
      MWAW_DEBUG_MSG(("MWProParser::sendText: page breaks seems bad\n"));
      break;
    }
    set.insert(MWProParserInternal::DataPosition(-1, (int) i, pageBreaks[i]));
  }

  MWAWInputStreamPtr input = getInput();
  long pos = zone->m_entries[0].begin();
  long asciiPos = pos;
  if (pos > 0)
    input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f, f2;
  cPos = 0;
  std::set<MWProParserInternal::DataPosition,
      MWProParserInternal::DataPosition::Compare>::const_iterator it;
  for (it = set.begin(); it != set.end(); ++it) {
    MWProParserInternal::DataPosition const &data = *it;
    long oldPos = pos;
    if (data.m_pos < cPos) {
      MWAW_DEBUG_MSG(("MWProParser::sendText: position go backward, stop...\n"));
      break;
    }
    if (data.m_pos != cPos) {
      if (pos > 0) {
        std::string text("");
        for (long i = cPos; i < data.m_pos; i++) {
          char ch = (char) input->readULong(1);
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
        pos = zone->m_entries[(size_t)data.m_id].begin();
        if (pos > 0)
          input->seek(pos, WPX_SEEK_SET);
      }
      break;
    case 2: {
      // save the position because we read some extra data ( footnote, table, textbox)
      long actPos = input->tell();
      switch (zone->m_tokens[(size_t)data.m_id].m_type) {
      case 1:
        if (getListener()) getListener()->insertField(MWAWField(MWAWField::PageNumber));
        break;
      case 2:
        if (vers == 1 && listenerState.isSent(zone->m_tokens[(size_t)data.m_id].m_blockId)) {
          MWAW_DEBUG_MSG(("MWProParser::sendText: footnote is already sent...\n"));
        } else {
          int id = zone->m_tokens[(size_t)data.m_id].m_blockId;
          if (vers == 0) id = -id;
          MWAWSubDocumentPtr subdoc(new MWProParserInternal::SubDocument(*this, getInput(), id));
          getListener()->insertNote(MWAWNote(MWAWNote::FootNote), subdoc);
        }
        break;
      case 3:
        break; // footnote content, ok
      case 4:
        if (vers==0) {
          MWAWPosition pictPos(Vec2i(0,0), zone->m_tokens[(size_t)data.m_id].m_box.size(), WPX_POINT);
          pictPos.setRelativePosition(MWAWPosition::Char, MWAWPosition::XLeft, MWAWPosition::YBottom);
          sendPictureZone(zone->m_tokens[(size_t)data.m_id].m_blockId, pictPos);
        } else {
          listenerState.send(zone->m_tokens[(size_t)data.m_id].m_blockId);
          listenerState.resendAll();
        }
        break;
      case 5:
        break; // hyphen ok
      case 6:
        if (getListener()) getListener()->insertField(MWAWField(MWAWField::Date));
        break;
      case 7:
        if (getListener()) getListener()->insertField(MWAWField(MWAWField::Time));
        break;
      case 8:
        if (getListener()) getListener()->insertField(MWAWField(MWAWField::Title));
        break;
      case 9:
        if (getListener()) getListener()->insertUnicodeString("#REVISION#");
        break;
      case 10:
        if (getListener()) {
          int numSection = listenerState.numSection()+1;
          std::stringstream s;
          s << numSection;
          getListener()->insertUnicodeString(s.str().c_str());
        }
        break;
      default:
        break;
      }
      f << "token[" << zone->m_tokens[(size_t)data.m_id] << "],";
      input->seek(actPos, WPX_SEEK_SET);
      break;
    }
    case 1:
      if (m_structures) {
        listenerState.sendFont(zone->m_ids[0][(size_t)data.m_id].m_id);
        f << "[" << listenerState.getFontDebugString(zone->m_ids[0][(size_t)data.m_id].m_id) << "],";
      } else
        f << "[" << zone->m_ids[0][(size_t)data.m_id] << "],";
      break;
    case 0:
      if (m_structures) {
        listenerState.sendParagraph(zone->m_ids[1][(size_t)data.m_id].m_id);
        f << "[" << listenerState.getParagraphDebugString(zone->m_ids[1][(size_t)data.m_id].m_id) << "],";
      } else
        f << "[" << zone->m_ids[1][(size_t)data.m_id] << "],";
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
bool MWProParser::sendPictureZone(int blockId, MWAWPosition const &pictPos,
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
                              MWAWPosition pictPos,
                              WPXPropertyList const &extras)
{
  if (!zone) return false;
  if (zone->m_type != 1) {
    MWAW_DEBUG_MSG(("MWProParser::sendPicture: not a picture date\n"));
    return false;
  }

  zone->m_parsed = true;

  // ok init is done
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw::DebugStream f;

  f << "Entries(PICT),";
  asciiFile.addPos(0);
  asciiFile.addNote(f.str().c_str());

  input->seek(0, WPX_SEEK_SET);
  long pictSize = (long) input->readULong(4);
  if (pictSize < 10 || pictSize > long(zone->m_data.size())) {
    MWAW_DEBUG_MSG(("MWProParser::sendPicture: oops a pb with pictSize\n"));
    asciiFile.addPos(4);
    asciiFile.addNote("#PICT");
    return false;
  }
  shared_ptr<MWAWPict> pict(MWAWPictData::get(input, (int)pictSize));
  if (!pict) {
    // sometimes this just fails because the pictSize is not correct
    input->seek(14, WPX_SEEK_SET);
    if (input->readULong(2) == 0x1101) { // try to force the size to be ok
      WPXBinaryData data;
      input->seek(0, WPX_SEEK_SET);
      input->readDataBlock(4+pictSize, data);
      unsigned char *dataPtr=const_cast<unsigned char *>(data.getDataBuffer());
      if (!dataPtr) {
        MWAW_DEBUG_MSG(("MWProParser::sendPicture: oops where is the picture...\n"));
        return false;
      }

      dataPtr[4]=dataPtr[2];
      dataPtr[5]=dataPtr[3];

      MWAWInputStreamPtr pictInput=MWAWInputStream::get(data, false);
      if (!pictInput) {
        MWAW_DEBUG_MSG(("MWProParser::sendPicture: oops where is the picture input...\n"));
        return false;
      }

      pictInput->seek(4, WPX_SEEK_SET);
      pict.reset(MWAWPictData::get(pictInput, (int)pictSize));
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
  libmwaw::Debug::dumpFile(file, f.str().c_str());
  asciiFile.addPos(4+pictSize);
  asciiFile.addNote("PICT(end)");
#endif

  if (!pict) { // ok, we can not do anything except sending the data...
    MWAW_DEBUG_MSG(("MWProParser::parseDataZone: no sure this is a picture\n"));
    if (pictPos.size().x() <= 0 || pictPos.size().y() <= 0)
      pictPos=MWAWPosition(Vec2f(0,0),Vec2f(100.,100.), WPX_POINT);
    if (getListener()) {
      WPXBinaryData data;
      input->seek(4, WPX_SEEK_SET);
      input->readDataBlock(pictSize, data);
      getListener()->insertPicture(pictPos, data, "image/pict", extras);
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

  if (getListener()) {
    WPXBinaryData data;
    std::string type;
    if (pict->getBinary(data,type))
      getListener()->insertPicture(pictPos, data, type, extras);
  }
  return true;
}

////////////////////////////////////////////////////////////
// some debug functions
////////////////////////////////////////////////////////////

#ifdef DEBUG
void MWProParser::saveOriginal(MWAWInputStreamPtr input)
{
  libmwaw::DebugStream f;

  int ptSz = version()==0 ? 2 : 4;
  libmwaw::DebugFile orig;
  orig.setStream(input);
  orig.open("orig");
  int bl = 0;
  while(1) {
    long pos = bl*0x100;
    input->seek(pos, WPX_SEEK_SET);
    if (long(input->tell()) != pos)
      break;
    f.str("");
    if (bl) {
      input->seek(-ptSz, WPX_SEEK_CUR);
      long val = input->readLong(ptSz);
      long next = val > 0 ? val : -val;
      if (next > 0 && next < 0x1000) {
        f << "next=" << std::hex << (next-1)*0x100 << std::dec << ",";
        if (val < 0)
          f << "N=?" << input->readLong(ptSz) << ",";
        orig.addPos(pos-ptSz);
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
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;

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
      char ch = (char) input->readULong(1);
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
