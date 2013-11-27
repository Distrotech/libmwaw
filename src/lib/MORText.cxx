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

#include <librevenge/librevenge.h>

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
#include "MWAWSubDocument.hxx"

#include "MORParser.hxx"

#include "MORText.hxx"

/** Internal: the structures of a MORText */
namespace MORTextInternal
{
////////////////////////////////////////
//! Internal: the paragraph of a MORText
struct Paragraph : public MWAWParagraph {
  //! constructor
  Paragraph(): MWAWParagraph(), m_listType(0), m_customListLevel(),
    m_pageBreak(false), m_keepOutlineTogether(false)
  {
    m_marginsFromParent[0]=0.3;
    m_marginsFromParent[1]=0;
  }
  //! set the left margin in inch
  void setLeftMargin(double margin, bool fromParent)
  {
    if (fromParent) {
      m_marginsFromParent[0]=margin;
      m_margins[1]=0;
    }
    else {
      m_marginsFromParent[0]=-100;
      m_margins[1]=margin;
    }
  }
  //! set the right margin in inch
  void setRightMargin(double margin, bool fromParent)
  {
    if (fromParent) {
      m_marginsFromParent[1]=margin;
      m_margins[2]=0;
    }
    else {
      m_marginsFromParent[1]=-100;
      m_margins[2]=margin;
    }
  }
  //! update the paragraph to obtain the final paragraph
  void updateToFinalState(MWAWParagraph const &parent, int level, MWAWListManager &listManager)
  {
    bool leftUseParent=m_marginsFromParent[0]>-10;
    if (leftUseParent)
      m_margins[1]=*parent.m_margins[1]+m_marginsFromParent[0];
    if (m_marginsFromParent[1]>-10)
      m_margins[2]=*parent.m_margins[2]+m_marginsFromParent[1];
    if (level<0)
      return;

    MWAWListLevel listLevel;
    switch (m_listType) {
    case 1: // leader
      listLevel.m_type = MWAWListLevel::BULLET;
      listLevel.m_bullet = "+"; // in fact + + and -
      break;
    case 2: // hardvard
      listLevel.m_suffix = (level <= 3) ? "." : ")";
      if (level == 1) listLevel.m_type = MWAWListLevel::UPPER_ROMAN;
      else if (level == 2) listLevel.m_type = MWAWListLevel::UPPER_ALPHA;
      else if (level == 3) listLevel.m_type = MWAWListLevel::DECIMAL;
      else if (level == 4) listLevel.m_type =  MWAWListLevel::LOWER_ALPHA;
      else if ((level%3)==2) {
        listLevel.m_prefix = "(";
        listLevel.m_type = MWAWListLevel::DECIMAL;
      }
      else if ((level%3)==0) {
        listLevel.m_prefix = "(";
        listLevel.m_type = MWAWListLevel::LOWER_ALPHA;
      }
      else
        listLevel.m_type = MWAWListLevel::LOWER_ROMAN;
      break;
    case 3: // numeric
      listLevel.m_type = MWAWListLevel::DECIMAL;
      listLevel.m_suffix = ".";
      break;
    case 4: // legal
      listLevel.m_type = MWAWListLevel::DECIMAL;
      listLevel.m_numBeforeLabels = level-1;
      listLevel.m_suffix = ".";
      listLevel.m_labelWidth = 0.2*level;
      break;
    case 5: // bullets
      listLevel.m_type = MWAWListLevel::BULLET;
      libmwaw::appendUnicode(0x2022, listLevel.m_bullet);
      break;
    case 0: // none
      return;
    default:
      if (m_listType<11)
        return;

      listLevel=m_customListLevel;
      break;
    }

    m_listLevelIndex = level+1;
    shared_ptr<MWAWList> parentList;
    if (*parent.m_listId>=0)
      parentList=listManager.getList(*parent.m_listId);
    shared_ptr<MWAWList> list=
      listManager.getNewList(parentList, level+1, listLevel);
    if (list)
      m_listId=list->getId();
    else
      m_listLevel=listLevel;

    m_margins[1]=m_margins[1].get()-listLevel.m_labelWidth;
  }
  //! the left and right margins from parent in inches
  double m_marginsFromParent[2];
  //! the list type (0: none, 1: leader, ...)
  int m_listType;
  //! a custom list level ( only defined if m_listType>=0xb)
  MWAWListLevel m_customListLevel;
  //! true if we need to add a page break before
  bool m_pageBreak;
  //! true if we need to keep outline together
  bool m_keepOutlineTogether;
};

////////////////////////////////////////
//! Internal: the outline data of a MORText
struct Outline {
  //! constructor
  Outline()
  {
    // set to default value
    for (int i=0; i < 4; i++)
      m_fonts[i]=MWAWFont(3,12);
    m_paragraphs[0].m_listType=1;
  }
  //! the paragraphs : organizer, slide, tree, unknowns
  Paragraph m_paragraphs[4];
  //! the fonts : organizer, slide, tree unknowns
  MWAWFont m_fonts[4];
};

////////////////////////////////////////
//! Internal and low level: the outline modifier header of a MORText
struct OutlineMod {
  //! constructor
  OutlineMod(): m_type(-1), m_flags(0), m_entry(), m_extra("")
  {
    for (int i=0; i<2; i++)
      m_unknowns[i]=0;
  }
  //! returns the data id to change in Outline
  int getModId() const
  {
    if (m_unknowns[0] || (m_flags&0xF)!= 1)
      return 3;
    switch (m_flags>>4) {
    case 1:
      return 0;
    case 2:
      return 1;
    case 4:
      return 2;
    default:
      return 3;
    }
    return 2;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, OutlineMod const &head)
  {
    switch (head.m_flags>>4) {
    case 1: // organizer
      break;
    case 2:
      o << "slide,";
      break;
    case 4:
      o << "tree,";
      break;
    default:
      o << "##wh=" << std::hex << (head.m_flags>>4) << std::dec << ",";
      break;
    }
    switch (head.m_type) {
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
    for (int i=0; i<2; i++) { // unkn0=0|1|2(related to clone?), unkn1=0|1|999
      if (head.m_unknowns[i])
        o << "unkn" << i << "=" << head.m_unknowns[i] << ",";
    }
    if (head.m_flags&0xF)
      o << "fl=" << std::hex << (head.m_flags&0xF) << std::dec << ",";
    if (head.m_entry.valid())
      o << std::hex << head.m_entry.begin() << "<->" << head.m_entry.end() << std::dec << ",";
    o << head.m_extra;
    return o;
  }
  //! the type
  int m_type;
  //! the flag
  int m_flags;
  //! the data entry
  MWAWEntry m_entry;
  //! some unknown flags
  int m_unknowns[2];
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the comment data of a MORText
struct Comment {
  //! constructor
  Comment() : m_entry(), m_extra("")
  {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Comment const &comment)
  {
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
  //! an enum used to define the different type of data attached to a topic
  enum AttachementType { AOutline=0, AComment, ASpeakerNote };
  //! constructor
  Topic() : m_entry(), m_level(0), m_isCloned(false), m_cloneId(-1), m_numPageBreak(-1), m_isStartSlide(false), m_extra("")
  {
    for (int i=0; i < 3; i++) {
      m_hasList[i]=false;
      m_attachList[i]=-1;
    }
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Topic const &topic)
  {
    if (topic.m_level>0)
      o << "level=" << topic.m_level << ",";
    if (topic.m_hasList[AOutline])
      o << "outline,";
    if (topic.m_hasList[AComment])
      o << "comment,";
    if (topic.m_hasList[ASpeakerNote])
      o << "speakerNote,";
    if (topic.m_isCloned)
      o << "cloned,";
    if (topic.m_cloneId >= 0)
      o << "cloneId=" << topic.m_cloneId << ",";
    if (topic.m_isStartSlide)
      o << "newSlide,";
    o << topic.m_extra;
    return o;
  }
  //! the text entry
  MWAWEntry m_entry;
  //! the topic level
  int m_level;
  //! true if the entry is cloned
  bool m_isCloned;
  //! if not 0, indicate that we must cloned the cloneId^th clone
  int m_cloneId;
  //! a list of boolean use to note if a topic is associated with a Outline, ...
  bool m_hasList[3];
  //! a list of id to retrieve the attachment
  int m_attachList[3];
  //! the number of pages in the sub list
  int m_numPageBreak;
  //! true if we start a new slide
  bool m_isStartSlide;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the state of a MORText
struct State {
  //! constructor
  State() : m_version(-1), m_topicList(), m_commentList(), m_speakerList(), m_outlineList(),
    m_actualComment(0), m_actualSpeaker(0), m_actualOutline(0), m_numPages(-1), m_actualPage(1)
  {
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
  //! the actual comment
  int m_actualComment;
  //! the actual speaker note
  int m_actualSpeaker;
  //! the actual outline
  int m_actualOutline;
  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MORText
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MORText &pars, MWAWInputStreamPtr input, int zId, int what) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_textParser(&pars), m_id(zId), m_what(what) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const
  {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  /** the text parser */
  MORText *m_textParser;
  //! the subdocument id
  int m_id;
  //! a int to know what to send 0: header/footer, 1: comment, 2:note
  int m_what;
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
  switch (m_what) {
  case 0: {
    std::vector<MWAWParagraph> paraStack;
    m_textParser->sendTopic(m_id,0,paraStack);
    break;
  }
  case 1:
    m_textParser->sendComment(m_id);
    break;
  case 2:
    m_textParser->sendSpeakerNote(m_id);
    break;
  default:
    MWAW_DEBUG_MSG(("SubDocument::parse: unknow value in m_what\n"));
    break;
  }
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_textParser != sDoc->m_textParser) return true;
  if (m_id != sDoc->m_id) return true;
  return m_what != sDoc->m_what;
}
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

  MWAW_DEBUG_MSG(("MORText::createZones is not called\n"));
  const_cast<MORText *>(this)->createZones();
  return m_state->m_numPages;
}

shared_ptr<MWAWSubDocument> MORText::getHeaderFooter(bool header)
{
  shared_ptr<MWAWSubDocument> res;
  size_t id=header ? 1 : 2;
  if (id >= m_state->m_topicList.size())
    return res;
  MORTextInternal::Topic const &topic= m_state->m_topicList[id];
  // check if the content is empty
  int comment=topic.m_attachList[MORTextInternal::Topic::AComment];
  if (comment < 0 || comment >= int(m_state->m_commentList.size()))
    return res;
  if (m_state->m_commentList[size_t(comment)].m_entry.length()<=4)
    return res;
  res.reset(new MORTextInternal::SubDocument(*this, m_parserState->m_input, int(id), 0));
  return res;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

//
// find/send the different zones
//
bool MORText::createZones()
{
  if (m_state->m_topicList.empty())
    return false;

  // first create the list of cloned topic
  std::vector<int> clonedList;
  for (size_t i=0; i < m_state->m_topicList.size(); i++) {
    MORTextInternal::Topic const &topic=m_state->m_topicList[i];
    if (topic.m_isCloned)
      clonedList.push_back(int(i));
  }
  // now associated each topic with its outline, its comment, its original topic (if clone)
  int numCloned=int(clonedList.size());
  size_t actAttach[3]= {0,0,0};
  size_t numAttach[3]= {0,0,0};
  numAttach[MORTextInternal::Topic::AOutline]=m_state->m_outlineList.size();
  numAttach[MORTextInternal::Topic::AComment]=m_state->m_commentList.size();
  numAttach[MORTextInternal::Topic::ASpeakerNote]=m_state->m_speakerList.size();
  for (size_t i=0; i < m_state->m_topicList.size(); i++) {
    MORTextInternal::Topic &topic=m_state->m_topicList[i];
    for (int j=0; j < 3; j++) {
      if (!topic.m_hasList[j])
        continue;
      if (actAttach[j] >= numAttach[j]) {
        MWAW_DEBUG_MSG(("MORText::createZones: can not find attach-%d for topic %d\n", int(j), int(i)));
        continue;
      }
      topic.m_attachList[j]=int(actAttach[j]++);
      switch (j) {
      case MORTextInternal::Topic::AOutline:
        break;
      case MORTextInternal::Topic::AComment: // no need to add empty comment
        if (m_state->m_commentList[(size_t)topic.m_attachList[j]].m_entry.length() <= 4)
          topic.m_attachList[j]=-1;
        break;
      case MORTextInternal::Topic::ASpeakerNote: // no need to add empty speaker note
        if (m_state->m_speakerList[(size_t)topic.m_attachList[j]].length() <= 4)
          topic.m_attachList[j]=-1;
        break;
      default:
        break;
      }
    }
    int cloneId=topic.m_cloneId;
    if (cloneId < 0)
      continue;
    if (cloneId==0 || cloneId > numCloned) {
      MWAW_DEBUG_MSG(("MORText::createZones: can not find original for topic %d\n", int(i)));
      topic.m_cloneId=-1;
    }
    else
      topic.m_cloneId=clonedList[size_t(cloneId-1)];
  }

  // now check that we have no loop
  for (size_t i=0; i < m_state->m_topicList.size(); i++) {
    MORTextInternal::Topic &topic=m_state->m_topicList[i];
    if (topic.m_cloneId<0)
      continue;
    std::set<size_t> parent;
    checkTopicList(i, parent);
  }
  // now compute the number of page
  int nPages = 1;
  for (size_t i=0; i < m_state->m_topicList.size(); i++) {
    MORTextInternal::Topic const &topic=m_state->m_topicList[i];
    if (topic.m_numPageBreak >= 0) {
      nPages+=topic.m_numPageBreak;
    }
    int outId=topic.m_attachList[MORTextInternal::Topic::AOutline];
    if (outId<0) continue;
    if (m_state->m_outlineList[(size_t)outId].m_paragraphs[0].m_pageBreak)
      nPages++;
  }
  m_state->m_actualPage = 1;
  m_state->m_numPages = nPages;
  return true;
}

int MORText::getLastTopicChildId(int tId) const
{
  size_t numTopic=m_state->m_topicList.size();
  if (tId < 0||tId>= int(numTopic)) {
    MWAW_DEBUG_MSG(("MORText::getLastTopicChildId: can not find topic %d\n", tId));
    return tId;
  }
  size_t p=size_t(tId);
  int level=m_state->m_topicList[p].m_level;
  while (p+1<numTopic && m_state->m_topicList[p].m_level>level)
    p++;
  return int(p);
}

int MORText::checkTopicList(size_t tId, std::set<size_t> &parent)
{
  size_t numTopic=m_state->m_topicList.size();
  if (tId>=numTopic) {
    MWAW_DEBUG_MSG(("MORText::checkTopicList: can not find topic %d\n", int(tId)));
    return 0;
  }
  if (parent.find(tId)!=parent.end()) {
    MWAW_DEBUG_MSG(("MORText::checkTopicList: repairing fails\n"));
    throw libmwaw::ParseException();
  }
  parent.insert(tId);
  MORTextInternal::Topic &topic=m_state->m_topicList[tId];
  int nPages=0;
  int outId=topic.m_attachList[MORTextInternal::Topic::AOutline];
  if (outId>=0) {
    if (m_state->m_outlineList[(size_t)outId].m_paragraphs[0].m_pageBreak)
      nPages++;
  }
  int id=int(tId);
  if (topic.m_cloneId>=0) {
    if (parent.find(size_t(topic.m_cloneId))!=parent.end()) {
      MWAW_DEBUG_MSG(("MORText::checkTopicList: find a loop, remove a clone\n"));
      topic.m_cloneId=-1;
      parent.erase(tId);
      return 0;
    }
    id = topic.m_cloneId;
    parent.insert(size_t(id));
  }
  int lastTId=getLastTopicChildId(id);
  for (int i=id+1; i<=lastTId; i++)
    nPages+=checkTopicList(size_t(i), parent);
  topic.m_numPageBreak=nPages;
  parent.erase(tId);
  if (id!=int(tId))
    parent.erase(size_t(tId));
  return nPages;
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

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  entry.setParsed(true);

  ascFile.addPos(pos);
  ascFile.addNote("Entries(Topic)");

  int N=int(entry.length()/10);
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
    if (flag&0x40) {
      topic.m_hasList[MORTextInternal::Topic::ASpeakerNote]=true;
      f << "S" << m_state->m_actualSpeaker++ << ",";
    }
    if (flag&0x80) {
      topic.m_hasList[MORTextInternal::Topic::AComment]=true;
      f << "C" << m_state->m_actualComment++ << ",";
    }
    if (flag&0x400) f << "showComment,";
    if (flag&0x2000) topic.m_isStartSlide=true;
    if (flag&0x8000) {
      topic.m_hasList[MORTextInternal::Topic::AOutline]=true;
      f << "O" << m_state->m_actualOutline++ << ",";
    }
    // find only bits: 5208
    flag &= 0x5B4A;
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
    int val = (int) input->readLong(2); // a small number 1 or 2
    if (val)
      f << "f1=" << val << ",";
    topic.m_extra=f.str();
    m_state->m_topicList.push_back(topic);

    f.str("");
    f << "Topic-" << i << ":" << topic;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+10, librevenge::RVNG_SEEK_SET);
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

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  entry.setParsed(true);

  ascFile.addPos(pos);
  ascFile.addNote("Entries(Comment)");

  int N=int(entry.length()/8);
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
    int val = (int) input->readLong(2); // always 4 ?
    if (val != 4)
      f << "f0=" << val << ",";
    val = (int) input->readULong(2); // some flag ? find 0x3333 0x200d ...
    if (val) f << "fl=" << std::hex << val << std::dec << ",";

    comment.m_extra=f.str();
    m_state->m_commentList.push_back(comment);
    f.str("");
    f << "Comment-C" << i << ":" << comment;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+8, librevenge::RVNG_SEEK_SET);
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

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  entry.setParsed(true);

  f << "Entries(SpeakerNote):";
  int N=int(entry.length()/4);
  for (int i=0; i < N; i++) {
    long fPos = input->readLong(4);
    f << "S" << i << ":pos=" << std::hex << fPos << std::dec << ",";
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
  return true;
}

//
// send the text
//
bool MORText::sendMainText()
{
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  std::vector<MWAWParagraph> paraStack;
  /* skip 0: unknown, 1: header(comment), 2: footer(comment), 3: title?, after text */
  for (size_t i=4; i < m_state->m_topicList.size(); i++) {
    MORTextInternal::Topic const &topic = m_state->m_topicList[i];
    MWAWEntry const &entry=topic.m_entry;
    if (!entry.valid()) { // a clone ?
      sendTopic(int(i),0,paraStack);
      continue;
    }
    ascFile.addPos(entry.end());
    ascFile.addNote("_");
    if (sendTopic(int(i),0,paraStack))
      continue;
    ascFile.addPos(entry.end());
    ascFile.addNote("_");
    f.str("");
    f << "Topic-" << i << "[data]:";
    ascFile.addPos(entry.begin());
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MORText::sendComment(int cId)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("MORText::sendComment: can not find a listener!"));
    return true;
  }
  if (cId < 0 || cId >= int(m_state->m_commentList.size())) {
    MWAW_DEBUG_MSG(("MORText::sendComment: can not find the comment %d!", cId));
    return false;
  }
  return sendText(m_state->m_commentList[size_t(cId)].m_entry, MWAWFont(3,12));
}

bool MORText::sendSpeakerNote(int nId)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("MORText::sendSpeakerNote: can not find a listener!"));
    return true;
  }
  if (nId < 0 || nId >= int(m_state->m_speakerList.size())) {
    MWAW_DEBUG_MSG(("MORText::sendSpeakerNote: can not find the speaker note %d!", nId));
    return false;
  }
  return sendText(m_state->m_speakerList[size_t(nId)], MWAWFont(3,12));
}

