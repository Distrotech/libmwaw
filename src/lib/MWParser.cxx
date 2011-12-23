/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * For further information visit http://libmwaw.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

/* Inspired of TN-012-Disk-Based-MW-Format.txt */

#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include <libwpd/WPXString.h>

#include "TMWAWPosition.hxx"
#include "TMWAWPictMac.hxx"
#include "TMWAWPrint.hxx"

#include "IMWAWHeader.hxx"

#include "MWAWStruct.hxx"
#include "MWAWTools.hxx"
#include "MWAWContentListener.hxx"

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
//! the paragraph/.. information
struct Information {
  enum Type { TEXT, RULER, GRAPHIC, PAGEBREAK, UNKNOWN };

  //! constructor
  Information() :
    m_type(UNKNOWN),  m_compressed(false), m_pos(), m_height(0),
    m_justify(DMWAW_PARAGRAPH_JUSTIFICATION_LEFT), m_justifySet(false),
    m_data(),m_font()
  {}

  friend std::ostream &operator<<(std::ostream &o, Information const &info);

  //! the type
  Type m_type;

  //! a flag to know if the text data are compressed
  bool m_compressed;

  //! top left position
  TMWAWPosition m_pos;

  //! the paragraph height
  int m_height;

  //! paragraph justification : DMWAW_PARAGRAPH_JUSTIFICATION*
  int m_justify;

  //! true if the justification must be used
  bool m_justifySet;
  //! the position in the file
  IMWAWEntry m_data;

