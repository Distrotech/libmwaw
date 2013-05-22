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

#include <cstring>
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
#include "MWAWList.hxx"
#include "MWAWPageSpan.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictData.hxx"
#include "MWAWPosition.hxx"
#include "MWAWRSRCParser.hxx"

#include "MORParser.hxx"

#include "MORText.hxx"

/** Internal: the structures of a MORText */
namespace MORTextInternal
{
////////////////////////////////////////
//! Internal: the paragraph of a MORText
struct Paragraph : public MWAWParagraph {
  //! constructor
  Paragraph(): MWAWParagraph(), m_listType(0), m_listLevel(),
    m_pageBreak(false), m_keepOutlineTogether(false) {
    for (int i=0; i < 2; i++)
      m_marginsFromParent[i]=0;
  }
  //! set the left margin in inch
  void setLeftMargin(double margin, bool fromParent) {
    if (fromParent) {
      m_marginsFromParent[0]=margin;
      m_margins[1]=0;
    } else {
      m_marginsFromParent[0]=0;
      m_margins[1]=margin;
    }
  }
  //! set the right margin in inch
  void setRightMargin(double margin, bool fromParent) {
    if (fromParent) {
      m_marginsFromParent[1]=margin;
      m_margins[2]=0;
    } else {
      m_marginsFromParent[1]=0;
      m_margins[2]=margin;
    }
  }
  //! the left and right margins from parent in inches
  double m_marginsFromParent[2];
  //! the list type (0: none, 1: leader, ...)
  int m_listType;
  //! a custom list level ( only defined if m_listType>=0xb)
  MWAWListLevel m_listLevel;
  //! true if we need to add a page break before
  bool m_pageBreak;
  //! true if we need to keep outline together
  bool m_keepOutlineTogether;
};

////////////////////////////////////////
//! Internal: the outline data of a MORText
struct Outline {
  //! constructor
  Outline() : m_paragraph(), m_font(3,12, MWAWFont::boldBit) {
  }
  //! the paragraph
  Paragraph m_paragraph;
  //! the font
  MWAWFont m_font;
};

////////////////////////////////////////
//! Internal and low level: the outline modifier header of a MORText
struct OutlineMod {
  //! constructor
  OutlineMod(): m_type(-1), m_entry(), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, OutlineMod const &head) {
    switch(head.m_type) {
    case 0x301:
      o << "font,";
      break;
    case 0x402:
      o << "fSize,";
      break;
    case 0x603:
      o << "fFlags,";
      break;
    case 0x804:
      o << "fColor,";
      break;
    case 0xa05:
      o << "interline,";
      break;
    case 0xc0f:
      o << "firstIndent,";
      break;
    case 0xf07:
      o << "tabs,";
      break;
    case 0x1006:
      o << "justify,";
      break;
    case 0x1208:
      o << "bef/afterspace,";
      break;
    case 0x1409:
      o << "lMargin,";
      break;
    case 0x160a:
      o << "rMargin,";
      break;
    case 0x190b:
      o << "list[type],";
      break;
    case 0x1a0c:
      o << "break,";
      break;
    case 0x1c0d:
      o << "keepline,";
      break;
    case 0x1e0e:
      o << "keep[outline],";
      break;
    case -1:
      break;
    default:
      // 2914|2b15|3319|3b1c|6532|6934|7137|773a|7b3c -> backside|backpattern
      o << "type=" << std::hex << head.m_type << std::dec << ",";
      break;
    }
    if (head.m_entry.valid())
      o << std::hex << head.m_entry.begin() << "<->" << head.m_entry.end() << std::dec << ",";
    o << head.m_extra;
    return o;
  }
  //! the type
  int m_type;
  //! the data entry
  MWAWEntry m_entry;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the comment data of a MORText
struct Comment {
  //! constructor
  Comment() : m_entry(), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Comment const &comment) {
    o << comment.m_extra;
    return o;
  }
  //! the text entry
  MWAWEntry m_entry;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the topic data of a MORText
struct Topic {
  //! constructor
  Topic() : m_entry(), m_level(0), m_hasOutline(false), m_hasComment(false),
    m_hasSpeakerNote(false), m_isCloned(false), m_cloneId(0), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Topic const &topic) {
    if (topic.m_level>0)
      o << "level=" << topic.m_level << ",";
    if (topic.m_hasOutline)
      o << "outline,";
    if (topic.m_hasComment)
      o << "comment,";
    if (topic.m_hasSpeakerNote)
      o << "speakerNote,";
    if (topic.m_isCloned)
      o << "cloned,";
    if (topic.m_cloneId)
      o << "cloneId=" << topic.m_cloneId << ",";
    o << topic.m_extra;
    return o;
  }
  //! the text entry
  MWAWEntry m_entry;
  //! the topic level
  int m_level;
  //! true if the topic has a outline data
  bool m_hasOutline;
  //! true if the topic has a comment data
  bool m_hasComment;
  //! true if the topic has a speaker note
  bool m_hasSpeakerNote;
  //! true if the entry is cloned
  bool m_isCloned;
  //! if not 0, indicate that we must cloned the cloneId^th clone
  int m_cloneId;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the state of a MORText
struct State {
  //! constructor
  State() : m_version(-1), m_topicList(), m_commentList(), m_speakerList(), m_outlineList(), m_numPages(-1), m_actualPage(1) {
  }

