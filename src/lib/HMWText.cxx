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
  State() : m_version(-1), m_numPages(-1), m_actualPage(0) {
  }

  //! the file version
  mutable int m_version;

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

int HMWText::numPages() const
{
  if (m_state->m_numPages >= 0)
    return m_state->m_numPages;
#if 0
  const_cast<HMWText *>(this)->computePositions();
#endif
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

  long dataSz = (long) zone->m_data.size();
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw::DebugStream f;
  zone->m_parsed = true;

  long pos = 0;
  input->seek(0, WPX_SEEK_SET);
  f << "PTR=" << std::hex << zone->m_filePos << std::dec << ",";
  asciiFile.addPos(0);
  asciiFile.addNote(f.str().c_str());

  long val;
  while (!input->atEOS()) {
    pos = input->tell();
    f.str("");
    f << zone->name()<< ":";
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
      done=readFont(zone,font);
      break;
    }
    case 2: { // ruler
      f << "ruler,";
      HMWTextInternal::Paragraph para;
      done=readParagraph(zone,para);
      break;
    }
    case 3: { // footnote, object?
      f << "token,";
      HMWTextInternal::Token token;
      done=readToken(zone,token);
      break;
    }
    case 4:
      f << "section,";
      break;
    case 9:
      f << "rubi,";
      break;
    case 14:
      f << "endRubi?,";
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
      MWAW_DEBUG_MSG(("HMWText::readTextZone: can not read a zone\n"));
      break;
    }

    pos = input->tell();
    f.str("");
    f << zone->name()<< ":text,";
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
        f << "###";
        break;
      }
      f << (char) c;
    }
    if (input->tell() != pos) {
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
    }
    if (!ok) {
      MWAW_DEBUG_MSG(("HMWText::readTextZone: can not read a text zone\n"));
      break;
    }
  }
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
  font.sendTo(m_listener.get(), m_convertissor, ft);
}

