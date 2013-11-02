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

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"

#include "MWAWContentListener.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPosition.hxx"

#include "MWAWFont.hxx"

// line function

std::ostream &operator<<(std::ostream &o, MWAWFont::Line const &line)
{
  if (!line.isSet())
    return o;
  switch (line.m_style) {
  case MWAWFont::Line::Dot:
    o << "dotted";
    break;
  case MWAWFont::Line::LargeDot:
    o << "dotted[large]";
    break;
  case MWAWFont::Line::Dash:
    o << "dash";
    break;
  case MWAWFont::Line::Simple:
    o << "solid";
    break;
  case MWAWFont::Line::Wave:
    o << "wave";
    break;
  case MWAWFont::Line::None:
  default:
    break;
  }
  switch(line.m_type) {
  case MWAWFont::Line::Double:
    o << ":double";
    break;
  case MWAWFont::Line::Triple:
    o << ":triple";
    break;
  case MWAWFont::Line::Single:
  default:
    break;
  }
  if (line.m_word) o << ":byword";
  if (line.m_width < 1.0 || line.m_width > 1.0)
    o << ":w=" << line.m_width ;
  if (line.m_color.isSet())
    o << ":col=" << line.m_color.get();
  return o;
}

void MWAWFont::Line::addTo(RVNGPropertyList &propList, std::string const &type) const
{
  if (!isSet()) return;

  std::stringstream s;
  s << "style:text-" << type << "-type";
  propList.insert(s.str().c_str(), (m_type==Single) ? "single" : "double");

  if (m_word) {
    s.str("");
    s << "style:text-" << type << "-mode";
    propList.insert(s.str().c_str(), "skip-white-space");
  }

  s.str("");
  s << "style:text-" << type << "-style";
  switch(m_style) {
  case Dot:
  case LargeDot:
    propList.insert(s.str().c_str(), "dotted");
    break;
  case Dash:
    propList.insert(s.str().c_str(), "dash");
    break;
  case Simple:
    propList.insert(s.str().c_str(), "solid");
    break;
  case Wave:
    propList.insert(s.str().c_str(), "wave");
    break;
  case None:
  default:
    break;
  }
  if (m_color.isSet()) {
    s.str("");
    s << "style:text-" << type << "-color";
    propList.insert(s.str().c_str(), m_color.get().str().c_str());
  }
  //normal, bold, thin, dash, medium, and thick
  s.str("");
  s << "style:text-" << type << "-width";
  if (m_width <= 0.6)
    propList.insert(s.str().c_str(), "thin");
  else if (m_width >= 1.5)
    propList.insert(s.str().c_str(), "thick");
}

// script function

std::string MWAWFont::Script::str(float fSize) const
{
  if (!isSet() || ((m_delta<=0&&m_delta>=0) && m_scale==100))
    return "";
  std::stringstream o;
  if (m_deltaUnit == RVNG_GENERIC) {
    MWAW_DEBUG_MSG(("MWAWFont::Script::str: can not be called with generic position\n"));
    return "";
  }
  float delta = m_delta;
  if (m_deltaUnit != RVNG_PERCENT) {
    // first transform in point
    if (m_deltaUnit != RVNG_POINT)
      delta=MWAWPosition::getScaleFactor(m_deltaUnit, RVNG_POINT)*delta;
    // now transform in percent
    if (fSize<=0) {
      static bool first=true;
      if (first) {
        MWAW_DEBUG_MSG(("MWAWFont::Script::str: can not be find font size (supposed 12pt)\n"));
        first = false;
      }
      fSize=12;
    }
    delta=100.f*delta/fSize;
    if (delta > 100) delta = 100;
    else if (delta < -100) delta = -100;
  }
  o << delta << "% " << m_scale << "%";
  return o.str();
}

// font function

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
  if (m_deltaSpacing.isSet()) {
    if (m_deltaSpacing.get() > 0)
      o << "extended=" << m_deltaSpacing.get() << "pt,";
    else if (m_deltaSpacing.get() < 0)
      o << "condensed=" << -m_deltaSpacing.get() << "pt,";
  }
  if (m_texteWidthScaling.isSet())
    o << "scaling[width]=" <<  m_texteWidthScaling.get()*100.f << "%,";
  if (m_scriptPosition.isSet() && m_scriptPosition.get().isSet())
    o << "script=" << m_scriptPosition.get().str(size()) << ",";
  if (m_flags.isSet() && m_flags.get()) {
    o << "fl=";
    uint32_t flag = m_flags.get();
    if (flag&boldBit) o << "b:";
    if (flag&italicBit) o << "it:";
    if (flag&embossBit) o << "emboss:";
    if (flag&shadowBit) o << "shadow:";
    if (flag&outlineBit) o << "outline:";
    if (flag&smallCapsBit) o << "smallCaps:";
    if (flag&allCapsBit) o << "allCaps:";
    if (flag&lowercaseBit) o << "lowercase:";
    if (flag&hiddenBit) o << "hidden:";
    if (flag&reverseVideoBit) o << "reverseVideo:";
    if (flag&blinkBit) o << "blink:";
    if (flag&boxedBit) o << "box:";
    if (flag&boxedRoundedBit) o << "box[rounded]:";
    if (flag&reverseWritingBit) o << "reverseWriting:";
    o << ",";
  }
  if (m_overline.isSet() && m_overline->isSet())
    o << "overline=[" << m_overline.get() << "],";
  if (m_strikeoutline.isSet() && m_strikeoutline->isSet())
    o << "strikeOut=[" << m_strikeoutline.get() << "],";
  if (m_underline.isSet() && m_underline->isSet())
    o << "underline=[" << m_underline.get() << "],";
  if (hasColor())
    o << "col=" << m_color.get()<< ",";
  if (m_backgroundColor.isSet() && !m_backgroundColor.get().isWhite())
    o << "backCol=" << m_backgroundColor.get() << ",";
  if (m_language.isSet() && m_language.get().length())
    o << "lang=" << m_language.get() << ",";
  o << m_extra;
  return o.str();
}

