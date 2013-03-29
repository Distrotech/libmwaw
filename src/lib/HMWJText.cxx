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
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWDebug.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "HMWJParser.hxx"

#include "HMWJText.hxx"

/** Internal: the structures of a HMWJText */
namespace HMWJTextInternal
{
/** different PLC types */
enum PLCType { TYPE1=0, TYPE2, TYPE3, TOKEN, Unknown};
/** Internal and low level: the PLC different types and their structures of a HMWJText */
struct PLC {
  //! constructor
  PLC(PLCType w= Unknown, int id=0) : m_type(w), m_id(id), m_extra("") {}
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, PLC const &plc) {
    switch(plc.m_type) {
    case TYPE1:
      o << "PA" << plc.m_id << ",";
      break;
    case TYPE2:
      o << "PB" << plc.m_id << ",";
      break;
    case TYPE3:
      o << "PC" << plc.m_id << ",";
      break;
    case TOKEN:
      o << "T" << plc.m_id << ",";
      break;
    case Unknown:
    default:
      o << "#unknown" << plc.m_id << ",";
    }
    o << plc.m_extra;
    return o;
  }
  //! PLC type
  PLCType m_type;
  //! the indentificator
  int m_id;
  //! extra data
  std::string m_extra;
};

/** Internal: class to store a token of a HMWJText */
struct Token {
  //! constructor
  Token() : m_type(0), m_id(0), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Token const &tk) {
    switch(tk.m_type) { // checkme
    case 0:
      break;
    case 1:
      o << "field,";
      break;
    case 2:
      o << "toc,";
      break;
    case 0x20:
      o << "bookmark,";
      break;
    default:
      o << "#type=" << tk.m_type << ",";
      break;
    }
    if (tk.m_id)
      o << "id=" << std::hex << tk.m_id << std::dec << ",";
    o << tk.m_extra;
    return o;
  }
  //! the token type
  int m_type;
  //! the id ( to be send)
  long m_id;
  //! extra string string
  std::string m_extra;
};

//! Internal: a struct used to store a text zone
struct TextZone {
  TextZone() : m_entry(), m_plcMap(), m_tokenList(), m_parsed(false) {
  }

  //! the main entry
  MWAWEntry m_entry;

  //! the plc map
  std::multimap<long, PLC> m_plcMap;
  //! the tokens list
  std::vector<Token> m_tokenList;

  //! true if the zone is sended
  bool m_parsed;
};


/** Internal: class to store the paragraph properties of a HMWJText */
struct Paragraph : public MWAWParagraph {
  //! Constructor
  Paragraph() : MWAWParagraph(), m_type(0), m_addPageBreak(false) {
  }
  //! destructor
  ~Paragraph() {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Paragraph const &ind) {
    switch(ind.m_type) {
    case 0:
      break;
    case 1:
      o << "header,";
      break;
    case 2:
      o << "footer,";
      break;
    case 5:
      o << "footnote,";
      break;
    default:
      o << "#type=" << ind.m_type << ",";
      break;
    }
    o << reinterpret_cast<MWAWParagraph const &>(ind) << ",";
    if (ind.m_addPageBreak) o << "pageBreakBef,";
    return o;
  }
  //! the type
  int m_type;
  //! flag to store a force page break
  bool m_addPageBreak;
};

////////////////////////////////////////
//! Internal: the state of a HMWJText
struct State {
  //! constructor
  State() : m_version(-1), m_textZoneList(), m_numPages(-1), m_actualPage(0) {
  }
  //! the file version
  mutable int m_version;
  //! the list of text zone
  std::vector<TextZone> m_textZoneList;
  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
};

////////////////////////////////////////
//! Internal: the subdocument of a HMWJText
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(HMWJText &pars, MWAWInputStreamPtr input, int id, libmwaw::SubDocumentType type) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_textParser(&pars), m_id(id), m_type(type) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  /** the text parser */
  HMWJText *m_textParser;
  //! the subdocument id
  int m_id;
  //! the subdocument type
  libmwaw::SubDocumentType m_type;
private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_textParser);

  long pos = m_input->tell();
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_textParser != sDoc->m_textParser) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_type != sDoc->m_type) return true;
  return false;
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
HMWJText::HMWJText(HMWJParser &parser) :
  m_parserState(parser.getParserState()), m_state(new HMWJTextInternal::State), m_mainParser(&parser)
{
}

HMWJText::~HMWJText()
{
}

