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

#ifndef MWAW_SUB_DOCUMENT_HXX
#define MWAW_SUB_DOCUMENT_HXX

#include "libmwaw_internal.hxx"
#include "MWAWEntry.hxx"

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
  bool operator==(MWAWSubDocument const &doc) const
  {
    return !operator!=(doc);
  }
  //! comparison operator!=
  bool operator!=(shared_ptr<MWAWSubDocument> const &doc) const;
  //! comparison operator==
  bool operator==(shared_ptr<MWAWSubDocument> const &doc) const
  {
    return !operator!=(doc);
  }

  /** virtual parse function
   *
   * this function is called to parse the subdocument */
  virtual void parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType subDocumentType) = 0;

protected:
  //! the main zone parser
  MWAWParser *m_parser;
  //! the input
  shared_ptr<MWAWInputStream> m_input;
  //! if valid the zone to parse
  MWAWEntry m_zone;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
