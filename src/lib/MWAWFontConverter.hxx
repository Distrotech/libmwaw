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

/* This header contains code specific to a mac file :
 *     - a namespace used to convert Mac font characters in unicode
 */

#ifndef MWAW_FONT_CONVERTER
#  define MWAW_FONT_CONVERTER

#  include <assert.h>
#  include <string>
#  include <map>

#  include "libmwaw_tools.hxx"

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
  //! the constructor
  MWAWFontConverter();
  //! the destructor
  ~MWAWFontConverter();

  //! returns an unique id > 255, if unknown
  int getId(std::string const &name) const;
  //! returns empty string if unknown
  std::string getName(int macId) const;
  //! fixes the name corresponding to an id
  void setCorrespondance(int macId, std::string const &name);

  //
  // Odt data
  //

  /** final font name and a delta which can be used to change the size
  if no name is found, return "Times New Roman" */
  void getOdtInfo(int macId, std::string &name, int &deltaSize) const;

  /* converts a character in unicode
     \return -1 if the character is not transformed */
  int unicode(int macId, unsigned char c) const;

protected:
  //! the main manager
  mutable shared_ptr<MWAWFontConverterInternal::State> m_manager;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
