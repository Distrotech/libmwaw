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

#include <sstream>
#include <string>

#include "MWAWEntry.hxx"
#include "MWAWInputStream.hxx"

#include "FWStruct.hxx"

namespace FWStruct
{
bool getColor(int color, MWAWColor &col)
{
  if (color==0xFFFF) return false;
  if (color&0x8000) // true color
    col=MWAWColor((unsigned char)(((color>>10)&0x1F)<<3),
                  (unsigned char)(((color>>5)&0x1F)<<3),
                  (unsigned char)((color&0x1F)<<3));
  else if ((color&0x6000)==0x6000) // black
    col=MWAWColor(0,0,0);
  else if ((color&0x4000) || (color>=0&&color <= 100)) { // gray in %
    int val = ((color&0x7F)*255)/100;
    if (val > 255) val = 0;
    else val = 255-val;
    unsigned char c = (unsigned char) val;
    col=MWAWColor(c,c,c);
  }
  else
    return false;
  return true;
}

std::string getTypeName(int type)
{
  switch (type) {
  case 0:
    return "columns,";
  case 1:
    return "tabs,";
  case 2:
    return "item,";
  case 3:
    return "style,";
  case 0xa:
    return "main,";
  case 0xb:
    return "comment,";
  case 0xc:
    return "footnote,";
  case 0xd:
    return "endnote,";
  case 0x10: // checkme
    return "index,";
  case 0x11: // checkme
    return "header,";
  case 0x13:
    return "sidebar,";
  case 0x14:
    return "sidebar[simple],";
  case 0x15:
    return "graphic,";
  case 0x18: // in general empty
    return "variableText,";
  // 13-14: always find with child
  // 0xb, 11-12 can also have child...
  case 0x19:
    return "reference,";
  case 0x1a:
    return "referenceRedirect,";
  case 0x1e:
    return "variableRedirect,";
  case 0x1f:
    return "dataMod,";
  default:
    break;
  }
  std::stringstream s;
  s << "type=" << std::hex << type << std::dec << ",";
  return s.str();
}

////////////////////////////////////////////////////////////
// Border
////////////////////////////////////////////////////////////
MWAWBorder Border::getBorder(int type)
{
  MWAWBorder res;
  if ((type%2)==0)
    res.m_type=MWAWBorder::Double;
  res.m_width=float(type/2);
  return res;
}

void Border::addToFrame(librevenge::RVNGPropertyList &pList) const
{
  if (!m_backColor.isWhite())
    pList.insert("fo:background-color", m_backColor.str().c_str());
  if (hasShadow()) {
    std::stringstream s;
    s << m_shadowColor.str() << " " << 0.03527f*float(m_shadowDepl[0]) << "cm "
      << 0.03527f*float(m_shadowDepl[1]) << "cm";
    pList.insert("style:shadow", s.str().c_str());
  }
  if (m_frameBorder.isEmpty())
    return;
  MWAWBorder bord=m_frameBorder;
  bord.m_color = m_color[0];
  bord.addTo(pList,"");
}

bool Border::read(shared_ptr<FWStruct::Entry> zone, int fSz)
{
  *this=FWStruct::Border();
  if (fSz < 26) {
    MWAW_DEBUG_MSG(("FWStruct::Border::read: find unexpected size\n"));
    return false;
  }
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugStream f;
  long pos = input->tell();

  int width[3];
  int totalW = 0;
  for (int i=0; i < 3; ++i) totalW += (width[i]=(int) input->readLong(1));
  if (width[0]&&width[2]) {
    m_frameBorder.m_style=MWAWBorder::Simple;
    m_frameBorder.m_type=MWAWBorder::Double;
    m_frameBorder.m_width=0.5*double(totalW);
    m_frameBorder.m_widthsList.resize(3);
    for (size_t i=0; i < 3; ++i)
      m_frameBorder.m_widthsList[i]=0.5*(double) width[i];
  }
  else if (!width[0] && !width[1] && width[2]) {
    m_frameBorder.m_style=MWAWBorder::Simple;
    m_frameBorder.m_width=0.5*double(totalW);
  }
  else if (totalW) {
    MWAW_DEBUG_MSG(("FWStruct::Border::read: frame border width seems odd\n"));
    f << "###frame[w]=[";
    for (int i=0; i < 3; ++i) f << width[i] << ",";
    f << "],";
  }
  int val = (int) input->readLong(1);
  if (val)
    m_shadowDepl=Vec2i(val,val);
  val = (int) input->readLong(1);
  if (val) f << "frame[rectCorner]=" << val << ",";
  m_type[0] = (int) input->readLong(1);
  MWAWColor col;
  for (int j = 0; j < 7; j++) {
    val = (int) input->readULong(2);
    if (getColor(val,col)) {
      switch (j) {
      case 1: // border
        m_color[0] = col;
        break;
      case 2:
        m_shadowColor=col;
        break;
      case 3: // separator
        m_color[1] = col;
        break;
      case 4: // = border?
        if (m_color[0] != col)
          f << "#col[border2]=" << col << ",";
        break;
      case 5:
        m_frontColor=col;
        break;
      case 6:
        m_backColor=col;
        break;
      default:
        if (!col.isBlack())
          f << "col" << j << "=" << col << ",";
      }
    }
    else
      f << "#col" << j << "=" << std::hex << val << std::dec << ",";
  }
  for (int j = 0; j < 2; j++) { // g0=g1=0
    val = (int) input->readLong(1);
    if (val) f << "g" << j << "=" << val << ",";
  }
  m_type[1] = (int) input->readLong(1); // sepH
  m_type[2] = (int) input->readLong(1); // sepV
  m_flags = (int) input->readULong(2);
  m_extra = f.str();
  input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
  return true;
}

std::vector<Variable<MWAWBorder> > Border::getParagraphBorders() const
{
  std::vector<Variable<MWAWBorder> > res;
  int wh=-1;
  if (m_type[0]>0 && m_type[0]<=8) wh=0;
  else if (m_type[1]>0 && m_type[1]<=8) wh=1;
  if (wh == -1)
    return res;
  Variable<MWAWBorder> border=getBorder(m_type[wh]);
  border->m_color=m_color[wh];
  if (wh==0)
    res.resize(4,border);
  else {
    res.resize(4);
    res[libmwaw::Bottom]=border;
  }
  return res;
}

std::ostream &operator<<(std::ostream &o, Border const &p)
{
  if (!p.m_frontColor.isBlack())
    o << "frontColor=" << p.m_frontColor << ",";
  if (!p.m_backColor.isWhite())
    o << "backColor=" << p.m_backColor << ",";
  if (p.hasShadow())
    o << "shadow=" << p.m_shadowDepl << "[" << p.m_shadowColor << "],";
  for (int w=0; w < 3; w++) {
    if (!p.m_type[w]) continue;
    if (w==0)
      o << "border=";
    else if (w==1)
      o << "sep[H]=";
    else
      o << "sep[V]=";
    switch (p.m_type[w]) {
    case 0: // none
      break;
    case 1:
      o << "hairline:";
      break;
    case 2:
      o << "hairline double:";
      break;
    case 3:
      o << "normal:";
      break;
    case 4:
      o << "normal double:";
      break;
    case 5:
      o << "2pt:";
      break;
    case 7:
      o << "3pt:";
      break;
    default:
      o << "#type[" << p.m_type[w] << "]:";
    }
    if (w!=2 && !p.m_color[w].isBlack())
      o << "col=" << p.m_color[w] << ",";
    else
      o << ",";
  }
  if (!p.m_frameBorder.isEmpty())
    o << "border[frame]=" << p.m_frameBorder << ",";
  if (p.m_flags & 0x4000)
    o << "setBorder,";
  if (p.m_flags & 0x8000)
    o << "setSeparator,";
  if (p.m_flags & 0x3FFF)
    o << "flags=" << std::hex << (p.m_flags & 0x3FFF) << std::dec << ",";
  o << p.m_extra;
  return o;
}
////////////////////////////////////////////////////////////
// Entry
////////////////////////////////////////////////////////////
Entry::Entry(MWAWInputStreamPtr input) : MWAWEntry(), m_input(input), m_nextId(-2), m_type(-1), m_typeId(-3), m_data(), m_asciiFile()
{
  for (int i = 0; i < 3; i++)
    m_values[i] = 0;
}
Entry::~Entry()
{
  closeDebugFile();
}

std::ostream &operator<<(std::ostream &o, Entry const &entry)
{
  if (entry.type().length()) {
    o << entry.type();
    if (entry.id() >= 0) o << "[" << entry.id() << "]";
    o << ",";
  }
  if (entry.m_id != -1) {
    o << "fId=" << entry.m_id << ",";
  }
  switch (entry.m_type) {
  case -1:
    break;
  case 0xa:
    o << "main,";
    break;
  case 0x11:
    o << "header,";
    break;
  case 0x12:
    o << "footer,";
    break;
  case 0x13:
    o << "textbox,";
    break;
  default:
    o << "zType=" << std::hex << entry.m_type << std::dec << ",";
  }
  if (entry.m_typeId != -3) {
    if (entry.m_typeId >= 0) o << "text/graphic,";
    else if (entry.m_typeId == -2)
      o << "null,";
    else if (entry.m_typeId == -1)
      o << "main,";
    else
      o << "#type=" << entry.m_typeId << ",";
  }
  for (int i = 0; i < 3; i++)
    if (entry.m_values[i])
      o << "e" << i << "=" << entry.m_values[i] << ",";
  if (entry.m_extra.length())
    o << entry.m_extra << ",";
  return o;
}

bool Entry::valid() const
{
  return m_input && MWAWEntry::valid();
}

void Entry::update()
{
  if (!m_data.size()) return;

  setBegin(0);
  setLength((long)m_data.size());
  m_input=MWAWInputStream::get(m_data, false);
  if (!m_input) {
    MWAW_DEBUG_MSG(("Entry::update: problem the input size is bad!!!\n"));
    return;
  }
  m_asciiFile.reset(new libmwaw::DebugFile(m_input));
  std::stringstream s;
  if (m_typeId == -1)
    s << "MainZoneM" << m_id;
  else
    s << "DataZone" << m_id;
  m_asciiFile->open(s.str());
}

void Entry::closeDebugFile()
{
  if (!m_data.size()) return;
  m_asciiFile->reset();
}

libmwaw::DebugFile &Entry::getAsciiFile()
{
  return *m_asciiFile;
}

bool Entry::operator==(const Entry &a) const
{
  if (MWAWEntry::operator!=(a)) return false;
  if (m_input.get() != a.m_input.get()) return false;
  if (id() != a.id()) return false;
  if (m_nextId != a.m_nextId) return false;
  if (m_type != a.m_type) return false;
  if (m_typeId != a.m_typeId) return false;
  if (m_id != a.m_id) return false;
  for (int i  = 0; i < 3; i++)
    if (m_values[i] != a.m_values[i]) return false;
  return true;
}

////////////////////////////////////////////////////////////
// read the zone data header
bool ZoneHeader::read(shared_ptr<FWStruct::Entry> zone)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->getAsciiFile();
  libmwaw::DebugStream f;
  bool typedDoc = m_type > 0;
  long pos = input->tell();
  if (pos+73 > zone->end())
    return false;

