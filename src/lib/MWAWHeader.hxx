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

#ifndef MWAW_HEADER_H
#define MWAW_HEADER_H

#include <vector>

#include <libwpd-stream/libwpd-stream.h>

#include <libmwaw/libmwaw.hxx>
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
  MWAWHeader(MWAWDocument::DocumentType type=MWAWDocument::UNKNOWN, int vers=0,
             DocumentKind kind = MWAWDocument::K_TEXT);
  //! destructor
  virtual ~MWAWHeader();

  /** tests the input file and returns a header if the file looks like a MWAW document ( trying first to use the resource parsed if it exists )

  \note this check phase can only be partial ; ie. we only test the first bytes of the file and/or the existence of some oles. This explains that MWAWDocument implements a more complete test to recognize the difference Mac Files which share the same type of header...
  */
  static std::vector<MWAWHeader> constructHeader
  (MWAWInputStreamPtr input, shared_ptr<MWAWRSRCParser> rsrcParser);

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
