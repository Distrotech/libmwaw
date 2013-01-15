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

#include <iostream>

#include "libmwaw_internal.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWPrinter.hxx"

namespace libmwaw
{
//------------------------------------------------------------
//
// The printer information
// see http://www.mactech.com/articles/mactech/Vol.01/01.09/AllAboutPrinting/index.html
//
//------------------------------------------------------------


//
// PrinterRect : a rectangle
//
bool PrinterRect::read(MWAWInputStreamPtr input, Vec2i const &res)
{
  int x,y;
  for (int st = 0; st < 2; st++) {
    y = int(input->readLong(2));
    y = int(float(y)*72./float(res.y()));
    x = int(input->readLong(2));
    x = int(float(x)*72./float(res.x()));
    m_pos[st].set(x,y);
  }

  if (input->atEOS()) return false;

  if (m_pos[0].x() > m_pos[1].x() || m_pos[0].y() > m_pos[1].y())
    return false;
  return true;
}

//! Internal: structure used to keep a rectangle with its resolution
struct PrinterRectResolution {
  PrinterRectResolution() : m_rect(), m_resolution(), m_iDev(-1) {}
  //! page dimension
  PrinterRect page() const {
    return m_rect;
  }
  //! resolution
  Vec2i const &resolution() const {
    return m_resolution;
  }

  //! operator <<
  friend std::ostream &operator<< (std::ostream &o, PrinterRectResolution const &r) {
    o << r.m_rect << ":" << r.m_resolution;
    return o;
  }
  //! reads the data from file
  bool read(MWAWInputStreamPtr input) {
    m_iDev = int(input->readLong(2));
    int y = int(input->readLong(2));
    int x = int(input->readLong(2));
    if (x <= 0 || y <= 0) return false;
    m_resolution.set(x,y);
    return m_rect.read(input, m_resolution);
  }

protected:
  //! returns the main rectangle
  PrinterRect m_rect;
  //! returns the resolution
  Vec2i m_resolution;
  //! a field which is reserved
  int m_iDev;
};

//! Internal: structure used to keep the printer style information
struct PrinterStyle {
  /** operator<<

  \note print nothing*/
  friend std::ostream &operator<< (std::ostream &o, PrinterStyle const & ) {
    return o;
  }
  //! reads data from file
  bool read(MWAWInputStreamPtr input) {
    m_wDev = (int) input->readLong(2);
    m_pageWidth = (int) input->readLong(2);
    m_pageHeight = (int) input->readLong(2);
    if (m_pageWidth < 0 || m_pageHeight < 0) return false;
    m_port = (int) input->readULong(1);
    m_feed = (int) input->readLong(1);
    if (input->atEOS()) return false;
    return true;
  }

protected:
  int m_wDev /** used internally */;
  int m_feed /** paper type: cut, fanfold, mechcut or other */;
  int m_pageHeight /** paper height */, m_pageWidth /** paper width*/, m_port /** printer or modem port */;
};

//! Internal: structure used to keep a printer job
struct PrinterJob {
  //! operator<<
  friend std::ostream &operator<< (std::ostream &o, PrinterJob const &r ) {
    o << "fP=" << r.m_firstPage << ", lP=" << r.m_lastPage << ", copies=" << r.m_copies;
    if (r.m_fileVol || r.m_fileVers) o << ", fVol=" << r.m_fileVol << ", fVers=" << r.m_fileVers;
    return o;
  }
  //! read data from file
  bool read(MWAWInputStreamPtr input) {
    m_firstPage = (int) input->readLong(2);
    m_lastPage = (int) input->readLong(2);
    m_copies = (int) input->readLong(2);
    m_jobDocLoop = (int) input->readULong(1);
    m_fromUser = (int) input->readLong(1);
    // skip pIdleProc
    if (input->seek(4, WPX_SEEK_CUR) != 0 || input->atEOS()) return false;
    // skip pFileName
    if (input->seek(4, WPX_SEEK_CUR) != 0 || input->atEOS()) return false;
    m_fileVol = (int) input->readLong(2);
    m_fileVers = (int) input->readLong(1);
    return true;
  }

protected:
  int m_firstPage /** first page*/, m_lastPage/** last page*/, m_copies /** number of copies */;
  int m_jobDocLoop/** printing method: draft or defered */;
  int m_fileVol /** volume reference number*/ , m_fileVers /** version of spool file */;
  //! reserved
  int m_fromUser;
};

//
// PrinterInfo storage class
//
struct PrinterInfoData {
  PrinterInfoData(): m_info(), m_paper(), m_feed(), m_info2(), m_job(), m_version(-1) {}

  //! printer information
  PrinterRectResolution m_info;
  //! paper
  PrinterRect m_paper;
  //! printer style
  PrinterStyle m_feed;
  //! printer information
  PrinterRectResolution m_info2;
  //! jobs
  PrinterJob m_job;
  //! reserved
  int m_version;
};

//
// PrinterInfo : ie. apple TPrint
//
PrinterInfo::PrinterInfo() : m_data(new PrinterInfoData) {}
PrinterInfo::~PrinterInfo()
{
}
PrinterInfo &PrinterInfo::operator=(PrinterInfo const &)
{
  MWAW_DEBUG_MSG(("PrinterInfo::operator: MUST NOT BE CALLED\n"));
  return *this;
}

PrinterRect PrinterInfo::page() const
{
  return m_data->m_info.page();
}
PrinterRect PrinterInfo::paper() const
{
  return m_data->m_paper;
}

//! operator<< for a PrinterInfo
std::ostream &operator<< (std::ostream &o, PrinterInfo const &r)
{
  o << "page = " << r.m_data->m_info << ", paper = " << r.m_data->m_paper
    << ", infoPt: " << r.m_data->m_info2 << ", jobs: [" << r.m_data->m_job << "]";
  return o;
}

bool PrinterInfo::read(MWAWInputStreamPtr input)
{
  m_data->m_version = (int) input->readLong(2);
  if (!m_data->m_info.read(input)) return false;
  if (!m_data->m_paper.read(input, m_data->m_info.resolution())) return false;
  if (!m_data->m_feed.read(input)) return false;
  long pos = input->tell();
  if (!m_data->m_info2.read(input)) {
    // can be left unfilled, so as we do not use the result, ...
    input->seek(pos+14, WPX_SEEK_SET);
    if (input->tell() != pos+14) return false;
  }
  // skip unknown structure prXInfo
  if (input->seek(16, WPX_SEEK_CUR) != 0 || input->atEOS()) return false;

  if (!m_data->m_job.read(input)) return false;
  input->readLong(1);

  // skip printX 19 short + 2 align
  pos = input->tell();
  if (input->seek(19*2,WPX_SEEK_CUR) != 0 || input->tell()!=pos+19*2) return false;
  return true;
}
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
