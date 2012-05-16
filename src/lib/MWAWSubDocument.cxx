/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2005 Fridrich Strba (fridrich.strba@bluewin.ch)
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
 * For further information visit http://libwpd.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#include "MWAWSubDocument.hxx"

#include "MWAWInputStream.hxx"

MWAWSubDocument::MWAWSubDocument(MWAWParser *pars, MWAWInputStreamPtr ip, MWAWEntry const &z):
  m_parser(pars), m_input(ip), m_zone(z)
{
}

MWAWSubDocument::MWAWSubDocument(MWAWSubDocument const &doc) : m_parser(0), m_input(), m_zone()
{
  *this = doc;
}

MWAWSubDocument::~MWAWSubDocument()
{
}

MWAWSubDocument &MWAWSubDocument::operator=(MWAWSubDocument const &doc)
{
  if (&doc != this) {
    m_parser = doc.m_parser;
    m_input = doc.m_input;
    m_zone = doc.m_zone;
  }
  return *this;
}

bool MWAWSubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (doc.m_parser != m_parser) return true;
  if (doc.m_input.get() != m_input.get()) return true;
  if (doc.m_zone != m_zone) return true;
  return false;
}

bool MWAWSubDocument::operator!=(shared_ptr<MWAWSubDocument> const &doc) const
{
  if (!doc) return true;
  return operator!=(*doc.get());
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:

