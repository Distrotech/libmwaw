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

#ifndef MWAW_HEADER_H
#define MWAW_HEADER_H

#include <vector>

#include <libwpd-stream/libwpd-stream.h>
#include "libmwaw_internal.hxx"

#include "MWAWDocument.hxx"
#include "MWAWInputStream.hxx"

/** \brief a function used by MWAWDocument to store the version of document and the input
 *
 * This class is mainly used to maintain a symetry with the libwpd library */

class MWAWHeader
{
public:
  typedef enum MWAWDocument::DocumentType DocumentType;
  typedef enum MWAWDocument::DocumentKind DocumentKind;


  //! constructor given the input
  MWAWHeader(MWAWDocument::DocumentType type=MWAWDocument::UNKNOWN, int vers=0);
  //! destructor
  virtual ~MWAWHeader();

  /** tests the input file and returns a header if the file looks like a MWAW document.

  \note this check phase can only be partial ; ie. we only test the first bytes of the file and/or the existence of some oles. This explains that MWAWDocument implements a more complete test to recognize the difference Mac Files which share the same type of header...
  */
  static std::vector<MWAWHeader> constructHeader(MWAWInputStreamPtr input);

  //! resets the data
  void reset(MWAWDocument::DocumentType type, int vers,
             DocumentKind kind = MWAWDocument::K_TEXT) {
    m_docType = type;
    m_version = vers;
    m_docKind = kind;
  }

  //! returns the major version
  int getMajorVersion() const {
    return m_version;
  }
  //! sets the major version
  void setMajorVersion(int version) {
    m_version=version;
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
  /** the document version */
  int m_version;
  /** the document type */
  DocumentType m_docType;
  /** the document kind */
  DocumentKind m_docKind;
};

#endif /* MWAWHEADER_H */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
