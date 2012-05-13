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

#ifndef MWAW_SUBDOCUMENT_HXX
#define MWAW_SUBDOCUMENT_HXX

#include "libmwaw_internal.hxx"
#include "MWAWEntry.hxx"

class MWAWInputStream;
typedef shared_ptr<MWAWInputStream> MWAWInputStreamPtr;
class MWAWContentListener;
typedef shared_ptr<MWAWContentListener> MWAWContentListenerPtr;

class MWAWParser;

/** abstract class used to store a subdocument (with a comparison function) */
class MWAWSubDocument
{
public:
  //! constructor from parser, input stream and zone in the input
  MWAWSubDocument(MWAWParser *pars, MWAWInputStreamPtr ip, MWAWEntry const &z);
  //! copy constructor
  MWAWSubDocument(MWAWSubDocument const &doc);
  //! copy operator
  MWAWSubDocument &operator=(MWAWSubDocument const &doc);
  //! virtual destructor
  virtual ~MWAWSubDocument();

  //! comparison operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! comparison operator==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  /** virtual parse function
   *
   * this function is called to parse the subdocument */
  virtual void parse(MWAWContentListenerPtr &listener, MWAWSubDocumentType subDocumentType) = 0;

protected:
  //! the main zone parser
  MWAWParser *m_parser;
  //! the input
  shared_ptr<MWAWInputStream> m_input;
  //! if valid the zone to parse
  MWAWEntry m_zone;
};

//! a smartptr of MWAWSubDocument
typedef shared_ptr<MWAWSubDocument> MWAWSubDocumentPtr;

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