  int val = (int)input->readULong(1);
  if (!typedDoc && val)
    return false;
  if (val) f << "#type[high]" << std::hex << val << std::dec << ",";
  int type = (int)input->readULong(1);
  if (!(type >= 0x18 && type <=0x1f) && !(type >= 0xc && type <= 0xe)
      &&!(typedDoc && type==0x5a))
    return false;
  f << "type=" << std::hex << type << std::dec << ",";

  val = (int)input->readULong(2);
  if (val) {
    if (!typedDoc) return false;
    f << "#f0=" << val << ",";
  }
  val = (int)input->readULong(1); // 0, 6 or 0x10, 0x1e
  if (val) f << "f1=" << std::hex << val << std::dec << ",";
  val = (int)input->readLong(1); // 0 or  0x1 or -10
  if (val != 1) f << "f2=" << val << ",";
  int N = (int)input->readLong(2);
  if (N) // can be a big number, but some time 0, 1, 3, 4, ...
    f << "N0=" << N << ",";
  // small number between 1 and 0x1f
  val = (int)input->readLong(2);
  if (val) f << "N1=" << val << ",";

  val = (int)input->readLong(1); // 0, 1, 2, -1, -2
  if (val) f << "f3=" << val << ",";
  val = (int)input->readULong(1); // 12, 1f, 22, 23, 25, 2d, 32, 60, 62, 66, 67, ...
  if (val) f << "f4=" << std::hex << val << std::dec << ",";

