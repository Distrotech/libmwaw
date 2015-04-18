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

  MWAWVec2i paperSize = info.paper().size();
  MWAWVec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  // define margin from print info
  MWAWVec2i lTopMargin= -1 * info.paper().pos(0);
  MWAWVec2i rBotMargin=info.paper().pos(1) - info.page().pos(1);

  // move margin left | top
  int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
  int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
  lTopMargin -= MWAWVec2i(decalX, decalY);
  rBotMargin += MWAWVec2i(decalX, decalY);

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
  if (type=="ZOBJ") {
    m_parserState->m_kind=MWAWDocument::MWAW_K_DRAW;
    MWAW_DEBUG_MSG(("GreatWksDocument::checkHeader: find a draw file\n"));
  }
  else if (type=="ZCAL") {
    m_parserState->m_kind=MWAWDocument::MWAW_K_SPREADSHEET;
    MWAW_DEBUG_MSG(("GreatWksDocument::checkHeader: find a spreadsheet file\n"));
  }
  else if (type=="ZDBS") {
    m_parserState->m_kind=MWAWDocument::MWAW_K_DATABASE;
    MWAW_DEBUG_MSG(("GreatWksDocument::checkHeader: find a database file\n"));
  }
  else if (type!="ZWRT")
    return false;

  // extra check for database must be done in GreatWksDBParser
  if (strict && m_parserState->m_kind!=MWAWDocument::MWAW_K_DATABASE) {
    // check that the fonts table is in expected position
    long fontPos=-1;
    if (m_parserState->m_kind==MWAWDocument::MWAW_K_DRAW)
      fontPos = 0x4a;
    else if (m_parserState->m_kind==MWAWDocument::MWAW_K_SPREADSHEET)
      fontPos=18;
    else if (m_parserState->m_kind==MWAWDocument::MWAW_K_TEXT)
      fontPos = vers==1 ? 0x302 : 0x308;
    if (fontPos>0 && (input->seek(fontPos, librevenge::RVNG_SEEK_SET) || !m_textParser->readFontNames())) {
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


////////////////////////////////////////////////////////////
// read a formula
////////////////////////////////////////////////////////////
bool GreatWksDocument::readCellInFormula(MWAWVec2i const &pos, MWAWCellContent::FormulaInstruction &instr)
{
  MWAWInputStreamPtr input=m_parserState->m_input;
  instr=MWAWCellContent::FormulaInstruction();
  instr.m_type=MWAWCellContent::FormulaInstruction::F_Cell;
  bool absolute[2] = { true, true};
  int cPos[2];
  for (int i=0; i<2; ++i) {
    int val = (int) input->readULong(2);
    if (val & 0x8000) {
      absolute[i]=false;
      if (val&0x4000)
        cPos[i] = pos[i]+(val-0xFFFF);
      else
        cPos[i] = pos[i]+(val-0x7FFF);
    }
    else
      cPos[i]=val;
  }

  if (cPos[0] < 1 || cPos[1] < 1) {
    MWAW_DEBUG_MSG(("GreatWksDocument::readCellInFormula: can not read cell position\n"));
    return false;
  }
  instr.m_position[0]=MWAWVec2i(cPos[0]-1,cPos[1]-1);
  instr.m_positionRelative[0]=MWAWVec2b(!absolute[0],!absolute[1]);
  return true;
}

bool GreatWksDocument::readString(long endPos, std::string &res)
{
  res="";
  MWAWInputStreamPtr input=m_parserState->m_input;
  long pos=input->tell();
  int fSz=(int) input->readULong(1);
  if (pos+1+fSz>endPos) {
    MWAW_DEBUG_MSG(("GreatWksDocument::readString: can not read string size\n"));
    return false;
  }
  for (int i=0; i<fSz; ++i)
    res += (char) input->readULong(1);
  return true;
}

bool GreatWksDocument::readNumber(long endPos, double &res, bool &isNan)
{
  MWAWInputStreamPtr input=m_parserState->m_input;
  long pos=input->tell();
  if (pos+10>endPos) {
    MWAW_DEBUG_MSG(("GreatWksDocument::readNumber: can not read a number\n"));
    return false;
  }
  return input->readDouble10(res, isNan);
}

namespace GreatWksDocumentInternal
{
struct Functions {
  char const *m_name;
  int m_arity;
};

static Functions const s_listFunctions[] = {
  { "=", 1}, {"", -1} /*UNKN*/, {"", 0}/*SPEC:long*/, {"", -1} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", 0} /*SPEC:double*/,{ "", -2} /*UNKN*/,{ "", 0} /*SPEC:text*/,
  { "", 0} /*SPEC:short*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,

  { "", 0} /*SPEC:cell*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,

  { ":", 2} /*SPEC:concatenate cell*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,

  { "(", 1}, { "-", 1}, { "+", 1} /*checkme*/,{ "^", 2},
  { "*", 2}, { "/", 2}, { "+", 2}, { "-", 2},
  { "", -2} /*UNKN*/,{ "=", 2}, { "<", 2}, { "<=", 2},
  { ">", 2}, { ">=", 2}, { "<>", 2}, { "", -2} /*UNKN*/,

};
}

bool GreatWksDocument::readFormula(MWAWVec2i const &cPos, long endPos, std::vector<MWAWCellContent::FormulaInstruction> &formula, std::string &error)
{
  MWAWInputStreamPtr input=m_parserState->m_input;
  libmwaw::DebugStream f;
  std::vector<std::vector<MWAWCellContent::FormulaInstruction> > stack;
  bool ok=true;
  while (!input->isEnd()) {
    long pos=input->tell();
    if (pos >= endPos)
      break;
    int arity=0, val, type=(int) input->readULong(1);
    MWAWCellContent::FormulaInstruction instr;
    switch (type) {
    case 2:
      if (pos+1+2 > endPos) {
        ok = false;
        break;
      }
      val = (int) input->readLong(2);
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Long;
      instr.m_longValue=val;
      break;
    case 5: {
      double value;
      bool isNan;
      ok=readNumber(endPos, value, isNan);
      if (!ok) break;
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Double;
      instr.m_doubleValue=value;
      break;
    }
    case 7: {
      std::string text;
      ok=readString(endPos, text);
      if (!ok) break;
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Text;
      instr.m_content=text;
      break;
    }
    case 8:
      if (pos+1+1 > endPos) {
        ok = false;
        break;
      }
      val = (int) input->readLong(1);
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Long;
      instr.m_longValue=val;
      break;
    case 0x10: {
      ok=readCellInFormula(cPos,instr);
      if (!ok) break;
      f << instr << ",";
      break;
    }
    case 0x41: {
      // field in a database formula
      if (pos+1+1 > endPos || m_parserState->m_kind!=MWAWDocument::MWAW_K_DATABASE) {
        ok = false;
        break;
      }
      std::string text;
      ok=readString(endPos, text);
      if (!ok) break;
      // we have not sufficient information to fill the position, let GreatWksDBParser fills it
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Cell;
      instr.m_positionRelative[0]=MWAWVec2b(true,true);
      instr.m_content=text;
      break;
    }
    case 0x40: {
      if (pos+1+1 > endPos) {
        ok = false;
        break;
      }
      val = (int) input->readULong(1);
      arity= (int) input->readULong(1);
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Function;

      static char const* (s_functions[]) = {
        "", "Abs", "Exp", "Fact", "Int", "Ln", "Log", "Log10",
        "Mod", "Pi", "Product", "Rand", "Round", "Sign", "Sqrt", "Trunc",

        "Average", "Count", "CountA", "Max", "Min", "StDev", "StDevP", "Sum",
        "Var", "VarP", "Acos", "Asin", "Atan", "Atan2", "Cos", "Sin",

        "Tan", "Degrees", "Radians", "And", "Choose", "False", "If", "IsBlank",
        "IsErr", "IsError", "IsLogical", "IsNa", "IsNonText", "IsNum", "IsRef", "IsText",

        "Not", "Or", "True", "Char", "Clean", "Code", "Dollar", "Exact",
        "Find", "Fixed", "Left", "Len", "Lower", "Mid", "Proper"/*checkme: first majuscule*/, "Replace",

        "Rept", "Right", "Search", "Substitute", "Trim", "Upper", "DDB", "FV",
        "IPMT", "IRR", "MIRR", "NPER", "NPV", "PMT", "PPMT", "PV",

        "Rate", "SLN", "SYD", "Annuity", "Compound", "Date", "Day", "Hour",
        "Minute", "Month", "Now", "Second", "Time", "Weekday", "Year", "HLookup",

        "Index", "Lookup", "Match", "N", "Na", "T", "Type", "VLookup",
        "", "", "", "", "", "", "", "",

      };
      std::string functName("");
      if (val < 0x70) functName=s_functions[val];
      if (!functName.empty())
        instr.m_content=functName;
      else {
        std::stringstream s;
        s << "Funct" << std::hex << val << std::dec << "#";
        instr.m_content=s.str();
      }
      break;
    }
    default:
      if (type >= 0x40 || GreatWksDocumentInternal::s_listFunctions[type].m_arity == -2) {
        f.str("");
        f << "##Funct" << std::hex << type << std::dec;
        ok = false;
        break;
      }
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Function;
      instr.m_content=GreatWksDocumentInternal::s_listFunctions[type].m_name;
      ok=!instr.m_content.empty();
      arity = GreatWksDocumentInternal::s_listFunctions[type].m_arity;
      if (arity == -1) arity = (int) input->readLong(1);
      if (arity<0) ok=false;
      break;
    }
    if (!ok) break;
    std::vector<MWAWCellContent::FormulaInstruction> child;
    if (instr.m_type!=MWAWCellContent::FormulaInstruction::F_Function) {
      child.push_back(instr);
      stack.push_back(child);
      continue;
    }
    size_t numElt = stack.size();
    if ((int) numElt < arity) {
      f.str("");
      f << instr.m_content << "[##" << arity << "]";
      ok = false;
      break;
    }
    if ((instr.m_content[0] >= 'A' && instr.m_content[0] <= 'Z') || instr.m_content[0] == '(') {
      if (instr.m_content[0] != '(')
        child.push_back(instr);

      instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
      instr.m_content="(";
      child.push_back(instr);
      for (int i = 0; i < arity; i++) {
        if (i) {
          instr.m_content=";";
          child.push_back(instr);
        }
        std::vector<MWAWCellContent::FormulaInstruction> const &node=
          stack[size_t((int)numElt-arity+i)];
        child.insert(child.end(), node.begin(), node.end());
      }
      instr.m_content=")";
      child.push_back(instr);

      stack.resize(size_t((int) numElt-arity+1));
      stack[size_t((int)numElt-arity)] = child;
      continue;
    }
    if (arity==1) {
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
      stack[numElt-1].insert(stack[numElt-1].begin(), instr);
      if (type==0 && pos+1==endPos)
        break;
      continue;
    }
    if (arity==2 && instr.m_content==":") {
      if (stack[numElt-2].size()!=1 || stack[numElt-2][0].m_type!=MWAWCellContent::FormulaInstruction::F_Cell ||
          stack[numElt-1].size()!=1 || stack[numElt-1][0].m_type!=MWAWCellContent::FormulaInstruction::F_Cell) {
        f << "### unexpected type of concatenate argument";
        ok=false;
        break;
      }
      instr=stack[numElt-2][0];
      MWAWCellContent::FormulaInstruction instr2=stack[numElt-1][0];
      instr.m_type=MWAWCellContent::FormulaInstruction::F_CellList;
      instr.m_position[1]=instr2.m_position[0];
      instr.m_positionRelative[1]=instr2.m_positionRelative[0];
      stack[numElt-2][0]=instr;
      stack.resize(numElt-1);
      continue;
    }
    if (arity==2) {
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
      stack[numElt-2].push_back(instr);
      stack[numElt-2].insert(stack[numElt-2].end(), stack[numElt-1].begin(), stack[numElt-1].end());
      stack.resize(numElt-1);
      continue;
    }
    ok=false;
    f << "### unexpected arity";
    break;
  }

  if (!ok) ;
  else if (stack.size()==1 && stack[0].size()>1 && stack[0][0].m_content=="=") {
    formula.insert(formula.begin(),stack[0].begin()+1,stack[0].end());
    return true;
  }
  else
    f << "###stack problem";

  m_parserState->m_asciiFile.addDelimiter(input->tell(),'#');
  static bool first = true;
  if (first) {
    MWAW_DEBUG_MSG(("GreatWksDocument::readFormula: I can not read some formula\n"));
    first = false;
  }

  error = f.str();
  f.str("");
  for (size_t i = 0; i < stack.size(); ++i) {
    for (size_t j=0; j < stack[i].size(); ++j)
      f << stack[i][j] << ",";
  }
  f << error;
  error = f.str();
  return false;
}


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
