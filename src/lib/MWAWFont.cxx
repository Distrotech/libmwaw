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
#include "MWAWPosition.hxx"

#include "MWAWFont.hxx"
std::string MWAWFont::Line::getPropertyValue(MWAWFont::Line::Style const &style)
{
  switch (style) {
  case Dot:
  case LargeDot:
    return "dotted";
  case Dash:
    return "dash";
  case Single:
    return "solid";
  case Double:
    return "double";
    break;
  case None:
  default:
    break;
  }
  return "";
}

std::string MWAWFont::Script::str(int fSize) const
{
  if (!isSet() || (m_delta==0 && m_scale==100))
    return "";
  std::stringstream o;
  if (m_deltaUnit == WPX_GENERIC) {
    MWAW_DEBUG_MSG(("MWAWFont::Script::str: can not be called with generic position\n"));
    return "";
  }
  int delta = m_delta;
  if (m_deltaUnit != WPX_PERCENT) {
    // first transform in point
    if (m_deltaUnit != WPX_POINT)
      delta=int(MWAWPosition::getScaleFactor(m_deltaUnit, WPX_POINT)*float(delta));
    // now transform in percent
    if (fSize<=0) {
      static bool first=true;
      if (first) {
        MWAW_DEBUG_MSG(("MWAWFont::Script::str: can not be find font size (supposed 12pt)\n"));
        first = false;
      }
      fSize=12;
    }
    delta=int(100.f*float(delta)/float(fSize));
    if (delta > 100) delta = 100;
    else if (delta < -100) delta = -100;
  }
  o << delta << "% " << m_scale << "%";
  return o.str();
}

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
  if (m_deltaSpacing.isSet() && m_deltaSpacing.get()) {
    if (m_deltaSpacing.get() > 0)
      o << "extended=" << m_deltaSpacing.get() << "pt,";
    else
      o << "condensed=" << -m_deltaSpacing.get() << "pt,";
  }
  if (m_scriptPosition.isSet() && m_scriptPosition.get().isSet())
    o << "script=" << m_scriptPosition.get().str(size()) << ",";
  if (m_flags.isSet() && m_flags.get()) {
    o << "fl=";
    uint32_t flag = m_flags.get();
    if (flag&boldBit) o << "b:";
    if (flag&italicBit) o << "it:";
    if (flag&overlineBit) o << "overL:";
    if (flag&embossBit) o << "emboss:";
    if (flag&shadowBit) o << "shadow:";
    if (flag&outlineBit) o << "outline:";
    if (flag&strikeOutBit) o << "strikeout:";
    if (flag&smallCapsBit) o << "smallCaps:";
    if (flag&allCapsBit) o << "allCaps:";
    if (flag&lowercaseBit) o << "lowercase:";
    if (flag&hiddenBit) o << "hidden:";
    if (flag&reverseVideoBit) o << "reversed:";
    if (flag&blinkBit) o << "blink:";
    o << ",";
  }
  if (m_underline.isSet()) o << "underline=" << Line::getPropertyValue(getUnderlineStyle()) << ":";
  if (hasColor())
    o << "col=" << m_color.get()<< "),";
  if (m_backgroundColor.isSet() && !m_backgroundColor.get().isWhite())
    o << "backCol=" << m_backgroundColor.get() << ",";
  o << m_extra;
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
  actualFont.setDeltaLetterSpacing(deltaLetterSpacing());
  listener->setFontDLSpacing(deltaLetterSpacing());
  actualFont.set(script());
  listener->setFontScript(script());

  actualFont.setFlags(flags());
  listener->setFontAttributes(actualFont.flags());
  actualFont.setUnderlineStyle(getUnderlineStyle());
  listener->setFontUnderlineStyle(actualFont.getUnderlineStyle());
  actualFont.setColor(m_color.get());
  listener->setFontColor(m_color.get());
  actualFont.setBackgroundColor(m_backgroundColor.get());
  listener->setFontBackgroundColor(m_backgroundColor.get());
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
