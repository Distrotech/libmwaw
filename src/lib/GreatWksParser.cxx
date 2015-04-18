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
#include <set>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWTextListener.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSection.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "GreatWksDocument.hxx"
#include "GreatWksGraph.hxx"
#include "GreatWksText.hxx"

#include "GreatWksParser.hxx"

/** Internal: the structures of a GreatWksParser */
namespace GreatWksParserInternal
{

////////////////////////////////////////
//! Internal: the state of a GreatWksParser
struct State {
  //! constructor
  State() : m_columnsWidth(), m_hasColSep(false), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
    for (int i=0; i<4; ++i)
      m_hfFlags[i]=false;
  }
  //! returns the number of expected header/footer zones
  int numHeaderFooters() const
  {
    int num=0;
    if (m_hfFlags[2]) num++; // header
    if (m_hfFlags[3]) num++; // footer
    if (m_hfFlags[1]) num*=2; // lf page
    return num;
  }

  //! returns a section
  MWAWSection getSection() const
  {
    MWAWSection sec;
    size_t numCols = m_columnsWidth.size()/2;
    if (numCols <= 1)
      return sec;
    sec.m_columns.resize(size_t(numCols));
    if (m_hasColSep)
      sec.m_columnSeparator=MWAWBorder();
    for (size_t c=0; c < numCols; c++) {
      double wSep=0;
      if (c)
        wSep += sec.m_columns[c].m_margins[libmwaw::Left]=
                  double(m_columnsWidth[2*c]-m_columnsWidth[2*c-1])/72./2.;
      if (c+1!=numCols)
        wSep+=sec.m_columns[c].m_margins[libmwaw::Right]=
                double(m_columnsWidth[2*c+2]-m_columnsWidth[2*c+1])/72./2.;
      sec.m_columns[c].m_width =
        double(m_columnsWidth[2*c+1]-m_columnsWidth[2*c])+72.*wSep;
      sec.m_columns[c].m_widthUnit = librevenge::RVNG_POINT;
    }
    return sec;
  }

  //! the columns dimension
  std::vector<double> m_columnsWidth;
  //! flags to define header/footer (titlePage, l/rPage, header, footer)
  bool m_hfFlags[4];
  //! true if columns have columns separator
  bool m_hasColSep;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a GreatWksParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(GreatWksParser &pars, MWAWInputStreamPtr input, int zoneId) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

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
  //! the subdocument id
  int m_id;
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("GreatWksParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  if (type!=libmwaw::DOC_HEADER_FOOTER) {
    MWAW_DEBUG_MSG(("GreatWksParserInternal::SubDocument::parse: unknown type\n"));
    return;
  }
  GreatWksParser *parser=dynamic_cast<GreatWksParser *>(m_parser);
  if (!parser) {
    MWAW_DEBUG_MSG(("GreatWksParserInternal::SubDocument::parse: no parser\n"));
    return;
  }

