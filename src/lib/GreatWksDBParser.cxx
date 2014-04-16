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
#include <cmath>
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

/** a cell of a GreatWksDBParser */
class Cell : public MWAWCell
{
public:
  /// constructor
  Cell() : m_content() { }
  //! returns true if the cell do contain any content
  bool isEmpty() const
  {
    return m_content.empty() && !hasBorders();
  }

  //! the cell content
  MWAWCellContent m_content;
};

/** a field of a GreatWksDBParser */
struct Field {
  //! the file type
  enum Type { F_Unknown, F_Text, F_Number, F_Date, F_Time, F_Memo, F_Picture, F_Formula, F_Summary };
  //! constructor
  Field() : m_type(F_Unknown), m_id(-1), m_name(""), m_format(), m_linkZone(0), m_recordBlock(),
    m_formula(), m_summaryType(0), m_summaryField(0), m_isSequence(false), m_firstNumber(1), m_incrementNumber(1), m_extra("")
  {
  }
  //! returns the default content type which corresponds to a field
  MWAWCellContent::Type getContentType() const
  {
    if (m_type==F_Number)
      return m_isSequence ? MWAWCellContent::C_FORMULA : MWAWCellContent::C_NUMBER;
    if (m_type==F_Time || m_type==F_Date) return MWAWCellContent::C_NUMBER;
    if (m_type==F_Formula || m_type==F_Summary) return MWAWCellContent::C_FORMULA;
    if (m_type==F_Text || m_type==F_Memo) return MWAWCellContent::C_TEXT;
    return MWAWCellContent::C_NONE;
  }
  //! update the cell to correspond to the final data
  bool updateCell(int row, int numCol, Cell &cell) const;
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Field const &field);
  //! the field type
  Type m_type;
  //! the field id
  int m_id;
  //! the field name
  std::string m_name;
  //! the field format
  MWAWCell::Format m_format;
  //! the file position which stores the position link to record zone
  long m_linkZone;
  //! the block file position which stores the position of the field's record
  BlockHeader m_recordBlock;

  // formula

  //! the formula
  std::vector<MWAWCellContent::FormulaInstruction> m_formula;

  //! the summary type: 1:average, 2:count, 3:total, 4:minimum, 5:maximum
  int m_summaryType;
  //! the summary field
  int m_summaryField;

  // sequence (or unique val)

  //! true if the number is a sequence
  bool m_isSequence;
  //! the first number (in case of progression sequence)
  int m_firstNumber;
  //! the increment number (in case of progression sequence)
  int m_incrementNumber;
  //! extra data
  std::string m_extra;
};

bool Field::updateCell(int row, int numCol, Cell &cell) const
{
  std::vector<MWAWCellContent::FormulaInstruction> &formula=cell.m_content.m_formula;
  if (m_type==F_Formula) {
    if (m_formula.size()==0)
      return false;
    cell.m_content.m_contentType=MWAWCellContent::C_FORMULA;
    formula=m_formula;
  }
  else if (m_isSequence && !cell.m_content.isValueSet()) {
    cell.m_content.m_contentType=MWAWCellContent::C_NUMBER;
    cell.m_content.setValue(double(m_firstNumber+row*m_incrementNumber));
  }
  else if (m_summaryType>0 && m_summaryType<6) {
    cell.m_content.m_contentType=MWAWCellContent::C_FORMULA;
    MWAWCellContent::FormulaInstruction instr;
    instr.m_type=MWAWCellContent::FormulaInstruction::F_Function;
    char const *(wh[])= {"Average", "Count", "Sum", "Min", "Max"};
    instr.m_content=wh[m_summaryType-1];
    formula.push_back(instr);
    instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
    instr.m_content="(";
    formula.push_back(instr);
    instr.m_type=MWAWCellContent::FormulaInstruction::F_CellList;
    instr.m_position[0][0]=0;
    instr.m_position[1][0]=numCol-1;
    instr.m_position[0][1]=instr.m_position[1][1]=m_summaryField;
    instr.m_positionRelative[0]=instr.m_positionRelative[0]=Vec2b(false,false);
    instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
    instr.m_content=")";
    formula.push_back(instr);
    return true;
  }
  // change the reference date from 1/1/1904 to 1/1/1900
  if (m_type==F_Date && cell.m_content.isValueSet())
    cell.m_content.setValue(cell.m_content.m_value+1462.);
  // and try to update the 1D formula in 2D
  for (size_t i=0; i<formula.size(); ++i) {
    MWAWCellContent::FormulaInstruction &instr=formula[i];
    if (instr.m_type==MWAWCellContent::FormulaInstruction::F_Cell)
      instr.m_position[0][1]=row;
    else if (instr.m_type==MWAWCellContent::FormulaInstruction::F_CellList)
      instr.m_position[0][1]=instr.m_position[1][1]=row;
  }
  return true;
}

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

