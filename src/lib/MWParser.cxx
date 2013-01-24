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

/* Inspired of TN-012-Disk-Based-MW-Format.txt */

#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSubDocument.hxx"

#include "MWParser.hxx"

/** Internal: the structures of a MWParser */
namespace MWParserInternal
{

//! Document header
struct FileHeader {
  FileHeader() : m_hideFirstPageHeaderFooter(false), m_startNumberPage(1),
    m_freeListPos(0), m_freeListLength(0), m_freeListAllocated(0),
    m_dataPos(0) {
    for (int i=0; i < 3; i++) m_numParagraphs[i] = 0;
  }

  friend std::ostream &operator<<(std::ostream &o, FileHeader const &header);

  //! the number of lines : text, header footer
  int m_numParagraphs[3];
  //! true if the first page header/footer must be draw
  bool m_hideFirstPageHeaderFooter;
  //! the first number page
  int m_startNumberPage;
  //! free list start position
  long m_freeListPos;
  //! free list length
  long m_freeListLength;
  //! free list allocated
  long m_freeListAllocated;
  //! the begin of data ( if version == 3)
  long m_dataPos;
};

std::ostream &operator<<(std::ostream &o, FileHeader const &header)
{
  for (int i=0; i < 3; i++) {
    if (!header.m_numParagraphs[i]) continue;
    o << "numParagraph";
    if (i==1) o << "[header]";
    else if (i==2) o << "[footer]";
    o << "=" << header.m_numParagraphs[i] << ",";
  }
  if (header.m_hideFirstPageHeaderFooter)
    o << "noHeaderFooter[FirstPage],";
  if (header.m_startNumberPage != 1)
    o << "firstPageNumber=" << header.m_startNumberPage << ",";
  if (header.m_freeListPos) {
    o << "FreeList=" << std::hex
      << header.m_freeListPos
      << "[" << header.m_freeListLength << "+" << header.m_freeListAllocated << "],"
      << std::dec << ",";
  }
  if (header.m_dataPos)
    o << "DataPos="  << std::hex << header.m_dataPos << std::dec << ",";

  return o;
}

////////////////////////////////////////
//! the paragraph... information
struct Information {
  /** the different type */
  enum Type { TEXT, RULER, GRAPHIC, PAGEBREAK, UNKNOWN };

  //! constructor
  Information() :
    m_type(UNKNOWN),  m_compressed(false), m_pos(), m_height(0),
    m_justify(libmwaw::JustificationLeft), m_justifySet(false),
    m_data(),m_font()
  {}

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Information const &info);

  //! the type
  Type m_type;

  //! a flag to know if the text data are compressed
  bool m_compressed;

  //! top left position
  MWAWPosition m_pos;

  //! the paragraph height
  int m_height;

  //! paragraph justification : MWAW_PARAGRAPH_JUSTIFICATION*
  libmwaw::Justification m_justify;

  //! true if the justification must be used
  bool m_justifySet;
  //! the position in the file
  MWAWEntry m_data;

  //! the font
  MWAWFont m_font;
};

std::ostream &operator<<(std::ostream &o, Information const &info)
{
  switch (info.m_type) {
  case Information::TEXT:
    o << "text";
    if (info.m_compressed) o << "[compressed]";
    o << ",";
    break;
  case Information::RULER:
    o << "indent,";
    break;
  case Information::GRAPHIC:
    o << "graphics,";
    break;
  case Information::PAGEBREAK:
    o << "pageBreak,";
    break;
  case Information::UNKNOWN:
  default:
    o << "###unknownType,";
    break;
  }
  o << info.m_pos << ",";
  if (info.m_height) o << "height=" << info.m_height << ",";

  if (info.m_justifySet) {
    switch (info.m_justify) {
    case libmwaw::JustificationLeft:
      o << "left[justify],";
      break;
    case libmwaw::JustificationCenter:
      o << "center[justify],";
      break;
    case libmwaw::JustificationRight:
      o << "right[justify],";
      break;
    case libmwaw::JustificationFull:
      o << "full[justify],";
      break;
    case libmwaw::JustificationFullAllLines:
      o << "fullAllLines[justify],";
      break;
    default:
      o << "###unknown[justify],";
      break;
    }
  }
  if (info.m_data.begin() > 0)
    o << std::hex << "data=[" << info.m_data.begin() << "-" << info.m_data.end() << "]," << std::dec;
  return o;
}

////////////////////////////////////////
/** Internal: class to store the paragraph properties */
struct Paragraph : public MWAWParagraph {
  //! Constructor
  Paragraph() {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Paragraph const &ind) {
    o << reinterpret_cast<MWAWParagraph const &>(ind);
    return o;
  }
};

////////////////////////////////////////
//! the windows structure
struct WindowsInfo {
  WindowsInfo() : m_startSel(), m_endSel(), m_posTopY(0),
    m_informations(),
    m_firstParagLine(), m_linesHeight(),
    m_pageNumber(), m_date(), m_time()
  { }

  /** small function used to recognized empty header or footer */
  bool isEmpty() const {
    if (m_informations.size() == 0) return true;
    if (m_pageNumber.x() >= 0 || m_date.x() >= 0 || m_time.x() >= 0)
      return false;
    if (m_informations.size() > 2) return false;
    for (size_t i = 0; i < m_informations.size(); i++) {
      switch (m_informations[i].m_type) {
      case Information::GRAPHIC:
        return false;
      case Information::TEXT:
        if (m_informations[i].m_data.length() != 10)
          return false;
        // empty line : ok
        break;
      case Information::RULER:
      case Information::PAGEBREAK:
      case Information::UNKNOWN:
      default:
        break;
      }
    }
    return true;
  }

  friend std::ostream &operator<<(std::ostream &o, WindowsInfo const &w);

