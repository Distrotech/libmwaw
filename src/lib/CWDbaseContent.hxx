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
 * The main database content used by Claris Works parser to store spreedsheet/Databese
 *
 */
#ifndef CW_DBASE_CONTENT
#  define CW_DBASE_CONTENT

#include <iostream>
#include <map>
#include <vector>

#include "libmwaw_internal.hxx"

#include "MWAWEntry.hxx"

class CWStyleManager;

//! small structure used to parse main content of a spreadsheet/database zone
class CWDbaseContent
{
public:
  //! constructor
  CWDbaseContent(MWAWParserStatePtr parserState, shared_ptr<CWStyleManager> styleManager, bool spreadsheet);
  //! destructor
  ~CWDbaseContent();
  //! try to read the record structure
  bool readContent();

  //! returns the dimension of the read data
  bool getExtrema(Vec2i &min, Vec2i &max) const;
  //! returns the list of filled record (for database)
  bool getRecordList(std::vector<int> &list) const;

  //! try to send a cell content to the listener
  bool send(Vec2i const &pos);
protected:
  /** struct which stores a record in CWDbaseContent */
  struct Record {
    //! different result type
    enum Type { R_Unknown, R_Long, R_Double, R_String };
    //! contructor
    Record() : m_style(-1), m_resType(R_Unknown), m_resLong(0), m_resDouble(0), m_resString() {
    }
    //! the style if known
    int m_style;
    //! the result type
    Type m_resType;
    //! the result if int/long
    long m_resLong;
    //! the result id double
    double m_resDouble;
    //! the result entry if string
    MWAWEntry m_resString;
  };
  /** struct which stores a column in CWDbaseContent */
  struct Column {
    //! constructor
    Column() : m_idRecordMap() {
    }
    //! a map line (or record id) to record
    std::map<int,Record> m_idRecordMap;
  };

  //! try to read the columns list structure(CTAB)
  bool readColumnList();
  //! try to read the column structure(COLM): a list of chnk
  bool readColumn(int c);
  //! try to read a list of records(CHNK)
  bool readRecordList(Vec2i const &where, Column &col);
  //! try to read a spreadsheet record
  bool readRecordSS(Vec2i const &where, long pos, Record &record);
  //! try to read a spreadsheet record(v1-v3)
  bool readRecordSSV1(Vec2i const &where, long pos, Record &record);
  //! try to read a database record
  bool readRecordDB(Vec2i const &where, long pos, Record &record);

  //! the file version
  int m_version;
  //! a bool to know if this is a spreadsheet or a database
  bool m_isSpreadsheet;

  //! the parser state
  MWAWParserStatePtr m_parserState;
  //! the style manager
  shared_ptr<CWStyleManager> m_styleManager;

  //! a map col id to column
  std::map<int, Column> m_idColumnMap;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:


