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
 * Parser to Claris Works text document ( database part )
 *
 */
#ifndef CW_MWAW_DATABASE
#  define CW_MWAW_DATABASE

#include <list>
#include <string>
#include <vector>

#include "DMWAWPageSpan.hxx"

#include "MWAWPosition.hxx"

#include "MWAWEntry.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

#include "CWStruct.hxx"

typedef class MWAWContentListener CWContentListener;
typedef shared_ptr<CWContentListener> CWContentListenerPtr;

namespace MWAWStruct
{
class Font;
}

namespace MWAWTools
{
class Convertissor;
typedef shared_ptr<Convertissor> ConvertissorPtr;
}

namespace CWDatabaseInternal
{
struct Database;
struct Field;
struct State;
}

class CWParser;

/** \brief the main class to read the text part of Claris Works file
 *
 *
 *
 */
class CWDatabase
{
  friend class CWParser;

public:
  //! constructor
  CWDatabase(MWAWInputStreamPtr ip, CWParser &parser, MWAWTools::ConvertissorPtr &convertissor);
  //! destructor
  virtual ~CWDatabase();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

  //! reads the zone Text DSET
  shared_ptr<CWStruct::DSET> readDatabaseZone
  (CWStruct::DSET const &zone, MWAWEntry const &entry, bool &complete);

protected:

  //! sets the listener in this class and in the helper classes
  void setListener(CWContentListenerPtr listen) {
    m_listener = listen;
  }

  //
  // Intermediate level
  //
  //! try to read the database structure
  bool readDatabaseFields(CWDatabaseInternal::Database &dBase);

  //! try to read the default structure
  bool readDatabaseDefaults(CWDatabaseInternal::Database &dBase);

  //! try to read the record structure
  bool readDatabaseContent(CWDatabaseInternal::Database &dBase);

  //
  // low level
  //

  //! returns the debug file
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

private:
  CWDatabase(CWDatabase const &orig);
  CWDatabase &operator=(CWDatabase const &orig);

protected:
  //
  // data
  //
  //! the input
  MWAWInputStreamPtr m_input;

  //! the listener
  CWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWTools::ConvertissorPtr m_convertissor;

  //! the state
  shared_ptr<CWDatabaseInternal::State> m_state;

  //! the main parser;
  CWParser *m_mainParser;

  //! the debug file
  libmwaw::DebugFile &m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
