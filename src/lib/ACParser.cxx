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
#include "MWAWList.hxx"
#include "MWAWParagraph.hxx"
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
//! Internal: class used to store a list type in ACParser
struct Label {
  //! constructor
  Label(int type=-1) : m_type(type) { }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Label const &lbl) {
    switch(lbl.m_type) {
    case 0:
      o << "noLabel,";
      break;
    case 2:
      o << "checkbox,";
      break;
    case 0xb:
      o << "decimal,"; // 1.0,
      break;
    case 0xc:
      o << "I A...,";
      break;
    case 0xe:
      o << "custom,";
      break;
    default:
      o << "#labelType=" << lbl.m_type << ",";
      break;
    }
    return o;
  }
  //! operator==
  bool operator==(Label const &lbl) const {
    return m_type==lbl.m_type;
  }
  //! operator=!
  bool operator!=(Label const &lbl) const {
    return !operator==(lbl);
  }
  //! the label type
  int m_type;
};

////////////////////////////////////////
//! Internal: class used to store the printing preferences in ACParser
struct Printing {
  //! constructor
  Printing() : m_font() {
    for (int i = 0; i < 2; i++)
      m_flags[i]=0;
  }
  //! returns true if the header is empty
  bool isEmpty() const {
    return (m_flags[1]&7)==0;
  }
  //! operator==
  bool operator==(Printing const &print) const {
    if (m_font != print.m_font)
      return false;
    for (int i=0; i<2; i++) {
      if (m_flags[i]!=print.m_flags[i])
        return false;
    }
    return true;
  }
  //! operator=!
  bool operator!=(Printing const &print) const {
    return !operator==(print);
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Printing const &print) {
    if (print.m_flags[0]==1)
      o << "useFooter,";
    else if (print.m_flags[0])
      o << "#fl0=" << print.m_flags[0] << ",";
    int flag = print.m_flags[1];
    if (flag&1)
      o << "title,";
    if (flag&2)
      o << "date,";
    if (flag&4)
      o << "pagenumber,";
    flag &= 0xFFF8;
    if (flag)
      o << "#flags=" << std::hex << flag << std::dec << ",";
    return o;
  }
  //! the font
  MWAWFont m_font;
  //! the flags
  int m_flags[2];
};

////////////////////////////////////////
//! Internal: class used to store the optional preferences in ACParser
struct Option {
  //! constructor
  Option(int flags=0) : m_flags(flags) { }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Option const &opt) {
    int flag = opt.m_flags;
    if (flag&0x1000)
      o << "speaker[dial],";
    if (flag&0x4)
      o << "smart['\"],";
    if (flag&0x8)
      o << "open[startup],";
    // flag&0x10: always ?
    if (flag&0x20)
      o << "nolabel[picture],";
    if (flag&0x40)
      o << "noframe[current],";
    if (flag&0x80)
      o << "nolabel[clipboard],";

    flag &= 0xEF13;
    if (flag) // find also flag&(10|400|4000|8000)
      o << "option[flags]=" << std::hex << flag << std::dec << ",";
    return o;
  }
  //! the flags
  int m_flags;
};

