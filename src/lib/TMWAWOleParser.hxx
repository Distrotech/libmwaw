/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libwpd
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
 * For further information visit http://libmwaw.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
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

#include <list>
#include <map>
#include <string>
#include <vector>
#include <libwpd/WPXBinaryData.h>
#include <libwpd/WPXString.h>

#include <libwpd-stream/WPXStream.h>

#include "libmwaw_tools.hxx"
#include "TMWAWInputStream.hxx"
#include "TMWAWPosition.hxx"

#include "TMWAWDebug.hxx"

class WPXBinaryData;

namespace TMWAWOleParserInternal
{
class CompObj;
}

/** \brief a class used to parse some basic oles
    Tries to read the different ole parts and stores their contents in form of picture.
 */
class TMWAWOleParser
{
public:
  /** constructor
      \param mainName: name of the main ole, we must avoid to parse */
  TMWAWOleParser(char const *mainName);

  /** destructor */
  ~TMWAWOleParser();

  /** tries to parse basic OLE (excepted mainName)
      \return false if fileInput is not an Ole file */
  bool parse(TMWAWInputStreamPtr fileInput);

  //! returns the list of unknown ole
  std::vector<std::string> const &getNotParse() const {
    return m_unknownOLEs;
  }

  //! returns the list of id for which we have find a representation
  std::vector<int> const &getObjectsId() const {
    return m_objectsId;
  }
  //! returns the list of data positions which have been read
  std::vector<TMWAWPosition> const &getObjectsPosition() const {
    return m_objectsPosition;
  }
  //! returns the list of data which have been read
  std::vector<WPXBinaryData> const &getObjects() const {
    return m_objects;
  }

  //! returns the picture corresponding to an id
  bool getObject(int id, WPXBinaryData &obj, TMWAWPosition &pos)  const {
    for (int i = 0; i < int(m_objectsId.size()); i++) {
      if (m_objectsId[i] != id) continue;
      obj = m_objects[i];
      pos = m_objectsPosition[i];
      return true;
    }
    obj.clear();
    return false;
  }

  /*! \brief sets an object
   * just in case, the external parsing find another representation
   */
  void setObject(int id, WPXBinaryData const &obj, TMWAWPosition const &pos) {
    for (int i = 0; i < int(m_objectsId.size()); i++) {
      if (m_objectsId[i] != id) continue;
      m_objects[i] = obj;
      m_objectsPosition[i] = pos;
      return;
    }
    m_objects.push_back(obj);
    m_objectsPosition.push_back(pos);
    m_objectsId.push_back(id);
  }

protected:

  //!  the "Ole" small structure : unknown contain
  bool readOle(TMWAWInputStreamPtr ip, std::string const &oleName,
               libmwaw_tools::DebugFile &ascii);
  //!  the "MM" small structure : seems to contain the file versions
  bool readMM(TMWAWInputStreamPtr input, std::string const &oleName,
              libmwaw_tools::DebugFile &ascii);
  //!  the "ObjInfo" small structure : seems to contain 3 ints=0,3,4
  bool readObjInfo(TMWAWInputStreamPtr input, std::string const &oleName,
                   libmwaw_tools::DebugFile &ascii);
  //!  the "CompObj" contains : UserType,ClipName,ProgIdName
  bool readCompObj(TMWAWInputStreamPtr ip, std::string const &oleName,
                   libmwaw_tools::DebugFile &ascii);

  /** the OlePres001 seems to contain standart picture file and size */
  bool isOlePres(TMWAWInputStreamPtr ip, std::string const &oleName);
  /** extracts the picture of OlePres001 if it is possible */
  bool readOlePres(TMWAWInputStreamPtr ip, WPXBinaryData &data, TMWAWPosition &pos,
                   libmwaw_tools::DebugFile &ascii);

  //! theOle10Native : basic Windows© picture, with no size
  bool isOle10Native(TMWAWInputStreamPtr ip, std::string const &oleName);
  /** extracts the picture if it is possible */
  bool readOle10Native(TMWAWInputStreamPtr ip, WPXBinaryData &data,
                       libmwaw_tools::DebugFile &ascii);

  /** \brief the Contents : in general a picture : a PNG, an JPEG, a basic metafile,
   * I find also a Word art picture, which are not sucefull read
   */
  bool readContents(TMWAWInputStreamPtr input, std::string const &oleName,
                    WPXBinaryData &pict, TMWAWPosition &pos, libmwaw_tools::DebugFile &ascii);

  /** the CONTENTS : seems to store a header size, the header
   * and then a object in EMF (with the same header)...
   * \note I only find such lib in 2 files, so the parsing may be incomplete
   *  and many such Ole rejected
   */
  bool readCONTENTS(TMWAWInputStreamPtr input, std::string const &oleName,
                    WPXBinaryData &pict, TMWAWPosition &pos, libmwaw_tools::DebugFile &ascii);


  //! if filled, does not parse content with this name
  std::string m_avoidOLE;
  //! list of ole which can not be parsed
  std::vector<std::string> m_unknownOLEs;

  //! list of pictures read
  std::vector<WPXBinaryData> m_objects;
  //! list of picture size ( if known)
  std::vector<TMWAWPosition> m_objectsPosition;
  //! list of pictures id
  std::vector<int> m_objectsId;

  //! a smart ptr used to stored the list of compobj id->name
  shared_ptr<TMWAWOleParserInternal::CompObj> m_compObjIdName;

};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
