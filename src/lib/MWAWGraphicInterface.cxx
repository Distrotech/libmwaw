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
 * Copyright (C) 2006 Ariya Hidayat (ariya@kde.org)
 * Copyright (C) 2004 Marc Oude Kotte (marc@solcon.nl)
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

#include <string.h>

#include <map>
#include <sstream>
#include <string>

#include <libwpd/libwpd.h>
#include <libmwaw/libmwaw.hxx>

#include "libmwaw_internal.hxx"

#include "MWAWPropertyHandler.hxx"

#include "MWAWGraphicInterface.hxx"

//! a name space used to define internal data of MWAWGraphicInterface
namespace MWAWGraphicInterfaceInternal
{
//! the state of a MWAWGraphicInterface
struct State {
  //! constructor
  State() : m_encoder(), m_listIdToPropertyMap() {
  }
  //! try to retrieve a list element property
  bool retrieveListElement(int id, int level, WPXPropertyList &list) const;
  //! add a list definition in the list property map
  void addListElement(WPXPropertyList const &list);
  //! the encoder
  MWAWPropertyHandlerEncoder m_encoder;
  //! a multimap list id to property list item map
  std::multimap<int, WPXPropertyList> m_listIdToPropertyMap;
};

void State::addListElement(WPXPropertyList const &list)
{
  if (!list["libwpd:id"] || !list["libwpd:level"]) {
    MWAW_DEBUG_MSG(("MWAWGraphicInterfaceInternal::addListElement: can not find the id or the level\n"));
    return;
  }
  int id=list["libwpd:id"]->getInt();
  int level=list["libwpd:level"]->getInt();
  std::multimap<int, WPXPropertyList>::iterator it=m_listIdToPropertyMap.lower_bound(id);
  while (it!=m_listIdToPropertyMap.end() && it->first == id) {
    if (it->second["libwpd:level"]->getInt()==level) {
      m_listIdToPropertyMap.erase(it);
      break;
    }
    ++it;
  }
  m_listIdToPropertyMap.insert(std::multimap<int, WPXPropertyList>::value_type(id,list));
}

bool State::retrieveListElement(int id, int level, WPXPropertyList &list) const
{
  std::multimap<int, WPXPropertyList>::const_iterator it=m_listIdToPropertyMap.lower_bound(id);
  while (it!=m_listIdToPropertyMap.end() && it->first == id) {
    if (it->second["libwpd:level"]->getInt()==level) {
      list = it->second;
      return true;
    }
    ++it;
  }
  MWAW_DEBUG_MSG(("MWAWGraphicInterfaceInternal::retrieveListElement: can not find the id=%d or the level=%d\n", id, level));
  return false;
}

}

MWAWGraphicInterface::MWAWGraphicInterface() : m_state(new MWAWGraphicInterfaceInternal::State)
{
}

MWAWGraphicInterface::~MWAWGraphicInterface()
{
}

bool MWAWGraphicInterface::getBinaryResult(WPXBinaryData &result, std::string &mimeType)
{
  if (!m_state->m_encoder.getData(result))
    return false;
  mimeType = "image/mwaw-odg";
  return true;
}

void MWAWGraphicInterface::startDocument(const ::WPXPropertyList &list)
{
  m_state->m_encoder.startElement("Graphics", list);
}

void MWAWGraphicInterface::endDocument()
{
  m_state->m_encoder.endElement("Graphics");
}

void MWAWGraphicInterface::setDocumentMetaData(const WPXPropertyList &)
{
}

void MWAWGraphicInterface::startPage(const ::WPXPropertyList &)
{
}

void MWAWGraphicInterface::endPage()
{
}

void MWAWGraphicInterface::setStyle(const ::WPXPropertyList &list, const ::WPXPropertyListVector &gradient)
{
  m_state->m_encoder.startElement("SetStyle", list, gradient);
  m_state->m_encoder.endElement("SetStyle");
}

void MWAWGraphicInterface::startLayer(const ::WPXPropertyList &list)
{
  m_state->m_encoder.startElement("Layer", list);
}

