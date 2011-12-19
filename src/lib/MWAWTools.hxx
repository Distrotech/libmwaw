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

#ifndef MWAW_MWAW_TOOLS
#  define MWAW_MWAW_TOOLS

#include <string>

#include "libmwaw_libwpd.hxx"

#include "libmwaw_tools.hxx"
#include "TMWAWInputStream.hxx"

#include "MWAWStruct.hxx"

class WPXString;

namespace libmwaw_tools_mac
{
class Font;
}

/** Small function used to read a number, a string, a formula in a spreadsheet
    or a database */
namespace MWAWTools
{

//! a class to convert MWAW data in our data
class Convertissor
{
public:
  //! constructor
  Convertissor();
  //! destructor
  ~Convertissor();

  /////////////////////
  // Font
  /////////////////////

  //! returns empty string if unknown
  std::string getFontName(int macId) const;

  //! returns an unique id > 255, if unknown
  int getFontId(std::string const &name) const;

  //! returns an unicode character corresponding to a font
  int getUnicode(MWAWStruct::Font const &f, unsigned char c) const;

  /** converts a string in a WPXString ( UTF16 )

  \note this function removes all control character */
  WPXString getUnicode(MWAWStruct::Font const &f, std::string const &str) const;

  /** final font name and a delta which can be used to change the size
  if no name is found, return "Times New Roman" */
  void getFontOdtInfo(int macId, std::string &name, int &deltaSize) const;

  //! fixes the name corresponding to an id
  void setFontCorrespondance(int macId, std::string const &name);

  /** a debug funtion to return the font content */
#ifndef DEBUG
  std::string getFontDebugString(MWAWStruct::Font const &) const {
    return "";
  }
#else
  std::string getFontDebugString(MWAWStruct::Font const &ft) const;
#endif

  /** convert a mac font flags in a libMWAW flags */
  int convertFontFlags(int mFlags) {
    int flag = 0;
    if (mFlags & 0x1) flag |= DMWAW_BOLD_BIT;
    if (mFlags & 0x2) flag |= DMWAW_ITALICS_BIT;
    if (mFlags & 0x4) flag |= DMWAW_UNDERLINE_BIT;
    if (mFlags & 0x8) flag |= DMWAW_EMBOSS_BIT;
    if (mFlags & 0x10) flag |= DMWAW_SHADOW_BIT;
    if (mFlags & 0x20) flag |= DMWAW_SUPERSCRIPT_BIT;
    if (mFlags & 0x40) flag |= DMWAW_SUBSCRIPT_BIT;
    // & 0x80 : smaller
    return flag;
  }

  /////////////////////
  // Palette
  /////////////////////

  /** returns the mac MS indexed colors
  \note the MSWorks color palette, it seems used to store font color in Works 3.0 */
  std::vector<Vec3uc> const &getColor3Palette() const {
    initPalettes();
    return m_palette3;
  }

  /** returns the mac bitmap indexed colors*/
  std::vector<Vec3uc> const &getColor4Palette() const {
    initPalettes();
    return m_palette4;
  }

  /*           conversion data                */

protected:
  /** the font manager */
  shared_ptr<libmwaw_tools_mac::Font> m_fontManager;

  /** init the different palettes if there are not initialized */
  void initPalettes() const;

  //! the mac.v3 color palette
  mutable std::vector<Vec3uc>  m_palette3;

  //! the mac.v4 color palette
  mutable std::vector<Vec3uc>  m_palette4;
};
typedef shared_ptr<Convertissor> ConvertissorPtr;

/////////////////////
// Cell, ...
/////////////////////


/** reads a cell from actual position to end position-1 */
bool readString(TMWAWInputStreamPtr input, long endPos, std::string &res);

/** reads a number which can be preceded by a string */
bool readNumber(TMWAWInputStreamPtr input, long endPos, double &res, std::string &str);

/** reads a formula, returns the formula and the result's string*/
bool readFormula(TMWAWInputStreamPtr input, long endPos, int dim, std::string &str, std::string &res, double &value);
}
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