////////////////////////////////////////
//! Internal: the state of a ACParser
struct State {
  //! constructor
  State() : m_printerPreferences(), m_title(""), m_label(), m_stringLabel(""), m_eof(-1), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0) {
  }

  //! the printer preferences
  Printing m_printerPreferences;
  //! the title (if defined)
  std::string m_title;
  //! the list type
  Label m_label;
  //! the custom label (if defined)
  std::string m_stringLabel;
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
  SubDocument(ACParser &pars, MWAWInputStreamPtr input) :
    MWAWSubDocument(&pars, input, MWAWEntry()) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const {
    if (MWAWSubDocument::operator!=(doc)) return true;
    SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
    if (!sDoc) return true;
    return false;
  }

  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("ACParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_parser);
  static_cast<ACParser *>(m_parser)->sendHeaderFooter();
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

////////////////////////////////////////////////////////////
// interface with the text parser
////////////////////////////////////////////////////////////

shared_ptr<MWAWList> ACParser::getMainList()
{
  MWAWListLevel level;
  level.m_labelAfterSpace=0.05;
  std::vector<MWAWListLevel> levels;
  switch (m_state->m_label.m_type) {
  case 0: // none
    level.m_type=MWAWListLevel::NONE;
    levels.resize(10, level);
    break;
  case 2: // checkbox
    level.m_type=MWAWListLevel::BULLET;
    libmwaw::appendUnicode(0x2610, level.m_bullet);
    levels.resize(10, level);
    break;
  case 0xb: // 1.0 1.1 1.1.1 1.1.1.1 1.1.1.1.1 ...
    level.m_suffix = ".";
    level.m_type=MWAWListLevel::DECIMAL;
    for (int i=0; i < 10; i++) {
      level.m_numBeforeLabels=i;
      levels.push_back(level);
    }
    break;
  case 0xc: // I. A. 1. a. i. [(1). (a). ]*
    level.m_suffix = ".";
    level.m_type=MWAWListLevel::UPPER_ROMAN;
    levels.push_back(level);
    level.m_type=MWAWListLevel::UPPER_ALPHA;
    levels.push_back(level);
    level.m_type=MWAWListLevel::DECIMAL;
    levels.push_back(level);
    level.m_type=MWAWListLevel::LOWER_ALPHA;
    levels.push_back(level);
    level.m_type=MWAWListLevel::LOWER_ROMAN;
    levels.push_back(level);
    level.m_prefix = "(";
    level.m_suffix = ").";
    for (int i=0; i < 4; i++) {
      level.m_type=MWAWListLevel::DECIMAL;
      levels.push_back(level);
      level.m_type=MWAWListLevel::LOWER_ALPHA;
      levels.push_back(level);
    }
    break;
  default: // ok, switch to custom or by default bullet
  case 0xe: {//custom
    level.m_type=MWAWListLevel::BULLET;
    libmwaw::appendUnicode(0x2022, level.m_bullet);
    MWAWFontConverterPtr fontConvert=getFontConverter();
    if (!fontConvert) {
      MWAW_DEBUG_MSG(("ACParser::getMainList: can not find the listener\n"));
    } else {
      for (size_t c= 0; c < m_state->m_stringLabel.size(); c++) {
        int unicode=fontConvert->unicode(3, (unsigned char) m_state->m_stringLabel[1]);
        level.m_bullet="";
        libmwaw::appendUnicode((unicode > 0) ? uint32_t(unicode):0x2022, level.m_bullet);
        levels.push_back(level);
      }
    }
    while (levels.size() < 10)
      levels.push_back(level);
    break;
  }
  }
  shared_ptr<MWAWList> list;
  MWAWListManagerPtr listManager=getParserState()->m_listManager;
  if (!listManager) {
    MWAW_DEBUG_MSG(("ACParser::getMainList: can not find the list manager\n"));
    return list;
  }

  for (size_t s=0; s < levels.size(); s++) {
    list = listManager->getNewList(list, int(s+1), levels[s]);
    if (!list) break;
  }
  return list;
}
////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
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
  if (!m_state->m_printerPreferences.isEmpty()) {
    MWAWHeaderFooter hF(m_state->m_printerPreferences.m_flags[0]!=1 ?
                        MWAWHeaderFooter::HEADER : MWAWHeaderFooter::FOOTER,
                        MWAWHeaderFooter::ALL);
    hF.m_subDocument.reset(new ACParserInternal::SubDocument(*this, getInput()));
    ps.setHeaderFooter(hF);
  }
  std::vector<MWAWPageSpan> pageList(1,ps);
  MWAWContentListenerPtr listen(new MWAWContentListener(*getParserState(), pageList, documentInterface));
  setListener(listen);
  listen->startDocument();
}