  //! the file version
  mutable int m_version;
  //! the topic list
  std::vector<Topic> m_topicList;
  //! the header/footer/comment list
  std::vector<Comment> m_commentList;
  //! the speaker note list
  std::vector<MWAWEntry> m_speakerList;
  //! the outline list
  std::vector<Outline> m_outlineList;
  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MORText::MORText(MORParser &parser) :
  m_parserState(parser.getParserState()), m_state(new MORTextInternal::State), m_mainParser(&parser)
{
}

MORText::~MORText()
{ }

int MORText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int MORText::numPages() const
{
  if (m_state->m_numPages >= 0)
    return m_state->m_numPages;

  int nPages=1;
  // fixme
  return m_state->m_numPages = nPages;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

//
// find/send the different zones
//
bool MORText::createZones()
{
  return false;
}

bool MORText::sendMainText()
{
  return true;
}

//! read a text entry
bool MORText::readText(MWAWEntry const &entry)
{
  if (entry.length()<4) {
    MWAW_DEBUG_MSG(("MORText::readText: the entry is bad\n"));
    return false;
  }
  long pos = entry.begin();
  long endPos = entry.end();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  input->seek(pos+4, WPX_SEEK_SET);
  f << "Entries(Text):";
  int val;
  while (!input->atEOS()) {
    long actPos = input->tell();
    if (actPos >= endPos)
      break;
    unsigned char c=(unsigned char)input->readULong(1);
    if (c!=0x1b) {
      f << c;
      continue;
    }
    if (actPos+1 >= endPos) {
      f << "@[#]";
      MWAW_DEBUG_MSG(("MORText::readText: text end by 0x1b\n"));
      continue;
    }
    int fld=(int)input->readULong(1);
    switch(fld) {
    case 0x9:
      f << "\t";
      break;
    case 0xd: // EOL in header/footer
      f << (char) 0xd;
      break;
    case 0x2d: // geneva
      f << "@[fId=def]";
      break;
    case 0x2e: // 12
      f << "@[fSz=def]";
      break;
    case 0x2f: // black
      f << "@[fCol=def]";
      break;
    case 0x30: // font
      if (actPos+4+2 > endPos) {
        f << "@[#fId]";
        MWAW_DEBUG_MSG(("MORText::readText: field font seems too short\n"));
        break;
      }
      val = (int) input->readULong(2);
      if (!val&0x8000) {
        MWAW_DEBUG_MSG(("MORText::readText: field fId: unexpected id\n"));
        f << "@[#fId]";
        input->seek(-2, WPX_SEEK_CUR);
        break;
      }
      f << "@[fId=" << (val&0x7FFF) << "]";
      val = (int) input->readULong(2);
      if (val!=0x1b30) {
        MWAW_DEBUG_MSG(("MORText::readText: field fId: unexpected end field\n"));
        f << "###";
        input->seek(-2, WPX_SEEK_CUR);
        break;
      }
      break;
    case 0x31:
      if (actPos+4+2 > endPos) {
        f << "@[#fSz]";
        MWAW_DEBUG_MSG(("MORText::readText: field fSz seems too short\n"));
        break;
      }
      val = (int) input->readLong(2);
      f << "@[fSz=" << val << "]";
      if (val <= 0) {
        MWAW_DEBUG_MSG(("MORText::readText: field fSz seems bad\n"));
        f << "###";
      }
      val = (int) input->readULong(2);
      if (val!=0x1b31) {
        MWAW_DEBUG_MSG(("MORText::readText: field fSz: unexpected end field\n"));
        f << "###";
        input->seek(-2, WPX_SEEK_CUR);
        break;
      }
      break;
    case 0x38:
      if (actPos+4+10 > endPos) {
        f << "@[#fCol]";
        MWAW_DEBUG_MSG(("MORText::readText: field fCol seems too short\n"));
        break;
      }
      // fixme: format e, red, green, blue, e
      f << "@[fCol=";
      for (int i=0; i < 5; i++) {
        val=(int)input->readULong(2);
        if (val)
          f << std::hex << val << std::dec << ",";
        else
          f << ",";
      }
      f << "]";
      val = (int) input->readULong(2);
      if (val!=0x1b38) {
        MWAW_DEBUG_MSG(("MORText::readText: field fCol: unexpected end field\n"));
        f << "###";
        input->seek(-2, WPX_SEEK_CUR);
        break;
      }
      break;
    case 0x41:
      f << "@[supersc]";
      break;
    case 0x42: // in fact, (line boldà^bold
      f << "@[b]";
      break;
    case 0x49: // in fact, (line italic)^italic
      f << "@[it]";
      break;
    case 0x4c:
      f << "@[subsc]";
      break;
    case 0x4f:
      f << "@[outline]";
      break;
    case 0x53:
      f << "@[shadow]";
      break;
    case 0x55:
      f << "@[underl]";
      break;
    case 0x61:
      f << "@[script=def]";
      break;
    case 0x62:
      f << "@[/b]";
      break;
    case 0x69:
      f << "@[/it]";
      break;
    case 0x6f:
      f << "@[/outline]";
      break;
    case 0x73:
      f << "@[/shadow]";
      break;
    case 0x75:
      f << "@[/underl]";
      break;
    case 0xb9:
      if (actPos+4+8 > endPos) {
        f << "@[#b9]";
        MWAW_DEBUG_MSG(("MORText::readText: field b9 seems too short\n"));
        break;
      }
      f << "@[b9:";
      /* c f0f1 c with f0f1
         - 30000: filename, 40000: foldername
         - 5000a: pagenumber, c000a: page count
         - 80200: date,90200: date[lastmodif]
         - 60001: time,70001: time[lastmodif]
      */
      for (int i=0; i < 4; i++) { // c ? ? c
        val=(int)input->readLong(2);
        if (val)
          f << std::hex << val << std::dec << ",";
        else
          f << ",";
      }
      f << "]";
      val = (int) input->readULong(2);
      if (val!=0x1bb9) {
        MWAW_DEBUG_MSG(("MORText::readText: field b9: unexpected end field\n"));
        f << "###";
        input->seek(-2, WPX_SEEK_CUR);
        break;
      }
      break;
    default: {
      int sz=(int) input->readULong(2);
      if (sz>4 && actPos+sz<=endPos) {
        input->seek(actPos+sz-4, WPX_SEEK_SET);
        if ((int) input->readULong(2)==sz &&
            (int) input->readULong(2)==int(0x1b00|fld)) {
          MWAW_DEBUG_MSG(("MORText::readText: find a complex unknown field\n"));
          f << "@[#" << std::hex << fld << std::dec << ":" << sz << "]";
          break;
        }
        input->seek(actPos+2, WPX_SEEK_SET);
      }
      MWAW_DEBUG_MSG(("MORText::readText: find a unknown field\n"));
      f << "@[#" << std::hex << fld << std::dec << "]";
      break;
    }
    }
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

// try read to the file text position
bool MORText::readTopic(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%10)) {
    MWAW_DEBUG_MSG(("MORText::readTopic: the entry is bad\n"));
    return false;
  }
  // topic 0-2: ?, header,footer, topic 3: title, after document
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  entry.setParsed(true);

  ascFile.addPos(pos);
  ascFile.addNote("Entries(Topic)");

  int N=int(entry.length()/10);
  int val;
  for (int i=0; i < N; i++) {
    pos=input->tell();
    MORTextInternal::Topic topic;
    f.str("");
    topic.m_level = (int) input->readLong(2);
    bool isAClone=false;
    int flag = (int) input->readULong(2); // some flag ?
    if ((flag&1)==0) f << "hidden,";
    if (flag&4) f << "marked,";
    if (flag&0x10) topic.m_isCloned=true;
    if (flag&0x20) isAClone=true;
    if (flag&0x40) topic.m_hasSpeakerNote=true;
    if (flag&0x80) topic.m_hasComment=true;
    if (flag&0x400) f << "showComment,";
    if (flag&0x8000) topic.m_hasOutline=true;
    // find only bits: 7208
    flag &= 0x7B4A;
    if (flag) f << "fl=" << std::hex << flag << std::dec << ",";
    long fPos = input->readLong(4);
    if (isAClone)
      topic.m_cloneId=(int) fPos;
    else {
      f << "pos=" << std::hex << fPos << std::dec << ",";
      topic.m_entry.setBegin(fPos);
      if (!m_mainParser->checkAndFindSize(topic.m_entry)) {
        MWAW_DEBUG_MSG(("MORText::readTopic: can not read a text position\n"));
        f << "###";
        topic.m_entry = MWAWEntry();
      }
    }
    val = (int) input->readLong(2); // a small number 1 or 2
    if (val)
      f << "f1=" << val << ",";
    topic.m_extra=f.str();
    m_state->m_topicList.push_back(topic);

    f.str("");
    f << "Topic-" << i << ":" << topic;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+10, WPX_SEEK_SET);
  }
  for (size_t i=0; i < m_state->m_topicList.size(); i++) {
    MWAWEntry const &tEntry=m_state->m_topicList[i].m_entry;
    if (!tEntry.valid())
      continue;
    ascFile.addPos(tEntry.end());
    ascFile.addNote("_");
    if (readText(tEntry))
      continue;
    f.str("");
    f << "Topic-" << i << "[data]:";
    ascFile.addPos(tEntry.begin());
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MORText::readComment(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%8)) {
    MWAW_DEBUG_MSG(("MORText::readComment: the entry is bad\n"));
    return false;
  }
  // comment0? comment1: header, comment2: footer
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  entry.setParsed(true);

  ascFile.addPos(pos);
  ascFile.addNote("Entries(Comment)");

  int N=int(entry.length()/8);
  int val;
  for (int i=0; i < N; i++) {
    pos=input->tell();
    MORTextInternal::Comment comment;
    f.str("");
    long fPos = input->readLong(4);
    f << "pos=" << std::hex << fPos << std::dec << ",";
    comment.m_entry.setBegin(fPos);
    if (!m_mainParser->checkAndFindSize(comment.m_entry)) {
      MWAW_DEBUG_MSG(("MORText::readComment: can not read a file position\n"));
      f << "###";
      comment.m_entry.setLength(0);
    }
    val = (int) input->readLong(2); // always 4 ?
    if (val != 4)
      f << "f0=" << val << ",";
    val = (int) input->readULong(2); // some flag ? find 0x3333 0x200d ...
    if (val) f << "fl=" << std::hex << val << std::dec << ",";

    comment.m_extra=f.str();
    m_state->m_commentList.push_back(comment);
    f.str("");
    f << "Comment-" << i << ":" << comment;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+8, WPX_SEEK_SET);
  }
  for (size_t i=0; i < m_state->m_commentList.size(); i++) {
    MWAWEntry const &tEntry=m_state->m_commentList[i].m_entry;
    if (!tEntry.valid())
      continue;
    ascFile.addPos(tEntry.end());
    ascFile.addNote("_");
    if (readText(tEntry))
      continue;
    f.str("");
    f << "Comment-" << i << "[data]:";
    ascFile.addPos(tEntry.begin());
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MORText::readSpeakerNote(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%4)) {
    MWAW_DEBUG_MSG(("MORText::readSpeakerNote: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  entry.setParsed(true);

  f << "Entries(SpeakerNote):";
  int N=int(entry.length()/4);
  for (int i=0; i < N; i++) {
    long fPos = input->readLong(4);
    f << "pos=" << std::hex << fPos << std::dec << ",";
    MWAWEntry tEntry;
    tEntry.setBegin(fPos);
    if (!m_mainParser->checkAndFindSize(tEntry)) {
      MWAW_DEBUG_MSG(("MORText::readSpeakerNote: can not read a file position\n"));
      f << "###";
      tEntry.setLength(0);
    }
    m_state->m_speakerList.push_back(tEntry);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  for (size_t i=0; i < m_state->m_speakerList.size(); i++) {
    MWAWEntry const &tEntry=m_state->m_speakerList[i];
    if (!tEntry.valid())
      continue;
    ascFile.addPos(tEntry.end());
    ascFile.addNote("_");
    if (readText(tEntry))
      continue;
    f.str("");
    f << "SpeakerNote-" << i << "[data]:";
    ascFile.addPos(tEntry.begin());
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

//
// send the text
//

//
// send a graphic
//

//////////////////////////////////////////////
// Fonts
//////////////////////////////////////////////
bool MORText::readFonts(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MORText::readFonts: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  long endPos = entry.end();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  entry.setParsed(true);

  int n=0;
  while (1) {
    pos=input->tell();
    if (pos+1 > endPos) {
      MWAW_DEBUG_MSG(("MORText::readFonts: problem reading a font\n"));
      break;
    }
    int fSz=int(input->readULong(1));
    if (fSz==0)
      break;
    if (pos+1+fSz+2 > endPos) {
      input->seek(-1, WPX_SEEK_CUR);
      break;
    }
    f.str("");
    if (n==0)
      f << "Entries(Fonts)-" << n++ << ",";
    else
      f << "Fonts-"  << n++ << ":";
    std::string name("");
    for (int i=0; i < fSz; i++)
      name+=(char) input->readULong(1);
    if ((fSz&1)==0) input->seek(1, WPX_SEEK_CUR);
    int id=(int) input->readULong(2);
    f << name << ",id=" << id << ",";
    if (name.length())
      m_parserState->m_fontConverter->setCorrespondance(id, name);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  pos = input->tell();
  if (pos != endPos) {
    MWAW_DEBUG_MSG(("MORText::readFonts: problem reading a font\n"));
    ascFile.addPos(pos);
    ascFile.addNote("Fonts:###");
  }

  return true;
}

//////////////////////////////////////////////
// outline
//////////////////////////////////////////////
bool MORText::readOutlineList(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%4)) {
    MWAW_DEBUG_MSG(("MORText::readOutlineList: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  entry.setParsed(true);

  f << "Entries(Outline):";
  int N=int(entry.length()/4);
  std::vector<MWAWEntry> posList;
  for (int i=0; i < N; i++) {
    MWAWEntry tEntry;
    tEntry.setBegin(input->readLong(4));
    tEntry.setId(int(i));
    if (!m_mainParser->checkAndFindSize(tEntry)) {
      MWAW_DEBUG_MSG(("MORText::readOutlineList: can not read a file position\n"));
      f << "###,";
    } else
      f << std::hex << tEntry.begin() << "<->" << tEntry.end() << ",";
    posList.push_back(tEntry);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  for (size_t i=0; i < posList.size(); i++) {
    MWAWEntry const &tEntry=posList[i];
    if (!tEntry.valid())
      continue;
    MORTextInternal::Outline outline;
    if (readOutline(tEntry, outline)) {
      m_state->m_outlineList.push_back(outline);
      continue;
    }
    m_state->m_outlineList.push_back(MORTextInternal::Outline());
    ascFile.addPos(tEntry.begin());
    ascFile.addNote("Outline-data:###");
    ascFile.addPos(tEntry.end());
    ascFile.addNote("_");
  }
  return true;
}

bool MORText::readOutline(MWAWEntry const &entry, MORTextInternal::Outline &outline)
{
  if (!entry.valid() || entry.length()<8) {
    MWAW_DEBUG_MSG(("MORText::readOutline: the entry is bad\n"));
    return false;
  }
  int vers = version();
  long pos = entry.begin();
  long endPos = entry.end();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  input->seek(pos+4, WPX_SEEK_SET); // skip size

  f << "Outline[data" << entry.id() << "]:";
  int val=(int) input->readULong(2);
  if (val!=6*(vers-1)) {
    MWAW_DEBUG_MSG(("MORText::readOutline: find unexpected type\n"));
    f << "#f0=" << val << ",";
  }
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  long lastListPos = pos+8+N*16;
  if (lastListPos > endPos) {
    MWAW_DEBUG_MSG(("MORText::readOutline: can not read length\n"));
    return false;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  MORTextInternal::Paragraph &para = outline.m_paragraph;
  std::vector<MORTextInternal::OutlineMod> outlineModList;
  uint32_t fFlags=outline.m_font.flags();
  for (int n=0; n<N; n++) {
    pos = input->tell();
    f.str("");

    MORTextInternal::OutlineMod outlineMod;
    val=int(input->readLong(1));
    if (val!=6*(vers-1))
      f << "#f0=" << val << ",";
    val=int(input->readULong(1));
    if (val) // [0|1|2|4]|[0..7]
      f << "fl=" << std::hex << val << std::dec << ",";
    for (int i=0; i <2; i++) { // f0=0|1|2, f1=0|1|999
      val=int(input->readLong(2));
      if (val)
        f << "f" << i+1 << "=" << val << ",";
    }
    outlineMod.m_type=int(input->readULong(2));
    int values[4];
    for (int i=0; i < 4; i++)
      values[i] = (int) input->readULong(2);
    bool haveExtra=false;
    switch(outlineMod.m_type) {
    case 0x301: // font name
    case 0xf07: // left indent+tabs
      haveExtra=true;
      break;
    case 0x402:
      // size can be very big, force it to be smallest than 100
      if (values[0]>0 && values[0] <= 100) {
        outline.m_font.setSize(values[0]);
        f << "sz=" << values[0] << ",";
      } else {
        MWAW_DEBUG_MSG(("MORText::readOutline: the font size seems bad\n"));
        f << "##sz=" << values[0] << ",";
      }
      break;
    case 0x603: {
      uint32_t bit=0;
      switch(values[0]) {
      case 0:
        fFlags=0;
        f << "plain";
        break;
      case 1:
        bit = MWAWFont::boldBit;
        f << "b";
        break;
      case 2:
        bit = MWAWFont::italicBit;
        f << "it";
        break;
      case 3:
        if (values[1]==1)
          outline.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
        f << "underl";
        break;
      case 4:
        bit = MWAWFont::outlineBit;
        f << "outline";
        break;
      case 5:
        bit = MWAWFont::shadowBit;
        f << "shadow";
        break;
      default:
        f << "##fl=" << std::hex << values[0] << std::dec;
        break;
      }
      if (values[1]==0) {
        if (bit) fFlags = fFlags & (~bit);
        f << "[of]";
      } else if (values[1]!=1)
        f << "=##" << values[1] << ",";
      else {
        fFlags |= bit;
        values[1]=0;
      }
      break;
    }
    case 0x804: {
      MWAWColor col(((uint16_t)values[0])>>8, ((uint16_t)values[1])>>8, ((uint16_t)values[2])>>8);
      outline.m_font.setColor(col);
      f << col << ",";
      values[1]=values[2]=0;
      break;
    }
    case 0xa05:
      if (values[0]&0x8000) {
        para.setInterline(double(values[0]&0x7FFF)/20., WPX_POINT, MWAWParagraph::AtLeast);
        f << "interline=" << *para.m_spacings[0] << "pt,";
      } else {
        para.setInterline(double(values[0])/double(0x1000), WPX_PERCENT);
        f << "interline=" << 100* *para.m_spacings[0] << "%,";
      }
      break;
    case 0xc0f: // firstIndent
      para.m_margins[0] = double(values[0])/1440.;
      f << "indent=" << *para.m_margins[0] << ",";
      break;
    case 0x1006:
      switch(values[0]) {
      case 0:
        para.m_justify = MWAWParagraph::JustificationLeft;
        f << "left,";
        break;
      case 1:
        para.m_justify = MWAWParagraph::JustificationCenter;
        f << "center,";
        break;
      case 2:
        para.m_justify = MWAWParagraph::JustificationRight;
        f << "right,";
        break;
      case 3:
        para.m_justify = MWAWParagraph::JustificationFull;
        f << "full,";
        break;
      default:
        f << "##justify=" << values[0] << ",";
        break;
      }
      break;
    case 0x1208:
      if (values[0] & 0x8000) {
        para.m_spacings[1]=double(values[0]&0x7FFF)/1440.;
        f << "bef=" << double(values[0]&0x7FFF)/20. << "pt,";
      } else {
        // assume 12pt
        para.m_spacings[1]=double(values[0])/double(0x1000)*12./720.;
        if (values[0])
          f << "bef=" << 100.*double(values[0])/double(0x1000) << "%,";
      }

      if (values[1] & 0x8000) {
        para.m_spacings[2]=double(values[1]&0x7FFF)/1440.;
        f << "aft=" << double(values[1]&0x7FFF)/20. << "pt,";
      } else {
        para.m_spacings[2]=double(values[1])/double(0x1000)*12./720.;
        if (values[1])
          f << "aft=" << 100.*double(values[1])/double(0x1000) << "%,";
      }
      values[1]=0;
      break;
    case 0x1409: // lMargin in TWIP
      para.setLeftMargin(double(values[0]&0x7FFF)/1440., (values[0]&0x8000)==0);
      if (values[0]&0x8000)
        f << "indent=" << double(values[0]&0x7FFF)/1440. << ",";
      else // checkme
        f << "indent=" << double(values[0])/1440. << "[fromParent],";
      break;
    case 0x160a: // rMargin in TWIP
      para.setRightMargin(double(values[0]&0x7FFF)/1440., (values[0]&0x8000)==0);
      if (values[0]&0x8000)
        f << "indent=" << double(values[0]&0x7FFF)/1440. << ",";
      else // checkme
        f << "indent=" << double(values[0])/1440. << "[fromParent],";
      break;
    case 0x1a0c:
      para.m_pageBreak=(values[0]==0x100);
      if(values[0]==0x100)
        f << "pagebreak,";
      else if (values[0]==0)
        f << "no,";
      else
        f << "##break=" << std::hex << values[0] << std::dec << ",";
      break;
    case 0x1c0d:
      if(values[0]==0x100) {
        para.m_breakStatus = (*para.m_breakStatus)|MWAWParagraph::NoBreakWithNextBit;
        f << "together,";
      } else if (values[0]==0) {
        para.m_breakStatus = (*para.m_breakStatus)|int(~MWAWParagraph::NoBreakWithNextBit);
        f << "no,";
      } else
        f << "#keepLine=" << std::hex << values[0] << std::dec << ",";
      break;
    case 0x1e0e:
      para.m_keepOutlineTogether = (values[0]==0x100);
      if(values[0]==0x100)
        f << "together,";
      else if (values[0]==0)
        f << "no,";
      else
        f << "#keepOutline=" << std::hex << values[0] << std::dec << ",";
      break;
    case 0x190b:
      para.m_listType = values[0];
      switch(values[0]) {
      case 0:
        f << "no,";
        break;
      case 1:
        f << "leader,";
        break;
      case 2:
        f << "hardward,";
        break;
      case 3:
        f << "numeric,";
        break;
      case 4:
        f << "legal,";
        break;
      case 5:
        f << "bullets,";
        break;
      default:
        if (values[0]>=11) {
          f << "custom[" << values[0] << "],";
          haveExtra=true;
          break;
        }
        f << "##bullet=" << values[0] << ",";
        break;
      }
      break;
    default:
      if (values[0])
        f << "f2=" << std::hex << values[0] << std::dec << ",";
      // use heuristic to define extra data
      if (values[0]>0x2800)
        haveExtra = (values[0] & 0x0100);
      else
        haveExtra=values[1]==0;
      break;
    }
    outline.m_font.setFlags(fFlags);
    if (values[1]) f << "g0=" << std::hex << values[1] << std::dec << ",";

    if (haveExtra && values[3]>0 &&
        lastListPos+values[2]+values[3] <= endPos) {
      outlineMod.m_entry.setBegin(lastListPos+values[2]);
      outlineMod.m_entry.setLength(values[3]);
      outlineMod.m_entry.setId(n);
    } else {
      for (int i=2; i < 4; i++) {
        if (values[i])
          f << "g" << i-1 << "=" << std::hex << values[i] << std::dec << ",";
      }
    }
    outlineMod.m_extra=f.str();
    f.str("");
    f << "Outline[data" << entry.id() << "-" << n << "]:" << outlineMod;
    outlineModList.push_back(outlineMod);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+16, WPX_SEEK_SET);
  }

  for (size_t n=0; n < outlineModList.size(); n++) {
    MORTextInternal::OutlineMod const &outlineMod=outlineModList[n];
    if (!outlineMod.m_entry.valid())
      continue;
    f.str("");
    f << "Outline[dataA-" << n << "]:";
    bool ok=false;
    switch (outlineMod.m_type) {
    case 0x301: {
      std::string fName;
      int fId;
      ok = readFont(outlineMod.m_entry, fName,fId);
      if (!ok) break;
      f << "font=[";
      f << "name=" << fName;
      if (fId>=0) {
        f << ":" << fId;
        outline.m_font.setId(fId);
      }
      f << "],";
      break;
    }
    case 0xf07: {
      std::string mess;
      ok = readTabs(outlineMod.m_entry, para, mess);
      if (!ok) break;
      f << "tabs=[" << mess << "],";
      break;
    }
    case 0x190b: {
      ok = readCustomListLevel(outlineMod.m_entry, para.m_listLevel);
      if (!ok) break;
      f << para.m_listLevel << ",";
      break;
    }
    default:
      break;
    }
    // can be also pattern or backside or custom header
    if (!ok) {
      f << "[" << outlineMod << "]";
      if (!parseUnknown(outlineMod.m_entry, 0))
        f << "###";
    }
    ascFile.addPos(outlineMod.m_entry.begin());
    ascFile.addNote(f.str().c_str());
  }
  ascFile.addPos(endPos);
  ascFile.addNote("_");
  return true;
}

//////////////////////////////////////////////
// small structure
//////////////////////////////////////////////
bool MORText::readFont(MWAWEntry const &entry, std::string &fName, int &fId)
{
  fName="";
  fId=-1;
  if (entry.length() < 2)
    return false;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int fSz=(int) input->readULong(1);
  long remain=entry.length()-long(1+fSz);
  if (fSz==0 || remain<0 || remain==1)
    return false;
  if (remain>=2 && remain!=2+(1-(fSz%2)))
    return false;
  for (int i=0; i < fSz; i++) {
    char c=(char) input->readULong(1);
    if (c==0) return false;
    fName+=c;
  }
  if (remain==0) { // let try to retrieve the font id
    fId=m_parserState->m_fontConverter->getId(fName);
    return true;
  }
  if ((fSz%2)==0) input->seek(1,WPX_SEEK_CUR);
  fId=(int) input->readULong(2);
  return true;
}

bool MORText::readCustomListLevel(MWAWEntry const &entry, MWAWListLevel &level)
{
  level=MWAWListLevel();
  if (entry.length()<22)
    return false;

  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugStream f;
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  MWAWFont font;
  int fId=(int) input->readULong(2);
  if (fId==0xFFFF) // default
    ;
  else if (fId&0x8000) {
    f << "fId=" << (fId&0x7FFF) << ",";
    font.setId(fId&0x7FFF);
  } else
    f << "#fId=" << std::hex << fId << std::dec << ",";
  int fSz = (int) input->readLong(2);
  if (fSz != -1) {
    font.setSize(fSz);
    f << "fSz=" << fSz << ",";
  }
  int fFlags=(int) input->readULong(1);
  uint32_t flags=0;
  if (fFlags&1) flags |= MWAWFont::boldBit;
  if (fFlags&2) flags |= MWAWFont::italicBit;
  if (fFlags&4) font.setUnderlineStyle(MWAWFont::Line::Simple);
  if (fFlags&8) flags |= MWAWFont::outlineBit;
  if (fFlags&0x10) flags |= MWAWFont::shadowBit;
  if (fFlags&0xE0)
    f << "#fFlags=" << std::hex << (fFlags&0xE0) << std::dec << ",";
  font.setFlags(flags);

  int fColor = (int) input->readLong(1);
  if (fColor==1)
    input->seek(6, WPX_SEEK_CUR);
  else if (fColor==3) {
    unsigned char color[3];
    for (int i=0; i < 3; i++)
      color[i]=(unsigned char) (input->readULong(2)>>8);
    font.setColor(MWAWColor(color[0], color[1], color[2]));
  } else {
    f << "#fCol=" << fColor << ",";
    input->seek(6, WPX_SEEK_CUR);
  }
#if defined(DEBUG_WITH_FILES)
  f << "font=[" << font.getDebugString(m_parserState->m_fontConverter) << "],";
#endif

  // now 4 bool
  bool bVal[4]= {false, false, false, false };
  long val;
  for (int i=0; i < 4; i++) {
    val = input->readLong(1);
    if (!val) continue;
    if (val!=1) {
      f << "#g" << i << "=" << val << ",";
      continue;
    }
    bVal[i]=true;
  }
  for (int i=0; i<2; i++) { // g1: field left ident modify?
    if (bVal[i])
      f << "g" << i << "=true,";
  }
  if (bVal[2]) // or flush left at
    f << "flushRight,";
  if (!bVal[3])
    f << "useFirstlineIdent,";
  val = input->readLong(2);
  if (val!=0x2d0) // default 0.5"
    f << "leftIdent=" << double(val)/1440. << ",";
  val = input->readLong(2);
  if (val != 0xb)
    f << "f6=" << val << ",";
  if (fId!=0xFFFF) { // maybe the font name
    int fontSz=(int) input->readULong(1);
    if (!fontSz || input->tell()+fontSz >= entry.end())
      input->seek(-1,WPX_SEEK_CUR);
    else {
      std::string fName("");
      for (int i=0; i<fontSz; i++)
        fName+=(char) input->readULong(1);
      f << "fName=" << fName << ",";
      int newId=m_parserState->m_fontConverter->getId(fName);
      if (newId > 0)
        font.setId(fId=newId);
    }
  }

  int labelSz = (int) input->readULong(1);
  if (input->tell()+labelSz != entry.end())
    return false;

  f << "label=";
  if (fId == 0xFFFF)
    fId=3;
  for (int c=0; c < labelSz; c++) {
    unsigned char ch=(unsigned char) input->readULong(1);
    f << ch;
    int unicode = m_parserState->m_fontConverter->unicode(fId, (unsigned char)ch);
    if (unicode!=-1)
      libmwaw::appendUnicode(uint32_t(unicode), level.m_bullet);
    else if (ch==0x9 || ch > 0x1f)
      libmwaw::appendUnicode((uint32_t)c, level.m_bullet);
    else {
      f << "##";
      MWAW_DEBUG_MSG(("MORText::readCustomListLevel: label char seems bad\n"));
      libmwaw::appendUnicode('#', level.m_bullet);
    }
  }
  f << ",";
  level.m_type=MWAWListLevel::BULLET;
  level.m_extra=f.str();
  if (input->tell()!=entry.end())
    m_parserState->m_asciiFile.addDelimiter(input->tell(),'|');
  return true;
}

bool MORText::readTabs(MWAWEntry const &entry, MORTextInternal::Paragraph &para,
                       std::string &mess)
{
  mess="";
  if (entry.length() < 4)
    return false;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugStream f;

  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int nTabs=(int) input->readULong(2);
  if (entry.length()!=4+4*nTabs)
    return false;
  int repeat=(int) input->readLong(2);
  if (uint16_t(repeat)==0x8000) // special case
    f << "def[center,right],";
  else
    f << "repeat=" << double(repeat)/1440. << ",";
  para.m_tabs->resize(0);
  for (int i=0; i < nTabs; i++) {
    libmwaw::DebugStream f2;
    MWAWTabStop tab;
    tab.m_position = double(input->readULong(2))/1440.;
    int val=(int) input->readULong(1);
    switch(val&0xF) {
    case 1: // left
      break;
    case 2:
      tab.m_alignment=MWAWTabStop::CENTER;
      break;
    case 3:
      tab.m_alignment=MWAWTabStop::RIGHT;
      break;
    case 4:
      tab.m_alignment=MWAWTabStop::DECIMAL;
      break;
    default:
      f2 << "#align=" << (val&0xF) << ",";
      break;
    }
    switch(val>>4) {
    case 0: // none
      break;
    case 1:
      tab.m_leaderCharacter = '_';
      break;
    case 3: // more large space
      f2 << "dot[large],";
    case 2:
      tab.m_leaderCharacter = '.';
      break;
    default:
      f2 << "#leader=" << (val>>4) << ",";
      break;
    }
    char decimalChar = (char) input->readULong(1);
    if (decimalChar) {
      int unicode= m_parserState->m_fontConverter->unicode(3, (unsigned char) decimalChar);
      if (unicode==-1)
        tab.m_decimalCharacter = uint16_t(decimalChar);
      else
        tab.m_decimalCharacter = uint16_t(unicode);
    }
    f << "tab" << i << "=[" << tab << "," << f2.str() << "],";
    para.m_tabs->push_back(tab);
  }
  mess=f.str();
  return true;
}

//////////////////////////////////////////////
// unknown structure
//////////////////////////////////////////////
bool MORText::parseUnknown(MWAWEntry const &entry, long fDecal)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  MORStruct::Pattern pattern;
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  if (m_mainParser->readPattern(entry.end(),pattern)) {
    f << pattern;
    if (input->tell()!=entry.end())
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos+fDecal);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  // can we find a backsidde here
  input->seek(pos, WPX_SEEK_SET);
  std::string extra("");
  if (m_mainParser->readBackside(entry.end(), extra)) {
    f << extra;
    if (input->tell()!=entry.end())
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos+fDecal);
    ascFile.addNote(f.str().c_str());
    return true;
  }

  std::string mess;
  MORTextInternal::Paragraph para;
  if (readTabs(entry, para, mess)) {
    f << "tabs=[" << mess << "],";
    ascFile.addPos(pos+fDecal);
    ascFile.addNote(f.str().c_str());
    return true;
  }

  std::string fName;
  int fId;
  if (readFont(entry, fName,fId)) {
    f << "font=[";
    f << "name=" << fName;
    if (fId>=0) f << ":" << fId;
    f << "],";
    ascFile.addPos(pos+fDecal);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  return false;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
