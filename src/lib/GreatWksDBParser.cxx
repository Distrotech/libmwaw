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

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSection.hxx"
#include "MWAWSpreadsheetListener.hxx"
#include "MWAWSubDocument.hxx"

#include "GreatWksDocument.hxx"
#include "GreatWksGraph.hxx"
#include "GreatWksText.hxx"

#include "GreatWksDBParser.hxx"

/** Internal: the structures of a GreatWksDBParser */
namespace GreatWksDBParserInternal
{
/**a style of a GreatWksDBParser */
class Style
{
public:
  /** constructor */
  Style() : m_font(3,10), m_backgroundColor(MWAWColor::white())
  {
  }
  /** the font style */
  MWAWFont m_font;
  /** the cell background color */
  MWAWColor m_backgroundColor;
};

/**a big zone header of a GreatWksDBParser */
struct BlockHeader {
  /** constructor */
  BlockHeader() : m_name("")
  {
    for (int i=0; i<3; ++i) m_ptr[i]=0;
  }
  /** returns true if the block is empty */
  bool isEmpty() const
  {
    for (int i=0; i<3; ++i) if (m_ptr[i]) return false;
    return true;
  }
  /** returns true if the entry has many block */
  bool isComplex() const
  {
    return m_ptr[0]!=m_ptr[1] || m_ptr[0]!=m_ptr[2];
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, BlockHeader const &entry)
  {
    o << std::hex << entry.m_ptr[0];
    if (entry.isComplex()) o << "[" << entry.m_ptr[1] << "," << entry.m_ptr[2] << "]";
    o << std::dec;
    if (!entry.m_name.empty()) o << ":" << entry.m_name;
    return o;
  }

  /** the list of pointer: first, next, end? */
  long m_ptr[3];
  /** the block name */
  std::string m_name;
};

/** a big block of a GreatWksDBParser */
struct Block {
  //! a small block of a GreatWksDBParserInternal::Block
  struct Zone {
    /** constructor */
    Zone() : m_ptr(0), m_N(0), m_dataSize(0)
    {
    }
    /** the begin zone pointer */
    long m_ptr;
    /** the number of sub data */
    int m_N;
    /** the data size */
    long m_dataSize;
  };
  //! constructor
  Block(BlockHeader const &header) : m_header(header), m_zoneList()
  {
  }
  //! constructor given a zone
  Block(BlockHeader const &header, Zone &zone) : m_header(header), m_zoneList()
  {
    m_zoneList.push_back(zone);
  }
  //! destructor
  ~Block()
  {
  }
  //! returns true if the zone list is empty
  bool isEmpty() const
  {
    return m_zoneList.empty();
  }
  //! returns the number of zone
  size_t getNumZones() const
  {
    return m_zoneList.size();
  }
  //! returns the ith zone
  Zone const &getZone(size_t i) const
  {
    if (i>=m_zoneList.size()) {
      MWAW_DEBUG_MSG(("GreatWksDBParserInternal::Block: can not find zone %d\n", int(i)));
      static Zone empty;
      return empty;
    }
    return m_zoneList[i];
  }
  //! the corresponding entry header
  BlockHeader m_header;
  //! the zone list
  std::vector<Zone> m_zoneList;
private:
  Block(Block const &orig);
  Block &operator=(Block const &orig);
};

/** a field of a GreatWksDBParser */
struct Field {
  //! the file type
  enum Type { F_Unknown, F_Text, F_Number, F_Date, F_Time, F_Memo, F_Picture, F_Formula, F_Summary };
  //! constructor
  Field() : m_type(F_Unknown), m_id(-1), m_name(""), m_linkZone(0), m_recordBlock(), m_extra("")
  {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Field const &field);
  //! the field type
  Type m_type;
  //! the field id
  int m_id;
  //! the field name
  std::string m_name;
  //! the file position which stores the position link to record zone
  long m_linkZone;
  //! the block file position which stores the position of the field's record
  BlockHeader m_recordBlock;
  //! extra data
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, Field const &field)
{
  switch (field.m_type) {
  case Field::F_Text:
    o << "text,";
    break;
  case Field::F_Number:
    o << "number,";
    break;
  case Field::F_Date:
    o << "date,";
    break;
  case Field::F_Time:
    o << "time,";
    break;
  case Field::F_Picture:
    o << "picture,";
    break;
  case Field::F_Memo:
    o << "memo,";
    break;
  case Field::F_Formula:
    o << "formula,";
    break;
  case Field::F_Summary:
    o << "summary,";
    break;
  case Field::F_Unknown:
    break;
  default:
    MWAW_DEBUG_MSG(("GreatWksDBParser::Field:operator<<: find unexpected type\n"));
    break;
  }
  if (field.m_id>=0) o << "id=" << field.m_id << ",";
  if (!field.m_name.empty()) o << "name=" << field.m_name << ",";
  if (field.m_linkZone>0) o << "zone[link]=" << std::hex << field.m_linkZone << std::dec << ",";
  if (!field.m_recordBlock.isEmpty()) o << "zone[record]=" << field.m_recordBlock << ",";
  o << field.m_extra;
  return o;
}

/** a cell of a GreatWksDBParser */
class Cell : public MWAWCell
{
public:
  /// constructor
  Cell() : m_content(), m_style(-1) { }
  //! returns true if the cell do contain any content
  bool isEmpty() const
  {
    return m_content.empty() && !hasBorders();
  }

