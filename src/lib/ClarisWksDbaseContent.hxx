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
#ifndef CLARIS_WKS_DBASE_CONTENT
#  define CLARIS_WKS_DBASE_CONTENT

#include <iostream>
#include <map>
#include <vector>

#include "libmwaw_internal.hxx"

#include "MWAWEntry.hxx"
#include "MWAWFont.hxx"

// remove me
#include "ClarisWksStyleManager.hxx"

class ClarisWksStyleManager;

//! small structure used to parse main content of a spreadsheet/database zone
class ClarisWksDbaseContent
{
public:
  //! constructor
  ClarisWksDbaseContent(MWAWParserStatePtr parserState, shared_ptr<ClarisWksStyleManager> styleManager, bool spreadsheet);
  //! destructor
  ~ClarisWksDbaseContent();
  //! try to read the record structure
  bool readContent();

  //! returns the dimension of the read data
  bool getExtrema(Vec2i &min, Vec2i &max) const;
  //! returns the list of filled record (for database)
  bool getRecordList(std::vector<int> &list) const;

  //! try to send a cell content to the listener
  bool send(Vec2i const &pos);
  //! set the field format ( for database )
  void setDatabaseFormats(std::vector<ClarisWksStyleManager::CellFormat> const &format);
protected:
  /** struct which stores a record in ClarisWksDbaseContent */
  struct Record {
    //! different result type
    enum Type { R_Unknown, R_Long, R_Double, R_String };
    //! contructor
    Record() : m_style(-1), m_resType(R_Unknown), m_resLong(0), m_resDouble(0), m_resDoubleNaN(false), m_resString(), m_format(0), m_font(3,9), m_justify(0), m_borders(0)
    {
    }

    //! the style if known
    int m_style;
    //! the result type
    Type m_resType;
    //! the result if int/long
    long m_resLong;
    //! the result if double
    double m_resDouble;
    //! a flag to know if a double result is nan or not
    bool m_resDoubleNaN;
    //! the result entry if string
    MWAWEntry m_resString;
    //! the format ( in a v1-3 spreadsheet)
    int m_format;
    //! the font ( in v1-3 spreadsheet)
    MWAWFont m_font;
    //! cell justify in v1-3 spreadsheet
    int m_justify;
    //! the border in v1-3 spreadsheet
    int m_borders;
  };
  /** struct which stores a column in ClarisWksDbaseContent */
  struct Column {
    //! constructor
    Column() : m_idRecordMap()
    {
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

  //! send a double with a corresponding cell format
  void send(double val, bool isNotaNumber, ClarisWksStyleManager::CellFormat const &format);

  //! the file version
  int m_version;
  //! a bool to know if this is a spreadsheet or a database
  bool m_isSpreadsheet;

  //! the parser state
  MWAWParserStatePtr m_parserState;
  //! the style manager
  shared_ptr<ClarisWksStyleManager> m_styleManager;

  //! a map col id to column
  std::map<int, Column> m_idColumnMap;
  //! the databse format
  std::vector<ClarisWksStyleManager::CellFormat> m_dbFormatList;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:


