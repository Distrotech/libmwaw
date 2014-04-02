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
#include <sstream>

#include <librevenge/librevenge.h>


#include "MWAWCell.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWOLEParser.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSpreadsheetListener.hxx"
#include "MWAWSubDocument.hxx"

#include "MsWksGraph.hxx"
#include "MsWks3Text.hxx"
#include "MsWksDocument.hxx"

#include "MsWksDBParser.hxx"

/** Internal: the structures of a MsWksDBParser */
namespace MsWksDBParserInternal
{
////////////////////////////////////////////////////////////
// the data ( and the list view )
////////////////////////////////////////////////////////////

/** a class to store the serial data which code a auto increment
    column with potential prefix and suffix in v4. */
class SerialFormula
{
public:
  //! the constructor
  SerialFormula() : m_increment(0), m_nextValue(0), m_prefix(""), m_suffix("") { }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, SerialFormula const &form);

  //! update a content (knowing the value)
  void updateContent(long value, MWAWCellContent &content);

  //! the increment
  long m_increment;
  //! the first value
  long m_nextValue;

  //! the prefix
  std::string m_prefix;

  //! the suffix
  std::string m_suffix;
};

void SerialFormula::updateContent(long value, MWAWCellContent &content)
{
  std::vector<MWAWCellContent::FormulaInstruction> &formula=content.m_formula;
  formula.resize(0);
  content.m_contentType=MWAWCellContent::C_FORMULA;
  MWAWCellContent::FormulaInstruction instr;
  if (m_prefix.empty() && m_suffix.empty()) {
    instr.m_type=MWAWCellContent::FormulaInstruction::F_Long;
    instr.m_longValue=value;
    formula.push_back(instr);
    return;
  }
  instr.m_type=MWAWCellContent::FormulaInstruction::F_Function;
  instr.m_content="Concatenate";
  formula.push_back(instr);
  instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
  instr.m_content="(";
  formula.push_back(instr);
  if (!m_prefix.empty()) {
    instr.m_type=MWAWCellContent::FormulaInstruction::F_Text;
    instr.m_content=m_prefix;
    formula.push_back(instr);
    instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
    instr.m_content=";";
    formula.push_back(instr);
  }
  instr.m_type=MWAWCellContent::FormulaInstruction::F_Long;
  instr.m_longValue=value;
  formula.push_back(instr);
  if (!m_suffix.empty()) {
    instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
    instr.m_content=";";
    formula.push_back(instr);
    instr.m_type=MWAWCellContent::FormulaInstruction::F_Text;
    instr.m_content=m_suffix;
    formula.push_back(instr);
  }
  instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
  instr.m_content=")";
  formula.push_back(instr);
}

//! operator<<
std::ostream &operator<<(std::ostream &o, SerialFormula const &form)
{
  if (form.m_prefix.length()) o << "\"" << form.m_prefix << "\".";
  o << "(" << form.m_nextValue << "+" << form.m_increment << "*Row)";
  if (form.m_suffix.length()) o << ".\"" << form.m_suffix << "\"";
  return o;
}

/** the type of each field */
class FieldType : public MWAWCell
{
public:
  //! constructor
  FieldType() : MWAWCell(), m_content(), m_name(""), m_used(true), m_isSerial(false),
    m_serialId(0), m_serialFormula(), m_height(0), m_extra("")
  { }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, FieldType const &fType);

  //! returns true if the field has no content
  bool empty() const
  {
    return !m_isSerial && m_content.empty();
  }

  //! update a field with the record data
  void updateWithContent(Vec2i const &pos, MWAWCellContent const &content);

  /** the cell content */
  MWAWCellContent m_content;
  /** the field name */
  std::string m_name;

  /** a flag to know if the field is used or not */
  bool m_used;
  /** a flag to deferentiate a serial formula to a formula */
  bool m_isSerial;
  /** serialized field identificator */
  int m_serialId;
  /** the serial formula ( if the field is serialized ) */
  SerialFormula m_serialFormula;

  /** the height */
  int m_height;
  /** extra data */
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, FieldType const &fType)
{
  if (!fType.m_used) {
    o << "unused,";
    return o;
  }
  o << static_cast<MWAWCell const &>(fType);
  o << fType.m_content;
  if (fType.m_name.length())
    o << "name=\"" << fType.m_name << "\",";

  if (fType.m_isSerial && fType.m_serialId) o << "serialId=" << fType.m_serialId << ",";
  if (fType.m_height) o << "h=" << fType.m_height << ",";

  o << fType.m_extra;
  return o;
}

void FieldType::updateWithContent(Vec2i const &pos, MWAWCellContent const &content)
{
  setPosition(pos);
  if (m_isSerial) {
    m_serialFormula.updateContent((long)(content.m_value+0.4), m_content);
    return;
  }
  MWAWCell::Format const &format=getFormat();
  if (!content.empty()) {
    if (m_content.m_contentType==MWAWCellContent::C_FORMULA) {
      // special case, we need to conserve the formula
      if (content.m_contentType==MWAWCellContent::C_NUMBER)
        m_content.setValue(content.m_value);
      else
        m_content.m_textEntry=content.m_textEntry;
    }
    else
      m_content=content;
  }
  // change the reference date from 1/1/1904 to 1/1/1900
  if (format.m_format==MWAWCell::F_DATE && m_content.isValueSet())
    m_content.setValue(m_content.m_value+1462.);
  // and try to update the 1D formula in 2D
  std::vector<MWAWCellContent::FormulaInstruction> &formula=m_content.m_formula;
  for (size_t i=0; i<formula.size(); ++i) {
    MWAWCellContent::FormulaInstruction &instr=formula[i];
    if (instr.m_type==MWAWCellContent::FormulaInstruction::F_Cell) {
      --instr.m_position[0][0];
      instr.m_position[0][1]=pos[1];
    }
    else if (instr.m_type==MWAWCellContent::FormulaInstruction::F_CellList) {
      --instr.m_position[0][0];
      --instr.m_position[1][0];
      instr.m_position[0][1]=instr.m_position[1][1]=pos[1];
    }
  }
}

/** the database */
class DataBase
{
public:
  /** constructor */
  DataBase() : m_numFields(0), m_numRecords(0), m_listFieldTypes(), m_listRecords() {}
  //! convert the m_widthCols in a vector of of point size
  std::vector<float> convertInPoint(std::vector<int> const &list, float defSize) const
  {
    size_t numCols=m_listFieldTypes.size();
    std::vector<float> res;
    res.resize(numCols);
    for (size_t i = 0; i < numCols; i++) {
      if (i>=list.size() || list[i] < 0) res[i] = defSize;
      else res[i] = float(list[i]);
    }
    return res;
  }

  /** the number of fields */
  int m_numFields;

  /** the number of records */
  int m_numRecords;
  /** the list of field type

  \note which begins by an unused field */
  std::vector<FieldType> m_listFieldTypes;

  /** the list of record by row */
  std::vector<std::vector<MWAWCellContent> > m_listRecords;

};

////////////////////////////////////////////////////////////
// the forms
////////////////////////////////////////////////////////////
enum FormVisibility { V_VALUE, V_NAMEVALUE, V_HIDDEN, V_HEADER, V_UNDEF, V_UNKNOWN };


/**a class used to store a form */
class FormType
{
public:
  /** the constructor */
  FormType() : m_fieldId(-1), m_visible(V_UNKNOWN), m_font(), m_backColor(0), m_extra("") {}

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, FormType const &form);

  //! the associated field
  int m_fieldId;

  //! the form visibility
  FormVisibility m_visible;

  //! the font
  MWAWFont m_font;

  //! the backgroun color
  int m_backColor;

  //! two bdbox one for content and field name
  Box2f m_bdbox[2];

  //! extra data
  std::string m_extra;
};

//! operator<<
std::ostream &operator<<(std::ostream &o, FormType const &form)
{
  if (form.m_fieldId != -1) o << "Field" << form.m_fieldId << ",";
  switch (form.m_visible) {
  case V_VALUE:
    o << "value,";
    break;
  case V_NAMEVALUE:
    break;
  case V_HIDDEN:
    o << "hidden,";
    break;
  case V_HEADER:
    o << "header,";
    break;
  case V_UNDEF:
    o << "undef,";
    break;
  case V_UNKNOWN:
    o << "unknown[visible],";
    break;
  default:
    o << "###visible=" << form.m_visible << ",";
    break;
  }

  if (form.m_visible ==  V_VALUE || form.m_visible ==  V_NAMEVALUE)
    o << "bdbox(value)=" << form.m_bdbox[0] << ",";
  if (form.m_visible == V_NAMEVALUE)
    o << "bdbox(fName)=" << form.m_bdbox[1] << ",";
  o << form.m_extra;
  return o;
}
/** a class used to store a form */
class Form
{
public:
  //! the constructor
  Form() : m_name(""), m_listTypes() {}

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Form const &form);

  //! the form name
  std::string m_name;

  /** three bdbox : the last is the page bdbox*/
  Box2f m_bdbox[3];

  /** the list of fields type */
  std::vector<FormType> m_listTypes;
};

//! operator<<
std::ostream &operator<<(std::ostream &o, Form const &form)
{
  o << "name=\"" << form.m_name << "\",";
  o << "bdbox0=" << form.m_bdbox[0] << ",";
  o << "points=" << form.m_bdbox[1] << ",";
  o << "bdbox(page)=" << form.m_bdbox[2] << ",";
  return o;
}

/** a class used to store a list of forms */
class Forms
{
public:
  /** the constructor */
  Forms() : m_numForms(0), m_font(), m_backColor(0), m_listForms(), m_extra("") { }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Forms const &form);
  /** the number of forms */
  int m_numForms;

  /** the default font */
  MWAWFont m_font;

  /** the default font back color */
  int m_backColor;

  /** two bdbox */
  Box2f m_bdbox[2];

  /** the list of forms */
  std::vector<Form> m_listForms;
  /** extra data */
  std::string m_extra;
};