bool MORText::sendTopic(int tId, int dLevel, std::vector<MWAWParagraph> &paraStack)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("MORText::sendTopic: can not find a listener!"));
    return true;
  }
  if (tId < 0 || tId >= int(m_state->m_topicList.size())) {
    MWAW_DEBUG_MSG(("MORText::sendTopic: can not find the topic note %d!", tId));
    return false;
  }
  MORTextInternal::Topic const &topic=m_state->m_topicList[size_t(tId)];
  MWAWEntry entry=topic.m_entry;
  int level=topic.m_level+dLevel;
  if (level < 0) {
    if (tId>=4) {
      MWAW_DEBUG_MSG(("MORText::sendTopic: oops level is negatif!\n"));
    }
    level=0;
  }
  // data to send clone child
  int actId=tId, lastId = tId, cloneDLevel=0;
  if (tId==1||tId==2) {
    // header/footer: the data are in the comment...
    int comment=topic.m_attachList[MORTextInternal::Topic::AComment];
    if (comment<0||comment>=int(m_state->m_commentList.size()))
      return false;
    entry=m_state->m_commentList[size_t(comment)].m_entry;
  }
  else if (topic.m_cloneId>=0 && topic.m_cloneId < (int) m_state->m_topicList.size()) {
    MORTextInternal::Topic const &cTopic =
      m_state->m_topicList[size_t(topic.m_cloneId)];
    entry=cTopic.m_entry;
    actId = topic.m_cloneId;
    lastId = getLastTopicChildId(topic.m_cloneId);
    cloneDLevel = cTopic.m_level-level;;
  }
  if (!entry.valid())
    return false;
  // retrieve the ouline
  MWAWFont font(3,12);
  MORTextInternal::Paragraph para;
  int outl=topic.m_attachList[MORTextInternal::Topic::AOutline];
  if (outl>=0 && outl < (int) m_state->m_outlineList.size()) {
    MORTextInternal::Outline const &outline=m_state->m_outlineList[size_t(outl)];
    if (outline.m_paragraphs[0].m_pageBreak)
      m_mainParser->newPage(++m_state->m_actualPage);
    para=outline.m_paragraphs[0];
    font = outline.m_fonts[0];
  }
  else if (tId>=4) {
    /* default: leader is default for a paragraph

    note: sometimes, some small level are bold by default, I do not understand why ? */
    para.m_listType=1;
  }
  if (tId==1||tId==2) // force no list in header footer
    para.m_listType=0;
  if (level >= int(paraStack.size()))
    paraStack.resize(size_t(level+1));
  if (level>0)
    para.updateToFinalState(paraStack[size_t(level-1)], level,
                            *m_parserState->m_listManager);
  else
    para.updateToFinalState(MWAWParagraph(),0, *m_parserState->m_listManager);
  if (level>=0)
    paraStack[size_t(level)]=para;

  listener->setParagraph(para);
  bool ok=sendText(entry, font);
  if (tId==1||tId==2)
    return true;
  // send potential comment and speakernote
  int comment=topic.m_attachList[MORTextInternal::Topic::AComment];
  if (comment>=0) {
    MWAWSubDocumentPtr doc(new MORTextInternal::SubDocument(*this, m_parserState->m_input, comment, 1));
    listener->insertComment(doc);
  }
  int speaker=topic.m_attachList[MORTextInternal::Topic::ASpeakerNote];
  if (speaker>=0) {
    MWAWSubDocumentPtr doc(new MORTextInternal::SubDocument(*this, m_parserState->m_input, speaker, 2));
    listener->insertComment(doc);
  }

  listener->insertEOL();
  for (int i=actId+1; i <= lastId; i++)
    sendTopic(i, dLevel+cloneDLevel, paraStack);
  return ok;
}

