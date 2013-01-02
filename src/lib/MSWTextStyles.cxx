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
#include <map>

#include "MWAWContentListener.hxx"

#include "MSWParser.hxx"
#include "MSWStruct.hxx"
#include "MSWText.hxx"

#include "MSWTextStyles.hxx"

/** Internal: the structures of a MSWTextStyles */
namespace MSWTextStylesInternal
{
////////////////////////////////////////
//! Internal: the state of a MSWTextStylesInternal
struct State {
  //! constructor
  State() : m_version(-1), m_defaultFont(2,12),
    m_fontList(), m_paragraphList(), m_sectionList(),
    m_textstructParagraphList(),
    m_styleFontMap(), m_styleParagraphMap() {
  }
  //! the file version
  int m_version;

  //! the default font ( NewYork 12pt)
  MWAWFont m_defaultFont;

  //! the list of fonts
  std::vector<MSWStruct::Font> m_fontList;

  //! the list of paragraph
  std::vector<MSWStruct::Paragraph> m_paragraphList;

  //! the list of section
  std::vector<MSWStruct::Section> m_sectionList;

  //! the list of paragraph in textstruct
  std::vector<MSWStruct::Paragraph> m_textstructParagraphList;

  //! the list of fonts in style
  std::map<int, MSWStruct::Font> m_styleFontMap;

  //! the list of paragraph in style
  std::map<int, MSWStruct::Paragraph> m_styleParagraphMap;
};
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MSWTextStyles::MSWTextStyles
(MWAWInputStreamPtr ip, MSWText &textParser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new MSWTextStylesInternal::State),
  m_mainParser(textParser.m_mainParser), m_textParser(&textParser), m_asciiFile(textParser.ascii())
{
}

MSWTextStyles::~MSWTextStyles()
{ }

int MSWTextStyles::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_textParser->version();
  return m_state->m_version;
}

MWAWFont const &MSWTextStyles::getDefaultFont() const
{
  return m_state->m_defaultFont;
}