  // small number, g0, g2 often negative
  for (int i = 0; i < 4; i++) {
    val = (int)input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }

  val = (int)input->readLong(2); // alway -2
  if (val != -2) {
    if (val > 0 || val < -2) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    f << "#g4=" << val << ",";
  }
  for (int i = 0; i < 3; i++) {
    // first a small number < 3e1, g6,g7 almost always 0 expected one time g6=-1a9
    val = (int)input->readLong(4);
    if (!val) continue;
    if (i==2 && !typedDoc)
      return false;
    f << "g" << i+5 << "=" << val << ",";
  }
  m_fileId = (int)input->readULong(2);
  m_docId = (int)input->readULong(2);
  for (int i=0; i < 3; ++i) { // h0: a small number 0..be, h1=0|1, h2=0..7
    val = (int)input->readLong(2);
    if (val)
      f << "h" << i << "=" << val << ",";
  }
  // now probably dependent of the type
  switch (m_type) {
  case 0x13: // sidebar
  case 0x14: // sidebar simple
    for (int i=0; i < 3; ++i) { // h3=0..e, h4=0..d, h5=0..a8
      val = (int)input->readLong(2);
      if (val)
        f << "h" << i+3 << "=" << val << ",";
    }
    f << "PTR=[";
    for (int i=0; i < 2; ++i)
      f << std::hex << input->readULong(4) << std::dec << ",";
    f << "],";
    m_wrapping=(int) input->readLong(1);
    val = (int)input->readLong(1);
    if (val) f << "#h6=" << val << ",";
    for (int i=0; i<6; ++i) { // 0
      val = (int)input->readLong(2);
      if (val)
        f << "h" << i+7 << "=" << val << ",";
    }
    break;
  default:
    break;
  }
  m_extra = f.str();
  if (input->tell()!=pos+72)
    asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(pos);
  input->seek(pos+72, librevenge::RVNG_SEEK_SET);
  f.str("");
  return true;
}

std::ostream &operator<<(std::ostream &o, ZoneHeader const &dt)
{
  if (dt.m_type >= 0) o << FWStruct::getTypeName(dt.m_type);
  if (dt.m_fileId >= 0) o << "fileId=" << dt.m_fileId << ",";
  if (dt.m_docId >= 0) o << "docId=" << dt.m_docId << ",";
  switch (dt.m_wrapping) {
  case -1:
    break;
  case 0:
    o << "wrapToShape,";
    break;
  case 1:
    o << "wrap[rect],";
    break;
  case 2:
    o << "wrap[shrinkToFit],";
    break;
  case 3:
    o << "wrap[background],";
    break;
  default:
    o << "#wrap=" << dt.m_wrapping << ",";
    break;
  }
  o << dt.m_extra;
  return o;
}

}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
