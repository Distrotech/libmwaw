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

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "ACText.hxx"

#include "ACParser.hxx"

/** Internal: the structures of a ACParser */
namespace ACParserInternal
{
////////////////////////////////////////
//! Internal: the state of a ACParser
struct State {
  //! constructor
  State() : m_eof(-1), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0) {
  }

  //! end of file
  long m_eof;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a ACParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(ACParser &pars, MWAWInputStreamPtr input, bool header) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_isHeader(header) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const {
    if (MWAWSubDocument::operator!=(doc)) return true;
    SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
    if (!sDoc) return true;
    if (m_isHeader != sDoc->m_isHeader) return true;
    return false;
  }

  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  //! true if we need to send the parser
  int m_isHeader;
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("ACParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_parser);

  // reinterpret_cast<ACParser *>(m_parser)->sendHeaderFooter(m_isHeader);
}
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
ACParser::ACParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_state(), m_textParser()
{
  init();
}

ACParser::~ACParser()
{
}

void ACParser::init()
{
  resetListener();
  setAsciiName("main-1");

  m_state.reset(new ACParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);

  m_textParser.reset(new ACText(*this));
}

MWAWInputStreamPtr ACParser::rsrcInput()
{
  return getRSRCParser()->getInput();
}

libmwaw::DebugFile &ACParser::rsrcAscii()
{
  return getRSRCParser()->ascii();
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
Vec2f ACParser::getPageLeftTop() const
{
  return Vec2f(float(getPageSpan().getMarginLeft()),
               float(getPageSpan().getMarginTop()+m_state->m_headerHeight/72.0));
}

////////////////////////////////////////////////////////////
// interface with the text parser
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
bool ACParser::isFilePos(long pos)
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

void ACParser::newPage(int number)
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

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void ACParser::parse(WPXDocumentInterface *docInterface)
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
      m_textParser->sendMainText();
#ifdef DEBUG
      m_textParser->flushExtra();
#endif
    }
    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("ACParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void ACParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getListener()) {
    MWAW_DEBUG_MSG(("ACParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  int numPages = 1;
  if (m_textParser->numPages() > numPages)
    numPages = m_textParser->numPages();
  m_state->m_numPages = numPages;

  MWAWPageSpan ps(getPageSpan());
  ps.setPageSpan(m_state->m_numPages+1);
  std::vector<MWAWPageSpan> pageList(1,ps);
  MWAWContentListenerPtr listen(new MWAWContentListener(*getParserState(), pageList, documentInterface));
  setListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool ACParser::createZones()
{
  int vers=version();
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  MWAWInputStreamPtr input = getInput();
  // libmwaw::DebugStream f;

  if (rsrcParser) {
    // STR:0 -> title name, STR:1 custom label

    std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
    std::multimap<std::string, MWAWEntry>::iterator it;

    // the 0 zone
    char const *(zNames[]) = {"PSET", "WSIZ", "LABL", "QOPT", "QHDR"};
    for (int z = 0; z < 5; z++) {
      it = entryMap.lower_bound(zNames[z]);
      while (it != entryMap.end()) {
        if (it->first != zNames[z])
          break;
        MWAWEntry const &entry = it++->second;
        switch(z) {
        case 0:
          readPrintInfo(entry);
          break;
        case 1:
          readWindowPos(entry);
          break;
        case 2:
          readLabel(entry);
          break;
        case 3:
          readOption(entry);
          break;
        case 4:
          readQHDR(entry);
          break;
        default:
          break;
        }
      }
    }
  }

  input->seek(vers>=3 ? 2 : 0, WPX_SEEK_SET);
  int line=0;
  while (!input->atEOS()) {
    if (!m_textParser->readLine(line++))
      break;
  }

  ascii().addPos(input->tell());
  ascii().addNote("Entries(Loose)");

  if (!m_textParser->createZones())
    return false;
  return false;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool ACParser::readPrintInfo(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() != 120) {
    MWAW_DEBUG_MSG(("ACParser::readPrintInfo: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  libmwaw::PrinterInfo info;
  if (!info.read(input)) return false;
  f << "Entries(PrintInfo):"<< info;
  entry.setParsed(true);

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

  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the windows positon info
////////////////////////////////////////////////////////////
bool ACParser::readWindowPos(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() != 8) {
    MWAW_DEBUG_MSG(("ACParser::readWindowPos: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  f << "Entries(WindowPos):";
  entry.setParsed(true);
  int dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = (int) input->readLong(2);
  f << "pos=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// small resource fork
////////////////////////////////////////////////////////////

// label kind
bool ACParser::readLabel(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() != 2) {
    MWAW_DEBUG_MSG(("ACParser::readLabel: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  f << "Entries(Label):";
  entry.setParsed(true);
  int val = (int) input->readLong(2); // a small int 0|2|b|c
  switch(val) {
  case 0:
    f << "noLabel,";
    break;
  case 2:
    f << "checkbox,";
    break;
  case 0xb:
    f << "decimal,"; // 1.0,
    break;
  case 0xc:
    f << "I A...,";
    break;
  case 0xe:
    f << "custom,";
    break;
  default:
    f << "#labelType=" << val << ",";
    break;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

// QHDR : 2 flags ?
bool ACParser::readQHDR(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() != 20) {
    MWAW_DEBUG_MSG(("ACParser::readQHDR: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  f << "Entries(QHDR):";
  entry.setParsed(true);
  int val;
  // f0=2|16|21, f1=12|18, f2=0|3, f3=1[footer?]
  for (int i = 0; i < 4; i++) {
    val = (int) input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  for (int i = 0; i < 2; i++) {
    val = (int) input->readLong(1);
    if (val)
      f << "fl" << i << "=" << val << ",";
  }
  for (int i = 0; i < 5; i++) {
    val = (int) input->readLong(2);
    if (val)
      f << "g" << i << "=" << val << ",";
  }

  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

// Option : small modificator change
bool ACParser::readOption(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() != 2) {
    MWAW_DEBUG_MSG(("ACParser::readOption: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  f << "Entries(Option):";
  entry.setParsed(true);
  int fl0=(int) input->readULong(1);
  if (fl0&0x10)
    f << "speaker[dial],";
  fl0 &= 0xEF;
  if (fl0) // find also fl0&(4|40|80)
    f << "fl0=" << std::hex << fl0 << std::dec << ",";
  int fl1=(int) input->readULong(1);
  if (fl1&0x4)
    f << "smart['\"],";
  if (fl1&0x8)
    f << "open[startup],";
  if (fl1&0x20)
    f << "nolabel[picture],";
  if (fl1&0x40)
    f << "noframe[current],";
  if (fl1&0x80)
    f << "nolabel[clipboard],";
  fl1 &= 0x13;
  if (fl1) // always fl1&10 ?
    f << "fl1=" << std::hex << fl1 << std::dec << ",";
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
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
bool ACParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = ACParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !isFilePos(22))
    return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";

  // first check end of file
  input->seek(0,WPX_SEEK_SET);
  while(!input->atEOS()) {
    if (input->seek(1024, WPX_SEEK_CUR) != 0) break;
  }
  input->seek(-4, WPX_SEEK_CUR);
  if (!isFilePos(input->tell()))
    return false;
  int last[2];
  for (int i = 0; i < 2; i++)
    last[i]=(int) input->readLong(2);
  int vers=-1;
  if (last[0]==0x4E4C && last[1]==0x544F)
    vers=3;
  else if (last[1]==0)
    vers=1;
  if (vers<=0)
    return false;
  setVersion(vers);

  // ok, now check the beginning of the file
  int val;
  input->seek(0, WPX_SEEK_SET);
  if (vers==3) {
    val=(int) input->readULong(2);
    if (val!=3) {
      if (strict) return false;
      if (val < 1 || val > 4)
        return false;
      f << "#vers=" << val << ",";
      MWAW_DEBUG_MSG(("ACParser::checkHeader: find unexpected version: %d\n", val));
    }
  }
  val = (int) input->readULong(2); // id
  if (val <= 0 || val > 5)
    return false;
  val = (int) input->readULong(2); // type
  if (val != 1 && val !=2)
    return false;

  // check that the first text size is valid
  input->seek(vers==1 ? 18 : 20, WPX_SEEK_SET);
  long sz=(long) input->readULong(4);
  if (!isFilePos(input->tell()+sz))
    return false;

  if (header)
    header->reset(MWAWDocument::ACT, vers);
  if (vers >= 3) {
    ascii().addPos(0);
    ascii().addNote(f.str().c_str());
  }
  return true;
}



// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
