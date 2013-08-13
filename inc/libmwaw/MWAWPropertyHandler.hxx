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

#ifndef MWAW_PROPERTY_HANDLER
#  define MWAW_PROPERTY_HANDLER

#  include <assert.h>
#  include <ostream>
#  include <sstream>
#  include <string>

class WPXBinaryData;
class WPXProperty;
class WPXPropertyList;
class WPXPropertyListVector;
class WPXString;

//! a generic property handler
class MWAWPropertyHandler
{
public:
  //! constructor
  MWAWPropertyHandler() {}
  //! destructor
  virtual ~MWAWPropertyHandler() {}

  //! starts an element
  virtual void startElement(const char *psName, const WPXPropertyList &xPropList) = 0;
  //! starts an element ( given a vector list )
  virtual void startElement(const char *psName, const WPXPropertyList &xPropList,
			    const WPXPropertyListVector &vect);
  //! ends an element
  virtual void endElement(const char *psName) = 0;
  //! inserts a simple element ( note: endElement must not be called )
  virtual void insertElement(const char *psName);
  //! writes a list of characters
  virtual void characters(WPXString const &sCharacters) = 0;

  //! checks a encoded WPXBinaryData created by MWAWPropertyHandlerEncoder
  bool checkData(WPXBinaryData const &encoded);
  //! reads a encoded WPXBinaryData created by MWAWPropertyHandlerEncoder
  bool readData(WPXBinaryData const &encoded);
};

/*! \brief write in WPXBinaryData a list of tags/and properties
 *
 * In order to be read by writerperfect, we must code document consisting in
 * tag and propertyList in an intermediar format:
 *  - [string:s]: an int length(s) follow by the length(s) characters of string s
 *  - [property:p]: a string value p.getStr()
 *  - [propertyList:pList]: a int: #pList followed by pList[0].key(),pList[0], pList[1].key(),pList[1], ...
 *  - [propertyListVector:v]: a int: #v followed by v[0], v[1], ...
 *
 *  - [startElement:name proplist:prop]: char 'S', [string] name, prop
 *  - [startElement2:name proplist:prop proplistvector:vector]:
 *          char 'V', [string] name, prop, vector
 *  - [insertElement:name]: char 'I', [string] name
 *  - [endElement:name ]: char 'E', [string] name
 *  - [characters:s ]: char 'T', [string] s
 *            - if len(s)==0, we write nothing
 *            - the string is written as is (ie. we do not escaped any characters).
*/
class MWAWPropertyHandlerEncoder
{
public:
  //! constructor
  MWAWPropertyHandlerEncoder();

  //! starts an element
  void startElement(const char *psName, const WPXPropertyList &xPropList);
  //! starts an element (added in libmwaw-0.1.12)
  void startElement(const char *psName, const WPXPropertyList &xPropList,
		    const WPXPropertyListVector &vect);
  //! ends an element
  void endElement(const char *psName);
  //! insert a simple element
  void insertElement(const char *psName);
  //! writes a list of characters
  void characters(std::string const &sCharacters);
  //! retrieves the data
  bool getData(WPXBinaryData &data);

protected:
  //! adds an integer value in f
  void writeInteger(int val);
  //! adds a string: size and string
  void writeString(const char *name);
  //! adds a property: a string key, a char:b,d,i,s,I,P,%,T type, a string corresponding to value
  void writeProperty(const char *key, const WPXProperty &prop);
  //! adds a property list: int \#prop, 2\#prop*[string]
  void writePropertyList(const WPXPropertyList &prop);

  //! the streamfile
  std::stringstream m_f;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
