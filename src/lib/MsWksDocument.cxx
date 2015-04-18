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

#include <string.h>

#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWTextListener.hxx"
#include "MWAWHeader.hxx"
#include "MWAWOLEParser.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWParser.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSection.hxx"
#include "MWAWSubDocument.hxx"
#include "MWAWRSRCParser.hxx"

#include "MsWksGraph.hxx"
#include "MsWks3Text.hxx"
#include "MsWks4Text.hxx"
#include "MsWks4Zone.hxx"

#include "MsWksDocument.hxx"

/** Internal: the structures of a MsWksDocument */
namespace MsWksDocumentInternal
{

////////////////////////////////////////
//! Internal: the state of a MsWksDocument
struct State {
  //! constructor
  State() : m_kind(MWAWDocument::MWAW_K_TEXT), m_fileHeaderSize(0),
    m_freeZoneId(0), m_typeZoneMap(), m_entryMap(), m_hasHeader(false), m_hasFooter(false),
    m_oleParser(), m_headerParser(), m_footerParser(), m_footnoteParser(), m_frameParserMap(), m_unparsedOlesName(),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }

  //! the type of document
  MWAWDocument::Kind m_kind;
  //! the file header size
  long m_fileHeaderSize;
  //! an id to count the number of free map
  mutable int m_freeZoneId;
  //! the list of zone (for v1-v3) document
  std::multimap<int, MsWksDocument::Zone> m_typeZoneMap;
  //! the list of entries, name->entry ( for v4 document)
  std::multimap<std::string, MWAWEntry> m_entryMap;
  bool m_hasHeader /** true if there is a header v3*/, m_hasFooter /** true if there is a footer v3*/;

  // ole data
  /** the ole parser */
  shared_ptr<MWAWOLEParser> m_oleParser;
  shared_ptr<MsWks4Zone> m_headerParser /**parser of the header ole*/, m_footerParser /**parser of the footer ole*/,
             m_footnoteParser /**parser of the footnote ole*/;
  /**the frame parsers: name-> parser*/
  std::map<std::string, shared_ptr<MsWks4Zone> > m_frameParserMap;
  //! the list of unparsed OLEs
  std::vector<std::string> m_unparsedOlesName;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MsWksDocument
class SubDocument : public MWAWSubDocument
{
public:
  enum Type { Note, OLE, Zone, Text };
  SubDocument(MsWksDocument &document, MWAWInputStreamPtr input, Type type,
              int zoneId) :
    MWAWSubDocument(document.m_parser, input, MWAWEntry()), m_document(document), m_type(type), m_id(zoneId) {}

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
  void parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  /** the main document */
  MsWksDocument &m_document;
  /** the type */
  Type m_type;
  /** the subdocument id*/
  int m_id;
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("MsWksDocument::SubDocument::parse: no listener\n"));
    return;
  }

  long pos = m_input->tell();
  switch (m_type) {
  case Note:
    m_document.sendFootnoteContent(m_id);
    break;
  case OLE:
    if (m_id==MsWksDocument::Z_HEADER) m_document.sendTextbox(MWAWEntry(),"QHdr");
    else if (m_id==MsWksDocument::Z_FOOTER) m_document.sendTextbox(MWAWEntry(),"QFtr");
    else {
      MWAW_DEBUG_MSG(("MsWksDocument::SubDocument::parse: unexpected ole zone %d\n", m_id));
    }
    break;
  case Text:
    m_document.sendText(m_id);
    break;
  case Zone:
    m_document.sendZone(m_id);
    break;
  default:
    MWAW_DEBUG_MSG(("MsWksDocument::SubDocument::parse: unexpected zone type\n"));
    break;
  }
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (&m_document != &sDoc->m_document) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_type != sDoc->m_type) return true;
  return false;
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, accessor functions...
////////////////////////////////////////////////////////////
MsWksDocument::MsWksDocument(MWAWInputStreamPtr input, MWAWParser &parser) :
  m_state(), m_parserState(parser.getParserState()), m_parser(&parser), m_parentDocument(0),
  m_input(input), m_asciiFile(), m_graphParser(), m_textParser3(), m_textParser4(), m_newPage(0)
{
  m_state.reset(new MsWksDocumentInternal::State());
  m_graphParser.reset(new MsWksGraph(*this));
}

MsWksDocument::~MsWksDocument()
{
}

void MsWksDocument::initAsciiFile(std::string const &name)
{
  m_asciiFile.setStream(m_input);
  m_asciiFile.open(name);
}

shared_ptr<MsWks3Text> MsWksDocument::getTextParser3()
{
  if (!m_textParser3) m_textParser3.reset(new MsWks3Text(*this));
  return m_textParser3;
}

shared_ptr<MsWks4Text> MsWksDocument::getTextParser4()
{
  if (!m_textParser4) m_textParser4.reset(new MsWks4Text(*this));
  return m_textParser4;
}

void MsWksDocument::setVersion(int vers)
{
  m_parserState->m_version=vers;
}

int MsWksDocument::version() const
{
  return m_parserState->m_version;
}

MWAWDocument::Kind MsWksDocument::getKind() const
{
  return m_state->m_kind;
}

void MsWksDocument::setKind(MWAWDocument::Kind kind)
{
  m_state->m_kind=kind;
}

int MsWksDocument::getNewZoneId() const
{
  return MsWksDocument::Z_NONE + ++m_state->m_freeZoneId;
}

MsWksDocument::Zone MsWksDocument::getZone(MsWksDocument::ZoneType type) const
{
  Zone res;
  if (m_state->m_typeZoneMap.find(int(type)) != m_state->m_typeZoneMap.end())
    res = m_state->m_typeZoneMap.find(int(type))->second;
  return res;
}

