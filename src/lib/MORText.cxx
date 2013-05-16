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
//! Internal: the state of a MORText
struct State {
  //! constructor
  State() : m_colorList(), m_version(-1), m_numPages(-1), m_actualPage(1) {
  }

  //! set the default color map
  void setDefaultColorList(int version);
  //! a list colorId -> color
  std::vector<MWAWColor> m_colorList;
  //! the file version
  mutable int m_version;
  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
};

void State::setDefaultColorList(int version)
{
  if (m_colorList.size()) return;
  if (version==3) {
    uint32_t const defCol[20] = {
      0x000000, 0xff0000, 0x00ff00, 0x0000ff, 0x00ffff, 0xff00db, 0xffff00, 0x8d02ff,
      0xff9200, 0x7f7f7f, 0x994914, 0x000000, 0x484848, 0x880000, 0x008600, 0x838300,
      0xff9200, 0x7f7f7f, 0x994914, 0xfffff
    };
    m_colorList.resize(20);
    for (size_t i = 0; i < 20; i++)
      m_colorList[i] = defCol[i];
    return;
  }
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

  int nPages=1;
  // fixme
  return m_state->m_numPages = nPages;
}

bool MORText::getColor(int id, MWAWColor &col) const
{
  int numColor = (int) m_state->m_colorList.size();
  if (!numColor) {
    m_state->setDefaultColorList(version());
    numColor = int(m_state->m_colorList.size());
  }
  if (id < 0 || id >= numColor)
    return false;
  col = m_state->m_colorList[size_t(id)];
  return true;
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


//
// send the text
//

//
// send a graphic
//

//////////////////////////////////////////////
// Fonts
//////////////////////////////////////////////

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
