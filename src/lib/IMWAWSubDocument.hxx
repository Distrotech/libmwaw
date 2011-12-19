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

#ifndef IMWAW_SUBDOCUMENT_H
#define IMWAW_SUBDOCUMENT_H

#include "DMWAWSubDocument.hxx"

#include "TMWAWInputStream.hxx"

#include "libmwaw_internal.hxx"
#include "IMWAWEntry.hxx"

class IMWAWContentListener;
typedef shared_ptr<IMWAWContentListener> IMWAWContentListenerPtr;

class IMWAWParser;

/** abstract class used to store a subdocument (with a comparison function) */
class IMWAWSubDocument : public DMWAWSubDocument
{
public:
  //! constructor from parser, input stream and zone in the input
  IMWAWSubDocument(IMWAWParser *pars, TMWAWInputStreamPtr ip, IMWAWEntry const &z):
    DMWAWSubDocument(), m_parser(pars), m_input(ip), m_zone(z) {}
  //! copy constructor
  IMWAWSubDocument(IMWAWSubDocument const &doc) : DMWAWSubDocument(), m_parser(0), m_input(), m_zone() {
    *this = doc;
  }
  //! copy operator
  IMWAWSubDocument &operator=(IMWAWSubDocument const &doc) {
    if (&doc != this) {
      m_parser = doc.m_parser;
      m_input = doc.m_input;
      m_zone = doc.m_zone;
    }
    return *this;
  }
  //! virtual destructor
  virtual ~IMWAWSubDocument() {}

  //! comparison operator!=
  virtual bool operator!=(IMWAWSubDocument const &doc) const {
    if (doc.m_parser != m_parser) return true;
    if (doc.m_input.get() != m_input.get()) return true;
    if (doc.m_zone != m_zone) return true;
    return false;
  }
  //! comparison operator==
  virtual bool operator==(IMWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  /** virtual parse function
   *
   * this function is called to parse the subdocument */
  virtual void parse(IMWAWContentListenerPtr &listener, DMWAWSubDocumentType subDocumentType) = 0;

protected:
  //! the main zone parser
  IMWAWParser *m_parser;
  //! the input
  TMWAWInputStreamPtr m_input;
  //! if valid the zone to parse
  IMWAWEntry m_zone;
};

//! a smartptr of IMWAWSubDocument
typedef shared_ptr<IMWAWSubDocument> IMWAWSubDocumentPtr;

#endif /* MWAWSUBDOCUMENT_H */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
