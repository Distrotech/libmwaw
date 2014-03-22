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

#include "MWAWTextListener.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWParser.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSection.hxx"
#include "MWAWRSRCParser.hxx"

#include "GreatWksGraph.hxx"
#include "GreatWksText.hxx"

#include "GreatWksDocument.hxx"

/** Internal: the structures of a GreatWksDocument */
namespace GreatWksDocumentInternal
{

////////////////////////////////////////
//! Internal: the state of a GreatWksDocument
struct State {
  //! constructor
  State() : m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
GreatWksDocument::GreatWksDocument(MWAWParser &parser) :
  m_state(), m_parserState(parser.getParserState()),
  m_parser(&parser), m_graphParser(), m_textParser(),
  m_getMainSection(0), m_newPage(0)
{
  m_state.reset(new GreatWksDocumentInternal::State);

  m_graphParser.reset(new GreatWksGraph(*this));
  m_textParser.reset(new GreatWksText(*this));
}

GreatWksDocument::~GreatWksDocument()
{
}

MWAWInputStreamPtr GreatWksDocument::rsrcInput()
{
  return m_parser->getRSRCParser()->getInput();
}

libmwaw::DebugFile &GreatWksDocument::rsrcAscii()
{
  return m_parser->getRSRCParser()->ascii();
}

////////////////////////////////////////////////////////////
// interface via callback
////////////////////////////////////////////////////////////
MWAWSection GreatWksDocument::getMainSection() const
{
  if (!m_getMainSection) {
    MWAW_DEBUG_MSG(("GreatWksDocument::getMainSection: can not find getMainSection callback\n"));
    return MWAWSection();
  }
  return (m_parser->*m_getMainSection)();
}

void GreatWksDocument::newPage(int page)
{
  if (!m_newPage) {
    MWAW_DEBUG_MSG(("GreatWksDocument::newPage: can not find newPage callback\n"));
    return;
  }
  (m_parser->*m_newPage)(page);
}


////////////////////////////////////////////////////////////
// interface with the text document
////////////////////////////////////////////////////////////
bool GreatWksDocument::sendTextbox(MWAWEntry const &entry, MWAWListenerPtr listener)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long actPos = input->tell();
  bool ok=getTextParser()->sendTextbox(entry, listener);
  input->seek(actPos, librevenge::RVNG_SEEK_SET);
  return ok;
}

bool GreatWksDocument::canSendTextboxAsGraphic(MWAWEntry const &entry)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long actPos = input->tell();
  bool ok=getTextParser()->canSendTextBoxAsGraphic(entry);
  input->seek(actPos, librevenge::RVNG_SEEK_SET);
  return ok;
}