int HMWJText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int HMWJText::numPages() const
{
  return m_state->m_numPages;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//     Text
////////////////////////////////////////////////////////////
bool HMWJText::sendText(long /*id*/, long /*subId*/)
{
  return false;
}

bool HMWJText::sendText(HMWJTextInternal::TextZone const &zone)
{
  if (!zone.m_entry.valid()) {
    MWAW_DEBUG_MSG(("HMWJText::sendText: call without entry\n"));
    return false;
  }
  WPXBinaryData data;
  if (!m_mainParser->decodeZone(zone.m_entry, data)) {
    MWAW_DEBUG_MSG(("HMWJText::sendText: can not decode a zone\n"));
    m_parserState->m_asciiFile.addPos(zone.m_entry.begin());
    m_parserState->m_asciiFile.addNote("###");
    return false;
  }
  if (!data.size())
    return true;

  WPXInputStream *dataInput = const_cast<WPXInputStream *>(data.getDataStream());
  if (!dataInput) {
    MWAW_DEBUG_MSG(("HMWJText::sendText: can not find my input\n"));
    return false;
  }

  MWAWInputStreamPtr input(new MWAWInputStream(dataInput, false));
  libmwaw::DebugFile asciiFile;

#ifdef DEBUG_WITH_FILES
  static int tId=0;
  std::stringstream s;
  s << "Text" << tId++;
  asciiFile.setStream(input);
  asciiFile.open(s.str().c_str());
  s << ".data";
  libmwaw::Debug::dumpFile(data, s.str().c_str());
#endif

  asciiFile.addPos(0);
  asciiFile.addNote("Entries(TextData)");
  return true;
}

bool HMWJText::readTextZone(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("HMWJText::readTextZone: called without any entry\n"));
    return false;
  }
  if (entry.length() < 8+20*3) {
    MWAW_DEBUG_MSG(("HMWJText::readTextZone: the entry seems too short\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  entry.setParsed(true);

  long pos = entry.begin()+8; // skip header
  long endPos = entry.end();
  input->seek(pos, WPX_SEEK_SET);

  HMWJTextInternal::TextZone zone;
  long val;
  HMWJTextInternal::PLC plc;
  // probably a list of 3 plc: char?, ruler?, ?
  for (int i = 0; i < 3; i++) {
    f.str("");
    f << entry.name() << "-" << i << ":";

    pos = input->tell();
    int expectedSz = i<2 ? 8 : 4;

    HMWJZoneHeader header(false);
    if (!m_mainParser->readClassicHeader(header,endPos) || header.m_fieldSize!=expectedSz) {
      MWAW_DEBUG_MSG(("HMWJText::readTextZone: can not read zone %d\n", i));
      f << "###";
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      input->seek(pos, WPX_SEEK_SET);
      break;
    }
    f << header;
    long zoneEnd=pos+4+header.m_length;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());

    if (i==0)
      plc.m_type = HMWJTextInternal::TYPE1;
    else if (i==1)
      plc.m_type = HMWJTextInternal::TYPE2;
    else if (i==2)
      plc.m_type = HMWJTextInternal::TYPE3;
    for (int j = 0; j < header.m_n; j++) {
      pos = input->tell();
      f.str("");
      f << entry.name() << "-" << i << "[" << j << "]:";
      long fPos = input->readLong(2);
      if (fPos) f << "fPos=" << std::hex << fPos << std::dec << ",";
      for (int k = 0; k < 3; k++) {
        val = input->readLong(2);
        if (val) f << "f" << k << "=" << val << ",";
        if (4+2*k >= header.m_fieldSize) break;
      }
      plc.m_id = j;
      zone.m_plcMap.insert
      (std::multimap<long, HMWJTextInternal::PLC>::value_type(fPos, plc));
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      input->seek(pos+header.m_fieldSize, WPX_SEEK_SET);
    }

    if (input->tell() != zoneEnd) { // junk ?
      asciiFile.addDelimiter(input->tell(),'|');
      input->seek(zoneEnd, WPX_SEEK_SET);
    }
  }

  asciiFile.addPos(endPos);
  asciiFile.addNote("_");
  // now potentially a token zone, called with endPos-1 to avoid reading the last text zone
  readTextToken(endPos-1, zone);

  pos = input->tell();
  if (pos==endPos) {
    MWAW_DEBUG_MSG(("HMWJText::readTextZone: can not read find the last zone\n"));

    return true;
  }

  // normally now the text data but check if there remains some intermediar unparsed zone
  long dataSz = (long) input->readULong(4);
  while (dataSz>0 && pos+4+dataSz < endPos) {
    MWAW_DEBUG_MSG(("HMWJText::readTextZone: find some unparsed zone\n"));
    f.str("");
    f << entry.name() << "-###:";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());

    pos = pos+4+dataSz;
    input->seek(pos, WPX_SEEK_SET);
    dataSz = (long) input->readULong(4);
  }
  input->seek(pos, WPX_SEEK_SET);

  // the text data
  f.str("");
  f << entry.name() << "-text:";
  dataSz = (long) input->readULong(4);
  if (pos+4+dataSz>endPos) {
    MWAW_DEBUG_MSG(("HMWJText::readTextZone: can not read last zone size\n"));
    f << "###sz=" << dataSz;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    return false;
  }

  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  zone.m_entry.setBegin(pos);
  zone.m_entry.setEnd(endPos);
  zone.m_entry.setName(entry.name());
  m_state->m_textZoneList.push_back(zone);

  // REMOVEME
  sendText(zone);
  return true;
}

