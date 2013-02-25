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

/* This header contains code specific to a mac file :
 *     - a namespace used to convert Mac font characters in unicode
 */

#ifndef MWAW_FONT_CONVERTER
#  define MWAW_FONT_CONVERTER

#  include <assert.h>
#  include <string>
#  include <map>

#  include "libmwaw_internal.hxx"

class MWAWInputStream;
typedef shared_ptr<MWAWInputStream> MWAWInputStreamPtr;

namespace MWAWFontConverterInternal
{
class State;
}

/*! \brief a namespace used to convert Mac font characters in unicode
 *
 * On old mac system, a font is either determined by a name or an unique id.
 * The standart font have a "fixed" id < 35, the user font can have different id,
 * (ie. when you installed a font with some id, if a font with the same id already
 * exists, a new id will generated for this font). Unfortunatly, Microsoft files seem
 * to only store the font id....
 *
 * A font also consists in 256 independent characters which are not normalised
 * (and a user can easily modify a characters of a font).
 */
class MWAWFontConverter
{
public:
  //! the character encoding type
  enum Encoding { E_DEFAULT, E_SJIS };

  //! the constructor
  MWAWFontConverter();
  //! the destructor
  ~MWAWFontConverter();

  //! returns an unique id > 255, if unknown
  int getId(std::string const &name, std::string family="") const;
  //! returns empty string if unknown
  std::string getName(int macId) const;
  //! fixes the name corresponding to an id
  void setCorrespondance(int macId, std::string const &name, std::string family="");

  //
  // Odt data
  //

  /** final font name and a delta which can be used to change the size
  if no name is found, return "Times New Roman" */
  void getOdtInfo(int macId, std::string &name, int &deltaSize) const;

  /** converts a character in unicode
     \return -1 if the character is not transformed */
  int unicode(int macId, unsigned char c) const;

  /** converts a character in unicode, if needed can read the next input caracter
     \return -1 if the character is not transformed */
  int unicode(int macId, unsigned char c, MWAWInputStreamPtr &input) const;

  /** converts a character in unicode, if needed can read the next input caracter in str
     \return -1 if the character is not transformed */
  int unicode(int macId, unsigned char c, unsigned char const *(&str), int len) const;
protected:
  /** check if a string is valid, if not, convert it to a valid string */
  static std::string getValidName(std::string const &name);

  //! the main manager
  mutable shared_ptr<MWAWFontConverterInternal::State> m_manager;
};

typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