std::multimap<int, MsWksDocument::Zone> &MsWksDocument::getTypeZoneMap()
{
  return m_state->m_typeZoneMap;
}

std::multimap<std::string, MWAWEntry> &MsWksDocument::getEntryMap()
{
  return m_state->m_entryMap;
}

long MsWksDocument::getLengthOfFileHeader3() const
{
  return m_state->m_fileHeaderSize;
}

void MsWksDocument::sendText(int id)
{
  if (m_textParser3) m_textParser3->sendZone(id);
}

void MsWksDocument::sendZone(int zoneType)
{
  Zone zone=getZone(ZoneType(zoneType));
  if (zone.m_zoneId >= 0)
    m_graphParser->sendAll(zone.m_zoneId, zoneType==MsWksDocument::Z_MAIN);
  if (zone.m_textId >= 0) sendText(zone.m_textId);
}

void MsWksDocument::sendFootnoteContent(int noteId)
{
  if (!m_parserState->getMainListener()) return;
  if (!m_state->m_footnoteParser) {
    MWAW_DEBUG_MSG(("MsWksDocument::sendFootnoteContent: can not found the footnote parser\n"));
    m_parserState->getMainListener()->insertChar(' ');
    return;
  }
  m_state->m_footnoteParser->createZones(false);
  m_state->m_footnoteParser->readFootNote(noteId);
}

////////////////////////////////////////////////////////////
// page span function
////////////////////////////////////////////////////////////
bool MsWksDocument::hasHeader() const
{
  return m_state->m_hasHeader;
}

bool MsWksDocument::hasFooter() const
{
  return m_state->m_hasFooter;
}

float MsWksDocument::getHeaderFooterHeight(bool header) const
{
  return header ? float(m_state->m_headerHeight) : float(m_state->m_footerHeight);
}

void MsWksDocument::getPageSpanList(std::vector<MWAWPageSpan> &pagesList, int &numPages)
{
  Zone mainZone=getZone(Z_MAIN);
  // first count the page
  numPages = 1;
  if (m_textParser3 && mainZone.m_textId >= 0 && m_textParser3->numPages(mainZone.m_textId) > numPages)
    numPages = m_textParser3->numPages(mainZone.m_textId);
  if (m_textParser4 && m_textParser4->numPages() > numPages)
    numPages = m_textParser4->numPages();
  if (m_graphParser->numPages(mainZone.m_zoneId) > numPages)
    numPages = m_graphParser->numPages(mainZone.m_zoneId);
  // now update the page list
  MWAWPageSpan ps(m_parserState->m_pageSpan);
  int id = m_textParser3 ? m_textParser3->getHeader() : -1;
  if (id >= 0) {
    m_state->m_headerHeight=12;
    MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
    header.m_subDocument.reset
    (new MsWksDocumentInternal::SubDocument
     (*this, getInput(), MsWksDocumentInternal::SubDocument::Text, id));
    ps.setHeaderFooter(header);
  }
  else if (getZone(MsWksDocument::Z_HEADER).m_zoneId >= 0) {
    MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
    header.m_subDocument.reset
    (new MsWksDocumentInternal::SubDocument
     (*this, getInput(), MsWksDocumentInternal::SubDocument::Zone, int(MsWksDocument::Z_HEADER)));
    ps.setHeaderFooter(header);
  }
  else if (m_state->m_headerParser) {
    MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
    header.m_subDocument.reset
    (new MsWksDocumentInternal::SubDocument
     (*this, getInput(), MsWksDocumentInternal::SubDocument::OLE, int(MsWksDocument::Z_HEADER)));
    ps.setHeaderFooter(header);
  }
  id = m_textParser3 ? m_textParser3->getFooter() : -1;
  if (id >= 0) {
    m_state->m_footerHeight=12;
    MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    footer.m_subDocument.reset
    (new MsWksDocumentInternal::SubDocument
     (*this, getInput(), MsWksDocumentInternal::SubDocument::Text, id));
    ps.setHeaderFooter(footer);
  }
  else if (getZone(MsWksDocument::Z_FOOTER).m_zoneId >= 0) {
    MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    footer.m_subDocument.reset
    (new MsWksDocumentInternal::SubDocument
     (*this, getInput(), MsWksDocumentInternal::SubDocument::Zone, int(MsWksDocument::Z_FOOTER)));
    ps.setHeaderFooter(footer);
  }
  else if (m_state->m_footerParser) {
    MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    footer.m_subDocument.reset
    (new MsWksDocumentInternal::SubDocument
     (*this, getInput(), MsWksDocumentInternal::SubDocument::OLE, int(MsWksDocument::Z_FOOTER)));
    ps.setHeaderFooter(footer);
  }
  ps.setPageSpan(numPages+1);
  pagesList=std::vector<MWAWPageSpan>(1,ps);
}

////////////////////////////////////////////////////////////
// interface via callback
////////////////////////////////////////////////////////////
void MsWksDocument::newPage(int page, bool softBreak)
{
  if (m_parentDocument)
    return m_parentDocument->newPage(page, softBreak);
  if (!m_newPage) {
    MWAW_DEBUG_MSG(("MsWksDocument::newPage: can not find the newPage callback\n"));
    return;
  }
  (m_parser->*m_newPage)(page, softBreak);
}

