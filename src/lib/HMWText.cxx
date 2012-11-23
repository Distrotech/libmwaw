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
#include "MWAWPageSpan.hxx"
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

/** Internal: class to store the paragraph properties */
struct Paragraph : public MWAWParagraph {
  //! Constructor
  Paragraph() : MWAWParagraph() {
  }
  //! destructor
  ~Paragraph() {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Paragraph const &ind) {
    o << reinterpret_cast<MWAWParagraph const &>(ind) << ",";
    return o;
  }
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
    bool ok = false, done=false;;
    switch(type) {
    case 1: {
      HMWTextInternal::Font font;
      ok=done=readFont(zone,font);
      break;
    }
    case 2: { // ruler
      f << "ruler,";
      ok = (pos+106 <= dataSz);
      if (!ok) break;

      input->seek(pos+103, WPX_SEEK_SET);
      int nTabs = (int) input->readLong(1);
      ok = pos+106+nTabs*12 <= dataSz;
      input->seek(pos+106+nTabs*12, WPX_SEEK_SET);
      if (!ok) break;
      break;
    }
    case 3: // footnote, object?
      f << "type=3,";
      ok = (pos+14 <= dataSz);
      if (!ok) break;
      input->seek(pos+14, WPX_SEEK_SET);
      break;
    case 4:
      f << "section,";
      ok = (pos+6 <= dataSz);
      if (!ok) break;
      input->seek(pos+6, WPX_SEEK_SET);
      break;
      // 9: rubi text
      // e: end rubi?
    default: {
      f << "##type=" << type;
      long sz = input->readULong(2);
      ok = pos+6+sz;
      input->seek(pos+6+sz, WPX_SEEK_SET);
      break;
    }
    }
    if (!done) {
      if (!ok) f << "###";
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      if (!ok) break;
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
    if (!ok)
      break;
  }
  return true;
}

////////////////////////////////////////////////////////////
//     Fonts
////////////////////////////////////////////////////////////

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

  f.str("");
  f << "TextZone[font]:";
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
  int val = (int) input->readLong(2);
  if (val!=100) {
    MWAW_DEBUG_MSG(("HMWText::readParagraph: the data size seems bad\n"));
    f << "##dataSz=" << val << ",";
  }
  input->seek(pos+98, WPX_SEEK_SET);
  int nTabs = (int) input->readULong(2);
  if (pos+102+nTabs*12 <= dataSz) {
    MWAW_DEBUG_MSG(("HMWText::readParagraph: can not read numbers of tab\n"));
    return false;
  }
  input->seek(pos+102+nTabs*12, WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//     the zones header
////////////////////////////////////////////////////////////


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
