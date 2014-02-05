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

#include <librevenge/librevenge.h>
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
  State() : m_encoder()
  {
  }
  //! the encoder
  MWAWPropertyHandlerEncoder m_encoder;
};

}

MWAWGraphicInterface::MWAWGraphicInterface() : m_state(new MWAWGraphicInterfaceInternal::State)
{
}

MWAWGraphicInterface::~MWAWGraphicInterface()
{
}

bool MWAWGraphicInterface::getBinaryResult(librevenge::RVNGBinaryData &result, std::string &mimeType)
{
  if (!m_state->m_encoder.getData(result))
    return false;
  mimeType = "image/mwaw-odg";
  return true;
}

void MWAWGraphicInterface::startDocument(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("StartDocument", list);
}

void MWAWGraphicInterface::endDocument()
{
  m_state->m_encoder.insertElement("EndDocument");
}

void MWAWGraphicInterface::setDocumentMetaData(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("SetDocumentMetaData", list);
}

void MWAWGraphicInterface::startPage(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("StartPage", list);
}

void MWAWGraphicInterface::endPage()
{
  m_state->m_encoder.insertElement("EndPage");
}

void MWAWGraphicInterface::setStyle(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("SetStyle", list);
}

void MWAWGraphicInterface::startLayer(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("StartLayer", list);
}

void MWAWGraphicInterface::endLayer()
{
  m_state->m_encoder.insertElement("EndLayer");
}

void MWAWGraphicInterface::startEmbeddedGraphics(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("StartEmbeddedGraphics", list);
}

void MWAWGraphicInterface::endEmbeddedGraphics()
{
  m_state->m_encoder.insertElement("StartEmbeddedGraphics");
}

void MWAWGraphicInterface::drawRectangle(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DrawRectangle", list);
}

void MWAWGraphicInterface::drawEllipse(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DrawEllipse", list);
}

void MWAWGraphicInterface::drawPolygon(const ::librevenge::RVNGPropertyList &vertices)
{
  m_state->m_encoder.insertElement("DrawPolygon", vertices);
}

void MWAWGraphicInterface::drawPolyline(const ::librevenge::RVNGPropertyList &vertices)
{
  m_state->m_encoder.insertElement("DrawPolyline", vertices);
}

void MWAWGraphicInterface::drawPath(const ::librevenge::RVNGPropertyList &path)
{
  m_state->m_encoder.insertElement("DrawPath", path);
}

void MWAWGraphicInterface::drawGraphicObject(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DrawGraphicObject", list);
}

void MWAWGraphicInterface::startTextObject(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("StartTextObject", list);
}

void MWAWGraphicInterface::endTextObject()
{
  m_state->m_encoder.insertElement("EndTextObject");
}

void MWAWGraphicInterface::startTableObject(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("StartTableObject", list);
}

void MWAWGraphicInterface::endTableObject()
{
  m_state->m_encoder.insertElement("EndTableObject");
}

void MWAWGraphicInterface::openTableRow(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenTableRow", list);
}

void MWAWGraphicInterface::closeTableRow()
{
  m_state->m_encoder.insertElement("CloseTableRow");
}

void MWAWGraphicInterface::openTableCell(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenTableCell", list);
}

void MWAWGraphicInterface::closeTableCell()
{
  m_state->m_encoder.insertElement("CloseTableCell");
}

void MWAWGraphicInterface::insertCoveredTableCell(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("InsertCoveredTableCell", list);
}

void MWAWGraphicInterface::insertTab()
{
  m_state->m_encoder.insertElement("InsertTab");
}

void MWAWGraphicInterface::insertSpace()
{
  m_state->m_encoder.insertElement("InsertSpace");
}

void MWAWGraphicInterface::insertText(const librevenge::RVNGString &text)
{
  m_state->m_encoder.characters(text.cstr());
}

void MWAWGraphicInterface::insertLineBreak()
{
  m_state->m_encoder.insertElement("InsertLineBreak");
  insertText("\n");
}

void MWAWGraphicInterface::insertField(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("InsertField", list);
}

void MWAWGraphicInterface::openLink(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenLink", list);
}

void MWAWGraphicInterface::closeLink()
{
  m_state->m_encoder.insertElement("CloseLink");
}

void MWAWGraphicInterface::openOrderedListLevel(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenOrderedListLevel", list);
}

void MWAWGraphicInterface::openUnorderedListLevel(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenUnorderedListLevel", list);
}

void MWAWGraphicInterface::closeOrderedListLevel()
{
  m_state->m_encoder.insertElement("CloseOrderedListLevel");
}

void MWAWGraphicInterface::closeUnorderedListLevel()
{
  m_state->m_encoder.insertElement("CloseOrderedListLevel");
}

void MWAWGraphicInterface::openListElement(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenListElement", list);
}

void MWAWGraphicInterface::closeListElement()
{
  m_state->m_encoder.insertElement("CloseListElement");
}

void MWAWGraphicInterface::defineParagraphStyle(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DefineParagraphStyle", list);
}

void MWAWGraphicInterface::openParagraph(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenParagraph", list);
}

void MWAWGraphicInterface::closeParagraph()
{
  m_state->m_encoder.insertElement("CloseParagraph");
}

void MWAWGraphicInterface::defineCharacterStyle(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DefineCharacterStyle", list);
}

void MWAWGraphicInterface::openSpan(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenSpan", list);
}

void MWAWGraphicInterface::closeSpan()
{
  m_state->m_encoder.insertElement("CloseSpan");
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