  //! the font
  MWAWStruct::Font m_font;
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
  default:
    o << "###unknownType,";
    break;
  }
  o << info.m_pos << ",";
  if (info.m_height) o << "height=" << info.m_height << ",";

  if (info.m_justifySet) {
    switch (info.m_justify) {
    case DMWAW_PARAGRAPH_JUSTIFICATION_LEFT:
      o << "left[justify],";
      break;
    case DMWAW_PARAGRAPH_JUSTIFICATION_CENTER:
      o << "center[justify],";
      break;
    case DMWAW_PARAGRAPH_JUSTIFICATION_RIGHT:
      o << "right[justify],";
      break;
    case DMWAW_PARAGRAPH_JUSTIFICATION_FULL:
      o << "full[justify],";
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
struct Paragraph {
  //! Constructor
  Paragraph() :  m_spacing(1.), m_tabs(), m_justify (DMWAW_PARAGRAPH_JUSTIFICATION_LEFT) {
    for(int c = 0; c < 3; c++) m_margins[c] = 0.0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Paragraph const &ind) {
    if (ind.m_justify) {
      o << "Just=";
      switch(ind.m_justify) {
      case DMWAW_PARAGRAPH_JUSTIFICATION_LEFT:
        o << "left";
        break;
      case DMWAW_PARAGRAPH_JUSTIFICATION_CENTER:
        o << "centered";
        break;
      case DMWAW_PARAGRAPH_JUSTIFICATION_RIGHT:
        o << "right";
        break;
      case DMWAW_PARAGRAPH_JUSTIFICATION_FULL:
        o << "full";
        break;
      default:
        o << "#just=" << ind.m_justify << ", ";
        break;
      }
      o << ", ";
    }
    if (ind.m_spacing != 1.0) o << "spacing=" << ind.m_spacing << ", ";
    if (ind.m_margins[0]) o << "firstLPos=" << ind.m_margins[0] << ", ";
    if (ind.m_margins[1]) o << "leftPos=" << ind.m_margins[1] << ", ";
    if (ind.m_margins[2]) o << "rightPos=" << ind.m_margins[2] << ", ";

    libmwaw::internal::printTabs(o, ind.m_tabs);
    return o;
  }

  /** the margins in inches
   *
   * 0: first line left, 1: left, 2: right
   */
  float m_margins[3];
  /** the spacing */
  float m_spacing;

  //! the tabulations
  std::vector<DMWAWTabStop> m_tabs;
  //! paragraph justification : DWPS_PARAGRAPH_JUSTIFICATION*
  int m_justify;
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
    for (int i = 0; i < int(m_informations.size()); i++) {
      switch (m_informations[i].m_type) {
      case Information::GRAPHIC:
        return false;
      case Information::TEXT:
        if (m_informations[i].m_data.length() != 10)
          return false;
        // empty line : ok
        break;
      case Information::RULER:
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
  State() : m_version(-1), m_compressCorr(""), m_actPage(0), m_numPages(0), m_fileHeader(),
    m_headerHeight(0), m_footerHeight(0)

  {
    m_compressCorr = " etnroaisdlhcfp";
  }

  //! the file version
  int m_version;
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
class SubDocument : public IMWAWSubDocument
{
public:
  SubDocument(MWParser &pars, TMWAWInputStreamPtr input, int zoneId) :
    IMWAWSubDocument(&pars, input, IMWAWEntry()), m_id(zoneId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(IMWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(IMWAWSubDocument const &doc) const {
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
  void parse(IMWAWContentListenerPtr &listener, DMWAWSubDocumentType type);

protected:
  //! the subdocument id
  int m_id;
};

void SubDocument::parse(IMWAWContentListenerPtr &listener, DMWAWSubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  MWContentListener *listen = dynamic_cast<MWContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
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

bool SubDocument::operator!=(IMWAWSubDocument const &doc) const
{
  if (IMWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_id != sDoc->m_id) return true;
  return false;
}
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MWParser::MWParser(TMWAWInputStreamPtr input, IMWAWHeader * header) :
  IMWAWParser(input, header), m_listener(), m_convertissor(), m_state(),
  m_pageSpan(), m_listSubDocuments(), m_asciiFile(), m_asciiName("")
{
  init();
}

MWParser::~MWParser()
{
  if (m_listener.get()) m_listener->endDocument();
}

void MWParser::init()
{
  m_convertissor.reset(new MWAWTools::Convertissor);
  m_listener.reset();
  m_asciiName = "main-1";

  m_state.reset(new MWParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  m_pageSpan.setMarginTop(0.1);
  m_pageSpan.setMarginBottom(0.1);
  m_pageSpan.setMarginLeft(0.1);
  m_pageSpan.setMarginRight(0.1);
}

void MWParser::setListener(MWContentListenerPtr listen)
{
  m_listener = listen;
}

int MWParser::version() const
{
  return m_state->m_version;
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
float MWParser::pageHeight() const
{
  return m_pageSpan.getFormLength()-m_pageSpan.getMarginTop()-m_pageSpan.getMarginBottom()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0;
}

float MWParser::pageWidth() const
{
  return m_pageSpan.getFormWidth()-m_pageSpan.getMarginLeft()-m_pageSpan.getMarginRight();
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
    m_listener->insertBreak(DMWAW_PAGE_BREAK);
  }
}



////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MWParser::parse(WPXDocumentInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw_libwpd::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
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

  if (!ok) throw(libmwaw_libwpd::ParseException());
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
  std::list<DMWAWPageSpan> pageList;
  DMWAWPageSpan ps(m_pageSpan);
  for (int i = 1; i < 3; i++) {
    if (m_state->m_windows[i].isEmpty()) {
#ifdef DEBUG
      sendWindow(i); // force the parsing
#endif
      continue;
    }
    DMWAWTableList tableList;
    shared_ptr<MWParserInternal::SubDocument> subdoc
    (new MWParserInternal::SubDocument(*this, getInput(), i));
    m_listSubDocuments.push_back(subdoc);
    ps.setHeaderFooter((i==1) ? HEADER : FOOTER, 0, ALL, subdoc.get(), tableList);
  }

  for (int i = 0; i <= m_state->m_numPages; i++) pageList.push_back(ps);

  //
  MWContentListenerPtr listen =
    MWContentListener::create(pageList, documentInterface, m_convertissor);
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
  TMWAWInputStreamPtr input = getInput();
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
    for (int j=0; j < int(info.m_informations.size()); j++)
      height+=info.m_informations[j].m_height;
    if (i == 1) m_state->m_headerHeight = height;
    else m_state->m_footerHeight = height;
  }
  int numPages = 0;
  MWParserInternal::WindowsInfo const &mainInfo = m_state->m_windows[0];
  for (int i=0; i < int(mainInfo.m_informations.size()); i++) {
    if (mainInfo.m_informations[i].m_pos.page() > numPages)
      numPages = mainInfo.m_informations[i].m_pos.page();
  }
  m_state->m_numPages = numPages+1;

  return true;
}

bool MWParser::createZonesV3()
{
  TMWAWInputStreamPtr input = getInput();
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
      int type = input->readLong(2);
      int sz = input->readLong(2);
      input->seek(pos+4+sz, WPX_SEEK_SET);
      if (sz < 0 || long(input->tell()) !=  pos+4+sz) {
        MWAW_DEBUG_MSG(("MWParser::createZonesV3: pb with dataZone\n"));
        return (p != 0);
      }
      IMWAWEntry entry;
      entry.setBegin(pos+4);
      entry.setLength(sz);
      if (int(wInfo.m_informations.size()) <= p)
        continue;
      wInfo.m_informations[p].m_data = entry;
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
      if (newType != wInfo.m_informations[p].m_type) {
        MWAW_DEBUG_MSG(("MWParser::createZonesV3: types are inconstant\n"));
        if (newType != MWParserInternal::Information::UNKNOWN)
          wInfo.m_informations[p].m_type = newType;
      }
    }
  }
  if (!input->atEOS()) {
    ascii().addPos(input->tell());
    ascii().addNote("Entries(END)");
  }

  int numPages = 0;
  MWParserInternal::WindowsInfo const &mainInfo = m_state->m_windows[0];
  for (int i=0; i < int(mainInfo.m_informations.size()); i++) {
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
  int numInfo = info.m_informations.size();
  int numPara = info.m_firstParagLine.size();

  // first send the graphic
  for (int i=0; i < numInfo; i++) {
    if (info.m_informations[i].m_type != MWParserInternal::Information::GRAPHIC)
      continue;
    readGraphic(info.m_informations[i]);
  }

  if (version() <= 3 && zone == 0)
    newPage(1);
  for (int i=0; i < numInfo; i++) {
    if (zone == 0)
      newPage(info.m_informations[i].m_pos.page()+1);
    switch(info.m_informations[i].m_type) {
    case MWParserInternal::Information::TEXT:
      if (!zone || info.m_informations[i].m_data.length() != 10) {
        std::vector<int> lineHeight;
        if (i < numPara) {
          int firstLine = info.m_firstParagLine[i];
          int lastLine = (i+1 < numPara) ?  info.m_firstParagLine[i+1] : int(info.m_linesHeight.size());
          for (int line = firstLine; line < lastLine; line++)
            lineHeight.push_back(info.m_linesHeight[line]);
        }
        readText(info.m_informations[i], lineHeight);
      }
      break;
    case MWParserInternal::Information::RULER:
      readParagraph(info.m_informations[i]);
      break;
    case MWParserInternal::Information::GRAPHIC:
      if (m_listener && info.m_informations[i].m_height) {
        // insert a line to take care of the graphic size
        m_listener->lineSpacingChange(info.m_informations[i].m_height, WPX_POINT);
        m_listener->insertEOL();
      }
      break;
    case MWParserInternal::Information::PAGEBREAK:
      readPageBreak(info.m_informations[i]);
      if (zone == 0 && version() <= 3)
        newPage(info.m_informations[i].m_pos.page()+2);
      break;
    default:
      break;
    }
  }
  if (m_listener && zone) {
    // FIXME: try to insert field in the good place
    if (info.m_pageNumber.x() >= 0 && info.m_pageNumber.y() >= 0)
      m_listener->insertField(IMWAWContentListener::PageNumber);
    if (info.m_date.x() >= 0 && info.m_date.y() >= 0)
      m_listener->insertField(IMWAWContentListener::Date);
    if (info.m_time.x() >= 0 && info.m_time.y() >= 0)
      m_listener->insertField(IMWAWContentListener::Time);
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
bool MWParser::checkHeader(IMWAWHeader *header, bool strict)
{
  *m_state = MWParserInternal::State();
  MWParserInternal::FileHeader fHeader = m_state->m_fileHeader;

  TMWAWInputStreamPtr input = getInput();

  libmwaw_tools::DebugStream f;
  int headerSize=40;
  input->seek(headerSize,WPX_SEEK_SET);
  if (int(input->tell()) != headerSize) {
    MWAW_DEBUG_MSG(("MWParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,WPX_SEEK_SET);

  int vers = input->readULong(2);
  m_state->m_version = vers;

  std::string vName("");

  switch (vers) {
  case 3:
    vName="v2.2";
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
    vName="v4.5-4.6";
    m_state->m_version = vers;
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

  if (version() <= 3) fHeader.m_dataPos = input->readULong(2);

  for (int i = 0; i < 3; i++) {
    int numParag = input->readLong(2);
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
    fHeader.m_startNumberPage = input->readLong(2);
    headerSize=20;
  } else {
    fHeader.m_hideFirstPageHeaderFooter = (input->readULong(1)==0xFF);

    input->seek(7, WPX_SEEK_CUR); // unused + 4 display flags + active doc
    fHeader.m_startNumberPage = input->readLong(2);
    fHeader.m_freeListPos = input->readULong(4);
    fHeader.m_freeListLength = input->readULong(2);
    fHeader.m_freeListAllocated = input->readULong(2);
    // 14 unused
  }
  f << fHeader;

  // ok, we can finish initialization
  if (header) {
    header->setMajorVersion(m_state->m_version);
    header->setType(IMWAWDocument::MW);
  }

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
  TMWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw_tools::DebugStream f;
  // print info
  libmwaw_tools_mac::PrintInfo info;
  if (!info.read(input)) return false;
  f << "Entries(PrintInfo):"<< info;

  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  // define margin from print info
  Vec2i lTopMargin= -1 * info.paper().pos(0);
  Vec2i rBotMargin=info.paper().size() - info.page().size();

  // move margin left | top
  int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
  int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
  lTopMargin -= Vec2i(decalX, decalY);
  rBotMargin += Vec2i(decalX, decalY);

  // decrease right | bottom
  int rightMarg = rBotMargin.x() -50;
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
  TMWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  int windowsSize = version() <= 3 ? 34 : 46;

  input->seek(pos+windowsSize, WPX_SEEK_SET);
  if (long(input->tell()) !=pos+windowsSize) {
    MWAW_DEBUG_MSG(("MWParser::readWindowsInfo: file is too short\n"));
    return false;
  }

  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
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

  IMWAWEntry informations;
  IMWAWEntry lineHeightEntry;

  for (int i = 0; i < 2; i++) {
    int x = input->readLong(2);
    int y = input->readLong(2);
    if (i == 0) info.m_startSel = Vec2i(x,y);
    else info.m_endSel = Vec2i(x,y);
  }

  if (version() <= 3) {
    for (int i = 0; i < 2; i++) {
      int val = input->readLong(2);
      if (val) f << "unkn" << i << "=" << val << ",";
    }
  } else {
    info.m_posTopY = input->readLong(2);
    input->seek(2,WPX_SEEK_CUR); // need to redraw
    informations.setBegin(input->readULong(4));
    informations.setLength(input->readULong(2));
    informations.setTextId(which);

    lineHeightEntry.setBegin(input->readULong(4));
    lineHeightEntry.setLength(input->readULong(2));
    lineHeightEntry.setTextId(which);

    f << std::hex
      << "lineHeight=[" << lineHeightEntry.begin() << "-" << lineHeightEntry.end() << "],"
      << "informations=[" << informations.begin() << "-" << informations.end() << "],"
      << std::dec;
  }
  for (int i = 0; i < 3; i++) {
    int x = input->readLong(2);
    int y = input->readLong(2);
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
      int val = input->readULong(1);
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
bool MWParser::readLinesHeight(IMWAWEntry const &entry, std::vector<int> &firstParagLine, std::vector<int> &linesHeight)
{
  firstParagLine.resize(0);
  linesHeight.resize(0);

  if (!entry.valid()) return false;

  TMWAWInputStreamPtr input = getInput();

  input->seek(entry.end()-1, WPX_SEEK_SET);
  if (long(input->tell()) != entry.end()-1) {
    MWAW_DEBUG_MSG(("MWParser::readLinesHeight: file is too short\n"));
    return false;
  }

  long pos = entry.begin(), endPos = entry.end();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw_tools::DebugStream f;
  int numParag=0;
  while(input->tell() != endPos) {
    pos = input->tell();
    int sz = input->readULong(2);
    if (pos+sz+2 > endPos) {
      MWAW_DEBUG_MSG(("MWParser::readLinesHeight: find odd line\n"));
      return false;
    }

    firstParagLine.push_back(linesHeight.size());
    int actHeight = 0;
    bool heightOk = false;
    f.str("");
    f << "Entries(LineHeight)[" << entry.textId() << "-" << ++numParag << "]:";
    for (int c = 0; c < sz; c++) {
      int val = input->readULong(1);
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
  firstParagLine.push_back(linesHeight.size());

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

  TMWAWInputStreamPtr input = getInput();
  long pos = input->tell();

  libmwaw_tools::DebugStream f;
  for (int i = 0; i < numEntries; i++) {
    pos = input->tell();
    MWParserInternal::Information info;
    f.str("");
    f << "Entries(Information)[" << i+1 << "]:";
    int height = input->readLong(2);
    info.m_height = height;
    if (info.m_height < 0) {
      info.m_height = 0;
      info.m_type = MWParserInternal::Information::PAGEBREAK;
    } else if (info.m_height > 0)
      info.m_type = MWParserInternal::Information::TEXT;
    else
      info.m_type = MWParserInternal::Information::RULER;

    int y = input->readLong(2);
    info.m_pos=TMWAWPosition(Vec2f(0,y), Vec2f(0, height), WPX_POINT);
    info.m_pos.setPage(input->readLong(1));
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
bool MWParser::readInformations(IMWAWEntry const &entry, std::vector<MWParserInternal::Information> &informations)
{
  informations.resize(0);

  if (!entry.valid()) return false;

  TMWAWInputStreamPtr input = getInput();

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
  int numEntries = (endPos-pos)/16;
  libmwaw_tools::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  for (int i = 0; i < numEntries; i++) {
    pos = input->tell();

    f.str("");
    f << "Entries(Information)[" << entry.textId() << "-" << i+1 << "]:";
    MWParserInternal::Information info;
    int height = input->readLong(2);
    if (height < 0) {
      info.m_type = MWParserInternal::Information::GRAPHIC;
      height *= -1;
    } else if (height == 0)
      info.m_type = MWParserInternal::Information::RULER;
    else
      info.m_type = MWParserInternal::Information::TEXT;
    info.m_height = height;

    int y = input->readLong(2);
    int page = input->readULong(1);
    input->seek(3, WPX_SEEK_CUR); // unused
    info.m_pos = TMWAWPosition(Vec2f(0,y), Vec2f(0, height), WPX_POINT);
    info.m_pos.setPage(page);

    int paragStatus = input->readULong(1);
    switch(paragStatus & 0x3) {
    case 0:
      info.m_justify = DMWAW_PARAGRAPH_JUSTIFICATION_LEFT;
      break;
    case 1:
      info.m_justify = DMWAW_PARAGRAPH_JUSTIFICATION_CENTER;
      break;
    case 2:
      info.m_justify = DMWAW_PARAGRAPH_JUSTIFICATION_RIGHT;
      break;
    case 3:
      info.m_justify = DMWAW_PARAGRAPH_JUSTIFICATION_FULL;
      break;
    default:
      break;
    }
    info.m_compressed = (paragStatus & 0x8);
    info.m_justifySet = (paragStatus & 0x20);

    // other bits used internally
    unsigned int highPos = input->readULong(1);
    info.m_data.setBegin((highPos<<16)+input->readULong(2));
    info.m_data.setLength(input->readULong(2));

    int paragFormat = input->readULong(2);
    int flags = 0;
    // bit 1 = plain
    if (paragFormat&0x2) flags |= DMWAW_BOLD_BIT;
    if (paragFormat&0x4) flags |= DMWAW_ITALICS_BIT;
    if (paragFormat&0x8) flags |= DMWAW_UNDERLINE_BIT;
    if (paragFormat&0x10) flags |= DMWAW_EMBOSS_BIT;
    if (paragFormat&0x20) flags |= DMWAW_SHADOW_BIT;
    if (paragFormat&0x40) flags |= DMWAW_SUPERSCRIPT_BIT;
    if (paragFormat&0x80) flags |= DMWAW_SUBSCRIPT_BIT;
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
      MWAW_DEBUG_MSG(("MWParser::readInformations: unknown size\n"));
    }
    if (fontSize) info.m_font.setSize(fontSize);
    if ((paragFormat >> 11)&0x1F) info.m_font.setId((paragFormat >> 11)&0x1F);

    informations.push_back(info);
    f << info;
#ifdef DEBUG
    f << "font=[" << m_convertissor->getFontDebugString(info.m_font) << "]";
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
  IMWAWEntry const &entry = info.m_data;
  if (!entry.valid()) return false;

  TMWAWInputStreamPtr input = getInput();
  input->seek(entry.end()-1, WPX_SEEK_SET);
  if (long(input->tell()) != entry.end()-1) {
    MWAW_DEBUG_MSG(("MWParser::readText: file is too short\n"));
    return false;
  }

  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw_tools::DebugStream f;
  f << "Entries(Text):";

  int numChar = input->readULong(2);
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
          actualChar = input->readULong(1);
          actVal = (actualChar >> 4);
        } else
          actVal = (actualChar & 0xf);
        actualCharSet = !actualCharSet;
        if (st == 0) {
          if (actVal == 0xf) continue;
          text += compressCorr[actVal];
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

  int formatSize = input->readULong(2);
  if ((formatSize%6)!=0 || actPos+2+formatSize > entry.end()) {
    MWAW_DEBUG_MSG(("MWParser::readText: format is too long\n"));
    return false;
  }
  int numFormat = formatSize/6;

  std::vector<int> listPos;
  std::vector<MWAWStruct::Font> listFonts;

  for (int i = 0; i < numFormat; i++) {
    int pos = input->readULong(2);

    MWAWStruct::Font font;
    font.setSize(input->readULong(1));
    int flag = input->readULong(1);
    int flags = 0;
    // bit 1 = plain
    if (flag&0x1) flags |= DMWAW_BOLD_BIT;
    if (flag&0x2) flags |= DMWAW_ITALICS_BIT;
    if (flag&0x4) flags |= DMWAW_UNDERLINE_BIT;
    if (flag&0x8) flags |= DMWAW_EMBOSS_BIT;
    if (flag&0x10) flags |= DMWAW_SHADOW_BIT;
    if (flag&0x20) flags |= DMWAW_SUPERSCRIPT_BIT;
    if (flag&0x40) flags |= DMWAW_SUBSCRIPT_BIT;
    font.setFlags(flags);
    font.setId(input->readULong(2));
    listPos.push_back(pos);
    listFonts.push_back(font);
    f << ",f" << i << "=[pos=" << pos;
#ifdef DEBUG
    f << ",font=[" << m_convertissor->getFontDebugString(font) << "]";
#endif
    f << "]";
  }

  std::vector<int> const *lHeight = &lineHeight;
  int totalHeight = info.m_height;
  std::vector<int> textLineHeight;
  if (version() <= 3) {
    std::vector<int> fParagLines;
    long pos = input->tell();
    IMWAWEntry hEntry;
    hEntry.setBegin(pos);
    hEntry.setEnd(entry.end());

    if (readLinesHeight(hEntry, fParagLines, textLineHeight)) {
      lHeight = &textLineHeight;
      totalHeight = 0;
      for (int i = 0; i < int(textLineHeight.size()); i++)
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
      m_listener->lineSpacingChange(totalHeight/double(lHeight->size()), WPX_POINT);
    else
      m_listener->lineSpacingChange(1.2, WPX_PERCENT);

    MWAWStruct::Font font;
    bool fontSent = false;
    if (!numFormat || listPos[0] != 0) {
      font = info.m_font;
      font.sendTo(m_listener.get(), m_convertissor, font, true);
      fontSent = true;
    }
    if (info.m_justifySet)
      m_listener->justificationChange(info.m_justify);

    int actFormat = 0;
    numChar = text.length();
    for (int i = 0; i < numChar; i++) {
      if (actFormat < numFormat && i == listPos[actFormat]) {
        listFonts[actFormat].sendTo(m_listener.get(), m_convertissor, font, !fontSent);
        font = listFonts[actFormat];
        fontSent = true;
        actFormat++;
      }
      unsigned char c = text[i];
      int unicode = m_convertissor->getUnicode (font, c);
      if (unicode != -1) m_listener->insertUnicode(unicode);
      else if (c == 0x9)
        m_listener->insertTab();
      else if (c == 0xd)
        m_listener->insertEOL();
      else if (c >= 29)
        m_listener->insertCharacter(c);
      else {
        MWAW_DEBUG_MSG(("MWParser::readText: find an odd character : %d\n", int(c)));
      }
    }
  }

  ascii().addPos(version()<=3 ? pos-4 : pos);
  ascii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// read a paragraph
////////////////////////////////////////////////////////////
bool MWParser::readParagraph(MWParserInternal::Information const &info)
{
  IMWAWEntry const &entry = info.m_data;
  if (!entry.valid()) return false;
  if (entry.length() != 34) {
    MWAW_DEBUG_MSG(("MWParser::readParagraph: size is odd\n"));
    return false;
  }

  MWParserInternal::Paragraph parag;
  TMWAWInputStreamPtr input = getInput();

  input->seek(entry.end()-1, WPX_SEEK_SET);
  if (long(input->tell()) != entry.end()-1) {
    MWAW_DEBUG_MSG(("MWParser::readParagraph: file is too short\n"));
    return false;
  }

  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw_tools::DebugStream f;
  f << "Entries(Paragraph):";

  parag.m_margins[1] = input->readLong(2)/80.;
  parag.m_margins[2] = input->readLong(2)/80.;
  int justify = input->readLong(1);
  switch(justify) {
  case 0:
    parag.m_justify = DMWAW_PARAGRAPH_JUSTIFICATION_LEFT;
    break;
  case 1:
    parag.m_justify = DMWAW_PARAGRAPH_JUSTIFICATION_CENTER;
    break;
  case 2:
    parag.m_justify = DMWAW_PARAGRAPH_JUSTIFICATION_RIGHT;
    break;
  case 3:
    parag.m_justify = DMWAW_PARAGRAPH_JUSTIFICATION_FULL;
    break;
  default:
    f << "##justify=" << justify << ",";
    break;
  }
  int numTabs = input->readLong(1);
  if (numTabs < 0 || numTabs > 10) {
    f << "##numTabs=" << numTabs << ",";
    numTabs = 0;
  }
  int highspacing = input->readLong(1);
  if (highspacing) {
    MWAW_DEBUG_MSG(("MWParser::readParagraph: high spacing bit set=%d\n", highspacing));
  }
  int spacing = input->readLong(1);
  parag.m_spacing = 1.+spacing/2.0;
  parag.m_margins[0] = input->readLong(2)/80.;

  parag.m_tabs.resize(numTabs);
  for (int i = 0; i < numTabs; i++) {
    int numPixel = input->readLong(2);
    enum DMWAWTabAlignment align = LEFT;
    if (numPixel < 0) {
      align = DECIMAL;
      numPixel *= -1;
    }
    parag.m_tabs[i].m_alignment = align;
    parag.m_tabs[i].m_position = numPixel/72.0;
  }
  f << parag;

  if (m_listener) {
    double textWidth = pageWidth();

    // set the margin
    m_listener->setParagraphTextIndent(parag.m_margins[0]);
    m_listener->setParagraphMargin(parag.m_margins[1], DMWAW_LEFT);

    float rPos = 0;
    if (parag.m_margins[2] >= 0.0) {
      float rPos =textWidth-parag.m_margins[2]-28./72.;
      if (rPos < 0) rPos = 0;
    }
    m_listener->setParagraphMargin(rPos, DMWAW_RIGHT);

    m_listener->setTabs(parag.m_tabs,textWidth);
    m_listener->justificationChange(parag.m_justify);
  }
  ascii().addPos(version()<=3 ? pos-4 : pos);
  ascii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// read a paragraph
////////////////////////////////////////////////////////////
bool MWParser::readPageBreak(MWParserInternal::Information const &info)
{
  IMWAWEntry const &entry = info.m_data;
  if (!entry.valid()) return false;
  if (entry.length() != 21) {
    MWAW_DEBUG_MSG(("MWParser::readPageBreak: size is odd\n"));
    return false;
  }

  MWParserInternal::Paragraph parag;
  TMWAWInputStreamPtr input = getInput();

  input->seek(entry.end()-1, WPX_SEEK_SET);
  if (long(input->tell()) != entry.end()-1) {
    MWAW_DEBUG_MSG(("MWParser::readPageBreak: file is too short\n"));
    return false;
  }

  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw_tools::DebugStream f;

  f << "Entries(PageBreak):";
  for (int i = 0; i < 2; i++) {
    int val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  int dim[2]= {0,0};
  for (int i = 0; i < 2; i++) dim[i] = input->readLong(2);
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
  IMWAWEntry const &entry = info.m_data;
  if (!entry.valid()) return false;

  if (entry.length() < 12) {
    MWAW_DEBUG_MSG(("MWParser::readGraphic: file is too short\n"));
    return false;
  }

  TMWAWInputStreamPtr input = getInput();

  input->seek(entry.end()-1, WPX_SEEK_SET);
  if (long(input->tell()) != entry.end()-1) {
    MWAW_DEBUG_MSG(("MWParser::readGraphic: file is too short\n"));
    return false;
  }
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = input->readLong(2);
  if (dim[2] < dim[0] || dim[3] < dim[1]) {
    MWAW_DEBUG_MSG(("MWParser::readGraphic: bdbox is bad\n"));
    return false;
  }
  libmwaw_tools::DebugStream f;
  f << "Entries(Graphic):";

  Box2f box;
  libmwaw_tools::Pict::ReadResult res =
    libmwaw_tools::PictData::check(input, entry.length()-8, box);
  if (res == libmwaw_tools::Pict::MWAW_R_BAD) {
    MWAW_DEBUG_MSG(("MWParser::readGraphic: can not find the picture\n"));
    return false;
  }

  Vec2f actualSize(dim[3]-dim[1], dim[2]-dim[0]), naturalSize(actualSize);
  Vec2f orig(dim[1],info.m_pos.origin().y()+m_pageSpan.getMarginTop()*72);
  if (box.size().x() > 0 && box.size().y()  > 0) naturalSize = box.size();
  TMWAWPosition pictPos=TMWAWPosition(orig,actualSize, WPX_POINT);
  pictPos.m_anchorTo =  TMWAWPosition::Page;
  pictPos.m_wrapping =  TMWAWPosition::WRunThrough;
  pictPos.setNaturalSize(naturalSize);
  pictPos.setPage(info.m_pos.page()+1);

  f << pictPos;

  // get the picture
  input->seek(pos+8, WPX_SEEK_SET);

  shared_ptr<libmwaw_tools::Pict> pict(libmwaw_tools::PictData::get(input, entry.length()-8));
  if (pict)	{
    if (m_listener) {
      WPXBinaryData data;
      std::string type;
      if (pict->getBinary(data,type))
        m_listener->insertPicture(pictPos, data, type);
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
  TMWAWInputStreamPtr input = getInput();
  long pos = m_state->m_fileHeader.m_freeListPos;
  input->seek(pos+8, WPX_SEEK_SET);
  if (long(input->tell()) != pos+8) {
    MWAW_DEBUG_MSG(("MWParser::checkFreeList: list is too short\n"));
    return false;
  }
  input->seek(pos, WPX_SEEK_SET);

  libmwaw_tools::DebugStream f;
  int num = 0;
  while(!input->atEOS()) {
    pos = input->tell();
    long freePos = input->readULong(4);
    long sz = input->readULong(4);

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
