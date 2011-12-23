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
 * For further information visit http://libwps.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

/*
 * Parser to Claris Works text document ( spreadsheet part )
 *
 */
#ifndef CW_MWAW_SPREADSHEET
#  define CW_MWAW_SPREADSHEET

#include <list>
#include <string>
#include <vector>

#include "DMWAWPageSpan.hxx"

#include "TMWAWPosition.hxx"

#include "IMWAWEntry.hxx"
#include "IMWAWContentListener.hxx"
#include "IMWAWSubDocument.hxx"

#include "TMWAWDebug.hxx"
#include "TMWAWInputStream.hxx"

#include "IMWAWParser.hxx"

#include "CWStruct.hxx"

typedef class MWAWContentListener CWContentListener;
typedef shared_ptr<CWContentListener> CWContentListenerPtr;

namespace MWAWStruct
{
struct Font;
}

namespace MWAWTools
{
class Convertissor;
typedef shared_ptr<Convertissor> ConvertissorPtr;
}

namespace CWSpreadsheetInternal
{
struct Spreadsheet;
struct Field;
struct State;
}

class CWParser;

/** \brief the main class to read the text part of Claris Works file
 *
 *
 *
 */
class CWSpreadsheet
{
  friend class CWParser;

public:
  //! constructor
  CWSpreadsheet(TMWAWInputStreamPtr ip, CWParser &parser, MWAWTools::ConvertissorPtr &convertissor);
  //! destructor
  virtual ~CWSpreadsheet();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

  //! reads the zone Text DSET
  shared_ptr<CWStruct::DSET> readSpreadsheetZone
  (CWStruct::DSET const &zone, IMWAWEntry const &entry, bool &complete);

protected:

  //! sets the listener in this class and in the helper classes
  void setListener(CWContentListenerPtr listen) {
    m_listener = listen;
  }

  //
  // Intermediate level
  //

  /** try to read the first spreadsheet zone */
  bool readZone1(CWSpreadsheetInternal::Spreadsheet &sheet);

  /** try to read the second spreadsheet zone */
  bool readZone2(CWSpreadsheetInternal::Spreadsheet &sheet);

  //! try to read the record structure
  bool readContent(CWSpreadsheetInternal::Spreadsheet &sheet);

  //
  // low level
  //

  //! returns the debug file
  libmwaw_tools::DebugFile &ascii() {
    return m_asciiFile;
  }

private:
  CWSpreadsheet(CWSpreadsheet const &orig);
  CWSpreadsheet &operator=(CWSpreadsheet const &orig);

protected:
  //
  // data
  //
  //! the input
  TMWAWInputStreamPtr m_input;

  //! the listener
  CWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWTools::ConvertissorPtr m_convertissor;

  //! the state
  shared_ptr<CWSpreadsheetInternal::State> m_state;

  //! the main parser;
  CWParser *m_mainParser;

  //! the debug file
  libmwaw_tools::DebugFile &m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