void MWAWFont::addTo(RVNGPropertyList &pList, shared_ptr<MWAWFontConverter> convert) const
{
  int dSize = 0;
  std::string fName("");
  if (!convert) {
    MWAW_DEBUG_MSG(("MWAWFont::addTo: called without any font converter\n"));
  } else
    convert->getOdtInfo(id(), fName, dSize);
  if (fName.length())
    pList.insert("style:font-name", fName.c_str());
  float fSize = size()+float(dSize);
  pList.insert("fo:font-size", fSize, RVNG_POINT);

  uint32_t attributeBits = m_flags.get();
  if (attributeBits & italicBit)
    pList.insert("fo:font-style", "italic");
  if (attributeBits & boldBit)
    pList.insert("fo:font-weight", "bold");
  if (attributeBits & outlineBit)
    pList.insert("style:text-outline", "true");
  if (attributeBits & blinkBit)
    pList.insert("style:text-blinking", "true");
  if (attributeBits & shadowBit)
    pList.insert("fo:text-shadow", "1pt 1pt");
  if (attributeBits & hiddenBit)
    pList.insert("text:display", "none");
  if (attributeBits & lowercaseBit)
    pList.insert("fo:text-transform", "lowercase");
  if (attributeBits & allCapsBit)
    pList.insert("fo:text-transform", "uppercase");
  if (attributeBits & smallCapsBit)
    pList.insert("fo:font-variant", "small-caps");
  if (attributeBits & embossBit)
    pList.insert("style:font-relief", "embossed");
  else if (attributeBits & engraveBit)
    pList.insert("style:font-relief", "engraved");

  if (m_scriptPosition.isSet() && m_scriptPosition->isSet()) {
    std::string pos=m_scriptPosition->str(fSize);
    if (pos.length())
      pList.insert("style:text-position", pos.c_str());
  }

  if (m_overline.isSet() && m_overline->isSet())
    m_overline->addTo(pList, "overline");
  if (m_strikeoutline.isSet() && m_strikeoutline->isSet())
    m_strikeoutline->addTo(pList, "line-through");
  if (m_underline.isSet() && m_underline->isSet())
    m_underline->addTo(pList, "underline");
  if ((attributeBits & boxedBit) || (attributeBits & boxedRoundedBit)) {
    // do minimum: add a overline and a underline box
    Line simple(Line::Simple);
    if (!m_overline.isSet() || !m_overline->isSet())
      simple.addTo(pList, "overline");
    if (!m_underline.isSet() || !m_underline->isSet())
      simple.addTo(pList, "underline");
  }
  if (m_deltaSpacing.isSet() && (m_deltaSpacing.get() < 0 || m_deltaSpacing.get()>0))
    pList.insert("fo:letter-spacing", m_deltaSpacing.get(), RVNG_POINT);
  if (m_texteWidthScaling.isSet() && m_texteWidthScaling.get() > 0.0 &&
      (m_texteWidthScaling.get()>1.0||m_texteWidthScaling.get()<1.0))
    pList.insert("style:text-scale", m_texteWidthScaling.get(), RVNG_PERCENT);
  if (attributeBits & reverseVideoBit) {
    pList.insert("fo:color", m_backgroundColor->str().c_str());
    pList.insert("fo:background-color", m_color->str().c_str());
  } else {
    pList.insert("fo:color", m_color->str().c_str());
    if (m_backgroundColor.isSet() && !m_backgroundColor->isWhite())
      pList.insert("fo:background-color", m_backgroundColor->str().c_str());
  }
  if (m_language.isSet()) {
    size_t len=m_language->length();
    std::string lang(m_language.get());
    std::string country("none");
    if (len > 3 && lang[2]=='_') {
      country=lang.substr(3);
      lang=m_language->substr(0,2);
    } else if (len==0)
      lang="none";
    pList.insert("fo:language", lang.c_str());
    pList.insert("fo:country", country.c_str());
  }
  if (attributeBits & reverseWritingBit) {
    static bool first = true;
    if (first) {
      first = false;
      MWAW_DEBUG_MSG(("MWAWFont::addTo: sorry, reverse writing is not umplemented\n"));
    }
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