  long pos = m_input->tell();
  parser->sendHF(m_id);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
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
GreatWksParser::GreatWksParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWTextParser(input, rsrcParser, header), m_state(), m_document()
{
  init();
}

GreatWksParser::~GreatWksParser()
{
}

void GreatWksParser::init()
{
  resetTextListener();
  setAsciiName("main-1");

  m_state.reset(new GreatWksParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);

  m_document.reset(new GreatWksDocument(*this));
  m_document->m_newPage=static_cast<GreatWksDocument::NewPage>(&GreatWksParser::newPage);
  m_document->m_getMainSection=static_cast<GreatWksDocument::GetMainSection>(&GreatWksParser::getMainSection);
}

////////////////////////////////////////////////////////////
// interface with the text parser
////////////////////////////////////////////////////////////
MWAWSection GreatWksParser::getMainSection() const
{
  return m_state->getSection();
}

bool GreatWksParser::sendHF(int id)
{
  return m_document->getTextParser()->sendHF(id);
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void GreatWksParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!getTextListener() || m_state->m_actPage == 1)
      continue;
    getTextListener()->insertBreak(MWAWTextListener::PageBreak);
  }
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void GreatWksParser::parse(librevenge::RVNGTextInterface *docInterface)
{
  if (!getInput().get() || !checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = false;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      m_document->getGraphParser()->sendPageGraphics();
      m_document->getTextParser()->sendMainText();
#ifdef DEBUG
      m_document->getTextParser()->flushExtra();
#endif
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("GreatWksParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetTextListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void GreatWksParser::createDocument(librevenge::RVNGTextInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getTextListener()) {
    MWAW_DEBUG_MSG(("GreatWksParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  int numPages = 1;
  if (m_document->getGraphParser()->numPages() > numPages)
    numPages = m_document->getGraphParser()->numPages();
  if (m_document->getTextParser()->numPages() > numPages)
    numPages = m_document->getTextParser()->numPages();
  m_state->m_numPages = numPages;

  MWAWPageSpan ps(getPageSpan());
  int numHF=m_state->numHeaderFooters();
  if (numHF!=m_document->getTextParser()->numHFZones()) {
    MWAW_DEBUG_MSG(("GreatWksParser::createDocument: header/footer will be ignored\n"));
    numHF=0;
  }
  std::vector<MWAWPageSpan> pageList;
  if (numHF && m_state->m_hfFlags[0]) // title page have no header/footer
    pageList.push_back(ps);
  else
    numPages++;
  if (numHF) {
    int id=0;
    for (int w=0; w<2; ++w) {
      if (!m_state->m_hfFlags[w+2])
        continue;
      MWAWHeaderFooter::Type type=
        w==0 ? MWAWHeaderFooter::HEADER : MWAWHeaderFooter::FOOTER;
      MWAWHeaderFooter hF;
      if (m_state->m_hfFlags[1]==false) {
        hF=MWAWHeaderFooter(type, MWAWHeaderFooter::ALL);
        hF.m_subDocument.reset(new GreatWksParserInternal::SubDocument(*this, getInput(), id++));
        ps.setHeaderFooter(hF);
        continue;
      }
      hF=MWAWHeaderFooter(type, MWAWHeaderFooter::ODD);
      hF.m_subDocument.reset(new GreatWksParserInternal::SubDocument(*this, getInput(), id++));
      ps.setHeaderFooter(hF);
      hF=MWAWHeaderFooter(type, MWAWHeaderFooter::EVEN);
      hF.m_subDocument.reset(new GreatWksParserInternal::SubDocument(*this, getInput(), id++));
      ps.setHeaderFooter(hF);
    }
  }
  ps.setPageSpan(numPages);
  pageList.push_back(ps);
  MWAWTextListenerPtr listen(new MWAWTextListener(*getParserState(), pageList, documentInterface));
  setTextListener(listen);
  listen->startDocument();
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool GreatWksParser::createZones()
{
  m_document->readRSRCZones();
  MWAWInputStreamPtr input = getInput();
  long pos=36;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (!readDocInfo()) {
    ascii().addPos(pos);
    ascii().addNote("Entries(DocInfo):###");
    return false;
  }

  bool ok=m_document->getTextParser()->createZones(m_state->numHeaderFooters());
  if (input->isEnd()) // v1 file end here
    return ok;

  pos = input->tell();
  if (!m_document->getGraphParser()->readGraphicZone())
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (!input->isEnd()) {
    pos = input->tell();
    MWAW_DEBUG_MSG(("GreatWksParser::createZones: find some extra data\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Loose):");
    ascii().addPos(pos+200);
    ascii().addNote("_");
  }

  return ok;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read some unknown zone in data fork
////////////////////////////////////////////////////////////
bool GreatWksParser::readDocInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  int const vers=version();
  if (!input->checkPosition(pos+46+(vers==2?6:0)+12+38*16)) {
    MWAW_DEBUG_MSG(("GreatWksParser::readDocInfo: the zone is too short\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(DocInfo):";
  int val;
  for (int i=0; i < 4; ++i) {
    static char const *(wh[])= {"fl0", "fl1", "smartquote","hidepict"};
    val =(int) input->readLong(1);
    if (!val) continue;
    if (val==1) f << wh[i] << ",";
    else f << "#" << wh[i] << "=" << val << ",";
  }
  val =(int) input->readLong(2);
  if (val!=1) f << "first[page]=" << val << ",";
  for (int i=0; i < 19; ++i) { // always 0
    val =(int) input->readLong(2);
    if (val)
      f << "f" << i+1 << "=" << val << ",";
  }
  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos+=46+(vers==2?6:0);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f.str("");
  f << "DocInfo-II:";
  for (int i=0; i < 4; ++i) {
    val=(int) input->readLong(1);
    if (!val) continue;
    static char const *(wh[])= {"titlePage", "left/rightPage", "header","footer"};
    if (val!=1) {
      f << "#" << wh[i] << "=" << val << ",";
      continue;
    }
    f << wh[i] << ",";
    m_state->m_hfFlags[i]=true;
  }

  val=(int) input->readLong(2); // f1=1|2
  if (val)
    f << "f0=" << val << ",";
  f << "colSep[w]=" << float(input->readLong(4))/65536.f << ",";
  val=(int) input->readLong(1);
  if (val==1) f << "same[colW]?,";
  else if (val) f << "#same[colW]=" << val << ",";
  val=(int) input->readLong(1);
  if (val==1) {
    f << "hasColSep,";
    m_state->m_hasColSep = true;
  }
  else if (val) f << "#hasColSep=" << val << ",";
  input->seek(pos+12, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i=0; i < 14; ++i) {
    pos = input->tell();
    f.str("");
    if (i<4) {
      static char const *wh[]= {"margins", "header/footer", "1", "pageDim" };
      f << "DocInfo[" << wh[i] << "]:";
    }
    else
      f << "DocInfo[" << i << "]:";

    double dim[4];
    for (int j=0; j<4; ++j)
      dim[j]=double(input->readLong(4))/65536.;
    if (dim[0]>0 || dim[1]>0||dim[2]>0||dim[3]>0) {
      f << "dim=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
      if (i==0) {
        getPageSpan().setMarginTop(dim[0]/72.0);
        getPageSpan().setMarginBottom(dim[2]/72.0);
        getPageSpan().setMarginLeft(dim[1]/72.0);
        getPageSpan().setMarginRight(dim[3]/72.0);
      }
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+16, librevenge::RVNG_SEEK_SET);
  }
  for (int st=0; st < 2; ++st) {
    pos=input->tell();
    f.str("");
    if (st==0)
      f << "DocInfo[leftPage]:";
    else
      f << "DocInfo[rightPage]:";
    for (int i=0; i < 12; ++i) {
      double dim[4];
      for (int j=0; j<4; ++j)
        dim[j]=double(input->readLong(4))/65536.;
      if (dim[0]>0 || dim[1]>0||dim[2]>0||dim[3]>0) {
        switch (i) {
        case 0:
          f << "header=";
          break;
        case 11:
          f << "footer=";
          break;
        default:
          f << "col" << i-1 << "=";
          if (st==1)
            continue;
          m_state->m_columnsWidth.push_back(dim[1]);
          m_state->m_columnsWidth.push_back(dim[3]);
          break;
        }
        f << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
      }
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool GreatWksParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = GreatWksParserInternal::State();
  if (!m_document->checkHeader(header,strict)) return false;
  return getParserState()->m_kind==MWAWDocument::MWAW_K_TEXT;
}



// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
