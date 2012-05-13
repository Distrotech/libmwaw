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

#ifndef MWAW_PARSER_H
#define MWAW_PARSER_H

#include <ostream>
#include <string>
#include <vector>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWEntry.hxx"
#include "MWAWHeader.hxx"

class WPXDocumentInterface;

/** virtual class which defines the ancestor of all main zone parser
 *
 * \note this class is generally associated with a IMWAWTextParser
 */
class MWAWParser
{
public:
  //! virtual destructor
  virtual ~MWAWParser() {}
  //! virtual function used to parse the input
  virtual void parse(WPXDocumentInterface *documentInterface) = 0;

  //! returns the works version
  int version() const {
    return m_version;
  }
  //! sets the works version
  void setVersion(int vers) {
    m_version = vers;
  }

protected:
  //! constructor (protected
  MWAWParser(MWAWInputStreamPtr input, MWAWHeader *header):
    m_version (header->getMajorVersion()), m_asciiFile(input),
    m_input(input), m_header(header) {}

  //! returns the header
  MWAWHeader *getHeader() {
    return m_header;
  }
  //! returns the actual input
  MWAWInputStreamPtr &getInput() {
    return m_input;
  }
  //! reset the input and all stored data
  void resetInput(MWAWInputStreamPtr &ip) {
    m_input = ip;
    m_asciiFile.reset();
    m_asciiFile.setStream(ip);
  }

  //! the actual version
  uint8_t m_version;

  //! a DebugFile used to write what we recognize when we parse the document
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

  //! the debug file
  libmwaw::DebugFile m_asciiFile;

private:
  //! private copy constructor: forbidden
  MWAWParser(const MWAWParser &);
  //! private operator=: forbidden
  MWAWParser &operator=(const MWAWParser &);

  //! the input
  MWAWInputStreamPtr m_input;
  //! the header
  MWAWHeader *m_header;
};


#endif /* MWAWPARSER_H */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
