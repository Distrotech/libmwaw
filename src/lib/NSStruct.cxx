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

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "NSParser.hxx"

#include "NSStruct.hxx"

namespace NSStruct
{
std::ostream &operator<< (std::ostream &o, Position const &pos)
{
  o << pos.m_paragraph << "x";
  if (pos.m_word) o << pos.m_word << "x";
  else o << "_x";
  if (pos.m_char) o << pos.m_char;
  else o << "_";

  return o;
}

std::ostream &operator<< (std::ostream &o, FootnoteInfo const &fnote)
{
  if (fnote.m_flags&0x4) o << "sepPos=rigth,";
  if (fnote.m_flags&0x8) o << "endNotes,"; // footnote|endnote
  if (fnote.m_flags&0x10) o << "renumber[Pages],";
  if (fnote.m_flags&0x20) o << "maySep,"; // can put note on diff page than ref
  if (fnote.m_flags&0x40) o << "dontBrk,"; // dont brk a footnote
  if (fnote.m_flags&0x80) o << "notePos=bottom,";
  if (fnote.m_flags&0xFF03)
    o << "fl=" << std::hex << (fnote.m_flags&0xFF03) << std::dec << ",";
  if (fnote.m_distToDocument != 5)
    o << "dist=" << fnote.m_distToDocument << "[bef],";
  if (fnote.m_distSeparator != 36)
    o << "dist=" << fnote.m_distSeparator << "[between],";
  if (fnote.m_separatorLength != 108)
    o << "w[sep]=" << fnote.m_separatorLength << "pt";
  if (fnote.m_unknown) o << "unkn=" << fnote.m_unknown << ",";
  return o;
}

////////////////////////////////////////////////////////////
// read a recursive zone
////////////////////////////////////////////////////////////
bool RecursifData::read(NSParser &parser, MWAWEntry const &entry)
{
  if (!m_info || m_info->m_zoneType < 0 || m_info->m_zoneType >= 3) {
    MWAW_DEBUG_MSG(("NSStruct::RecursifData::Read: find unexpected zoneType\n"));
    return false;
  }
  if (m_level < 0 || m_level >= 3) {
    MWAW_DEBUG_MSG(("NSStruct::RecursifData::Read: find unexpected level: %d\n", m_level));
    return false;
  }
  if (entry.length() < 12) {
    MWAW_DEBUG_MSG(("NSStruct::RecursifData::Read: the entry is bad\n"));
    return false;
  }

  int zoneId = int(m_info->m_zoneType);
  entry.setParsed(true);
  MWAWInputStreamPtr input = parser.rsrcInput();
  libmwaw::DebugFile &asciiFile = parser.rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  libmwaw::DebugStream f;
  if (m_level == 0) {
    f << "Entries(" << entry.name() << "):";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
  }
  int num = 0;
  while (input->tell() != entry.end()) {
    pos = input->tell();
    bool ok = true;
    if (pos+12 > entry.end()) {
      MWAW_DEBUG_MSG(("NSStruct::RecursifData::Read: can not read entry %d\n", num-1));
      ok = false;
    }
    int level = (int) input->readLong(2);
    if (level != m_level && level != m_level+1) {
      MWAW_DEBUG_MSG(("NSStruct::RecursifData::Read: find unexpected level for entry %d\n", num-1));
      ok = false;
    }
    f.str("");
    f << entry.name() << level << "-" << num++;
    if (zoneId) f << "[" << zoneId << "]";
    f << ":";
    if (!ok) {
      f << "###";
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      return false;
    }
    int val = (int) input->readLong(2);
    f << "unkn=" << val << ","; // always x10 for level<=2 and c for level==3 ?
    long sz = input->readLong(4);
    long totalSz = sz;
    long minSize = 16;
    if (level == 3) {
      totalSz += 13;
      if (totalSz%2) totalSz++;
      minSize = 14;
    }
    long endPos = pos+totalSz;
    if (totalSz < minSize || endPos > entry.end()) {
      MWAW_DEBUG_MSG(("NSStruct::RecursifData::Read: can not read entry %d\n", num-1));
      f << "###";
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      return false;
    }

    int type = (int) input->readLong(4);
    if ((level == 1 && type == 0x7FFFFEDF) ||
        (level == 2 && type == 0x7FFFFFFF))
      ;
    else if ((type>>16)==0x7FFF)
      f << "type=" << type-0x7FFFFFFF-1 << ",";
    else
      f << "type=" << type << ",";

    if (level != 3) {
      val = (int) input->readULong(4);
      if ((level==1 && val==0x10) || (level==2 && val == 1))
        ;
      else
        f << "wh=" << val << ",";
    }
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());

    Node child;
    child.m_type = type;
    child.m_entry = entry;
    child.m_entry.setBegin(input->tell());
    child.m_entry.setEnd(endPos);

    if (level == 3) {
      child.m_entry.setLength(sz);
      m_childList.push_back(child);
      input->seek(endPos, librevenge::RVNG_SEEK_SET);
      continue;
    }

    if (child.m_entry.length()==0) {
      if (level != 1) {
        MWAW_DEBUG_MSG(("NSStruct::RecursifData::Read: Oops find 0 length child\n"));
        asciiFile.addPos(pos);
        asciiFile.addNote("###");
      }
      continue;
    }

    shared_ptr<RecursifData> childData(new RecursifData(*this));
    childData->m_level = level;
    child.m_data = childData;

    if (!childData->read(parser, child.m_entry)) {
      MWAW_DEBUG_MSG(("NSStruct::RecursifData::Read: can not read child entry\n"));
      asciiFile.addPos(pos);
      asciiFile.addNote("###");
    }
    else
      m_childList.push_back(child);

    input->seek(endPos, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
