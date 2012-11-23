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
    uint32_t flag = m_flags.get();
    if (flag&MWAW_BOLD_BIT) o << "b:";
    if (flag&MWAW_ITALICS_BIT) o << "it:";
    if (flag&MWAW_OVERLINE_BIT) o << "overL:";
    if (flag&MWAW_EMBOSS_BIT) o << "emboss:";
    if (flag&MWAW_SHADOW_BIT) o << "shadow:";
    if (flag&MWAW_OUTLINE_BIT) o << "outline:";
    if (flag&MWAW_STRIKEOUT_BIT) o << "strikeout:";
    if (flag&MWAW_SMALL_CAPS_BIT) o << "smallCaps:";
    if (flag&MWAW_ALL_CAPS_BIT) o << "allCaps:";
    if (flag&MWAW_HIDDEN_BIT) o << "hidden:";
    if (flag&MWAW_SMALL_PRINT_BIT) o << "consended:";
    if (flag&MWAW_LARGE_BIT) o << "extended:";
    if ((flag&MWAW_SUPERSCRIPT_BIT) || (flag&MWAW_SUPERSCRIPT100_BIT))
      o << "superS:";
    if ((flag&MWAW_SUBSCRIPT_BIT) || (flag&MWAW_SUBSCRIPT100_BIT))
      o << "subS:";
    if (flag&MWAW_REVERSEVIDEO_BIT) o << "reversed:";
    if (flag&MWAW_BLINK_BIT) o << "blink:";
    o << ",";
  }
  if (m_underline.isSet()) o << "underline=" << getUnderlineStyle() << ":";
  if (hasColor())
    o << "col=(" << std::hex << m_color.get() << std::dec << "),";
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
    listener->setFontName(fName.c_str());
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
  actualFont.setUnderlineStyle(getUnderlineStyle());
  listener->setFontUnderlineStyle(actualFont.getUnderlineStyle());
  actualFont.setColor(m_color.get());
  listener->setFontColor(m_color.get());
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
