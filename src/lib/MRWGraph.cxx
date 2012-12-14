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

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWCell.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPictBasic.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "MRWParser.hxx"

#include "MRWGraph.hxx"

/** Internal: the structures of a MRWGraph */
namespace MRWGraphInternal
{
////////////////////////////////////////
//! Internal: the state of a MRWGraph
struct State {
  //! constructor
  State() : m_numPages(0) { }

  int m_numPages /* the number of pages */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MRWGraph
class SubDocument : public MWAWSubDocument
{
public:
  //! constructor
  SubDocument(MRWGraph &pars, MWAWInputStreamPtr input, MWAWPosition pos, long id, int subId=0) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_graphParser(&pars), m_id(id), m_subId(subId), m_pos(pos) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  /** the graph parser */
  MRWGraph *m_graphParser;
  //! the zone id
  long m_id;
  //! the zone subId ( for table cell )
  long m_subId;
  //! the position in a frame
  MWAWPosition m_pos;

private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("MRWGraphInternal::SubDocument::parse: no listener\n"));
    return;
  }
  MRWContentListener *listen = dynamic_cast<MRWContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("MRWGraphInternal::SubDocument::parse: bad listener\n"));
    return;
  }

  assert(m_graphParser);

  long pos = m_input->tell();
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_graphParser != sDoc->m_graphParser) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_subId != sDoc->m_subId) return true;
  if (m_pos != sDoc->m_pos) return true;
  return false;
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MRWGraph::MRWGraph
(MWAWInputStreamPtr ip, MRWParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new MRWGraphInternal::State),
  m_mainParser(&parser), m_asciiFile(parser.ascii())
{
}

MRWGraph::~MRWGraph()
{ }

int MRWGraph::version() const
{
  return m_mainParser->version();
}