bool MORText::sendText(MWAWEntry const &entry, MWAWFont const &font)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("MORText::sendText: can not find a listener!"));
    return true;
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  long pos = entry.begin();
  long endPos = entry.end();
  if (entry.length()==4) { // no text, we can stop here
    ascFile.addPos(pos);
    ascFile.addNote("Entries(Text):");
    ascFile.addPos(entry.end());
    ascFile.addNote("_");
    return true;
  }

  if (entry.length()<4) {
    MWAW_DEBUG_MSG(("MORText::sendText: the entry is bad\n"));
    return false;
  }

  input->seek(pos+4, librevenge::RVNG_SEEK_SET);
  listener->setFont(font);
  f << "Entries(Text):";
  int val;
  MWAWFont ft(font);
  MWAWColor defCol;
  font.getColor(defCol);
  uint32_t defFlags=font.flags();
  bool defHasUnderline=font.getUnderline().isSet();
  listener->setFont(ft);
  while (!input->isEnd()) {
    long actPos = input->tell();
    if (actPos >= endPos)
      break;
    unsigned char c=(unsigned char)input->readULong(1);
    if (c!=0x1b) {
      listener->insertCharacter(c);
      f << c;
      continue;
    }
    if (actPos+1 >= endPos) {
      f << "@[#]";
      MWAW_DEBUG_MSG(("MORText::sendText: text end by 0x1b\n"));
      continue;
    }
    int fld=(int)input->readULong(1);
    bool sendFont=true;
    switch (fld) {
    case 0x9:
      listener->insertTab();
      f << "\t";
      break;
    case 0xd: // EOL in header/footer
      listener->insertEOL();
      f << (char) 0xd;
      break;
    case 0x2d: // geneva
      ft.setId(font.id());
      sendFont=false;
      f << "@[fId=def]";
      break;
    case 0x2e: // 12
      ft.setSize(font.size());
      sendFont=false;
      f << "@[fSz=def]";
      break;
    case 0x2f: // black
      ft.setColor(defCol);
      sendFont=false;
      f << "@[fCol=def]";
      break;
    case 0x30: // font
      if (actPos+4+2 > endPos) {
        f << "@[#fId]";
        MWAW_DEBUG_MSG(("MORText::sendText: field font seems too short\n"));
        break;
      }
      val = (int) input->readULong(2);
      if (!(val&0x8000)) {
        MWAW_DEBUG_MSG(("MORText::sendText: field fId: unexpected id\n"));
        f << "@[#fId]";
        input->seek(-2, librevenge::RVNG_SEEK_CUR);
        break;
      }
      f << "@[fId=" << (val&0x7FFF) << "]";
      ft.setId(val&0x7FFF);
      sendFont = false;
      val = (int) input->readULong(2);
      if (val!=0x1b30) {
        MWAW_DEBUG_MSG(("MORText::sendText: field fId: unexpected end field\n"));
        f << "###";
        input->seek(-2, librevenge::RVNG_SEEK_CUR);
        break;
      }
      break;
    case 0x31:
      if (actPos+4+2 > endPos) {
        f << "@[#fSz]";
        MWAW_DEBUG_MSG(("MORText::sendText: field fSz seems too short\n"));
        break;
      }
      val = (int) input->readLong(2);
      f << "@[fSz=" << val << "]";
      if (val <= 0) {
        MWAW_DEBUG_MSG(("MORText::sendText: field fSz seems bad\n"));
        f << "###";
      }
      else {
        ft.setSize((float) val);
        sendFont = false;
      }
      val = (int) input->readULong(2);
      if (val!=0x1b31) {
        MWAW_DEBUG_MSG(("MORText::sendText: field fSz: unexpected end field\n"));
        f << "###";
        input->seek(-2, librevenge::RVNG_SEEK_CUR);
        break;
      }
      break;
    case 0x38: {
      if (actPos+4+10 > endPos) {
        f << "@[#fCol]";
        MWAW_DEBUG_MSG(("MORText::sendText: field fCol seems too short\n"));
        break;
      }
      uint16_t values[5];
      for (int i=0; i < 5; i++)
        values[i]=(uint16_t)input->readULong(2);
      if (values[0]!=0xe || values[4]!=0xe) {
        MWAW_DEBUG_MSG(("MORText::sendText: field fCol: color sep seems bad\n"));
        f << "@[fCol###]";
      }
      else {
        MWAWColor col((unsigned char)(values[1]>>8),
                      (unsigned char)(values[2]>>8),
                      (unsigned char)(values[3]>>8));
        ft.setColor(col);
        sendFont = false;
        f << "@[fCol=" << col << "]";
      }
      val = (int) input->readULong(2);
      if (val!=0x1b38) {
        MWAW_DEBUG_MSG(("MORText::sendText: field fCol: unexpected end field\n"));
        f << "###";
        input->seek(-2, librevenge::RVNG_SEEK_CUR);
        break;
      }
      break;
    }
    case 0x41:
      ft.set(MWAWFont::Script(33));
      sendFont = false;
      f << "@[supersc]";
      break;
    case 0x42: // in fact, (line bold)^bold
      if (defFlags&MWAWFont::boldBit)
        ft.setFlags(ft.flags()&uint32_t(~MWAWFont::boldBit));
      else
        ft.setFlags(ft.flags()|MWAWFont::boldBit);
      sendFont = false;
      f << "@[b]";
      break;
    case 0x49: // in fact, (line italic)^italic
      if (defFlags&MWAWFont::italicBit)
        ft.setFlags(ft.flags()&uint32_t(~MWAWFont::italicBit));
      else
        ft.setFlags(ft.flags()|MWAWFont::italicBit);
      sendFont = false;
      f << "@[it]";
      break;
    case 0x4c:
      ft.set(MWAWFont::Script(-33));
      sendFont = false;
      f << "@[subsc]";
      break;
    case 0x4f:
      if (defFlags&MWAWFont::outlineBit)
        ft.setFlags(ft.flags()&uint32_t(~MWAWFont::outlineBit));
      else
        ft.setFlags(ft.flags()|MWAWFont::outlineBit);
      sendFont = false;
      f << "@[outline]";
      break;
    case 0x53:
      if (defFlags&MWAWFont::shadowBit)
        ft.setFlags(ft.flags()&uint32_t(~MWAWFont::shadowBit));
      else
        ft.setFlags(ft.flags()|MWAWFont::shadowBit);
      sendFont = false;
      f << "@[shadow]";
      break;
    case 0x55:
      ft.setUnderlineStyle(defHasUnderline ? MWAWFont::Line::None : MWAWFont::Line::Simple);
      sendFont = false;
      f << "@[underl]";
      break;
    case 0x61:
      ft.set(MWAWFont::Script());
      sendFont = false;
      f << "@[script=def]";
      break;
    case 0x62:
      if ((defFlags&MWAWFont::boldBit)==0)
        ft.setFlags(ft.flags()&uint32_t(~MWAWFont::boldBit));
      else
        ft.setFlags(ft.flags()|MWAWFont::boldBit);
      sendFont = false;
      f << "@[/b]";
      break;
    case 0x69:
      if ((defFlags&MWAWFont::italicBit)==0)
        ft.setFlags(ft.flags()&uint32_t(~MWAWFont::italicBit));
      else
        ft.setFlags(ft.flags()|MWAWFont::italicBit);
      sendFont = false;
      f << "@[/it]";
      break;
    case 0x6f:
      if ((defFlags&MWAWFont::outlineBit)==0)
        ft.setFlags(ft.flags()&uint32_t(~MWAWFont::outlineBit));
      else
        ft.setFlags(ft.flags()|MWAWFont::outlineBit);
      sendFont = false;
      f << "@[/outline]";
      break;
    case 0x73:
      if ((defFlags&MWAWFont::shadowBit)==0)
        ft.setFlags(ft.flags()&uint32_t(~MWAWFont::shadowBit));
      else
        ft.setFlags(ft.flags()|MWAWFont::shadowBit);
      sendFont = false;
      f << "@[/shadow]";
      break;
    case 0x75:
      ft.setUnderlineStyle(defHasUnderline ? MWAWFont::Line::Simple : MWAWFont::Line::None);
      sendFont = false;
      f << "@[/underl]";
      break;
    case 0xb9: {
      if (actPos+4+8 > endPos) {
        f << "@[#field]";
        MWAW_DEBUG_MSG(("MORText::sendText: field b9 seems too short\n"));
        break;
      }
      uint16_t values[4];
      for (int i=0; i < 4; i++)
        values[i]=(uint16_t)input->readULong(2);
      if (values[0]!=0xc || values[3]!=0xc) {
        MWAW_DEBUG_MSG(("MORText::sendText: field the separator seems bad\n"));
        f << "@[field###]";
      }
      else {
        switch (values[1]) {
        case 1:
          listener->insertUnicodeString("#Slide#");
          f << "@[slide/title";
          if (values[2]) f << ":" << values[2]; // always 0?
          f << "]";
          break;
        case 3:
          listener->insertField(MWAWField::Title);
          f << "@[title";
          if (values[2]) f << ":" << values[2]; // always 0?
          f << "]";
          break;
        case 4:
          listener->insertUnicodeString("#Folder#");
          f << "@[folder]";
          if (values[2]) f << ":" << values[2]; // always 0?
          f << "]";
          break;
        case 5:
          listener->insertField(MWAWField::PageNumber);
          f << "@[pNumber";
          if (values[2]!=0xa) f << ":" << values[2];
          f << "]";
          break;
        case 6: // actual time
        case 7: // last modif
          listener->insertField(MWAWField::Time);
          f << "@[time";
          if (values[1]==7) f << "2";
          if (values[2]!=1) f << ":" << values[2];
          f << "]";
          break;
        case 8: // actual date
        case 9: // last modif
          listener->insertField(MWAWField::Date);
          f << "@[date";
          if (values[1]==7) f << "2";
          if (values[2]!=0x200) f << ":" << values[2];
          f << "]";
          break;
        case 0xc:
          listener->insertField(MWAWField::PageCount);
          f << "@[pNumber";
          if (values[2]!=0xa) f << ":" << values[2];
          f << "]";
          break;
        default:
          f << "@[field=##" << values[1] << ":" << values[2] << "]";
          MWAW_DEBUG_MSG(("MORText::sendText: unknown field\n"));
          break;
        }
      }
      val = (int) input->readULong(2);
      if (val!=0x1bb9) {
        MWAW_DEBUG_MSG(("MORText::sendText: field b9: unexpected end field\n"));
        f << "###";
        input->seek(-2, librevenge::RVNG_SEEK_CUR);
      }
      break;
    }
    case 0xf9: {
      if (actPos+22 > endPos) {
        f << "@[#picture]";
        MWAW_DEBUG_MSG(("MORText::sendText: field f9 seems too short\n"));
        break;
      }
      long sz=(long) input->readULong(4);
      if (sz<22 || actPos+sz>=endPos) {
        MWAW_DEBUG_MSG(("MORText::sendText: field f9: bad field size\n"));
        f << "###";
        input->seek(actPos+2, librevenge::RVNG_SEEK_CUR);
        break;
      }
      input->seek(actPos+sz-6, librevenge::RVNG_SEEK_SET);
      if ((int) input->readULong(4)!=sz &&
          (int) input->readULong(2)!=int(0x1b00|fld)) {
        MWAW_DEBUG_MSG(("MORText::sendText: find a unknown picture end field\n"));
        f << "@[#" << std::hex << fld << std::dec << ":" << sz << "]";
        input->seek(actPos+2, librevenge::RVNG_SEEK_SET);
        break;
      }
      input->seek(actPos+6, librevenge::RVNG_SEEK_SET);
      f << "[picture:";
      val = (int) input->readLong(2);
      if (val!=0x100)
        f << "type=" << std::hex << val << std::dec << ",";
      float dim[4];
      for (int i=0; i < 4; ++i)
        dim[i] = float(input->readLong(2));
      Box2f bdbox(Vec2f(dim[1],dim[0]), Vec2f(dim[3],dim[2]));
      f << "bdbox=" << bdbox << ",";
      if (sz>22) {
        shared_ptr<MWAWPict> pict(MWAWPictData::get(input, (int)sz-22));
        librevenge::RVNGBinaryData data;
        std::string type;
        if (pict && pict->getBinary(data,type)) {
          MWAWPosition pictPos(Vec2f(0,0), bdbox.size(), librevenge::RVNG_POINT);
          pictPos.m_anchorTo = MWAWPosition::Char;
          listener->insertPicture(pictPos, data, type);
        }
#ifdef DEBUG_WITH_FILES
        if (1) {
          librevenge::RVNGBinaryData file;
          input->seek(actPos+16, librevenge::RVNG_SEEK_SET);
          input->readDataBlock(sz-22, file);
          static int volatile pictName = 0;
          libmwaw::DebugStream f2;
          f2 << "Pict-" << ++pictName << ".pct";
          libmwaw::Debug::dumpFile(file, f2.str().c_str());
          ascFile.skipZone(actPos+16, actPos+sz-7);
        }
#endif
      }
      input->seek(actPos+sz, librevenge::RVNG_SEEK_SET);
      break;
    }
    default: {
      int sz=(int) input->readULong(2);
      if (sz>4 && actPos+sz<=endPos) {
        input->seek(actPos+sz-4, librevenge::RVNG_SEEK_SET);
        if ((int) input->readULong(2)==sz &&
            (int) input->readULong(2)==int(0x1b00|fld)) {
          MWAW_DEBUG_MSG(("MORText::sendText: find a unknown field, but can infer size\n"));
          f << "@[#" << std::hex << fld << std::dec << ":" << sz << "]";
          break;
        }
        input->seek(actPos+2, librevenge::RVNG_SEEK_SET);
      }
      MWAW_DEBUG_MSG(("MORText::sendText: find a unknown field\n"));
      f << "@[#" << std::hex << fld << std::dec << "]";
      break;
    }
    }
    if (!sendFont)
      listener->setFont(ft);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
  return true;
}

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

  input->seek(pos, librevenge::RVNG_SEEK_SET);
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
      input->seek(-1, librevenge::RVNG_SEEK_CUR);
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
    if ((fSz&1)==0) input->seek(1, librevenge::RVNG_SEEK_CUR);
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

  input->seek(pos, librevenge::RVNG_SEEK_SET);
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
    }
    else
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

  input->seek(pos+4, librevenge::RVNG_SEEK_SET); // skip size

  f << "Outline[O" << entry.id() << "]:";
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

  std::vector<MORTextInternal::OutlineMod> outlineModList;
  for (int n=0; n<N; n++) {
    pos = input->tell();
    f.str("");

    MORTextInternal::OutlineMod outlineMod;
    val=int(input->readLong(1));
    if (val!=6*(vers-1))
      f << "#f0=" << val << ",";

    outlineMod.m_flags=int(input->readULong(1));
    for (int i=0; i <2; i++)
      outlineMod.m_unknowns[i]=int(input->readLong(2));
    outlineMod.m_type=int(input->readULong(2));
    int values[4];
    for (int i=0; i < 4; i++)
      values[i] = (int) input->readULong(2);
    int const which= outlineMod.getModId();
    MORTextInternal::Paragraph &para = outline.m_paragraphs[which];
    MWAWFont &font=outline.m_fonts[which];
    uint32_t fFlags=font.flags();
    bool haveExtra=false;
    switch (outlineMod.m_type) {
    case 0x301: // font name
    case 0xf07: // left indent+tabs
      haveExtra=true;
      break;
    case 0x402:
      // size can be very big, force it to be smallest than 100
      if (values[0]>0 && values[0] <= 100) {
        font.setSize((float) values[0]);
        f << "sz=" << values[0] << ",";
      }
      else {
        MWAW_DEBUG_MSG(("MORText::readOutline: the font size seems bad\n"));
        f << "##sz=" << values[0] << ",";
      }
      break;
    case 0x603: {
      uint32_t bit=0;
      switch (values[0]) {
      case 0:
        f << "plain";
        if (values[1]==1)
          fFlags=0;
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
          font.setUnderlineStyle(MWAWFont::Line::Simple);
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
      if (values[1]==1) {
        if (bit) fFlags = fFlags & (~bit);
        f << "[of],";
      }
      else if (values[1]!=0)
        f << "=##" << values[1] << ",";
      else
        fFlags |= bit;
      values[1]=0;
      break;
    }
    case 0x804: {
      MWAWColor col((unsigned char)(((uint16_t)values[0])>>8),
                    (unsigned char)(((uint16_t)values[1])>>8),
                    (unsigned char)(((uint16_t)values[2])>>8));
      font.setColor(col);
      f << col << ",";
      values[1]=values[2]=0;
      break;
    }
    case 0xa05:
      if (values[0]&0x8000) {
        para.setInterline(double(values[0]&0x7FFF)/20., librevenge::RVNG_POINT, MWAWParagraph::AtLeast);
        f << "interline=" << *para.m_spacings[0] << "pt,";
      }
      else {
        para.setInterline(double(values[0])/double(0x1000), librevenge::RVNG_PERCENT);
        f << "interline=" << 100* *para.m_spacings[0] << "%,";
      }
      break;
    case 0xc0f: // firstIndent
      para.m_margins[0] = double(values[0])/1440.;
      f << "indent=" << *para.m_margins[0] << ",";
      break;
    case 0x1006:
      switch (values[0]) {
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
      }
      else {
        // assume 12pt
        para.m_spacings[1]=double(values[0])/double(0x1000)*12./72.;
        if (values[0])
          f << "bef=" << 100.*double(values[0])/double(0x1000) << "%,";
      }

      if (values[1] & 0x8000) {
        para.m_spacings[2]=double(values[1]&0x7FFF)/1440.;
        f << "aft=" << double(values[1]&0x7FFF)/20. << "pt,";
      }
      else {
        para.m_spacings[2]=double(values[1])/double(0x1000)*12./72.;
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
      if (values[0]==0x100)
        f << "pagebreak,";
      else if (values[0]==0)
        f << "no,";
      else
        f << "##break=" << std::hex << values[0] << std::dec << ",";
      break;
    case 0x1c0d:
      if (values[0]==0x100) {
        para.m_breakStatus = (*para.m_breakStatus)|MWAWParagraph::NoBreakWithNextBit;
        f << "together,";
      }
      else if (values[0]==0) {
        para.m_breakStatus = (*para.m_breakStatus)|int(~MWAWParagraph::NoBreakWithNextBit);
        f << "no,";
      }
      else
        f << "#keepLine=" << std::hex << values[0] << std::dec << ",";
      break;
    case 0x1e0e:
      para.m_keepOutlineTogether = (values[0]==0x100);
      if (values[0]==0x100)
        f << "together,";
      else if (values[0]==0)
        f << "no,";
      else
        f << "#keepOutline=" << std::hex << values[0] << std::dec << ",";
      break;
    case 0x190b:
      para.m_listType = values[0];
      switch (values[0]) {
      case 0:
        f << "no,";
        break;
      case 1:
        f << "leader,";
        break;
      case 2:
        f << "hardvard,";
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
    font.setFlags(fFlags);
    if (values[1]) f << "g0=" << std::hex << values[1] << std::dec << ",";

    if (haveExtra && values[3]>0 &&
        lastListPos+values[2]+values[3] <= endPos) {
      outlineMod.m_entry.setBegin(lastListPos+values[2]);
      outlineMod.m_entry.setLength(values[3]);
      outlineMod.m_entry.setId(n);
    }
    else {
      for (int i=2; i < 4; i++) {
        if (values[i])
          f << "g" << i-1 << "=" << std::hex << values[i] << std::dec << ",";
      }
    }
    outlineMod.m_extra=f.str();
    f.str("");
    f << "Outline[O" << entry.id() << "-" << n << "]:" << outlineMod;
    outlineModList.push_back(outlineMod);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+16, librevenge::RVNG_SEEK_SET);
  }

  for (size_t n=0; n < outlineModList.size(); n++) {
    MORTextInternal::OutlineMod const &outlineMod=outlineModList[n];
    if (!outlineMod.m_entry.valid())
      continue;
    f.str("");
    f << "Outline[O" << entry.id() << "-A" << n << "]:";
    bool ok=false;

    MORTextInternal::Paragraph &para = outline.m_paragraphs[outlineMod.getModId()];
    MWAWFont &font=outline.m_fonts[outlineMod.getModId()];
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
        font.setId(fId);
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
      ok = readCustomListLevel(outlineMod.m_entry, para.m_customListLevel);
      if (!ok) break;
      f << para.m_customListLevel << ",";
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
  input->seek(pos, librevenge::RVNG_SEEK_SET);

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
  if ((fSz%2)==0) input->seek(1,librevenge::RVNG_SEEK_CUR);
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
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  MWAWFont font;
  int fId=(int) input->readULong(2);
  if (fId==0xFFFF) // default
    ;
  else if (fId&0x8000) {
    f << "fId=" << (fId&0x7FFF) << ",";
    font.setId(fId&0x7FFF);
  }
  else
    f << "#fId=" << std::hex << fId << std::dec << ",";
  int fSz = (int) input->readLong(2);
  if (fSz != -1) {
    font.setSize((float) fSz);
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
    input->seek(6, librevenge::RVNG_SEEK_CUR);
  else if (fColor==3) {
    unsigned char color[3];
    for (int i=0; i < 3; i++)
      color[i]=(unsigned char)(input->readULong(2)>>8);
    font.setColor(MWAWColor(color[0], color[1], color[2]));
  }
  else {
    f << "#fCol=" << fColor << ",";
    input->seek(6, librevenge::RVNG_SEEK_CUR);
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
      input->seek(-1,librevenge::RVNG_SEEK_CUR);
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
      libmwaw::appendUnicode(uint32_t(unicode), level.m_label);
    else if (ch==0x9 || ch > 0x1f)
      libmwaw::appendUnicode((uint32_t)c, level.m_label);
    else {
      f << "##";
      MWAW_DEBUG_MSG(("MORText::readCustomListLevel: label char seems bad\n"));
      libmwaw::appendUnicode('#', level.m_label);
    }
  }
  f << ",";
  level.m_type=MWAWListLevel::LABEL;
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
  input->seek(pos, librevenge::RVNG_SEEK_SET);

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
    switch (val&0xF) {
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
    switch (val>>4) {
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
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (m_mainParser->readPattern(entry.end(),pattern)) {
    f << pattern;
    if (input->tell()!=entry.end())
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos+fDecal);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  // can we find a backsidde here
  input->seek(pos, librevenge::RVNG_SEEK_SET);
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
