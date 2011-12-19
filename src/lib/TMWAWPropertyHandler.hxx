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
 * For further information visit http://libmwaw.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
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
class WPXString;

//! a generic property handler
class TMWAWPropertyHandler
{
public:
  //! constructor
  TMWAWPropertyHandler() {}
  //! destructor
  virtual ~TMWAWPropertyHandler() {}

  //! starts an element
  virtual void startElement(const char *psName, const WPXPropertyList &xPropList) = 0;
  //! ends an element
  virtual void endElement(const char *psName) = 0;
  //! writes a list of characters
  virtual void characters(WPXString const &sCharacters) = 0;

  //! checks a encoded WPXBinaryData created by TMWAWPropertyHandlerEncoder
  bool checkData(WPXBinaryData const &encoded);
  //! reads a encoded WPXBinaryData created by TMWAWPropertyHandlerEncoder
  bool readData(WPXBinaryData const &encoded);
};

/*! \brief write in WPXBinaryData a list of tags/and properties
 *
 * In order to be read by writerperfect, we must code document consisting in
 * tag and propertyList in an intermediar format:
 *  - [string:s]: an int length(s) follow by the length(s) characters of string s
 *  - [startElement:name proplist:prop]:
 *          char 'S', [string] name, int \#properties, 2\#prop*[string]
 *           (ie. \#prop sequence of ([string] key, [string] value) )
 *  - [endElement:name ]: char 'E',  [string] name
 *  - [characters:s ]: char 'T', [string] s
 *            - if len(s)==0, we write nothing
 *            - the string is written as is (ie. we do not escaped any characters).
*/
class TMWAWPropertyHandlerEncoder
{
public:
  //! constructor
  TMWAWPropertyHandlerEncoder();

  //! starts an element
  void startElement(const char *psName, const WPXPropertyList &xPropList);
  //! ends an element
  void endElement(const char *psName);
  //! writes a list of characters
  void characters(std::string const &sCharacters);
  //! retrieves the data
  bool getData(WPXBinaryData &data);

protected:
  //! adds a string : size and string
  void writeString(const char *name);

  //! adds an integer value in f
  void writeInteger(int val);

  //! the streamfile
  std::stringstream m_f;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