/** the database of a GreatWksDBParser */
class Database
{
public:
  //! constructor
  Database() : m_numRecords(0), m_rowList(), m_fieldList(), m_widthDefault(75), m_widthCols(), m_heightDefault(13), m_heightRows(),
    m_rowCellsMap(), m_name("Sheet0")
  {
  }
  //! add a cell data in one given position
  bool addCell(Vec2i const &pos, Cell const &cell)
  {
    if (pos[0]<0 || pos[0]>=int(m_fieldList.size()) || pos[1]<0) {
      MWAW_DEBUG_MSG(("GreatWksDBParserInternal::Database::addCell: the cell position seems bad\n"));
      return false;
    }
    if (m_rowCellsMap.find(pos[1])==m_rowCellsMap.end())
      m_rowCellsMap[pos[1]]=std::vector<Cell>();
    std::vector<Cell> &cells=m_rowCellsMap.find(pos[1])->second;
    if (pos[0]>=(int) cells.size())
      cells.resize(m_fieldList.size());
    cells[size_t(pos[0])]=cell;
    return true;
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
    size_t numCols=m_fieldList.size();
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
  //! the list of rows data
  std::vector<MWAWEntry> m_rowList;
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
  /** the map row -> list of cells */
  std::map<int, std::vector<Cell> > m_rowCellsMap;
  /** the database name */
  std::string m_name;
protected:
};

////////////////////////////////////////
//! Internal: the state of a GreatWksDBParser
struct State {
  //! constructor
  State() : m_database(), m_idZonesMap(), m_blocks(), m_actPage(0), m_numPages(0),
    m_headerBlockHeader(), m_footerBlockHeader(), m_headerPrint(false), m_footerPrint(false), m_headerHeight(0), m_footerHeight(0)
  {
  }
  /** the database */
  Database m_database;
  /** a map id to zone used to stored the small zones */
  std::map<int, MWAWEntry> m_idZonesMap;
  /** the list of big blocks */
  std::vector<BlockHeader> m_blocks;
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
        ok = readFreeList(*block);
        break;
      case 1: // a list of record<->id
        ok = readRowLinks(*block);
        break;
      case 2: // a list of 0<->id
        ok = readBlockHeader2(*block);
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
    case 2:
      ok=readFormLinks(it->second);
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
    // zone4: never find with data, dSz=112, seems one time in v2
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
    if (it!=m_state->m_idZonesMap.end()) {
      if (!readFieldAuxis(it->second))
        readSmallZone(it->second);
    }
  }
  GreatWksDBParserInternal::Database &database=m_state->m_database;
  /* First read the fields' records then the rows' records.  As the
     rows' records contain more information than the fields' records,
     there will replace the field data if we success to read them
     (and if not, we will keep the fields' records).
   */
  for (size_t i=0; i < database.m_fieldList.size(); ++i) {
    if (!database.m_fieldList[i].m_linkZone) continue;
    readFieldLinks(database.m_fieldList[i]);
  }
  for (size_t i=0; i < database.m_rowList.size(); ++i)
    readRowRecords(database.m_rowList[i]);
  for (size_t i=0; i < database.m_fieldList.size(); ++i) {
    if (database.m_fieldList[i].m_recordBlock.isEmpty()) continue;
    readFieldRecords(database.m_fieldList[i]);
  }
  if (database.m_rowCellsMap.size())
    return true;
  // let check if we can reconstruct something
  for (size_t i=0; i < database.m_fieldList.size(); ++i) {
    if (database.m_fieldList[i].m_recordBlock.isEmpty()) continue;
    if (database.m_fieldList[i].m_isSequence) return true;
    if (database.m_fieldList[i].m_type==GreatWksDBParserInternal::Field::F_Summary) return true;
  }
  MWAW_DEBUG_MSG(("GreatWksDBParser::readDatabase: can not find any cellule\n"));
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
    static char const *(wh[])= {"Free", "RecLink", "Block2"};
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
    else if (val) f << "h" << i << "=" << val << ",";
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
    // checkme: does the zone type can be deduced from i?
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

bool GreatWksDBParser::readFreeList(GreatWksDBParserInternal::Block &block)
{
  MWAWInputStreamPtr input = getInput();
  GreatWksDBParserInternal::BlockHeader const &header=block.m_header;
  libmwaw::DebugStream f;
  for (size_t z=0; z<block.getNumZones(); ++z) {
    GreatWksDBParserInternal::Block::Zone const &zone=block.getZone(z);
    long pos=zone.m_ptr;
    if (!pos || !input->checkPosition(pos+zone.m_N*8)) {
      MWAW_DEBUG_MSG(("GreatWksDBParser::readFreeList: the zone seems too short\n"));
      f.str("");
      f << "Entries(" << header.m_name << ")[" << header << "]:###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      continue;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << "Entries(" << header.m_name << "):zones=[";
    for (int i=0; i<zone.m_N; ++i) {
      /* a ptr followed by 400000000|size or the oposite
         normally the content of a free block seems to be 0x8000, dataSz
         excepted on time where it is 8, dataSz
       */
      long values[2];
      for (int j=0; j<2; ++j) values[j]=(long) input->readULong(4);
      if ((values[0]&0xFF000000)==0x40000000) {
        long tmp=values[0];
        values[0]=values[1];
        values[1]=tmp;
      }
      if ((values[0]&0xFF000000)==0 && (values[1]&0xFF000000)==0x40000000 &&
          input->checkPosition(values[0]+(values[1]&0xFFFFFF))) {
        ascii().addPos(values[0]);
        ascii().addNote("Free");
        ascii().addPos(values[0]+(values[1]&0xFFFFFF));
        ascii().addNote("_");
        f << std::hex << values[0] << ":" << (values[1]&0xFFFFFF) << std::dec << ",";
      }
      else {
        MWAW_DEBUG_MSG(("GreatWksDBParser::readFreeList: find an odd free zone\n"));
        f << "###" << std::hex << values[0] << ":" << values[1] << std::dec << ",";
      }
    }
    f << "]";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    ascii().addPos(input->tell());
    ascii().addNote("_");
  }
  return true;
}

bool GreatWksDBParser::readRowLinks(GreatWksDBParserInternal::Block &block)
{
  MWAWInputStreamPtr input = getInput();
  GreatWksDBParserInternal::BlockHeader const &header=block.m_header;
  GreatWksDBParserInternal::Database &database=m_state->m_database;
  libmwaw::DebugStream f;
  for (size_t z=0; z<block.getNumZones(); ++z) {
    GreatWksDBParserInternal::Block::Zone const &zone=block.getZone(z);
    long pos=zone.m_ptr;
    if (!pos || !input->checkPosition(pos+zone.m_N*8)) {
      MWAW_DEBUG_MSG(("GreatWksDBParser::readRowLinks: the zone seems too short\n"));
      f.str("");
      f << "Entries(" << header.m_name << ")[" << header << "]:###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      continue;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << "Entries(" << header.m_name << "):ptrs=[";
    for (int i=0; i<zone.m_N; ++i) {
      long ptr=(long) input->readULong(4);
      int id=(int) input->readLong(4);
      if (ptr) {
        MWAWEntry entry;
        entry.setBegin(ptr);
        entry.setId(id);
        database.m_rowList.push_back(entry);
      }
      else {
        MWAW_DEBUG_MSG(("GreatWksDBParser::readRowLinks: find an empty record zone\n"));
        f << "###";
      }
      f << std::hex << ptr << std::dec << ":" << id << ",";
    }
    f << "]";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    ascii().addPos(input->tell());
    ascii().addNote("_");
  }
  return true;
}

bool GreatWksDBParser::readBlockHeader2(GreatWksDBParserInternal::Block &block)
{
  MWAWInputStreamPtr input = getInput();
  GreatWksDBParserInternal::BlockHeader const &header=block.m_header;
  libmwaw::DebugStream f;
  for (size_t z=0; z<block.getNumZones(); ++z) {
    GreatWksDBParserInternal::Block::Zone const &zone=block.getZone(z);
    long pos=zone.m_ptr;
    if (!pos || !input->checkPosition(pos+zone.m_N*8)) {
      MWAW_DEBUG_MSG(("GreatWksDBParser::readBlockHeader2: the zone seems too short\n"));
      f.str("");
      f << "Entries(" << header.m_name << ")[" << header << "]:###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      continue;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << "Entries(" << header.m_name << "):unkn=[";
    for (int i=0; i<zone.m_N; ++i) {
      f << input->readULong(4) << ":"; // always 0?
      f << input->readLong(4) << ","; // ids
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
      "Zone0A", "Field", "FrmLink", "Zone3A", "Zone4A", "ListFrmula", "ListSummary", "Zone7A",
      "Zone8A", "Form", "FldLink", "Zone11A", "Zone12A", "FldAuxi", "Zone14A"
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
// read the row zone
////////////////////////////////////////////////////////////
bool GreatWksDBParser::readRowRecords(MWAWEntry const &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (!pos || !input->checkPosition(pos+6)) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readRowRecords: can not find a row position\n"));
    if (pos) {
      ascii().addPos(pos);
      ascii().addNote("Entries(RowRec):###");
    }
    return false;
  }

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(RowRec):";
  // probably type=0 followed by ?
  int val=(int) input->readULong(2);
  if (val!=0xFF) f << "f0=" << std::hex << val << std::dec << ",";
  long dSz=(long) input->readULong(4);
  long endPos=pos+6+dSz;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readRowRecords: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  ascii().addPos(endPos);
  ascii().addNote("_");

  GreatWksDBParserInternal::Database &database=m_state->m_database;
  for (size_t fl=0; fl<database.m_fieldList.size(); ++fl) {
    pos=input->tell();
    f.str("");
    f << "RowRec-" << fl << ":";
    if (pos >= endPos) {
      MWAW_DEBUG_MSG(("GreatWksDBParser::readRowRecords: actual pos seems bad\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return true;
    }
    GreatWksDBParserInternal::Field const &field=database.m_fieldList[fl];
    bool ok=true;

    GreatWksDBParserInternal::Cell cell;
    cell.setFormat(field.m_format);
    switch (field.m_type) {
    case GreatWksDBParserInternal::Field::F_Text: {
      int sSz=(int) input->readULong(1);
      if (pos+1+sSz>endPos) {
        MWAW_DEBUG_MSG(("GreatWksDBParser::readRowRecords: the string length seems bad\n"));
        f << "###";
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
      }
      cell.m_content.m_contentType=MWAWCellContent::C_TEXT;
      cell.m_content.m_textEntry.setBegin(input->tell());
      cell.m_content.m_textEntry.setLength(sSz);
      database.addCell(Vec2i(field.m_id, entry.id()), cell);

      std::string text("");
      for (int i=0; i<sSz; ++i) text+=(char) input->readULong(1);
      f << text << ",";
      // realign to a multiple of 2
      if ((sSz%2)==0) input->seek(1,librevenge::RVNG_SEEK_CUR);
      break;
    }
    case GreatWksDBParserInternal::Field::F_Summary: {
      if (pos+10>endPos) {
        MWAW_DEBUG_MSG(("GreatWksDBParser::readRowRecords: can not read a summary zone\n"));
        f << "###";
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        return false;
      }
      // formula will be will later
      cell.m_content.m_contentType=MWAWCellContent::C_FORMULA;
      database.addCell(Vec2i(field.m_id, entry.id()), cell);
      f << "summary,";
      input->seek(pos+10, librevenge::RVNG_SEEK_SET);
      break;
    }
    case GreatWksDBParserInternal::Field::F_Number:
    case GreatWksDBParserInternal::Field::F_Date:
    case GreatWksDBParserInternal::Field::F_Time: {
      if (pos+10>endPos) {
        MWAW_DEBUG_MSG(("GreatWksDBParser::readRowRecords: can not read a number zone\n"));
        f << "###";
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        return false;
      }
      double value;
      bool isNan;
      if (!input->readDouble10(value, isNan)) {
        static bool first=true;
        if (first) {
          // can be normal, ie. 7fff403200000000000000 means no value which generates an error
          MWAW_DEBUG_MSG(("GreatWksDBParser::readRowRecords: can not read a number, maybe a undef number\n"));
          first=false;
        }
        f << "#";
      }
      else {
        cell.m_content.m_contentType=MWAWCellContent::C_NUMBER;
        cell.m_content.setValue(value);
        database.addCell(Vec2i(field.m_id, entry.id()), cell);
        f << value;
      }
      input->seek(pos+10, librevenge::RVNG_SEEK_SET);
      break;
    }
    case GreatWksDBParserInternal::Field::F_Picture: {
      long pSz=(int) input->readULong(4);
      if (pos+4+pSz>endPos) {
        MWAW_DEBUG_MSG(("GreatWksDBParser::readRowRecords: the picture length seems bad\n"));
        f << "###";
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
      }
      if (pSz) {
#ifdef DEBUG_WITH_FILES
        ascii().skipZone(pos+4,pos+4+pSz-1);
        librevenge::RVNGBinaryData file;
        input->seek(pos+4,librevenge::RVNG_SEEK_SET);
        input->readDataBlock(pSz, file);

        static int volatile pictName = 0;
        libmwaw::DebugStream f2;
        f2 << "DATA-" << ++pictName << ".pct";
        libmwaw::Debug::dumpFile(file, f2.str().c_str());
#endif
      }
      input->seek(pos+4+pSz, librevenge::RVNG_SEEK_SET);
      break;
    }
    case GreatWksDBParserInternal::Field::F_Memo: {
      long mSz=(int) input->readULong(4);
      if (pos+4+mSz>endPos) {
        MWAW_DEBUG_MSG(("GreatWksDBParser::readRowRecords: the memo length seems bad\n"));
        f << "###";
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
      }
      cell.m_content.m_contentType=MWAWCellContent::C_TEXT;
      cell.m_content.m_textEntry.setBegin(input->tell());
      cell.m_content.m_textEntry.setLength(mSz);
      database.addCell(Vec2i(field.m_id, entry.id()), cell);
      // probably a simple textbox ( see sendSimpleTextbox )
      if (mSz) f << "memo,";
      input->seek(pos+4+mSz, librevenge::RVNG_SEEK_SET);
      break;
    }
    case GreatWksDBParserInternal::Field::F_Formula: {
      std::string extra("");
      ok=readFormulaResult(endPos, cell, extra);
      if (ok)
        database.addCell(Vec2i(field.m_id, entry.id()), cell);
      f << extra;
      break;
    }
    case GreatWksDBParserInternal::Field::F_Unknown:
    default:
      ok=false;
      break;
    }
    if (!ok)
      break;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  pos=input->tell();
  if (pos+2==endPos) {
    // seems ok to find 0 here
    val=(int) input->readLong(2);
    if (val) {
      f.str("");
      f << "RowRec[end]:" << val << ",";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
    }
    else {
      ascii().addPos(pos);
      ascii().addNote("_");
    }
  }
  else if (pos!=endPos) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readRowRecords: find some extra data\n"));
    ascii().addPos(pos);
    ascii().addNote("RowRec[end]:###");
  }

  return true;
}

bool GreatWksDBParser::readFormula(long endPos, std::vector<MWAWCellContent::FormulaInstruction> &formula)
{
  formula.resize(0);
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  long dSz=(long) input->readULong(2);
  long endHeader=pos+2+dSz;
  if (dSz<2 || endHeader > endPos) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readFormula: can not read a formula\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(Formula):";
  std::string error("");
  if (!m_document->readFormula(Vec2i(0,0),endHeader,formula, error))
    f << "###";
  f << "[";
  for (size_t l=0; l < formula.size(); ++l)
    f << formula[l];
  f << "]" << error << ",";

  input->seek(endHeader, librevenge::RVNG_SEEK_SET);
  int N=(int) input->readULong(2);
  if (endHeader+2*(N+1)>endPos) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readFormula: can not read a formula field(II)\n"));
    return false;
  }
  int val=(int) input->readLong(2); // Here I find always 2, the field size ?
  if (val!=2) f << "g0=" << val << ",";
  std::vector<int> varList;
  if (N) {
    f << "list[varId]=[";
    for (int i=0; i<N; ++i) {
      val=(int) input->readLong(2);
      varList.push_back(val);
      f << val << ",";
    }
    f << "],";
  }
  // time to update the formula
  int id=0;
  for (size_t l=0; l < formula.size(); ++l) {
    MWAWCellContent::FormulaInstruction &instr=formula[l];
    if (instr.m_type!=MWAWCellContent::FormulaInstruction::F_Cell)
      continue;
    if (id>=(int) varList.size() || varList[size_t(id)]<=0) {
      MWAW_DEBUG_MSG(("GreatWksDBParser::readFormula: can not associated a cell with field\n"));
      f << "###";
      formula.resize(0);
      break;
    }
    instr.m_position[0][0]=varList[size_t(id++)]-1;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

/* read the result of a formula.

   note: we fill the cell content, which can overidden later if we find a formula to associate with the cell */
bool GreatWksDBParser::readFormulaResult(long endPos, GreatWksDBParserInternal::Cell &cell, std::string &extra)
{
  libmwaw::DebugStream f;
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (pos+2>endPos) return false;
  int type=(int) input->readLong(2);
  switch (type) {
  case 0: // average
  case 1: // summary
  case 5: // number
  case 9: // date
  case 0xa: { // time
    if (pos+12>endPos) return false;
    double value;
    bool isNan;
    if (!input->readDouble10(value, isNan))
      f << "#double,";
    else {
      cell.m_content.m_contentType=MWAWCellContent::C_NUMBER;
      cell.m_content.setValue(value);
      f << value << ",";
    }
    input->seek(pos+12, librevenge::RVNG_SEEK_SET);
    break;
  }
  case 7: {
    // checkme
    int sSz=(int) input->readULong(1);
    if (pos+3+sSz>endPos) {
      MWAW_DEBUG_MSG(("GreatWksSSParser::readFormulaResult: can not read a string value\n"));
      return false;
    }
    cell.m_content.m_contentType=MWAWCellContent::C_TEXT;
    cell.m_content.m_textEntry.setBegin(input->tell());
    cell.m_content.m_textEntry.setLength(sSz);
    // also modify the cell format in text
    MWAWCell::Format format=cell.getFormat();
    format.m_format=MWAWCell::F_TEXT;
    cell.setFormat(format);
    std::string text("");
    for (int i=0; i<sSz; ++i)
      text += (char) input->readULong(1);
    f << "\"" << text << "\",";
    if ((sSz%2)==0)
      input->seek(1, librevenge::RVNG_SEEK_CUR);
    break;
  }
  case 8: { // bool or long
    if (pos+4>endPos) return false;
    int val=(int) input->readLong(2);
    cell.m_content.m_contentType=MWAWCellContent::C_NUMBER;
    cell.m_content.setValue((double) val);
    f << input->readLong(2) << ",";
    break;
  }
  case 0xf: // nan
    if (pos+6>endPos) return false;
    cell.m_content.m_contentType=MWAWCellContent::C_NUMBER;
    cell.m_content.setValue(std::numeric_limits<double>::quiet_NaN());
    f << "nan" << input->readLong(4) << ",";
    break;
  default:
    MWAW_DEBUG_MSG(("GreatWksDBParser::readFormulaResult: find unknown type %d\n", type));
    f << "#type=" << type << ",";
    extra=f.str();
    return false;
  }
  extra=f.str();
  return true;
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
    if (readField(field))
      f << field;
    else {
      field = GreatWksDBParserInternal::Field();
      MWAW_DEBUG_MSG(("GreatWksDBParser::readFields: can not read a field\n"));
      f << "###";
    }
    m_state->m_database.m_fieldList.push_back(field);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+dSz, librevenge::RVNG_SEEK_SET);
  }
  for (size_t fl=0; fl<m_state->m_database.m_fieldList.size(); ++fl) {
    GreatWksDBParserInternal::Field &field=m_state->m_database.m_fieldList[fl];
    pos=input->tell();
    if (field.m_type==GreatWksDBParserInternal::Field::F_Summary) {
      if (pos+18>entry.end()) {
        MWAW_DEBUG_MSG(("GreatWksDBParser::readFields: can not read a summary field\n"));
        break;
      }
      f.str("");
      f << "Field[summary]:";
      int val=(int) input->readULong(2); // 10a or 180
      if (val!=0x10a) f << "fl=" << std::hex << val << std::dec << ",";
      field.m_summaryType=(int) input->readULong(2);
      if (field.m_summaryType > 0 && field.m_summaryType < 6) {
        static char const *(wh[]) = { "", "average", "count", "total", "minimum", "maximum" };
        f << wh[field.m_summaryType] << ",";
      }
      else
        f << "#type=" << field.m_summaryType << ",";
      field.m_summaryField=(int)input->readLong(2)-1;
      f << "field=" << field.m_summaryField << ",";
      val=(int) input->readULong(2); // 1e0 or 14
      if (val!=0x1e0) f << "f0=" << std::hex << val << std::dec << ",";
      for (int i=0; i<5; ++i) { // always 0
        val=(int) input->readULong(2);
        if (val)  f << "f" << i+1 << "=" << val << ",";
      }

      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      input->seek(pos+18, librevenge::RVNG_SEEK_SET);
      continue;
    }
    if (field.m_type!=GreatWksDBParserInternal::Field::F_Formula)
      continue;
    if (!readFormula(entry.end(), field.m_formula)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
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
  MWAWCell::Format &format = field.m_format;
  switch (type) {
  case 5:
    field.m_type=GreatWksDBParserInternal::Field::F_Number;
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_GENERIC;
    break;
  case 7:
    field.m_type=GreatWksDBParserInternal::Field::F_Text;
    format.m_format=MWAWCell::F_TEXT;
    break;
  case 9:
    field.m_type=GreatWksDBParserInternal::Field::F_Date;
    format.m_format=MWAWCell::F_DATE;
    break;
  case 0xa:
    field.m_type=GreatWksDBParserInternal::Field::F_Time;
    format.m_format=MWAWCell::F_TIME;
    break;
  case 0xc:
    field.m_type=GreatWksDBParserInternal::Field::F_Memo;
    format.m_format=MWAWCell::F_TEXT;
    break;
  case 0xd:
    field.m_type=GreatWksDBParserInternal::Field::F_Picture;
    break;
  case 0xFF:
    field.m_type=GreatWksDBParserInternal::Field::F_Formula;
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_GENERIC;
    break;
  case 0xFE:
    field.m_type=GreatWksDBParserInternal::Field::F_Summary;
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_GENERIC;
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
  field.m_id=(int) input->readLong(2)-1;
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

bool GreatWksDBParser::readFieldAuxis(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<10) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldAuxis: the entry length seems bad\n"));
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
  if (vers==1 || (vers==2 && dSz<0x46) || entry.length()<10+N*dSz) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldAuxis: the number of data seems bad\n"));
    f << "###";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  GreatWksDBParserInternal::Database &database=m_state->m_database;
  if (N>(int) database.m_fieldList.size()) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldAuxis: the field list seems too short\n"));
    database.m_fieldList.resize(size_t(N));
  }
  for (int i=0; i<N; ++i) {
    long pos=input->tell();
    GreatWksDBParserInternal::Field &field=database.m_fieldList[size_t(i)];
    f.str("");
    f << "FldAuxi-" << i << ":";
    int val=(int) input->readLong(2);
    if (val) {
      // checkme: find val=10 for a sequence with increment=10, so...
      f << "type[increment]=" << val << ",";
      field.m_isSequence=true;
    }
    val=(int) input->readLong(2);
    if (val==1) {
      f << "isUnique[number],";
      // let use a sequence
      field.m_isSequence=true;
    }
    else if (val)
      f << "isUnique[number]=" << val << ",";
    for (int j=0; j<2; ++j) { // always 0?
      val=(int) input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    field.m_firstNumber=(int) input->readLong(2);
    if (field.m_firstNumber!=1)
      f << "first[sequence]=" << field.m_firstNumber << ",";
    val=(int) input->readLong(2); // always 0
    if (val) f << "f2=" << val << ",";
    field.m_incrementNumber=(int) input->readLong(2);
    if (field.m_incrementNumber!=1)
      f << "increment[sequence]=" << field.m_incrementNumber << ",";
    for (int j=0; j < 28; ++j) { // always 0
      val=(int) input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    input->seek(pos+dSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}
////////////////////////////////////////////////////////////
// read a list of records corresponding to a field
////////////////////////////////////////////////////////////
bool GreatWksDBParser::readFieldRecords(GreatWksDBParserInternal::Field &field)
{
  if (field.m_recordBlock.isEmpty()) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldRecords: can not read a record zone\n"));
    return false;
  }
  shared_ptr<GreatWksDBParserInternal::Block> block=createBlock(field.m_recordBlock);
  if (!block) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldRecords: can not find the records input\n"));
    return false;
  }
  GreatWksDBParserInternal::Database &database=m_state->m_database;
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  for (size_t z=0; z<block->getNumZones(); ++z) {
    GreatWksDBParserInternal::Block::Zone const &zone=block->getZone(z);
    long pos=zone.m_ptr;
    f.str("");
    f << "Entries(FldRec)[" << field.m_id << "]:type=" << int(field.m_type) << ",";
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    int const N=zone.m_N;
    int const dSz=(int) zone.m_dataSize;
    if (field.m_type==GreatWksDBParserInternal::Field::F_Text) {
      if (!input->checkPosition(pos+dSz+N*5)) {
        MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldRecords: can not read a text zone\n"));
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
      // now read a list of small int: the row number
      for (int i=0; i<N; ++i)
        rows.push_back((int) input->readULong(4));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      int actPos=0;
      for (size_t i=0; i<(size_t)N; ++i) {
        int nextPos= i+1==size_t(N) ? dSz : positions[i+1];
        if (actPos!=positions[i] || nextPos>dSz) {
          MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldRecords: can not read a text zone, pb with a position\n"));
          f << "###";
          break;
        }
        GreatWksDBParserInternal::Cell cell;
        cell.setFormat(field.m_format);
        cell.m_content.m_contentType=MWAWCellContent::C_TEXT;
        cell.m_content.m_textEntry.setBegin(input->tell());
        std::string text("");
        bool findEmptyChar=false;
        while (actPos<nextPos) {
          char c=(char) input->readULong(1);
          // c==0 can indicate an empty field
          if (c) text += c;
          else findEmptyChar=true;
          ++actPos;
        }
        cell.m_content.m_textEntry.setEnd(input->tell()-(findEmptyChar ? 1 : 0));
        database.addCell(Vec2i(field.m_id, rows[i]), cell);
        f << "\"" << text << "\":" << rows[i] << ",";
      }
      input->seek(pos+12+dSz+5*N, librevenge::RVNG_SEEK_SET);
    }
    else if (field.m_type==GreatWksDBParserInternal::Field::F_Number ||
             field.m_type==GreatWksDBParserInternal::Field::F_Date ||
             field.m_type==GreatWksDBParserInternal::Field::F_Time) {
      if (!input->checkPosition(pos+14*N)) {
        MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldRecords: can not read a number zone\n"));
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
            MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldRecords: can not read a number, maybe a undef number\n"));
            first=false;
          }
          f << "#";
          input->seek(fPos+14, librevenge::RVNG_SEEK_SET);
        }
        else {
          GreatWksDBParserInternal::Cell cell;
          cell.setFormat(field.m_format);
          cell.m_content.m_contentType=MWAWCellContent::C_NUMBER;
          cell.m_content.setValue(value);
          database.addCell(Vec2i(field.m_id, row), cell);
          f << value;
        }
        f << ":" << row << ",";
      }
    }
    else {
      MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldRecords: does not know how to read list for type=%d\n", int(field.m_type)));
      f << "###";
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    ascii().addPos(input->tell());
    ascii().addNote("_");
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the links between fields and record zone
////////////////////////////////////////////////////////////
bool GreatWksDBParser::readFieldLinks(GreatWksDBParserInternal::Field &field)
{
  MWAWInputStreamPtr input = getInput();
  if (field.m_linkZone<=0 || !input->checkPosition(field.m_linkZone+32)) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldLinks: can not read a link between field to records\n"));
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
    MWAW_DEBUG_MSG(("GreatWksDBParser::readFieldLinks: the zone position seems bad\n"));
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
// read a list of form
////////////////////////////////////////////////////////////
bool GreatWksDBParser::readFormLinks(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<10) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readFormLinks: the entry length seems bad\n"));
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
  if (entry.length()!=10+N*dSz || dSz<4) {
    MWAW_DEBUG_MSG(("GreatWksDBParser::readFormLinks: the number of data seems bad\n"));
    f << "###";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  std::vector<MWAWEntry> listZones;
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
      if (checkSmallZone(zone))
        listZones.push_back(zone);
      else
        f << "###";
      f << "ptr" << i << "=" << std::hex << ptr << std::dec << ",";
    }
    input->seek(pos+dSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  for (size_t z=0; z<listZones.size(); ++z)
    readForm(listZones[z]);
  return true;
}

////////////////////////////////////////////////////////////
// read zone 2 link zone
////////////////////////////////////////////////////////////
bool GreatWksDBParser::readForm(MWAWEntry const &entry)
{
  int const vers=version();
  /* not really sure what may be the header size, ie.
     in v1: 264 seems ok,
     in v2: 276 seems ok,
     but some v1 file converted in v2 seems to keep intact the v1 structure :-~

     This must be fixed if we want to export the form...
  */
  int const headerSize=vers==1 ? 264 : 276;
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  libmwaw::DebugStream f;
  f << "Entries(" << entry.name() << "):";
  ascii().addPos(entry.end());
  ascii().addNote("_");
  if (entry.id()!=9 || entry.length() < 6+headerSize) {
    f << "###";
    MWAW_DEBUG_MSG(("GreatWksDBParser::readForm: the entry seems bad\n"));
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  input->seek(pos+6, librevenge::RVNG_SEEK_SET);
  // a big number 5bafac : an id?
  f << "f0=" << std::hex << input->readULong(4) << std::dec << ",";
  // small number between 1 and 4
  f << "id=" << input->readLong(2) << ",";
  int sSz=(int) input->readULong(1);
  if (sSz>32) {
    f << "###";
    MWAW_DEBUG_MSG(("GreatWksDBParser::readForm: the string size seems bad\n"));
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  std::string text("");
  for (int i=0; i<sSz; ++i) text+=(char) input->readULong(1);
  f << text << ",";
  input->seek(pos+44, librevenge::RVNG_SEEK_SET);
  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(pos+headerSize, librevenge::RVNG_SEEK_SET);
  pos=input->tell();
  if (!m_document->getGraphParser()->readPageFrames())
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  pos=input->tell();
  if (pos!=entry.end()) {
    ascii().addPos(pos);
    ascii().addNote("Form:end");
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
  MWAWInputStreamPtr input=getInput();
  GreatWksDBParserInternal::Database const &database=m_state->m_database;
  std::vector<GreatWksDBParserInternal::Field> const &fields = database.m_fieldList;
  size_t numFields=fields.size();
  // fixme: use first layout colWidth here
  listener->openSheet(std::vector<float>(numFields,76), librevenge::RVNG_POINT, "Sheet0");
  int r=0;
  std::map<int, std::vector<GreatWksDBParserInternal::Cell> >::const_iterator rIt;
  for (rIt=database.m_rowCellsMap.begin(); rIt != database.m_rowCellsMap.end(); ++rIt, ++r) {
    std::vector<GreatWksDBParserInternal::Cell> const &row=rIt->second;
    listener->openSheetRow(12, librevenge::RVNG_POINT);
    for (size_t c=0; c<row.size(); ++c) {
      if (c>=numFields) break;
      GreatWksDBParserInternal::Field const &field=fields[c];
      GreatWksDBParserInternal::Cell cell;
      field.updateCell(int(r), int(numFields), cell);
      if (cell.isEmpty()) continue;

      MWAWCellContent const &content=cell.m_content;
      listener->openSheetCell(cell, content);
      if (content.m_contentType==MWAWCellContent::C_TEXT && content.m_textEntry.valid()) {
        input->seek(content.m_textEntry.begin(), librevenge::RVNG_SEEK_SET);
        while (!input->isEnd() && input->tell()<content.m_textEntry.end()) {
          unsigned char ch=(unsigned char) input->readULong(1);
          if (ch==0xd)
            listener->insertEOL();
          else if (ch<30) {
            MWAW_DEBUG_MSG(("GreatWksDBParser::sendDatabase: find some odd character\n"));
            break;
          }
          else
            listener->insertCharacter(ch);
        }
      }
      listener->closeSheetCell();
    }
    listener->closeSheetRow();
  }
  listener->closeSheet();
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool GreatWksDBParser::checkHeader(MWAWHeader *header, bool strict)
{
  MWAWInputStreamPtr input = getInput();
  *m_state = GreatWksDBParserInternal::State();
  if (!m_document->checkHeader(header, strict) || !input) return false;
  if (getParserState()->m_kind!=MWAWDocument::MWAW_K_DATABASE)
    return false;
  if (!strict) return true;

  // let check that the 3 header are defined
  input->seek(16, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<3; ++i) {
    GreatWksDBParserInternal::BlockHeader block;
    if (!readBlockHeader(block) || block.m_ptr[0]==0 || (block.m_ptr[0]&0x0FF)) return false;
    input->seek(8, librevenge::RVNG_SEEK_CUR);
  }
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