void ACParser::sendHeaderFooter()
{
  MWAWContentListenerPtr listener=getListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("ACParser::sendHeaderFooter: can not find the listener\n"));
    return;
  }
  ACParserInternal::Printing const &print=m_state->m_printerPreferences;
  MWAWParagraph para;
  para.m_justify=MWAWParagraph::JustificationCenter;
  listener->setParagraph(para);
  listener->setFont(print.m_font);
  bool printDone=false;
  for (int i=0, wh=1; i < 3; i++, wh*=2) {
    if ((print.m_flags[1]&wh)==0)
      continue;
    if (printDone)
      listener->insertChar(' ');
    switch(i) {
    case 0:
      if (!m_state->m_title.length()) {
        listener->insertField(MWAWField::Title);
        break;
      }
      for (size_t s=0; s < m_state->m_title.length(); s++)
        listener->insertCharacter((unsigned char)m_state->m_title[s]);
      break;
    case 1: {
      MWAWField field(MWAWField::Date);
      field.m_DTFormat="%b %d, %Y";
      listener->insertField(field);
      break;
    }
    case 2:
      listener->insertField(MWAWField::PageNumber);
      break;
    default:
      MWAW_DEBUG_MSG(("ACParser::sendHeaderFooter: unexpected step\n"));
      break;
    }
    printDone=true;
  }
  if (!printDone)
    listener->insertChar(' ');
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool ACParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  readRSRCZones();
  if (version()>=3) {
    input->setReadInverted(true);
    if (!readEndDataV3()) {
      ascii().addPos(input->tell());
      ascii().addNote("Entries(Loose)");
    }
    input->setReadInverted(false);
  }
  return m_textParser->createZones();
}

bool ACParser::readRSRCZones()
{
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  if (!rsrcParser)
    return true;
  if (version() < 3) { // never seens so, better ignore
    MWAW_DEBUG_MSG(("ACParser::readRSRCZones: find a resource fork in v1-v2!!!\n"));
    return false;
  }


  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;

  // STR:0 -> title name, STR:1 custom label
  it = entryMap.lower_bound("STR ");
  while (it != entryMap.end()) {
    if (it->first != "STR ")
      break;
    MWAWEntry const &entry = it++->second;
    entry.setParsed(true);
    std::string str("");
    if (!rsrcParser->parseSTR(entry,str) || str.length()==0)
      continue;
    switch(entry.id()) {
    case 0:
      m_state->m_title=str;
      break;
    case 1:
      m_state->m_stringLabel=str;
      break;
    default:
      MWAW_DEBUG_MSG(("ACParser::readRSRCZones: find unexpected STR:%d\n", entry.id()));
      break;
    }
  }
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
        readHFProperties(entry);
        break;
      default:
        break;
      }
    }
  }
  return true;
}

