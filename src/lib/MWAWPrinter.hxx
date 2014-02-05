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

/* This header contains code specific to a mac file needed to read
 * TPrint structure (ie. the structure which keeps the printer parameters)
 * see http://developer.apple.com/documentation/Mac/QuickDraw/QuickDraw-411.html
 *   or http://www.mactech.com/articles/mactech/Vol.01/01.09/AllAboutPrinting/index.html
 */

#ifndef MWAW_PRINTER
#  define MWAW_PRINTER

#  include <assert.h>
#  include <ostream>

#  include "libmwaw_internal.hxx"

namespace libmwaw
{
/** \struct PrinterInfoData
    \brief internal structure used to keep TPrint content */
struct PrinterInfoData;

//! the Apple© rectangle : Rect
struct PrinterRect {
  //! returns the size
  Vec2i size() const
  {
    return m_pos[1]-m_pos[0];
  }
  //! returns the position ( 0: leftTop, 1:rightBottom )
  Vec2i pos(int i) const
  {
    return m_pos[i];
  }

  //! operator <<
  friend std::ostream &operator<< (std::ostream &o, PrinterRect const &r)
  {
    o << "[" << r.m_pos[0] << " " << r.m_pos[1] << "]";
    return o;
  }

  //! read value in a file, knowing the resolution
  bool read(MWAWInputStreamPtr input, Vec2i const &res);

protected:
  //! the LT and RB positions
  Vec2i m_pos[2];
};

//! the Apple© printer information : TPrint
struct PrinterInfo {
  //! constructor
  PrinterInfo();
  //! destructor
  ~PrinterInfo();

  //! returns the page rectangle
  PrinterRect page() const;
  //! returns the paper rectangle
  PrinterRect paper() const;

  friend std::ostream &operator<< (std::ostream &o, PrinterInfo const &r);

  //! reads the struture in a file
  bool read(MWAWInputStreamPtr input);

protected:
  //! internal data
  shared_ptr<PrinterInfoData> m_data;
private:
  PrinterInfo(PrinterInfo const &orig);
  PrinterInfo &operator=(PrinterInfo const &orig);
};
}

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