////////////////////////////////////////////////////////////
// interface with the graph document
////////////////////////////////////////////////////////////
bool GreatWksDocument::sendPicture(MWAWEntry const &entry, MWAWPosition pos)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long actPos = input->tell();
  bool ok=getGraphParser()->sendPicture(entry, pos);
  input->seek(actPos, librevenge::RVNG_SEEK_SET);
  return ok;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool GreatWksDocument::readRSRCZones()
{
  MWAWRSRCParserPtr rsrcParser = m_parser->getRSRCParser();
  if (!rsrcParser)
    return true;

  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;
  // the 1 zone
  char const *(zNames[]) = {"PRNT", "PAT#", "WPSN", "PlTT", "ARRs", "DaHS", "GrDS", "NxEd" };
  for (int z = 0; z < 8; ++z) {
    it = entryMap.lower_bound(zNames[z]);
    while (it != entryMap.end()) {
      if (it->first != zNames[z])
        break;
      MWAWEntry const &entry = it++->second;
      switch (z) {
      case 0:
        readPrintInfo(entry);
        break;
      case 1:
        m_graphParser->readPatterns(entry);
        break;
      case 2:
        readWPSN(entry);
        break;
      case 3: // only in v2
        m_graphParser->readPalettes(entry);
        break;
      case 4: // only in v2?
        readARRs(entry);
        break;
      case 5: // only in v2?
        readDaHS(entry);
        break;
      case 6: // only in v2?
        readGrDS(entry);
        break;
      case 7: // only in v2?
        readNxEd(entry);
        break;
      default:
        break;
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the windows position blocks
////////////////////////////////////////////////////////////
bool GreatWksDocument::readWPSN(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%24) != 2) {
    MWAW_DEBUG_MSG(("GreatWksDocument::readWPSN: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;
  entry.setParsed(true);

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(Windows):";
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  if (2+24*N!=int(entry.length())) {
    f << "###";
    MWAW_DEBUG_MSG(("GreatWksDocument::readWPSN: the number of entries seems bad\n"));
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i < N; ++i) {
    pos = input->tell();
    f.str("");
    f << "Windows-" << i << ":";
    int width[2];
    for (int j=0; j < 2; ++j)
      width[j]=(int) input->readLong(2);
    f << "w=" << width[1] << "x" << width[0] << ",";
    int LT[2];
    for (int j=0; j < 2; ++j)
      LT[j]=(int) input->readLong(2);
    f << "LT=" << LT[1] << "x" << LT[0] << ",";
    for (int st=0; st < 2; ++st) {
      int dim[4];
      for (int j=0; j < 4; ++j)
        dim[j]=(int) input->readLong(2);
      if (dim[0]!=LT[0] || dim[1]!=LT[1] || dim[2]!=LT[0]+width[0])
        f << "dim" << st << "=" << dim[1] << "x" << dim[0] << "<->"
          << dim[3] << "x" << dim[2] << ",";
    }
    input->seek(pos+24, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool GreatWksDocument::readPrintInfo(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() != 120) {
    MWAW_DEBUG_MSG(("GreatWksDocument::readPrintInfo: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;

  input->seek(pos, librevenge::RVNG_SEEK_SET);
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

  m_parser->getPageSpan().setMarginTop(lTopMargin.y()/72.0);
  m_parser->getPageSpan().setMarginBottom(botMarg/72.0);
  m_parser->getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
  m_parser->getPageSpan().setMarginRight(rightMarg/72.0);
  m_parser->getPageSpan().setFormLength(paperSize.y()/72.);
  m_parser->getPageSpan().setFormWidth(paperSize.x()/72.);

  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read some unknown zone in rsrc fork
////////////////////////////////////////////////////////////
bool GreatWksDocument::readARRs(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%32)) {
    MWAW_DEBUG_MSG(("GreatWksDocument::readARRs: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos-4);
  ascFile.addNote("Entries(ARRs)");
  int N=int(entry.length()/32);
  for (int i=0; i < N; ++i) {
    pos = input->tell();
    f.str("");
    f << "ARRs-" << i << ":";
    input->seek(pos+32, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool GreatWksDocument::readDaHS(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() < 44 || (entry.length()%12) != 8) {
    MWAW_DEBUG_MSG(("GreatWksDocument::readDaHS: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;
  entry.setParsed(true);

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(DaHS):";
  int val=(int) input->readLong(2);
  if (val!=2)
    f << "#f0=" << val << ",";
  val=(int) input->readLong(2);
  if (val!=9)
    f << "#f1=" << val << ",";
  ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  pos=entry.begin()+44;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int N=int((entry.length()-44))/12;

  for (int i=0; i < N; ++i) {
    pos = input->tell();
    f.str("");
    f << "DaHS-" << i << ":";
    input->seek(pos+12, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  return true;
}

bool GreatWksDocument::readGrDS(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%16)) {
    MWAW_DEBUG_MSG(("GreatWksDocument::readGrDS: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos-4);
  ascFile.addNote("Entries(GrDS)");
  int N=int(entry.length()/16);
  for (int i=0; i < N; ++i) {
    pos = input->tell();
    f.str("");
    f << "GrDS-" << i << ":";
    int val=(int)input->readLong(2); // 1,2,3
    f << "unkn=" << val << ",";
    for (int st=0; st < 2; ++st) {
      unsigned char col[3];
      for (int j=0; j < 3; ++j)
        col[j]=(unsigned char)(input->readULong(2)>>8);
      MWAWColor color(col[0], col[1], col[2]);
      if (st==0) {
        if (!color.isWhite()) f << "backColor=" << color << ",";
      }
      else if (!color.isBlack()) f << "frontColor=" << color << ",";
    }
    val = (int) input->readULong(2);
    if (val) f << "ptr?=" << std::hex << val << std::dec << ",";
    input->seek(pos+16, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(i==0?pos-4:pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool GreatWksDocument::readNxEd(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<4) {
    MWAW_DEBUG_MSG(("GreatWksDocument::readNxEd: the entry is bad\n"));
    return false;
  }

  if (entry.length()!=4) {
    MWAW_DEBUG_MSG(("GreatWksDocument::readNxEd: OHHHH the entry is filled\n"));
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;
  entry.setParsed(true);

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(NxED):";
  for (int i = 0; i < 2; ++i) { // always 0
    int val=(int) input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  ascFile.addPos(pos-4);
  ascFile.addNote("Entries(NxED):");
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool GreatWksDocument::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = GreatWksDocumentInternal::State();
  MWAWInputStreamPtr input = m_parserState->m_input;
  if (!input || !input->hasDataFork() || !input->checkPosition(0x4c))
    return false;

  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "FileHeader:";

  input->seek(0,librevenge::RVNG_SEEK_SET);
  int vers=(int) input->readLong(1);
  if (vers < 1 || vers > 2)
    return false;
  if (input->readLong(1))
    return false;
  m_parserState->m_version=vers;
  std::string type("");
  for (int i=0; i < 4; ++i)
    type+=(char) input->readLong(1);
  bool isDraw=false, isSheet=false;
  if (type=="ZOBJ") {
    isDraw=true;
    m_parserState->m_kind=MWAWDocument::MWAW_K_DRAW;
    MWAW_DEBUG_MSG(("GreatWksDocument::checkHeader: find a draw file\n"));
  }
  else if (type=="ZCAL") {
    isSheet=true;
    m_parserState->m_kind=MWAWDocument::MWAW_K_SPREADSHEET;
    MWAW_DEBUG_MSG(("GreatWksDocument::checkHeader: find a spreadsheet file\n"));
  }
  else if (type!="ZWRT")
    return false;

  if (strict) {
    // check that the fonts table is in expected position
    long fontPos;
    if (isDraw)
      fontPos = 0x4a;
    else if (isSheet)
      fontPos=18;
    else
      fontPos = vers==1 ? 0x302 : 0x308;
    if (input->seek(fontPos, librevenge::RVNG_SEEK_SET) || !m_textParser->readFontNames()) {
      MWAW_DEBUG_MSG(("GreatWksDocument::checkHeader: can not find fonts table\n"));
      return false;
    }
  }
  ascFile.addPos(0);
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(6);
  ascFile.addNote("FileHeader-II:");

  if (header)
    header->reset(MWAWDocument::MWAW_T_GREATWORKS, vers,m_parserState->m_kind);
  return true;
}



// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