bool ACParser::readEndDataV3()
{
  if (version()<3)
    return true;
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  input->seek(m_state->m_eof-8,WPX_SEEK_SET);
  long pos=(long) input->readULong(4);
  if (pos < 18 || !isFilePos(pos)) {
    MWAW_DEBUG_MSG(("ACParser::readEndDataV3: oops begin of ressource is bad\n"));
    ascii().addPos(m_state->m_eof-8);
    ascii().addNote("###");
    return false;
  }
  ascii().addPos(m_state->m_eof-8);
  ascii().addNote("_");

  input->seek(pos, WPX_SEEK_SET);

  f << "Entries(QOpt):";
  ACParserInternal::Label lbl((int) input->readLong(1));
  f << lbl << ",";
  if (m_state->m_label!=lbl) {
    if (m_state->m_label.m_type) {
      MWAW_DEBUG_MSG(("ACParser::readEndDataV3: oops the label seems set and different\n"));
    } else
      m_state->m_label = lbl;
  }
  int val=(int) input->readLong(1);
  if (val != 1) // always 1
    f << "f0=" << val << ",";
  ACParserInternal::Option opt((int) input->readULong(2));
  f << opt;
  // string: name, followed by abbreviation
  for (int i = 0; i < 2; i++) {
    long fPos=input->tell();
    int fSz = (int) input->readULong(1);
    if (!isFilePos(fPos+fSz+1)) {
      MWAW_DEBUG_MSG(("ACText::createZones: can not read following string\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return true;
    }
    if (!fSz) continue;
    std::string str("");
    for (int s=0; s < fSz; s++)
      str+=(char) input->readULong(1);
    if (!str.length())
      continue;
    f << "str" << i << "=" << str << ",";
    std::string &which=(i==0) ? m_state->m_title : m_state->m_stringLabel;
    if (which.length()) {
      if (which != str) {
        MWAW_DEBUG_MSG(("ACText::createZones: find a different string\n"));
        f << "###";
      }
      continue;
    }
    which = str;
  }
  val=(int) input->readLong(1);
  if (val) // always 0 or a another string
    f << "f1=" << val << ",";

  // from here unknown: maybe related to the printer definition...
  float dim[2];
  for (int i = 0; i < 2; i++) // very unsure...
    dim[i] = float(input->readULong(2))/256.f;
  f << "dim?=" << dim[1] << "x" << dim[0] << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = input->tell();
  f.str("");
  f << "Entries(Loose):";
  int type = (int) input->readULong(1);
  f << "type?=" << std::hex << type << ",";
  ascii().addPos(pos);
  ascii().addNote("Entries(Loose)");

  if (type!=255 || !isFilePos(pos+200))
    return true;

  input->seek(pos+198, WPX_SEEK_SET);
  pos = input->tell();
  int N=(int) input->readLong(2);
  if (N<0 || !isFilePos(pos+2+34*N))
    return true;
  f.str("");
  f << "Entries(Font):N=" << N << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i= 0; i < N; i++) {
    pos=input->tell();
    f.str("");
    f << "Font-" << i << ":";
    std::string str("");
    while (input->tell()<pos+32) {
      char c=(char) input->readULong(1);
      if (!c) break;
      str+=c;
    }
    if (str.length())
      f << str << ",";
    input->seek(pos+32, WPX_SEEK_SET);
    for (int j = 0; j < 2; j++) {
      val = (int) input->readLong(1);
      if (val) f << "f" << j << "=" << std::hex << val << std::dec << ",";
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  pos = input->tell();
  ascii().addPos(pos);
  ascii().addNote("Entries(Loose)[II]");
  return true;
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
  m_state->m_label.m_type=(int) input->readLong(2);
  f << m_state->m_label;
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

// header/footer properties
bool ACParser::readHFProperties(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() != 20) {
    MWAW_DEBUG_MSG(("ACParser::readHFProperties: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  f << "Entries(QHDR):";
  entry.setParsed(true);
  for (int st = 0; st < 2; st++) {
    if (st==0)
      f << "headerFooter=[";
    else
      f << "unknown=[";
    ACParserInternal::Printing print;
    print.m_font.setId((int) input->readLong(2));
    print.m_font.setSize((float) input->readLong(2));
    int flag=(int) input->readLong(2);
    uint32_t flags = 0;
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) print.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0x8) flags |= MWAWFont::embossBit;
    if (flag&0x10) flags |= MWAWFont::shadowBit;
    print.m_font.setFlags(flags);
#ifdef DEBUG
    f << "font=[" << print.m_font.getDebugString(getFontConverter()) << "],";
#endif
    flag &= 0xE0;
    if (flag)
      f << "#font[flags]=" << std::hex << flags << std::dec << ",";
    for (int i=0; i < 2; i++)
      print.m_flags[i] = (int) input->readULong(2);
    f << print << "],";
    if (st==0)
      m_state->m_printerPreferences = print;
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
  ACParserInternal::Option opt((int) input->readULong(2));
  f << opt;
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
  m_state->m_eof = input->tell();
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
  val = (int) input->readULong(2); // depth ( first topic must have depth=1)
  if (val != 1)
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
