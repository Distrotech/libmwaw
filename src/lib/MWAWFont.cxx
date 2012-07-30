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

#include <sstream>

#include "libmwaw_internal.hxx"

#include "MWAWContentListener.hxx"
#include "MWAWFontConverter.hxx"

#include "MWAWFont.hxx"

std::string MWAWFont::getDebugString(shared_ptr<MWAWFontConverter> &converter) const
{
  std::stringstream o;
  o << std::dec;
  if (id() != -1) {
    if (converter)
      o << "nam='" << converter->getName(id()) << "',";
    else
      o << "id=" << id() << ",";
  }
  if (size() > 0) o << "sz=" << size() << ",";

  if (m_flags.isSet() && m_flags.get()) {
    o << "fl=";
    if (m_flags.get()&MWAW_BOLD_BIT) o << "b:";
    if (m_flags.get()&MWAW_ITALICS_BIT) o << "it:";
    if (m_flags.get()&MWAW_UNDERLINE_BIT) o << "underL:";
    if (m_flags.get()&MWAW_OVERLINE_BIT) o << "overL:";
    if (m_flags.get()&MWAW_EMBOSS_BIT) o << "emboss:";
    if (m_flags.get()&MWAW_SHADOW_BIT) o << "shadow:";
    if (m_flags.get()&MWAW_OUTLINE_BIT) o << "outline:";
    if (m_flags.get()&MWAW_DOUBLE_UNDERLINE_BIT) o << "2underL:";
    if (m_flags.get()&MWAW_STRIKEOUT_BIT) o << "strikeout:";
    if (m_flags.get()&MWAW_SMALL_CAPS_BIT) o << "smallCaps:";
    if (m_flags.get()&MWAW_ALL_CAPS_BIT) o << "allCaps:";
    if (m_flags.get()&MWAW_HIDDEN_BIT) o << "hidden:";
    if (m_flags.get()&MWAW_SMALL_PRINT_BIT) o << "consended:";
    if (m_flags.get()&MWAW_LARGE_BIT) o << "extended:";
    if ((m_flags.get()&MWAW_SUPERSCRIPT_BIT) || (m_flags.get()&MWAW_SUPERSCRIPT100_BIT))
      o << "superS:";
    if ((m_flags.get()&MWAW_SUBSCRIPT_BIT) || (m_flags.get()&MWAW_SUBSCRIPT100_BIT))
      o << "subS:";
    o << ",";
  }

  if (hasColor()) {
    o << "col=(" << std::hex;
    for (int i = 0; i < 3; i++) o << int(m_color.get()[i]) << ",";
    o << std::dec << "),";
  }
  return o.str();
}

void MWAWFont::sendTo(MWAWContentListener *listener, shared_ptr<MWAWFontConverter> &convert, MWAWFont &actualFont) const
{
  if (listener == 0L) return;

  std::string fName;
  int dSize = 0;
  int newSize = size();

  if (id() != -1) {
    actualFont.setId(id());
    convert->getOdtInfo(actualFont.id(), fName, dSize);
    listener->setTextFont(fName.c_str());
    // if no size reset to default
    if (newSize == -1) newSize = 12;
  }

  if (newSize > 0) {
    actualFont.setSize(newSize);
    dSize = 0;
    convert->getOdtInfo(actualFont.id(), fName, dSize);
    listener->setFontSize(uint16_t(actualFont.size()+dSize));
  }

  actualFont.setFlags(flags());
  listener->setFontAttributes(actualFont.flags());
  actualFont.setColor(m_color.get());
  uint32_t col = uint32_t(((m_color.get()[0]&0xFF)<<16) | ((m_color.get()[1]&0xFF)<<8) | (m_color.get()[2]&0xFF));
  listener->setTextColor(col);
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