bool HMWJText::readTextToken(long endPos, HMWJTextInternal::TextZone &zone)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  long pos=input->tell();
  if (pos+4>=endPos)
    return true;

  f << "Entries(TextToken):";
  HMWJZoneHeader header(false);
  if (!m_mainParser->readClassicHeader(header,endPos) || header.m_fieldSize!=16) {
    input->seek(pos, WPX_SEEK_SET);
    return true;
  }
  f << header;
  long zoneEnd=pos+4+header.m_length;
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  int val;
  HMWJTextInternal::PLC plc(HMWJTextInternal::TOKEN);
  for (int i = 0; i < header.m_n; i++) {
    pos = input->tell();
    f.str("");
    HMWJTextInternal::Token tkn;
    tkn.m_type=(int)input->readLong(1);
    for (int j=0; j < 2; j++) { // f0=0, f1=0|1|11
      val = (int) input->readLong(1);
      if (val) f << "f" << j << "=" << val << ",";
    }
    val = (int) input->readLong(1);
    if (val) f << "id?=" << val << ",";
    long fPos = input->readLong(4);

    for (int j=0; j < 2; j++) { // g0=0, g1=0|2|5|a|10
      val = (int) input->readLong(2);
      if (val) f << "g" << j << "=" << val << ",";
    }
    tkn.m_id = (long)  input->readULong(4);
    tkn.m_extra = f.str();
    zone.m_tokenList.push_back(tkn);
    plc.m_id = i;
    zone.m_plcMap.insert(std::multimap<long, HMWJTextInternal::PLC>::value_type(fPos, plc));
    f.str("");
    f << "TextToken-" << i << ":";
    if (fPos) f << "fPos=" << std::hex << fPos << std::dec << ",";
    f << tkn;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+16, WPX_SEEK_SET);
  }

  if (input->tell() != zoneEnd) {
    asciiFile.addDelimiter(input->tell(),'|');
    input->seek(zoneEnd, WPX_SEEK_SET);
  }

  pos = input->tell();
  // next: we can find potential bookmark zone
  int i = 0;
  while (!input->atEOS()) {
    pos = input->tell();
    long dataSz = (long) input->readULong(4);
    zoneEnd = pos+4+dataSz;
    if (dataSz<0 || zoneEnd >= endPos)
      break;

    f.str("");
    f << "TextToken-data" << i++ << ":";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    int fSz=(int) input->readULong(1);
    if (fSz == dataSz-2 || fSz==dataSz-1) {
      std::string bkmark("");
      for (int c=0; c < fSz; c++)
        bkmark+=(char) input->readULong(1);
      f << bkmark;
    } else
      f << "###";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());

    input->seek(zoneEnd, WPX_SEEK_SET);
  }
  input->seek(pos, WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//     Fonts
////////////////////////////////////////////////////////////

// a single font
bool HMWJText::readFont(MWAWFont &font, long endPos)
{
  font = MWAWFont(-1,-1);

  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell(), debPos=pos;
  if (endPos <= 0) {
    long dataSz=(long) input->readULong(4);
    pos+=4;
    endPos=pos+dataSz;
    if (!m_mainParser->isFilePos(endPos)) {
      MWAW_DEBUG_MSG(("HMWJText::readFont: pb reading font size\n"));
      input->seek(debPos, WPX_SEEK_SET);
      return false;
    }
  }
  long len=endPos-pos;
  if (len < 24) {
    MWAW_DEBUG_MSG(("HMWJText::readFont: the zone is too short\n"));
    input->seek(debPos, WPX_SEEK_SET);
    return false;
  }

  libmwaw::DebugStream f;
  font.setId((int) input->readLong(2));
  int val = (int) input->readLong(2);
  if (val) f << "#f1=" << val << ",";
  font.setSize(float(input->readLong(4))/65536.f);
  float expand = float(input->readLong(4))/65536.f;
  if (expand < 0 || expand > 0)
    font.setDeltaLetterSpacing(expand*font.size());
  float xScale = float(input->readLong(4))/65536.f;
  if (xScale < 1.0 || xScale > 1.0)
    font.setTexteWidthScaling(xScale);

  int flag =(int) input->readULong(2);
  uint32_t flags=0;
  if (flag&1) {
    font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.setUnderlineType(MWAWFont::Line::Double);
  }
  if (flag&2)
    font.setUnderlineStyle(MWAWFont::Line::Dot);
  if (flag&4) {
    font.setUnderlineStyle(MWAWFont::Line::Dot);
    font.setUnderlineWidth(2.0);
  }
  if (flag&8)
    font.setUnderlineStyle(MWAWFont::Line::Dash);
  if (flag&0x10)
    font.setStrikeOutStyle(MWAWFont::Line::Simple);
  if (flag&0x20) {
    font.setStrikeOutStyle(MWAWFont::Line::Simple);
    font.setStrikeOutType(MWAWFont::Line::Double);
  }
  if (flag&0xFFC0)
    f << "#flag0=" << std::hex << (flag&0xFFF2) << std::dec << ",";
  flag =(int) input->readULong(2);
  if (flag&1) flags |= MWAWFont::boldBit;
  if (flag&0x2) flags |= MWAWFont::italicBit;
  if (flag&0x4) flags |= MWAWFont::outlineBit;
  if (flag&0x8) flags |= MWAWFont::shadowBit;
  if (flag&0x10) flags |= MWAWFont::reverseVideoBit;
  if (flag&0x20) font.set(MWAWFont::Script::super100());
  if (flag&0x40) font.set(MWAWFont::Script::sub100());
  if (flag&0x80) {
    if (flag&0x20)
      font.set(MWAWFont::Script(48,WPX_PERCENT,58));
    else if (flag&0x40)
      font.set(MWAWFont::Script(16,WPX_PERCENT,58));
    else
      font.set(MWAWFont::Script::super());
  }
  if (flag&0x100) {
    font.setOverlineStyle(MWAWFont::Line::Dot);
    font.setOverlineWidth(2.0);
  }
  if (flag&0x200) flags |= MWAWFont::boxedBit;
  if (flag&0x400) flags |= MWAWFont::boxedRoundedBit;
  if (flag&0x800) {
    font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.setUnderlineWidth(0.5);
  }
  if (flag&0x1000) font.setUnderlineStyle(MWAWFont::Line::Simple);
  if (flag&0x2000) {
    font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.setUnderlineWidth(2.0);
  }
  if (flag&0x4000) {
    font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.setUnderlineWidth(3.0);
  }
  if (flag&0x8000) {
    font.setStrikeOutStyle(MWAWFont::Line::Simple);
    font.setStrikeOutType(MWAWFont::Line::Double);
    font.setUnderlineWidth(0.5);
  }
  int color = (int) input->readLong(2);
  MWAWColor col;
  if (color && m_mainParser->getColor(color, 1, col))
    font.setColor(col);
  else if (color)
    f << "##fColor=" << color << ",";
  val = (int) input->readLong(2);
  if (val) f << "#unk=" << val << ",";
  if (len >= 28) {
    color = (int) input->readLong(2);
    int pattern = (int) input->readLong(2);
    if ((color || pattern) && m_mainParser->getColor(color, pattern, col))
      font.setBackgroundColor(col);
    else if (color || pattern)
      f << "#backColor=" << color << ", #pattern=" << pattern << ",";
  }
  if (input->tell() != endPos)
    m_parserState->m_asciiFile.addDelimiter(input->tell(),'|');
  font.setFlags(flags);
  font.m_extra = f.str();

  input->seek(endPos, WPX_SEEK_SET);
  return true;
}

bool HMWJText::readFonts(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("HMWJText::readFonts: called without any entry\n"));
    return false;
  }
  if (entry.length() <= 8) {
    MWAW_DEBUG_MSG(("HMWJText::readFonts: the entry seems too short\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  entry.setParsed(true);

  long pos = entry.begin()+8; // skip header
  long endPos = entry.end();
  input->seek(pos, WPX_SEEK_SET);

  // first read the header
  f << entry.name() << "[header]:";
  HMWJZoneHeader mainHeader(false);
  if (!m_mainParser->readClassicHeader(mainHeader,endPos) || mainHeader.m_fieldSize!=8) {
    MWAW_DEBUG_MSG(("HMWJText::readFonts: can not read the header\n"));
    f << "###sz=" << mainHeader.m_length;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    return false;
  }
  long headerEnd=pos+4+mainHeader.m_length;
  f << mainHeader;
  long val;
  f << "unk=[";
  for (int i = 0; i < mainHeader.m_n; i++) {
    f << "[";
    val = input->readLong(2); // always -2 ?
    if (val!=-2)
      f << val << ",";
    else
      f << "_,";
    val = (long) input->readULong(2); // 0 or 5020 : junk?
    if (val)
      f << std::hex << val << std::dec << ",";
    else
      f << "_,";
    val = (long) input->readULong(4); // id
    f << std::hex << val << std::dec;
    f << "]";
  }
  f << "],";
  if (input->tell()!=headerEnd) {
    asciiFile.addDelimiter(input->tell(),'|');
    input->seek(headerEnd, WPX_SEEK_SET);
  }
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  for (int i = 0; i < mainHeader.m_n; i++) {
    pos = input->tell();
    f.str("");
    f << entry.name() << "-" << i << ":";
    MWAWFont font(-1,-1);
    if (!readFont(font) || input->tell() > endPos) {
      MWAW_DEBUG_MSG(("HMWJText::readFonts: can not read font %d\n", i));
      f << "###";
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      return false;
    }
    f << font.getDebugString(m_parserState->m_fontConverter) << ",";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
  }
  asciiFile.addPos(endPos);
  asciiFile.addNote("_");
  return true;
}

// the list of fonts
bool HMWJText::readFontNames(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("HMWJText::readFontNames: called without any entry\n"));
    return false;
  }
  if (entry.length() < 28) {
    MWAW_DEBUG_MSG(("HMWJText::readFontNames: the entry seems too short\n"));
    return false;
  }
  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  entry.setParsed(true);

  f << entry.name() << "[data]:";

  long pos = entry.begin()+8; // skip header
  input->seek(pos, WPX_SEEK_SET);
  int N, val;
  long readDataSz = (long) input->readULong(4);
  if (readDataSz+12 != entry.length()) {
    MWAW_DEBUG_MSG(("HMWJText::readFontNames: the data size seems odd\n"));
    f << "##dataSz=" << readDataSz << ",";
  }
  N = (int) input->readLong(2);
  f << "N=" << N << ",";
  long fieldSz =  (long) input->readULong(4);
  if (fieldSz != 68) {
    MWAW_DEBUG_MSG(("HMWJText::readFontNames: the field size seems odd\n"));
    f << "##fieldSz=" << fieldSz << ",";
  }
  for (int i = 0; i < 3; i++) { //f1=f2=1
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  long id = (long) input->readULong(4);
  if (id) f << "id=" << std::hex << id << std::dec << ",";

  long expectedSz = N*68+28;
  if (expectedSz != entry.length() && expectedSz+1 != entry.length()) {
    MWAW_DEBUG_MSG(("HMWJText::readFontNames: the entry size seems odd\n"));
    return false;
  }
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << entry.name() << "-" << i << ":";
    int fId = (int) input->readLong(2);
    f << "fId=" << fId << ",";
    val = (int) input->readLong(2);
    if (val != fId)
      f << "#fId2=" << val << ",";
    int fSz = (int) input->readULong(1);
    if (fSz+5 > 68) {
      f << "###fSz";
      MWAW_DEBUG_MSG(("HMWJText::readFontNames: can not read a font\n"));
    } else {
      std::string name("");
      for (int c = 0; c < fSz; c++)
        name += (char) input->readULong(1);
      f << name;
      m_parserState->m_fontConverter->setCorrespondance(fId, name);
    }
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+68, WPX_SEEK_SET);
  }
  asciiFile.addPos(entry.end());
  asciiFile.addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
