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

/*
  300 : 8
  500 : 4,
  700 : 8 */
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

#include "MWProParser.hxx"

/** Internal: the structures of a MWProParser */
namespace MWProParserInternal
{

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
//! Internal: the state of a MWProParser
struct State {
  //! constructor
  State() : m_version(-1), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)

  {
  }

  //! the file version
  int m_version;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MWProParser
class SubDocument : public IMWAWSubDocument
{
public:
  SubDocument(MWProParser &pars, TMWAWInputStreamPtr input, int zoneId) :
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
  MWProContentListener *listen = dynamic_cast<MWProContentListener *>(listener.get());
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
  // reinterpret_cast<MWProParser *>(m_parser)->sendWindow(m_id);
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
MWProParser::MWProParser(TMWAWInputStreamPtr input, IMWAWHeader * header) :
  IMWAWParser(input, header), m_listener(), m_convertissor(), m_state(),
  m_pageSpan(), m_listSubDocuments(), m_asciiFile(), m_asciiName("")
{
  init();
}

MWProParser::~MWProParser()
{
  if (m_listener.get()) m_listener->endDocument();
}

void MWProParser::init()
{
  m_convertissor.reset(new MWAWTools::Convertissor);
  m_listener.reset();
  m_asciiName = "main-1";

  m_state.reset(new MWProParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  m_pageSpan.setMarginTop(0.1);
  m_pageSpan.setMarginBottom(0.1);
  m_pageSpan.setMarginLeft(0.1);
  m_pageSpan.setMarginRight(0.1);
}

void MWProParser::setListener(MWProContentListenerPtr listen)
{
  m_listener = listen;
}

int MWProParser::version() const
{
  return m_state->m_version;
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
float MWProParser::pageHeight() const
{
  return m_pageSpan.getFormLength()-m_pageSpan.getMarginTop()-m_pageSpan.getMarginBottom()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0;
}

float MWProParser::pageWidth() const
{
  return m_pageSpan.getFormWidth()-m_pageSpan.getMarginLeft()-m_pageSpan.getMarginRight();
}


////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void MWProParser::newPage(int number)
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
void MWProParser::parse(WPXDocumentInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw_libwpd::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    ok = createZones();
    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("MWProParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  if (!ok) throw(libmwaw_libwpd::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MWProParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (m_listener) {
    MWAW_DEBUG_MSG(("MWProParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;
  m_state->m_numPages = 1;

  // create the page list
  std::list<DMWAWPageSpan> pageList;
  DMWAWPageSpan ps(m_pageSpan);

  for (int i = 0; i <= m_state->m_numPages; i++) pageList.push_back(ps);

  //
  MWProContentListenerPtr listen =
    MWProContentListener::create(pageList, documentInterface, m_convertissor);
  setListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool MWProParser::createZones()
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
  if (!readDocHeader()) {
    ascii().addPos(pos);
    ascii().addNote("##Entries(Data0)");
    input->seek(0x200, WPX_SEEK_SET);
  }

  long val;
  bool ok = readStyles() && readCharStyles();
  if (ok) {
    pos = input->tell();
    if (!readZoneA()) {
      ascii().addPos(pos);
      ascii().addNote("Entries(ZonA):#");
      input->seek(pos+16, WPX_SEEK_SET);
    }
  }

  if (ok) {
    pos = input->tell();
    ok = readFonts();
    if (!ok) {
      ascii().addPos(pos);
      ascii().addNote("Entries(Fonts):#");
    }
  }
  if (ok) {
    pos = input->tell();
    ok = readZoneB();
    if (!ok) {
      ascii().addPos(pos);
      ascii().addNote("Entries(ZonB):#");
    }
  }
  if (ok) {
    pos = input->tell();
    ok = readZoneC();
    if (!ok) {
      ascii().addPos(pos);
      ascii().addNote("Entries(ZonC):#");
    }
  }
  if (ok) {
    pos = input->tell();
    ascii().addPos(pos);
    ascii().addNote("Entries(ZonD)");
  }
  pos = input->tell();

  while(readAZone())
    pos = input->tell();
  input->seek(pos, WPX_SEEK_SET);
  while (!input->atEOS()) {
    pos = input->tell();
    val = input->readULong(2);
    if (val == 0xFFFFL) {
      input->seek(-5, WPX_SEEK_CUR);
      val = input->readULong(1);
      if (val == 0x20 /* empty ruler? */|| val == 0x64 /* font ?*/) {
        input->seek(pos-3, WPX_SEEK_SET);
        long newPos = input->tell();
        while (!input->atEOS() && readAZone())
          newPos = input->tell();
        if (newPos > pos) {
          input->seek(newPos, WPX_SEEK_SET);
          ascii().addPos(newPos);
          ascii().addNote("_");
          continue;
        }
      }
      input->seek(pos+1, WPX_SEEK_SET);
      continue;
    }
    if ((val&0xFF) == 0xFFL)
      input->seek(pos+1, WPX_SEEK_SET);
    else
      input->seek(pos+2, WPX_SEEK_SET);
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
bool MWProParser::checkHeader(IMWAWHeader *header, bool strict)
{
  *m_state = MWProParserInternal::State();

  TMWAWInputStreamPtr input = getInput();

  libmwaw_tools::DebugStream f;
  int headerSize=4;
  input->seek(headerSize+0x78,WPX_SEEK_SET);
  if (int(input->tell()) != headerSize+0x78) {
    MWAW_DEBUG_MSG(("MWProParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,WPX_SEEK_SET);

  int vers = input->readULong(2);
  int val = input->readULong(2);

  f << "FileHeader:";
  switch (vers) {
  case 4:
    vers = 1;
    if (val != 4) {
#ifdef DEBUG
      if (strict || val < 3 || val > 5)
        return false;
      f << "#unk=" << val << ",";
#else
      return false;
#endif
    }
    break;
  default:
    MWAW_DEBUG_MSG(("MWProParser::checkHeader: unknown version\n"));
    return false;
  }
  m_state->m_version = vers;
  f << "vers=" << vers << ",";
  if (strict) {
    if (!readPrintInfo())
      return false;
    input->seek(0xdd, WPX_SEEK_SET);
    // "MP" seems always in this position
    if (input->readULong(2) != 0x4d50)
      return false;
  }


  // ok, we can finish initialization
  if (header) {
    header->setMajorVersion(m_state->m_version);
    header->setType(IMWAWDocument::MWPRO);
  }

  //
  input->seek(headerSize, WPX_SEEK_SET);

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());
  ascii().addPos(headerSize);

  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool MWProParser::readPrintInfo()
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
    MWAW_DEBUG_MSG(("MWProParser::readPrintInfo: file is too short\n"));
    return false;
  }
  ascii().addPos(input->tell());

  return true;
}

bool MWProParser::readAZone()
{
  TMWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw_tools::DebugStream f;

  int val = input->readULong(1);
  if (val == 0x20 ||
      val == 0x14 || val == 0x15 /* font */) {
    f << "Entries(Zone" << std::hex << val << std::dec << "):";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+8, WPX_SEEK_SET);
    return true;
  }
  if (val == 0x64) {
    f << "Entries(Zone" << std::hex << val << std::dec << "):";
    input->seek(pos+11, WPX_SEEK_SET);
    int len = input->readULong(1);
    if (len) { // can be a style entry
      std::string name("");
      bool ok = true;
      for (int i = 0; i < len; i++) {
        int c = input->readULong(1);
        ok = (c >= 9 && c <= 0x80);
        if (!ok) {
          input->seek(pos+12, WPX_SEEK_SET);
          break;
        }
        name += char(c);
      }
      if (ok) {
        f << name;
        input->seek(pos+12+32, WPX_SEEK_SET);
      }
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return true;
  }
  input->seek(pos+26, WPX_SEEK_SET);
  val = input->readULong(2);
  if (val == 0xff) { // tab ? last N : follow by 4*N : data
    ascii().addPos(pos);
    ascii().addNote("Entries(ZoneA):");
    input->seek(pos+32, WPX_SEEK_SET);
    return true;
  }

  input->seek(pos, WPX_SEEK_SET);
  return false;
}

////////////////////////////////////////////////////////////
// read an unknown zone
////////////////////////////////////////////////////////////
bool MWProParser::readDocHeader()
{
  TMWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw_tools::DebugStream f;

  f << "Entries(Data0):";
  long val = input->readLong(1); // always 0 ?
  if (val) f << "unkn=" << val << ",";
  int N=input->readLong(2); // find 2, a, 9e, 1a
  f << "N?=" << N << ",";
  N = input->readLong(1); // almost always 0, find one time 6 ?
  if (N) f << "N1?=" << N << ",";
  val = input->readLong(2); // almost always 0x622, find also 0 and 12
  f << "f0=" << std::hex << val << std::dec << ",";
  val = input->readLong(1); // always 0 ?
  if (val) f << "unkn1=" << val << ",";
  N = input->readLong(2);
  f << "N2?=" << N << ",";
  val = input->readLong(1); // almost always 1 ( find one time 2)
  f << "f1=" << val << ",";
  int const defVal[] = { 0x64, 0/*small number between 1 and 8*/, 0x24 };
  for (int i = 0; i < 3; i++) {
    val = input->readLong(2);
    if (val != defVal[i])
      f << "f" << i+2 << "=" << val << ",";
  }
  for (int i = 5; i < 10; i++) { // always 0 ?
    val = input->readLong(1);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  val = input->readLong(2); // always 480 ?
  if (val != 0x480) f << "f10=" << val << ",";
  int dim[6];
  for (int i = 0; i < 6; i++)
    dim[i] = input->readLong(4);
  f << "dim=" << dim[1]/256. << "x" << dim[0]/256. << ",";
  f << "margins=[";
  for (int i = 2; i < 6; i++) f << dim[i]/256. << ",";
  f << "],";
  ascii().addDelimiter(input->tell(), '|');
  /** then find
      00000000fd0000000000018200000100002f00
      44[40|80] followed by something like a7c3ec07|a7c4c3c6 : 2 ptrs ?
      6f6600000000000000080009000105050506010401
   */
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(pos+97, WPX_SEEK_SET);
  pos = input->tell();
  f.str("");
  f << "Data0-A:";
  val = input->readULong(2);
  if (val != 0x4d50) // MP
    f << "#keyWord=" << std::hex << val <<std::dec;
  //always 4, 4, 6 ?
  for (int i = 0; i < 3; i++) {
    val = input->readLong(1);
    if ((i==2 && val!=6) || (i < 2 && val != 4))
      f << "f" << i << "=" << val << ",";
  }
  for (int i = 3; i < 9; i++) { // always 0 ?
    val = input->readLong(2);
    if (val) f << "f"  << i << "=" << val << ",";
  }
  // some dim ?
  f << "dim=[";
  for (int i = 0; i < 4; i++)
    f << input->readLong(2) << ",";
  f << "],";
  // always 0x48 0x48
  for (int i = 0; i < 2; i++) {
    val = input->readLong(2);
    if (val != 0x48) f << "g"  << i << "=" << val << ",";
  }
  // always 0 ?
  for (int i = 2; i < 42; i++) {
    val = input->readLong(2);
    if (val) f << "g"  << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // then junk ? (ie. find a string portion, a list of 0...),
  pos = input->tell();
  f.str("");
  f << "Data0-B:";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // interesting data seems to begin again in 0x200...
  input->seek(0x200, WPX_SEEK_SET);
  ascii().addPos(input->tell());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the font names
bool MWProParser::readFonts()
{
  TMWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw_tools::DebugStream f;

  long sz = input->readULong(2);
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  long endPos = pos+2+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MWProParser::readFonts: file is too short\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  input->seek(pos+2, WPX_SEEK_SET);
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  f << "Entries(Fonts):";
  int N=input->readULong(2);
  if (3*N+2 > sz) {
    MWAW_DEBUG_MSG(("MWProParser::readFonts: can not read the number of fonts\n"));
    input->seek(endPos, WPX_SEEK_SET);
    f << "#";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return true;
  }

  for (int ft = 0; ft < N; ft++) {
    int fId = input->readLong(2);
    f << "[id=" << fId << ",";
    int sSz = input->readULong(1);
    if (long(input->tell())+sSz > endPos) {
      MWAW_DEBUG_MSG(("MWProParser::readFonts: can not read the %d font\n", ft));
      f << "#";
      break;
    }
    std::string name("");
    for (int i = 0; i < sSz; i++)
      name += char(input->readULong(1));
    if (name.length()) {
      m_convertissor->setFontCorrespondance(fId, name);
      f << name;
    }
    f << "],";
  }

  if (long(input->tell()) != endPos)
    ascii().addDelimiter(input->tell(),'|');
  input->seek(endPos, WPX_SEEK_SET);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the styles ( character )
bool MWProParser::readCharStyles()
{
  TMWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw_tools::DebugStream f;

  long sz = input->readULong(4);
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  long endPos = pos+sz;
  if ((sz%0x42) != 0) {
    MWAW_DEBUG_MSG(("MWProParser::readCharStyles: find an odd value for sz\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MWProParser::readCharStyles: file is too short\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  input->seek(pos+4, WPX_SEEK_SET);
  f << "Entries(CharStyles):";
  int N = sz/0x42;
  f << "N=" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << "CharStyles-" << i << ":";
    int sSz = input->readULong(1);
    if (sSz > 33) {
      MWAW_DEBUG_MSG(("MWProParser::readCharStyles: string size seems odd\n"));
      sSz = 33;
      f << "#";
    }
    std::string name("");
    for (int c = 0; c < sSz; c++)
      name += char(input->readULong(1));
    f << name << ",";
    input->seek(pos+34, WPX_SEEK_SET);
    int val = input->readLong(2);
    if (val != -1) f << "unkn=" << val << ",";
    f << "ptr?=" << std::hex << input->readULong(4) << std::dec << ",";
    val = input->readLong(2); // small number between 0 and 2 (nextId?)
    if (val) f << "f0=" << val << ",";
    for (int j = 1; j < 3; j++) { // [-1,0,1], [0,1 or ee]
      val = input->readLong(1);
      if (val) f << "f" << j <<"=" << val << ",";
    }
    for (int j = 3; j < 5; j++) { // always 0 ?
      val = input->readLong(2);
      if (val) f << "f" << j <<"=" << val << ",";
    }

    int fId = input->readLong(2);
    if (fId != -1)
      f << "fId?="<< fId << ",";
    val = input->readLong(2);
    if (val!=-1 || fId != -1) // 0 28, 30, 38, 60
      f << "fFlags=" << std::hex << val << std::dec << ",";
    val = input->readLong(2); // always 0
    if (val) f << "f5=" << val << ",";
    for (int j = 0; j < 4; j++) { // [0,1,8], [0,2,4], [1,ff,24]
      val = input->readULong(1);
      if (j==3 && val == 0x64) continue;
      if (val) f << "g" << j << "=" << val << ",";
    }
    for (int j = 0; j < 4; j++) {
      val = input->readULong(2);
      if (j == 1 && val == i) continue;
      if (val) f << "h" << j << "=" << val << ",";
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

    input->seek(pos+0x42, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the styles ( paragraph )
bool MWProParser::readStyles()
{
  TMWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw_tools::DebugStream f;

  long sz = input->readULong(4);
  if ((sz%0x106) != 0) {
    MWAW_DEBUG_MSG(("MWProParser::readStyles: find an odd value for sz\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  f << "Entries(Style):";
  int N = sz/0x106;
  f << "N=" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    if (!readStyle(i)) {
      f.str("");
      f << "#Style-" << i << ":";
      input->seek(pos, WPX_SEEK_SET);
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return false;
    }
  }
  ascii().addPos(input->tell());
  ascii().addNote("_");

  return true;
}

bool MWProParser::readStyle(int styleId)
{
  TMWAWInputStreamPtr input = getInput();
  long debPos = input->tell(), pos = debPos;
  libmwaw_tools::DebugStream f;

  // checkme something is odd here
  long sz = styleId ? 0x106 : (0x106+8);
  long endPos = pos+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MWProParser::readStyle: file is too short\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  input->seek(pos, WPX_SEEK_SET);
  f << "Style-" << styleId << ":";
  int strlen = input->readULong(1);
  if (!strlen || strlen > 29) {
    MWAW_DEBUG_MSG(("MWProParser::readStyle: style name length seems bad!!"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  std::string name("");
  for (int i = 0; i < strlen; i++) // default
    name+=char(input->readULong(1));
  f << name << ",";
  input->seek(pos+5+29, WPX_SEEK_SET); // probably end of name
  int val = input->readLong(2); // almost always -1, sometimes 0 or 1
  if (val!=-1) f << "f0=" << val << ",";
  val = input->readLong(2);
  if (val) f << "f1=" << val << ","; // numTabs or idStyle?

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = input->tell();
  f.str("");
  f << "Style-" << styleId << "(II):";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(pos+32, WPX_SEEK_SET);
  // normally a series of 0020 8 zones ( tab ?)
  int const def[]= {0, 0x20, 0xFF, 0xFF, 0xFF, 0xFF, 0x2e, 0 };
  for (int i = 0; i < 20; i++) {
    pos = input->tell();
    bool isDef = true;
    for (int c = 0; c < 8; c++) {
      val = input->readULong(1);
      if (val != def[c])
        isDef = false;
    }
    ascii().addPos(pos);
    if (isDef) {
      ascii().addNote("_");
      continue;
    }
    f.str("");
    f << "Style" << styleId << "[tab" << i << "]:";
    ascii().addNote(f.str().c_str());
  }
  pos = input->tell();
  f.str("");
  f << "Style-" << styleId << "(III):";
  val = input->readLong(2);
  if (val != styleId) f << "#id=" << val << ",";
  val = input->readLong(2);
  if (val != -1) f << "nextId?=" << val << ",";
  val = input->readLong(1); // -1 0 or 1
  if (val) f << "f0=" << val << ",";
  for (int i = 1; i < 5; i++) { // 0, then 0|1, 0, 0
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  f << "fId?="<<input->readULong(1) << ",";
  val = input->readLong(1); // always 0 ?
  if (val) f << "f5=" << val << ",";
  val = input->readULong(1); // 28, 30, 38, 60
  f << "f6=" << std::hex << val << std::dec << ",";
  val = input->readLong(2); // always 0
  if (val) f << "f7=" << val << ",";
  if (styleId == 0) {
    int fl = input->readLong(2); // for all style ?
    switch (fl) {
    case 0:
      break;
    case -1:
      for (int i = 0; i < 2; i++) { // -4 0
        val = input->readLong(2);
        if (val)
          f << "g" << i << "=" << val << ",";
      }
      break;
    default:
      f << "#fl=" << fl << ",";
      break;
    }
    int N = input->readLong(2); // often equal to numStyle, but not always...
    f << "numStyle?=" << N << ",";
  }
  for (int i = 0; i < 3; i++) { // often 0, 0, 1 ( but not always)
    val = input->readULong(1);
    if (val)
      f << "g" << i << "=" << std::hex << val << std::dec << ",";
  }
  bool ok = (input->readULong(1) == 0x64);
  input->seek(-1, WPX_SEEK_CUR);

  if (ok) {
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

    pos = input->tell();
    ascii().addPos(pos);

    f.str("");
    f << "Style-" << styleId << "(end):";
    ascii().addNote(f.str().c_str());
    // almost always : 640000ffff00000000ffff but one time 640000000100000000ffff
    input->seek(pos+11, WPX_SEEK_SET);
    return true;
  }

  MWAW_DEBUG_MSG(("MWProParser::readStyle: end of style seems bad\n"));
  if (long(input->tell()) != endPos-11)
    ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  f.str("");
  f << "Style-" << styleId << "(end):###";
  ascii().addPos(endPos-11);
  ascii().addNote(f.str().c_str());
  input->seek(endPos, WPX_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
// read some unknowns zone
bool MWProParser::readZoneA()
{
  TMWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw_tools::DebugStream f;

  long endPos = pos+16;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MWProParser::readZonA: file is too short\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  input->seek(pos, WPX_SEEK_SET);
  f << "Entries(ZonA):";
  int val = input->readLong(2);
  f << "f0=" << val << ",";
  val = input->readLong(2); // 0 or -1
  if (val == -1) { // followed by -1, 0 ?
    f << "*";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+8, WPX_SEEK_SET);
    return true;
  }
  if (val) f << "f1=" << val << ",";
  for (int i = 2; i < 8; i++) {
    /* f0, f2 : small number , f4=f6 (almost always), others 0? */
    val = input->readLong(2);
    if (val && i >= 4) f << "f" << i << "=" << std::hex << val << std::dec << ",";
    else if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(endPos, WPX_SEEK_SET);
  return true;
}

bool MWProParser::readZoneB()
{
  TMWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw_tools::DebugStream f;

  int val = input->readULong(2);
  if (val==0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  f << "Entries(ZonB):val=" << val << ",";
  int N = (val+4)/5;
  if (val != 1 && val != 6) {
    MWAW_DEBUG_MSG(("MWProParser::readZonB: length is approximated\n"));
    f << "###";
  }

  long endPos = pos+N*14+2;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MWProParser::readZonB: file is too short\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  input->seek(pos+2, WPX_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int n = 0; n < N; n++) {
    pos = input->tell();
    f.str("");
    f << "ZonB" << "-" << n;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+14, WPX_SEEK_SET);
  }
  return true;
}

bool MWProParser::readZoneC()
{
  TMWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw_tools::DebugStream f;

  long sz = input->readULong(4);
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  long endPos = pos+sz;
  if ((sz%20) != 0) {
    MWAW_DEBUG_MSG(("MWProParser::readZoneC: find an odd value for sz\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MWProParser::readZoneC: file is too short\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  input->seek(pos+4, WPX_SEEK_SET);
  f << "Entries(ZonC):";
  int N = sz/20;
  f << "N=" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int n = 0; n < N; n++) {
    pos = input->tell();
    if ((pos&0xff)>=0xfa) {
      ascii().addPos(pos);
      ascii().addNote("_ZonC");
      pos = pos+4;
      input->seek(pos, WPX_SEEK_SET);
    }
    f.str("");
    f << "ZonC" << "-" << n;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+20, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read a text
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read a paragraph
////////////////////////////////////////////////////////////

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