// a font in the text zone
bool HMWText::readFont(shared_ptr<HMWZone> zone, HMWTextInternal::Font &font)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWText::readFont: called without any zone\n"));
    return false;
  }

  font = HMWTextInternal::Font();

  MWAWInputStreamPtr input = zone->m_input;
  long pos = input->tell();
  if (pos+30 > (long) zone->m_data.size()) {
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
  font.m_font.setSize((int) input->readLong(2));
  for (int i = 0; i < 5; i++) { // 0,0,0,1,0
    val = (int) input->readLong(2);
    if ((i==3 && val!=1) || (i!=3 && val))
      f << "#f" << i+2 << "=" << val << ",";
  }

  int flag =(int) input->readULong(2);
  uint32_t flags=0;
  if (flag&1)
    font.m_font.setUnderlineStyle(MWAWBorder::Double);
  if (flag&2)
    font.m_font.setUnderlineStyle(MWAWBorder::Dot);
  if (flag&4) {
    font.m_font.setUnderlineStyle(MWAWBorder::Dot);
    f << "underline[w=2],";
  }
  if (flag&8)
    font.m_font.setUnderlineStyle(MWAWBorder::Dash);
  if (flag&0x10)
    flags |= MWAW_STRIKEOUT_BIT;
  if (flag&0x20) {
    flags |= MWAW_STRIKEOUT_BIT;
    f << "strike[double],";
  }
  if (flag&0xFFC0)
    f << "#flag0=" << std::hex << (flag&0xFFF2) << std::dec << ",";
  flag =(int) input->readULong(2);
  if (flag&1) flags |= MWAW_BOLD_BIT;
  if (flag&0x2) flags |= MWAW_ITALICS_BIT;
  if (flag&0x4) flags |= MWAW_OUTLINE_BIT;
  if (flag&0x8) flags |= MWAW_SHADOW_BIT;
  if (flag&0x10) flags |= MWAW_REVERSEVIDEO_BIT;
  if (flag&0x20) flags |= MWAW_SUPERSCRIPT100_BIT;
  if (flag&0x40) flags |= MWAW_SUBSCRIPT100_BIT;
  if (flag&0x80) flags |= MWAW_SUPERSCRIPT_BIT;
  if (flag&0x100) {
    flags |= MWAW_OVERLINE_BIT;
    f << "overline[dotted],";
  }
  if (flag&0x200) f << "border[rectangle],";
  if (flag&0x400) f << "border[rounded],";
  if (flag&0x800) font.m_font.setUnderlineStyle(MWAWBorder::Single);
  if (flag&0x1000) {
    font.m_font.setUnderlineStyle(MWAWBorder::Single);
    f << "underline[w=2]";
  }
  if (flag&0x2000) {
    font.m_font.setUnderlineStyle(MWAWBorder::Single);
    f << "underline[w=3]";
  }
  if (flag&0xC000)
    f << "#flag1=" << std::hex << (flag&0xC000) << std::dec << ",";
  /* 0: black, 0x16:red, 0x72:green, 0xc1: blue*/
  int color = (int) input->readLong(2);
  if (color)
    f << "fColor=" << color << ",";
  val = (int) input->readLong(2);
  if (val) f << "#unk=" << val << ",";
  color = (int) input->readLong(2);
  if (color)
    f << "backColor=" << color << ",";
  int pattern = (int) input->readLong(2);
  if (pattern)
    f << "pattern=" << pattern << ",";
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

  zone->ascii().addPos(pos-4);
  zone->ascii().addNote(f.str().c_str());

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
  long dataSz = (long) zone->m_data.size();
  if (dataSz < 2) {
    MWAW_DEBUG_MSG(("HMWText::readFontNames: the zone seems too short\n"));
    return false;
  }
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw::DebugStream f;
  zone->m_parsed = true;

  f << zone->name() << ":PTR=" << std::hex << zone->m_filePos << std::dec << ",";
  input->seek(0, WPX_SEEK_SET);
  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  long expectedSz = N*68+2;
  if (expectedSz != dataSz && expectedSz+1 != dataSz) {
    MWAW_DEBUG_MSG(("HMWText::readFontNames: the zone size seems odd\n"));
    return false;
  }
  asciiFile.addPos(0);
  asciiFile.addNote(f.str().c_str());

  long pos;
  int val;
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
  long dataSz = (long) zone->m_data.size();
  if (dataSz < 2) {
    MWAW_DEBUG_MSG(("HMWText::readStyles: the zone seems too short\n"));
    return false;
  }
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw::DebugStream f;
  zone->m_parsed = true;

  f << zone->name() << ":PTR=" << std::hex << zone->m_filePos << std::dec << ",";
  input->seek(0, WPX_SEEK_SET);
  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  long expectedSz = N*636+2;
  if (expectedSz != dataSz && expectedSz+1 != dataSz) {
    MWAW_DEBUG_MSG(("HMWText::readStyles: the zone size seems odd\n"));
    return false;
  }
  asciiFile.addPos(0);
  asciiFile.addNote(f.str().c_str());

  long pos;
  int val;
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << zone->name() << "-" << i << ":";
    val = (int) input->readLong(2);
    if (val != i) f << "#id=" << val << ",";

    // f0=c2|c6, f2=0|44, f3=1|14|15|16: fontId?
    for (int j=0; j < 4; j++) {
      val = (int) input->readULong(1);
      if (val)
        f << "f" << j << "=" << std::hex << val << std::dec << ",";
    }
    /* g1=9|a|c|e|12|18: size ?, g5=1, g8=0|1, g25=0|18|30|48, g31=1, g35=0|1 */
    for (int j=0; j < 37; j++) {
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
    for (int j = 0; j < 100; j++) {
      val = (int) input->readULong(2);
      if (val)
        f << "h" << j << "=" << val  << ",";
    }
    for (int j = 0; j < 41; j++) {
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
    int fSz = (int) input->readULong(1);
    if (pos2+5+fSz > pos+636) {
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
    if (input->tell() != pos+636)
      asciiFile.addDelimiter(input->tell(),'|');

    input->seek(pos+636, WPX_SEEK_SET);
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
void HMWText::setProperty(HMWTextInternal::Paragraph const &para, float width)
{
  if (!m_listener) return;
  double origRMargin = para.m_margins[2].get();
  double rMargin=double(width)-origRMargin;
  if (rMargin < 0.0) rMargin = 0;
  const_cast<HMWTextInternal::Paragraph &>(para).m_margins[2] = rMargin;
  para.send(m_listener);
  const_cast<HMWTextInternal::Paragraph &>(para).m_margins[2] = origRMargin;
}

bool HMWText::readParagraph(shared_ptr<HMWZone> zone, HMWTextInternal::Paragraph &para)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWText::readParagraph: called without any zone\n"));
    return false;
  }

  para = HMWTextInternal::Paragraph();

  MWAWInputStreamPtr input = zone->m_input;
  long pos = input->tell();
  long dataSz = (long) zone->m_data.size();
  if (pos+102 > dataSz) {
    MWAW_DEBUG_MSG(("HMWText::readParagraph: the zone is too short\n"));
    return false;
  }

  libmwaw::DebugStream f;
  libmwaw::DebugFile &asciiFile = zone->m_asciiFile;
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
bool HMWText::readToken(shared_ptr<HMWZone> zone, HMWTextInternal::Token &token)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWText::readToken: called without any zone\n"));
    return false;
  }

  token = HMWTextInternal::Token();

  MWAWInputStreamPtr input = zone->m_input;
  long pos = input->tell();
  long dataSz = (long) zone->m_data.size();
  if (pos+10 > dataSz) {
    MWAW_DEBUG_MSG(("HMWText::readToken: the zone is too short\n"));
    return false;
  }

  libmwaw::DebugStream f;
  libmwaw::DebugFile &asciiFile = zone->m_asciiFile;
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
  long dataSz = (long) zone->m_data.size();
  if (dataSz < 160) {
    MWAW_DEBUG_MSG(("HMWText::readSections: the zone seems too short\n"));
    return false;
  }

  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw::DebugStream f;
  zone->m_parsed = true;

  f << zone->name() << "(A):PTR=" << std::hex << zone->m_filePos << std::dec << ",";
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
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