void MsWksDocument::sendFootnote(int id)
{
  if (m_parentDocument)
    return m_parentDocument->sendFootnote(id);
  if (!m_parserState->getMainListener()) return;

  MWAWSubDocumentPtr subdoc(new MsWksDocumentInternal::SubDocument
                            (*this, getInput(), MsWksDocumentInternal::SubDocument::Note, id));
  m_parserState->getMainListener()->insertNote(MWAWNote(MWAWNote::FootNote), subdoc);
}

void MsWksDocument::sendOLE(int id, MWAWPosition const &pictPos, MWAWGraphicStyle const &style)
{
  if (m_parentDocument)
    return m_parentDocument->sendOLE(id, pictPos, style);

  if (!m_parserState->getMainListener()) return;
  librevenge::RVNGBinaryData data;
  MWAWPosition pos;
  std::string type;
  if (!m_state->m_oleParser->getObject(id, data, pos, type)) {
    MWAW_DEBUG_MSG(("MsWksDocument::sendOLE: can not find OLE%d\n", id));
    return;
  }
  m_parserState->getMainListener()->insertPicture(pictPos, data, type, style);
}

void MsWksDocument::sendRBIL(int id, Vec2i const &sz)
{
  MsWksGraph::SendData sendData;
  sendData.m_type = MsWksGraph::SendData::RBIL;
  sendData.m_id = id;
  sendData.m_anchor =  MWAWPosition::Char;
  sendData.m_size = sz;
  m_graphParser->sendObjects(sendData);
}

