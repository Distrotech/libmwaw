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

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWCell.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWList.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSpreadsheetListener.hxx"
#include "MWAWSubDocument.hxx"

#include "BeagleWksStructManager.hxx"

#include "BeagleWksDBParser.hxx"

/** Internal: the structures of a BeagleWksDBParser */
namespace BeagleWksDBParserInternal
{
//! Internal: the cell of a BeagleWksDBParser
struct Cell : public MWAWCell {
  //! constructor
  Cell(Vec2i pos=Vec2i(0,0)) : MWAWCell(), m_content(), m_formula(-1), m_isEmpty(false)
  {
    setPosition(pos);
  }
  //! the cell content
  MWAWCellContent m_content;
  //! the formula id
  int m_formula;
  //! flag to know if the cell is empty
  bool m_isEmpty;
};

//! Internal: the spreadsheet of a BeagleWksDBParser
struct Database {
  //! constructor
  Database() : m_numFields(0), m_widthCols(), m_heightRows(), m_cells(), m_lastReadRow(-1)
  {
  }
  //! try to associate a formula to a cell
  bool addFormula(Vec2i const &cellPos, std::vector<MWAWCellContent::FormulaInstruction> const &formula);
  //! convert the m_widthCols, m_heightRows in a vector of of point size
  static std::vector<float> convertInPoint(std::vector<int> const &list,
      float defSize)
  {
    size_t numElt = list.size();
    std::vector<float> res;
    res.resize(numElt);
    for (size_t i = 0; i < numElt; i++) {
      if (list[i] < 0) res[i] = defSize;
      else res[i] = float(list[i]);
    }
    return res;
  }
  //! update the number of columns and the width
  void updateWidthCols()
  {
    int maxCol=-1;
    for (size_t i = 0; i < m_cells.size(); ++i) {
      if (m_cells[i].position()[0]>maxCol)
        maxCol = m_cells[i].position()[0];
    }
    m_widthCols.resize(size_t(maxCol+1),-1);
  }
  //! the number of rows
  int m_numFields;
  //! the column size in points
  std::vector<int> m_widthCols;
  //! the row size in points
  std::vector<int> m_heightRows;
  //! the list of not empty cells
  std::vector<Cell> m_cells;
  //! the last read rows
  int m_lastReadRow;
};

bool Database::addFormula(Vec2i const &cellPos, std::vector<MWAWCellContent::FormulaInstruction> const &formula)
{
  for (size_t c=0; c < m_cells.size(); ++c) {
    if (m_cells[c].position()!=cellPos)
      continue;
    m_cells[c].m_content.m_formula=formula;
    return true;
  }
  MWAW_DEBUG_MSG(("Database::addFormula: can not find cell with position %dx%d\n", cellPos[0], cellPos[1]));
  return false;
}

////////////////////////////////////////
//! Internal: the state of a BeagleWksDBParser
struct State {
  //! constructor
  State() :  m_databaseBegin(-1), m_database(), m_databaseName("Sheet0"), m_typeEntryMap(),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }
  //! returns a color corresponding to an id
  static bool getColor(int id, MWAWColor &color)
  {
    switch (id) {
    case 0:
      color=MWAWColor::black();
      return true;
    case 1:
      color=MWAWColor::white();
      return true;
    case 2:
      color=MWAWColor(0xFF,0,0);
      return true;
    case 3:
      color=MWAWColor(0,0xFF,0);
      return true;
    case 4:
      color=MWAWColor(0,0,0xFF);
      return true;
    case 5:
      color=MWAWColor(0,0xFF,0xFF);
      return true;
    case 6:
      color=MWAWColor(0xFF,0,0xFF);
      return true;
    case 7:
      color=MWAWColor(0xFF,0xFF,0);
      return true;
    default:
      MWAW_DEBUG_MSG(("BeagleWksDBParserInternal::State::getColor: unknown color %d\n", id));
      return false;
    }
  }
  /** the database begin position */
  long m_databaseBegin;
  /** the database */
  Database m_database;
  /** the database name */
  std::string m_databaseName;
  /** the type entry map */
  std::multimap<std::string, MWAWEntry> m_typeEntryMap;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a BeagleWksDBParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(BeagleWksDBParser &pars, MWAWInputStreamPtr input, MWAWEntry const &entry) :
    MWAWSubDocument(&pars, input, entry)
  {
  }

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const
  {
    return MWAWSubDocument::operator!=(doc);
  }
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const
  {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("BeagleWksDBParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  BeagleWksDBParser *parser=dynamic_cast<BeagleWksDBParser *>(m_parser);
  if (!parser) {
    MWAW_DEBUG_MSG(("BeagleWksDBParserInternal::SubDocument::parse: can not find the parser\n"));
    return;
  }
  long pos = m_input->tell();
  listener->setFont(MWAWFont(3,12)); // fixme
  parser->sendText(m_zone, true);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}

}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
BeagleWksDBParser::BeagleWksDBParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWSpreadsheetParser(input, rsrcParser, header), m_state(), m_structureManager()
{
  init();
}

BeagleWksDBParser::~BeagleWksDBParser()
{
}

void BeagleWksDBParser::init()
{
  resetSpreadsheetListener();
  setAsciiName("main-1");

  m_state.reset(new BeagleWksDBParserInternal::State);
  m_structureManager.reset(new BeagleWksStructManager(getParserState()));

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}

MWAWInputStreamPtr BeagleWksDBParser::rsrcInput()
{
  return getRSRCParser()->getInput();
}

libmwaw::DebugFile &BeagleWksDBParser::rsrcAscii()
{
  return getRSRCParser()->ascii();
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
Vec2f BeagleWksDBParser::getPageLeftTop() const
{
  return Vec2f(float(getPageSpan().getMarginLeft()),
               float(getPageSpan().getMarginTop()+m_state->m_headerHeight/72.0));
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void BeagleWksDBParser::parse(librevenge::RVNGSpreadsheetInterface *docInterface)
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
      sendDatabase();
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetSpreadsheetListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void BeagleWksDBParser::createDocument(librevenge::RVNGSpreadsheetInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getSpreadsheetListener()) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  int numPages = 1;
  m_state->m_numPages = numPages;

  MWAWEntry header, footer;
  m_structureManager->getHeaderFooterEntries(header,footer);
  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan ps(getPageSpan());
  if (header.valid()) {
    shared_ptr<BeagleWksDBParserInternal::SubDocument> subDoc
    (new BeagleWksDBParserInternal::SubDocument(*this, getInput(), header));
    MWAWHeaderFooter hf(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
    hf.m_subDocument=subDoc;
    ps.setHeaderFooter(hf);
  }
  if (footer.valid()) {
    shared_ptr<BeagleWksDBParserInternal::SubDocument> subDoc
    (new BeagleWksDBParserInternal::SubDocument(*this, getInput(), footer));
    MWAWHeaderFooter hf(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    hf.m_subDocument=subDoc;
    ps.setHeaderFooter(hf);
  }
  ps.setPageSpan(numPages);
  pageList.push_back(ps);

  MWAWSpreadsheetListenerPtr listen(new MWAWSpreadsheetListener(*getParserState(), pageList, documentInterface));
  setSpreadsheetListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool BeagleWksDBParser::createZones()
{
  readRSRCZones();
  MWAWInputStreamPtr input = getInput();
  if (input->seek(66, librevenge::RVNG_SEEK_SET) || !readPrintInfo())
    return false;
  long pos = input->tell();
  if (!input->checkPosition(pos+70)) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::createZones: the file can not contains Zones\n"));
    return false;
  }

  // now read the list of zones
  libmwaw::DebugStream f;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(Zones):";
  for (int i=0; i<7; ++i) { // checkme: at least 2 zones, maybe 7
    MWAWEntry entry;
    entry.setBegin(input->readLong(4));
    entry.setLength(input->readLong(4));
    entry.setId((int) input->readLong(2));
    if (entry.length()==0) continue;
    entry.setType(i==1?"Frame":"Unknown");
    f << entry.type() << "[" << entry.id() << "]="
      << std::hex << entry.begin() << "<->" << entry.end() << ",";
    if (!entry.valid() || !input->checkPosition(entry.end())) {
      f << "###";
      if (i<2) {
        MWAW_DEBUG_MSG(("BeagleWksDBParser::createZones: can not read the header zone, stop\n"));
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        return false;
      }
      MWAW_DEBUG_MSG(("BeagleWksDBParser::createZones: can not zones entry %d\n",i));
      continue;
    }
    m_state->m_typeEntryMap.insert
    (std::multimap<std::string, MWAWEntry>::value_type(entry.type(),entry));
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // now parse the different zones
  std::multimap<std::string, MWAWEntry>::iterator it;
  it=m_state->m_typeEntryMap.find("FontNames");
  if (it!=m_state->m_typeEntryMap.end())
    m_structureManager->readFontNames(it->second);
  it=m_state->m_typeEntryMap.find("Frame");
  if (it!=m_state->m_typeEntryMap.end())
    m_structureManager->readFrame(it->second);

  // now parse the different zones
  for (it=m_state->m_typeEntryMap.begin(); it!=m_state->m_typeEntryMap.end(); ++it) {
    MWAWEntry const &entry=it->second;
    if (entry.isParsed())
      continue;
    f.str("");
    f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
    ascii().addPos(entry.end());
    ascii().addNote("_");
  }

  input->seek(m_state->m_databaseBegin, librevenge::RVNG_SEEK_SET);
  pos = input->tell();
  if (!m_structureManager->readDocumentInfo())
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  pos = input->tell();
  if (!m_structureManager->readDocumentPreferences())
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (!readDatabase())
    return m_state->m_database.m_cells.size()!=0;
  pos=input->tell();
  int N=(int) input->readULong(2);
  if (N==0) {
    ascii().addPos(pos);
    ascii().addNote("_");
  }
  else {
    // unsure: find N=2+000602000d02
    f.str("");
    f << "Entries(UnknZone0):";
    if (!input->checkPosition(pos+2+3*N)) {
      MWAW_DEBUG_MSG(("BeagleWksDBParser::createZones: can not read UnkZone0\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return true;
    }
    f << "unkn0=[";
    for (int i=0; i< N; ++i)
      f << input->readLong(2) << ",";
    f << "],";
    f << "unkn1=[";
    for (int i=0; i< N; ++i)
      f << input->readLong(1) << ",";
    f << "],";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  pos=input->tell();
  N=(int) input->readULong(2);
  if (N==0) {
    ascii().addPos(pos);
    ascii().addNote("_");
  }
  else {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::createZones: find data in UnkZone1\n"));
    f.str("");
    f << "Entries(UnknZone1):###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return true;
  }

  pos=input->tell();
  f.str("");
  f << "Entries(Memo):";
  long dSz=(long) input->readULong(2);
  long endPos=pos+2+dSz;
  N=(int)input->readULong(2);
  if (dSz<2+2*long(N) || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::createZones: find data in UnkZone1\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return true;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "Memo-"<< i << ":";
    int val=(int) input->readLong(1);
    if (val) f << "f0=" << val << ",";
    int sSz=(int) input->readULong(1);
    if (pos+2+sSz>endPos) {
      MWAW_DEBUG_MSG(("BeagleWksDBParser::createZones: can not read a memo\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return true;
    }
    std::string text("");
    for (int c=0; c<sSz; ++c) text+=(char) input->readULong(1);
    f << text << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  input->seek(endPos, librevenge::RVNG_SEEK_SET);

  pos=input->tell();
  f.str("");
  f << "Entries(UnknZone2):";
  dSz=(long) input->readULong(2); // or maybe N
  endPos=pos+4+dSz;
  int val=(int)input->readULong(2);
  if ((dSz%2) || val!=0xeb || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::createZones: can not read zone2\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return true;
  }
  if (dSz) {
    f << "unkn=[";
    for (int i=0; i<dSz/2; ++i)
      f << input->readLong(2) << ",";
    f << "],";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  /*
    now find 0000000000000001000100000000 or
    0003001200101a0003000026011a000400002e010b03001300081a000300002c0103000200000003000100020000
   */
  ascii().addPos(input->tell());
  ascii().addNote("Entries(ZoneEnd)");
  return true;
}

bool BeagleWksDBParser::readRSRCZones()
{
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  if (!rsrcParser)
    return true;

  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;
  // the 1 zone
  char const *(zNames[]) = {"wPos", "DMPF" };
  for (int z = 0; z < 2; ++z) {
    it = entryMap.lower_bound(zNames[z]);
    while (it != entryMap.end()) {
      if (it->first != zNames[z])
        break;
      MWAWEntry const &entry = it++->second;
      switch (z) {
      case 0: // 1001
        m_structureManager->readwPos(entry);
        break;
      case 1: // find in one file with id=4661 6a1f 4057
        m_structureManager->readFontStyle(entry);
        break;
      /* find also
         - edpt: see sendPicture
         - DMPP: the paragraph style
         - sect and alis: position?, alis=filesystem alias(dir, filename, path...)
      */
      default:
        break;
      }
    }
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool BeagleWksDBParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (!input->checkPosition(pos+0x70))
    return false;

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
  input->seek(pos+0x78, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != pos+0x78) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::readPrintInfo: file is too short\n"));
    return false;
  }
  ascii().addPos(input->tell());

  return true;
}

////////////////////////////////////////////////////////////
// database
////////////////////////////////////////////////////////////
bool BeagleWksDBParser::readDatabase()
{
  if (!readFields() || !readLayouts()) return false;

  MWAWInputStreamPtr &input= getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+6)) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::readDatabase: can not find the database header\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(Database):";
  int val;
  for (int i=0; i<2; ++i) { // f0=0|1
    val =(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  val =(int) input->readLong(2);
  if (val!=7) f << "f2=" << val << ",";
  int N=(int) input->readLong(2);
  f << "N[records]=" << N << ",";
  val =(int) input->readLong(2);
  if (val) f << "f3=" << val << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << "Database-record" << i << ":";
    int id=(int) input->readLong(2);
    if (i!=id) f << "id=" << id << ",";
    long dSz=(long) input->readULong(4);
    long endPos=pos+6+dSz;
    if (!input->checkPosition(pos+6+dSz)) {
      MWAW_DEBUG_MSG(("BeagleWksDBParser::readDatabase: can not find the database row %d\n", i));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
    }
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  pos=input->tell();
  f.str("");
  f << "Database-field:";
  val =(int) input->readLong(2);
  if (val) f << "f0=" << val << ",";
  N=(int) input->readULong(2);
  f << "N[fields]=" << N << ",";
  val =(int) input->readLong(2);
  if (val) f << "f1=" << val << ",";
  int dSz =(int) input->readULong(2);
  f << "dSz=" << dSz << ",";
  if (dSz<14 || !input->checkPosition(pos+8+(N+1)*dSz)) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::readDatabase: can not find the database field format\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << "Database-field" << i << ":";
    val=(int) input->readULong(2);
    if (val!=1) f << "f0=" << val << ",";
    val=(int) input->readULong(2);
    if (val!=0x4b) f << "f1=" << val << ",";
    f << "font=[";
    f << "sz=" << input->readLong(2) << ",";
    f << "id=" << input->readULong(2) << ",";
    val=(int) input->readULong(2);
    if (val) f << "flags=" << std::hex << val << std::dec << ",";
    val=(int) input->readULong(2);
    if (val!=73) f << "flags1=" << std::hex << val << std::dec << ",";
    f << "],";
    val=(int) input->readLong(2);
    if (val!=i) f << "#id=" << val << ",";
    input->seek(pos+dSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}


////////////////////////////////////////////////////////////
// read the fields
////////////////////////////////////////////////////////////
bool BeagleWksDBParser::readFields()
{
  MWAWInputStreamPtr &input= getInput();
  BeagleWksDBParserInternal::Database &database=m_state->m_database;
  long pos=input->tell();
  if (!input->checkPosition(pos+6)) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::readFields: can not find the field zone\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(Field):";
  int val=(int) input->readLong(2);
  if (val) f << "f0=" << val << ",";
  val=(int) input->readLong(2);
  if (val!=0x2c) // may a type
    f << "f1=" << val << ",";
  database.m_numFields=(int) input->readULong(2);
  f << "num[fields]=" << database.m_numFields << ",";
  if (!input->checkPosition(pos+database.m_numFields*64)) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::readFields: can not find the fields zone\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  for (int fld=0; fld<database.m_numFields; ++fld) {
    pos=input->tell();
    f.str("");
    f << "Field-" << fld << ":";
    long dSz=(long) input->readULong(2);
    long endPos=pos+4+dSz;
    if (dSz<0x3c || !input->checkPosition(endPos)) {
      MWAW_DEBUG_MSG(("BeagleWksDBParser::readFields: can not read a field\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return false;
    }
    int id=(int) input->readLong(2);
    if (id) f << "id=" << id << ",";
    int sSz=(int) input->readULong(1);
    if (sSz+1>dSz) {
      MWAW_DEBUG_MSG(("BeagleWksDBParser::readFields: can not read a field\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return false;
    }
    std::string name("");
    for (int i=0; i<sSz; ++i) name += (char) input->readULong(1);
    f << "\"" << name << "\",";
    ascii().addDelimiter(input->tell(),'|');
    input->seek(endPos-10, librevenge::RVNG_SEEK_SET);
    ascii().addDelimiter(input->tell(),'|');
    int type=(int) input->readLong(1);
    switch (type) {
    case 0:
      f << "text,";
      break;
    case 1:
      f << "number,";
      break;
    case 2:
      f << "date,";
      break;
    case 3:
      f << "time,";
      break;
    case 4:
      f << "picture,";
      break;
    case 5:
      f << "formula,";
      break;
    case 6:
      f << "memo,";
      break;
    default:
      f << "#type=" << type << ",";
      break;
    }
    f << "form?=" << std::hex << input->readULong(1) << std::dec << ",";
    f << "id2=" << std::hex << input->readULong(4) << std::dec << ",";
    val=(int) input->readLong(2); // 0|-1
    if (val!=-1) f << "g0=" << val << ",";
    f << "g1=" << input->readLong(2) << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the layouts
////////////////////////////////////////////////////////////
bool BeagleWksDBParser::readLayouts()
{
  MWAWInputStreamPtr &input= getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+6)) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::readLayouts: can not find the layout zone\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(Layout):";
  int val=(int) input->readLong(2);
  if (val) f << "f0=" << val << ",";
  val=(int) input->readLong(2);
  if (val!=0x29) // may a type
    f << "f1=" << val << ",";
  int numLayouts=(int) input->readULong(2);
  f << "num[layout]=" << numLayouts << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int layout=0; layout<numLayouts; ++layout) {
    if (!readLayout(layout)) return false;
  }
  return true;
}

bool BeagleWksDBParser::readLayout(int id)
{
  MWAWInputStreamPtr &input= getInput();
  libmwaw::DebugStream f;
  f << "Layout-" << id << "[A]:";

  long pos=input->tell();
  int val=(int) input->readULong(1);
  long dSz=(long) input->readULong(2);
  long endPos=pos+1+dSz;
  if (val!=id || dSz<100 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::readLayouts: can find a layout\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  val=(int) input->readLong(2);
  if (val) f << "f0=" << val << ",";
  val=(int) input->readULong(1);
  if (val!=id) f << "#id=" << val << ",";
  int sSz=(int) input->readULong(1);
  if (sSz>30) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::readLayouts: can find layout string\n"));
    f << "###sSz=" << sSz << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  std::string name("");
  for (int i=0; i<sSz; ++i) name+=(char) input->readULong(1);
  f << name << ",";
  input->seek(pos+37, librevenge::RVNG_SEEK_SET);
  f << "ids=[" << std::hex;
  for (int i=0; i<3; ++i) f << input->readULong(4) << ",";
  f << std::dec << "],";
  val=(int) input->readLong(2); // small number
  f << "N=" << val << ",";
  for (int i=0; i<6; ++i) { // f4=0|78, f5=0|68
    static int const expected[]= {0x100, 0, 0, 0, 0, 0xffff };
    val = (int) input->readULong(2);
    if (val!=expected[i])
      f << "f" << i+2 << "=" << std::hex << val << std::dec << ",";
  }
  f << "g0=" << std::hex << input->readULong(4) << std::dec << ",";
  f << "id2s=[" << std::hex;
  for (int i=0; i<4; ++i) {
    val=(int) input->readULong(i==2 ? 2 : 4);
    if (val)
      f << val << ",";
    else
      f << "_,";
  }
  f << std::dec << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "Layout-" << id << "[B]:";
  double margins[4];
  f << "margins=[";
  for (int i=0; i < 4; ++i) {
    margins[i]=double(input->readLong(4))/72.;
    f << margins[i] << ",";
  }
  f << "],";
  for (int i=0; i<3; ++i) {
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  f << "id=" << std::hex << input->readULong(4) << std::dec << ",";
  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  pos=input->tell();
  f.str("");
  f << "Layout-" << id << "[C]:";
  for (int i=0; i<8; i++) {
    val=(int) input->readLong(1);
    if (val==1) f << "fl" << i << ",";
    else if (val) f << "fl" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  //--- now some big unknown zones
  for (int i=0; i<5; i++) {
    pos=input->tell();
    f.str("");
    f << "Layout-" << id << "[C-" << i << "]:";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+244, librevenge::RVNG_SEEK_SET);
  }
  pos=input->tell();
  f.str("");
  f << "Layout-" << id << "[C-5]:";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(pos+254, librevenge::RVNG_SEEK_SET);
  pos=input->tell();
  f.str("");
  f << "Layout-" << id << "[C-6]:";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(pos+1412, librevenge::RVNG_SEEK_SET);

  //--- end of unknown zone
  pos=input->tell();
  f.str("");
  f << "Layout-" << id << "[D]:";
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (!input->checkPosition(pos+2+2*N)) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::readLayouts: can find zone D\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  if (N) {
    f << "lists=[";
    for (int i=0; i<N; ++i) f << input->readLong(2) << ",";
    f << "],";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "Layout-" << id << "[fields]:";
  val=(int) input->readULong(2);
  int type=(int) input->readULong(2);
  N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (type!=0x5a || !input->checkPosition(pos+6+36*N)) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::readLayouts: can find field zone \n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i=0; i<N; i++) {
    pos=input->tell();
    f.str("");
    f << "Layout-" << id << "[field" << i << "]:";
    val=(int) input->readLong(2);
    if (val!=i) {
      MWAW_DEBUG_MSG(("BeagleWksDBParser::readLayouts: old field id\n"));
      f << "##id=" << val << ",";
    }
    val=(int) input->readLong(2);
    if (val!=0x4b) f << "f0=" << val << ",";
    for (int j=0; j<2; ++j) {
      f << "font" << j << "=[";
      f << "sz=" << input->readLong(2) << ",";
      f << "id=" << input->readULong(2) << ",";
      val=(int) input->readULong(2);
      if (val) f << "flags=" << std::hex << val << std::dec << ",";
      val=(int) input->readULong(2);
      if (val!=73) f << "flags1=" << std::hex << val << std::dec << ",";
      f << "],";
    }
    for (int j=0; j<2; ++j) {
      int dim[4];
      for (int k=0; k<4; ++k) dim[k]=(int) input->readLong(2);
      f << "box" << j << "=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+36, librevenge::RVNG_SEEK_SET);
  }

  pos=input->tell();
  f.str("");
  f << "Layout-" << id << "[list]:";
  dSz=(int) input->readULong(2);
  type=(int) input->readLong(2);
  N=(int) input->readULong(2);
  endPos=pos+6+dSz;
  if (2*N>dSz || type!=0x75 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::readLayouts: can read zone F\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  f << "N=" << N << ",";
  if (val!=0x75) f << "f0=" << val << ",";
  f << "lists=[";
  for (int i=0; i<N; ++i) f << input->readLong(2) << ",";
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}
////////////////////////////////////////////////////////////
// send data
////////////////////////////////////////////////////////////
bool BeagleWksDBParser::sendPageFrames()
{
  std::map<int, BeagleWksStructManager::Frame> const &frameMap = m_structureManager->getIdFrameMap();
  std::map<int, BeagleWksStructManager::Frame>::const_iterator it;
  for (it=frameMap.begin(); it!=frameMap.end(); ++it)
    sendFrame(it->second);
  return true;
}

bool BeagleWksDBParser::sendFrame(BeagleWksStructManager::Frame const &frame)
{
  MWAWPosition fPos(Vec2f(0,0), frame.m_dim, librevenge::RVNG_POINT);

  fPos.setPagePos(frame.m_page > 0 ? frame.m_page : 1, frame.m_origin);
  fPos.setRelativePosition(MWAWPosition::Page);
  fPos.m_wrapping = frame.m_wrap==0 ? MWAWPosition::WNone : MWAWPosition::WDynamic;

  MWAWGraphicStyle style=MWAWGraphicStyle::emptyStyle();
  style.setBorders(frame.m_bordersSet, frame.m_border);
  return sendPicture(frame.m_pictId, fPos, style);
}

// read/send picture (edtp resource)
bool BeagleWksDBParser::sendPicture
(int pId, MWAWPosition const &pictPos, MWAWGraphicStyle const &style)
{
  MWAWSpreadsheetListenerPtr listener=getSpreadsheetListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::sendPicture: can not find the listener\n"));
    return false;
  }
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  if (!rsrcParser) {
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("BeagleWksDBParser::sendPicture: need access to resource fork to retrieve picture content\n"));
      first=false;
    }
    return true;
  }

  librevenge::RVNGBinaryData data;
  if (!m_structureManager->readPicture(pId, data))
    return false;

  listener->insertPicture(pictPos, data, "image/pict", style);
  return true;
}

bool BeagleWksDBParser::sendText(MWAWEntry entry, bool /*headerFooter*/)
{
  MWAWSpreadsheetListenerPtr listener=getSpreadsheetListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::sendText: can not find the listener\n"));
    return false;
  }
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::sendText: can not find the entry\n"));
    return false;
  }

  MWAWInputStreamPtr &input= getInput();
  long endPos=entry.end();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  while (!input->isEnd()) {
    long pos=input->tell();
    if (pos>=endPos) break;
    unsigned char c = (unsigned char) input->readULong(1);
    switch (c) {
    case 0x9:
      listener->insertTab();
      break;
    case 0xd:
      listener->insertEOL();
      break;
    default:
      listener->insertCharacter((unsigned char) c);
      break;
    }
  }
  return true;
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
bool BeagleWksDBParser::sendDatabase()
{
  MWAWSpreadsheetListenerPtr listener=getSpreadsheetListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::sendDatabase: I can not find the listener\n"));
    return false;
  }
  MWAW_DEBUG_MSG(("BeagleWksDBParser::sendDatabase: not implemented\n"));
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
bool BeagleWksDBParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = BeagleWksDBParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->checkPosition(66))
    return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";

  input->seek(0, librevenge::RVNG_SEEK_SET);
  if (input->readLong(2)!=0x4257 || input->readLong(2)!=0x6b73 ||
      input->readLong(2)!=0x4257 || input->readLong(2)!=0x6462 ||
      input->readLong(2)!=0x4257 || input->readLong(2)!=0x6462) {
    return false;
  }
  for (int i=0; i < 9; ++i) { // f2=f6=1 other 0
    int val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  setVersion(1);

  if (header)
    header->reset(MWAWDocument::MWAW_T_BEAGLEWORKS, 1, MWAWDocument::MWAW_K_DATABASE);

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  long pos=input->tell();
  f.str("");
  f << "FileHeader-II:";
  m_state->m_databaseBegin=input->readLong(4);
  if (!input->checkPosition(m_state->m_databaseBegin)) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::checkHeader: can not read the database position\n"));
    return false;
  }
  f << "database[ptr]=" << std::hex << m_state->m_databaseBegin << std::dec << ",";
  for (int i=0; i < 11; ++i) { // f2=0x50c|58c|5ac f3=f5=9
    long val=input->readLong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  MWAWEntry entry;
  entry.setBegin(input->readLong(4));
  entry.setLength(input->readLong(4));
  entry.setId((int) input->readLong(2)); // in fact nFonts
  entry.setType("FontNames");
  f << "fontNames[ptr]=" << std::hex << entry.begin() << "<->" << entry.end()
    << std::dec << ",nFonts=" << entry.id() << ",";
  if (entry.length() && (!entry.valid() || !input->checkPosition(entry.end()))) {
    MWAW_DEBUG_MSG(("BeagleWksDBParser::checkHeader: can not read the font names position\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }

  m_state->m_typeEntryMap.insert
  (std::multimap<std::string, MWAWEntry>::value_type(entry.type(),entry));
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  if (strict && !readPrintInfo())
    return false;
  ascii().addPos(66);
  ascii().addNote("_");

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