//     Style
////////////////////////////////////////////////////////////
bool HMWJText::readStyles(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("HMWJText::readStyles: called without any zone\n"));
    return false;
  }

  long dataSz = entry.length();
  if (dataSz < 4) {
    MWAW_DEBUG_MSG(("HMWJText::readStyles: the zone seems too short\n"));
    return false;
  }
  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  entry.setParsed(true);

  f << entry.name() << "[header]:";

  long pos = entry.begin()+8; // skip header
  long endPos = entry.end();
  input->seek(pos, WPX_SEEK_SET);

  HMWJZoneHeader mainHeader(false);
  if (!m_mainParser->readClassicHeader(mainHeader,endPos) || mainHeader.m_fieldSize!=4) {
    MWAW_DEBUG_MSG(("HMWJText::readStyles: can not read the header\n"));
    f << "###sz=" << mainHeader.m_length;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    return false;
  }
  long headerEnd=pos+4+mainHeader.m_length;
  f << mainHeader;

  f << "listIds=[" << std::hex;
  for (int i = 0; i < mainHeader.m_n; i++)
    f << input->readULong(4) << ",";
  f << std::dec << "],";
  input->seek(headerEnd, WPX_SEEK_SET);
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  int val;
  for (int i = 0; i < mainHeader.m_n; i++) {
    f.str("");
    f << entry.name() << "-" << i << ":";
    pos = input->tell();
    long fieldSz = (long) input->readULong(4)+4;
    if (fieldSz < 0x1bc || pos+fieldSz > endPos) {
      f << "###";
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("HMWJText::readStyles: can not read field %d\n", i));
      return true;
    }
    val = (int) input->readULong(1);
    if (val != i) f << "#id=" << val << ",";

    // f0=c2|c6, f2=0|44, f3=1|14|15|16: fontId?
    for (int j=0; j < 5; j++) {
      val = (int) input->readULong(1);
      if (val)
        f << "f" << j << "=" << std::hex << val << std::dec << ",";
    }
    /* g1=9|a|c|e|12|18: size ?, g5=1, g8=0|1, g25=0|18|30|48, g31=1, g35=0|1 */
    for (int j=0; j < 33; j++) {
      val = (int) input->readULong(2);
      if (val)
        f << "g" << j << "=" << val  << ",";
    }
    for (int j=0; j < 4; j++) { // b,b,b,0
      val = (int) input->readULong(1);
      if ((j < 3 && val != 0xb) || (j==3 && val))
        f << "h" << j << "=" << val  << ",";
    }

    for (int j=0; j < 17; j++) { // always 0
      val = (int) input->readULong(2);
      if (val)
        f << "l" << j << "=" << val  << ",";
    }
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    long pos2 = input->tell();
    f.str("");
    f << entry.name() << "-" << i << "[B]:";
    // checkme probably f15=numTabs  ..
    for (int j = 0; j < 50; j++) {
      val = (int) input->readULong(2);
      if ((j < 5 && val != 1) || (j >= 5 && val))
        f << "f" << j << "=" << val  << ",";
    }
    for (int j = 0; j < 50; j++) {
      val = (int) input->readULong(2);
      if (val)
        f << "g" << j << "=" << val  << ",";
    }
    for (int j = 0; j < 43; j++) {
      val = (int) input->readULong(2);
      if (val)
        f << "h" << j << "=" << val  << ",";
    }
    asciiFile.addPos(pos2);
    asciiFile.addNote(f.str().c_str());

    pos2 = input->tell();
    f.str("");
    f << entry.name() << "-" << i << "[C]:";
    val = (int) input->readLong(2);
    if (val != -1) f << "unkn=" << val << ",";
    val  = (int) input->readLong(2);
    if (val != i) f << "#id" << val << ",";
    for (int j = 0; j < 4; j++) {
      val = (int) input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    int fSz = (int) input->readULong(1);
    if (input->tell()+fSz > pos+fieldSz) {
      MWAW_DEBUG_MSG(("HMWJText::readStyles: can not read styleName\n"));
      f << "###";
    } else {
      std::string name("");
      for (int j = 0; j < fSz; j++)
        name +=(char) input->readULong(1);
      f << name;
    }
    asciiFile.addPos(pos2);
    asciiFile.addNote(f.str().c_str());
    if (input->tell() != pos+fieldSz)
      asciiFile.addDelimiter(input->tell(),'|');

    input->seek(pos+fieldSz, WPX_SEEK_SET);
  }

  if (!input->atEOS()) {
    asciiFile.addPos(input->tell());
    asciiFile.addNote("_");
  }
  return true;
}

////////////////////////////////////////////////////////////
//     Paragraph
////////////////////////////////////////////////////////////
bool HMWJText::readParagraph(HMWJTextInternal::Paragraph &para, long endPos)
{
  para = HMWJTextInternal::Paragraph();

  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell(), debPos=pos;
  if (endPos <= 0) {
    long dataSz=(long) input->readULong(4);
    pos+=4;
    endPos=pos+dataSz;
    if (!m_mainParser->isFilePos(endPos)) {
      MWAW_DEBUG_MSG(("HMWJText::readParagraph: pb reading para size\n"));
      input->seek(debPos, WPX_SEEK_SET);
      return false;
    }
  }
  long len=endPos-pos;
  if (len < 102) {
    MWAW_DEBUG_MSG(("HMWJText::readParagraph: the zone is too short\n"));
    input->seek(debPos, WPX_SEEK_SET);
    return false;
  }

  libmwaw::DebugStream f;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  int flags = (int) input->readULong(1);
  if (flags&0x80)
    para.m_breakStatus = para.m_breakStatus.get()|MWAWParagraph::NoBreakWithNextBit;
  if (flags&0x40)
    para.m_breakStatus = para.m_breakStatus.get()|MWAWParagraph::NoBreakBit;
  if (flags&0x2)
    para.m_addPageBreak = true;
  if (flags&0x4)
    f << "linebreakByWord,";

  if (flags & 0x39) f << "#fl=" << std::hex << (flags&0x39) << std::dec << ",";

  int val = (int) input->readLong(2);
  if (val) f << "#f0=" << val << ",";
  val = (int) input->readULong(2);
  switch(val&3) {
  case 0:
    para.m_justify = MWAWParagraph::JustificationLeft;
    break;
  case 1:
    para.m_justify = MWAWParagraph::JustificationRight;
    break;
  case 2:
    para.m_justify = MWAWParagraph::JustificationCenter;
    break;
  case 3:
    para.m_justify = MWAWParagraph::JustificationFull;
    break;
  default:
    break;
  }
  if (val&0xFFFC) f << "#f1=" << val << ",";
  val = (int) input->readLong(1);
  if (val) f << "#f2=" << val << ",";
  para.m_type = (int) input->readLong(2);

  float dim[3];
  for (int i = 0; i < 3; i++)
    dim[i] = float(input->readLong(4))/65536.0f;
  para.m_marginsUnit = WPX_POINT;
  para.m_margins[0]=dim[0]+dim[1];
  para.m_margins[1]=dim[0];
  para.m_margins[2]=dim[2]; // ie. distance to rigth border - ...

  for (int i = 0; i < 3; i++)
    para.m_spacings[i] = float(input->readLong(4))/65536.0f;
  int spacingsUnit[3]; // 1=mm, ..., 4=pt, b=line
  for (int i = 0; i < 3; i++)
    spacingsUnit[i] = (int) input->readULong(1);
  if (spacingsUnit[0]==0xb)
    para.m_spacingsInterlineUnit = WPX_PERCENT;
  else
    para.m_spacingsInterlineUnit = WPX_POINT;
  for (int i = 1; i < 3; i++) // convert point|line -> inches
    para.m_spacings[i]= ((spacingsUnit[i]==0xb) ? 12.0 : 1.0)*(para.m_spacings[i].get())/72.0;

  val = (int) input->readLong(1);
  if (val) f << "#f3=" << val << ",";
  for (int i = 0;  i< 2; i++) { // one time f4=8000
    val = (int) input->readULong(2);
    if (val) f << "#f" << i+4 << "=" << std::hex << val << std::dec << ",";
  }
  // the borders
  char const *(wh[5]) = { "T", "L", "B", "R", "VSep" };
  MWAWBorder borders[5];
  for (int d=0; d < 5; d++)
    borders[d].m_width = float(input->readLong(4))/65536.f;
  for (int d=0; d < 5; d++) {
    val = (int) input->readULong(1);
    switch (val) {
    case 0: // normal
      break;
    case 1:
      borders[d].m_type = MWAWBorder::Double;
      break;
    case 2:
      borders[d].m_type = MWAWBorder::Double;
      f << "bord" << wh[d] << "[ext=2],";
      break;
    case 3:
      borders[d].m_type = MWAWBorder::Double;
      f << "bord" << wh[d] << "[int=2],";
      break;
    default:
      f << "#bord" << wh[d] << "[style=" << val << "],";
      break;
    }
  }
  int color[5], pattern[5];
  for (int d=0; d < 5; d++)
    color[d] = (int) input->readULong(1);
  for (int d=0; d < 5; d++)
    pattern[d] = (int) input->readULong(2);
  for (int d=0; d < 5; d++) {
    if (!color[d] && !pattern[d])
      continue;
    MWAWColor col;
    if (m_mainParser->getColor(color[d], pattern[d], col))
      borders[d].m_color = col;
    else
      f << "#bord" << wh[d] << "[col=" << color[d] << ",pat=" << pattern[d] << "],";
  }
  // update the paragraph
  para.m_borders.resize(6);
  MWAWBorder::Pos const (which[5]) = {
    MWAWBorder::Top, MWAWBorder::Left, MWAWBorder::Bottom, MWAWBorder::Right,
    MWAWBorder::VMiddle
  };
  for (int d=0; d < 5; d++) {
    if (borders[d].m_width <= 0)
      continue;
    para.m_borders[which[d]]=borders[d];
  }
  val = (int) input->readLong(1);
  if (val) f << "#f6=" << val << ",";
  double bMargins[5]= {0,0,0,0,0};
  for (int d = 0; d < 5; d++) {
    bMargins[d] =  double(input->readLong(4))/256./65536./72.;
    if (bMargins[d] > 0 || bMargins[d] < 0)
      f << "bordMarg" << wh[d] << "=" << bMargins[d] << ",";
  }
  int nTabs = (int) input->readULong(1);
  if (input->tell()+2+nTabs*12 > endPos) {
    MWAW_DEBUG_MSG(("HMWJText::readParagraph: can not read numbers of tab\n"));
    input->seek(debPos, WPX_SEEK_SET);
    return false;
  }
  val = (int) input->readULong(2);
  if (val) f << "#h3=" << val << ",";
  para.m_extra=f.str();
  f.str("");
  f << "Ruler:" << para;

  asciiFile.addPos(debPos);
  asciiFile.addNote(f.str().c_str());
  for (int i = 0; i < nTabs; i++) {
    pos = input->tell();
    f.str("");
    f << "Ruler[Tabs-" << i << "]:";

    MWAWTabStop tab;
    val = (int)input->readULong(1);
    switch(val) {
    case 0:
      break;
    case 1:
      tab.m_alignment = MWAWTabStop::CENTER;
      break;
    case 2:
      tab.m_alignment = MWAWTabStop::RIGHT;
      break;
    case 3:
      tab.m_alignment = MWAWTabStop::DECIMAL;
      break;
    case 4:
      tab.m_alignment = MWAWTabStop::BAR;
      break;
    default:
      f << "#type=" << val << ",";
      break;
    }
    val = (int)input->readULong(1);
    if (val) f << "barType=" << val << ",";
    val = (int)input->readULong(2);
    if (val) {
      int unicode= m_parserState->m_fontConverter->unicode(3, (unsigned char) val);
      if (unicode==-1)
        tab.m_decimalCharacter = uint16_t(val);
      else
        tab.m_decimalCharacter = uint16_t(unicode);
    }
    val = (int) input->readULong(2);
    if (val) {
      int unicode= m_parserState->m_fontConverter->unicode(3, (unsigned char) val);
      if (unicode==-1)
        tab.m_leaderCharacter = uint16_t(val);
      else
        tab.m_leaderCharacter = uint16_t(unicode);
    }
    val = (int)input->readULong(2); // 0|73|74|a044|f170|f1e0|f590
    if (val) f << "f0=" << std::hex << val << std::dec << ",";

    tab.m_position = float(input->readLong(4))/65536.f/72.f;
    para.m_tabs->push_back(tab);
    f << tab;

    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+12, WPX_SEEK_SET);
  }
  if (input->tell()!=endPos) {
    asciiFile.addDelimiter(input->tell(),'|');
    input->seek(endPos, WPX_SEEK_SET);
  }
  return true;
}