  Vec2i m_startSel, m_endSel; // start end selection (parag, char)
  int m_posTopY;
  std::vector<Information> m_informations;
  std::vector<int> m_firstParagLine, m_linesHeight;
  Vec2i m_pageNumber, m_date, m_time;
};

std::ostream &operator<<(std::ostream &o, WindowsInfo const &w)
{
  o << "sel=[" << w.m_startSel << "-" << w.m_endSel << "],";
  if (w.m_posTopY) o << "windowsY=" << w.m_posTopY << ",";
  o << "pageNumberPos=" << w.m_pageNumber << ",";
  o << "datePos=" << w.m_date << ",";
  o << "timePos=" << w.m_time << ",";
  return o;
}

////////////////////////////////////////
//! Internal: the state of a MWParser
struct State {
  //! constructor
  State() : m_compressCorr(""), m_actPage(0), m_numPages(0), m_fileHeader(),
    m_headerHeight(0), m_footerHeight(0)

  {
    m_compressCorr = " etnroaisdlhcfp";
  }

  //! the correspondance between int compressed and char : must be 15 character
  std::string m_compressCorr;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  //! the header
  FileHeader m_fileHeader;

  //! the information of main document, header, footer
  WindowsInfo m_windows[3];

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MWParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MWParser &pars, MWAWInputStreamPtr input, int zoneId) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! returns the subdocument \a id
  int getId() const {
    return m_id;
  }
  //! sets the subdocument \a id
  void setId(int vid) {
    m_id = vid;
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  //! the subdocument id
  int m_id;
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  if (m_id != 1 && m_id != 2) {
    MWAW_DEBUG_MSG(("SubDocument::parse: unknown zone\n"));
    return;
  }

  assert(m_parser);

  long pos = m_input->tell();
  reinterpret_cast<MWParser *>(m_parser)->sendWindow(m_id);
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_id != sDoc->m_id) return true;
  return false;
}
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MWParser::MWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_listener(), m_convertissor(), m_state(),
  m_pageSpan()
{
  init();
}

MWParser::~MWParser()
{
  if (m_listener.get()) m_listener->endDocument();
}

void MWParser::init()
{
  m_convertissor.reset(new MWAWFontConverter);
  m_listener.reset();
  setAsciiName("main-1");

  m_state.reset(new MWParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  m_pageSpan.setMarginTop(0.1);
  m_pageSpan.setMarginBottom(0.1);
  m_pageSpan.setMarginLeft(0.1);
  m_pageSpan.setMarginRight(0.1);
}

void MWParser::setListener(MWAWContentListenerPtr listen)
{
  m_listener = listen;
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
float MWParser::pageHeight() const
{
  return float(m_pageSpan.getFormLength()-m_pageSpan.getMarginTop()-m_pageSpan.getMarginBottom()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0);
}

float MWParser::pageWidth() const
{
  return float(m_pageSpan.getFormWidth()-m_pageSpan.getMarginLeft()-m_pageSpan.getMarginRight());
}


////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void MWParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!m_listener || m_state->m_actPage == 1)
      continue;
    m_listener->insertBreak(MWAW_PAGE_BREAK);
  }
}



