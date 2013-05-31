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
 *  freely inspired from istorage :
 * ------------------------------------------------------------
 *      Generic OLE Zones furnished with a copy of the file header
 *
 * Compound Storage (32 bit version)
 * Storage implementation
 *
 * This file contains the compound file implementation
 * of the storage interface.
 *
 * Copyright 1999 Francis Beaudet
 * Copyright 1999 Sylvain St-Germain
 * Copyright 1999 Thuy Nguyen
 * Copyright 2005 Mike McCormack
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * ------------------------------------------------------------
 */

#ifndef MWAW_OLE_PARSER_H
#define MWAW_OLE_PARSER_H

#include <string>
#include <vector>

#include <libwpd-stream/libwpd-stream.h>

#include "libmwaw_internal.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWDebug.hxx"

class WPXBinaryData;

namespace MWAWOLEParserInternal
{
class CompObj;
}

/** \brief a class used to parse some basic oles
    Tries to read the different ole parts and stores their contents in form of picture.
 */
class MWAWOLEParser
{
public:
  /** constructor
      \param mainName: name of the main ole, we must avoid to parse */
  MWAWOLEParser(std::string mainName);

  /** destructor */
  ~MWAWOLEParser();

  /** tries to parse basic OLE (excepted mainName)
      \return false if fileInput is not an Ole file */
  bool parse(MWAWInputStreamPtr fileInput);

  //! returns the list of unknown ole
  std::vector<std::string> const &getNotParse() const {
    return m_unknownOLEs;
  }

  //! returns the list of id for which we have find a representation
  std::vector<int> const &getObjectsId() const {
    return m_objectsId;
  }
  //! returns the list of data positions which have been read
  std::vector<MWAWPosition> const &getObjectsPosition() const {
    return m_objectsPosition;
  }
  //! returns the list of data which have been read
  std::vector<WPXBinaryData> const &getObjects() const {
    return m_objects;
  }
  //! returns the list of data type
  std::vector<std::string> const &getObjectsType() const {
    return m_objectsType;
  }

  //! returns the picture corresponding to an id
  bool getObject(int id, WPXBinaryData &obj, MWAWPosition &pos, std::string &type) const;

  /*! \brief sets an object
   * just in case, the external parsing find another representation
   */
  void setObject(int id, WPXBinaryData const &obj, MWAWPosition const &pos,
                 std::string const &type);

protected:

  //!  the "Ole" small structure : unknown contain
  static bool readOle(MWAWInputStreamPtr ip, std::string const &oleName,
                      libmwaw::DebugFile &ascii);
  //!  the "MM" small structure : seems to contain the file versions
  static bool readMM(MWAWInputStreamPtr input, std::string const &oleName,
                     libmwaw::DebugFile &ascii);
  //!  the "ObjInfo" small structure : seems to contain 3 ints=0,3,4
  static bool readObjInfo(MWAWInputStreamPtr input, std::string const &oleName,
                          libmwaw::DebugFile &ascii);
  //!  the "CompObj" contains : UserType,ClipName,ProgIdName
  bool readCompObj(MWAWInputStreamPtr ip, std::string const &oleName,
                   libmwaw::DebugFile &ascii);

  /** the OlePres001 seems to contain standart picture file and size */
  static  bool isOlePres(MWAWInputStreamPtr ip, std::string const &oleName);
  /** extracts the picture of OlePres001 if it is possible */
  static bool readOlePres(MWAWInputStreamPtr ip, WPXBinaryData &data, MWAWPosition &pos,
                          libmwaw::DebugFile &ascii);

  //! theOle10Native : basic Windows© picture, with no size
  static bool isOle10Native(MWAWInputStreamPtr ip, std::string const &oleName);
  /** extracts the picture if it is possible */
  static bool readOle10Native(MWAWInputStreamPtr ip, WPXBinaryData &data,
                              libmwaw::DebugFile &ascii);

  /** \brief the Contents : in general a picture : a PNG, an JPEG, a basic metafile,
   * I find also a Word art picture, which are not sucefull read
   */
  bool readContents(MWAWInputStreamPtr input, std::string const &oleName,
                    WPXBinaryData &pict, MWAWPosition &pos, libmwaw::DebugFile &ascii);

  /** the CONTENTS : seems to store a header size, the header
   * and then a object in EMF (with the same header)...
   * \note I only find such lib in 2 files, so the parsing may be incomplete
   *  and many such Ole rejected
   */
  bool readCONTENTS(MWAWInputStreamPtr input, std::string const &oleName,
                    WPXBinaryData &pict, MWAWPosition &pos, libmwaw::DebugFile &ascii);


  //! if filled, does not parse content with this name
  std::string m_avoidOLE;
  //! list of ole which can not be parsed
  std::vector<std::string> m_unknownOLEs;

  //! list of pictures read
  std::vector<WPXBinaryData> m_objects;
  //! list of picture size ( if known)
  std::vector<MWAWPosition> m_objectsPosition;
  //! list of pictures id
  std::vector<int> m_objectsId;
  //! list of picture type
  std::vector<std::string> m_objectsType;

  //! a smart ptr used to stored the list of compobj id->name
  shared_ptr<MWAWOLEParserInternal::CompObj> m_compObjIdName;

};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
