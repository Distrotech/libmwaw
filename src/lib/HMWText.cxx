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

#include "HMWParser.hxx"

#include "HMWText.hxx"

/** Internal: the structures of a HMWText */
namespace HMWTextInternal
{
/** Internal: the fonts of a HMWText*/
struct Font {
  //! the constructor
  Font(): m_font(-1,-1), m_extra("") { }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Font const &font);

  //! the font
  MWAWFont m_font;
  //! extra data
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, Font const &font)
{
  if (font.m_extra.length())
    o << font.m_extra << ",";
  return o;
}

/** Internal: class to store the paragraph properties of a HMWText */
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

/** Internal: class to store the token properties of a HMWText */
struct Token {
  //! Constructor
  Token() : m_type(0), m_id(-1), m_extra("") {
  }
  //! destructor
  ~Token() {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Token const &tkn) {
    o << "type=" << tkn.m_type << ",";
    o << "id=" << std::hex << tkn.m_id << std::dec << ",";
    o << tkn.m_extra;
    return o;
  }
  //! the type
  int m_type;
  //! the identificator
  long m_id;
  //! extra data, mainly for debugging
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the state of a HMWText
struct State {
  //! constructor
  State() : m_version(-1), m_fileType(-1), m_IdTextMaps(), m_numPages(-1), m_actualPage(0) {
  }

  //! the file version
  mutable int m_version;
  //! int to know if this is a Korean file (-1: unknown, 0: Japanese, 1: Korean)
  mutable int m_fileType;
  //! the map of id -> text zone
  std::multimap<long, shared_ptr<HMWZone> > m_IdTextMaps;
  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
};

////////////////////////////////////////
//! Internal: the subdocument of a HMWText
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(HMWText &pars, MWAWInputStreamPtr input, int id, libmwaw::SubDocumentType type) :
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
  HMWText *m_textParser;
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
  HMWContentListener *listen = dynamic_cast<HMWContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
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
HMWText::HMWText(MWAWInputStreamPtr ip, HMWParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert),
  m_state(new HMWTextInternal::State), m_mainParser(&parser)
{
}

HMWText::~HMWText()
{
}

int HMWText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_mainParser->version();
  return m_state->m_version;
}

bool HMWText::isKoreanFile() const
{
  if (m_state->m_fileType < 0)
    m_state->m_fileType = m_mainParser->isKoreanFile() ? 1 : 0;
  return m_state->m_fileType;
}

int HMWText::numPages() const
{
  return m_state->m_numPages;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//     Text
////////////////////////////////////////////////////////////
bool HMWText::readTextZone(shared_ptr<HMWZone> zone)
{
  if (!zone || !zone->valid()) {
    MWAW_DEBUG_MSG(("HMWText::readTextZone: called without any zone\n"));
    return false;
  }
  m_state->m_IdTextMaps.insert
  (std::multimap<long, shared_ptr<HMWZone> >::value_type(zone->m_id, zone));
  long dataSz = zone->length();
  MWAWInputStreamPtr input = zone->m_input;
  input->seek(zone->begin(), WPX_SEEK_SET);

  int actPage = 1, actCol = 0, numCol=1;
  long val, pos;
  while (!input->atEOS()) {
    pos = input->tell();
    val = (long) input->readULong(1);
    if (val == 0 && input->atEOS()) break;
    if (val != 1 || input->readLong(1) != 0)
      break;
    int type = (int) input->readLong(2);
    bool done=false;;
    switch(type) {
    case 2: { // ruler
      HMWTextInternal::Paragraph para;
      done=readParagraph(*zone,para);
      if (para.m_addPageBreak)
        actPage++;
      break;
    }
    default:
      break;
    }

    if (!done) {
      input->seek(pos+4, WPX_SEEK_SET);
      long sz = (long) input->readULong(2);
      if (pos+6+sz > dataSz)
        break;
      input->seek(pos+6+sz, WPX_SEEK_SET);
    }

    bool ok=true;
    while (!input->atEOS()) {
      int c=(int) input->readLong(2);
      if (c==0x100) {
        input->seek(-2, WPX_SEEK_CUR);
        break;
      }
      if (c==0 && input->atEOS())
        break;
      if (c==0) {
        ok = false;
        break;
      }
      if (c==2) {
        if (actCol < numCol-1 && numCol > 1)
          actCol++;
        else {
          actCol = 0;
          actPage++;
        }
      } else if (c==3)
        actPage++;
    }
    if (!ok)
      break;
  }
  if (actPage > m_state->m_numPages)
    m_state->m_numPages = actPage;
  return true;
}

bool HMWText::sendText(long id, long subId)
{
  std::multimap<long, shared_ptr<HMWZone> >::iterator tIt
    =m_state->m_IdTextMaps.lower_bound(id);
  if (tIt == m_state->m_IdTextMaps.end() || tIt->first != id) {
    MWAW_DEBUG_MSG(("HMWText::sendText: can not find the text zone\n"));
    return false;
  }
  while (tIt != m_state->m_IdTextMaps.end() && tIt->first == id) {
    shared_ptr<HMWZone> zone = (tIt++)->second;
    if (!zone || zone->m_subId != subId) continue;
    sendText(*zone);
    return true;
  }
  MWAW_DEBUG_MSG(("HMWText::sendText: can not find the text zone\n"));
  return false;
}

bool HMWText::sendText(HMWZone &zone)
{
  if (!zone.valid()) {
    MWAW_DEBUG_MSG(("HMWText::sendText: called without any zone\n"));
    return false;
  }
  if (!m_listener) {
    MWAW_DEBUG_MSG(("HMWText::sendText: can not find a listener\n"));
    return false;
  }

  long dataSz = zone.length();
  MWAWInputStreamPtr input = zone.m_input;
  libmwaw::DebugFile &asciiFile = zone.ascii();
  libmwaw::DebugStream f;
  zone.m_parsed = true;

  long pos = zone.begin();
  input->seek(pos, WPX_SEEK_SET);
  f << "PTR=" << std::hex << zone.fileBeginPos() << std::dec << ",";
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  MWAWFont actFont;
  int actPage = 1, actCol = 0, numCol=1, actSection = 1;
  float width = float(72.0*m_mainParser->pageWidth());

  long val;
  while (!input->atEOS()) {
    pos = input->tell();
    f.str("");
    f << zone.name()<< ":";
    val = (long) input->readULong(1);
    if (val == 0 && input->atEOS()) break;
    if (val != 1 || input->readLong(1) != 0) {
      f << "###";
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      break;
    }
    int type = (int) input->readLong(2);
    bool done=false;;
    switch(type) {
    case 1: {
      f << "font,";
      HMWTextInternal::Font font;
      if (!readFont(zone,font))
        break;
      done = true;
      actFont = font.m_font;
      setProperty(actFont);
      break;
    }
    case 2: { // ruler
      f << "ruler,";
      HMWTextInternal::Paragraph para;
      if (!readParagraph(zone,para))
        break;
      if (para.m_addPageBreak) {
        m_mainParser->newPage(actPage++);
        actCol = 0;
      }
      setProperty(para, width);
      done=true;
      break;
    }
    case 3: { // footnote, object?
      f << "token,";
      HMWTextInternal::Token token;
      done=readToken(zone,token);
      break;
    }
    case 4: // follow by id?
      actSection++;
      f << "section,";
      break;
    case 6: // follow by id?
      f << "toc,";
      break;
    case 7:
      f << "bookmark,";
      break;
    case 9: // follow by id?
      f << "rubi,";
      break;
    case 11: // follow by id?
      f << "endToc,";
      break;
    case 12: // follow by id?
      f << "endBookmark,";
      break;
    case 14:
      f << "endRubi,";
      break;
    default:
      break;
    }

    bool ok=true;
    if (!done) {
      input->seek(pos+4, WPX_SEEK_SET);
      long sz = (long) input->readULong(2);
      ok = pos+6+sz <= dataSz;
      if (!ok)
        f << "###";
      else
        input->seek(pos+6+sz, WPX_SEEK_SET);

      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
    }
    if (!ok) {
      MWAW_DEBUG_MSG(("HMWText::sendText: can not read a zone\n"));
      break;
    }

    pos = input->tell();
    f.str("");
    f << zone.name()<< ":text,";
    while (!input->atEOS()) {
      int c=(int) input->readULong(2);
      if (c==0x100) {
        input->seek(-2, WPX_SEEK_CUR);
        break;
      }
      if (c==0 && input->atEOS())
        break;
      if (c==0) {
        ok = false;
        f << "###";
        break;
      }
      switch(c) {
      case 0x1000:
        f << "[pgNum]";
        m_listener->insertField(MWAWContentListener::PageNumber);
        break;
      case 0x1001:
        f << "[pgCount]";
        m_listener->insertField(MWAWContentListener::PageCount);
        break;
      case 0x1002:
        f << "[date]";
        m_listener->insertField(MWAWContentListener::Date);
        break;
      case 0x1003:
        f << "[time]";
        m_listener->insertField(MWAWContentListener::Time);
        break;
      case 0x1004:
        f << "[title]";
        m_listener->insertField(MWAWContentListener::Title);
        break;
      case 0x1005: {
        std::stringstream s;
        f << "[section]";
        s << actSection;
        m_listener->insertUnicodeString(s.str().c_str());
        break;
      }
      case 2:
        f << "[colBreak]";
        if (actCol < numCol-1 && numCol > 1) {
          m_listener->insertBreak(MWAW_COLUMN_BREAK);
          actCol++;
        } else {
          actCol = 0;
          m_mainParser->newPage(actPage++);
        }
        break;
      case 3:
        f << "[pageBreak]";
        m_mainParser->newPage(actPage++);
        break;
      case 9:
        f << char(c);
        m_listener->insertTab();
        break;
      case 0xd:
        f << char(c);
        m_listener->insertEOL();
        break;
      default: {
        if (c <= 0x1f || c >= 0x100) {
          f << "###" << std::hex << c << std::dec;
          MWAW_DEBUG_MSG(("HMWText::sendText: find a odd char %x\n", c));
          break;
        }
        f << char(c);
        int unicode = m_convertissor->unicode(actFont.id(), (unsigned char) c);
        if (unicode != -1)
          m_listener->insertUnicode((uint32_t) unicode);
        else
          m_listener->insertCharacter((unsigned char) c);
        break;
      }
      }
    }
    if (input->tell() != pos) {
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
    }
    if (!ok) {
      MWAW_DEBUG_MSG(("HMWText::sendText: can not read a text zone\n"));
      break;
    }
  }
  // FIXME: remove this when the frame/textbox are sent normally
  m_listener->insertEOL();
  return true;
}

////////////////////////////////////////////////////////////
//     Fonts
////////////////////////////////////////////////////////////

// send the font to the listener
void HMWText::setProperty(MWAWFont const &font)
{
  if (!m_listener) return;
  MWAWFont ft;
  font.sendTo(m_listener.get(), ft);
}

// a font in the text zone
bool HMWText::readFont(HMWZone &zone, HMWTextInternal::Font &font)
{
  font = HMWTextInternal::Font();

  MWAWInputStreamPtr input = zone.m_input;
  long pos = input->tell();
  if (pos+30 > zone.length()) {
    MWAW_DEBUG_MSG(("HMWText::readFont: the zone is too short\n"));
    return false;
  }

  libmwaw::DebugStream f;
  int val = (int) input->readLong(2);
  if (val!=28) {
    MWAW_DEBUG_MSG(("HMWText::readFont: the data size seems bad\n"));
    f << "##dataSz=" << val << ",";
  }
  font.m_font.setId((int) input->readLong(2));
  val = (int) input->readLong(2);
  if (val) f << "#f1=" << val << ",";
  font.m_font.setSize(float(input->readLong(4))/65536.f);
  float expand = float(input->readLong(4))/65536.f;
  if (expand < 0 || expand > 0)
    font.m_font.setDeltaLetterSpacing(expand*font.m_font.size());
  float xScale = float(input->readLong(4))/65536.f;
  if (xScale < 1.0 || xScale > 1.0)
    font.m_font.setTexteWidthScaling(xScale);

  int flag =(int) input->readULong(2);
  uint32_t flags=0;
  if (flag&1) {
    font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.m_font.setUnderlineType(MWAWFont::Line::Double);
  }
  if (flag&2)
    font.m_font.setUnderlineStyle(MWAWFont::Line::Dot);
  if (flag&4) {
    font.m_font.setUnderlineStyle(MWAWFont::Line::Dot);
    font.m_font.setUnderlineWidth(2.0);
  }
  if (flag&8)
    font.m_font.setUnderlineStyle(MWAWFont::Line::Dash);
  if (flag&0x10)
    font.m_font.setStrikeOutStyle(MWAWFont::Line::Simple);
  if (flag&0x20) {
    font.m_font.setStrikeOutStyle(MWAWFont::Line::Simple);
    font.m_font.setStrikeOutType(MWAWFont::Line::Double);
  }
  if (flag&0xFFC0)
    f << "#flag0=" << std::hex << (flag&0xFFF2) << std::dec << ",";
  flag =(int) input->readULong(2);
  if (flag&1) flags |= MWAWFont::boldBit;
  if (flag&0x2) flags |= MWAWFont::italicBit;
  if (flag&0x4) flags |= MWAWFont::outlineBit;
  if (flag&0x8) flags |= MWAWFont::shadowBit;
  if (flag&0x10) flags |= MWAWFont::reverseVideoBit;
  if (flag&0x20) font.m_font.set(MWAWFont::Script::super100());
  if (flag&0x40) font.m_font.set(MWAWFont::Script::sub100());
  if (flag&0x80) {
    if (flag&0x20)
      font.m_font.set(MWAWFont::Script(48,WPX_PERCENT,58));
    else if (flag&0x40)
      font.m_font.set(MWAWFont::Script(16,WPX_PERCENT,58));
    else
      font.m_font.set(MWAWFont::Script::super());
  }
  if (flag&0x100) {
    font.m_font.setOverlineStyle(MWAWFont::Line::Dot);
    font.m_font.setOverlineWidth(2.0);
  }
  if (flag&0x200) flags |= MWAWFont::boxedBit;
  if (flag&0x400) flags |= MWAWFont::boxedRoundedBit;
  if (flag&0x800) {
    font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.m_font.setUnderlineWidth(0.5);
  }
  if (flag&0x1000) font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
  if (flag&0x2000) {
    font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.m_font.setUnderlineWidth(2.0);
  }
  if (flag&0x4000) {
    font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.m_font.setUnderlineWidth(3.0);
  }
  if (flag&0x8000) {
    font.m_font.setStrikeOutStyle(MWAWFont::Line::Simple);
    font.m_font.setStrikeOutType(MWAWFont::Line::Double);
    font.m_font.setUnderlineWidth(0.5);
  }
  int color = (int) input->readLong(2);
  MWAWColor col;
  if (color && m_mainParser->getColor(color, 1, col))
    font.m_font.setColor(col);
  else if (color)
    f << "##fColor=" << color << ",";
  val = (int) input->readLong(2);
  if (val) f << "#unk=" << val << ",";
  color = (int) input->readLong(2);
  int pattern = (int) input->readLong(2);
  if ((color || pattern) && m_mainParser->getColor(color, pattern, col))
    font.m_font.setBackgroundColor(col);
  else if (color || pattern)
    f << "#backColor=" << color << ", #pattern=" << pattern << ",";
  font.m_font.setFlags(flags);
  font.m_extra = f.str();

  static bool first=true;
  f.str("");
  if (first) {
    f << "Entries(FontDef):";
    first = false;
  } else
    f << "FontDef:";
  f << font.m_font.getDebugString(m_convertissor) << font << ",";

  zone.ascii().addPos(pos-4);
  zone.ascii().addNote(f.str().c_str());

  input->seek(pos+30, WPX_SEEK_SET);
  return true;
}

// the list of fonts
bool HMWText::readFontNames(shared_ptr<HMWZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWText::readFontNames: called without any zone\n"));
    return false;
  }
  long dataSz = zone->length();
  bool isKorean = isKoreanFile();
  long minimalSz = isKorean ? 2 : 28;
  if (dataSz < minimalSz) {
    MWAW_DEBUG_MSG(("HMWText::readFontNames: the zone seems too short\n"));
    return false;
  }
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->ascii();
  libmwaw::DebugStream f;
  zone->m_parsed = true;

  if (isKorean)
    f << zone->name() << ":PTR=" << std::hex << zone->fileBeginPos() << std::dec << ",";
  else
    f << zone->name() << "[data]:";

  long pos = zone->begin() + (isKorean ? 0 : 8);
  input->seek(pos, WPX_SEEK_SET);
  int N, val;
  if (!isKorean) {
    long readDataSz = (long) input->readULong(4);
    if (readDataSz+12 != dataSz) {
      MWAW_DEBUG_MSG(("HMWText::readFontNames: the data size seems odd\n"));
      f << "##dataSz=" << readDataSz << ",";
    }
    N = (int) input->readLong(2);
    f << "N=" << N << ",";
    long fieldSz =  (long) input->readULong(4);
    if (fieldSz != 68) {
      MWAW_DEBUG_MSG(("HMWText::readFontNames: the field size seems odd\n"));
      f << "##fieldSz=" << fieldSz << ",";
    }
    for (int i = 0; i < 5; i++) { //f1=f2=1
      val = (int) input->readLong(2);
      if (val) f << "f" << i << "=" << val << ",";
    }
  } else {
    N = (int) input->readLong(2);
    f << "N=" << N << ",";
  }
  long expectedSz = N*68+minimalSz;
  if (expectedSz != dataSz && expectedSz+1 != dataSz) {
    MWAW_DEBUG_MSG(("HMWText::readFontNames: the zone size seems odd\n"));
    return false;
  }
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << zone->name() << "-" << i << ":";
    int fId = (int) input->readLong(2);
    f << "fId=" << fId << ",";
    val = (int) input->readLong(2);
    if (val != fId)
      f << "#fId2=" << val << ",";
    int fSz = (int) input->readULong(1);
    if (fSz+5 > 68) {
      f << "###fSz";
      MWAW_DEBUG_MSG(("HMWText::readFontNames: can not read a font\n"));
    } else {
      std::string name("");
      for (int c = 0; c < fSz; c++)
        name += (char) input->readULong(1);
      f << name;
      m_convertissor->setCorrespondance(fId, name);
    }
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+68, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
//     Style
////////////////////////////////////////////////////////////
bool HMWText::readStyles(shared_ptr<HMWZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWText::readStyles: called without any zone\n"));
    return false;
  }

  bool isKorean = isKoreanFile();
  long minimalSz = isKorean ? 24 : 4;
  long dataSz = zone->length();
  if (dataSz < minimalSz) {
    MWAW_DEBUG_MSG(("HMWText::readStyles: the zone seems too short\n"));
    return false;
  }
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->ascii();
  libmwaw::DebugStream f;
  zone->m_parsed = true;

  if (isKorean)
    f << zone->name() << ":PTR=" << std::hex << zone->fileBeginPos() << std::dec << ",";
  else
    f << zone->name() << "[header]:";

  long pos = zone->begin() + (isKorean ? 0 : 8);
  long endPos = zone->begin()+dataSz;
  input->seek(pos, WPX_SEEK_SET);
  int N, val;
  long fieldSz = 636;
  if (isKorean) {
    N = (int) input->readULong(2);
    f << "N=" << N << ",";
    long expectedSz = N*fieldSz+2;
    if (expectedSz != dataSz && expectedSz+1 != dataSz) {
      MWAW_DEBUG_MSG(("HMWText::readStyles: the zone size seems odd\n"));
      return false;
    }
  } else {
    long headerSz = (long) input->readULong(4);
    if (headerSz < 0 || headerSz+4 > dataSz) {
      MWAW_DEBUG_MSG(("HMWText::readStyles: the header size seems odd\n"));
      return false;
    }
    N = (int) input->readULong(2);
    if (headerSz+4+4*N > dataSz || headerSz < 4*N+16) {
      MWAW_DEBUG_MSG(("HMWText::readStyles: the number of style seems odd\n"));
      if (headerSz+4+4*N > dataSz)
        return false;
    }
    for (int i = 0; i < 7; i++) {
      val = (int) input->readLong(2);
      if (val) f << "f" << i << "=" << val << ",";
    }
    f << "unkn=[";
    for (int i = 0; i < N; i++) { /* (1d:*)* */
      f << input->readLong(2) << ":";
      f << std::hex << input->readULong(2) << std::dec << ",";
    }
    f << "],";
    input->seek(pos+4+headerSz, WPX_SEEK_SET);
  }
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    f.str("");
    f << zone->name() << "-" << i << ":";
    pos = input->tell();
    if (!isKorean) {
      fieldSz = (long) input->readULong(4)+4;
      if (fieldSz < 0x1bc || pos+fieldSz > endPos) {
        f << "###";
        asciiFile.addPos(pos);
        asciiFile.addNote(f.str().c_str());

        MWAW_DEBUG_MSG(("HMWText::readStyles: can not read field %d\n", i));
        return true;
      }
    }
    val = (int) input->readULong(isKorean ? 2 : 1);
    if (val != i) f << "#id=" << val << ",";

    // f0=c2|c6, f2=0|44, f3=1|14|15|16: fontId?
    for (int j=0; j < (isKorean ? 4 : 5); j++) {
      val = (int) input->readULong(1);
      if (val)
        f << "f" << j << "=" << std::hex << val << std::dec << ",";
    }
    /* g1=9|a|c|e|12|18: size ?, g5=1, g8=0|1, g25=0|18|30|48, g31=1, g35=0|1 */
    for (int j=0; j < (isKorean ? 37 : 33); j++) {
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
    f << "Style-" << i << "[B]:";
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
    for (int j = 0; j < (isKorean ? 100 : 43); j++) {
      val = (int) input->readULong(2);
      if (val)
        f << "h" << j << "=" << val  << ",";
    }
    for (int j = 0; j < (isKorean ? 41 : 0); j++) {
      val = (int) input->readULong(2);
      if (val)
        f << "l" << j << "=" << val  << ",";
    }
    asciiFile.addPos(pos2);
    asciiFile.addNote(f.str().c_str());

    pos2 = input->tell();
    f.str("");
    f << "Style-" << i << "[C]:";
    val = (int) input->readLong(2);
    if (val != -1) f << "unkn=" << val << ",";
    val  = (int) input->readLong(2);
    if (val != i) f << "#id" << val << ",";
    if (!isKorean) {
      for (int j = 0; j < 4; j++) {
        val = (int) input->readLong(2);
        if (val) f << "f" << j << "=" << val << ",";
      }
    }
    int fSz = (int) input->readULong(1);
    if (input->tell()+fSz > pos+fieldSz) {
      MWAW_DEBUG_MSG(("HMWText::readStyles: can not read styleName\n"));
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
void HMWText::setProperty(HMWTextInternal::Paragraph const &para, float)
{
  if (!m_listener) return;
  para.send(m_listener);
}

bool HMWText::readParagraph(HMWZone &zone, HMWTextInternal::Paragraph &para)
{
  para = HMWTextInternal::Paragraph();

  MWAWInputStreamPtr input = zone.m_input;
  long pos = input->tell();
  long dataSz = zone.length();
  if (pos+102 > dataSz) {
    MWAW_DEBUG_MSG(("HMWText::readParagraph: the zone is too short\n"));
    return false;
  }

  libmwaw::DebugStream f;
  libmwaw::DebugFile &asciiFile = zone.ascii();
  int val = (int) input->readLong(2);
  if (val!=100) {
    MWAW_DEBUG_MSG(("HMWText::readParagraph: the data size seems bad\n"));
    f << "##dataSz=" << val << ",";
  }
  int flags = (int) input->readULong(1);
  if (flags&0x80)
    para.m_breakStatus = para.m_breakStatus.get()|libmwaw::NoBreakWithNextBit;
  if (flags&0x40)
    para.m_breakStatus = para.m_breakStatus.get()|libmwaw::NoBreakBit;
  if (flags&0x2)
    para.m_addPageBreak = true;
  if (flags&0x4)
    f << "linebreakByWord,";

  if (flags & 0x39) f << "#fl=" << std::hex << (flags&0x39) << std::dec << ",";

  val = (int) input->readLong(2);
  if (val) f << "#f0=" << val << ",";
  val = (int) input->readULong(2);
  switch(val&3) {
  case 0:
    para.m_justify = libmwaw::JustificationLeft;
    break;
  case 1:
    para.m_justify = libmwaw::JustificationRight;
    break;
  case 2:
    para.m_justify = libmwaw::JustificationCenter;
    break;
  case 3:
    para.m_justify = libmwaw::JustificationFull;
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
  if (val) f << "#f2=" << val << ",";
  for (int i = 0; i < 17; i++) { // g2=0|1, g4=0|1, g6=0|1|8
    val = (int) input->readLong(2);
    if (val) f << "#g" << i << "=" << val << ",";
  }
  for (int i = 0; i < 5; i++) { // h0=h1=h3=1, h4=0|1, h2=1|6
    val = (int) input->readLong(2);
    if (val!=1) f << "#h" << i << "=" << val << ",";
  }
  for (int i = 0; i < 8; i++) { // always 0?
    val = (int) input->readLong(2);
    if (val) f << "#h" << 5+i << "=" << val << ",";
  }
  int nTabs = (int) input->readULong(2);
  if (pos+102+nTabs*12 > dataSz) {
    MWAW_DEBUG_MSG(("HMWText::readParagraph: can not read numbers of tab\n"));
    return false;
  }
  val = (int) input->readULong(2); // always 0
  if (val) f << "#h14=" << val << ",";
  para.m_extra=f.str();
  static bool first=true;
  f.str("");
  if (first) {
    f << "Entries(Ruler):";
    first = false;
  } else
    f << "Ruler:";
  f << para;

  asciiFile.addPos(pos-4);
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
    if (val) f << "decimalChar=" << char(val) << ",";

    tab.m_leaderCharacter = (uint16_t)input->readULong(2);
    val = (int)input->readULong(2); // 0|73|74|a044|f170|f1e0|f590
    if (val) f << "f0=" << std::hex << val << std::dec << ",";

    tab.m_position = float(input->readLong(4))/65536.f/72.f;
    para.m_tabs->push_back(tab);
    f << tab;

    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+12, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
//     the token
////////////////////////////////////////////////////////////
bool HMWText::readToken(HMWZone &zone, HMWTextInternal::Token &token)
{
  token = HMWTextInternal::Token();

  MWAWInputStreamPtr input = zone.m_input;
  long pos = input->tell();
  long dataSz = zone.length();
  if (pos+10 > dataSz) {
    MWAW_DEBUG_MSG(("HMWText::readToken: the zone is too short\n"));
    return false;
  }

  libmwaw::DebugStream f;
  libmwaw::DebugFile &asciiFile = zone.ascii();
  int val = (int) input->readLong(2);
  if (val!=8) {
    MWAW_DEBUG_MSG(("HMWText::readToken: the data size seems bad\n"));
    f << "##dataSz=" << val << ",";
  }

  token.m_type = (int) input->readLong(1);
  val = (int) input->readLong(1);
  if (val) f << "f0=" << val << ",";
  val = (int) input->readLong(2);
  if (val) f << "f1=" << val << ",";
  token.m_id = (long)  input->readULong(4);
  token.m_extra = f.str();
  f.str("");
  static bool first=true;
  f.str("");
  if (first) {
    f << "Entries(Token):";
    first = false;
  } else
    f << "token:";
  f << token;
  asciiFile.addPos(pos-4);
  asciiFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
//     the sections
////////////////////////////////////////////////////////////
bool HMWText::readSections(shared_ptr<HMWZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWText::readSections: called without any zone\n"));
    return false;
  }
  long dataSz = zone->length();
  if (dataSz < 160) {
    MWAW_DEBUG_MSG(("HMWText::readSections: the zone seems too short\n"));
    return false;
  }

  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->ascii();
  libmwaw::DebugStream f;
  zone->m_parsed = true;

  f << zone->name() << "(A):PTR=" << std::hex << zone->fileBeginPos() << std::dec << ",";
  long pos=0;
  input->seek(pos, WPX_SEEK_SET);
  long val = input->readLong(2);
  if (val != 1) // always 1
    f << "f0=" << val << ",";
  int numColumns = (int) input->readLong(2);
  if (numColumns != 1)
    f << "nCols=" << numColumns << ",";
  for (int i= 0; i < 2; i++) { // 1,0
    val = (long) input->readLong(1);
    if (val) f << "f" << i+1 << "=" << val << ",";
  }
  for (int i = 0; i < 19; i++) { // always 0
    val = (long) input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }

  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  pos = input->tell();
  // a small zone which look like similar to some end of printinfo zone(A):
  f.str("");
  f << zone->name() << "(B):";
  float colWidth = float(input->readLong(4))/65536.f;
  f << "colWidth=" << colWidth << ",";
  asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  input->seek(pos+24, WPX_SEEK_SET);

  pos = input->tell();
  f.str("");
  f << zone->name() << "(C):";
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  input->seek(pos+40, WPX_SEEK_SET);

  pos = input->tell();
  f.str("");
  f << zone->name() << "(D):";
  for (int i = 0; i < 4; i++) {
    long id = input->readLong(4);
    if (!id) continue;
    if (i < 2) f << "headerId=" << std::hex << id << std::dec << ",";
    else f << "footerId=" << std::hex << id << std::dec << ",";
  }
  for (int i = 0; i < 8; i++) {
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

//! send data to the listener

void HMWText::flushExtra()
{
  if (!m_listener) return;
  std::multimap<long, shared_ptr<HMWZone> >::iterator tIt
    =m_state->m_IdTextMaps.begin();
  for ( ; tIt!=m_state->m_IdTextMaps.end(); tIt++) {
    if (!tIt->second) continue;
    HMWZone &zone = *tIt->second;
    if (zone.m_parsed) continue;
    sendText(zone);
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