void MsWksDocument::sendTextbox(MWAWEntry const &entry, std::string const &frame)
{
  if (m_parentDocument)
    return m_parentDocument->sendTextbox(entry,frame);
  MWAWListenerPtr listener=m_parserState->getMainListener();
  if (!listener) return;

  if (entry.length()==0) {
    listener->insertChar(' ');
    return;
  }

  MsWks4Zone *parser = 0;
  if (frame == "QHdr") parser=m_state->m_headerParser.get();
  else if (frame == "QFtr") parser=m_state->m_footerParser.get();
  else {
    std::map<std::string, shared_ptr<MsWks4Zone> >::iterator frameIt =
      m_state->m_frameParserMap.find(frame);
    if (frameIt != m_state->m_frameParserMap.end())
      parser = frameIt->second.get();
  }
  if (!parser || (entry.length() && parser->getTextPosition().length() < entry.end())) {
    MWAW_DEBUG_MSG(("MsWksDocument::sendTextbox: can not find frame ole: %s\n", frame.c_str()));
    listener->insertChar(' ');
    return;
  }

  // ok, create the entry
  MWAWEntry ent(entry);
  ent.setBegin(entry.begin()+parser->getTextPosition().begin());
  parser->createZones(false);
  parser->readContentZones(ent, false);
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
std::vector<MWAWColor> const &MsWksDocument::getPalette(int vers)
{
  switch (vers) {
  case 2: {
    static std::vector<MWAWColor> palette;
    palette.resize(9);
    palette[0]=MWAWColor(0,0,0); // undef
    palette[1]=MWAWColor(0,0,0);
    palette[2]=MWAWColor(255,255,255);
    palette[3]=MWAWColor(255,0,0);
    palette[4]=MWAWColor(0,255,0);
    palette[5]=MWAWColor(0,0,255);
    palette[6]=MWAWColor(0, 255,255);
    palette[7]=MWAWColor(255,0,255);
    palette[8]=MWAWColor(255,255,0);
    return palette;
  }
  case 3: {
    static std::vector<MWAWColor> palette;
    if (palette.size()==0) {
      palette.resize(256);
      size_t ind=0;
      for (int k = 0; k < 6; k++) {
        for (int j = 0; j < 6; j++) {
          for (int i = 0; i < 6; i++, ind++) {
            if (j==5 && i==2) break;
            palette[ind]=MWAWColor((unsigned char)(255-51*i), (unsigned char)(255-51*k), (unsigned char)(255-51*j));
          }
        }
      }

      // the last 2 lines
      for (int r = 0; r < 2; r++) {
        // the black, red, green, blue zone of 5*2
        for (int c = 0; c < 4; c++) {
          for (int i = 0; i < 5; i++, ind++) {
            int val = 17*r+51*i;
            if (c == 0) {
              palette[ind]=MWAWColor((unsigned char)val, (unsigned char)val, (unsigned char)val);
              continue;
            }
            int color[3]= {0,0,0};
            color[c-1]=val;
            palette[ind]=MWAWColor((unsigned char)(color[0]),(unsigned char)(color[1]),(unsigned char)(color[2]));
          }
        }
        // last part of j==5, i=2..5
        for (int k = r; k < 6; k+=2) {
          for (int i = 2; i < 6; i++, ind++)
            palette[ind]=MWAWColor((unsigned char)(255-51*i), (unsigned char)(255-51*k), (unsigned char)(255-51*5));
        }
      }
    }
    return palette;
  }
  case 4: {
    static std::vector<MWAWColor> palette;
    if (palette.size()==0) {
      palette.resize(256);
      size_t ind=0;
      for (int k = 0; k < 6; k++) {
        for (int j = 0; j < 6; j++) {
          for (int i = 0; i < 6; i++, ind++) {
            palette[ind]=
              MWAWColor((unsigned char)(255-51*k), (unsigned char)(255-51*j),
                        (unsigned char)(255-51*i));
          }
        }
      }
      ind--; // remove the black color
      for (int c = 0; c < 4; c++) {
        unsigned char color[3] = {0,0,0};
        unsigned char val=(unsigned char) 251;
        for (int i = 0; i < 10; i++) {
          val = (unsigned char)(val-17);
          if (c == 3) palette[ind++]=MWAWColor(val, val, val);
          else {
            color[c] = val;
            palette[ind++]=MWAWColor(color[0],color[1],color[2]);
          }
          if ((i%2)==1) val = (unsigned char)(val-17);
        }
      }

      // last is black
      palette[ind++]=MWAWColor(0,0,0);
    }
    return palette;
  }
  default:
    break;
  }
  MWAW_DEBUG_MSG(("MsWksDocument::getPalette: can not find palette for version %d\n", vers));
  static std::vector<MWAWColor> emptyPalette;
  return emptyPalette;
}

bool MsWksDocument::getColor(int id, MWAWColor &col, int vers)
{
  std::vector<MWAWColor> const &palette = getPalette(vers);
  if (palette.size()==0 || id < 0 || id >= int(palette.size()) ||
      (vers==2 && id==0)) {
    static bool first = true;
    if (first) {
      MWAW_DEBUG_MSG(("MsWksDocument::getColor: unknown color=%d\n", id));
      first = false;
    }
    return false;
  }
  col = palette[size_t(id)];
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// create the ole structures or get list of unparsed zones
////////////////////////////////////////////////////////////
std::vector<std::string> const &MsWksDocument::getUnparsedOLEZones() const
{
  return m_state->m_unparsedOlesName;
}

bool MsWksDocument::createOLEZones(MWAWInputStreamPtr input)
{
  if (!input || !input->isStructured()) return false;
  m_state->m_oleParser.reset(new MWAWOLEParser("MN0"));

  if (!m_state->m_oleParser->parse(input)) return false;

  // normally,
  // MacWorks/QHdr, MacWorks/QFtr, MacWorks/QFootnotes, MacWorks/QFrm<number>
  // MN0 (the main header)
  std::vector<std::string> unparsed = m_state->m_oleParser->getNotParse();

  size_t numUnparsed = unparsed.size();
  unparsed.push_back("MN0");

  for (size_t i = 0; i <= numUnparsed; i++) {
    std::string const &name = unparsed[i];

    // separated the directory and the name
    //    MatOST/MatadorObject1/Ole10Native
    //      -> dir="MatOST/MatadorObject1", base="Ole10Native"
    std::string::size_type pos = name.find_last_of('/');
    std::string dir, base;
    if (pos == std::string::npos) base = name;
    else if (pos == 0) base = name.substr(1);
    else {
      dir = name.substr(0,pos);
      base = name.substr(pos+1);
    }

    if (dir == "" && base == "MN0") continue;
    bool ok = false;
    bool isFrame = false;
    if (!ok && dir == "MacWorks") {
      ok = (base == "QHdr" || base == "QFtr" || base == "QFootnotes");
      if (!ok && strncmp(base.c_str(),"QFrm",4)==0)
        ok = isFrame = true;
    }
    if (!ok) {
      m_state->m_unparsedOlesName.push_back(name);
      continue;
    }

    MWAWInputStreamPtr ole = input->getSubStreamByName(name.c_str());
    if (!ole.get()) {
      MWAW_DEBUG_MSG(("MsWksDocument::createOLEZones: error: can not find OLE part: \"%s\"\n", name.c_str()));
      continue;
    }

    shared_ptr<MsWks4Zone> newParser(new MsWks4Zone(ole, m_parserState, *m_parser, name));
    try {
      ok = newParser->createZones(false);
    }
    catch (...) {
      ok = false;
    }

    if (!ok) {
      MWAW_DEBUG_MSG(("MsWksDocument::createOLEZones: error: can not parse OLE: \"%s\"\n", name.c_str()));
      continue;
    }

    // first update the parent document
    newParser->m_document->m_parentDocument=this;
    if (base == "QHdr") m_state->m_headerParser = newParser;
    else if (base == "QFtr") m_state->m_footerParser = newParser;
    else if (isFrame) {
      std::map<std::string, shared_ptr<MsWks4Zone> >::iterator frameIt =
        m_state->m_frameParserMap.find(base);
      if (frameIt != m_state->m_frameParserMap.end()) {
        MWAW_DEBUG_MSG(("MsWksDocument::createOLEZones: error: oops, I already find a frame zone %s\n", base.c_str()));
      }
      else
        m_state->m_frameParserMap[base] = newParser;
    }
    else if (base == "QFootnotes") m_state->m_footnoteParser = newParser;
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool MsWksDocument::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;
  // print info
  libmwaw::PrinterInfo info;
  if (!input->checkPosition(pos+0x78+8) || !info.read(input)) return false;
  f << "Entries(PrintInfo):"<< info;

  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  // now read the margin
  int margin[4];
  int maxSize = paperSize.x() > paperSize.y() ? paperSize.x() : paperSize.y();
  f << ", margin=(";
  for (int i = 0; i < 4; i++) {
    margin[i] = int(72.f/120.f*(float)input->readLong(2));
    if (margin[i] < -maxSize || margin[i] > maxSize) return false;
    f << margin[i];
    if (i != 3) f << ", ";
  }
  f << ")";

  // fixme: compute the real page length here...
  // define margin from print info
  Vec2i lTopMargin(margin[0],margin[1]), rBotMargin(margin[2],margin[3]);
  lTopMargin += paperSize - pageSize;

  int leftMargin = lTopMargin.x();
  int topMargin = lTopMargin.y();

  // decrease a little right and bottom margins Margin
  int rightMarg = rBotMargin.x()-50;
  if (rightMarg < 0) {
    leftMargin -= (-rightMarg);
    if (leftMargin < 0) leftMargin=0;
    rightMarg=0;
  }
  int botMarg = rBotMargin.y()-50;
  if (botMarg < 0) {
    topMargin -= (-botMarg);
    if (topMargin < 0) topMargin=0;
    botMarg=0;
  }

  m_parserState->m_pageSpan.setMarginTop(topMargin/72.0);
  m_parserState->m_pageSpan.setMarginBottom(botMarg/72.0);
  m_parserState->m_pageSpan.setMarginLeft(leftMargin/72.0);
  m_parserState->m_pageSpan.setMarginRight(rightMarg/72.0);
  m_parserState->m_pageSpan.setFormLength(paperSize.y()/72.);
  m_parserState->m_pageSpan.setFormWidth(paperSize.x()/72.);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+0x78+8, librevenge::RVNG_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
// read the document info
////////////////////////////////////////////////////////////
bool MsWksDocument::readDocumentInfo(long sz)
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;

  int vers = m_parserState->m_kind==MWAWDocument::MWAW_K_DATABASE ? 2 : version();
  int docId = 0;
  int docExtra = 0;
  int flag = 0;
  int expectedSz = 0x80;
  if (sz<=0) {
    if (input->readLong(1) != 2)
      return false;
    docId = (int) input->readULong(1);
    docExtra = (int) input->readULong(1);
    flag = (int) input->readULong(1);
    sz = (long) input->readULong(2);
    expectedSz = vers<=2 ? 0x15e : 0x9a;
  }
  long endPos = input->tell()+sz;
  if (!input->checkPosition(endPos))
    return false;

  if (sz < expectedSz) {
    if (sz < 0x78+8) {
      MWAW_DEBUG_MSG(("MsWksDocument::readDocumentInfo: size is too short\n"));
      return false;
    }
    MWAW_DEBUG_MSG(("MsWksDocument::readDocumentInfo: size is too short: try to continue\n"));
  }

  f << "Entries(DocInfo):";
  if (docId) f << "id=0x"<< std::hex << docId << ",";
  if (docExtra) f << "unk=" << docExtra << ","; // in v3: find 3, 7, 1x
  if (flag) f << "fl=" << flag << ","; // in v3: find 80, 84, e0
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  if (!readPrintInfo()) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return true;
  }

  if (sz < 0x9a) {
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return true;
  }
  pos = input->tell();
  f.str("");
  f << "DocInfo-1:";
  int val = (int) input->readLong(2);
  if ((val & 0x0400) && vers >= 3) {
    f << "titlepage,";
    val &= 0xFBFF;
  }
  if (val) f << "unkn=" << val << ",";
  if (vers <= 2) {
    for (int wh = 0; wh < 2; wh++) {
      long debPos = input->tell();
      std::string name(wh==0 ? "header" : "footer");
      std::string text = getTextParser3()->readHeaderFooterString(wh==0);
      if (text.size()) f << name << "="<< text << ",";

      long remain = debPos+100 - input->tell();
      for (long i = 0; i < remain; i++) {
        unsigned char c = (unsigned char) input->readULong(1);
        if (c == 0) continue;
        f << std::dec << "f"<< i << "=" << (int) c << ",";
      }
    }
    f << "defFid=" << input->readULong(2) << ",";
    f << "defFsz=" << input->readULong(2)/2 << ",";
    val = (int) input->readULong(2); // 0 or 8
    if (val) f << "#unkn=" << val << ",";
    int dim[2];
    for (int i = 0; i < 2; i++) dim[i] = (int) input->readULong(2);
    f << "dim=" << dim[0] << "x" << dim[1] << ",";
    /* followed by 0 (v1) or 0|0x21|0* (v2)*/
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    pos = input->tell();
    f.str("");
    f << "DocInfo-2:";
  }

  // last data ( normally 26)
  int numData = int((endPos - input->tell())/2);
  for (int i = 0; i < numData; i++) {
    val = (int) input->readLong(2);
    switch (i) {
    case 2:
      if (val!=1) f << "firstPageNumber=" << val << ",";
      break;
    case 3:
      if (val!=1) f << "firstNoteNumber=" << val << ",";
      break;
    default:
      if (val)
        f << "g" << i << "=" << val << ",";
      break;
    }
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(endPos, librevenge::RVNG_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
// read a header/footer zone info
////////////////////////////////////////////////////////////
bool MsWksDocument::readGroupHeaderFooter(bool header, int check)
{
  if (version() < 3) return false;

  MWAWInputStreamPtr input=getInput();
  long debPos = input->tell();

  long ptr = (long) input->readULong(2);
  if (input->isEnd()) return false;
  if (ptr) {
    if (check == 49) return false;
    if (check == 99) {
      MWAW_DEBUG_MSG(("MsWksDocument::readGroupHeaderFooter: find ptr=0x%lx\n", (long unsigned int)ptr));
    }
  }

  libmwaw::DebugStream f;

  int size = (int) input->readLong(2)+4;
  int realSize = 0x11;
  if (size < realSize) return false;
  if (input->readLong(2) != 0) return false;
  f << "Entries(GroupHInfo)";
  if (header)
    f << "[header]";
  else
    f << "[footer]";
  f << ": size=" << std::hex << size << std::dec << " BTXT";

  if (!input->checkPosition(debPos+size)) return false;

  input->seek(debPos+6, librevenge::RVNG_SEEK_SET);
  int N=(int) input->readLong(2);
  f << ", N=" << N;
  int dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = (int) input->readLong(2);

  MWAWBox2i box(Vec2i(dim[1], dim[0]), Vec2i(dim[3], dim[2]));
  if (box.size().x() < -2000 || box.size().y() < -2000 ||
      box.size().x() > 2000 || box.size().y() > 2000 ||
      box.min().x() < -200 || box.min().y() < -200) return false;
  if (check == 49 && box.size().x() == 0 &&  box.size().y() == 0) return false;
  f << ", BDBox =" << box;
  int val = (int) input->readULong(1);
  if (val) f << ", flag=" << val;

  input->seek(debPos+size, librevenge::RVNG_SEEK_SET);
  if (check < 99) return true;
  if (header) m_state->m_headerHeight = box.size().y();
  else m_state->m_footerHeight = box.size().y();
  std::multimap<int, MsWksDocument::Zone> &typeZoneMap=getTypeZoneMap();
  MsWksDocument::ZoneType type=
    header ? MsWksDocument::Z_HEADER : MsWksDocument::Z_FOOTER;
  MsWksDocument::Zone zone(type, int(typeZoneMap.size()));

  ascii().addPos(debPos);
  ascii().addNote(f.str().c_str());
  ascii().addPos(input->tell());

  input->seek(debPos+realSize, librevenge::RVNG_SEEK_SET);
  input->pushLimit(debPos+size);
  bool limitSet = true;
  for (int i = 0; i < N; i++) {
    long pos = input->tell();
    if (limitSet && pos==debPos+size) {
      limitSet = false;
      input->popLimit();
    }
    if (readZone(zone)) continue;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    zone.m_textId = getTextParser3()->createZones(N-i, false);
    if (zone.m_textId >= 0)
      break;
    MWAW_DEBUG_MSG(("MsWksDocument::readGroupHeaderFooter: can not find end of group\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  }
  if (limitSet) input->popLimit();
  if (long(input->tell()) < debPos+size) {
    ascii().addPos(input->tell());
    ascii().addNote("GroupHInfo-II");

    input->seek(debPos+size, librevenge::RVNG_SEEK_SET);

    ascii().addPos(debPos + size);
    ascii().addNote("_");
  }
  //  getGraphParser()->addDeltaToPositions(zone.m_zoneId, -1*box[0]);
  if (typeZoneMap.find(int(type)) != typeZoneMap.end()) {
    MWAW_DEBUG_MSG(("MsWksDocument::readGroupHeaderFooter: the zone already exists\n"));
  }
  else
    typeZoneMap.insert(std::multimap<int,MsWksDocument::Zone>::value_type(int(type),zone));
  return true;
}

////////////////////////////////////////////////////////////
// read a generic zone
////////////////////////////////////////////////////////////
bool MsWksDocument::readZone(MsWksDocument::Zone &zone)
{
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd()) return false;
  long pos = input->tell();
  MWAWEntry pict;
  int val = (int) input->readLong(1);
  input->seek(-1, librevenge::RVNG_SEEK_CUR);
  switch (val) {
  case 0: {
    if (m_graphParser->getEntryPicture(zone.m_zoneId, pict)>=0) {
      input->seek(pict.end(), librevenge::RVNG_SEEK_SET);
      return true;
    }
    break;
  }
  case 1: {
    if (m_graphParser->getEntryPictureV1(zone.m_zoneId, pict)>=0) {
      input->seek(pict.end(), librevenge::RVNG_SEEK_SET);
      return true;
    }
    break;
  }
  case 2:
    if (readDocumentInfo())
      return true;
    break;
  case 3: {
    // checkme, ok for text but is it also ok for other ?
    MWAWEntry group;
    group.setId(zone.m_zoneId);
    group.setName("RBDR");
    if (m_graphParser->readRB(input, group, 2))
      return true;
    break;
  }
  default:
    break;
  }

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  return false;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MsWksDocument::checkHeader3(MWAWHeader *header, bool strict)
{
  *m_state = MsWksDocumentInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;

  int numError = 0, val;

  const int headerSize = 0x20;

  libmwaw::DebugStream f;

  input->seek(0,librevenge::RVNG_SEEK_SET);

  m_state->m_hasHeader = m_state->m_hasFooter = false;
  int vers = (int) input->readULong(4);
  switch (vers) {
  case 11:
    setVersion(4);
    break;
  case 9:
    setVersion(3);
    break;
  case 8:
    setVersion(2);
    break;
  case 4:
    setVersion(1);
    break;
  default:
    if (strict) return false;

    MWAW_DEBUG_MSG(("MsWksDocument::checkHeader3: find unknown version 0x%x\n", (unsigned int) vers));
    // must we stop in this case, or can we continue ?
    if (vers < 0 || vers > 14) {
      MWAW_DEBUG_MSG(("MsWksDocument::checkHeader3: version too big, we stop\n"));
      return false;
    }
    setVersion((vers < 4) ? 1 : (vers < 8) ? 2 : (vers < 11) ? 3 : 4);
  }
  if (input->seek(headerSize,librevenge::RVNG_SEEK_SET) != 0 || input->isEnd())
    return false;

  if (input->seek(12,librevenge::RVNG_SEEK_SET) != 0) return false;

  for (int i = 0; i < 3; i++) {
    val = (int)(int) input->readLong(1);
    if (val < -10 || val > 10) {
      MWAW_DEBUG_MSG(("MsWksDocument::checkHeader3: find odd val%d=0x%x: not implemented\n", i, (unsigned int)val));
      numError++;
    }
  }
  input->seek(1,librevenge::RVNG_SEEK_CUR);
  int type = (int) input->readLong(2);
  switch (type) {
  // Text document
  case 1:
    break;
  case 2:
    m_state->m_kind = MWAWDocument::MWAW_K_DATABASE;
    break;
  case 3:
    m_state->m_kind = MWAWDocument::MWAW_K_SPREADSHEET;
    break;
  case 12:
    m_state->m_kind = MWAWDocument::MWAW_K_DRAW;
    break;
  default:
    MWAW_DEBUG_MSG(("MsWksDocument::checkHeader3: find odd type=%d: not implemented\n", type));
    return false;
  }

  if (version() < 1 || version() > 4)
    return false;

  //
  input->seek(0,librevenge::RVNG_SEEK_SET);
  f << "FileHeader: ";
  f << "version= " << input->readULong(4);
  long dim[4];
  for (int i = 0; i < 4; i++) dim[i] = input->readLong(2);
  bool checkDatabaseFileHSize=false;
  if (dim[2] <= dim[0] || dim[3] <= dim[1]) {
    // bdbox(0,0,0,0) can appear if there is no document info, so do not consider this an error for database file
    if (dim[0]==0 && dim[1]==0 && dim[2]==0 && dim[3]==0 && m_state->m_kind == MWAWDocument::MWAW_K_DATABASE) {
      checkDatabaseFileHSize=true;
    }
    else {
      MWAW_DEBUG_MSG(("MsWksDocument::checkHeader3: find odd bdbox\n"));
      numError++;
    }
  }
  f << ", windowdbdbox?=(";
  for (int i = 0; i < 4; i++) f << dim[i]<<",";
  f << "),";
  long fileHeaderSize=(long) input->readULong(4);
  if (checkDatabaseFileHSize && strict) {
    if ((version()==2 && fileHeaderSize!=0x516) || (version()==3&&fileHeaderSize!=0x50))
      return false;
  }
  if (fileHeaderSize)
    f << "headerSize=" << std::hex << fileHeaderSize << std::dec << ",";

  if (m_state->m_kind==MWAWDocument::MWAW_K_SPREADSHEET) {
    /* CHECKME: normally 0x56c, but find one time 0x56a in a v2 file
       which seems to imply that the spreadsheet begins earlier */
    if (fileHeaderSize!=0x56c && fileHeaderSize > (long) 0x550 && input->checkPosition(fileHeaderSize+0x20))
      m_state->m_fileHeaderSize=fileHeaderSize;
    else
      m_state->m_fileHeaderSize=0x56c;
  }
  type = (int) input->readULong(2);
  f << std::dec;
  switch (type) {
  case 1:
    f << "doc,";
    break;
  case 2:
    f << "database,";
    break; // with ##v3=50
  case 3:
    f << "spreadsheet,";
    break; // with ##v2=5,##v3=6c
  case 12:
    f << "draw,";
    break;
  default:
    f << "###type=" << type << ",";
    break;
  }
  f << "numlines?=" << input->readLong(2) << ",";
  val = (int) input->readLong(1); // 0, v2: 0, 4 or -4
  if (val)  f << "f0=" << val << ",";
  val = (int) input->readLong(1); // almost always 1
  if (val != 1) f << "f1=" << val << ",";
  for (int i = 11; i < headerSize/2; i++) { // v1: 0, 0, v2: 0, 0|1
    val = (int) input->readULong(2);
    if (!val) continue;
    f << "f" << i << "=" << std::hex << val << std::dec;
    if (version() >= 3 && i == 12) {
      if (val & 0x100) {
        m_state->m_hasHeader = true;
        f << "(Head)";
      }
      if (val & 0x200) {
        m_state->m_hasFooter = true;
        f << "(Foot)";
      }
    }
    f << ",";
  }

  if (header)
    header->reset(MWAWDocument::MWAW_T_MICROSOFTWORKS, version(), m_state->m_kind);

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());
  ascii().addPos(headerSize);

  input->seek(headerSize,librevenge::RVNG_SEEK_SET);
  return strict ? (numError==0) : (numError < 3);
}

////////////////////////////////////////////////////////////
// spreadsheet/database function
////////////////////////////////////////////////////////////
bool MsWksDocument::readDBString(long endPos, std::string &res)
{
  MWAWInputStreamPtr input=getInput();
  res = "";
  int error = 0;
  int ok = 0;
  while (input->tell() != endPos) {
    char c = (char) input->readLong(1);
    if (c < 27 && c != '\t' && c != '\n') error++;
    else ok++;
    res += c;
  }
  return ok >= error;
}

bool MsWksDocument::readDBNumber(long endPos, double &res, bool &isNan, std::string &str)
{
  MWAWInputStreamPtr input=getInput();
  res = 0;
  str="";
  long pos = input->tell();
  if (endPos > pos+10 && !readDBString(endPos-10,str)) return false;
  return input->tell() == endPos-10 && input->readDouble10(res,isNan);
}

bool MsWksDocument::readCellInFormula(MWAWCellContent::FormulaInstruction &instr, bool is2D)
{
  MWAWInputStreamPtr input=getInput();
  instr=MWAWCellContent::FormulaInstruction();
  instr.m_type=MWAWCellContent::FormulaInstruction::F_Cell;
  bool ok = true;
  if (is2D) {
    bool absolute[2] = { false, false};
    int type = (int) input->readULong(1);
    if (type & 0x80) {
      absolute[0] = true;
      type &= 0x7F;
    }
    if (type & 0x40) {
      absolute[1] = true;
      type &= 0xBF;
    }
    if (type) {
      MWAW_DEBUG_MSG(("MSWksSSParser::readCellInFormula:Pb find fl=%d when reading a cell\n", type));
      ok = false;
    }
    int pos[2]= {1,0};
    for (int i=0; i< 2; ++i)
      pos[i] = (int) input->readULong(1);

    if (pos[0] < 1 || pos[1] < 0) {
      if (ok) {
        MWAW_DEBUG_MSG(("MSWksSSParser::readCellInFormula: can not read cell position\n"));
      }
      return false;
    }
    instr.m_position[0]=Vec2i(pos[1],pos[0]-1);
    instr.m_positionRelative[0]=Vec2b(!absolute[1],!absolute[0]);
  }
  else
    instr.m_position[0]=Vec2i((int) input->readULong(1),0);
  return ok;
}

bool MsWksDocument::readFormula(long endPos, MWAWCellContent &content, std::string &extra)
{
  MWAWInputStreamPtr input=getInput();
  long pos = input->tell();
  extra="";
  if (pos == endPos) return false;

  std::stringstream f;
  std::vector<MWAWCellContent::FormulaInstruction> &formula=content.m_formula;
  bool is2D=m_parserState->m_kind!=MWAWDocument::MWAW_K_DATABASE;
  bool ok=true;
  while (input->tell() !=endPos) {
    pos = input->tell();
    int code = (int) input->readLong(1);
    MWAWCellContent::FormulaInstruction instr;
    bool findEnd=false;
    switch (code) {
    case 0x0:
    case 0x2:
    case 0x4:
    case 0x6: {
      static char const *wh[]= {"+","-","*","/"};
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
      instr.m_content=wh[code/2];
      break;
    }
    case 0x8: { // a number
      int sz = (int) input->readULong(1);
      std::string s;
      double val;
      bool isNan;
      if (pos+sz+12 <= endPos && readDBNumber((pos+2)+sz+10, val, isNan, s)) {
        instr.m_type=MWAWCellContent::FormulaInstruction::F_Double;
        instr.m_doubleValue=val;
        break;
      }
      f << "###number" << s;
      ok=false;
      break;
    }
    case 0x0a: { // a cell
      if (!readCellInFormula(instr, is2D))
        f << "#";
      break;
    }
    case 0x0c: { // function
      int v = (int) input->readULong(1);
      static char const *(listFunc) [0x41] = {
        "Abs", "Sum", "Na", "Error", "ACos", "And", "ASin", "ATan",
        "ATan2", "Average", "Choose", "Cos", "Count", "Exp", "False", "FV",
        "HLookup", "If", "Index", "Int", "IRR", "IsBlank", "IsError", "IsNa",
        "##Funct[30]", "Ln", "Lookup", "Log10", "Max", "Min", "Mod", "Not",
        "NPer", "NPV", "Or", "Pi", "Pmt", "PV", "Rand", "Round",
        "Sign", "Sin", "Sqrt", "StDev", "Tan", "True", "Var", "VLookup",
        "Match", "MIRR", "Rate", "Type", "Radians", "Degrees", "Sum" /*"SSum: checkme"*/, "Date",
        "Day", "Hour", "Minute", "Month", "Now", "Second", "Time", "Weekday",
        "Year"
      };
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Function;
      if ((v%2) == 0 && v >= 0 && v/2 <= 0x40)
        instr.m_content=listFunc[v/2];
      else {
        f << "###";
        MWAW_DEBUG_MSG(("MSWksSSParser::readFormula: find unknown function %x\n", (unsigned int)v));
        std::stringstream s;
        s << "Funct" << std::hex << v << std::dec;
        instr.m_content=s.str();
      }
      break;
    }
    case 0x0e: { // list of cell
      MWAWCellContent::FormulaInstruction instr2;
      if (endPos-pos< (is2D ? 9 : 5)) {
        f << "###list cell short";
        ok = false;
        break;
      }
      if (!readCellInFormula(instr, is2D) || !readCellInFormula(instr2, is2D))
        f << "#";
      instr.m_type=MWAWCellContent::FormulaInstruction::F_CellList;
      instr.m_position[1]=instr2.m_position[0];
      instr.m_positionRelative[1]=instr2.m_positionRelative[0];
      break;
    }
    case 0x16:
      findEnd=true;
      input->seek(-1, librevenge::RVNG_SEEK_CUR);
      f << ",";
      break;
    default:
      if ((code%2)==0 && code>=0x10 && code<=0x22) {
        static char const *wh[]= {"(", ")", ";", "end", "<", ">", "=", "<=", ">=", "<>" };
        instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
        instr.m_content=wh[(code-0x10)/2];
        break;
      }
      f << "###" << std::hex << code << std::dec;
      ok = false;
      break;
    }
    if (!ok || findEnd)
      break;
    f << instr;
    formula.push_back(instr);
  }
  if (ok)
    content.m_formula=formula;
  extra=f.str();
  pos = input->tell();
  if (endPos - pos < 21)
    return ok;
  // test if we have the value
  if (input->readLong(1) != 0x16) {
    input->seek(-1, librevenge::RVNG_SEEK_CUR);
    return true;
  }

  f.str("");
  f << std::dec << "unk1=[";
  // looks a little as a zone of cell ?? but this seems eroneous
  for (int i = 0; i < 2; i++) {
    int v = (int) input->readULong(1);

    int n0 = (int) input->readULong(1);
    int n1 = (int) input->readULong(1);
    if (i == 1) f << ":";
    f << n0 << "x" << n1;
    if (v) f << "##v";
  }
  f << std::hex << "],unk2=["; // 0, followed by a small number between 1 and 140
  for (int i = 0; i < 2; i++)
    f << input->readULong(2) << ",";
  f << "]";

  // the value
  double value;
  bool isNan;
  std::string res;
  if (!readDBNumber(endPos, value, isNan, res)) {
    MWAW_DEBUG_MSG(("MsWksDocument::readFormula: can not read val number\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f << ",###";
  }
  else {
    content.setValue(value);
    f << ":" << value << ",";
  }
  extra += f.str();
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
