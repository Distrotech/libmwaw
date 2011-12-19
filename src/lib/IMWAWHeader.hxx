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

#ifndef IMWAW_HEADER_H
#define IMWAW_HEADER_H

#include <libwpd-stream/libwpd-stream.h>
#include "libmwaw_internal.hxx"

#include "IMWAWDocument.hxx"
#include "TMWAWInputStream.hxx"

/** \brief a function used by IMWAWDocument to store the version of document and the input
 *
 * This class is mainly used to maintain a symetry with the libwpd library */

class IMWAWHeader
{
public:
  typedef enum IMWAWDocument::DocumentType DocumentType;
  typedef enum IMWAWDocument::DocumentKind DocumentKind;

  //! constructor given the input and the document version \sa m_majorVersion
  IMWAWHeader(TMWAWInputStreamPtr input, int majorVersion);
  //! destructor
  virtual ~IMWAWHeader();

  /** tests the input file and returns a header if the file looks like a MWAW document.

  \note this check phase can only be partial ; ie. we only test the first bytes of the file and/or the existence of some oles. This explains that IMWAWDocument implements a more complete test to recognize the difference Mac Files which share the same type of header...
  \note TODO: moves this function in IMWAWDocument
  */
  static IMWAWHeader * constructHeader(TMWAWInputStreamPtr input);

  //! returns the actual input
  TMWAWInputStreamPtr getInput() const {
    return m_input;
  }
  //! returns the major version
  int getMajorVersion() const {
    return m_majorVersion;
  }
  //! sets the major version
  void setMajorVersion(int version) {
    m_majorVersion=version;
  }

  //! returns the document type
  DocumentType getType() const {
    return m_docType;
  }
  //! sets the document type
  void setType(DocumentType type) {
    m_docType = type;
  }

  //! returns the document kind
  DocumentKind getKind() const {
    return m_docKind;
  }
  //! sets the document kind
  void setKind(DocumentKind kind) {
    m_docKind = kind;
  }

private:
  //! internal constructor: forbidden
  IMWAWHeader(const IMWAWHeader&);
  //! internal copy operator: forbidden
  IMWAWHeader& operator=(const IMWAWHeader&);

  //! input
  TMWAWInputStreamPtr m_input;
  /** the document version
   *
   * - 2-8: means a Pc document version 2-8
   * - 100: potential Mac Document ( partial check)
   * - 101-104: means a Mac document version 1-4
   */
  int m_majorVersion;
  /** the document type */
  DocumentType m_docType;
  /** the document kind */
  DocumentKind m_docKind;
};

#endif /* MWAWHEADER_H */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