void MWAWGraphicInterface::endLayer()
{
  m_state->m_encoder.endElement("Layer");
}

void MWAWGraphicInterface::startEmbeddedGraphics(const ::WPXPropertyList &list)
{
  m_state->m_encoder.startElement("EmbeddedGraphics", list);
}

void MWAWGraphicInterface::endEmbeddedGraphics()
{
  m_state->m_encoder.endElement("EmbeddedGraphics");
}

void MWAWGraphicInterface::drawRectangle(const ::WPXPropertyList &list)
{
  m_state->m_encoder.startElement("Rectangle", list);
  m_state->m_encoder.endElement("Rectangle");
}

void MWAWGraphicInterface::drawEllipse(const ::WPXPropertyList &list)
{
  m_state->m_encoder.startElement("Ellipse", list);
  m_state->m_encoder.endElement("Ellipse");
}

void MWAWGraphicInterface::drawPolygon(const ::WPXPropertyListVector &vertices)
{
  m_state->m_encoder.startElement("Polygon", WPXPropertyList(), vertices);
  m_state->m_encoder.endElement("Polygon");
}

void MWAWGraphicInterface::drawPolyline(const ::WPXPropertyListVector &vertices)
{
  m_state->m_encoder.startElement("Polyline", WPXPropertyList(), vertices);
  m_state->m_encoder.endElement("Polyline");
}

void MWAWGraphicInterface::drawPath(const ::WPXPropertyListVector &path)
{
  m_state->m_encoder.startElement("Path", WPXPropertyList(), path);
  m_state->m_encoder.endElement("Path");
}

void MWAWGraphicInterface::drawGraphicObject(const ::WPXPropertyList &list, const ::WPXBinaryData &binaryData)
{
  m_state->m_encoder.startElement("GraphicObject", list, binaryData);
  m_state->m_encoder.endElement("GraphicObject");
}

void MWAWGraphicInterface::startTextObject(const ::WPXPropertyList &list, const ::WPXPropertyListVector &path)
{
  m_state->m_encoder.startElement("TextObject", list, path);
}

void MWAWGraphicInterface::endTextObject()
{
  m_state->m_encoder.endElement("TextObject");
}

void MWAWGraphicInterface::insertTab()
{
  insertText("\t");
}

void MWAWGraphicInterface::insertSpace()
{
  insertText(" ");
}

void MWAWGraphicInterface::insertText(const WPXString &text)
{
  m_state->m_encoder.characters(text.cstr());
}

void MWAWGraphicInterface::insertLineBreak()
{
  insertText("\n");
}

void MWAWGraphicInterface::insertField(const WPXString &type, const WPXPropertyList &/*list*/)
{
  if (type=="text:title")
    insertText("#TITLE#");
  else if (type=="text:page-number")
    insertText("#P#");
  else if (type=="text-page-count")
    insertText("#C#");
  else {
    MWAW_DEBUG_MSG(("MWAWGraphicInterface::insertField: find unknown type\n"));
  }
}

void MWAWGraphicInterface::defineOrderedListLevel(const WPXPropertyList &list)
{
  m_state->addListElement(list);
}

void MWAWGraphicInterface::defineUnorderedListLevel(const WPXPropertyList &list)
{
  m_state->addListElement(list);
}

void MWAWGraphicInterface::openListElement(const WPXPropertyList &list, const WPXPropertyListVector &tabStops)
{
  openParagraph(list, tabStops);
}

void MWAWGraphicInterface::closeListElement()
{
  closeParagraph();
}

void MWAWGraphicInterface::openParagraph(const WPXPropertyList &list, const WPXPropertyListVector &)
{
  m_state->m_encoder.startElement("TextLine", list);
}

void MWAWGraphicInterface::closeParagraph()
{
  m_state->m_encoder.endElement("TextLine");
}

void MWAWGraphicInterface::openSpan(const WPXPropertyList &list)
{
  m_state->m_encoder.startElement("TextSpan", list);
}

void MWAWGraphicInterface::closeSpan()
{
  m_state->m_encoder.endElement("TextSpan");
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