//! operator<<
std::ostream &operator<<(std::ostream &o, Forms const &form)
{
  if (form.m_backColor) o << "#bkCol?=" << form.m_backColor << ",";
  o << "bdbox0=" << form.m_bdbox[0] << ",";
  o << "bdbox1=" << form.m_bdbox[1] << ",";
  o << "nForms=" << form.m_numForms << ",";
  o << form.m_extra;
  return o;
}
////////////////////////////////////////
//! Internal: the state of a MsWksDBParser
struct State {
  //! constructor
  State() : m_database(), m_widthCols(), m_forms(), m_numReports(),
    m_actPage(0), m_numPages(0), m_pageLength(-1)
  {
  }

  //! the database
  DataBase m_database;
  /** the column size in pixels(?) */
  std::vector<int> m_widthCols;
  /** the forms */
  Forms m_forms;
  /** the number of report */
  int m_numReports;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  //! the page length in point (if known)
  int m_pageLength;
};

////////////////////////////////////////
//! Internal: the subdocument of a MsWksDBParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MsWksDBParser &pars, MWAWInputStreamPtr input, int zoneId) :
    MWAWSubDocument(&pars, input, MWAWEntry()),m_id(zoneId) {}

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
  /** the subdocument id*/
  int m_id;
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("MsWksDBParser::SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_parser);

  long pos = m_input->tell();
  //MsWksDBParser *parser = static_cast<MsWksDBParser *>(m_parser);
  //parser->sendNote(m_id);
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
MsWksDBParser::MsWksDBParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWSpreadsheetParser(input, rsrcParser, header), m_state(), m_listZones(), m_document()
{
  MWAWInputStreamPtr mainInput=input;
  if (!input) return;
  if (input->isStructured()) {
    MWAWInputStreamPtr mainOle = input->getSubStreamByName("MN0");
    if (mainOle)
      mainInput=mainOle;
  }
  m_document.reset(new MsWksDocument(mainInput, *this));
  init();
}

MsWksDBParser::~MsWksDBParser()
{
}

void MsWksDBParser::init()
{
  resetSpreadsheetListener();
  setAsciiName("main-1");

  m_state.reset(new MsWksDBParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MsWksDBParser::parse(librevenge::RVNGSpreadsheetInterface *docInterface)
{
  if (!m_document || !m_document->getInput() || !checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = true;
  try {
#ifdef DEBUG
    MWAWInputStreamPtr &input= getInput();
    if (input->isStructured()) {
      shared_ptr<MWAWOLEParser> oleParser(new MWAWOLEParser("MN0"));
      oleParser->parse(input);
    }
#endif
    // create the asciiFile
    m_document->initAsciiFile(asciiName());

    checkHeader(0L);
    ok=createZones();
    if (ok) {
      createDocument(docInterface);
      sendDatabase();
    }
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MsWksDBParser::parse: exception catched when parsing\n"));
    ok = false;
  }
  m_document->ascii().reset();

  resetSpreadsheetListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MsWksDBParser::createDocument(librevenge::RVNGSpreadsheetInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getSpreadsheetListener()) {
    MWAW_DEBUG_MSG(("MsWksDBParser::createDocument: listener already exist\n"));
    return;
  }

  std::vector<MWAWPageSpan> pageList;
  m_state->m_actPage = 0;
  m_document->getPageSpanList(pageList, m_state->m_numPages);
  MWAWSpreadsheetListenerPtr listen(new MWAWSpreadsheetListener(*getParserState(), pageList, documentInterface));
  setSpreadsheetListener(listen);
  listen->startDocument();
  // time to send page information the graph parser and the text parser
  m_document->getGraphParser()->setPageLeftTop
  (Vec2f(72.f*float(getPageSpan().getMarginLeft()),
         72.f*float(getPageSpan().getMarginTop())+m_document->getHeaderFooterHeight(true)));
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool MsWksDBParser::createZones()
{
  if (getInput()->isStructured())
    m_document->createOLEZones(getInput());
  MWAWInputStreamPtr input = m_document->getInput();
  libmwaw::DebugFile &ascFile = m_document->ascii();
  if (!readDataBase()) return false;
  long pos = input->tell();
  if (!input->isEnd() && !readForms()) {
    MWAW_DEBUG_MSG(("MsWksDBParser::createZones: can not read the forms\n"));
    ascFile.addPos(pos);
    ascFile.addNote("Entries(DBForm):###");
    return true;
  }
  pos = input->tell();
  if (!input->isEnd() && !readReports()) {
    MWAW_DEBUG_MSG(("MsWksDBParser::createZones: can not read the report\n"));
    ascFile.addPos(pos);
    ascFile.addNote("Entries(DBReport):###");
    return true;
  }
  std::multimap<int, MsWksDocument::Zone> &typeZoneMap=m_document->getTypeZoneMap();
  // fixme: getNewZoneId here
  int mainId=MsWksDocument::Z_MAIN;
  typeZoneMap.insert(std::multimap<int,MsWksDocument::Zone>::value_type
                     (MsWksDocument::Z_MAIN,MsWksDocument::Zone(MsWksDocument::Z_MAIN, mainId)));
  pos = input->tell();
  if (input->isEnd() || input->readLong(2)!=0)
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  else {
    MWAWEntry group;
    group.setId(mainId);
    group.setName("RBDR");
    if (!m_document->m_graphParser->readRB(input,group,1)) {
      MWAW_DEBUG_MSG(("MsWksDBParser::createZones: can not read RBDR group\n"));
      ascFile.addPos(pos+2);
      ascFile.addNote("Entries(RBDR):###");
      input->seek(pos, librevenge::RVNG_SEEK_SET);
    }
  }

  // normally, the file is now parsed, let check for potential remaining zones
  while (!input->isEnd()) {
    MWAW_DEBUG_MSG(("MsWksDBParser::createZones: find some unexpected data\n"));
    pos = input->tell();
    MsWksDocument::Zone unknown;
    if (!m_document->readZone(unknown)) {
      ascFile.addPos(pos);
      ascFile.addNote("Entries(End)");
      ascFile.addPos(pos+100);
      ascFile.addNote("_");
      break;
    }
  }

  // ok, prepare the data
  m_state->m_numPages = 1;
  std::vector<int> linesH, pagesH;
  m_document->getGraphParser()->computePositions(mainId, linesH, pagesH);
  return true;
}

////////////////////////////////////////////////////////////
// read a database zone
////////////////////////////////////////////////////////////
bool MsWksDBParser::readDataBase()
{
  m_state->m_database=MsWksDBParserInternal::DataBase();

  MWAWInputStreamPtr input=m_document->getInput();
  int const vers=version();
  long pos = input->tell();
  int headerSize = vers <= 2 ? 14 : 80;
  long endHeader = pos+headerSize;
  if (!input->checkPosition(endHeader)) {
    MWAW_DEBUG_MSG(("MsWksDBParser::readDataBase: Segment A is too short\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_document->ascii();
  libmwaw::DebugStream f;
  f << "Entries(DataBase):";
  m_state->m_database.m_numFields=(int) input->readLong(2);
  m_state->m_database.m_numRecords=(int) input->readLong(2);
  f << "nFields=" << m_state->m_database.m_numFields << ",";
  f << "nRecord=" << m_state->m_database.m_numRecords << ",";
  int val;
  if (vers <= 2) {
    int numReports = (int) input->readLong(2); // 0 or 1
    m_state->m_numReports = numReports;
    if (numReports) f << "nReports=" << numReports << ",";
  }
  f << "unk1=[";
  for (int i = 0; i < 2; i++) f << input->readLong(1) << ",";
  f << "],";

  if (vers > 2) {
    int numReports = (int) input->readLong(2); // 0 or 1
    int unkn0 = (int) input->readLong(2);
    int unkn1 = (int) input->readLong(2);
    int numForms = (int) input->readLong(2); // 1 or 2
    int actualForms = (int) input->readLong(2); // 1 or 2

    m_state->m_numReports = numReports;
    m_state->m_forms.m_numForms = numForms>actualForms ? numForms:actualForms;
    if (numReports) f << "nReports=" << numReports << ",";
    if (numForms) f << "nForms=" << numForms << ",";
    if (actualForms) f << "aForm=" << actualForms << ",";
    f << "unkn2=[" << unkn0 << "," << unkn1 << "],";

  }
  /* I find: f0: numReports, f3=numForms or f4=actualForms
     unk=[0,1,],f1=1,f2=7,f3=1,f4=1,
     unk=[0,4,],f2=6,f3=1,f4=1,
     unk=[0,4,],f2=7,f3=1,f4=1,
     unk=[3,1,],f1=1,f2=7,f3=1,f4=1,
  */
  int nbElt = int(endHeader - input->tell())/2;
  for (int i = 0; i < nbElt; i++) {
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }


  input->seek(endHeader, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  if (vers <= 2) {
    if (!readFormV2()) return false;
    m_state->m_forms.m_numForms = (int) m_state->m_forms.m_listForms.size();

    if (!readFieldTypesV2()) return false;
    if (!readUnknownV2()) return false;

    /* the formula*/
    pos = input->tell();
    if (!readFormula()) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      int sz = (int) input->readLong(2);
      if (!input->checkPosition(pos+2+sz)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        MWAW_DEBUG_MSG(("MsWksDBParser::readDataBase: Segment default is too short\n"));
        return false;
      }
      ascFile.addPos(pos);
      ascFile.addNote("Entries(Formula):###");
      input->seek(pos+2+sz, librevenge::RVNG_SEEK_SET);
    }
    pos = input->tell();
    if (m_state->m_numReports && !readReportV2()) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      int sz = (int) input->readLong(2);
      if (!input->checkPosition(pos+2+sz)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        MWAW_DEBUG_MSG(("MsWksDBParser::readDataBase: Segment Form is too short\n"));
        return false;
      }

      ascFile.addPos(pos);
      ascFile.addNote("Entries(DBReport):###");

      input->seek(pos+2+sz, librevenge::RVNG_SEEK_SET);
    }

    if (!readRecords(false)) return false;
    return true;
  }

  long debRecord = input->tell();
  if (!readRecords(true)) return false;
  if (!readFieldTypes()) return false;

  // ok, we can now read the record
  pos = input->tell();
  input->seek(debRecord, librevenge::RVNG_SEEK_SET);
  if (!readRecords(false)) return false;
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  pos = input->tell();
  f.str("");
  f << "Entries(DBUnkn0):";
  // an odd segment  empty content or with f0=12,f1=12,f14=8000
  val = (int) input->readLong(2);
  if (val) f << "##unkn=" << val << ",";

  long sz=(long) input->readULong(2);
  f << "sz=" << sz << ",";

  if (!input->checkPosition(pos+4+sz)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MsWksDBParser::readDataBase: Segment Unkn0 is too short\n"));
    return false;
  }
  for (int i = 0; i < sz/2; i++) {
    val = (int) input->readULong(2);
    if (!val) continue;
    if (i > 3) f << "f" << i << "=" << std::hex << val << std::dec << ",";
    else f << "f" << i << "=" << val << ",";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  input->seek(pos+sz+4, librevenge::RVNG_SEEK_SET);

  int numData = vers==3 ? 4 : 6;
  for (int i = 0; i < numData; i++) {
    pos = input->tell();
    f.str("");
    bool ok = false;
    switch (i) {
    case 0:
      ok = readFormula();
      break;
    case 1:
      ok=readDefaultValues();
      break;
    case 2:
      if (vers == 4) ok = readSerialFormula();
      else ok = readFilters();
      break;
    case 3:
      if (vers == 4) ok = readFilters();
      break;
    case 4:
      // normally a classic version 2 document entry
      ok = input->readLong(2)==0 && m_document->readDocumentInfo((long) input->readULong(2));
      break;
    case 5: // probably similar to case 3(v3), but I always find a size=0
      break;
    default:
      MWAW_DEBUG_MSG(("MsWksDBParser::readDataBase: find some unknown type\n"));
      f << "##type=" << i << ",";
    }

    if (ok) continue;

    input->seek(pos, librevenge::RVNG_SEEK_SET);
    if (input->readLong(2) != 0) {
      input->seek(-2, librevenge::RVNG_SEEK_CUR);
      break;
    }
    sz = (long) input->readULong(2);

    if (!input->checkPosition(pos+4+sz)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    ascFile.addPos(pos);
    if (sz)
      f << "Entries(DBUnkn1)[" << i << "]:";
    else
      f << "_";
    ascFile.addNote(f.str().c_str());
  }

  return true;
}

////////////////////////////////////////////////////////////
// The form
////////////////////////////////////////////////////////////
bool MsWksDBParser::readFormV2()
{
  if (version() > 2) return false;
  if (!readColSize(m_state->m_widthCols)) return false;

  MWAWInputStreamPtr input=m_document->getInput();
  long pos = input->tell(), sz;
  libmwaw::DebugFile &ascFile = m_document->ascii();
  libmwaw::DebugStream f;

  sz = 0x1E2;
  if (!input->checkPosition(pos+sz)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MsWksDBParser::readFormV2 segment size is odd\n"));
    return false;
  }

  f << "Entries(DBForm):";
  for (int i = 0; i < 3; i++) { // always 0
    int val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  std::vector<int> const &colSize = m_state->m_widthCols;
  size_t numColSize = colSize.size();
  MsWksDBParserInternal::Form form;
  for (size_t elt = 0; elt < 60; elt++) {
    MsWksDBParserInternal::FormType fType;
    pos = input->tell();
    f.str("");
    float dim[2];
    for (int i = 0; i < 2; i++)
      dim[i] = (float) input->readLong(2)/72.0f;

    // constant size ?
    float normSize[] = { 96.f/72.f, 16.f/72.f };
    fType.m_bdbox[1] = Box2f(Vec2f(dim[0],dim[1]), Vec2f(dim[0]+normSize[0], dim[1]+normSize[1]));

    float fieldDim = elt < numColSize ? float(colSize[elt])/72.f : normSize[0];
    fType.m_bdbox[0] = Box2f(Vec2f(dim[0]+normSize[0],dim[1]),
                             Vec2f(dim[0]+normSize[0]+fieldDim, dim[1]+normSize[1]));


    int val = (int) input->readULong(2);
    if (val) // a dim, some flag ?
      f << "#unkn=" << std::hex << val << "," << std::dec;
    // I find followed by 0x2800 or 0x5000
    int what = (int) input->readULong(2);

    fType.m_extra=f.str();
    f.str("");
    f << "DBForm(Type:" << elt << "):";
    if (what) {
      // ok we keep it
      fType.m_visible = MsWksDBParserInternal::V_NAMEVALUE;
      fType.m_fieldId = int(elt)+1;
      form.m_listTypes.push_back(fType);
      f << fType << "what=" << std::hex << what << std::dec << ",";
    }
    else
      f << "unused,";

    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  m_state->m_forms.m_listForms.push_back(form);

  return true;
}


bool MsWksDBParser::readForms()
{
  if (version() <= 2) return false;

  if (!readColSize(m_state->m_widthCols)) return false;

  MWAWInputStreamPtr input=m_document->getInput();
  long pos = input->tell();
  libmwaw::DebugFile &ascFile = m_document->ascii();
  libmwaw::DebugStream f;

  long endPos = pos+44;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("MsWksDBParser::readForms: first zone is too short\n"));
    return false;
  }

  MsWksDBParserInternal::Forms &forms = m_state->m_forms;
  // I find f2=[0|1], f5=[0|8|24|169|201|260],f7=1, f8=1
  for (int i = 0; i < 9; i++) {
    int val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  MWAWFont font;
  font.setId((int) input->readULong(2));
  font.setSize((float) input->readLong(2));
  int val = (int) input->readLong(2);
  if (val) f << "#unkn=" << val << ",";

  int col[2];
  for (int i = 0; i < 2; i++) col[i] = (int) input->readULong(1);
  forms.m_backColor = col[0];
  MWAWColor color;
  if (col[1] != 0xFF && m_document->getColor(col[1], color, 3))
    font.setColor(color);
  // some flags ?
  f << "fFlags?=" << input->readULong(1) << ","; // always 1
  val = (int) input->readLong(1); // always 0
  if (val) f << "#unkn1=" << val << ",";
  forms.m_font = font;

  for (int bd = 0; bd < 2; bd++) { // two bdbox
    float dim[4];
    for (int i = 0; i < 4; i++) dim[i] = (float) input->readLong(2)/1440.f;
    forms.m_bdbox[bd] = Box2f(Vec2f(dim[0],dim[1]), Vec2f(dim[2], dim[3]));
  }
  forms.m_extra=f.str();

  f.str("");
  f << "Entries(DBForm):" << forms;
  f << "font=[" << forms.m_font.getDebugString(getParserState()->m_fontConverter) << "],";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  // report form can have 0 entry, so..
  if (input->isEnd()) return true;

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  for (int i = 0; i < forms.m_numForms; i++) {
    if (!readForm()) return false;
  }
  return true;
}

bool MsWksDBParser::readForm()
{
  MWAWInputStreamPtr input=m_document->getInput();
  int const vers=version();
  long pos = input->tell();
  libmwaw::DebugFile &ascFile = m_document->ascii();
  libmwaw::DebugStream f, f2;

  int expLength = (vers == 3) ? 108 : 84;

  long endPos = pos+expLength;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("MsWksDBParser::readForm: first zone is too short\n"));
    return false;
  }

  MsWksDBParserInternal::Forms &forms = m_state->m_forms;
  int numForm = (int) forms.m_listForms.size();
  MsWksDBParserInternal::Form form;

  int val = (int) input->readLong(2);
  if (val) f << "unkn=" << val << ",";
  f << "f0=" << input->readLong(2) << ",";

  long sz = (long) input->readULong(1);
  if (sz > 31) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MsWksDBParser::readForm: field name is too long\n"));
    return false;
  }
  std::string name("");
  for (long i = 0; i < sz; i++) name += (char) input->readULong(1);
  form.m_name = name;

  input->seek(pos+4+32, librevenge::RVNG_SEEK_SET);

  float dim[4];
  for (int bd = 0; bd < 2; bd++) { // two bdbox
    for (int i = 0; i < 4; i++) dim[i] = (float)input->readLong(2)/1440.f;
    form.m_bdbox[bd] = Box2f(Vec2f(dim[0],dim[1]), Vec2f(dim[2], dim[3]));
  }

  if (vers == 4) {
    // [16|1d], [348a|a412|b169] : headerId?
    f << "unk2=[" << input->readLong(2) << ",";
    f << std::hex  << input->readULong(2) << std::dec << "],";
  }

  // page bdbox
  for (int i = 0; i < 4; i++) dim[i] = (float) input->readLong(2)/72.f;
  form.m_bdbox[2] = Box2f(Vec2f(dim[0],dim[1]), Vec2f(dim[2], dim[3]));

  int numData = int(endPos-input->tell())/2;
  // g3 = [1,2,3], g4=[0,1,7], g6=[0,2,6], g7=[0,103,104,206]
  // g4 = 1 : header ?
  for (int i = 0; i < numData; i++) {
    val = (int) input->readLong(2);
    if (val) f << "g" << i << "=" << std::hex << val << std::dec << ",";
  }

  f2 << "DBForm-" << numForm << ":" << form << "," << f.str();

  ascFile.addPos(pos);
  ascFile.addNote(f2.str().c_str());
  input->seek(endPos, librevenge::RVNG_SEEK_SET);

  if (!readFormTypes(form)) return false;
  forms.m_listForms.push_back(form);

  // a form document entry followed
  pos = input->tell();
  if (input->readLong(2) != 0 || !m_document->readDocumentInfo((long) input->readULong(2))) {
    MWAW_DEBUG_MSG(("MsWksDBParser::readForm: can not find document info\n"));
    ascFile.addPos(pos);
    ascFile.addNote("DBForm:end###");
    return false;
  }

  if (vers == 3) { // version3 file have 4 int here
    for (int i = 0; i < 2; i++) {
      pos = input->tell();
      f.str("");
      sz = (long) input->readULong(4);
      endPos = pos+4+sz;

      if (!input->checkPosition(endPos)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        MWAW_DEBUG_MSG(("MsWksDBParser::readForm: Segment v3 is too long\n"));
        break;
      }
      f << "DBForm(v3-" << i << "):";
      input->seek(endPos, librevenge::RVNG_SEEK_SET);
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
  }
  // finally the list of graphics
  MWAWEntry group;
  group.setId(1);
  group.setName("RBDR");

  if (!m_document->m_graphParser->readRB(input,group,1)) {
    MWAW_DEBUG_MSG(("MsWksDBParser::readForm: can not read RBDR group\n"));
    ascFile.addPos(pos);
    ascFile.addNote("Entries(RBDR):###");
    return false;
  }

  return true;
}

bool MsWksDBParser::readFormTypes(MsWksDBParserInternal::Form &form)
{
  form.m_listTypes.resize(0);

  int const vers=version();
  MWAWInputStreamPtr input=m_document->getInput();
  long pos = input->tell();
  libmwaw::DebugFile &ascFile = m_document->ascii();
  libmwaw::DebugStream f;

  int expDataSize = (vers == 3) ? 28 : 42;

  int numFields = m_state->m_database.m_numFields;
  int numTypes = vers == 3 ? 254 : numFields;
  if (vers==4 && numTypes<4) numTypes=4; // at least 4?

  if (!input->checkPosition(pos+expDataSize*numTypes)) {
    MWAW_DEBUG_MSG(("MsWksDBParser::readFormTypes: is too short\n"));
    return false;
  }

  for (int nType = 0; nType < numTypes; nType++) {
    MsWksDBParserInternal::FormType fType;
    MsWksDBParserInternal::FormVisibility visible = MsWksDBParserInternal::V_UNKNOWN;
    pos = input->tell();
    f.str("");

    // in v3 fls[0] = 30=hidden ?, 40 = empty, 50=unused, 70=visible ? 80=selection
    // in v4 fls[0] = 38, 58, 78 ?
    // fls[1] seems constant in each file 0, a4, b3
    int flag= (int) input->readULong(1);
    int hiFlag = flag >> 4, lowFlag = flag & 0xF;
    if (hiFlag & 0x8) f << "[sel]"; // the selection
    switch (hiFlag & 7) {
    case 3:
      visible = MsWksDBParserInternal::V_VALUE;
      break;
    case 4:
      visible = MsWksDBParserInternal::V_UNDEF;
      break;
    case 5:
      visible = MsWksDBParserInternal::V_HIDDEN;
      break;
    case 7:
      visible = MsWksDBParserInternal::V_NAMEVALUE;
      break;
    case 0:
      if (nType == 0) {
        visible = MsWksDBParserInternal::V_HEADER;
        break;
      }
    default:
      f << "##unkn[visible]=" << hiFlag << ",";
      break;
    }
    fType.m_visible = visible;

    if (vers == 4) {
      if ((lowFlag & 8) != 8 && nType != 0) f << "###flagNoV4,";
      lowFlag &= 7;
    }
    if (lowFlag) f << "###VisFlag=" << lowFlag << ",";
    f << "flg=" << std::hex <<  input->readULong(1)  << std::dec << ",";

    // font,sz
    MWAWFont font;
    font.setId((int) input->readULong(2));
    font.setSize((float) input->readULong(2));
    int fontColor = (int) input->readULong(1);
    int backColor = (int) input->readULong(1);
    int fontFlags = (int) input->readULong(1);
    uint32_t fflags = 0;
    if (fontFlags) { // checkme
      if (fontFlags & 0x1) flag |= MWAWFont::boldBit;
      if (fontFlags & 0x2) flag |= MWAWFont::italicBit;
      if (fontFlags & 0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
      if (fontFlags & 0x8) flag |= MWAWFont::embossBit;
      if (fontFlags & 0x10) flag |= MWAWFont::shadowBit;
      if (fontFlags &0xE0) f << "fFlags=" << std::hex << (fontFlags &0xE0) << std::dec << ",";
    }
    font.setFlags(fflags);

    MWAWColor color;
    if (fontColor != 255 && m_document->getColor(fontColor, color, 3))
      font.setColor(color);
    fType.m_font = font;
    fType.m_backColor = backColor;

    int val = (int) input->readLong(1);
    // font flags
    if (val != fontFlags) f  << "#fFlagsView=" << val << ",";

    if (vers == 4) {
      // always 0101 ?
      f << "unkn1=" << std::hex << input->readLong(2) << std::dec << ",";

      /** [ff,5c,_,-8,_,-9] or [ff,a8,_,-8,_,-9] or [_,5c,_,-8,_,-9]**/
      f << "unkn2=[";
      for (int i = 0; i < 2; i++) {
        val = (int) input->readULong(1);
        if (val) f << std::hex << val << std::dec << ",";
        else f << "_,";
      }
      for (int i = 0; i < 4; i++) {
        val = (int) input->readLong(2);
        if (val) f << val << ",";
        else f << "_,";
      }
      f << "],";
    }

    float dim[4];
    for (int bd = 0; bd < 2; bd++) { // two bdbox
      for (int i = 0; i < 4; i++) dim[i] = (float)input->readLong(2)/72.f;
      fType.m_bdbox[bd] = Box2f(Vec2f(dim[1],dim[0]), Vec2f(dim[3], dim[2]));
    }

    // 0, 0 in v3, in v4 : two small number < 6
    for (int i = 0; i < 2; i++) {
      val = (int) input->readLong(1);
      if (val) f << "g" << i << "=" << val << ",";
    }


    if (vers == 4)
      f << "unkn3=" << std::hex << input->readULong(2) << std::dec;

    if (visible == MsWksDBParserInternal::V_NAMEVALUE ||
        visible == MsWksDBParserInternal::V_VALUE) {
      // ok we keep it
      fType.m_fieldId = nType;
      form.m_listTypes.push_back(fType);
    }

    input->seek(pos+expDataSize, librevenge::RVNG_SEEK_SET);

    libmwaw::DebugStream f2;
    f2 << "DBForm(Type:" << nType << "):";
    // sometimes all types are filed, sometimes only the used fields
    if (nType < numFields) {
      f2  << fType;
      f2 << "font=[" << fType.m_font.getDebugString(getParserState()->m_fontConverter) << "],";
      f2 << f.str();
    }
    ascFile.addPos(pos);
    ascFile.addNote(f2.str().c_str());
  }

  return true;
}

////////////////////////////////////////////////////////////
// The records
////////////////////////////////////////////////////////////

// reads the fields name and the fields value :
bool MsWksDBParser::readRecords(bool onlyCheck)
{
  int numRecord = m_state->m_database.m_numRecords;
  int const vers=version();
  if (numRecord < 0) {
    MWAW_DEBUG_MSG(("MsWksDBParser::readRecords: numRecords can not be negatifn"));
    return false;
  }

  MWAWInputStreamPtr input=m_document->getInput();

  long pos = input->tell();
  std::vector<MsWksDBParserInternal::FieldType> &listFields = m_state->m_database.m_listFieldTypes;
  int numFieldsHeader = (int) listFields.size();
  int numFields = m_state->m_database.m_numFields; // sometimes there is another field

  if (!onlyCheck && numFieldsHeader+1 < numFields) {
    MWAW_DEBUG_MSG(("MsWksDBParser::readRecords: Warning database.listFieldTypes is too short, some fields will be loose\n"));
  }

  libmwaw::DebugFile &ascFile = m_document->ascii();
  libmwaw::DebugStream f;
  int ptrSize = vers <= 2 ? 1 : 2;
  long sz = (long) input->readULong(2*ptrSize);
  f.str("");
  f << "Entries(FieldName): sz = " << sz;
  long endPos = pos+2*ptrSize+sz;

  if (!input->checkPosition(endPos)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MsWksDBParser::readRecords: Segment FieldName is too short\n"));
    return false;
  }

  if (!onlyCheck) {
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  // une liste de string
  for (int nField = 0; nField <= numFields; nField++) {
    pos = input->tell();
    f.str("");
    int sLen = (int) input->readULong(1);
    if (sLen == 255 && nField == numFields) {
      input->seek(-1, librevenge::RVNG_SEEK_CUR);
      break;
    }
    if (pos+sLen > endPos-ptrSize) {
      MWAW_DEBUG_MSG(("MsWksDBParser::readRecords: error reading segment FieldName\n"));
      input->seek(-1, librevenge::RVNG_SEEK_CUR);
      return false;
    }
    std::string str;
    for (int i = 0; i < sLen; i++) str+= (char) input->readULong(1);

    if (nField == numFields) {
      MWAW_DEBUG_MSG(("MsWksDBParser::readRecords: WARNING: numFields must be increased to correspond to the fName's list\n"));
      numFields++;
      m_state->m_database.m_numFields++;
    }
    if (onlyCheck) continue;

    if (nField+1 < numFieldsHeader)
      listFields[size_t(nField+1)].m_name = str;
    else {
      static bool first = true;
      if (first) {
        MWAW_DEBUG_MSG(("MsWksDBParser::readRecords: warning FieldName=%d ignored\n", nField));
        first = false;
      }
    }
    f << "FieldName-" << nField << "=" << str;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

  }

  pos = input->tell();
  bool ok = input->readULong(1) == 255;
  if (ok && ptrSize == 2) ok = input->readULong(1) == 0;

  if (!ok || input->tell() != endPos) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MsWksDBParser::readRecords: End of Segment FieldName is odd\n"));
    return false;
  }

  if (!onlyCheck) {
    ascFile.addDelimiter(pos,'|');
    m_state->m_database.m_listRecords.resize(size_t(numRecord));
  }

  for (int rec = 0; rec < numRecord; rec++) {
    pos = input->tell();
    f.str("");

    f << "Entries(DBRecord)["<< rec << "]:";
    sz =(long) input->readULong(2*ptrSize);
    f << "sz=" << sz << ",";

    endPos = pos+2*ptrSize+sz;
    if (!input->checkPosition(endPos)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }

    std::vector<MWAWCellContent> *row = 0L;
    if (!onlyCheck) {
      row = &m_state->m_database.m_listRecords[size_t(rec)];
      row->resize(size_t(numFields));

      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    for (int nField = 0; nField < numFields; nField++) {
      pos = input->tell();
      f.str("");
      f << "DBRecord["<< rec << "-" << nField << "]:";

      int fSz = (int) input->readULong(1);
      if (vers <= 2 && fSz >= 254) { // only for v2 or can we find it for v2 or v3 ?
        if (fSz == 254) {
          int skip = (int) input->readLong(1);
          if (skip > 0 && skip+nField < numFields) {
            nField+=skip-1;
            f << "skip=" << skip;
            ascFile.addPos(pos);
            ascFile.addNote(f.str().c_str());
            continue;
          }
        }
        else if (fSz == 255) {
          input->seek(-1, librevenge::RVNG_SEEK_CUR);
          break;
        }
      }
      long ePos = pos+1+fSz;
      if (ePos > endPos) {
        MWAW_DEBUG_MSG(("MsWksDBParser::readRecords: Record Content is too short\n"));
        input->seek(-1, librevenge::RVNG_SEEK_CUR);
        return false;
      }
      if (onlyCheck) {
        input->seek(ePos, librevenge::RVNG_SEEK_SET);
        continue;
      }

      ok = false;
      f.str("");
      f << "DBRecord["<< rec << "-" << nField << "]:";
      MWAWCellContent record;
      if (fSz == 0) ok = true;
      else if (nField < numFieldsHeader) {
        double value;
        bool isNan;
        std::string textValue;
        if (listFields[size_t(nField)].getFormat().m_format == MWAWCell::F_TEXT) {
          record.m_textEntry.setBegin(pos+1);
          record.m_textEntry.setLength(fSz);
          record.m_contentType=MWAWCellContent::C_TEXT;
          ok = m_document->readDBString(ePos, textValue);
        }
        else if (m_document->readDBNumber(ePos, value, isNan, textValue)) {
          record.setValue(value);
          record.m_contentType=MWAWCellContent::C_NUMBER;
          f << value << ",";
          ok=true;
        }
        if (!textValue.empty()) f << "\"" << textValue << "\",";
      }

      if (!ok) {
        static bool first = true;
        if (first) {
          MWAW_DEBUG_MSG(("MsWksDBParser::readRecords: warning Record=%d:%d ignored\n", rec, nField));
          first = false;
        }
        input->seek(ePos, librevenge::RVNG_SEEK_SET);
      }

      (*row)[size_t(nField)] = record;

      f << record;
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }

    pos = input->tell();
    ok = input->readULong(1) == 255;
    if (ok && ptrSize == 2) {
      int val=(int) input->readULong(1);
      if (!onlyCheck && val != 0) {
        // it is related to the selection : 0x80=filter, 0x40=match
        ascFile.addPos(pos);
        f.str("");
        f << "DBRecord["<< rec << "]:sel=" << std::hex << val << std::dec;
        ascFile.addNote(f.str().c_str());
      }
    }

    if (!ok || input->tell() != endPos) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      MWAW_DEBUG_MSG(("MsWksDBParser::readRecords: End of Record is odd\n"));
      return false;
    }

    if (!onlyCheck) ascFile.addDelimiter(pos,'|');
  }

  return true;
}

////////////////////////////////////////
// list of the fields type
////////////////////////////////////////
bool MsWksDBParser::readFieldTypes()
{
  MWAWInputStreamPtr input=m_document->getInput();

  long pos = input->tell();

  libmwaw::DebugFile &ascFile = m_document->ascii();
  libmwaw::DebugStream f;

  f << "Entries(FieldType):";
  int val = (int) input->readLong(2);
  if (val) f << "##unkn=" << val << ",";

  long sz =(long) input->readULong(2);
  f << "sz=" << sz << ",";
  long endFieldTypes = pos+4+sz;

  if (sz != 255*14 || !input->checkPosition(endFieldTypes)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MsWksDBParser::readFieldTypes: Segment length is odd %ld\n", sz));
    return false;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  std::vector<MsWksDBParserInternal::FieldType> &listFields = m_state->m_database.m_listFieldTypes;
  listFields.resize(255);
  for (size_t  elt = 0; elt < 255; elt++) {
    MsWksDBParserInternal::FieldType fieldType;
    MWAWCell::Format format;
    MWAWCellContent &content=fieldType.m_content;
    f.str("");
    pos = input->tell();
    val = (int) input->readULong(2);
    int type = (val>>13) & 3;
    int inputType = (val>>6) & 0x3;
    int subformat = ((val>>8) & 0x7);
    format.m_digits = (val>>2) & 0xF;
    switch ((val&3)) {
    case 0:
      fieldType.setHAlignement(MWAWCell::HALIGN_LEFT);
      break;
    case 1:
      fieldType.setHAlignement(MWAWCell::HALIGN_RIGHT);
      break;
    case 2:
      fieldType.setHAlignement(MWAWCell::HALIGN_CENTER);
      break;
    default:
      f << "#align=3,";
      break;
    }
    val &= 0x9800;

    switch (type) {
    case 0:
      content.m_contentType=MWAWCellContent::C_TEXT;
      format.m_format=MWAWCell::F_TEXT;
      break;
    case 1:
      format.m_format=MWAWCell::F_NUMBER;
      format.m_numberFormat=MWAWCell::F_NUMBER_GENERIC;
      content.m_contentType=MWAWCellContent::C_NUMBER;
      switch (subformat) { // checkme
      case 0:
        break;
      case 1:
        format.m_numberFormat=MWAWCell::F_NUMBER_DECIMAL;
        break;
      case 2:
        format.m_numberFormat=MWAWCell::F_NUMBER_CURRENCY;
        break;
      case 3:
        format.m_thousandHasSeparator=true;
        break;
      case 4:
        format.m_numberFormat=MWAWCell::F_NUMBER_CURRENCY;
        format.m_thousandHasSeparator=true;
        break;
      case 5:
        format.m_numberFormat=MWAWCell::F_NUMBER_SCIENTIFIC;
        break;
      case 6:
        format.m_numberFormat=MWAWCell::F_NUMBER_PERCENT;
        break;
      case 7:
        format.m_numberFormat=MWAWCell::F_NUMBER_PERCENT;
        format.m_thousandHasSeparator=true;
        break;
      default:
        f << "subform=##unkn" << subformat << ",";
        break;
      }
      subformat=0;
      break;
    case 2:
      format.m_format=MWAWCell::F_DATE;
      content.m_contentType=MWAWCellContent::C_NUMBER;
      if (subformat>=0 && subformat < 5) { // checkme
        static char const *(wh[])= {"%m/%d/%y", "%b %d, %Y", "%b, %d", "%b, %Y", "%a, %d %b, %Y" };
        format.m_DTFormat=wh[subformat];
        subformat=0;
      }
      break;
    case 3:
      format.m_format=MWAWCell::F_TIME;
      content.m_contentType=MWAWCellContent::C_NUMBER;
      if (subformat >= 0 && subformat < 4) { // checkme
        static char const *(wh[])= {"%I:%M:%S %p", "%I:%M %p", "%H:%M:%S", "%H:%M"};
        format.m_DTFormat=wh[subformat];
        subformat=0;
      }
      break;
    default:
      break;
    }
    switch (inputType) {
    default:
    case 0:
      break; /* none */
    case 1:
      fieldType.m_isSerial=true;
      content.m_contentType=MWAWCellContent::C_FORMULA;
      break;
    case 2:
      content.m_contentType=MWAWCellContent::C_FORMULA;
      break;
    case 3:
      f << "###inputType(unknown),";
      break;
    }
    if (subformat) f << "format[extra]=#" << subformat << ",";
    MWAWFont font;
    font.setId((int) input->readULong(2));
    font.setSize((float) input->readULong(2));
    int fontUnkn = (int) input->readLong(2); // 0x200 and in one field 0x300
    int fontFlags = (int) input->readLong(1);
    int fColor = (int) input->readULong(1);
    int unkn = (int) input->readULong(1); // always 0 ? back color?
    uint32_t fflags = 0;
    if (fontFlags) { // checkme
      if (fontFlags & 0x1) fflags |= MWAWFont::boldBit;
      if (fontFlags & 0x2) fflags |= MWAWFont::italicBit;
      if (fontFlags & 0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
      if (fontFlags & 0x8) fflags |= MWAWFont::embossBit;
      if (fontFlags & 0x10) fflags |= MWAWFont::shadowBit;
      if (fontFlags &0xE0) f << "fFlags=" << std::hex << (fontFlags &0xE0) << std::dec << ",";
    }
    font.setFlags(fflags);
    MWAWColor color;
    if (fColor != 255 && m_document->getColor(fColor, color, 3))
      font.setColor(color);
    fieldType.setFont(font);
    fieldType.m_height = (int) input->readULong(1);
    fieldType.m_serialId = (int) input->readULong(1);
    int what = (int) input->readLong(1);
    switch (what) {
    case 0:
      fieldType.m_used=false;
      break;
    case 0x60:
      break;
    default:
      f << "##unknWhat=" << std::hex << what << std::dec << ",";
    }
    if (fontUnkn != 0x200) f << "##unkn0="<< fontUnkn << ",";
    if (unkn) f << "###unkn1="<< unkn << ",";
    if (val) f << "###flags=" << val;
    fieldType.m_extra=f.str();
    fieldType.setFormat(format);

    f.str("");
    if (elt) {
      f << "FieldType-" << elt-1 << ":" << fieldType;
      listFields[elt-1] = fieldType;
    }
    else
      f << "FieldType-def:" << fieldType;
    f << "font=[" << font.getDebugString(getParserState()->m_fontConverter) << "],";

    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  input->seek(endFieldTypes, librevenge::RVNG_SEEK_SET);
  return true;
}

bool MsWksDBParser::readFieldTypesV2()
{
  MWAWInputStreamPtr input=m_document->getInput();

  long pos = input->tell();

  libmwaw::DebugFile &ascFile = m_document->ascii();
  libmwaw::DebugStream f;

  f << "Entries(FieldType):";

  long sz =60*4+4;
  long endFieldTypes = pos+sz;

  if (!input->checkPosition(endFieldTypes)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MsWksDBParser::readFieldTypesV2: Segment length is odd %ld\n", sz));
    return false;
  }
  for (int i = 0; i < 2; i++)
    f << input->readLong(2) << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());


  std::vector<MsWksDBParserInternal::FieldType> &listFields = m_state->m_database.m_listFieldTypes;
  listFields.resize(60);
  for (int elt = 0; elt < 60; elt++) {
    MsWksDBParserInternal::FieldType fieldType;
    MWAWCellContent &content=fieldType.m_content;
    MWAWCell::Format format;
    f.str("");
    pos = input->tell();
    int val = (int) input->readULong(2);
    int type = (val>>14) & 3;
    format.m_digits = (val>>8) & 0xF;
    switch (((val>>5) & 3)) {
    case 0:
      fieldType.setHAlignement(MWAWCell::HALIGN_LEFT);
      break;
    case 1:
      fieldType.setHAlignement(MWAWCell::HALIGN_RIGHT);
      break;
    case 2:
      fieldType.setHAlignement(MWAWCell::HALIGN_CENTER);
      break;
    default:
      f << "#align=3,";
      break;
    }

    int form = val & 7;
    val &= 0x3098;

    switch (type) {
    default:
    case 0:
      format.m_format=MWAWCell::F_TEXT;
      content.m_contentType=MWAWCellContent::C_TEXT;
      break;
    case 1:
      format.m_format=MWAWCell::F_NUMBER;
      format.m_numberFormat=MWAWCell::F_NUMBER_GENERIC;
      content.m_contentType=MWAWCellContent::C_NUMBER;
      break;
    case 2:
      format.m_format=MWAWCell::F_NUMBER;
      format.m_numberFormat=MWAWCell::F_NUMBER_GENERIC;
      content.m_contentType=MWAWCellContent::C_FORMULA;
      break;
    case 3:
      f << "##unknType,";
      break;
    }
    if (format.m_format==MWAWCell::F_NUMBER) {
      bool ok = true;
      switch (form) {
      case 1:
        f << "format[extra]=1,";
        break;
      case 3:
        break;
      case 6:
        f << "format[extra]=6,";
      case 5:
        format.m_format=MWAWCell::F_DATE;
        break;
      case 7:
        format.m_format=MWAWCell::F_TIME;
        break;
      default:
        ok = false;
      }
      if (ok) form = 0;
    }
    if (form) f << "#format[extra]=" << form << ",";
    int unkn = (int) input->readULong(1); // always 0 ?
    if (unkn) f << "##unkn=" << std::hex << unkn << std::dec << ",";
    int what = (int) input->readULong(1);

    switch (what) {
    case 0x80:
      break;
    default:
      f << "##unknWhat=" << std::hex << what << std::dec << ",";
    }
    fieldType.m_extra=f.str();
    fieldType.setFormat(format);

    f.str("");
    f << "FieldType-" << elt << ":" << fieldType;
    listFields[size_t(elt)] = fieldType;

    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  input->seek(endFieldTypes, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// the filters
////////////////////////////////////////////////////////////
bool MsWksDBParser::readFilters()
{
  MWAWInputStreamPtr input=m_document->getInput();

  long pos = input->tell();
  if (input->readLong(2) != 0) {
    MWAW_DEBUG_MSG(("MsWksDBParser::readFilters: size is odd\n"));
    return false;
  }

  libmwaw::DebugFile &ascFile = m_document->ascii();
  libmwaw::DebugStream f;
  f << "Entries(Filter):";

  long sz = (long) input->readULong(2);
  f << "sz=" << sz << ",";

  long endPos = pos+4+sz;
  if ((sz%514) != 8 || !input->checkPosition(endPos)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MsWksDBParser::readFilters: Segment length is odd %ld\n", sz));
    return false;
  }

  int numFilters = int(sz/514);
  f << "numFilters=" << numFilters << ",";

  int val = (int) input->readLong(2);
  if (val != numFilters) f << "###val=" << val << ",";
  for (int i = 0; i < 3; i++) f << input->readLong(2) << ",";

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  for (int filt = 0; filt < numFilters ; filt++) {
    pos = input->tell();
    f.str("");
    f << "Filter-" << filt << ":";

    sz = (long) input->readLong(1);
    if (sz <= 0 || sz > 31) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      MWAW_DEBUG_MSG(("MsWksDBParser::readFilters: filter name length is odd %ld\n", sz));
      return false;
    }
    std::string name("");
    for (long i = 0; i < sz; i++)
      name += (char) input->readULong(1);
    f << "name=\"" << name << "\",";
    input->seek(pos+32, librevenge::RVNG_SEEK_SET);

    val = (int) input->readLong(1);
    switch (val) {
    case 0:
      break;
    case 1:
      f << "inverted,";
      break;
    default:
      f << "###unknOp=" << val<<",";
      break;
    }
    val = (int) input->readLong(1);
    if (val) f << "###unkn=" << val;

    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    for (int i = 0; i < 6; i++) {
      pos = input->tell();
      f.str("");
      f << "Filter" << filt << "-" << i << ":";

      int field = (int) input->readLong(1);
      int op = (int) input->readLong(1);
      int op2 = (int) input->readLong(1);

      const char *(opName[]) = {
        "eq", "neq", "gt", "isBlank(not)", "geq", "lt", "leq", "contains",
        "contains(not)", "ends with", "ends with(not)",
        "begins with", "begins with(not)", "isBlank"
      };
      if (op < 0 || op >= 14 || op2 <= 0 || op2 > 2) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        MWAW_DEBUG_MSG(("MsWksDBParser::readFilters: unknown op=%d op2=%d\n", op, op2));
        return false;
      }

      ascFile.addDelimiter(input->tell(), '|');
      input->seek(pos+16, librevenge::RVNG_SEEK_SET);
      ascFile.addDelimiter(input->tell(), '|');
      sz = input->readLong(1);
      if (sz < 0 || sz > 63) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        MWAW_DEBUG_MSG(("MsWksDBParser::readFilters: val length is odd:%ld\n", sz));
        return false;
      }

      if (sz || op == 3 || op == 13) {
        std::string value;
        for (long c = 0; c < sz; c++) value += (char) input->readLong(1);
        if (i) {
          if (op2==1) f << "And ";
          else f << "Or ";
        }
        f << "Field" << field;
        f << " " << opName[op] << " " << value;
      }
      input->seek(pos+80, librevenge::RVNG_SEEK_SET);
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
  }

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// The report
////////////////////////////////////////////////////////////
bool MsWksDBParser::readReports()
{
  if (m_state->m_numReports == 0) return true;

  int const vers=version();
  MWAWInputStreamPtr input=m_document->getInput();
  long pos;
  libmwaw::DebugFile &ascFile = m_document->ascii();
  libmwaw::DebugStream f;

  int const numData=vers==4 ? 7 : 5;
  for (int rep = 0; rep < m_state->m_numReports; rep++) {
    for (int step = 0; step < numData; step++) {
      // 0 header
      // 1 list of field
      // 2-3 ?
      // 4 documentEntry
      // 5-6 RBDR
      pos = input->tell();
      f.str("");
      if (input->isEnd()) break;
      long sz = input->readLong(4);
      if (sz == -1) sz = 0; // seems normal
      if (!input->checkPosition(pos+4+sz)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        MWAW_DEBUG_MSG(("MsWksDBParser::readReports: report%d-%d is too long\n",rep,step));
        return false;
      }

      input->seek(pos, librevenge::RVNG_SEEK_SET);
      switch (step) {
      case 0:
        if (readReportHeader()) continue;
        break;
      case 1:
        if (vers == 3) {
          if (input->readLong(2) != 0 || !m_document->readDocumentInfo((long) input->readULong(2)))
            break;
          continue;
        }
        break;
      case 4:
        if (vers == 4) {
          if (input->readLong(2) != 0 || !m_document->readDocumentInfo((long) input->readULong(2)))
            break;
          continue;
        }
        break;
      default: {
        if (step < 4) break;
        MWAWEntry group;
        group.setId(1);
        group.setName("RBDR");

        if (m_document->m_graphParser->readRB(input,group,1)) continue;
        break;
      }
      }

      long endPos = pos+4+sz;
      input->seek(pos+4, librevenge::RVNG_SEEK_SET);
      if (step == 0) f << "Entries(DBReport):";
      else f << "DBReport-" << step << ":";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      input->seek(endPos, librevenge::RVNG_SEEK_SET);
    }
    if (vers == 4 || input->isEnd())
      continue;

    // this seems to be followed by a form: incomplete?
    pos=input->tell();
    if (!readForms()) {
      MWAW_DEBUG_MSG(("MsWksDBParser::readReports: can not read forms size\n"));
      ascFile.addPos(pos);
      ascFile.addNote("DBReport-forms:###");
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
  }
  return true;
}

////////////////////
////////////////////
bool MsWksDBParser::readReportHeader()
{
  if (version() != 4) return false;
  MWAWInputStreamPtr input=m_document->getInput();
  long pos = input->tell();
  libmwaw::DebugFile &ascFile = m_document->ascii();
  libmwaw::DebugStream f;

  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  if (sz < 32 || !input->checkPosition(endPos)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MsWksDBParser: readReportHeader: segment size is odd\n"));
    return false;
  }

  f << "Entries(DBReport):";
  int val=(int) input->readLong(1);
  if (val < 0 || val > 31) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MsWksDBParser: readReportHeader: name size is odd=%d\n", val));
    return false;
  }
  std::string name("");
  for (int i = 0; i < val; i++) name += (char) input->readULong(1);
  f << "\"" << name << "\",";
  input->seek(pos+4+32, librevenge::RVNG_SEEK_SET);

  val = (int) input->readLong(2);
  if (val) f << "###unkn0=" << val << ",";

  // font,sz
  MWAWFont font;
  font.setId((int) input->readULong(2));
  font.setSize((float) input->readULong(2));
  int fontColor = (int) input->readULong(1);
  int backColor = (int) input->readULong(1);
  int fontFlags = (int) input->readULong(1);
  uint32_t fflags = 0;
  if (fontFlags) { // checkme
    if (fontFlags & 0x1) fflags |= MWAWFont::boldBit;
    if (fontFlags & 0x2) fflags |= MWAWFont::italicBit;
    if (fontFlags & 0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (fontFlags & 0x8) fflags |= MWAWFont::embossBit;
    if (fontFlags & 0x10) fflags |= MWAWFont::shadowBit;
    if (fontFlags &0xE0) f << "fFlags=" << std::hex << (fontFlags &0xE0) << std::dec << ",";
  }
  font.setFlags(fflags);

  MWAWColor color;
  if (fontColor != 255 && m_document->getColor(fontColor, color, 3))
    font.setColor(color);
  f << "font=[" << font.getDebugString(getParserState()->m_fontConverter);
  if (backColor) f << "backcolor=" << backColor;
  f << "],";
  val = (int) input->readLong(1);
  // font flags
  if (val != fontFlags) f  << "#fFlagsView=" << val << ",";

  // a bdbox ?
  float dim[4];
  for (int i = 0; i < 4; i++) dim[i] = (float) input->readLong(2)/72.f;
  f << "bdbox(title)=" << Box2f(Vec2f(dim[0],dim[1]), Vec2f(dim[2], dim[3])) << ",";

  f << "unk0=["; // always _, _, 1, _, _, 1
  for (int i = 0; i < 6; i++) {
    val = (int) input->readLong(1);
    if (val) f << val << ",";
    else f << "_,";
  }
  f << "],pos?=[";
  for (int i = 0; i < 2; i++) f << (float) input->readLong(2)/1440.f << ",";
  f << "],";
  // checkMe
  int numReportData = (int) input->readLong(2);
  if (numReportData) f << "numReportData=" << numReportData << ",";
  for (int i = 0; i < 2; i++) dim[i] = (float) input->readLong(2)/1440.f;
  bool hasPos = dim[0]>0 || dim[1]>0;
  if ((hasPos && numReportData== 0) || (!hasPos && numReportData)) {
    MWAW_DEBUG_MSG(("MsWksDBParser: readReportHeader: potential problem with ReportData definition\n"));
    f << "###ReportData,";
  }
  if (hasPos) f << "posReportData?=[" << dim[0] << "," << dim[1] << "],";
  f << "unk1=["; // always [_,_,_,_,_,_,_,1,_,_,_,_,] ?
  for (int i = 0; i < 12; i++) {
    val = (int) input->readLong(1);
    if (val) f << val << ",";
    else f << "_,";
  }
  f << "],";

  // another bdbox : related to data size
  for (int i = 0; i < 4; i++) dim[i] = (float) input->readLong(2)/1440.f;
  if (dim[0]>0 || dim[1]>0 || dim[2]>0 || dim[3]>0)
    f << "bdbox2=" << Box2f(Vec2f(dim[0],dim[1]), Vec2f(dim[2], dim[3])) << ",";

  f << "unk2=["; // always [_,_,_,_,_,_,_,_] ?
  for (int i = 0; i < 8; i++) {
    val = (int) input->readLong(1);
    if (val) f << val << ",";
    else f << "_,";
  }
  f << "],";

  // 3 other bdbox ?
  for (int bd = 0; bd < 3; bd++) {
    for (int i = 0; i < 4; i++) dim[i] = (float)input->readLong(2)/72.f;
    f << "bdbox" << 3+bd << "=" << Box2f(Vec2f(dim[0],dim[1]), Vec2f(dim[2], dim[3])) << ",";
  }

  f << "unk3=["; // always [_,_,_,_,_,_,_,_,_, ...]
  for (int i = 0; i < 16; i++) {
    val = (int) input->readLong(1);
    if (val) f << val << ",";
    else f << "_,";
  }
  f << "],";

  f << "unk4=["; // 1f,7929 or 2b,a3ca
  for (int i = 0; i < 2; i++) f << std::hex << input->readULong(2) << std::dec << ",";
  f << "],";
  for (int i = 0; i < 4; i++) dim[i] = (float) input->readLong(2)/1440.f;
  f << "bdbox6" <<"=" << Box2f(Vec2f(dim[0],dim[1]), Vec2f(dim[2], dim[3])) << ",";

  for (int bd = 0; bd < 3; bd++) {
    for (int i = 0; i < 4; i++) dim[i] = (float)input->readLong(2)/72.f;
    f << "bdboxII(" << bd << ")=" << Box2f(Vec2f(dim[0],dim[1]), Vec2f(dim[2], dim[3])) << ",";
    val = (int) input->readLong(2);
    if (val) {
      switch (bd) {
      case 0:
        f << "#pos=" << float(val)/72.f << ",";
        break;
      case 1:
        f << "mainYBegin=" << float(val)/72.f << ",";
        break;
      case 2:
        f << "mainYEnd=" << float(val)/72.f << ",";
        break;
      default:
        f << "##pos=" << val << ",";
      }
    }
    f << ",";
  }
  for (int bd = 0; bd < 3; bd++) {
    for (int i = 0; i < 4; i++) dim[i] = (float)input->readLong(2)/72.f;
    f << "bdboxIII(" << bd << ")=" << Box2f(Vec2f(dim[0],dim[1]), Vec2f(dim[2], dim[3])) <<",";
  }
  val = (int) input->readLong(2);
  if (val)  f << "#unkn1=" << val << ",";
  f << "pos(III)=" << (float)input->readLong(2)/72.f << ",";
  int numReportData2 = (int) input->readLong(2);
  if (numReportData2) f << "numReportData2=" << numReportData << ",";
  f << "unkn2=" << input->readLong(2) << ",";
  f << "id?="  << input->readLong(1) << ",";
  f << "unk5=[" << std::hex;
  for (int i = 0; i < 9; i++) {
    val = (int) input->readULong(1);
    if (val) f << val << ",";
    else f << "_,";
  }
  f << std::dec << "],";
  f << "unk6=["; // 1f,7929 or 2b,a3ca
  for (int i = 0; i < 2; i++) f << std::hex << input->readULong(2) << std::dec << ",";
  f << "],";
  // g1 = 1
  for (int i = 0; i < 12; i++) {
    val = (int) input->readULong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  input->seek(endPos, librevenge::RVNG_SEEK_SET);

  return true; // 2, 5
}

bool MsWksDBParser::readReportV2()
{
  if (version() > 2) return false;

  MWAWInputStreamPtr input=m_document->getInput();
  long pos = input->tell();
  libmwaw::DebugFile &ascFile = m_document->ascii();
  libmwaw::DebugStream f;

  int sz = (int) input->readLong(2);
  long endPos = pos+2+sz;
  if (sz < 490 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("MsWksDBParser::readReportV2: first zone is too short\n"));
    return false;
  }

  f << "Entries(DBReport):";
  for (int i = 0; i < 2; i++) {
    int val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos = input->tell();
  std::vector<int> colSize;
  if (!readColSize(colSize)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  pos = input->tell();
  f.str("");
  f << "DBReport(I):";
  for (int i = 0; i < 119; i++) {
    int val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos = input->tell();
  if (!m_document->readDocumentInfo(0x15e))
    input->seek(pos, librevenge::RVNG_SEEK_SET);

  pos = input->tell();
  f.str("");
  f << "DBReport(II):";
  // 003e0003000911556e7469746c6564205265706f727420310000000000000000.....
  // int, int, int, name ?
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
bool MsWksDBParser::sendDatabase()
{
  MWAWSpreadsheetListenerPtr listener=getSpreadsheetListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MsWksDBParser::sendDatabase: I can not find the listener\n"));
    return false;
  }
  MWAWInputStreamPtr input=m_document->getInput();
  MsWksDBParserInternal::DataBase const &database=m_state->m_database;
  std::vector<MsWksDBParserInternal::FieldType> const &fields = database.m_listFieldTypes;
  size_t numFields=fields.size();
  std::vector<std::vector<MWAWCellContent> > const &records=database.m_listRecords;
  size_t numRecords=records.size();
  listener->openSheet(database.convertInPoint(m_state->m_widthCols,76), librevenge::RVNG_POINT, "Sheet0");
  int height=12;
  for (size_t c=0; c<numFields; ++c)
    if (fields[c].m_height > height) height=fields[c].m_height;
  for (size_t r=0; r<numRecords; ++r) {
    std::vector<MWAWCellContent> const &row=records[r];
    listener->openSheetRow(height, librevenge::RVNG_POINT);
    for (size_t c=0; c<row.size(); ++c) {
      if (c>=numFields) break;
      MsWksDBParserInternal::FieldType field=fields[c];
      field.updateWithContent(Vec2i(int(c),int(r)), row[c]);
      if (field.empty()) continue;

      MWAWCellContent const &content=field.m_content;
      listener->openSheetCell(field, content);
      if (content.m_contentType==MWAWCellContent::C_TEXT && content.m_textEntry.valid()) {
        listener->setFont(field.getFont());
        input->seek(content.m_textEntry.begin(), librevenge::RVNG_SEEK_SET);
        while (!input->isEnd() && input->tell()<content.m_textEntry.end()) {
          unsigned char ch=(unsigned char) input->readULong(1);
          if (ch==0xd)
            listener->insertEOL();
          else
            listener->insertCharacter(ch);
        }
      }
      listener->closeSheetCell();
    }
    listener->closeSheetRow();
  }
  listener->closeSheet();
  return false;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////
// the formula
bool MsWksDBParser::readFormula()
{
  MWAWInputStreamPtr input=m_document->getInput();
  long pos = input->tell();
  int ptrSz = version() <= 2 ? 1 : 2;
  if (ptrSz == 2 && input->readLong(2) != 0) {
    MWAW_DEBUG_MSG(("MsWksDBParser::readFormula: size is odd\n"));
    return false;
  }

  libmwaw::DebugFile &ascFile = m_document->ascii();
  libmwaw::DebugStream f;
  f << "Entries(Formula):";

  long sz =(long) input->readULong(2);
  f << "sz=" << sz << ",";

  long endPos = pos+2*ptrSz+sz;
  if (!input->checkPosition(endPos)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MsWksDBParser::readFormula: Segment length is odd %ld\n", sz));
    return false;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  int numFields = m_state->m_database.m_numFields;
  std::vector<MsWksDBParserInternal::FieldType> &listFields = m_state->m_database.m_listFieldTypes;
  int numFieldsHeader = (int) listFields.size();

  if (numFieldsHeader+1 < numFields) {
    MWAW_DEBUG_MSG(("MsWksDBParser::readFormula: the number of fields header are to small\n"));
    if (version() > 2) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    listFields.resize(size_t(numFields)+1);
  }
  for (int nField = 0; nField < numFields; nField++) {
    pos = input->tell();
    if (pos+ptrSz == endPos) break;
    f.str("");

    f << "Formula-" << nField << ":";

    int fVal = (int) input->readLong(1);
    bool ok = false;
    if (fVal == -2) { // skip
      int skip = (int) input->readLong(1);
      if (skip > 0 && skip+nField < numFields) {
        nField+=skip-1;
        f << "skip=" << skip;
        ok = true;
      }
    }
    else if (fVal < 0);   // error
    else if (fVal == 0) ok = true;
    else {
      MsWksDBParserInternal::FieldType &field = listFields[size_t(nField)];
      std::string extra;
      if (!field.m_isSerial && field.m_content.m_contentType==MWAWCellContent::C_FORMULA &&
          m_document->readFormula(pos+1+fVal, field.m_content, extra))
        f << field << extra;
      else {
        MWAW_DEBUG_MSG(("MsWksDBParser::readFormula: can not read formula for field:%d\n", nField));
        f << field << "###";
      }
      input->seek(pos+1+fVal, librevenge::RVNG_SEEK_SET);
      ok = true;
    }
    if (!ok || input->tell() >= endPos) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      MWAW_DEBUG_MSG(("MsWksDBParser::readFormula: can not read defValue for field:%d\n", nField));
      return false;
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  ascFile.addDelimiter(input->tell(),'|');
  bool ok = input->readULong(1) == 255;
  if (ptrSz==2 && ok) ok = input->readULong(1) == 0;

  if (!ok || input->tell() != endPos) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MsWksDBParser::readFormula: End of Record is odd\n"));
    return false;
  }

  return true;
}

/** reads the serial formula */
bool  MsWksDBParser::readSerialFormula()
{
  MWAWInputStreamPtr input=m_document->getInput();

  long pos = input->tell();
  if (input->readLong(2) != 0) {
    MWAW_DEBUG_MSG(("MsWksDBParser::readSerialFormula: size is odd\n"));
    return false;
  }

  libmwaw::DebugFile &ascFile = m_document->ascii();
  libmwaw::DebugStream f;
  f << "Entries(Serial):";

  long sz =(long) input->readULong(2);
  f << "sz=" << sz << ",";

  if ((sz%30) || !input->checkPosition(pos+2+sz)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MsWksDBParser::readSerialFormula: Segment length is odd %ld\n", sz));
    return false;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  int nbSer = int(sz/30);
  std::vector<MsWksDBParserInternal::SerialFormula> listSerial;
  for (int ser = 0; ser < nbSer; ser++) {
    MsWksDBParserInternal::SerialFormula serial;

    std::string prefSuff[2];
    pos = input->tell();
    f.str("");
    f << "Serial-" << ser << ":";
    for (int i = 0; i < 2; i++) {
      sz = (long) input->readLong(1);
      if (sz < 0 || sz > 10) {
        MWAW_DEBUG_MSG(("MsWksDBParser::readSerialFormula: string size is too long:%ld\n", sz));
        return false;
      }
      prefSuff[i] = "";
      for (int l = 0; l < sz; l++)
        prefSuff[i] += (char) input->readULong(1);
      input->seek(pos+11*(i+1), librevenge::RVNG_SEEK_SET);
    }
    serial.m_prefix = prefSuff[0];
    serial.m_suffix = prefSuff[1];
    serial.m_increment = (long) input->readULong(2);
    // the initial value seems to 48 bytes,
    // but we can suppose that the high 32 bytes are
    // almost always 0...
    unsigned long orig = (input->readULong(2) << 16) << 16;
    orig += input->readULong(4);
    serial.m_nextValue = (long) orig;

    f << serial;
    listSerial.push_back(serial);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  // ok, we can now associated the serial to the data type
  std::vector<MsWksDBParserInternal::FieldType> &listFields = m_state->m_database.m_listFieldTypes;
  int numFieldsHeader = (int) listFields.size();
  for (int i = 0; i < numFieldsHeader; i++) {
    MsWksDBParserInternal::FieldType &field = listFields[size_t(i)];
    if (!field.m_isSerial) continue;
    if (field.m_serialId <= 0 || field.m_serialId > nbSer) {
      MWAW_DEBUG_MSG(("MsWksDBParser::readSerialFormula: can not find serial for field :%d\n", i));
      continue;
    }
    field.m_serialFormula = listSerial[size_t(field.m_serialId-1)];
  }

  return true;
}

// the default values
bool MsWksDBParser::readDefaultValues()
{
  MWAWInputStreamPtr input=m_document->getInput();

  long pos = input->tell();
  if (input->readLong(2) != 0) {
    MWAW_DEBUG_MSG(("MsWksDBParser::readDefaultValues: size is odd\n"));
    return false;
  }

  libmwaw::DebugFile &ascFile = m_document->ascii();
  libmwaw::DebugStream f;
  f << "Entries(DefValues):";

  long sz =(long) input->readULong(2);
  f << "sz=" << sz << ",";

  long endPos = pos+4+sz;
  if (!input->checkPosition(endPos)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MsWksDBParser::readDefaultValues: Segment length is odd %ld\n", sz));
    return false;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  int numFields = m_state->m_database.m_numFields;
  std::vector<MsWksDBParserInternal::FieldType> &listFields = m_state->m_database.m_listFieldTypes;

  if ((int) listFields.size()+1 < numFields) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MsWksDBParser::readDefaultValues: the number of fields header are to small\n"));
    return false;
  }
  for (int nField = 0; nField < numFields; nField++) {
    pos = input->tell();
    if (pos+2 == endPos) break;
    f.str("");

    f << "DefValues-" << nField << ":";

    int fVal = (int) input->readLong(1);
    bool ok = false;
    if (fVal == -2) { // skip
      int skip = (int) input->readLong(1);
      if (skip > 0 && skip+nField < numFields) {
        nField+=skip-1;
        f << "skip=" << skip;
        ok = true;
      }
    }
    else if (fVal < 0);   // error
    else if (fVal == 0) ok = true;
    else {
      MsWksDBParserInternal::FieldType &field = listFields[size_t(nField)];
      MWAWCell::Format const &format=field.getFormat();
      MWAWCellContent content=field.m_content;
      double value;
      bool isNan;
      std::string text("");
      if (format.m_format == MWAWCell::F_TEXT) {
        content.m_textEntry.setBegin(pos+1);
        content.m_textEntry.setLength(fVal);
        ok = m_document->readDBString(pos+1+fVal, text);
        if (!text.empty()) f << "\"" << text << "\",";
      }
      else if (m_document->readDBNumber(pos+1+fVal, value, isNan, text)) {
        field.m_content.setValue(value);
        f << value << ",";
        ok = true;
      }
      if (ok) {
        if (content.m_contentType == MWAWCellContent::C_UNKNOWN)
          content.m_contentType = format.m_format == MWAWCell::F_TEXT ?
                                  MWAWCellContent::C_TEXT : MWAWCellContent::C_NUMBER;
        f << field << "," << text;
      }
    }
    if (!ok || input->tell() >= endPos) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      MWAW_DEBUG_MSG(("MsWksDBParser::readDefaultValues: can not read defValue for field:%d\n", nField));
      return false;
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  ascFile.addDelimiter(input->tell(),'|');
  bool ok = input->readULong(1) == 255;
  if (ok) ok = input->readULong(1) == 0;

  if (!ok || input->tell() != endPos) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MsWksDBParser::readDefaultValues: End of Record is odd\n"));
    return false;
  }

  return true;
}

// reads the columns size
bool MsWksDBParser::readColSize(std::vector<int> &colSize)
{
  MWAWInputStreamPtr input=m_document->getInput();
  int const vers=version();
  long pos = input->tell();
  colSize.resize(0);

  libmwaw::DebugFile &ascFile = m_document->ascii();
  libmwaw::DebugStream f;
  f << "Entries(ColSize):";
  int numElt = vers<=2 ? 62 : 257;
  int prevPos = 0;
  for (int i = 0; i < numElt; i++) {
    int val = (int) input->readLong(2);
    int n = (int) input->readULong(2);
    if (prevPos >= 0 && val >= prevPos) {
      if (prevPos) colSize.push_back(val-prevPos);
      prevPos = val;
    }
    else
      prevPos = -1;
    f << val;
    if (n != i+1) f << "[#" << n << "]";
    f << ",";
  }

  int lastVal = (int) input->readLong(2);
  if ((vers > 2 && lastVal != -1) || (vers <= 2 && lastVal != 0)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MsWksDBParser::readColSize:  end of colSize is odd\n"));
    return false;
  }

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// The unknown structure
////////////////////////////////////////////////////////////
bool MsWksDBParser::readUnknownV2()
{
  if (version() > 2) return false;

  MWAWInputStreamPtr input=m_document->getInput();
  long pos = input->tell();
  libmwaw::DebugFile &ascFile = m_document->ascii();
  libmwaw::DebugStream f;

  if (!input->checkPosition(pos+0x114)) {
    MWAW_DEBUG_MSG(("MsWksDBParser::readUnknownV2: first zone is too short\n"));
    return false;
  }

  f << "Entries(DBUnknown):";

  for (int i = 0; i < 6; i++) {
    int val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  int numUnkn0 = (int) input->readLong(2);
  int numUnkn1 = (int) input->readLong(2);
  f << "unkn0=[" << numUnkn0 << "," << numUnkn1 << "],";

  for (int i = 0; i < 120; i++) {
    int val = (int) input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }

  for (int st = 0; st < 2; st++) {
    MWAWFont font;
    font.setId((int) input->readLong(2));
    font.setSize((float) input->readLong(2));
    f << "font" << st << "=[" << font.getDebugString(getParserState()->m_fontConverter);
    int val = (int) input->readLong(2);
    if (val) f << "#unk0=" << val << ",";
    val = (int) input->readLong(2);
    if (val) f << "#unk1=" << val << ",";
    f << "],";
  }

  // 2, 18|14
  f << "unkn1=[" << input->readLong(2) << ",";
  f << input->readLong(2) << "],";

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  if (numUnkn0 || numUnkn1) {
    // not sure which one is followed by a 0x76 zone
    pos = input->tell();
    f.str("");
    f << "DBUnknown-A:";
    if (numUnkn0!=1 || numUnkn1 !=1) {
      MWAW_DEBUG_MSG(("MsWksDBParser::readUnknownV2: Checkme, potential problem\n"));
      f << "####";
    }
    for (int i = 0; i < 59; i++) {
      int val = (int) input->readLong(2);
      if (val) f << "f" << i << "=" << val << ",";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MsWksDBParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MsWksDBParserInternal::State();
  if (!m_document || !m_document->checkHeader3(header, strict)) return false;
  if (m_document->getKind() != MWAWDocument::MWAW_K_DATABASE)
    return false;
#ifndef DEBUG
  if (version() == 1)
    return false;
#endif
  return true;
}

////////////////////////////////////////////////////////////
// formula data
////////////////////////////////////////////////////////////

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