bool HMWJText::readParagraphs(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("HMWJText::readParagraphs: called without any entry\n"));
    return false;
  }
  if (entry.length() <= 8) {
    MWAW_DEBUG_MSG(("HMWJText::readParagraphs: the entry seems too short\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  entry.setParsed(true);

  long pos = entry.begin()+8; // skip header
  long endPos = entry.end();
  input->seek(pos, WPX_SEEK_SET);

  // first read the header
  f << entry.name() << "[header]:";
  HMWJZoneHeader mainHeader(false);
  if (!m_mainParser->readClassicHeader(mainHeader,endPos) || mainHeader.m_fieldSize!=12) {
    MWAW_DEBUG_MSG(("HMWJText::readParagraphs: can not read the header\n"));
    f << "###sz=" << mainHeader.m_length;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    return false;
  }
  long headerEnd=pos+4+mainHeader.m_length;
  f << mainHeader;

  long val;
  f << "unk=[";
  for (int i = 0; i < mainHeader.m_n; i++) {
    f << "[";
    val = input->readLong(2); // always -2 ?
    if (val!=-2)
      f << "unkn0=" << val << ",";
    val = (long) input->readULong(2); // 0|1|2|5
    if (val)
      f << "type=" << val << ",";
    val = (long) input->readULong(4); // a id
    if (val)
      f << "id1=" << std::hex << val << std::dec << ",";
    val = (long) input->readULong(4);
    if (val)
      f << "id2=" << std::hex << val << std::dec << ",";
    f << "]";
  }
  f << "],";
  if (input->tell()!=headerEnd) {
    asciiFile.addDelimiter(input->tell(),'|');
    input->seek(headerEnd, WPX_SEEK_SET);
  }
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  for (int i = 0; i < mainHeader.m_n; i++) {
    pos = input->tell();
    HMWJTextInternal::Paragraph paragraph;
    if (!readParagraph(paragraph) || input->tell() > endPos) {
      MWAW_DEBUG_MSG(("HMWJText::readParagraphs: can not read paragraph %d\n", i));
      asciiFile.addPos(pos);
      asciiFile.addNote("Ruler###");
      return false;
    }
  }
  asciiFile.addPos(endPos);
  asciiFile.addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
//     the token
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//     the sections
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

//! send data to the listener

void HMWJText::flushExtra()
{
  if (!m_parserState->m_listener) return;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
