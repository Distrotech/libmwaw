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

/*
 * Parser to Claris Works text document ( database part )
 *
 */
#ifndef CW_MWAW_DATABASE
#  define CW_MWAW_DATABASE

#include <list>
#include <string>
#include <vector>

#include "MWAWEntry.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

#include "CWStruct.hxx"

class MWAWContentListener;
typedef shared_ptr<MWAWContentListener> MWAWContentListenerPtr;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

namespace CWDatabaseInternal
{
struct Database;
struct Field;
struct State;
}

class CWParser;
class CWStyleManager;

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
  CWDatabase(MWAWInputStreamPtr ip, CWParser &parser, MWAWFontConverterPtr &convertissor);
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
  void setListener(MWAWContentListenerPtr listen) {
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
  MWAWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<CWDatabaseInternal::State> m_state;

  //! the main parser;
  CWParser *m_mainParser;

  //! the style manager
  shared_ptr<CWStyleManager> m_styleManager;

  //! the debug file
  libmwaw::DebugFile &m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