int MRWGraph::numPages() const
{
  if (m_state->m_numPages)
    return m_state->m_numPages;
  int nPages = 0;
  m_state->m_numPages = nPages;
  return nPages;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////

bool MRWGraph::readToken(MRWEntry const &entry, int)
{
  if (entry.length() < 3) {
    MWAW_DEBUG_MSG(("MRWGraph::readToken: data seems to short\n"));
    return false;
  }
  m_input->seek(entry.begin(), WPX_SEEK_SET);
  m_input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList, 100);
  m_input->popLimit();

  size_t numData = dataList.size();
  if (numData < 16) {
    MWAW_DEBUG_MSG(("MRWGraph::readToken: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  f << entry.name() << ":";
  size_t d = 0;
  long val;
  MRWStruct dt = dataList[d++];
  if (!dt.isBasic()) {
    MWAW_DEBUG_MSG(("MRWGraph::readToken: find unexpected type for id\n"));
    f << "###id=" << dt << ",";
  } else
    f << "id=" << std::hex << dt.value(0) << std::dec << ",";

  dt = dataList[d++];
  if (!dt.isBasic()) {
    MWAW_DEBUG_MSG(("MRWGraph::readToken: find unexpected type for f0\n"));
    f << "###f0=" << dt << ",";
  } else if (dt.value(0) != 5) // always 5
    f << "f0=" << std::hex << dt.value(0) << std::dec << ",";

  dt = dataList[d++];
  int type = 0, subType = 0;
  if (!dt.isBasic()) {
    MWAW_DEBUG_MSG(("MRWGraph::readToken: find unexpected type for type\n"));
    f << "###type=" << dt << ",";
  } else {
    val = dt.value(0);
    type = int(val >> 16);
    subType = int(val&0xFFFF);
  }
  switch(type) {
  case 0:
    f << "graph";
    if (subType!=20) f << "[" << subType << "]";
    f << ",";
    break;
  case 1:
    if (subType==0x17) f << "date,";
    else if (subType==0x18) f << "time,";
    else if (subType==0x19) f << "page,";
    else f << "#fieldType=" << subType << ",";
    break;
  default:
    f << "#type=" << type;
    if (subType) f << "[" << subType << "]";
    f << ",";
    break;
  }
  int dim[2];
  for (int i = 0; i < 2; i++) {
    MRWStruct const &data = dataList[d++];
    if (!data.isBasic()) {
      MWAW_DEBUG_MSG(("MRWGraph::readToken: can not read dim\n"));
      f << "###dim" << i << "=" << data << ",";
    } else
      dim[i] = int(data.value(0));
  }
  f << "dim=" << dim[0] << "x" << dim[1] << ",";
  for (int i = 0; i < 4; i++) { // always 0, except for field, find f4=0|1|6
    MRWStruct const &data = dataList[d++];
    if (!data.isBasic()) {
      MWAW_DEBUG_MSG(("MRWGraph::readToken: can not read f%d\n", i+1));
      f << "###f" << i+1 << "=" << dt << ",";
    } else if (data.value(0))
      f << "f" << i+1 << "=" << data.value(0) << ",";
  }

  dt = dataList[d++];
  if (!dt.isBasic()) {
    MWAW_DEBUG_MSG(("MRWGraph::readToken: find unexpected type for id2\n"));
    f << "###id2=" << dt << ",";
  } else if (dt.value(0))
    f << "id2=" << std::hex << dt.value(0) << std::dec << ",";

  dt = dataList[d++];
  if (!dt.isBasic()) {
    MWAW_DEBUG_MSG(("MRWGraph::readToken: find unexpected type for id2\n"));
    f << "###id2=" << dt << ",";
  } else if (int(dt.value(0)) != 1-type)
    f << "#isGraph=" << dt.value(0) << ",";
  for (int i = 0; i < 3; i++) { // always 0
    MRWStruct const &data = dataList[d++];
    if (!data.isBasic()) {
      MWAW_DEBUG_MSG(("MRWGraph::readToken: can not read f%d\n", i+5));
      f << "###f" << i+5 << "=" << data << ",";
    } else if (data.value(0))
      f << "f" << i+5 << "=" << data << ",";
  }

  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  ascii().addPos(dataList[d].m_filePos);
  f.str("");
  f << entry.name() << "(II):";
  for (int i = 0; i < 2; i++) {
    MRWStruct const &data = dataList[d++];
    std::string str;
    if (i==0 && readTokenBlock0(data, str)) {
      if (str.length())
        f << "block0=[" << str << "],";
      continue;
    }
    if (data.m_type != 0) {
      MWAW_DEBUG_MSG(("MRWGraph::readToken(II): can not read block%d\n", i));
      f << "###bl" << i << "=" << data << ",";
    } else {
      f << "bl" << i << "=[";
      m_input->seek(data.m_pos.begin(), WPX_SEEK_SET);
      for (int j = 0; j < int(data.m_pos.length()/2); j++) {
        val = (long) m_input->readULong(2);
        if (val) f << "f" << j << "=" << std::hex << val << std::dec << ",";
      }
      f << "],";
    }
  }

  if (type || numData < 32) {
    for ( ; d < numData; d++) {
      MRWStruct const &data = dataList[d];
      f << "#" << data << ",";
      static bool first = true;
      if (first) {
        first = false;
        MWAW_DEBUG_MSG(("MRWGraph::readToken: find some extra data \n"));
      }
    }
    ascii().addNote(f.str().c_str());

    m_input->seek(entry.end(), WPX_SEEK_SET);
    return true;
  }

  // ok now read the picture data
  ascii().addNote(f.str().c_str());
  ascii().addPos(dataList[d].m_filePos);
  f.str("");
  f << entry.name() << "(III):";
  for (int i = 0; i < 8; i++) { // always 0
    MRWStruct const &data = dataList[d++];
    if (!data.isBasic()) {
      MWAW_DEBUG_MSG(("MRWGraph::readToken(III): can not read f%d\n", i));
      f << "###f" << i << "=" << data << ",";
    } else if (data.value(0))
      f << "f" << i << "=" << data << ",";
  }
  dt = dataList[d++];
  if (!dt.isBasic()) {
    MWAW_DEBUG_MSG(("MRWGraph::readToken: find unexpected type for pictId\n"));
    f << "###pictId=" << dt << ",";
  } else if (dt.value(0))
    f << "pictId=" << std::hex << dt.value(0) << std::dec << ",";
  for (int i = 0; i < 6; i++) { // always 0
    MRWStruct const &data = dataList[d++];
    if (!data.isBasic()) {
      MWAW_DEBUG_MSG(("MRWGraph::readToken(III): can not read g%d\n", i));
      f << "###g" << i << "=" << data << ",";
    } else if (data.value(0))
      f << "g" << i << "=" << data << ",";
  }

  dt = dataList[d++];
  if (dt.m_type != 0 || !dt.m_pos.length()) {
    MWAW_DEBUG_MSG(("MRWGraph::readToken: can not find the picture data\n"));
    f << "###pictData=" << dt << ",";
  } else {
#ifdef DEBUG_WITH_FILES
    m_input->seek(dt.m_pos.begin(), WPX_SEEK_SET);
    WPXBinaryData file;
    m_input->readDataBlock(dt.m_pos.length(), file);

    static int volatile pictName = 0;
    std::stringstream fName;
    fName << "Pict" << ++pictName << ".pct";
    libmwaw::Debug::dumpFile(file, fName.str().c_str());

    ascii().skipZone(dt.m_pos.begin(),dt.m_pos.end()-1);
#endif
  }

  for ( ; d < numData; d++) {
    MRWStruct const &data = dataList[d];
    f << "#" << data << ",";
    static bool first = true;
    if (first) {
      first = false;
      MWAW_DEBUG_MSG(("MRWGraph::readToken: find some extra data \n"));
    }
  }
  ascii().addNote(f.str().c_str());

  m_input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

bool MRWGraph::readTokenBlock0(MRWStruct const &data, std::string &res)
{
  res = "";
  if (data.m_type != 0 || !data.m_pos.valid()) {
    MWAW_DEBUG_MSG(("MRWGraph::readTokenBlock0: called without data\n"));
    return false;
  }
  if (data.m_pos.length()<0x2c) {
    MWAW_DEBUG_MSG(("MRWGraph::readTokenBlock0: the data seems very short\n"));
    return false;
  }

  std::stringstream f;

  long pos = data.m_pos.begin();
  m_input->seek(pos, WPX_SEEK_SET);

  std::string fValue("");
  long val;
  for (int i = 0; i < 6; i++) { // pagenumber, ... value
    val = (long) m_input->readULong(1);
    if (!val) break;
    fValue += (char) val;
  }
  if (fValue.length()) f << "val=" << fValue << ",";

  m_input->seek(pos+6, WPX_SEEK_SET);
  val = m_input->readLong(2);
  if (val) f << "f0=" << val << ","; // find 6 in a data field

  fValue = "";
  for (int i = 0; i < 20; i++) { // data/time, ... value
    val = (long) m_input->readULong(1);
    if (!val) break;
    fValue += (char) val;
  }
  if (fValue.length()) f << "val=" << fValue << ",";
  m_input->seek(pos+28, WPX_SEEK_SET);

  val = m_input->readLong(4);
  if (val) f << "pId=" << std::hex << val << ",";

  int numRemains = int(data.m_pos.end()-m_input->tell())/2;
  for (int i = 0; i < numRemains; i++) { // always 0
    val = m_input->readLong(2);
    if (val) f << "f" << i+2 << "=" << val << ",";
  }
  res = f.str();
  return true;
}

bool MRWGraph::readPostscript(MRWEntry const &entry, int )
{
  if (entry.length() < 3) {
    MWAW_DEBUG_MSG(("MRWGraph::readPostscript: data seems to short\n"));
    return false;
  }
  m_input->seek(entry.begin(), WPX_SEEK_SET);
  m_input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList, 1+3);
  m_input->popLimit();

  if (int(dataList.size()) != 3) {
    MWAW_DEBUG_MSG(("MRWGraph::readPostscript: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  f << entry.name() << ":";
  size_t d = 0;
  long id = 0;
  for (int i = 0; i < 2; i++) {
    MRWStruct const &data = dataList[d++];
    if (!data.isBasic()) {
      MWAW_DEBUG_MSG(("MRWGraph::readPostscript: find unexpected type for f0\n"));
      f << "###f" << i << "=" << data << ",";
    } else if (data.value(0)) {
      if (i==0) // always 0?
        f << "f" << i << "=" << data.value(0) << ",";
      else if (i==1) {
        id = data.value(0);
        f << "id=" << std::hex << id << std::dec << ",";
      }
    }
  }
  MRWStruct const &data = dataList[d++];
  if (data.m_type != 0) {
    MWAW_DEBUG_MSG(("MRWGraph::readPostscript: can not find my file\n"));
  } else if (data.m_pos.valid()) {
#ifdef DEBUG_WITH_FILES
    m_input->seek(data.m_pos.begin(), WPX_SEEK_SET);
    WPXBinaryData file;
    m_input->readDataBlock(data.m_pos.length(), file);

    static int volatile psName = 0;
    std::stringstream fName;
    fName << "PS" << ++psName << ".ps";
    libmwaw::Debug::dumpFile(file, fName.str().c_str());

    ascii().skipZone(data.m_pos.begin(),data.m_pos.end()-1);
#endif
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  m_input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// send data to a listener
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// low level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// send data
////////////////////////////////////////////////////////////
bool MRWGraph::sendPageGraphics()
{
  return true;
}

void MRWGraph::flushExtra()
{
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
