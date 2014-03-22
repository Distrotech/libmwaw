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
#include "MWAWParser.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSection.hxx"
#include "MWAWRSRCParser.hxx"

#include "MsWksGraph.hxx"
#include "MsWks3Text.hxx"

#include "MsWksDocument.hxx"

/** Internal: the structures of a MsWksDocument */
namespace MsWksDocumentInternal
{

////////////////////////////////////////
//! Internal: the state of a MsWksDocument
struct State {
  //! constructor
  State() : m_kind(MWAWDocument::MWAW_K_TEXT), m_hasHeader(false), m_hasFooter(false),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }

  //! the type of document
  MWAWDocument::Kind m_kind;
  bool m_hasHeader /** true if there is a header v3*/, m_hasFooter /** true if there is a footer v3*/;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MsWksDocument::MsWksDocument(MWAWInputStreamPtr input, MWAWParser &parser) :
  m_state(), m_parserState(parser.getParserState()), m_input(input), m_parser(&parser), m_asciiFile(),
  m_graphParser(), m_textParser3(),
  m_newPage(0), m_sendFootnote(0), m_sendTextbox(0), m_sendOLE(0), m_sendRBIL(0)
{
  m_state.reset(new MsWksDocumentInternal::State);

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

////////////////////////////////////////////////////////////
// interface via callback
////////////////////////////////////////////////////////////
void MsWksDocument::newPage(int page, bool softBreak)
{
  if (!m_newPage) {
    MWAW_DEBUG_MSG(("MsWksDocument::newPage: can not find the newPage callback\n"));
    return;
  }
  (m_parser->*m_newPage)(page, softBreak);
}

void MsWksDocument::sendFootnote(int id)
{
  if (!m_sendFootnote) {
    MWAW_DEBUG_MSG(("MsWksDocument::sendFootnote: can not find the sendFootnote callback\n"));
    return;
  }
  (m_parser->*m_sendFootnote)(id);
}

void MsWksDocument::sendOLE(int id, MWAWPosition const &pos, MWAWGraphicStyle const &style)
{
  if (!m_sendOLE) {
    MWAW_DEBUG_MSG(("MsWksDocument::sendOLE: can not find the sendOLE callback\n"));
    return;
  }
  (m_parser->*m_sendOLE)(id, pos, style);
}

void MsWksDocument::sendRBIL(int id, Vec2i const &sz)
{
  if (!m_sendRBIL) {
    MWAW_DEBUG_MSG(("MsWksDocument::sendRBIL: can not find the sendRBIL callback\n"));
    return;
  }
  (m_parser->*m_sendRBIL)(id, sz);
}

void MsWksDocument::sendTextbox(MWAWEntry const &entry, std::string const &frame)
{
  if (!m_sendTextbox) {
    MWAW_DEBUG_MSG(("MsWksDocument::sendTextbox: can not find the sendTextbox callback\n"));
    if (m_parserState->getMainListener())
      m_parserState->getMainListener()->insertChar(' ');
    return;
  }
  (m_parser->*m_sendTextbox)(entry,frame);
}

////////////////////////////////////////////////////////////
// interface with the text document
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// interface with the graph document
////////////////////////////////////////////////////////////

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
// read the print info
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read some unknown zone
////////////////////////////////////////////////////////////

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
#ifndef DEBUG
    return false;
#else
    setVersion(4);
    break; // no-text Works4 file have classic header
#endif
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

    MWAW_DEBUG_MSG(("MsWksDocument::checkHeader3: find unknown version 0x%x\n", vers));
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
      MWAW_DEBUG_MSG(("MsWksDocument::checkHeader3: find odd val%d=0x%x: not implemented\n", i, val));
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
  if (dim[2] <= dim[0] || dim[3] <= dim[1]) {
    MWAW_DEBUG_MSG(("MsWksDocument::checkHeader3: find odd bdbox\n"));
    numError++;
  }
  f << ", windowdbdbox?=(";
  for (int i = 0; i < 4; i++) f << dim[i]<<",";
  f << "),";
  for (int i = 0; i < 4; i++) {
    val = (int) input->readULong(1);
    if (!val) continue;
    f << "##v" << i << "=" << std::hex << val <<",";
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
    if (m_state->m_kind==MWAWDocument::MWAW_K_TEXT && version() >= 3 && i == 12) {
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

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
