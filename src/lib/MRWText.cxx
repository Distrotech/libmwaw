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

#include "MRWParser.hxx"

#include "MRWText.hxx"

/** Internal: the structures of a MRWText */
namespace MRWTextInternal
{
////////////////////////////////////////
//! Internal: struct used to store the font of a MRWText
struct Font {
  //! constructor
  Font() : m_font(), m_localId(-1), m_tokenId(0), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Font const &font);

  //! the font
  MWAWFont m_font;
  //! the local id
  int m_localId;
  //! the token id
  long m_tokenId;
  //! extra data
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, Font const &font)
{
  if (font.m_localId >= 0)
    o << "FN" << font.m_localId << ",";
  if (font.m_tokenId > 0)
    o << "tokId=" << std::hex << font.m_tokenId << std::dec << ",";
  o << font.m_extra;
  return o;
}

////////////////////////////////////////
//! Internal: struct used to store the paragraph of a MRWText
struct Paragraph : public MWAWParagraph {
  //! constructor
  Paragraph() : MWAWParagraph(), m_colWidth(0) {
    for (int i = 0; i < 2; i++)
      m_backColor[i] = MWAWColor::white();
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Paragraph const &para);
  //! a column width
  int m_colWidth;
  //! two color
  MWAWColor m_backColor[2];
};

std::ostream &operator<<(std::ostream &o, Paragraph const &para)
{
  o << reinterpret_cast<MWAWParagraph const &>(para);
  if (para.m_colWidth)
    o << "colWidth=" << para.m_colWidth << ",";
  for (int i = 0; i < 2; i++) {
    if (!para.m_backColor[i].isWhite())
      o << "backColor" << i << "=" << para.m_backColor[i] << ",";
  }
  return o;
}
////////////////////////////////////////
//! Internal: struct used to store zone data of a MRWText
struct Zone {
  struct Information;

  //! constructor
  Zone() : m_infoList(), m_fontList(),m_rulerList(), m_idFontMap(), m_posFontMap(), m_posRulerMap(), m_actZone(0), m_parsed(false) {
  }

  //! returns a fonts corresponding to an id (if possible)
  bool getFont(int id, Font &ft) {
    if (id < 0 || id >= int(m_fontList.size())) {
      MWAW_DEBUG_MSG(("MRWTextInternal::Zone::getFont: can not find font %d\n", id));
      return false;
    }
    ft = m_fontList[size_t(id)];
    if (m_idFontMap.find(ft.m_localId) == m_idFontMap.end()) {
      MWAW_DEBUG_MSG(("MRWTextInternal::Zone::getFont: can not find font id %d\n", id));
    } else
      ft.m_font.setId(m_idFontMap[ft.m_localId]);
    return true;
  }
  //! returns a ruler corresponding to an id (if possible)
  bool getRuler(int id, Paragraph &ruler) {
    if (id < 0 || id >= int(m_rulerList.size())) {
      MWAW_DEBUG_MSG(("MRWTextInternal::Zone::getParagraph: can not find paragraph %d\n", id));
      return false;
    }
    ruler = m_rulerList[size_t(id)];
    return true;
  }
  //! the list of information of the text in the file
  std::vector<Information> m_infoList;
  //! a list of font
  std::vector<Font> m_fontList;
  //! a list of ruler
  std::vector<Paragraph> m_rulerList;
  //! a map id -> fontId
  std::map<int,int> m_idFontMap;
  //! a map pos -> fontId
  std::map<long,int> m_posFontMap;
  //! a map pos -> rulerId
  std::map<long,int> m_posRulerMap;
  //! a index used to know the next zone in MRWText::readZone
  int m_actZone;
  //! a flag to know if the zone is parsed
  bool m_parsed;

  //! struct used to keep the information of a small zone of MRWTextInternal::Zone
  struct Information {
    //! constructor
    Information() : m_pos(), m_cPos(0,0), m_extra("") { }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Information const &info) {
      if (info.m_cPos[0] || info.m_cPos[1])
        o << "cPos=" << std::hex << info.m_cPos << std::dec << ",";
      o << info.m_extra;
      return o;
    }

    //! the file position
    MWAWEntry m_pos;
    //! the characters positions
    Vec2l m_cPos;
    //! extra data
    std::string m_extra;
  };
};

////////////////////////////////////////
//! Internal: the state of a MRWText
struct State {
  //! constructor
  State() : m_version(-1), m_textZoneMap(), m_numPages(-1), m_actualPage(0) {
  }