  //! the cell content
  MWAWCellContent m_content;
  /** the cell style */
  int m_style;
};

/** the database of a MsWksDBParser */
class Database
{
public:
  //! constructor
  Database() : m_numRecords(0), m_fieldList(), m_widthDefault(75), m_widthCols(), m_heightDefault(13), m_heightRows(),
    m_cells(), m_name("Sheet0")
  {
  }
  //! returns the row size in point
  int getRowHeight(int row) const
  {
    if (row>=0&&row<(int) m_heightRows.size())
      return m_heightRows[size_t(row)];
    return m_heightDefault;
  }
  //! convert the m_widthCols in a vector of of point size
  std::vector<float> convertInPoint(std::vector<int> const &list) const
  {
    size_t numCols=size_t(getRightBottomPosition()[0]+1);
    std::vector<float> res;
    res.resize(numCols);
    for (size_t i = 0; i < numCols; i++) {
      if (i>=list.size() || list[i] < 0) res[i] = float(m_widthDefault);
      else res[i] = float(list[i]);
    }
    return res;
  }
  //! the number of records
  int m_numRecords;
  //! the list of field
  std::vector<Field> m_fieldList;
  /** the default column width */
  int m_widthDefault;
  /** the column size in points */
  std::vector<int> m_widthCols;
  /** the default row height */
  int m_heightDefault;
  /** the row height in points */
  std::vector<int> m_heightRows;
  /** the list of not empty cells */
  std::vector<Cell> m_cells;
  /** the database name */
  std::string m_name;
protected:
  /** returns the last Right Bottom cell position */
  Vec2i getRightBottomPosition() const
  {
    int maxX = 0, maxY = 0;
    size_t numCell = m_cells.size();
    for (size_t i = 0; i < numCell; i++) {
      Vec2i const &p = m_cells[i].position();
      if (p[0] > maxX) maxX = p[0];
      if (p[1] > maxY) maxY = p[1];
    }
    return Vec2i(maxX, maxY);
  }
};

////////////////////////////////////////
//! Internal: the state of a GreatWksDBParser
struct State {
  //! constructor
  State() : m_database(), m_idZonesMap(), m_blocks(), m_styleList(), m_actPage(0), m_numPages(0),
    m_headerBlockHeader(), m_footerBlockHeader(), m_headerPrint(false), m_footerPrint(false), m_headerHeight(0), m_footerHeight(0)
  {
  }
  //! returns the style corresponding to an id
  Style getStyle(int id) const
  {
    if (id<0 || id>=int(m_styleList.size())) {
      MWAW_DEBUG_MSG(("GreatWksDBParserInternal::State: can not find the style %d\n", id));
      return Style();
    }
    return m_styleList[size_t(id)];
  }
  /** the database */
  Database m_database;
  /** a map id to zone used to stored the small zones */
  std::map<int, MWAWEntry> m_idZonesMap;
  /** the list of big blocks */
  std::vector<BlockHeader> m_blocks;
  /** the list of style */
  std::vector<Style> m_styleList;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  MWAWEntry m_headerBlockHeader /** the header entry (in v2)*/, m_footerBlockHeader/**the footer entry (in v2)*/;
  bool m_headerPrint /** the header is printed */, m_footerPrint /* the footer is printed */;
  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a GreatWksDBParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(GreatWksDBParser &pars, MWAWInputStreamPtr input, int zoneId) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const
  {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  //! the subdocument id
  int m_id;
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("GreatWksDBParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  if (type!=libmwaw::DOC_HEADER_FOOTER) {
    MWAW_DEBUG_MSG(("GreatWksDBParserInternal::SubDocument::parse: unknown type\n"));
    return;
  }

  assert(m_parser);

  long pos = m_input->tell();
  static_cast<GreatWksDBParser *>(m_parser)->sendHF(m_id);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
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
GreatWksDBParser::GreatWksDBParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWSpreadsheetParser(input, rsrcParser, header), m_state(), m_document()
{
  init();
}

GreatWksDBParser::~GreatWksDBParser()
{
}

void GreatWksDBParser::init()
{
  resetSpreadsheetListener();
  setAsciiName("main-1");

  m_state.reset(new GreatWksDBParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);

  m_document.reset(new GreatWksDocument(*this));
}

////////////////////////////////////////////////////////////
// interface with the text parser
////////////////////////////////////////////////////////////
bool GreatWksDBParser::sendHF(int id)
{
  return m_document->getTextParser()->sendTextbox(id==0 ? m_state->m_headerBlockHeader : m_state->m_footerBlockHeader);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void GreatWksDBParser::parse(librevenge::RVNGSpreadsheetInterface *docInterface)
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
    MWAW_DEBUG_MSG(("GreatWksDBParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetSpreadsheetListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void GreatWksDBParser::createDocument(librevenge::RVNGSpreadsheetInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getSpreadsheetListener()) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  int numPages = 1;
  if (m_document->getGraphParser()->numPages() > numPages)
    numPages = m_document->getGraphParser()->numPages();
  if (m_document->getTextParser()->numPages() > numPages)
    numPages = m_document->getTextParser()->numPages();
  m_state->m_numPages = numPages;

  MWAWPageSpan ps(getPageSpan());
  std::vector<MWAWPageSpan> pageList;
  if (m_state->m_headerBlockHeader.valid()) {
    MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
    header.m_subDocument.reset(new GreatWksDBParserInternal::SubDocument(*this, getInput(), 0));
    ps.setHeaderFooter(header);
  }
  if (m_state->m_footerBlockHeader.valid()) {
    MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    footer.m_subDocument.reset(new GreatWksDBParserInternal::SubDocument(*this, getInput(), 1));
    ps.setHeaderFooter(footer);
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
bool GreatWksDBParser::createZones()
{
  m_document->readRSRCZones();

  if (!readHeader()) return false;

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;

  for (size_t i=0; i< m_state->m_blocks.size() ; ++i) {
    GreatWksDBParserInternal::BlockHeader &header = m_state->m_blocks[i];
    if (header.isEmpty()) continue;
    shared_ptr<GreatWksDBParserInternal::Block> block=createBlock(header);
    if (block) {
      bool ok=false;
      switch (i) {
      case 0: // unknown maybe some flag
        ok = readBlockHeader0(*block);
        break;
      case 1: // look like a list of position
      case 2: // checkme: seems similar to zone 1 so...
        ok = readRecordPosition(*block);
        break;
      case 3: // the record list
        ok = readRecordList(*block);
        break;
      default:
        break;
      }
      if (ok)
        continue;
    }
    f.str("");
    f << "Entries(" << header.m_name << ")[" << header << "]:";
    ascii().addPos(input->tell());
    ascii().addNote(f.str().c_str());
    if (!header.isComplex()) continue;
    for (int j=1; j<3; ++j) {
      if (!header.m_ptr[j]) continue;
      ascii().addPos(header.m_ptr[j]);
      ascii().addNote(f.str().c_str());
    }
  }

  std::map<int,MWAWEntry>::const_iterator it;
  for (it=m_state->m_idZonesMap.begin(); it!=m_state->m_idZonesMap.end(); ++it) {
    MWAWEntry const &entry=it->second;
    if (!entry.valid() || entry.isParsed()) continue;
    bool ok=false;
    switch (entry.id()) {
    // zones which will be parsed in readDatabase
    case 1: // the list of fields
    case 10: // the link between field and a zone record
    case 13: // a version 2 zone which seems to contains added data for each fields
      ok=true;
      break;
    case 2: // a list of Zone9?
      ok=readZone2(it->second);
      break;
    case 5: // one time with list=[19, 20], list of computed fields ?
    case 6: // one time with list=[21, 22], list of summary fields ?
    case 14: { // only in v2, probably linked to Zone2 zones, find [1,1,1] or [2,2]
      std::vector<int> list;
      ok=readIntList(it->second, list);
      if (!ok) break;
      f.str("");
      f << "Entries(" << entry.name() << "):";
      if (list.size()) {
        f << "lists=[";
        for (size_t i=0; i<list.size(); ++i) f << list[i] << ",";
        f << "],";
      }
      ascii().addPos(entry.begin());
      ascii().addNote(f.str().c_str());
      break;
    }
    case 7:  // always find with size=0
      if (entry.length()!=6) break;
      ok=true;
      f.str("");
      f << "Entries(" << entry.name() << "):";
      ascii().addPos(entry.begin());
      ascii().addNote(f.str().c_str());
      ascii().addPos(entry.end());
      ascii().addNote("_");
      break;
    case 12:
      ok=readZone12(entry);
      break;
    // zone3: never find with data, dSz=3C in v1 and dSz=3e in v2
    // zone4: never find with data, dSz=112
    // zone8: never seems
    default:
      ok=readSmallZone(entry);
      break;
    }
    if (ok || entry.isParsed()) continue;
    f.str("");
    f << "Entries(" << entry.name() << "):#";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
    ascii().addPos(entry.end());
    ascii().addNote("_");
  }

  if (!readDatabase())
    return false;

  if (!input->isEnd()) {
    long pos = input->tell();
    MWAW_DEBUG_MSG(("GreatWksDBParser::createZones: find some extra data\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Loose):");
    ascii().addPos(pos+100);
    ascii().addNote("_");
  }

  return true;
}

////////////////////////////////////////////////////////////
// read a database
////////////////////////////////////////////////////////////
bool GreatWksDBParser::readDatabase()
{
  MWAWInputStreamPtr input = getInput();
  std::map<int,MWAWEntry>::const_iterator it=m_state->m_idZonesMap.find(1);
  if (it==m_state->m_idZonesMap.end() || !readFields(it->second)) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readDatabase: can not find the list of fields\n"));
    return false;
  }
  if (version()==2) {
    // zone13: a v2 zone with dSz=46, content seems constant in a file
    it=m_state->m_idZonesMap.find(13);
    if (it!=m_state->m_idZonesMap.end())
      readSmallZone(it->second);
  }
  GreatWksDBParserInternal::Database &database=m_state->m_database;
  for (size_t i=0; i < database.m_fieldList.size(); ++i) {
    if (!database.m_fieldList[i].m_linkZone) continue;
    readFieldLink(database.m_fieldList[i]);
  }
  for (size_t i=0; i < database.m_fieldList.size(); ++i) {
    if (database.m_fieldList[i].m_recordBlock.isEmpty()) continue;
    readRecords(database.m_fieldList[i]);
  }
  return false;
}

////////////////////////////////////////////////////////////
// read the file header
////////////////////////////////////////////////////////////
bool GreatWksDBParser::readHeader()
{
  MWAWInputStreamPtr input = getInput();
  if (!input->checkPosition(0x200))
    return false;
  int const vers=version();
  libmwaw::DebugStream f;

  f << "Entries(HeaderZone):";
  long pos=16;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<3; ++i) {
    GreatWksDBParserInternal::BlockHeader block;
    if (!readBlockHeader(block)) {
      MWAW_DEBUG_MSG(("GreatWksDBParser::readHeader: can not read a entry\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return false;
    }
    static char const *(wh[])= {"Block0", "RecPos1", "RecPos2"};
    block.m_name=wh[i];
    m_state->m_blocks.push_back(block);
    f << block << ",";
    for (int j=0; j<4; ++j) {
      int val=(int) input->readULong(2);
      if (val)
        f << "f" << j << "[" << i << "]=" << val << ",";
    }
  }
  // an odd entry block (only 2 ptr), does this means that this zone has a size less than 512?
  GreatWksDBParserInternal::BlockHeader block;
  for (int i=0; i<2; ++i) block.m_ptr[i]=(long) input->readULong(4);
  block.m_ptr[2]=block.m_ptr[1];
  block.m_name="RecList";
  m_state->m_blocks.push_back(block);
  f << block << ",";

  int val=(int) input->readLong(2); // always 0
  if (val) f << "f4=" << val << ",";
  int numRecords=(int) input->readLong(2);
  m_state->m_database.m_numRecords=numRecords;
  f << "num[records]=" << numRecords << ",";
  for (int i=0; i<16; ++i) { // find h1=h9=h11=h13=numRecords, h3=h7=1|11, h14=2, h15=1
    val=(int) input->readLong(2);
    if (i==1 || i==9 || i==11 || i==13) {
      if (val==numRecords) continue;
      if (i==9) f << "act[record]=" << val << ",";
      else f << "h" << i << "=" << val << ",";
    }
    else if (val) f << "h" << i << "=" << std::hex << val << std::dec << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "Entries(Zones):";
  std::vector<MWAWEntry> zones;
  for (int i=0; i<9; ++i) {
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
    long ptr=(long) input->readULong(2);
    MWAWEntry entry;
    entry.setBegin(ptr);
    zones.push_back(entry);
    if (!ptr) continue;
    f << "Zone" << i << "A=" << std::hex << ptr << std::dec << ",";
  }
  if (vers==1) {
    for (int i=0; i<40; ++i) {
      val=(int) input->readLong(2);
      if (val!=-1) f << "g" << i << "=" << val << ",";
    }
  }
  else {
    // no sure what is the maximal number of potential added zones
    for (int i=0; i<20; ++i) {
      long ptr=(long) input->readULong(2);
      val=(int) input->readLong(2);
      if (val) f << "g" << i << "=" << val << ",";
      MWAWEntry entry;
      entry.setBegin(ptr);
      zones.push_back(entry);
      if (!ptr) continue;
      f << "Zone" << 9+i << "A=" << std::hex << ptr << std::dec << ",";
    }
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // from here, probably only junk in v1 and list of 0 + unkn number in v2
  ascii().addPos(input->tell());
  ascii().addNote("HeaderZone:end");

  if (vers==1) {
    // the list of record pointer does not seem to be present in v1
    MWAWEntry entry;
    entry.setBegin(0x200);
    entry.setId(10);
    entry.setName("Record");
    if (std::find(zones.begin(), zones.end(), entry)==zones.end())
      zones.push_back(entry);
  }

  for (size_t i=0; i<zones.size(); ++i) {
    MWAWEntry &entry=zones[i];
    if (!entry.begin()) continue;
    if (!checkSmallZone(entry)) {
      MWAW_DEBUG_MSG(("GreatWksDBParser::readHeader: find a bad zone %d\n", (int) i));
      entry.setLength(0);
      continue;
    }
    int const id=entry.id();
    if (m_state->m_idZonesMap.find(id)==m_state->m_idZonesMap.end())
      m_state->m_idZonesMap[id]=entry;
    else {
      MWAW_DEBUG_MSG(("GreatWksDBParser::readHeader: the zone entry %d already exists\n", id));
      f.str("");
      f << "Entries(" << entry.name() << "):###";
      ascii().addPos(entry.begin());
      ascii().addNote(f.str().c_str());
      ascii().addPos(entry.end());
      ascii().addNote("_");
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
// block gestion
////////////////////////////////////////////////////////////
bool GreatWksDBParser::readBlockHeader(GreatWksDBParserInternal::BlockHeader &entry)
{
  MWAWInputStreamPtr input = getInput();
  if (!input->checkPosition(input->tell()+12)) return false;
  for (int i=0; i<3; ++i) entry.m_ptr[i]=(long) input->readULong(4);
  return true;
}

shared_ptr<GreatWksDBParserInternal::Block> GreatWksDBParser::createBlock(GreatWksDBParserInternal::BlockHeader &entry)
{
  shared_ptr<GreatWksDBParserInternal::Block> res;

  MWAWInputStreamPtr input = getInput();
  long pos=entry.m_ptr[0];
  if (!pos || !input->checkPosition(pos+10)) return res;

  libmwaw::DebugStream f;
  std::set<long> seens;
  while (pos) {
    if (!input->checkPosition(pos+256) || seens.find(pos)!=seens.end()) {
      MWAW_DEBUG_MSG(("GreatWksDBParser::createBlock: find an old position\n"));
      ascii().addPos(pos);
      ascii().addNote("Entries(BlockHeader):###");
      return res;
    }
    seens.insert(pos);
    f.str("");
    f << "Entries(BlockHeader):";
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    int type=(int) input->readULong(1);
    f << "type=" << std::hex << type << std::dec << ",";
    if ((type&0xF0) != 0x40) {
      MWAW_DEBUG_MSG(("GreatWksDBParser::createBlock: find old block type\n"));
      f << "####";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return res;
    }
    int val=(int) input->readULong(1);
    if (val!=0x7F) // find fl=0 with type=44
      f << "fl=" << std::hex << val << std::dec << ",";
    GreatWksDBParserInternal::Block::Zone zone;
    zone.m_N = (int) input->readULong(2);
    if (zone.m_N)
      f << "N=" << zone.m_N << ",";
    if ((type&0x4)==0)
      zone.m_dataSize = (int) input->readULong(2);
    if (zone.m_dataSize)
      f << "dataSize=" << zone.m_dataSize << ",";
    if (type==0x41 || type==0x44) { // link to type or to flag
      val=(int) input->readULong(4);
      if (val) f << "prev=" << std::hex << val << std::dec << ",";
    }
    long next=(int) input->readULong(4);
    if (next) {
      f << "next=" << std::hex << next << std::dec << ",";
      if ((next&0x1FF)) {
        MWAW_DEBUG_MSG(("GreatWksDBParser::createBlock: the next zone seems bad(ignored it)\n"));
        f << "###";
        next=0;
      }
    }

    zone.m_ptr=input->tell();
    if (!res) res.reset(new GreatWksDBParserInternal::Block(entry));
    switch (type) {
    case 0x40: // a root with no data?
      ascii().addPos(input->tell());
      ascii().addNote("_");
      break;
    case 0x41: // ok
    case 0x44: // no dataSize
      res->m_zoneList.push_back(zone);
      break;
    default:
      res->m_zoneList.push_back(zone);
      MWAW_DEBUG_MSG(("GreatWksDBParser::createBlock: find some unexpected type %d, try to continue\n", type));
      f << "###";
      // let's add this zone, but ignored potential follower
      next=0;
      break;
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    pos=next;
  }
  return res;
}

////////////////////////////////////////////////////////////
// functions to read the known block structure
////////////////////////////////////////////////////////////
bool GreatWksDBParser::readRecordList(GreatWksDBParserInternal::Block &block)
{
  MWAWInputStreamPtr input = getInput();
  GreatWksDBParserInternal::BlockHeader const &header=block.m_header;
  libmwaw::DebugStream f;
  for (size_t z=0; z<block.getNumZones(); ++z) {
    GreatWksDBParserInternal::Block::Zone const &zone=block.getZone(z);
    long pos=zone.m_ptr;
    if (!pos || !input->checkPosition(pos+zone.m_N*4)) {
      MWAW_DEBUG_MSG(("GreatWksDBParser::readRecordList: the zone seems too short\n"));
      f.str("");
      f << "Entries(" << header.m_name << ")[" << header << "]:###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      continue;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << "Entries(" << header.m_name << "):list=[";
    for (int i=0; i< zone.m_N; ++i) f << input->readLong(4) << ",";
    f << "]";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    ascii().addPos(input->tell());
    ascii().addNote("_");
  }
  return true;
}

bool GreatWksDBParser::readRecordPosition(GreatWksDBParserInternal::Block &block)
{
  MWAWInputStreamPtr input = getInput();
  GreatWksDBParserInternal::BlockHeader const &header=block.m_header;
  libmwaw::DebugStream f;
  for (size_t z=0; z<block.getNumZones(); ++z) {
    GreatWksDBParserInternal::Block::Zone const &zone=block.getZone(z);
    long pos=zone.m_ptr;
    if (!pos || !input->checkPosition(pos+zone.m_N*8)) {
      MWAW_DEBUG_MSG(("GreatWksDBParser::readRecordPosition: the zone seems too short\n"));
      f.str("");
      f << "Entries(" << header.m_name << ")[" << header << "]:###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      continue;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << "Entries(" << header.m_name << "):dim=[";
    for (int i=0; i<zone.m_N; ++i) {
      // checkme: very often a multiple of 72., so maybe a dimension
      f << double(input->readULong(4))/72. << ":";
      f << input->readLong(4) << ","; // record id
    }
    f << "]";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    ascii().addPos(input->tell());
    ascii().addNote("_");
  }
  return true;
}

bool GreatWksDBParser::readBlockHeader0(GreatWksDBParserInternal::Block &block)
{
  MWAWInputStreamPtr input = getInput();
  GreatWksDBParserInternal::BlockHeader const &header=block.m_header;
  libmwaw::DebugStream f;
  for (size_t z=0; z<block.getNumZones(); ++z) {
    GreatWksDBParserInternal::Block::Zone const &zone=block.getZone(z);
    long pos=zone.m_ptr;
    if (!pos || !input->checkPosition(pos+zone.m_N*8)) {
      MWAW_DEBUG_MSG(("GreatWksDBParser::readBlockHeader0: the zone seems too short\n"));
      f.str("");
      f << "Entries(" << header.m_name << ")[" << header << "]:###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      continue;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << "Entries(" << header.m_name << "):flags=[";
    for (int i=0; i<zone.m_N; ++i) {
      /* look like 40000020:000055e0 or 00003dc0:40000040
         ie { 400000000|small number and some number }
         maybe some flags
      */
      f << std::hex << input->readULong(4) << ":" << input->readULong(4) << std::dec << ",";
    }
    f << "]";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    ascii().addPos(input->tell());
    ascii().addNote("_");
  }
  return true;
}

bool GreatWksDBParser::readBlock(GreatWksDBParserInternal::Block &block, int fieldSize)
{
  MWAWInputStreamPtr input = getInput();
  GreatWksDBParserInternal::BlockHeader const &header=block.m_header;
  libmwaw::DebugStream f;
  for (size_t z=0; z<block.getNumZones(); ++z) {
    GreatWksDBParserInternal::Block::Zone const &zone=block.getZone(z);
    long pos=zone.m_ptr;
    if (fieldSize<=0 || !pos || !input->checkPosition(pos+zone.m_N*fieldSize)) {
      MWAW_DEBUG_MSG(("GreatWksDBParser::readBlock: the fieldSize seems bad\n"));
      f.str("");
      f << "Entries(" << header.m_name << ")[" << header << "]:###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      continue;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    for (int i=0; i<zone.m_N; ++i) {
      pos=input->tell();
      f.str("");
      f << "Entries(" << header.m_name << ")[" << i << "]:";
      input->seek(pos+fieldSize, librevenge::RVNG_SEEK_SET);
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
    }
    ascii().addPos(input->tell());
    ascii().addNote("_");
  }
  return true;
}

////////////////////////////////////////////////////////////
// check if entry.begin() corresponds or not to a small zone
////////////////////////////////////////////////////////////
bool GreatWksDBParser::checkSmallZone(MWAWEntry &entry)
{
  if (entry.begin()<=0) return false;
  MWAWInputStreamPtr input = getInput();
  if (!input->checkPosition(entry.begin()+6)) return false;

  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  int id=(int) input->readLong(2);
  entry.setId(id);
  /* the zone with id=10 is in fact a list of zone with id=10, so
     its length will be underestimate */
  entry.setLength(6+(long) input->readULong(4));
  if (id>=0 && id<15) {
    static char const *(names[])= {
      "Zone0A", "Field", "Zone2A", "Zone3A", "Zone4A", "ListForm", "ListSum", "Zone7A",
      "Zone8A", "Zone9A", "Record", "Zone11A", "Zone12A", "FldAuxi", "Zone14A"
    };
    entry.setName(names[id]);
  }
  else {
    std::stringstream s;
    s << "Zone" << id << "A";
    entry.setName(s.str());
  }
  return input->checkPosition(entry.begin()+6);
}

////////////////////////////////////////////////////////////
// read the fields zone
////////////////////////////////////////////////////////////
bool GreatWksDBParser::readFields(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<10) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readFields: the entry length seems bad\n"));
    return false;
  }
  int const vers=version();
  entry.setParsed(true);
  ascii().addPos(entry.end());
  ascii().addNote("_");
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  f << "Entries(Field):";
  input->seek(entry.begin()+6, librevenge::RVNG_SEEK_SET);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  int dSz=(int) input->readULong(2);
  if ((vers==1 && dSz<0x3c) || (vers==2 && dSz<0x3e) || entry.length()<10+N*dSz) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readFields: the number of data seems bad\n"));
    f << "###";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  long pos;
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "Field-" << i << ":";
    GreatWksDBParserInternal::Field field;
    if (readField(field)) {
      m_state->m_database.m_fieldList.push_back(field);
      f << field;
    }
    else {
      MWAW_DEBUG_MSG(("GreatWksDBParser::readFields: can not read a field\n"));
      f << "###";
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+dSz, librevenge::RVNG_SEEK_SET);
  }
  pos=input->tell();
  if (input->tell()!=entry.end()) { // now some string or formula?
    ascii().addPos(pos);
    ascii().addNote("Field:end");
  }

  return true;
}

bool GreatWksDBParser::readField(GreatWksDBParserInternal::Field &field)
{
  field=GreatWksDBParserInternal::Field();
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+0x3c)) return false;
  int type=(int) input->readLong(2);
  libmwaw::DebugStream f;
  switch (type) {
  case 5:
    field.m_type=GreatWksDBParserInternal::Field::F_Number;
    break;
  case 7:
    field.m_type=GreatWksDBParserInternal::Field::F_Text;
    break;
  case 9:
    field.m_type=GreatWksDBParserInternal::Field::F_Date;
    break;
  case 0xa:
    field.m_type=GreatWksDBParserInternal::Field::F_Time;
    break;
  case 0xc:
    field.m_type=GreatWksDBParserInternal::Field::F_Memo;
    break;
  case 0xd:
    field.m_type=GreatWksDBParserInternal::Field::F_Picture;
    break;
  case 0xFF:
    field.m_type=GreatWksDBParserInternal::Field::F_Formula;
    break;
  case 0xFE:
    field.m_type=GreatWksDBParserInternal::Field::F_Summary;
    break;
  default:
    MWAW_DEBUG_MSG(("GreatWksDBParser::readField: find unknown file type\n"));
    f << "#type=" << type << ",";
  }
  int val;
  if (version()==2) { // always 5?
    val=(int) input->readLong(2);
    if (val!=5) f << "f0=" << val << ",";
  }
  for (int i=0; i<8; ++i) { // fl0-6=0|1, fl7=2c|38|ee|c4
    val=(int) input->readULong(1);
    if (val) f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  val=(int) input->readLong(2); // always 0
  if (val) f << "f1=" << val << ",";
  /* f2=f3=f4=0 excepted
     for formula find 8,5bb2b4,1b or 4,5bb200,5
     for summary find 0,0,05bb254 and 0,0,5bb268
  */
  val=(int) input->readLong(2);
  if (val) f << "f2=" << val << ",";
  for (int i=0; i<2; ++i) {
    val=(int) input->readULong(4);
    if (val) f << "f" << 3+i << "=" << std::hex << val << std::dec << ",";
  }
  val=(int) input->readLong(2); // always 0
  if (val) f << "f5=" << val << ",";
  field.m_linkZone=(long) input->readULong(2);
  field.m_id=(int) input->readLong(2);
  int fSz=(int) input->readULong(1);
  if (fSz>31) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readField: the name field size seems to big\n"));
    f << "###";
    ascii().addDelimiter(input->tell(),'|');
  }
  else {
    std::string name("");
    for (int i=0; i<fSz; ++i) name+=(char) input->readULong(1);
    field.m_name=name;
  }
  field.m_extra=f.str();
  return true;
}

////////////////////////////////////////////////////////////
// read a list of records corresponding to a link
////////////////////////////////////////////////////////////
bool GreatWksDBParser::readRecords(GreatWksDBParserInternal::Field &field)
{
  if (field.m_recordBlock.isEmpty()) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldRecord: can not read a record zone\n"));
    return false;
  }
  shared_ptr<GreatWksDBParserInternal::Block> block=createBlock(field.m_recordBlock);
  if (!block) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldRecord: can not find the records input\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  for (size_t z=0; z<block->getNumZones(); ++z) {
    GreatWksDBParserInternal::Block::Zone const &zone=block->getZone(z);
    long pos=zone.m_ptr;
    f.str("");
    f << "Entries(Record)[" << field.m_id << "]:type=" << int(field.m_type) << ",";
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    int const N=zone.m_N;
    int const dSz=(int) zone.m_dataSize;
    if (field.m_type==GreatWksDBParserInternal::Field::F_Text) {
      if (!input->checkPosition(pos+dSz+N*5)) {
        MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldRecord: can not read a text zone\n"));
        f << "###";
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        continue;
      }
      for (int i=0; i<6; ++i) { // always 0?
        int val=(int) input->readLong(2);
        if (val) f << "f" << i << "=" << val << ",";
      }
      std::vector<int> positions, rows;
      // first read the list of positions
      input->seek(pos+dSz, librevenge::RVNG_SEEK_SET);
      for (int i=0; i<N; ++i)
        positions.push_back((int) input->readULong(1));
      // now read a list of small int (the row number?)
      for (int i=0; i<N; ++i)
        rows.push_back((int) input->readULong(4));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      int actPos=0;
      for (size_t i=0; i<(size_t)N; ++i) {
        int nextPos= i+1==size_t(N) ? dSz : positions[i+1];
        if (actPos!=positions[i] || nextPos>dSz) {
          MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldRecord: can not read a text zone, pb with a position\n"));
          f << "###";
          break;
        }
        std::string text("");
        while (actPos<nextPos) {
          char c=(char) input->readULong(1);
          // c==0 can indicate an empty field
          if (c) text += c;
          ++actPos;
        }
        f << "\"" << text << "\":" << rows[i] << ",";
      }
      input->seek(pos+12+dSz+5*N, librevenge::RVNG_SEEK_SET);
    }
    else if (field.m_type==GreatWksDBParserInternal::Field::F_Number ||
             field.m_type==GreatWksDBParserInternal::Field::F_Date ||
             field.m_type==GreatWksDBParserInternal::Field::F_Time) {
      if (!input->checkPosition(pos+14*N)) {
        MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldRecord: can not read a number zone\n"));
        f << "###";
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        continue;
      }
      for (int i=0; i<N; ++i) {
        long fPos=input->tell();
        int row=(int) input->readLong(4);
        double value;
        bool isNan;
        if (!input->readDouble10(value, isNan)) {
          static bool first=true;
          if (first) {
            // can be normal, ie. 7fff403200000000000000 means no value which generates an error
            MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldRecord: can not read a number, maybe a undef number\n"));
            first=false;
          }
          f << "#";
          input->seek(fPos+14, librevenge::RVNG_SEEK_SET);
        }
        else
          f << value;
        f << ":" << row << ",";
      }
    }
    else {
      MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldRecord: does not know how to read list for type=%d\n", int(field.m_type)));
      f << "###";
    }
    ascii().addDelimiter(input->tell(),'|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the links between fields and record zone
////////////////////////////////////////////////////////////
bool GreatWksDBParser::readFieldLink(GreatWksDBParserInternal::Field &field)
{
  MWAWInputStreamPtr input = getInput();
  if (field.m_linkZone<=0 || !input->checkPosition(field.m_linkZone+32)) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldLink: can not read a link between field to records\n"));
    return false;
  }
  input->seek(field.m_linkZone, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(FldLink)[" << field.m_id << "]:";
  int val=(int) input->readLong(2);
  if (val!=0xa) f << "#type=" << val << ",";
  val=(int) input->readLong(2); // always 0
  if (val) f << "f0=" << val << ",";
  val=(int) input->readLong(2);
  if (val!=0x14) f << "f1=" << val << ",";
  if (!readBlockHeader(field.m_recordBlock)) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldLink: the zone position seems bad\n"));
    f << "###block,";
  }
  f << "block=" << field.m_recordBlock << ",";
  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(field.m_linkZone);
  ascii().addNote(f.str().c_str());
  ascii().addPos(field.m_linkZone+32);
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read a list of int
////////////////////////////////////////////////////////////
bool GreatWksDBParser::readIntList(MWAWEntry const &entry, std::vector<int> &list)
{
  list.resize(0);
  if (!entry.valid() || entry.length()<10) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readIntList: the entry length seems bad\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin()+6, librevenge::RVNG_SEEK_SET);
  int N=(int) input->readULong(2);
  int dSz=(int) input->readULong(2);
  if (entry.length()!=10+N*dSz || dSz!=2) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readIntList: the N or dSz value seems bad\n"));
    return false;
  }

  entry.setParsed(true);
  ascii().addPos(entry.end());
  ascii().addNote("_");

  for (int i=0; i<N; ++i)
    list.push_back((int) input->readLong(2));

  return true;
}

////////////////////////////////////////////////////////////
// read a unknown zone of pointers
////////////////////////////////////////////////////////////
bool GreatWksDBParser::readZone2(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<10) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readZone2: the entry length seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  ascii().addPos(entry.end());
  ascii().addNote("_");
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f, f2;
  f << "Entries(" << entry.name() << "):";
  input->seek(entry.begin()+6, librevenge::RVNG_SEEK_SET);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  int dSz=(int) input->readULong(2);
  if (entry.length()!=10+N*dSz || dSz<4) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readZone2: the number of data seems bad\n"));
    f << "###";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  long pos;
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << entry.name() << "-" << i << ":";
    int val=(int) input->readLong(2); // always 0?
    if (val) f << "f0=" << val << ",";
    int ptr=(int) input->readULong(2);
    if (ptr) {
      MWAWEntry zone;
      zone.setBegin(ptr);
      if (checkSmallZone(zone)) {
        f2.str("");
        f2 << "Entries(" << zone.name() << "):";
        ascii().addPos(ptr);
        ascii().addNote(f2.str().c_str());
        ascii().addPos(zone.end());
        ascii().addNote("_");
      }
      else
        f << "###";
      f << "ptr" << i << "=" << std::hex << ptr << std::dec << ",";
    }
    input->seek(pos+dSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  return true;
}

////////////////////////////////////////////////////////////
// read a unknown zone of zone
////////////////////////////////////////////////////////////
bool GreatWksDBParser::readZone12(MWAWEntry const &entry)
{
  int const vers=version();
  if (!entry.valid() || entry.length()!=10+2*vers) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readZone12: the entry length seems bad\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  f << "Entries(" << entry.name() << "):";
  input->seek(entry.begin()+6, librevenge::RVNG_SEEK_SET);

  entry.setParsed(true);
  ascii().addPos(entry.end());
  ascii().addNote("_");

  int val;
  for (int i=0; i<4; ++i) { // f0=f1=0, f2=f3=1
    val=(int) input->readLong(1);
    if (val) f << "f" << i << "=" << val << ",";
  }
  if (vers==2) // b0|2a
    f << "g0=" << input->readLong(2) << ",";
  val=(int) input->readLong(2);
  if (val) // 66|67|7a
    f << "g1=" << val << ",";
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// read unknown small zone
////////////////////////////////////////////////////////////
bool GreatWksDBParser::readSmallZone(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<10) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readSmallZone: the entry length seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  ascii().addPos(entry.end());
  ascii().addNote("_");
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  f << "Entries(" << entry.name() << "):";
  input->seek(entry.begin()+6, librevenge::RVNG_SEEK_SET);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  int dSz=(int) input->readULong(2);
  if (entry.length()!=10+N*dSz) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readSmallZone: the number of data seems bad\n"));
    f << "###";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  long pos;
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << entry.name() << "-" << i << ":";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+dSz, librevenge::RVNG_SEEK_SET);
  }

  return true;
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
bool GreatWksDBParser::sendDatabase()
{
  MWAWSpreadsheetListenerPtr listener=getSpreadsheetListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::sendDatabase: I can not find the listener\n"));
    return false;
  }
  MWAW_DEBUG_MSG(("GreatWksDBParser::sendDatabase: send a database is not implemented\n"));
  return false;
}

////////////////////////////////////////////////////////////
// read the fonts
////////////////////////////////////////////////////////////
bool GreatWksDBParser::readStyles()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  int const vers=version();
  libmwaw::DebugStream f;

  f << "Entries(Style):";
  long sz=(long) input->readULong(4);
  long endPos=pos+4+sz;
  int expectedSize=vers==1 ? 18 : 40;
  if (!input->checkPosition(endPos) || (sz%expectedSize)) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readStyles: can not find the font defs zone\n"));
    f << "###";
    ascii().addPos(pos-2);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos-2);
  ascii().addNote(f.str().c_str());
  int numFonts=int(sz/expectedSize);
  for (int i=0; i <numFonts; ++i) {
    pos=input->tell();
    f.str("");
    f << "Style-" << i << ":";
    GreatWksDBParserInternal::Style style;
    MWAWFont &font=style.m_font;
    int val=(int) input->readLong(2); // always 0 ?
    if (val) f << "#unkn=" << val << ",";
    val=(int) input->readLong(2);
    if (val!=1) f << "used?=" << val << ",";

    font.setId(m_document->getTextParser()->getFontId((int) input->readULong(2)));
    int flag =(int) input->readULong(2);
    uint32_t flags=0;
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0x8) flags |= MWAWFont::embossBit;
    if (flag&0x10) flags |= MWAWFont::shadowBit;
    if (flag&0x20) font.setDeltaLetterSpacing(-1);
    if (flag&0x40) font.setDeltaLetterSpacing(1);
    if (flag&0x100) font.set(MWAWFont::Script::super100());
    if (flag&0x200) font.set(MWAWFont::Script::sub100());
    if (flag&0x800) font.setStrikeOutStyle(MWAWFont::Line::Simple);
    if (flag&0x2000) {
      font.setUnderlineStyle(MWAWFont::Line::Simple);
      font.setUnderlineType(MWAWFont::Line::Double);
    }
    flag &=0xD480;
    if (flag) f << "#fl=" << std::hex << flag << std::dec << ",";
    font.setFlags(flags);
    font.setSize((float) input->readULong(2));
    unsigned char color[3];
    for (int c=0; c<3; ++c)
      color[c] = (unsigned char)(input->readULong(2)>>8);
    font.setColor(MWAWColor(color[0],color[1],color[2]));
    f << font.getDebugString(getParserState()->m_fontConverter) << ",";
    f << "h[line]?=" << input->readULong(2) << ",";
    if (vers==1) {
      m_state->m_styleList.push_back(style);
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      continue;
    }
    MWAWColor bfColors[2];
    for (int j=0; j<2; ++j) {  // front/back color?
      for (int c=0; c<3; ++c)
        color[c] = (unsigned char)(input->readULong(2)>>8);
      MWAWColor col(color[0],color[1],color[2]);
      bfColors[j]=col;
      if ((j==0 && col.isBlack()) || (j==1 && col.isWhite())) continue;
      if (j==0) f << "col[front]=" << MWAWColor(color[0],color[1],color[2]) << ",";
      else f << "col[back]=" << MWAWColor(color[0],color[1],color[2]) << ",";
    }
    int patId=(int) input->readLong(2);
    if (!patId) {
      style.m_backgroundColor=bfColors[1];
      input->seek(8, librevenge::RVNG_SEEK_CUR);
    }
    else {
      f << "pattern[id]=" << patId << ",";
      MWAWGraphicStyle::Pattern pattern;
      pattern.m_dim=Vec2i(8,8);
      pattern.m_data.resize(8);
      for (size_t j=0; j < 8; ++j)
        pattern.m_data[j]=(unsigned char) input->readULong(1);
      pattern.m_colors[0]=bfColors[1];
      pattern.m_colors[1]=bfColors[0];
      pattern.getAverageColor(style.m_backgroundColor);
      f << "pat=[" << pattern << "],";
    }
    m_state->m_styleList.push_back(style);
    ascii().addDelimiter(input->tell(), '|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+expectedSize, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool GreatWksDBParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = GreatWksDBParserInternal::State();
  if (!m_document->checkHeader(header, strict)) return false;
  return getParserState()->m_kind==MWAWDocument::MWAW_K_DATABASE;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