////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MWParser::parse(WPXDocumentInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());
    checkHeader(0L);
    if (getRSRCParser()) {
      MWAWEntry corrEntry = getRSRCParser()->getEntry("STR ", 700);
      std::string corrString("");
      if (corrEntry.valid() && getRSRCParser()->parseSTR(corrEntry, corrString)) {
        if (corrString.length() != 15) {
          MWAW_DEBUG_MSG(("MWParser::parse: resource correspondance string seems bad\n"));
        } else
          m_state->m_compressCorr = corrString;
      }
    }
    ok = (version() <= 3) ? createZonesV3() : createZones();
    if (ok) {
      createDocument(docInterface);
      sendWindow(0);
    }

    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("MWParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MWParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (m_listener) {
    MWAW_DEBUG_MSG(("MWParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan ps(m_pageSpan);
  for (int i = 1; i < 3; i++) {
    if (m_state->m_windows[i].isEmpty()) {
#ifdef DEBUG
      sendWindow(i); // force the parsing
#endif
      continue;
    }
    shared_ptr<MWAWSubDocument> subdoc(new MWParserInternal::SubDocument(*this, getInput(), i));
    ps.setHeaderFooter((i==1) ? MWAWPageSpan::HEADER : MWAWPageSpan::FOOTER, MWAWPageSpan::ALL, subdoc);
  }

  for (int i = 0; i <= m_state->m_numPages; i++) pageList.push_back(ps);

  //
  MWAWContentListenerPtr listen(new MWAWContentListener(m_convertissor, pageList, documentInterface));
  setListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool MWParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();

  if (!readPrintInfo()) {
    // bad sign, but we can try to recover
    ascii().addPos(pos);
    ascii().addNote("###PrintInfo");
    input->seek(pos+0x78, WPX_SEEK_SET);
  }

  pos = input->tell();
  for (int i = 0; i < 3; i++) {
    if (readWindowsInfo(i))
      continue;
    if (i == 2) return false; // problem on the main zone, better quit

    // reset state
    m_state->m_windows[2-i] = MWParserInternal::WindowsInfo();
    int const windowsSize = 46;

    // and try to continue
    input->seek(pos+(i+1)*windowsSize, WPX_SEEK_SET);
  }

#ifdef DEBUG
  checkFreeList();
#endif

  // ok, we can find calculate the number of pages and the header and the footer height
  for (int i = 1; i < 3; i++) {
    MWParserInternal::WindowsInfo const &info = m_state->m_windows[i];
    if (info.isEmpty()) // avoid reserving space for empty header/footer
      continue;
    int height = 0;
    for (size_t j=0; j < info.m_informations.size(); j++)
      height+=info.m_informations[j].m_height;
    if (i == 1) m_state->m_headerHeight = height;
    else m_state->m_footerHeight = height;
  }
  int numPages = 0;
  MWParserInternal::WindowsInfo const &mainInfo = m_state->m_windows[0];
  for (size_t i=0; i < mainInfo.m_informations.size(); i++) {
    if (mainInfo.m_informations[i].m_pos.page() > numPages)
      numPages = mainInfo.m_informations[i].m_pos.page();
  }
  m_state->m_numPages = numPages+1;

  return true;
}

bool MWParser::createZonesV3()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();

  if (!readPrintInfo()) {
    // bad sign, but we can try to recover
    ascii().addPos(pos);
    ascii().addNote("###PrintInfo");
    input->seek(pos+0x78, WPX_SEEK_SET);
  }

  pos = input->tell();
  for (int i = 0; i < 3; i++) {
    if (readWindowsInfo(i))
      continue;
    if (i == 2) return false; // problem on the main zone, better quit

    // reset state
    m_state->m_windows[2-i] = MWParserInternal::WindowsInfo();
    int const windowsSize = 34;

    // and try to continue
    input->seek(pos+(i+1)*windowsSize, WPX_SEEK_SET);
  }

  MWParserInternal::FileHeader const &header = m_state->m_fileHeader;

  for (int i = 0; i < 3; i++) {
    if (!readInformationsV3
        (header.m_numParagraphs[i], m_state->m_windows[i].m_informations))
      return false;
  }
  if (int(input->tell()) != header.m_dataPos) {
    MWAW_DEBUG_MSG(("MWParser::createZonesV3: pb with dataPos\n"));
    ascii().addPos(input->tell());
    ascii().addNote("###FileHeader");

    // posibility to do very bad thing from here, so we stop
    if (int(input->tell()) > header.m_dataPos)
      return false;

    // and try to continue
    input->seek(header.m_dataPos, WPX_SEEK_SET);
    if (int(input->tell()) != header.m_dataPos)
      return false;
  }
  for (int z = 0; z < 3; z++) {
    int numParag = header.m_numParagraphs[z];
    MWParserInternal::WindowsInfo &wInfo = m_state->m_windows[z];
    for (int p = 0; p < numParag; p++) {
      pos = input->tell();
      int type = (int) input->readLong(2);
      int sz = (int) input->readLong(2);
      input->seek(pos+4+sz, WPX_SEEK_SET);
      if (sz < 0 || long(input->tell()) !=  pos+4+sz) {
        MWAW_DEBUG_MSG(("MWParser::createZonesV3: pb with dataZone\n"));
        return (p != 0);
      }
      MWAWEntry entry;
      entry.setBegin(pos+4);
      entry.setLength(sz);
      if (int(wInfo.m_informations.size()) <= p)
        continue;
      wInfo.m_informations[(size_t)p].m_data = entry;
      MWParserInternal::Information::Type newType =
        MWParserInternal::Information::UNKNOWN;

      switch((type & 0x7)) {
      case 0:
        newType=MWParserInternal::Information::RULER;
        break;
      case 1:
        newType=MWParserInternal::Information::TEXT;
        break;
      case 2:
        newType=MWParserInternal::Information::PAGEBREAK;
        break;
      default:
        break;
      }
      if (newType != wInfo.m_informations[(size_t)p].m_type) {
        MWAW_DEBUG_MSG(("MWParser::createZonesV3: types are inconstant\n"));
        if (newType != MWParserInternal::Information::UNKNOWN)
          wInfo.m_informations[(size_t)p].m_type = newType;
      }
    }
  }
  if (!input->atEOS()) {
    ascii().addPos(input->tell());
    ascii().addNote("Entries(END)");
  }

  int numPages = 0;
  MWParserInternal::WindowsInfo const &mainInfo = m_state->m_windows[0];
  for (size_t i=0; i < mainInfo.m_informations.size(); i++) {
    if (mainInfo.m_informations[i].m_pos.page() > numPages)
      numPages = mainInfo.m_informations[i].m_pos.page();
  }
  m_state->m_numPages = numPages+1;
  return true;
}

bool MWParser::sendWindow(int zone)
{
  if (zone < 0 || zone >= 3) {
    MWAW_DEBUG_MSG(("MWParser::sendZone: invalid zone %d\n", zone));
    return false;
  }

  MWParserInternal::WindowsInfo const &info = m_state->m_windows[zone];
  size_t numInfo = info.m_informations.size();
  int numPara = int(info.m_firstParagLine.size());

  if (version() <= 3 && zone == 0)
    newPage(1);
  for (size_t i=0; i < numInfo; i++) {
    if (zone == 0)
      newPage(info.m_informations[i].m_pos.page()+1);
    switch(info.m_informations[i].m_type) {
    case MWParserInternal::Information::TEXT:
      if (!zone || info.m_informations[i].m_data.length() != 10) {
        std::vector<int> lineHeight;
        if (int(i) < numPara) {
          int firstLine = info.m_firstParagLine[i];
          int lastLine = (int(i+1) < numPara) ?  info.m_firstParagLine[i+1] : int(info.m_linesHeight.size());
          for (int line = firstLine; line < lastLine; line++)
            lineHeight.push_back(info.m_linesHeight[(size_t)line]);
        }
        readText(info.m_informations[i], lineHeight);
      }
      break;
    case MWParserInternal::Information::RULER:
      readParagraph(info.m_informations[i]);
      break;
    case MWParserInternal::Information::GRAPHIC:
      readGraphic(info.m_informations[i]);
      break;
    case MWParserInternal::Information::PAGEBREAK:
      readPageBreak(info.m_informations[i]);
      if (zone == 0 && version() <= 3)
        newPage(info.m_informations[i].m_pos.page()+2);
      break;
    case MWParserInternal::Information::UNKNOWN:
    default:
      break;
    }
  }
  if (m_listener && zone) {
    // FIXME: try to insert field in the good place
    if (info.m_pageNumber.x() >= 0 && info.m_pageNumber.y() >= 0)
      m_listener->insertField(MWAWContentListener::PageNumber);
    if (info.m_date.x() >= 0 && info.m_date.y() >= 0)
      m_listener->insertField(MWAWContentListener::Date);
    if (info.m_time.x() >= 0 && info.m_time.y() >= 0)
      m_listener->insertField(MWAWContentListener::Time);
  }
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MWParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MWParserInternal::State();
  MWParserInternal::FileHeader fHeader = m_state->m_fileHeader;

  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;

  libmwaw::DebugStream f;
  int headerSize=40;
  input->seek(headerSize,WPX_SEEK_SET);
  if (int(input->tell()) != headerSize) {
    MWAW_DEBUG_MSG(("MWParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,WPX_SEEK_SET);

  int vers = (int) input->readULong(2);
  setVersion(vers);

  std::string vName("");

  switch (vers) {
  case 3:
    vName="v1.0-2.2";
    break;
  case 1:
  case 2:
  case 4:
  case 5:
  case 7:
#ifndef DEBUG
    return false;
#endif
    break;
  case 6: // version 4.5 ( also version 5.01 of Claris MacWrite )
    vName="v4.5-5.01";
    break;
  default:
    MWAW_DEBUG_MSG(("MWParser::checkHeader: unknown version\n"));
    return false;
  }
  if (!vName.length()) {
    MWAW_DEBUG_MSG(("Maybe a MacWrite file unknown version(%d)\n", vers));
  } else {
    MWAW_DEBUG_MSG(("MacWrite file %s\n", vName.c_str()));
  }

  f << "FileHeader: vers=" << vers << ",";

  if (version() <= 3) fHeader.m_dataPos = (int) input->readULong(2);

  for (int i = 0; i < 3; i++) {
    int numParag = (int) input->readLong(2);
    fHeader.m_numParagraphs[i] = numParag;
    if (numParag < 0) {
      MWAW_DEBUG_MSG(("MWParser::checkHeader: numParagraphs if negative : %d\n",
                      numParag));
      return false;
    }
  }

  if (version() <= 3) {
    input->seek(6, WPX_SEEK_CUR); // unknown
    if (input->readLong(1)) f << "hasFooter(?);";
    if (input->readLong(1)) f << "hasHeader(?),";
    fHeader.m_startNumberPage = (int) input->readLong(2);
    headerSize=20;
  } else {
    fHeader.m_hideFirstPageHeaderFooter = (input->readULong(1)==0xFF);

    input->seek(7, WPX_SEEK_CUR); // unused + 4 display flags + active doc
    fHeader.m_startNumberPage = (int) input->readLong(2);
    fHeader.m_freeListPos = (long) input->readULong(4);
    fHeader.m_freeListLength = (int) input->readULong(2);
    fHeader.m_freeListAllocated = (int) input->readULong(2);
    // 14 unused
  }
  f << fHeader;

  //
  input->seek(headerSize, WPX_SEEK_SET);
  if (strict) {
    if (!readPrintInfo())
      return false;
    long testPos = version() <= 3 ? fHeader.m_dataPos : fHeader.m_freeListPos;
    input->seek(testPos, WPX_SEEK_SET);
    if (long(input->tell()) != testPos)
      return false;
  }
  input->seek(headerSize, WPX_SEEK_SET);
  m_state->m_fileHeader = fHeader;

  // ok, we can finish initialization
  if (header)
    header->reset(MWAWDocument::MW, version());

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());
  ascii().addPos(headerSize);

  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool MWParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;
  // print info
  libmwaw::PrinterInfo info;
  if (!info.read(input)) return false;
  f << "Entries(PrintInfo):"<< info;

  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  // define margin from print info
  Vec2i lTopMargin= -1 * info.paper().pos(0);
  Vec2i rBotMargin=info.paper().pos(1) - info.page().pos(1);

  // move margin left | top
  int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
  int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
  lTopMargin -= Vec2i(decalX, decalY);
  rBotMargin += Vec2i(decalX, decalY);

  // decrease right | bottom
  int rightMarg = rBotMargin.x() -10;
  if (rightMarg < 0) rightMarg=0;
  int botMarg = rBotMargin.y() -50;
  if (botMarg < 0) botMarg=0;

  m_pageSpan.setMarginTop(lTopMargin.y()/72.0);
  m_pageSpan.setMarginBottom(botMarg/72.0);
  m_pageSpan.setMarginLeft(lTopMargin.x()/72.0);
  m_pageSpan.setMarginRight(rightMarg/72.0);
  m_pageSpan.setFormLength(paperSize.y()/72.);
  m_pageSpan.setFormWidth(paperSize.x()/72.);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+0x78, WPX_SEEK_SET);
  if (long(input->tell()) != pos+0x78) {
    MWAW_DEBUG_MSG(("MWParser::readPrintInfo: file is too short\n"));
    return false;
  }
  ascii().addPos(input->tell());

  return true;
}

////////////////////////////////////////////////////////////
// read the windows info
////////////////////////////////////////////////////////////
bool MWParser::readWindowsInfo(int wh)
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  int windowsSize = version() <= 3 ? 34 : 46;

  input->seek(pos+windowsSize, WPX_SEEK_SET);
  if (long(input->tell()) !=pos+windowsSize) {
    MWAW_DEBUG_MSG(("MWParser::readWindowsInfo: file is too short\n"));
    return false;
  }

  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(Windows)";
  switch(wh) {
  case 0:
    f << "[Footer]";
    break;
  case 1:
    f << "[Header]";
    break;
  case 2:
    break;
  default:
    MWAW_DEBUG_MSG(("MWParser::readWindowsInfo: called with bad which=%d\n",wh));
    return false;
  }

  int which = 2-wh;
  MWParserInternal::WindowsInfo &info = m_state->m_windows[which];
  f << ": ";

  MWAWEntry informations;
  MWAWEntry lineHeightEntry;

  for (int i = 0; i < 2; i++) {
    int x = (int) input->readLong(2);
    int y = (int) input->readLong(2);
    if (i == 0) info.m_startSel = Vec2i(x,y);
    else info.m_endSel = Vec2i(x,y);
  }

  if (version() <= 3) {
    for (int i = 0; i < 2; i++) {
      int val = (int) input->readLong(2);
      if (val) f << "unkn" << i << "=" << val << ",";
    }
  } else {
    info.m_posTopY = (int) input->readLong(2);
    input->seek(2,WPX_SEEK_CUR); // need to redraw
    informations.setBegin((long) input->readULong(4));
    informations.setLength((long) input->readULong(2));
    informations.setId(which);

    lineHeightEntry.setBegin((long) input->readULong(4));
    lineHeightEntry.setLength((long) input->readULong(2));
    lineHeightEntry.setId(which);

    f << std::hex
      << "lineHeight=[" << lineHeightEntry.begin() << "-" << lineHeightEntry.end() << "],"
      << "informations=[" << informations.begin() << "-" << informations.end() << "],"
      << std::dec;
  }
  for (int i = 0; i < 3; i++) {
    int x = (int) input->readLong(2);
    int y = (int) input->readLong(2);
    if (i == 0) info.m_pageNumber = Vec2i(x,y);
    else if (i == 1) info.m_date = Vec2i(x,y);
    else info.m_time = Vec2i(x,y);
  }
  f << info;
  bool ok=true;
  if (version() <= 3) {
    input->seek(6,WPX_SEEK_CUR); // unknown flags: ff ff ff ff ff 00
    f << "actFont=" << input->readLong(1) << ",";
    for (int i= 0; i < 2; i++) {
      int val = (int) input->readULong(1);
      if (val==255) f << "f" << i << "=true,";
    }
    f << "flg=" << input->readLong(1);
  } else {
    input->seek(4,WPX_SEEK_CUR); // unused
    if (input->readULong(1) == 0xFF) f << "redrawOval,";
    if (input->readULong(1) == 0xFF) f << "lastOvalUpdate,";
    f << "actStyle=" << input->readLong(2) << ",";
    f << "actFont=" << input->readLong(2);

    if (!readLinesHeight(lineHeightEntry, info.m_firstParagLine, info.m_linesHeight)) {
      // ok, try to continue without lineHeight
      info.m_firstParagLine.resize(0);
      info.m_linesHeight.resize(0);
    }
    ok = readInformations(informations, info.m_informations);
    if (!ok) info.m_informations.resize(0);
  }

  input->seek(pos+windowsSize, WPX_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  ascii().addPos(input->tell());

  return ok;
}

////////////////////////////////////////////////////////////
// read the lines height
////////////////////////////////////////////////////////////
bool MWParser::readLinesHeight(MWAWEntry const &entry, std::vector<int> &firstParagLine, std::vector<int> &linesHeight)
{
  firstParagLine.resize(0);
  linesHeight.resize(0);

  if (!entry.valid()) return false;

  MWAWInputStreamPtr input = getInput();

  input->seek(entry.end()-1, WPX_SEEK_SET);
  if (long(input->tell()) != entry.end()-1) {
    MWAW_DEBUG_MSG(("MWParser::readLinesHeight: file is too short\n"));
    return false;
  }

  long pos = entry.begin(), endPos = entry.end();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  int numParag=0;
  while(input->tell() != endPos) {
    pos = input->tell();
    int sz = (int) input->readULong(2);
    if (pos+sz+2 > endPos) {
      MWAW_DEBUG_MSG(("MWParser::readLinesHeight: find odd line\n"));
      return false;
    }

    firstParagLine.push_back(int(linesHeight.size()));
    int actHeight = 0;
    bool heightOk = false;
    f.str("");
    f << "Entries(LineHeight)[" << entry.id() << "-" << ++numParag << "]:";
    for (int c = 0; c < sz; c++) {
      int val = (int) input->readULong(1);
      if (val & 0x80) {
        val &= 0x7f;
        if (!heightOk || val==0) {
          MWAW_DEBUG_MSG(("MWParser::readLinesHeight: find factor without height \n"));
          return false;
        }

        for (int i = 0; i < val-1; i++)
          linesHeight.push_back(actHeight);
        if (val != 0x7f) heightOk = false;
        f << "x" << val;
        continue;
      }
      actHeight = val;
      linesHeight.push_back(actHeight);
      heightOk = true;
      if (c) f << ",";
      f << actHeight;
    }

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

    if ((sz%2)==1) sz++;
    input->seek(pos+sz+2, WPX_SEEK_SET);
  }
  firstParagLine.push_back(int(linesHeight.size()));

  ascii().addPos(endPos);
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the entries
////////////////////////////////////////////////////////////
bool MWParser::readInformationsV3(int numEntries, std::vector<MWParserInternal::Information> &informations)
{
  informations.resize(0);

  if (numEntries < 0) return false;
  if (numEntries == 0) return true;

  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();

  libmwaw::DebugStream f;
  for (int i = 0; i < numEntries; i++) {
    pos = input->tell();
    MWParserInternal::Information info;
    f.str("");
    f << "Entries(Information)[" << i+1 << "]:";
    int height = (int) input->readLong(2);
    info.m_height = height;
    if (info.m_height < 0) {
      info.m_height = 0;
      info.m_type = MWParserInternal::Information::PAGEBREAK;
    } else if (info.m_height > 0)
      info.m_type = MWParserInternal::Information::TEXT;
    else
      info.m_type = MWParserInternal::Information::RULER;

    int y = (int) input->readLong(2);
    info.m_pos=MWAWPosition(Vec2f(0,float(y)), Vec2f(0, float(height)), WPX_POINT);
    info.m_pos.setPage((int) input->readLong(1));
    f << info;
    informations.push_back(info);

    f << "unkn1=" << std::hex << input->readULong(2) << std::dec << ",";
    f << "unkn2=" << std::hex << input->readULong(1) << std::dec << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  ascii().addPos(input->tell());
  ascii().addNote("_");

  return true;
}

////////////////////////////////////////////////////////////
// read the entries
////////////////////////////////////////////////////////////
bool MWParser::readInformations(MWAWEntry const &entry, std::vector<MWParserInternal::Information> &informations)
{
  informations.resize(0);

  if (!entry.valid()) return false;

  MWAWInputStreamPtr input = getInput();

  input->seek(entry.end()-1, WPX_SEEK_SET);
  if (long(input->tell()) != entry.end()-1) {
    MWAW_DEBUG_MSG(("MWParser::readInformations: file is too short\n"));
    return false;
  }

  long pos = entry.begin(), endPos = entry.end();
  if ((endPos-pos)%16) {
    MWAW_DEBUG_MSG(("MWParser::readInformations: entry size is odd\n"));
    return false;
  }
  int numEntries = int((endPos-pos)/16);
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  for (int i = 0; i < numEntries; i++) {
    pos = input->tell();

    f.str("");
    f << "Entries(Information)[" << entry.id() << "-" << i+1 << "]:";
    MWParserInternal::Information info;
    int height = (int) input->readLong(2);
    if (height < 0) {
      info.m_type = MWParserInternal::Information::GRAPHIC;
      height *= -1;
    } else if (height == 0)
      info.m_type = MWParserInternal::Information::RULER;
    else
      info.m_type = MWParserInternal::Information::TEXT;
    info.m_height = height;

    int y = (int) input->readLong(2);
    int page = (int) input->readULong(1);
    input->seek(3, WPX_SEEK_CUR); // unused
    info.m_pos = MWAWPosition(Vec2f(0,float(y)), Vec2f(0, float(height)), WPX_POINT);
    info.m_pos.setPage(page);

    int paragStatus = (int) input->readULong(1);
    switch(paragStatus & 0x3) {
    case 0:
      info.m_justify = libmwaw::JustificationLeft;
      break;
    case 1:
      info.m_justify = libmwaw::JustificationCenter;
      break;
    case 2:
      info.m_justify = libmwaw::JustificationRight;
      break;
    case 3:
      info.m_justify = libmwaw::JustificationFull;
      break;
    default:
      break;
    }
    info.m_compressed = (paragStatus & 0x8);
    info.m_justifySet = (paragStatus & 0x20);

    // other bits used internally
    unsigned int highPos = (unsigned int) input->readULong(1);
    info.m_data.setBegin(long(highPos<<16)+(long)input->readULong(2));
    info.m_data.setLength((long)input->readULong(2));

    int paragFormat = (int) input->readULong(2);
    uint32_t flags = 0;
    // bit 1 = plain
    if (paragFormat&0x2) flags |= MWAWFont::boldBit;
    if (paragFormat&0x4) flags |= MWAWFont::italicBit;
    if (paragFormat&0x8) info.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (paragFormat&0x10) flags |= MWAWFont::embossBit;
    if (paragFormat&0x20) flags |= MWAWFont::shadowBit;
    if (paragFormat&0x40)
      info.m_font.set(MWAWFont::Script::super100());
    if (paragFormat&0x80)
      info.m_font.set(MWAWFont::Script::sub100());
    info.m_font.setFlags(flags);

    int fontSize = 0;
    switch((paragFormat >> 8) & 7) {
    case 0:
      break;
    case 1:
      fontSize=9;
      break;
    case 2:
      fontSize=10;
      break;
    case 3:
      fontSize=12;
      break;
    case 4:
      fontSize=14;
      break;
    case 5:
      fontSize=18;
      break;
    case 6:
      fontSize=14;
      break;
    default:
      MWAW_DEBUG_MSG(("MWParser::readInformations: unknown size=7\n"));
    }
    if (fontSize) info.m_font.setSize(float(fontSize));
    if ((paragFormat >> 11)&0x1F) info.m_font.setId((paragFormat >> 11)&0x1F);

    informations.push_back(info);
    f << info;
#ifdef DEBUG
    f << "font=[" << info.m_font.getDebugString(m_convertissor) << "]";
#endif

    input->seek(pos+16, WPX_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  ascii().addPos(endPos);
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read a text
////////////////////////////////////////////////////////////
bool MWParser::readText(MWParserInternal::Information const &info,
                        std::vector<int> const &lineHeight)
{
  MWAWEntry const &entry = info.m_data;
  if (!entry.valid()) return false;

  MWAWInputStreamPtr input = getInput();
  input->seek(entry.end()-1, WPX_SEEK_SET);
  if (long(input->tell()) != entry.end()-1) {
    MWAW_DEBUG_MSG(("MWParser::readText: file is too short\n"));
    return false;
  }

  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(Text):";

  int numChar = (int) input->readULong(2);
  std::string text("");
  if (!info.m_compressed) {
    if (numChar+2 >= entry.length()) {
      MWAW_DEBUG_MSG(("MWParser::readText: text is too long\n"));
      return false;
    }
    for (int i = 0; i < numChar; i++)
      text += (char) input->readULong(1);
  } else {
    std::string compressCorr = m_state->m_compressCorr;

    int actualChar = 0;
    bool actualCharSet = false;

    for (int i = 0; i < numChar; i++) {
      int highByte = 0;
      for (int st = 0; st < 3; st++) {
        int actVal;
        if (!actualCharSet ) {
          if (long(input->tell()) >= entry.end()) {
            MWAW_DEBUG_MSG(("MWParser::readText: text is too long\n"));
            return false;
          }
          actualChar = (int) input->readULong(1);
          actVal = (actualChar >> 4);
        } else
          actVal = (actualChar & 0xf);
        actualCharSet = !actualCharSet;
        if (st == 0) {
          if (actVal == 0xf) continue;
          text += compressCorr[(size_t) actVal];
          break;
        }
        if (st == 1) { // high bytes
          highByte = (actVal<<4);
          continue;
        }
        text += (char) (highByte | actVal);
      }
    }
  }
  f << "'" << text << "'";

  long actPos = input->tell();
  if ((actPos-pos)%2==1) {
    input->seek(1,WPX_SEEK_CUR);
    actPos++;
  }

  int formatSize = (int) input->readULong(2);
  if ((formatSize%6)!=0 || actPos+2+formatSize > entry.end()) {
    MWAW_DEBUG_MSG(("MWParser::readText: format is too long\n"));
    return false;
  }
  int numFormat = formatSize/6;

  std::vector<int> listPos;
  std::vector<MWAWFont> listFonts;

  for (int i = 0; i < numFormat; i++) {
    int tPos = (int) input->readULong(2);

    MWAWFont font;
    font.setSize((float)input->readULong(1));
    int flag = (int) input->readULong(1);
    uint32_t flags = 0;
    // bit 1 = plain
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0x8) flags |= MWAWFont::embossBit;
    if (flag&0x10) flags |= MWAWFont::shadowBit;
    if (flag&0x20) font.set(MWAWFont::Script::super100());
    if (flag&0x40) font.set(MWAWFont::Script::sub100());
    font.setFlags(flags);
    font.setId((int) input->readULong(2));
    listPos.push_back(tPos);
    listFonts.push_back(font);
    f << ",f" << i << "=[pos=" << tPos;
#ifdef DEBUG
    f << ",font=[" << font.getDebugString(m_convertissor) << "]";
#endif
    f << "]";
  }

  std::vector<int> const *lHeight = &lineHeight;
  int totalHeight = info.m_height;
  std::vector<int> textLineHeight;
  if (version() <= 3) {
    std::vector<int> fParagLines;
    pos = input->tell();
    MWAWEntry hEntry;
    hEntry.setBegin(pos);
    hEntry.setEnd(entry.end());

    if (readLinesHeight(hEntry, fParagLines, textLineHeight)) {
      lHeight = &textLineHeight;
      totalHeight = 0;
      for (size_t i = 0; i < textLineHeight.size(); i++)
        totalHeight+=textLineHeight[i];
    } else
      input->seek(pos, WPX_SEEK_SET);
  }
  if (long(input->tell()) != entry.end()) {
    f << "#badend";
    ascii().addDelimiter(input->tell(), '|');
  }

  if (m_listener) {
    if (totalHeight && lHeight->size()) // fixme find a way to associate the good size to each line
      m_listener->setParagraphLineSpacing(totalHeight/double(lHeight->size()), WPX_POINT);
    else
      m_listener->setParagraphLineSpacing(1.2, WPX_PERCENT);

    if (!numFormat || listPos[0] != 0)
      m_listener->setFont(info.m_font);
    if (info.m_justifySet)
      m_listener->setParagraphJustification(info.m_justify);

    int actFormat = 0;
    numChar = int(text.length());
    for (int i = 0; i < numChar; i++) {
      if (actFormat < numFormat && i == listPos[(size_t)actFormat]) {
        m_listener->setFont(listFonts[(size_t)actFormat]);
        actFormat++;
      }
      unsigned char c = (unsigned char) text[(size_t)i];
      if (c == 0x9)
        m_listener->insertTab();
      else if (c == 0xd)
        m_listener->insertEOL();
      else
        m_listener->insertCharacter(c);
    }
  }

  ascii().addPos(version()<=3 ? entry.begin()-4 : entry.begin());
  ascii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// read a paragraph
////////////////////////////////////////////////////////////
bool MWParser::readParagraph(MWParserInternal::Information const &info)
{
  MWAWEntry const &entry = info.m_data;
  if (!entry.valid()) return false;
  if (entry.length() != 34) {
    MWAW_DEBUG_MSG(("MWParser::readParagraph: size is odd\n"));
    return false;
  }

  MWParserInternal::Paragraph parag;
  MWAWInputStreamPtr input = getInput();

  input->seek(entry.end()-1, WPX_SEEK_SET);
  if (long(input->tell()) != entry.end()-1) {
    MWAW_DEBUG_MSG(("MWParser::readParagraph: file is too short\n"));
    return false;
  }

  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(Paragraph):";

  parag.m_margins[1] = float(input->readLong(2))/80.f;
  parag.m_margins[2] = float(input->readLong(2))/80.f;
  int justify = (int) input->readLong(1);
  switch(justify) {
  case 0:
    parag.m_justify = libmwaw::JustificationLeft;
    break;
  case 1:
    parag.m_justify = libmwaw::JustificationCenter;
    break;
  case 2:
    parag.m_justify = libmwaw::JustificationRight;
    break;
  case 3:
    parag.m_justify = libmwaw::JustificationFull;
    break;
  default:
    f << "##justify=" << justify << ",";
    break;
  }
  int numTabs = (int) input->readLong(1);
  if (numTabs < 0 || numTabs > 10) {
    f << "##numTabs=" << numTabs << ",";
    numTabs = 0;
  }
  int highspacing = (int) input->readLong(1);
  if (highspacing) {
    MWAW_DEBUG_MSG(("MWParser::readParagraph: high spacing bit set=%d\n", highspacing));
  }
  int spacing = (int) input->readLong(1);
  parag.m_spacings[0] = 1.+spacing/2.0;
  parag.m_margins[0] = float(input->readLong(2))/80.f;

  parag.m_tabs->resize((size_t) numTabs);
  for (size_t i = 0; i < (size_t) numTabs; i++) {
    int numPixel = (int) input->readLong(2);
    MWAWTabStop::Alignment align = MWAWTabStop::LEFT;
    if (numPixel < 0) {
      align = MWAWTabStop::DECIMAL;
      numPixel *= -1;
    }
    (*parag.m_tabs)[i].m_alignment = align;
    (*parag.m_tabs)[i].m_position = numPixel/72.0;
  }
  *(parag.m_margins[0]) -= parag.m_margins[1].get();
  if (parag.m_margins[2].get() > 0.0)
    parag.m_margins[2]=pageWidth()-parag.m_margins[2].get()-1.0;
  if (parag.m_margins[2].get() < 0) parag.m_margins[2] = 0;
  if (parag.m_spacings[0].get() < 1.0) {
    f << "#interline=" << parag.m_spacings[0].get() << ",";
    parag.m_spacings[0] = 1.0;
  }
  f << parag;

  if (m_listener)
    parag.send(m_listener);
  ascii().addPos(version()<=3 ? pos-4 : pos);
  ascii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// read the page break
////////////////////////////////////////////////////////////
bool MWParser::readPageBreak(MWParserInternal::Information const &info)
{
  MWAWEntry const &entry = info.m_data;
  if (!entry.valid()) return false;
  if (entry.length() != 21) {
    MWAW_DEBUG_MSG(("MWParser::readPageBreak: size is odd\n"));
    return false;
  }

  MWParserInternal::Paragraph parag;
  MWAWInputStreamPtr input = getInput();

  input->seek(entry.end()-1, WPX_SEEK_SET);
  if (long(input->tell()) != entry.end()-1) {
    MWAW_DEBUG_MSG(("MWParser::readPageBreak: file is too short\n"));
    return false;
  }

  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;

  f << "Entries(PageBreak):";
  for (int i = 0; i < 2; i++) {
    int val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  int dim[2]= {0,0};
  for (int i = 0; i < 2; i++) dim[i] = (int) input->readLong(2);
  f << "pageSize(?)=" << dim[0] << "x" << dim[1] << ",";
  f << "unk=" << input->readLong(2) << ","; // find 0xd

  // find MAGICPIC
  std::string name("");
  for (int i = 0; i < 8; i++)
    name += char(input->readULong(1));
  f << name << ",";
  // then I find 1101ff: end of quickdraw pict1 ?
  ascii().addPos(version()<=3 ? pos-4 : pos);
  ascii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// read a graphic
////////////////////////////////////////////////////////////
bool MWParser::readGraphic(MWParserInternal::Information const &info)
{
  MWAWEntry const &entry = info.m_data;
  if (!entry.valid()) return false;

  if (entry.length() < 12) {
    MWAW_DEBUG_MSG(("MWParser::readGraphic: file is too short\n"));
    return false;
  }

  MWAWInputStreamPtr input = getInput();

  input->seek(entry.end()-1, WPX_SEEK_SET);
  if (long(input->tell()) != entry.end()-1) {
    MWAW_DEBUG_MSG(("MWParser::readGraphic: file is too short\n"));
    return false;
  }
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = (int) input->readLong(2);
  if (dim[2] < dim[0] || dim[3] < dim[1]) {
    MWAW_DEBUG_MSG(("MWParser::readGraphic: bdbox is bad\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(Graphic):";

  Box2f box;
  MWAWPict::ReadResult res = MWAWPictData::check(input, int(entry.length()-8), box);
  if (res == MWAWPict::MWAW_R_BAD) {
    MWAW_DEBUG_MSG(("MWParser::readGraphic: can not find the picture\n"));
    return false;
  }

  Vec2f actualSize(float(dim[3]-dim[1]), float(dim[2]-dim[0])), naturalSize(actualSize);
  if (box.size().x() > 0 && box.size().y()  > 0) naturalSize = box.size();
  MWAWPosition pictPos=MWAWPosition(Vec2i(0,0),actualSize, WPX_POINT);
  pictPos.setRelativePosition(MWAWPosition::Char);
  pictPos.setNaturalSize(naturalSize);
  f << pictPos;

  // get the picture
  input->seek(pos+8, WPX_SEEK_SET);

  shared_ptr<MWAWPict> pict(MWAWPictData::get(input, int(entry.length()-8)));
  if (pict) {
    if (m_listener) {
      m_listener->setParagraphLineSpacing(1.0, WPX_PERCENT);

      WPXBinaryData data;
      std::string type;
      if (pict->getBinary(data,type))
        m_listener->insertPicture(pictPos, data, type);
      m_listener->insertEOL();
    }
    ascii().skipZone(pos+8, entry.end()-1);
  }

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return true;
}


////////////////////////////////////////////////////////////
// read the free list
////////////////////////////////////////////////////////////
bool MWParser::checkFreeList()
{
  if (version() <= 3)
    return true;
  MWAWInputStreamPtr input = getInput();
  long pos = m_state->m_fileHeader.m_freeListPos;
  input->seek(pos+8, WPX_SEEK_SET);
  if (long(input->tell()) != pos+8) {
    MWAW_DEBUG_MSG(("MWParser::checkFreeList: list is too short\n"));
    return false;
  }
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  int num = 0;
  while(!input->atEOS()) {
    pos = input->tell();
    long freePos = (long) input->readULong(4);
    long sz = (long) input->readULong(4);

    if (long(input->tell()) != pos+8) {
      MWAW_DEBUG_MSG(("MWParser::checkFreeList: list is too short\n"));
      return false;
    }

    f.str("");
    f << "Entries(FreeList)[" << ++num << "]:" << std::hex << freePos << "-" << sz;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

    if (input->atEOS()) break;

    input->seek(freePos+sz, WPX_SEEK_SET);
    if (long(input->tell()) != freePos+sz) {
      MWAW_DEBUG_MSG(("MWParser::checkFreeList: bad free block\n"));
      return false;
    }

    f.str("");
    f << "Entries(FreeBlock)[" << num << "]:";

    ascii().addPos(freePos);
    ascii().addNote(f.str().c_str());

    input->seek(pos+8, WPX_SEEK_SET);
  }

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