  //! return a reference to a textzone ( if zone not exists, created it )
  Zone &getZone(int id) {
    std::map<int,Zone>::iterator it = m_textZoneMap.find(id);
    if (it != m_textZoneMap.end())
      return it->second;
    it = m_textZoneMap.insert
         (std::map<int,Zone>::value_type(id,Zone())).first;
    return it->second;
  }
  //! the file version
  mutable int m_version;
  //! a map id -> textZone
  std::map<int,Zone> m_textZoneMap;
  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MRWText::MRWText(MWAWInputStreamPtr ip, MRWParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert),
  m_state(new MRWTextInternal::State), m_mainParser(&parser), m_asciiFile(parser.ascii())
{
}

MRWText::~MRWText()
{
}

int MRWText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_mainParser->version();
  return m_state->m_version;
}

int MRWText::numPages() const
{
  if (m_state->m_numPages <= 0) {
    int nPages = 0;
    std::map<int,MRWTextInternal::Zone>::const_iterator it = m_state->m_textZoneMap.begin();
    for ( ; it != m_state->m_textZoneMap.end(); it++) {
      nPages = computeNumPages(it->second);
      if (nPages)
        break;
    }
    m_state->m_numPages=nPages ? nPages : 1;
  }
  return m_state->m_numPages;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//     Text
////////////////////////////////////////////////////////////
bool MRWText::readZone(MRWEntry const &entry, int zoneId)
{
  if (entry.length() < 0x3) {
    MWAW_DEBUG_MSG(("MRWText::readZone: data seems to short\n"));
    return false;
  }

  m_input->seek(entry.begin(), WPX_SEEK_SET);
  m_input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList);
  m_input->popLimit();

  if (dataList.size() != 1) {
    MWAW_DEBUG_MSG(("MRWText::readZone: can find my data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  f << entry.name() << "[data]:";
  MRWStruct const &data = dataList[0];
  if (data.m_type) {
    MWAW_DEBUG_MSG(("MRWText::readZone: find unexpected type zone\n"));
    return false;
  }

  MRWTextInternal::Zone &zone = m_state->getZone(zoneId);
  if (zone.m_actZone < 0 || zone.m_actZone >= int(zone.m_infoList.size())) {
    MWAW_DEBUG_MSG(("MRWText::readZone: actZone seems bad\n"));
    if (zone.m_actZone < 0)
      zone.m_actZone = int(zone.m_infoList.size());
    zone.m_infoList.resize(size_t(zone.m_actZone)+1);
  }
  MRWTextInternal::Zone::Information &info
    = zone.m_infoList[size_t(zone.m_actZone++)];
  info.m_pos = data.m_pos;

  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  m_input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

int MRWText::computeNumPages(MRWTextInternal::Zone const &zone) const
{
  int nPages = 0;
  long pos = m_input->tell();
  for (size_t z=0; z < zone.m_infoList.size(); z++) {
    MRWTextInternal::Zone::Information const &info=zone.m_infoList[z];
    if (!info.m_pos.valid()) continue;
    if (nPages==0) nPages=1;
    m_input->seek(info.m_pos.begin(), WPX_SEEK_SET);
    long numChar = info.m_pos.length();
    while (numChar-- > 0) {
      if (m_input->readULong(1)==0xc)
        nPages++;
    }
  }

  m_input->seek(pos, WPX_SEEK_SET);
  return nPages;
}

bool MRWText::readTextStruct(MRWEntry const &entry, int zoneId)
{
  if (entry.length() < entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readTextStruct: data seems to short\n"));
    return false;
  }
  m_input->seek(entry.begin(), WPX_SEEK_SET);
  m_input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList, 1+22*entry.m_N);
  m_input->popLimit();

  if (int(dataList.size()) != 22*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readTextStruct: find unexpected number of data\n"));
    return false;
  }

  MRWTextInternal::Zone &zone = m_state->getZone(zoneId);
  libmwaw::DebugStream f;
  size_t d = 0;
  for (int i = 0; i < entry.m_N; i++) {
    MRWTextInternal::Zone::Information info;
    ascii().addPos(dataList[d].m_filePos);

    int posi[4]= {0,0,0,0};
    int dim[4]= {0,0,0,0}, dim2[4]= {0,0,0,0};
    f.str("");
    for (int j = 0; j < 22; j++) {
      MRWStruct const &data = dataList[d++];
      if (!data.isBasic()) {
        f << "###f" << j << "=" << data << ",";
        continue;
      }
      switch(j) {
      case 0:
      case 1:
        info.m_cPos[j] = data.value(0);
        break;
      case 2:
        if (data.value(0))
          f << "fl0=" << std::hex << data.value(0) << std::dec << ",";
        break;
      case 3:
      case 4:
      case 5:
      case 6:
        posi[j-3]=(int) data.value(0);
        while (j < 6)
          posi[++j-3]=(int) dataList[d++].value(0);
        if (posi[0] || posi[1])
          f << "pos0=" << posi[0] << ":" << posi[1] << ",";
        if (posi[2] || posi[3])
          f << "pos1=" << posi[2] << ":" << posi[3] << ",";
        break;
      case 8:
      case 9:
      case 10:
      case 11:
        dim[j-8]=(int) data.value(0);
        while (j < 11)
          dim[++j-8]=(int) dataList[d++].value(0);
        if (dim[0]||dim[1]||dim[2]||dim[3])
          f << "pos2=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
        break;
      case 12:
        if (data.value(0)!=info.m_cPos[1]-info.m_cPos[0])
          f << "nChar(diff)=" << info.m_cPos[1]-info.m_cPos[0]-data.value(0) << ",";
        break;
      case 14: // a small number(cst) in the list of structure
      case 16: // an id?
        if (data.value(0))
          f << "f" << j << "=" << data.value(0) << ",";
        break;
      case 15: // 0xcc00|0xce40|... some flags? a dim
        if (data.value(0))
          f << "f" << j << "=" << std::hex << data.value(0) << std::dec << ",";
        break;
      case 17:
      case 18:
      case 19:
      case 20:
        dim2[j-17]=(int) data.value(0);
        while (j < 20)
          dim2[++j-17]=(int) dataList[d++].value(0);
        if (dim2[0]||dim2[1]||dim2[2]||dim2[3])
          f << "pos3=" << dim2[1] << "x" << dim2[0] << "<->" << dim2[3] << "x" << dim2[2] << ",";
        break;
      default:
        if (data.value(0))
          f << "#f" << j << "=" << data.value(0) << ",";
        break;
      }
    }
    info.m_extra = f.str();
    zone.m_infoList.push_back(info);
    f.str("");
    f << entry.name() << "-" << i << ":" << info;
    ascii().addNote(f.str().c_str());
  }
  m_input->seek(entry.end(), WPX_SEEK_SET);

  return true;
}

bool MRWText::send(int zoneId)
{
  if (!m_listener) {
    MWAW_DEBUG_MSG(("MRWText::send: can not find the listener\n"));
    return false;
  }
  if (m_state->m_textZoneMap.find(zoneId) == m_state->m_textZoneMap.end()) {
    MWAW_DEBUG_MSG(("MRWText::send: can not find the text zone %d\n", zoneId));
    return false;
  }
  MRWTextInternal::Zone &zone = m_state->getZone(zoneId);
  zone.m_parsed = true;

  long actChar = 0;
  MRWTextInternal::Font actFont;
  setProperty(actFont.m_font);
  int actPage = 1;
  if (zoneId==0)
    m_mainParser->newPage(actPage);
  for (size_t z = 0; z < zone.m_infoList.size(); z++) {
    long pos = zone.m_infoList[z].m_pos.begin();
    long endPos = zone.m_infoList[z].m_pos.end();
    m_input->seek(pos, WPX_SEEK_SET);

    libmwaw::DebugStream f;
    f << "Text[" << std::hex << actChar << std::dec << "]:";
    int tokenEndPos = 0;
    while (!m_input->atEOS()) {
      long actPos = m_input->tell();
      if (actPos == endPos)
        break;
      if (zone.m_posRulerMap.find(actChar)!=zone.m_posRulerMap.end()) {
        int id = zone.m_posRulerMap.find(actChar)->second;
        f << "[P" << id << "]";
        MRWTextInternal::Paragraph para;
        if (zone.getRuler(id, para))
          setProperty(para);
      }
      if (zone.m_posFontMap.find(actChar)!=zone.m_posFontMap.end()) {
        int id = zone.m_posFontMap.find(actChar)->second;
        f << "[F" << id << "]";
        MRWTextInternal::Font font;
        if (zone.getFont(id, font)) {
          actFont = font;
          setProperty(font.m_font);
          if (font.m_tokenId > 0) {
            m_mainParser->sendToken(zoneId, font.m_tokenId);
            tokenEndPos = -2;
          }
        }
      }

      char c = (char) m_input->readULong(1);
      actChar++;
      if (tokenEndPos) {
        if ((c=='[' && tokenEndPos==-2)||(c==']' && tokenEndPos==-1)) {
          tokenEndPos++;
          f << c;
          continue;
        }
        tokenEndPos++;
        MWAW_DEBUG_MSG(("MRWText::send: find unexpected char for a token\n"));
      }
      switch(c) {
      case 0x6: {
        static bool first = true;
        if (first) {
          MWAW_DEBUG_MSG(("MRWText::send: find some table: unimplemented\n"));
          first = false;
        }
        f << "#";
        m_listener->insertEOL();
        break;
      }
      case 0x7: // fixme end of cell
        f << "#";
        m_listener->insertCharacter(' ');
        break;
      case 0x9:
        m_listener->insertTab();
        break;
      case 0xa:
        m_listener->insertEOL(true);
        break;
      case 0xc:
        m_mainParser->newPage(++actPage);
        break;
      case 0xd:
        m_listener->insertEOL();
        break;
        // some special character
      case 0x11:
        m_listener->insertUnicode(0x2318);
        break;
      case 0x12:
        m_listener->insertUnicode(0x2713);
        break;
      case 0x14:
        m_listener->insertUnicode(0xF8FF);
        break;
      case 0x1f: // soft hyphen, ignore
        break;
      default: {
        int unicode = m_convertissor->unicode (actFont.m_font.id(), (unsigned char) c);
        if (unicode == -1) {
          if (c >= 0 && c < 0x20) {
            MWAW_DEBUG_MSG(("MRWText::sendText: Find odd char %x\n", int(c)));
            f << "#";
          } else
            m_listener->insertCharacter((uint8_t) c);
        } else
          m_listener->insertUnicode((uint32_t) unicode);
      }
      }

      f << c;
      if (c==0xa || c==0xd || actPos==endPos-1) {
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());

        pos = actPos;
        f.str("");
        f << "Text:";
      }
    }
  }

  return true;
}

bool MRWText::readPLCZone(MRWEntry const &entry, int zoneId)
{
  if (entry.length() < 2*entry.m_N-1) {
    MWAW_DEBUG_MSG(("MRWText::readPLCZone: data seems to short\n"));
    return false;
  }

  m_input->seek(entry.begin(), WPX_SEEK_SET);
  m_input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList,1+2*entry.m_N);
  m_input->popLimit();

  if (int(dataList.size()) != 2*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readPLCZone: find unexpected number of data\n"));
    return false;
  }

  MRWTextInternal::Zone &zone = m_state->getZone(zoneId);
  bool isCharZone = entry.m_fileType==4;
  std::map<long,int> &map = isCharZone ? zone.m_posFontMap : zone.m_posRulerMap;
  libmwaw::DebugStream f;
  long pos = entry.begin();
  for (size_t d=0; d < dataList.size(); d+=2) {
    if (d%40==0) {
      if (d) {
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
      }
      f.str("");
      f << entry.name() << ":";
      pos = dataList[d].m_filePos;
    }
    long cPos = dataList[d].value(0);
    int id = (int) dataList[d+1].value(0);
    map[cPos] = id;
    f << std::hex << cPos << std::dec; //pos
    if (isCharZone)
      f << "(F" << id << "),"; // id
    else
      f << "(P" << id << "),"; // id
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  m_input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//     Fonts
////////////////////////////////////////////////////////////
void MRWText::setProperty(MWAWFont const &font)
{
  if (!m_listener) return;
  MWAWFont aFont;
  font.sendTo(m_listener.get(), m_convertissor, aFont);
}

bool MRWText::readFontNames(MRWEntry const &entry, int zoneId)
{
  if (entry.length() < entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readFontNames: data seems to short\n"));
    return false;
  }

  m_input->seek(entry.begin(), WPX_SEEK_SET);
  m_input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList,1+19*entry.m_N);
  m_input->popLimit();

  if (int(dataList.size()) != 19*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readFontNames: find unexpected number of data\n"));
    return false;
  }

  MRWTextInternal::Zone &zone = m_state->getZone(zoneId);
  libmwaw::DebugStream f;
  size_t d = 0;
  int val;
  for (int i = 0; i < entry.m_N; i++) {
    f.str("");
    f << entry.name() << "-FN" << i << ":";
    ascii().addPos(dataList[d].m_filePos);
    std::string fontName("");
    for (int j = 0; j < 2; j++, d++) {
      MRWStruct const &data = dataList[d];
      if (data.m_type!=0 || !data.m_pos.valid()) {
        MWAW_DEBUG_MSG(("MRWText::readFontNames: name %d seems bad\n", j));
        f << "###" << data << ",";
        continue;
      }
      long pos = data.m_pos.begin();
      m_input->seek(pos, WPX_SEEK_SET);
      int fSz = int(m_input->readULong(1));
      if (fSz+1 > data.m_pos.length()) {
        MWAW_DEBUG_MSG(("MRWText::readFontNames: field name %d seems bad\n", j));
        f << data << "[###fSz=" << fSz << ",";
        continue;
      }
      std::string name("");
      for (int c = 0; c < fSz; c++)
        name+=(char) m_input->readULong(1);
      if (j == 0) {
        fontName = name;
        f << name << ",";
      } else
        f << "nFont=" << name << ",";
    }
    val = (int) dataList[d++].value(0);
    if (val != 4) // always 4
      f << "f0=" << val << ",";
    val = (int) dataList[d++].value(0);
    if (val) // always 0
      f << "f1=" << val << ",";
    int fId = (int) (uint16_t) dataList[d++].value(0);
    f << "fId=" << fId << ",";
    for (int j = 5; j < 19; j++) { // f14=1,f15=0|3
      MRWStruct const &data = dataList[d++];
      if (data.m_type==0 || data.numValues() > 1)
        f << "f" << j-3 << "=" << data << ",";
      else if (data.value(0))
        f << "f" << j-3 << "=" << data.value(0) << ",";
    }
    if (fontName.length())
      m_convertissor->setCorrespondance(fId, fontName);
    zone.m_idFontMap[i] = fId;
    ascii().addNote(f.str().c_str());
  }
  m_input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

bool MRWText::readFonts(MRWEntry const &entry, int zoneId)
{
  if (entry.length() < entry.m_N+1) {
    MWAW_DEBUG_MSG(("MRWText::readFonts: data seems to short\n"));
    return false;
  }

  m_input->seek(entry.begin(), WPX_SEEK_SET);
  m_input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList,1+1+77*entry.m_N);
  m_input->popLimit();

  if (int(dataList.size()) != 1+77*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readFonts: find unexpected number of data\n"));
    return false;
  }

  MRWTextInternal::Zone &zone = m_state->getZone(zoneId);
  libmwaw::DebugStream f;
  f << entry.name() << ":unkn=" << dataList[0].value(0);
  ascii().addPos(dataList[0].m_filePos);
  ascii().addNote(f.str().c_str());

  size_t d = 1;
  for (int i = 0; i < entry.m_N; i++) {
    MRWTextInternal::Font font;
    f.str("");
    ascii().addPos(dataList[d].m_filePos);

    size_t fD = d;
    long val;
    uint32_t fFlags=0;
    for (int j = 0; j < 77; j++) {
      MRWStruct const &dt = dataList[d++];
      if (!dt.isBasic()) continue;
      unsigned char color[3];
      switch (j) {
      case 0:
        font.m_localId=(int) dt.value(0);
        break;
      case 1: // 0|1
      case 2: // 0
      case 3: // small number
      case 4: // 0|..|10
      case 5: // 0|1|2
      case 6: // 0
      case 8: // small number or 1001,1002
      case 17: // small number between 4 && 39
      case 19: // 0
      case 20: // 0|1|3|5
      case 21: // 0
      case 22: // 0
      case 30: // small number between 0 or 0x3a9 ( a dim?)
      case 59: // 0|9 ( associated with f74?)
      case 66: // 0|1
      case 74: // 0|1|2|3
        if (dt.value(0))
          f << "f" << j << "=" << dt.value(0) << ",";
        break;
      case 7:  // 0, 1000, 100000, 102004
        if (dt.value(0))
          f << "fl0=" << std::hex << dt.value(0) << std::dec << ",";
        break;
      case 9:
      case 10:
      case 11:
        color[0]=color[1]=color[2]=0;
        color[j-9]=(unsigned char) (dt.value(0)>>8);
        while (j < 11)
          color[++j-9] = (unsigned char) (dataList[d++].value(0));
        font.m_font.setColor(MWAWColor(color[0],color[1],color[2]));
        break;
      case 12: // big number
      case 16: // 0 or one time 0x180
        if (dt.value(0))
          f << "f" << j << "=" << std::hex << dt.value(0) << std::dec << ",";
        break;
      case 13:
      case 14:
      case 15:
        color[0]=color[1]=color[2]=0xFF;
        color[j-13]=(unsigned char) (dt.value(0)>>8);
        while (j < 15)
          color[++j-13] = (unsigned char) (dataList[d++].value(0)>>8);
        font.m_font.setBackgroundColor(MWAWColor(color[0],color[1],color[2]));
        break;
      case 18: {
        val = dt.value(0);
        font.m_font.setSize(int(val>>16));
        if (val & 0xFFFF)
          f << "#fSz[high]=" << (val & 0xFFFF) << ",";
        break;
      }
      case 23: { // f23[high]=0|1
        int low = (int16_t)(dt.value(0));
        if (low) f << "f" << j << "[low]=" << low << ",";
        int high = (int16_t)(dt.value(0)>>16);
        if (high) f << "f" << j << "[high]=" << high << ",";
        break;
      }
      case 24: // 0
      case 25: // 0
      case 27: // 0
      case 29: // 0
      case 32: // 0
      case 33: // 0
      case 35: // 0
        if (dt.value(0))
          f << "f" << j << "=" << dt.value(0) << ",";
        break;
      case 26: // 0|very big number id f28
      case 28: // 0|very big number
      case 31: // 0|very big number
      case 34: // an id?
        if (!dt.value(0))
          break;
        switch(j) {
        case 28:
          f << "idA";
          break;
        case 31:
          f << "idB";
          break;
        default:
          f << "f" << j;
          break;
        }
        f << "=" << std::hex << int32_t(dt.value(0)) << std::dec << ",";
        break;
        // tokens
      case 36: // 0|1|6 related to token?
        if (dt.value(0))
          f << "tok0=" << dt.value(0) << ",";
      case 37: // another id?
        if (dt.value(0))
          f << "tok1=" << std::hex << dt.value(0) << std::dec << ",";
        break;
      case 38:
        font.m_tokenId = dt.value(0);
        break;
      case 39:
      case 40:
      case 41:
      case 42:
      case 43:
      case 44:
      case 45:
      case 46:
      case 47:
      case 48:
      case 49:
      case 50:
      case 58:
        if (int16_t(dt.value(0))==-1) {
          switch(j) {
          case 39:
            fFlags |= MWAWFont::boldBit;
            break;
          case 40:
            fFlags |= MWAWFont::italicBit;
            break;
          case 41:
            font.m_font.setUnderlineStyle(MWAWFont::Line::Single);
            break;
          case 42:
            fFlags |= MWAWFont::outlineBit;
            break;
          case 43:
            fFlags |= MWAWFont::shadowBit;
            break;
          case 44:
            f << "condense,";
            break;
          case 45:
            f << "expand,";
            break;
          case 46:
            font.m_font.setUnderlineStyle(MWAWFont::Line::Double);
            break;
          case 47:
            font.m_font.setUnderlineStyle(MWAWFont::Line::Single);
            f << "underline[word],";
            break;
          case 48:
            font.m_font.setUnderlineStyle(MWAWFont::Line::Dot);
            break;
          case 50:
            font.m_font.setStrikeOutStyle(MWAWFont::Line::Single);
            break;
          case 58:
            f << "boxed" << j-57 << ",";
            break;
          default:
            MWAW_DEBUG_MSG(("MRWText::readFonts: find unknown font flag: %d\n", j));
            f << "#f" << j << ",";
            break;
          }
        } else if (dt.value(0))
          f << "#f" << j << "=" << dt.value(0) << ",";
        break;
      case 51:
        if (dt.value(0) > 0) {
          font.m_font.set(MWAWFont::Script::super100());
          if (dt.value(0) != 3)
            f << "superscript[pos]=" << dt.value(0) << ",";
        } else if (dt.value(0))
          f << "#superscript=" << dt.value(0) << ",";
        break;
      case 52:
        if (dt.value(0) > 0) {
          font.m_font.set(MWAWFont::Script::sub100());
          if (dt.value(0) != 3)
            f << "subscript[pos]=" << dt.value(0) << ",";
        } else if (dt.value(0))
          f << "#subscript=" << dt.value(0) << ",";
        break;
      default:
        if (dt.value(0))
          f << "#f" << j << "=" << dt.value(0) << ",";
        break;
      }
    }
    font.m_font.setFlags(fFlags);
    // the error
    d = fD;
    for (int j = 0; j < 77; j++, d++) {
      if (!dataList[d].isBasic())
        f << "#f" << j << "=" << dataList[d] << ",";
    }
    zone.m_fontList.push_back(font);

    font.m_extra = f.str();
    f.str("");
    f << entry.name() << "-F" << i << ":"
      << font.m_font.getDebugString(m_convertissor) << font;
    ascii().addNote(f.str().c_str());
  }
  m_input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//     Style
////////////////////////////////////////////////////////////
bool MRWText::readStyleNames(MRWEntry const &entry, int)
{
  if (entry.length() < entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readStyleNames: data seems to short\n"));
    return false;
  }

  m_input->seek(entry.begin(), WPX_SEEK_SET);
  m_input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList,1+2*entry.m_N);
  m_input->popLimit();

  if (int(dataList.size()) != 2*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readStyleNames: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  size_t d = 0;
  for (int i = 0; i < entry.m_N; i++) {
    f.str("");
    f << entry.name() << "-" << i << ":";
    ascii().addPos(dataList[d].m_filePos);
    if (!dataList[d].isBasic()) {
      MWAW_DEBUG_MSG(("MRWText::readStyleNames: bad id field for style %d\n", i));
      f << "###" << dataList[d] << ",";
    } else
      f << "id=" << dataList[d].value(0) << ",";
    d++;

    std::string name("");
    MRWStruct const &data = dataList[d++];
    if (data.m_type!=0 || !data.m_pos.valid()) {
      MWAW_DEBUG_MSG(("MRWText::readStyleNames: name %d seems bad\n", i));
      f << "###" << data << ",";
    } else {
      long pos = data.m_pos.begin();
      m_input->seek(pos, WPX_SEEK_SET);
      int fSz = int(m_input->readULong(1));
      if (fSz+1 > data.m_pos.length()) {
        MWAW_DEBUG_MSG(("MRWText::readStyleNames: field name %d seems bad\n", i));
        f << data << "[###fSz=" << fSz << ",";
      } else {
        for (int c = 0; c < fSz; c++)
          name+=(char) m_input->readULong(1);
        f << name << ",";
      }
    }
    ascii().addNote(f.str().c_str());
  }
  m_input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//     Paragraph
////////////////////////////////////////////////////////////
void MRWText::setProperty(MRWTextInternal::Paragraph const &ruler)
{
  if (!m_listener) return;
  ruler.send(m_listener);
}

bool MRWText::readRulers(MRWEntry const &entry, int zoneId)
{
  if (entry.length() < entry.m_N+1) {
    MWAW_DEBUG_MSG(("MRWText::readRulers: data seems to short\n"));
    return false;
  }

  m_input->seek(entry.begin(), WPX_SEEK_SET);
  m_input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList,1+3*68*entry.m_N);
  m_input->popLimit();

  int numDatas = int(dataList.size());
  if (numDatas < 68*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readRulers: find unexpected number of data\n"));
    return false;
  }

  MRWTextInternal::Zone &zone = m_state->getZone(zoneId);
  libmwaw::DebugStream f;
  size_t d = 0;
  for (int i = 0; i < entry.m_N; i++) {
    MRWTextInternal::Paragraph para;
    ascii().addPos(dataList[d].m_filePos);

    if (int(d+68) > numDatas) {
      MWAW_DEBUG_MSG(("MRWText::readRulers: ruler %d is too short\n", i));
      f << "###";
      ascii().addNote(f.str().c_str());
      return true;
    }
    size_t fD = d;
    unsigned char color[3];
    f.str("");
    int nTabs = 0;
    for (int j = 0; j < 58; j++, d++) {
      MRWStruct const &dt = dataList[d];
      if (!dt.isBasic()) continue;
      switch(j) {
      case 0:
        switch(dt.value(0)) {
        case 0:
          break;
        case 1:
          para.m_justify = libmwaw::JustificationCenter;
          break;
        case 2:
          para.m_justify = libmwaw::JustificationRight;
          break;
        case 3:
          para.m_justify = libmwaw::JustificationFull;
          break;
        case 4:
          para.m_justify = libmwaw::JustificationFullAllLines;
          break;
        default:
          f << "#justify=" << dt.value(0) << ",";
        }
        break;
      case 3:
      case 4:
        para.m_margins[j-2] = float(dt.value(0))/72.f;
        break;
      case 5:
        para.m_margins[0] = float(dt.value(0))/72.f;
        break;
      case 6:
        if (!dt.value(0)) break;
        para.m_spacings[0] = float(dt.value(0))/65536.f;
        para.m_spacingsInterlineUnit = WPX_PERCENT;
        break;
      case 8:
        if (!dt.value(0)) break;
        para.m_spacings[0] = float(dt.value(0))/65536.f;
        para.m_spacingsInterlineUnit = WPX_POINT;
        break;
      case 10:
        if (!dt.value(0)) break;
        para.m_spacings[1] = float(dt.value(0))/72.f;
        break;
      case 11:
        if (!dt.value(0)) break;
        para.m_spacings[2] = float(dt.value(0))/72.f;
        break;
      case 30:
        if (!dt.value(0)!=0x18) break;
        f << "#f30=" << dt.value(0) << ",";
        break;
      case 1: // always 0
      case 2: // small number between -15 and 15
      case 7: // always 0
      case 9: // always 0
      case 22: // 1|2|4 1=normal?
      case 29: // 0|64
        if (dt.value(0))
          f << "f" << j << "=" << dt.value(0) << ",";
        break;
      case 23: // big number related to fonts f28?
      case 24: // big number related to fonts f31?
        if (dt.value(0))
          f << "id" << char('A'+(j-23)) << "=" << std::hex << int32_t(dt.value(0)) << std::dec << ",";
        break;
      case 35:
      case 36:
      case 37:
        color[0]=color[1]=color[2]=0xFF;
        color[j-35]=(unsigned char) (dt.value(0)>>8);
        while (j < 37)
          color[++j-35] = (unsigned char) (dataList[d++].value(0)>>8);
        para.m_backColor[0] = MWAWColor(color[0],color[1],color[2]);
        break;
      case 40:
        para.m_colWidth = (int) dt.value(0);
        break;
      case 47:
      case 48:
      case 49:
        color[0]=color[1]=color[2]=0xFF;
        color[j-47]=(unsigned char) (dt.value(0)>>8);
        while (j < 49)
          color[++j-47] = (unsigned char) (dataList[d++].value(0)>>8);
        para.m_backColor[1] = MWAWColor(color[0],color[1],color[2]);
        break;
      case 56: { // border type?
        long val = dt.value(0);
        if (!val) break;
        int depl = 24;
        f << "border?=[";
        for (int b = 0; b < 4; b++) {
          if ((val>>depl)&0xFF)
            f << ((val>>depl)&0xFF) << ",";
          else
            f << "_";
          depl -= 8;
        }
        f << "],";
        break;
      }
      case 57:
        nTabs = (int) dt.value(0);
        break;
      default:
        if (dt.value(0))
          f << "#f" << j << "=" << dt.value(0) << ",";
        break;
      }
    }
    //    para.m_margins[0] = para.m_margins[0].get()+para.m_margins[1].get();
    d = fD;
    for (int j = 0; j < 58; j++, d++) {
      if (!dataList[d].isBasic())
        f << "#f" << j << "=" << dataList[d].isBasic() << ",";
    }
    if (nTabs < 0 || int(d)+4*nTabs+10 > numDatas) {
      MWAW_DEBUG_MSG(("MRWText::readRulers: can not read numtabs\n"));
      para.m_extra = f.str();
      f.str("");
      f << entry.name() << "-P" << i << ":" << para << "," << "###";
      ascii().addNote(f.str().c_str());
      zone.m_rulerList.push_back(para);
      return true;
    }
    if (nTabs) {
      for (int j = 0; j < nTabs; j++) {
        MWAWTabStop tab;
        for (int k = 0; k < 4; k++, d++) {
          MRWStruct const &dt = dataList[d];
          if (!dt.isBasic()) {
            f << "#tabs" << j << "[" << k << "]=" << dt << ",";
            continue;
          }
          switch(k) {
          case 0:
            switch(dt.value(0)) {
            case 1:
              break;
            case 2:
              tab.m_alignment = MWAWTabStop::CENTER;
              break;
            case 3:
              tab.m_alignment = MWAWTabStop::RIGHT;
              break;
            case 4:
              tab.m_alignment = MWAWTabStop::DECIMAL;
              break;
            default:
              f << "#tabAlign" << j << "=" << dt.value(0) << ",";
              break;
            }
            break;
          case 1:
            tab.m_position = float(dt.value(0))/72.f;
            break;
          case 2:
            if (dt.value(0))
              tab.m_leaderCharacter = uint16_t(dt.value(0));
            break;
          default:
            if (dt.value(0))
              f << "#tabs" << j << "[" << 3 << "]=" << dt.value(0) << ",";
            break;
          }
        }
        para.m_tabs->push_back(tab);
      }
    }
    for (int j = 0; j < 10; j++, d++) {
      MRWStruct const &dt = dataList[d];
      if (j==1 && dt.m_type==0) { // a block with sz=40
        m_input->seek(dt.m_pos.begin(), WPX_SEEK_SET);
        int fSz = (int) m_input->readULong(1);
        if (fSz+1>dt.m_pos.length()) {
          MWAW_DEBUG_MSG(("MRWText::readRulers: can not read paragraph name\n"));
          f << "#name,";
          continue;
        }
        if (fSz == 0) continue;
        std::string name("");
        for (int k = 0; k < fSz; k++)
          name+=(char) m_input->readULong(1);
        f << "name=" << name << ",";
        continue;
      }
      if (!dt.isBasic() || j==1) {
        f << "#g" << j << "=" << dt << ",";
        continue;
      }
      switch(j) {
      case 0: // always 36 ?
        if (dt.value(0)!=36)
          f << "#g0=" << dt.value(0) << ",";
        break;
      case 4: { // 0|80|1000
        long val = dt.value(0);
        if (!val) break;
        if (val & 0x1000) {
          val &= 0xFFFFEFFF;
          MWAWList::Level theLevel;
          theLevel.m_type = MWAWList::Level::BULLET;
          theLevel.m_labelIndent = para.m_margins[1].get();
          MWAWContentListener::appendUnicode(0x2022, theLevel.m_bullet);
          para.m_listLevelIndex = 1;
          para.m_listLevel=theLevel;
        }
        if (val)
          f << "flag?=" << std::hex << val << std::dec << ",";
        break;
      }
      case 7: // 1|3|de
      case 8: // 0|4 : 4 for header entry?
        if (dt.value(0))
          f << "g" << j << "=" << dt.value(0) << ",";
        break;
      default:
        if (dt.value(0))
          f << "#g" << j << "=" << dt.value(0) << ",";
        break;
      }
    }
    zone.m_rulerList.push_back(para);
    para.m_extra = f.str();
    f.str("");
    f << entry.name() << "-P" << i << ":" << para << ",";
    ascii().addNote(f.str().c_str());
  }
  m_input->seek(entry.end(), WPX_SEEK_SET);
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

void MRWText::flushExtra()
{
  //if (!m_listener) return;
  std::map<int,MRWTextInternal::Zone>::iterator it =
    m_state->m_textZoneMap.begin();
  for ( ; it != m_state->m_textZoneMap.end(); it++) {
    if (it->second.m_parsed)
      continue;
    send(it->first);
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