////////////////////////////////////////////////////////////
// try to read a font
////////////////////////////////////////////////////////////
bool MSWTextStyles::readFont(MSWStruct::Font &font, MSWTextStyles::ZoneType type)
{
  bool mainZone = type==TextZone;
  libmwaw::DebugStream f;

  long pos = m_input->tell();
  int sz = (int) m_input->readULong(1);
  if (sz > 20 || sz == 3) {
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  if (sz == 0) return true;

  int flag = (int) m_input->readULong(1);
  uint32_t flags = 0;
  if (flag&0x80) flags |= MWAWFont::boldBit;
  if (flag&0x40) flags |= MWAWFont::italicBit;
  if (flag&0x20) flags |= MWAWFont::strikeOutBit;
  if (flag&0x10) flags |= MWAWFont::outlineBit;
  if (flag&0x8) flags |= MWAWFont::shadowBit;
  if (flag&0x4) flags |= MWAWFont::smallCapsBit;
  if (flag&0x2) flags |= MWAWFont::allCapsBit;
  if (flag&0x1) flags |= MWAWFont::hiddenBit;

  int what = 0;
  /*  01: horizontal decal, 2: vertical decal, 4; underline, 08: fSize,  10: set font, 20: font color, 40: ???(maybe reset)
  */
  if (sz >= 2) what = (int) m_input->readULong(1);

  if (sz >= 4) {
    int fId = (int) m_input->readULong(2);
    if (fId) {
      if (mainZone && (what & 0x50)==0) f << "#fId,";
      font.m_font->setId(fId);
    } else if (what & 0x10) {
    }
    what &= 0xEF;
  } else if (what & 0x10) {
  }
  if (sz >= 5) {
    int fSz = (int) m_input->readULong(1)/2;
    if (fSz) {
      if (mainZone && (what & 0x48)==0) f << "#fSz,";
      font.m_font->setSize(fSz);
    }
    what &= 0xF7;
  }

  if (sz >= 6) {
    int decal = (int) m_input->readLong(1); // unit point
    if (decal) {
      if (what & 0x2) {
        if (decal > 0)
          flags |= MWAWFont::superscript100Bit;
        else
          flags |= MWAWFont::subscript100Bit;
      } else
        f << "#vDecal=" << decal;
    }
    what &= 0xFD;
  }
  if (sz >= 7) {
    int decal = (int) m_input->readLong(1); // unit point > 0 -> expand < 0: condensed
    if (decal) {
      if ((what & 0x1) == 0) f << "#";
      f << "hDecal=" << decal <<",";
    }
    what &= 0xFE;
  }

  if (sz >= 8) {
    int val = (int) m_input->readULong(1);
    if (val & 0xF0) {
      if (what & 0x20) {
        Vec3uc col;
        if (m_mainParser->getColor((val>>4),col))
          font.m_font->setColor(col);
        else
          f << "#fColor=" << (val>>4) << ",";
      } else
        f << "#fColor=" << (val>>4) << ",";
    }
    what &= 0xDF;

    if (val && (what & 0x4)) {
      switch(val&0xf) {
      case 8:
        font.m_font->setUnderlineStyle(MWAWBorder::Dot);
        break;
      case 6:
        font.m_font->setUnderlineStyle(MWAWBorder::Double);
        break;
      case 2:
        font.m_font->setUnderlineStyle(MWAWBorder::Single);
        break;
      default:
        f << "#underline=" << (val &0xf) << ",";
        font.m_font->setUnderlineStyle(MWAWBorder::Single);
      }
      what &= 0xFB;
    } else if (val & 0xf)
      f << "#underline?=" << (val &0xf) << ",";
  }

  font.m_unknown =what;
  font.m_font->setFlags(flags);

  bool ok = false;
  if (mainZone && sz >= 10 && sz <= 12) {
    int wh = (int) m_input->readULong(1);
    long pictPos = 0;
    for (int i = 10; i < 13; i++) {
      pictPos <<= 8;
      if (i <= sz) pictPos += m_input->readULong(1);
    }
    long actPos = m_input->tell();
    if (m_mainParser->checkPicturePos(pictPos, wh)) {
      ok = true;
      m_input->seek(actPos, WPX_SEEK_SET);
      font.m_picturePos = pictPos;
      f << "pictWh=" << wh << ",";
    } else
      m_input->seek(pos+1+8, WPX_SEEK_SET);
  }
  if (!ok && sz >= 9) {
    int wh = (int) m_input->readLong(1);
    switch(wh) {
    case -1:
      ok = true;
      break;
    case 0: // line height ?
      if (sz < 10) break;
      font.m_size=(int) m_input->readULong(1)/2;
      ok = true;
      break;
    default:
      break;
    }
  }
  if (!ok && sz >= 9) {
    m_input->seek(pos+1+8, WPX_SEEK_SET);
    f << "#";
  }
  if (long(m_input->tell()) != pos+1+sz)
    ascii().addDelimiter(m_input->tell(), '|');

  m_input->seek(pos+1+sz, WPX_SEEK_SET);
  font.m_extra = f.str();
  return true;
}

bool MSWTextStyles::getFont(ZoneType type, int id, MSWStruct::Font &font)
{
  MSWStruct::Font *fFont = 0;
  switch(type) {
  case TextZone:
    if (id < 0 || id >= int(m_state->m_fontList.size()))
      break;
    fFont = &m_state->m_fontList[(size_t)id];
    break;
  case StyleZone:
    if (m_state->m_styleFontMap.find(id) == m_state->m_styleFontMap.end())
      break;
    fFont = &m_state->m_styleFontMap.find(id)->second;
    break;
  case TextStructZone:
  case InParagraphDefinition:
  default:
    MWAW_DEBUG_MSG(("MSWTextStyles::getFont: do not know how to send this type of font\n"));
    return false;
  }
  if (!fFont) {
    MWAW_DEBUG_MSG(("MSWTextStyles::getFont: can not find font with %d[type=%d]\n", id, int(type)));
    return false;
  }
  int fId = font.m_font->id(), fSz = font.m_font->size();
  font = *fFont;
  if (font.m_font->id() < 0)
    font.m_font->setId(fId);
  if (font.m_font->size() <= 0)
    font.m_font->setSize(fSz);
  return true;
}

bool MSWTextStyles::sendFont(ZoneType type, int id, MSWStruct::Font &actFont)
{
  if (!m_listener || !getFont(type, id, actFont)) return true;
  setProperty(actFont);
  return true;
}

void MSWTextStyles::setProperty(MSWStruct::Font const &font)
{
  if (!m_listener) return;
  MWAWFont tmp = font.m_font.get();
  if (tmp.id() < 0) tmp.setId(m_state->m_defaultFont.id());
  if (tmp.size() <= 0) tmp.setSize(m_state->m_defaultFont.size());
  tmp.setFlags(font.getFlags());
  tmp.setUnderlineStyle(font.getUnderlineStyle());
  tmp.sendTo(m_listener.get(), m_convertissor, tmp);
}

////////////////////////////////////////////////////////////
// read/send the paragraph zone
////////////////////////////////////////////////////////////
bool MSWTextStyles::getParagraph(ZoneType type, int id, MSWStruct::Paragraph &para)
{
  switch(type) {
  case TextZone:
    if (id < 0 || id >= int(m_state->m_paragraphList.size()))
      break;
    para = m_state->m_paragraphList[(size_t)id];
    return true;
  case StyleZone:
    if (m_state->m_styleParagraphMap.find(id) == m_state->m_styleParagraphMap.end())
      break;
    para = m_state->m_styleParagraphMap.find(id)->second;
    return true;
  case TextStructZone:
    if (id < 0 || id >= int(m_state->m_textstructParagraphList.size()))
      break;
    para = m_state->m_textstructParagraphList[(size_t)id];
    return true;
  case InParagraphDefinition:
  default:
    MWAW_DEBUG_MSG(("MSWTextStyles::getParagraph: do not know how to send this type of font\n"));
    return false;
  }

  MWAW_DEBUG_MSG(("MSWTextStyles::getParagraph: can not find paragraph with %d[type=%d]\n", id, int(type)));
  return false;
}

void MSWTextStyles::sendDefaultParagraph()
{
  if (!m_listener) return;
  MSWStruct::Paragraph defPara(version());
  setProperty(defPara, false);
}

bool MSWTextStyles::readParagraph(MSWStruct::Paragraph &para, int dataSz)
{
  int sz;
  if (dataSz >= 0)
    sz = dataSz;
  else
    sz = (int) m_input->readULong(2);

  long pos = m_input->tell();
  long endPos = pos+sz;

  if (sz == 0) return true;
  if (!m_mainParser->isFilePos(endPos)) return false;

  int const vers = version();
  libmwaw::DebugStream f;
  int numFonts[2]= {0,0};
  while (long(m_input->tell()) < endPos) {
    long actPos = m_input->tell();
    /* 5-16: basic paragraph properties
       75-84: basic section properties
       other
     */
    if (para.read(m_input,endPos)) continue;
    m_input->seek(actPos, WPX_SEEK_SET);

    int wh = (int) m_input->readULong(1), val;
    if (vers <= 3 && wh >= 0x36 && wh <= 0x45) {
      // this section data has different meaning in v3 and after...
      m_input->seek(actPos, WPX_SEEK_SET);
      break;
    }
    bool done = false;
    long dSz = endPos-actPos;
    switch(wh) {
    case 0:
      done = (actPos+1==endPos||(dataSz==2 && actPos+2==endPos));
      break;
    case 0x38:
      if (dSz < 4) break;
      val = (int) m_input->readLong(1);
      if (val != 2) f << "#shadType=" <<  val << ",";
      f << "shad=" << float(m_input->readLong(2))/100.f << "%,";
      done = true;
      break;
    case 0x3a:
      f << "f" << std::hex << wh << std::dec << ",";
      done = true;
      break;
    case 0x4d: {
      if (dSz < 2) break;
      val = (int) m_input->readLong(1);
      uint32_t flags = para.m_modFont->m_font->flags();
      if (val < 0) {
        para.m_modFont->m_font->setFlags(flags|MWAWFont::subscript100Bit);
        f << "subScript=" << -val/2 << ",";
      } else if (val > 0) {
        para.m_modFont->m_font->setFlags(flags|MWAWFont::superscript100Bit);
        f << "superScript=" << val/2 << ",";
      } else f << "#pos=" << 0 << ",";
      done = true;
      break;
    }
    case 0x3c: // bold
    case 0x3d: // italic?
    case 0x3e: // strikeout (chekme)
    case 0x3f: // outline (chekme)
    case 0x40: // shadow (chekme)
    case 0x41: // small caps (chekme)
    case 0x42: // all caps (chekme)
    case 0x43: // hidden (chekme)
    case 0x45: // underline
    case 0x4a: {
      if (dSz < 2) break;
      done = true;
      val = (int) m_input->readULong(1);
      if (wh == 0x4a) {
        if (val > 4 && val < 40)
          para.m_modFont->m_font->setSize(val/2);
        else
          f << "#fSize=" << val << ",";
        break;
      }
      switch(wh) {
      case 0x3c:
      case 0x3d:
      case 0x3e:
      case 0x3f:
      case 0x40:
      case 0x41:
      case 0x42:
      case 0x43:
        para.m_modFont->m_flags[wh-0x3c]=val;
        break;
      case 0x45:
        para.m_modFont->m_flags[8]=val;
        break;
      default:
        break;
      }
      break;
    }
    case 0x44:
      if (dSz < 3) break;
      done = true;
      val = (int) m_input->readULong(2);
      para.m_modFont->m_font->setId(val);
      break;
    case 0x2: // a small number between 0 and 4
    case 0x34: // 0 ( one time)
    case 0x47: // 0 one time
    case 0x49: // 0 ( one time)
    case 0x4c: // 0, 6, -12
    case 0x5e: // 0
      if (dSz < 2) break;
      done = true;
      val = (int) m_input->readLong(1);
      f << "f" << std::hex << wh << std::dec << "=" << val << ",";
      break;
    case 0x23: // alway 0 ?
      if (dSz < 3) break;
      done = true;
      val = (int) m_input->readLong(2);
      f << "f" << std::hex << wh << std::dec << "=" << val << ",";
      break;
    case 0x9a: { // a size and 2 number ?
      if (dSz < 2) break;
      sz = (int) m_input->readULong(1);
      if (2+sz > dSz || (sz%2)) {
        done = false;
        f << "#";
        break;
      }
      done = true;
      f << "f" << std::hex << wh << "=[";
      for (int i = 0; i < sz/2; i++)
        f << m_input->readULong(2) << ",";
      f << std::dec << "],";
      break;
    }
    case 0x9f: // two small number: table range?
      if (dSz < 3) break;
      done = true;
      f << "f" << std::hex << wh << "=[";
      for (int i = 0; i < 2; i++)
        f << m_input->readULong(1) << ",";
      f << std::dec << "],";
      break;
    case 3: // four small number
      if (dSz < 5) break;
      done = true;
      f << "f" << std::hex << wh << std::dec << "=[";
      for (int i = 0; i < 4; i++)
        f << m_input->readLong(1) << ",";
      f << "],";
      break;
    case 0x50: // two small number
      if (dSz < 4) break;
      done = true;
      f << "f" << std::hex << wh << std::dec << "=[";
      f << m_input->readLong(1) << ",";
      f << m_input->readLong(2) << ",";
      f << "],";
      break;
    case 0x4f: // a small int and a pos?
      if (dSz < 4) break;
      done = true;
      f << "f" << std::hex << wh << std::dec << "=[";
      f << m_input->readLong(1) << ",";
      f << std::hex << m_input->readULong(2) << std::dec << "],";
      break;
    case 0x9e: // two small number + a pos?
      if (dSz < 5) break;
      done = true;
      f << "f" << std::hex << wh << std::dec << "=[";
      for (int i = 0; i < 2; i++)
        f << m_input->readLong(1) << ",";
      f << std::hex << m_input->readULong(2) << std::dec << "],";
      break;
    case 0x4e:
    case 0x53: { // same as 4e but with size=0xa
      done = true;
      Variable<MSWStruct::Font> tmp, *font = &tmp;
      bool extra=false;
      if (wh == 0x4e) {
        if (numFonts[0]++)
          extra = true;
        else
          font = &para.m_font;
      } else {
        if (numFonts[1]++)
          extra = true;
        else
          font = &para.m_font2;
      }
      if (!readFont(**font, InParagraphDefinition) || long(m_input->tell()) > endPos) {
        done = false;
        f << "#";
        break;
      } else if (extra) {
        f << "#font";
        if (wh == 0x53) f << "2";
        f << "=[" << tmp->m_font->getDebugString(m_convertissor) << "," << *tmp << "],";
      }
      break;
    }
    case 0x5f: { // 4 index
      if (dSz < 10) break;
      done = true;
      sz = (int) m_input->readULong(1);
      if (sz != 8) f << "#sz=" << sz << ",";
      f << "f5f=[";
      for (int i = 0; i < 4; i++) f << m_input->readLong(2) << ",";
      f << "],";
      break;
    }
    case 0x17: {
      // find sz=5,9,12,13,17
      sz = (int) m_input->readULong(1);
      if (!sz || 2+sz > dSz) break;
      done = true;
      f << "f" << std::hex << wh << "=[";
      for (int i = 0; i < sz; i++) {
        val= (int) m_input->readULong(1);
        if (val) f << val << ",";
        else f << "_,";
      }
      f << std::dec << "],";
      break;
    }
    case 0x94: // checkme space between column divided by 2 (in table) ?
      if (dSz < 3) break;
      done = true;
      val = (int) m_input->readLong(2);
      f << "colsSep?=" << 2*val/1440. << ",";
      break;
    default:
      break;
    }
    if (!done) {
      m_input->seek(actPos, WPX_SEEK_SET);
      break;
    }
  }
  if (long(m_input->tell()) != endPos) {
    static bool first = true;
    if (first) {
      MWAW_DEBUG_MSG(("MSWTextStyles::readParagraph: can not read end of paragraph\n"));
      first = false;
    }
    ascii().addDelimiter(m_input->tell(),'|');
    f << "####";
    m_input->seek(endPos, WPX_SEEK_SET);
  }
  para.m_extra += f.str();

  return true;
}

void MSWTextStyles::setProperty(MSWStruct::Paragraph const &para,
                                bool recursifCall)
{
  if (!m_listener) return;
  if (para.m_section.isSet() && !recursifCall)
    setProperty(para.m_section.get());
  para.send(m_listener);
}

////////////////////////////////////////////////////////////
// read the char/parag plc
////////////////////////////////////////////////////////////
bool MSWTextStyles::readPLCList(MSWEntry &entry)
{
  if (entry.length() < 10 || (entry.length()%6) != 4) {
    MWAW_DEBUG_MSG(("MSWTextStyles::readPLCList: the zone size seems odd\n"));
    return false;
  }
  long pos = entry.begin();
  entry.setParsed(true);
  m_input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << entry.type() << ":";
  int N=int(entry.length()/6);
  std::vector<long> textPos; // limit of the text in the file
  textPos.resize((size_t)N+1);
  for (int i = 0; i <= N; i++) textPos[(size_t)i] = (long) m_input->readULong(4);
  int const expectedSize = (version() <= 3) ? 0x80 : 0x200;
  for (int i = 0; i < N; i++) {
    if (!m_mainParser->isFilePos(textPos[(size_t)i])) f << "#";

    long defPos = (long) m_input->readULong(2);
    f << std::hex << "[filePos?=" << textPos[(size_t)i] << ",dPos=" << defPos << std::dec << ",";
    f << "],";

    MSWEntry plc;
    plc.setType(entry.id() ? "ParagPLC" : "CharPLC");
    plc.setId(i);
    plc.setBegin(defPos*expectedSize);
    plc.setLength(expectedSize);
    if (!m_mainParser->isFilePos(plc.end())) {
      f << "#PLC,";
      MWAW_DEBUG_MSG(("MSWTextStyles::readPLCList: plc def is outside the file\n"));
    } else {
      long actPos = m_input->tell();
      Vec2<long> fLimit(textPos[(size_t)i], textPos[(size_t)i+1]);
      readPLC(plc, entry.id(), fLimit);
      m_input->seek(actPos, WPX_SEEK_SET);
    }
  }
  f << std::hex << "end?=" << textPos[(size_t)N] << ",";

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

bool MSWTextStyles::readPLC(MSWEntry &entry, int type, Vec2<long> const &fLimit)
{
  int const vers = version();
  int const expectedSize = (vers <= 3) ? 0x80 : 0x200;
  int const posFactor = (vers <= 3) ? 1 : 2;
  if (entry.length() != expectedSize) {
    MWAW_DEBUG_MSG(("MSWTextStyles::readPLC: the zone size seems odd\n"));
    return false;
  }
  m_input->seek(entry.end()-1, WPX_SEEK_SET);
  int N=(int) m_input->readULong(1);
  if (5*(N+1) > entry.length()) {
    MWAW_DEBUG_MSG(("MSWTextStyles::readPLC: the number of plc seems odd\n"));
    return false;
  }

  long pos = entry.begin();
  entry.setParsed(true);
  libmwaw::DebugStream f;
  f << "Entries("<< entry.type() << ")[" << entry.id() << "]:N=" << N << ",";

  m_input->seek(pos, WPX_SEEK_SET);
  std::vector<long> filePos;
  filePos.resize((size_t) N+1);
  for (int i = 0; i <= N; i++)
    filePos[(size_t)i] = (long) m_input->readULong(4);
  if (filePos[0] != fLimit[0]) {
    MWAW_DEBUG_MSG(("MSWTextStyles::readPLC: bad first limit\n"));
    return false;
  }
  std::map<int, int> mapPosId;
  std::vector<int> decal;
  decal.resize((size_t)N);
  size_t numData = type == 0 ? m_state->m_fontList.size() :
                   m_state->m_paragraphList.size();
  MSWText::PLC::Type plcType = type == 0 ? MSWText::PLC::Font : MSWText::PLC::Paragraph;
  std::multimap<long, MSWText::PLC> &plcMap = m_textParser->getFilePLCMap();

  for (size_t i = 0; i < size_t(N); i++) {
    decal[i] = (int) m_input->readULong(1);
    int id = -1;
    if (decal[i]) {
      if (mapPosId.find(decal[i]) != mapPosId.end())
        id = mapPosId.find(decal[i])->second;
      else {
        id = int(numData++);
        mapPosId[decal[i]] = id;

        long actPos = m_input->tell();
        libmwaw::DebugStream f2;
        f2 << entry.type() << "-";

        long dataPos = entry.begin()+posFactor*decal[i];
        if (type == 0) {
          m_input->seek(dataPos, WPX_SEEK_SET);
          f2 << "F" << id << ":";
          MSWStruct::Font font;
          if (!readFont(font, TextZone)) {
            font = MSWStruct::Font();
            f2 << "#";
          } else
            f2 << font.m_font->getDebugString(m_convertissor) << font << ",";
          m_state->m_fontList.push_back(font);
        } else {
          MSWStruct::Paragraph para(vers);
          f2 << "P" << id << ":";

          m_input->seek(dataPos, WPX_SEEK_SET);
          int sz = (int) m_input->readLong(1);
          long endPos;
          if (vers <= 3) {
            sz++;
            endPos = dataPos+sz;
          } else
            endPos = dataPos+2*sz+1;
          if (sz < 4 || endPos > entry.end()) {
            MWAW_DEBUG_MSG(("MSWTextStyles::readPLC: can not read plcSz\n"));
            f2 << "#";
          } else {
            int pId = (int) m_input->readLong(1);
            if (m_state->m_styleParagraphMap.find(pId)==m_state->m_styleParagraphMap.end()) {
              MWAW_DEBUG_MSG(("MSWTextStyles::readPLC: can not find parent paragraph\n"));
              f2 << "#";
            } else
              para = m_state->m_styleParagraphMap.find(pId)->second;
            f2 << "sP" << pId << ",";
            int val = (int) m_input->readLong(1);
            if (val) // some flag: 0, 20 ?
              f2 << "g0=" << std::hex << val << std::dec << ",";
            val = (int) m_input->readLong(1);
            if (val) // a small number ?
              f2 << "g1=" << val << ",";
            if (vers > 3) {
              para.m_dim->setX(float(m_input->readULong(2))/1440.f);
              para.m_dim->setY(float(m_input->readULong(2))/72.f);
            }
            if (sz > 4) {
              ascii().addDelimiter(m_input->tell(),'|');
              if (readParagraph(para, int(endPos-m_input->tell()))) {
#ifdef DEBUG_WITH_FILES
                para.print(f2, m_convertissor);
#endif
              } else {
                para = MSWStruct::Paragraph(vers);
                f2 << "#";
              }
            }
          }
          m_state->m_paragraphList.push_back(para);
        }
        m_input->seek(actPos, WPX_SEEK_SET);
        ascii().addPos(dataPos);
        ascii().addNote(f2.str().c_str());
      }
    }
    f << std::hex << filePos[i] << std::dec;
    MSWText::PLC plc(plcType, id);
    plcMap.insert(std::multimap<long,MSWText::PLC>::value_type(filePos[i], plc));
    if (id >= 0) {
      if (type==0) f << ":F" << id;
      else f << ":P" << id;
    }
    f << ",";
  }

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(entry.end());
  ascii().addNote("_");
  if (filePos[(size_t)N] != fLimit[1]) {
    MSWEntry nextEntry(entry);
    nextEntry.setBegin(entry.begin()+expectedSize);
    Vec2<long> newLimit(filePos[(size_t)N], fLimit[1]);
    readPLC(nextEntry,type,newLimit);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the text structure
////////////////////////////////////////////////////////////
bool MSWTextStyles::readTextStructList(MSWEntry &entry)
{
  if (entry.length() < 19) {
    MWAW_DEBUG_MSG(("MSWTextStyles::readTextStructList: the zone seems to short\n"));
    return false;
  }
  int const vers = version();
  long pos = entry.begin();
  m_input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  int type = (int) m_input->readLong(1);
  if (type != 1 && type != 2) {
    MWAW_DEBUG_MSG(("MSWTextStyles::readTextStructList: find odd type %d\n", type));
    return false;
  }

  int num = 0;
  while (type == 1) {
    /* probably a paragraph definition. Fixme: create a function */
    int length = (int) m_input->readULong(2);
    long endPos = pos+3+length;
    if (endPos > entry.end()) {
      ascii().addPos(pos);
      ascii().addNote("TextStruct[paragraph]#");
      MWAW_DEBUG_MSG(("MSWTextStyles::readTextStructList: zone(paragraph) is too big\n"));
      return false;
    }
    f.str("");
    f << "ParagPLC:tP" << num++<< "]:";
    MSWStruct::Paragraph para(vers);
    m_input->seek(-2,WPX_SEEK_CUR);
    if (readParagraph(para) && long(m_input->tell()) <= endPos) {
#ifdef DEBUG_WITH_FILES
      para.print(f, m_convertissor);
#endif
    } else {
      para = MSWStruct::Paragraph(vers);
      f << "#";
    }
    m_state->m_textstructParagraphList.push_back(para);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(endPos, WPX_SEEK_SET);

    pos = m_input->tell();
    type = (int) m_input->readULong(1);
    if (type == 2) break;
    if (type != 1) {
      MWAW_DEBUG_MSG(("MSWTextStyles::readTextStructList: find odd type %d\n", type));
      return false;
    }
  }
  m_input->seek(-1,WPX_SEEK_CUR);
  return true;
}

int MSWTextStyles::readTextStructParaZone(std::string &extra)
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;
  int id = -1;
  int c = (int) m_input->readULong(1);
  switch (c) {
    // find also 0x1e80 here, look like a tmp setting...
  case 0x80:
    id = (int) m_input->readULong(1);
    break;
  case 0:
    break;
  default: {
    MSWStruct::Paragraph para(version());
    m_input->seek(-1, WPX_SEEK_CUR);
    if (readParagraph(para, 2)) {
      id = int(m_state->m_textstructParagraphList.size());
      m_state->m_textstructParagraphList.push_back(para);
#ifdef DEBUG_WITH_FILES
      f << "[";
      para.print(f, m_convertissor);
      f << "]";
#endif
    } else {
      m_input->seek(pos+1, WPX_SEEK_SET);
      f << "#f" << std::hex << c << std::dec << "=" << (int) m_input->readULong(1);
    }
    break;
  }
  }
  extra = f.str();
  m_input->seek(pos+2, WPX_SEEK_SET);
  return id;
}

////////////////////////////////////////////////////////////
// read/send the section zone
////////////////////////////////////////////////////////////
bool MSWTextStyles::getSection(ZoneType type, int id, MSWStruct::Section &section)
{
  switch(type) {
  case TextZone:
    if (id < 0 || id >= int(m_state->m_sectionList.size()))
      break;
    section = m_state->m_sectionList[(size_t) id];
    return true;
    break;
  case StyleZone:
  case TextStructZone:
  case InParagraphDefinition:
  default:
    MWAW_DEBUG_MSG(("MSWTextStyles:::getSection do not know how to get this type of section\n"));
    return false;
  }
  MWAW_DEBUG_MSG(("MSWTextStyles:::getSection can not find this section\n"));
  return false;
}

bool MSWTextStyles::getSectionParagraph(ZoneType type, int id, MSWStruct::Paragraph &para)
{
  MSWStruct::Section sec;
  if (!getSection(type, id, sec)) return false;
  if (!sec.m_paragraphId.isSet()) return false;
  return getParagraph(StyleZone, *sec.m_paragraphId, para);
}

bool MSWTextStyles::getSectionFont(ZoneType type, int id, MSWStruct::Font &font)
{
  MSWStruct::Section sec;
  if (!getSection(type, id, sec)) return false;

  if (!sec.m_paragraphId.isSet()) return false;
  MSWStruct::Paragraph para(version());
  if (!getParagraph(StyleZone, *sec.m_paragraphId, para))
    return false;

  if (para.m_font2.isSet())
    font = para.m_font2.get();
  else if (para.m_font.isSet())
    font = para.m_font.get();
  else
    return false;
  return true;
}


bool MSWTextStyles::readSection(MSWEntry &entry)
{
  if (entry.length() < 14 || (entry.length()%10) != 4) {
    MWAW_DEBUG_MSG(("MSWTextStyles::readSection: the zone size seems odd\n"));
    return false;
  }
  long pos = entry.begin();
  entry.setParsed(true);
  m_input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Section:";
  size_t N=size_t(entry.length()/10);
  std::vector<long> positions; // checkme
  positions.resize(N+1);
  for (size_t i = 0; i <= N; i++) positions[i] = (long) m_input->readULong(4);

  MSWText::PLC plc(MSWText::PLC::Section);
  std::multimap<long, MSWText::PLC> &plcMap = m_textParser->getTextPLCMap();
  long textLength = m_textParser->getMainTextLength();
  for (size_t i = 0; i < N; i++) {
    MSWStruct::Section sec;
    sec.m_type = (int) m_input->readULong(1);
    sec.m_flag = (int) m_input->readULong(1);
    sec.m_id = int(i);
    unsigned long filePos = m_input->readULong(4);
    if (textLength && positions[i] > textLength) {
      MWAW_DEBUG_MSG(("MSWTextStyles::readSection: text positions is bad...\n"));
      f << "#";
    } else {
      plc.m_id = int(i);
      plcMap.insert(std::multimap<long,MSWText::PLC>::value_type(positions[i],plc));
    }
    f << std::hex << "pos?=" << positions[i] << ":[" << sec << ",";
    if (filePos != 0xFFFFFFFFL) {
      f << "pos=" << std::hex << filePos << std::dec << ",";
      long actPos = m_input->tell();
      readSection(sec,(long) filePos);
      m_input->seek(actPos, WPX_SEEK_SET);
    }
    f << "],";

    m_state->m_sectionList.push_back(sec);
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

bool MSWTextStyles::readSection(MSWStruct::Section &sec, long debPos)
{
  if (!m_mainParser->isFilePos(debPos)) {
    MWAW_DEBUG_MSG(("MSWTextStyles::readSection: can not find section data...\n"));
    return false;
  }
  int const vers = version();
  m_input->seek(debPos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  int sz = (int) m_input->readULong(1);
  long endPos = debPos+sz+1;
  if (sz < 1 || sz >= 255) {
    MWAW_DEBUG_MSG(("MSWTextStyles::readSection: data section size seems bad...\n"));
    f << "Section-" << sec.m_id.get() << ":#" << sec;
    ascii().addPos(debPos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  while (m_input->tell() < endPos) {
    long pos = m_input->tell();
    bool ok;
    if (vers <= 3)
      ok = sec.readV3(m_input, endPos);
    else
      ok = sec.read(m_input, endPos);
    if (ok) continue;
    f << "#";
    ascii().addDelimiter(pos,'|');
    break;
  }
  f << "Section-S" << sec.m_id.get() << ":" << sec;
  ascii().addPos(debPos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(endPos);
  ascii().addNote("_");
  return true;
}

void MSWTextStyles::setProperty(MSWStruct::Section const &sec)
{
  if (!m_listener) return;
  if (m_listener->isHeaderFooterOpened()) {
    MWAW_DEBUG_MSG(("MSWTextStyles::setProperty: can not open a section in header/footer\n"));
  } else {
    int numCols = sec.m_col.get();
    int actCols = m_listener->getSectionNumColumns();
    if (numCols >= 1 && actCols > 1 && sec.m_colBreak.get()) {
      if (!m_listener->isSectionOpened()) {
        MWAW_DEBUG_MSG(("MSWTextStyles::setProperty: section is not opened\n"));
      } else
        m_listener->insertBreak(MWAW_COLUMN_BREAK);
    } else {
      if (m_listener->isSectionOpened())
        m_listener->closeSection();
      if (numCols<=1) m_listener->openSection();
      else {
        // column seems to have equal size
        int colWidth = int((72.0*m_mainParser->pageWidth())/numCols);
        std::vector<int> colSize;
        colSize.resize((size_t) numCols);
        for (int i = 0; i < numCols; i++) colSize[(size_t)i] = colWidth;
        m_listener->openSection(colSize, WPX_POINT);
      }
    }
  }
}

bool MSWTextStyles::sendSection(int id)
{
  if (!m_listener) return true;

  if (id < 0 || id >= int(m_state->m_sectionList.size())) {
    MWAW_DEBUG_MSG(("MSWTextStyles::sendText: can not find new section\n"));
    return false;
  }
  setProperty(m_state->m_sectionList[(size_t) id]);
  return true;
}

////////////////////////////////////////////////////////////
// read the styles
////////////////////////////////////////////////////////////
bool MSWTextStyles::readStyles(MSWEntry &entry)
{
  if (entry.length() < 6) {
    MWAW_DEBUG_MSG(("MSWTextStyles::readStyles: zone seems to short...\n"));
    return false;
  }
  m_state->m_styleFontMap.clear();
  m_state->m_styleParagraphMap.clear();
  entry.setParsed(true);
  ascii().addPos(entry.end());
  ascii().addNote("_");

  long pos = entry.begin();
  libmwaw::DebugStream f;
  m_input->seek(pos, WPX_SEEK_SET);
  f << entry << ":";
  int N = (int) m_input->readLong(2);
  if (N) f << "N?=" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // first find the different zone
  long debPos[4];
  int overOk[3]= {0,30,100}; // name, font, paragraph
  for (int st = 0; st < 3; st++) {
    debPos[st] = m_input->tell();
    int dataSz = (int) m_input->readULong(2);
    long endPos = debPos[st]+dataSz;
    if (dataSz < 2+N || endPos > entry.end()+overOk[st]) {
      ascii().addPos(pos);
      ascii().addNote("###Styles(bad)");
      MWAW_DEBUG_MSG(("MSWTextStyles::readStyles: can not read styles(%d)...\n", st));
      return false;
    }
    if (endPos > entry.end()) {
      entry.setEnd(endPos+1);
      MWAW_DEBUG_MSG(("MSWTextStyles::readStyles(%d): size seems incoherent...\n", st));
      f.str("");
      f << "#sz=" << dataSz << ",";
      ascii().addPos(debPos[st]);
      ascii().addNote(f.str().c_str());
    }
    m_input->seek(endPos, WPX_SEEK_SET);
  }
  debPos[3] = m_input->tell();
  // read the styles parents
  std::vector<int> previous, order;
  if (readStylesHierarchy(entry, N, previous))
    order=orderStyles(previous);

  int N1=0;
  MSWEntry zone;
  zone.setBegin(debPos[0]);
  zone.setEnd(debPos[1]);
  if (!readStylesNames(zone, N, N1)) {
    N1=int(previous.size())-N;
    if (N1 < 0)
      return false;
  }
  // ok, repair previous and order if need
  if (int(previous.size()) < N+N1)
    previous.resize(size_t(N+N1), -1000);
  if (int(order.size()) < N+N1) {
    for (int i = int(order.size()); i < N+N1; i++)
      order.push_back(i);
  }
  zone.setBegin(debPos[1]);
  zone.setEnd(debPos[2]);
  readStylesFont(zone, N, previous, order);

  zone.setBegin(debPos[2]);
  zone.setEnd(debPos[3]);
  readStylesParagraph(zone, N, previous, order);
  return true;
}

bool MSWTextStyles::readStylesNames(MSWEntry const &zone, int N, int &Nnamed)
{
  long pos = zone.begin();
  m_input->seek(pos+2, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Styles(names):";
  int actN=0;
  while (long(m_input->tell()) < zone.end()) {
    int sz = (int) m_input->readULong(1);
    if (sz == 0) {
      f << "*";
      actN++;
      continue;
    }
    if (sz == 0xFF) {
      f << "_";
      actN++;
      continue;
    }
    pos = m_input->tell();
    if (pos+sz > zone.end()) {
      MWAW_DEBUG_MSG(("MSWTextStyles::readStylesNames: zone(names) seems to short...\n"));
      f << "#";
      ascii().addNote(f.str().c_str());
      m_input->seek(pos-1, WPX_SEEK_SET);
      break;
    }
    std::string s("");
    for (int i = 0; i < sz; i++) s += char(m_input->readULong(1));
    f << "N" << actN-N << "=" ;
    f << s << ",";
    actN++;
  }
  Nnamed=actN-N;
  if (Nnamed < 0) {
    MWAW_DEBUG_MSG(("MSWTextStyles::readStylesNames: zone(names) seems to short: stop...\n"));
    f << "#";
  }
  ascii().addPos(zone.begin());
  ascii().addNote(f.str().c_str());
  return Nnamed >= 0;
}

bool MSWTextStyles::readStylesFont
(MSWEntry &zone, int N, std::vector<int> const &previous, std::vector<int> const &order)
{
  libmwaw::DebugStream f;
  long pos = zone.begin();
  ascii().addPos(pos);
  ascii().addNote("Styles(font):");

  m_input->seek(pos+2, WPX_SEEK_SET);
  size_t numElt = order.size();
  std::vector<long> debPos;
  std::vector<int> dataSize;
  debPos.resize(numElt, 0);
  dataSize.resize(numElt, 0);
  for (size_t i = 0; i < numElt; i++) {
    pos = m_input->tell();
    debPos[i] = pos;
    int sz = dataSize[i] = (int) m_input->readULong(1);
    if (sz == 0xFF)
      sz = 0;
    else if (pos+1+sz > zone.end()) {
      MWAW_DEBUG_MSG(("MSWTextStyles::readStylesFont: can not read a font\n"));
      if (i == 0)
        return false;
      numElt = i-1;
      break;
    }
    if (sz)
      m_input->seek(sz, WPX_SEEK_CUR);
    else {
      f.str("");
      f << "CharPLC(sF" << int(i)-N << "):";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
    }
  }

  for (size_t i = 0; i < order.size(); i++) {
    int id = order[i];
    if (id < 0 || id >= int(numElt)) continue;
    int prevId = previous[(size_t)id];
    MSWStruct::Font font;
    if (prevId >= 0 && m_state->m_styleFontMap.find(prevId-N) != m_state->m_styleFontMap.end())
      font = m_state->m_styleFontMap.find(prevId-N)->second;
    if (dataSize[(size_t)id] && dataSize[(size_t)id] != 0xFF) {
      m_input->seek(debPos[(size_t)id], WPX_SEEK_SET);

      f.str("");
      f << "CharPLC(sF" << id-int(N) << "):";
      if (!readFont(font, StyleZone)) f << "#";
      f << "font=[" << font.m_font->getDebugString(m_convertissor) << font << "],";
      ascii().addPos(debPos[(size_t)id]);
      ascii().addNote(f.str().c_str());
    }
    m_state->m_styleFontMap.insert(std::map<int,MSWStruct::Font>::value_type(id-N,font));
  }
  return true;
}

bool MSWTextStyles::readStylesParagraph(MSWEntry &zone, int N, std::vector<int> const &previous,
                                        std::vector<int> const &order)
{
  int const vers=version();
  int minSz = vers <= 3 ? 3 : 7;
  libmwaw::DebugStream f;
  long pos = zone.begin();
  ascii().addPos(pos);
  ascii().addNote("Styles(paragraph):");

  m_input->seek(pos+2, WPX_SEEK_SET);
  size_t numElt = order.size();
  std::vector<long> debPos;
  std::vector<int> dataSize;
  debPos.resize(numElt, 0);
  dataSize.resize(numElt, 0);
  for (size_t i = 0; i < numElt; i++) {
    pos = m_input->tell();
    debPos[i] = pos;
    int sz = dataSize[i] = (int) m_input->readULong(1);
    if (sz != 0xFF && pos+1+sz > zone.end()) {
      MWAW_DEBUG_MSG(("MSWTextStyles::readStylesParagraph: can not read a paragraph\n"));
      if (i == 0)
        return false;
      numElt = i-1;
      break;
    }
    if (sz && sz != 0xFF)
      m_input->seek(sz, WPX_SEEK_CUR);
    else {
      f.str("");
      f << "ParagPLC(sP" << int(i)-N << "):";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
    }
  }
  for (size_t i = 0; i < order.size(); i++) {
    int id = order[i];
    if (id < 0 || id >= int(numElt)) continue;
    int prevId = previous[(size_t) id];
    MSWStruct::Paragraph para(vers);
    if (prevId >= 0 && m_state->m_styleParagraphMap.find(prevId-N) != m_state->m_styleParagraphMap.end())
      para = m_state->m_styleParagraphMap.find(prevId-N)->second;
    if (m_state->m_styleFontMap.find(id-N) != m_state->m_styleFontMap.end())
      para.m_font = m_state->m_styleFontMap.find(id-N)->second;
    if (dataSize[(size_t) id] == 0 || dataSize[(size_t) id] == 0xFF) {
    } else {
      f.str("");
      f << "ParagPLC(sP" << id-N << "):";
      if (dataSize[(size_t) id] < minSz) {
        MWAW_DEBUG_MSG(("MSWTextStyles::readStylesParagraph: zone(paragraph) the id seems bad...\n"));
        f << "#";
      } else {
        m_input->seek(debPos[(size_t) id]+1, WPX_SEEK_SET);
        int pId = (int) m_input->readLong(1);
        if (id >= N && pId != id-N) {
          MWAW_DEBUG_MSG(("MSWTextStyles::readStylesParagraph: zone(paragraph) the id seems bad...\n"));
          f << "#id=" << pId << ",";
        }
        for (int j = 0; j < 3; j++) { // 0, 0|c,0|1
          int val = (int) m_input->readLong(2);
          if (val) f << "g" << j << "=" << val << ",";
          if (vers <= 3) break;
        }
        if (dataSize[(size_t) id] != minSz && !readParagraph(para, dataSize[(size_t) id]-minSz))
          f << "#";
#ifdef DEBUG_WITH_FILES
        para.print(f, m_convertissor);
#endif
      }
      ascii().addPos(debPos[(size_t) id]);
      ascii().addNote(f.str().c_str());
    }
    m_state->m_styleParagraphMap.insert
    (std::map<int,MSWStruct::Paragraph>::value_type(id-N,para));
  }
  return true;
}

bool MSWTextStyles::readStylesHierarchy(MSWEntry &entry, int N, std::vector<int> &previous)
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;
  f << "Styles(hierarchy):";

  int N2 = (int) m_input->readULong(2);
  if (N2 < N) {
    MWAW_DEBUG_MSG(("MSWTextStyles::readStylesHierarchy: N seems too small...\n"));
    f << "#N=" << N2 << ",";
    ascii().addPos(pos);
    ascii().addNote("Styles(hierarchy):#"); // big problem
    return false;
  }
  if (pos+(N2+1)*2 > entry.end()) {
    if (N2>40) {
      MWAW_DEBUG_MSG(("MSWTextStyles::readStylesHierarchy: N seems very big...\n"));
      ascii().addPos(pos);
      ascii().addNote("Styles(hierarchy):#"); // big problem
    }
    f << "#";
  }
  previous.resize(0);
  previous.resize((size_t) N2, -1000);
  for (int i = 0; i < N2; i++) {
    int v0 = (int) m_input->readLong(1); // often 0 or i-N
    int v1 = (int) m_input->readLong(1);
    f << "prev(sP"<< i-N << ")";
    if (v1 == -34) {
    } else if (v1 < -N || v1+N >= N2)
      f << "=###" << v1;
    else {
      previous[(size_t) i] = v1+N;
      f << "=sP" << v1;
    }
    if (v0 ==0)
      f << ",";
    else if (v0==i-N)
      f << "*,";
    else if (v0)
      f << "[" << v0 << "],";
  }

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = m_input->tell();
  if (pos < entry.end()) {
    ascii().addPos(pos);
    ascii().addNote("_");
  } else if (pos > entry.end())
    entry.setEnd(pos);

  return true;
}

std::vector<int> MSWTextStyles::orderStyles(std::vector<int> const &previous)
{
  std::vector<int> order, numChild;
  size_t N = previous.size();
  numChild.resize(N, 0);
  for (size_t i = 0; i < N; i++) {
    if (previous[i] == -1000) continue;
    if (previous[i] < 0 || previous[i] >= int(N)) {
      MWAW_DEBUG_MSG(("MSWTextStyles::orderStyles: find a bad previous %d\n", previous[i]));
      continue;
    }
    numChild[(size_t) previous[i]]++;
  }
  order.resize(N);
  size_t numElt = 0;
  while (numElt < N) {
    bool read = false;
    for (size_t i = 0; i < N; i++) {
      if (numChild[i]) continue;
      order[N-(++numElt)]=int(i);
      if (previous[i] >= 0 && previous[i] < int(N))
        numChild[(size_t) previous[i]]--;
      read = true;
      numChild[i]=-1;
    }
    if (read) continue;
    MWAW_DEBUG_MSG(("MSWTextStyles::orderStyles: find a loop, stop...\n"));
    for (size_t i = 0; i < N; i++) {
      if (numChild[i] != -1)
        order[N-(++numElt)]=int(i);
    }
    break;
  }
  return order;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
