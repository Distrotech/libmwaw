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

#include "MWAWGraphicEncoder.hxx"

//! a name space used to define internal data of MWAWGraphicEncoder
namespace MWAWGraphicEncoderInternal
{
//! the state of a MWAWGraphicEncoder
struct State {
  //! constructor
  State() : m_encoder()
  {
  }
  //! the encoder
  MWAWPropertyHandlerEncoder m_encoder;
};

}

MWAWGraphicEncoder::MWAWGraphicEncoder() : librevenge::RVNGDrawingInterface(), m_state(new MWAWGraphicEncoderInternal::State)
{
}

MWAWGraphicEncoder::~MWAWGraphicEncoder()
{
}

bool MWAWGraphicEncoder::getBinaryResult(librevenge::RVNGBinaryData &result, std::string &mimeType)
{
  if (!m_state->m_encoder.getData(result))
    return false;
  mimeType = "image/mwaw-odg";
  return true;
}

void MWAWGraphicEncoder::startDocument(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("StartDocument", list);
}

void MWAWGraphicEncoder::endDocument()
{
  m_state->m_encoder.insertElement("EndDocument");
}

void MWAWGraphicEncoder::setDocumentMetaData(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("SetDocumentMetaData", list);
}

void MWAWGraphicEncoder::startPage(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("StartPage", list);
}

void MWAWGraphicEncoder::endPage()
{
  m_state->m_encoder.insertElement("EndPage");
}

void MWAWGraphicEncoder::setStyle(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("SetStyle", list);
}

void MWAWGraphicEncoder::startLayer(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("StartLayer", list);
}

void MWAWGraphicEncoder::endLayer()
{
  m_state->m_encoder.insertElement("EndLayer");
}

void MWAWGraphicEncoder::startEmbeddedGraphics(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("StartEmbeddedGraphics", list);
}

void MWAWGraphicEncoder::endEmbeddedGraphics()
{
  m_state->m_encoder.insertElement("StartEmbeddedGraphics");
}

void MWAWGraphicEncoder::openGroup(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenGroup", list);
}

void MWAWGraphicEncoder::closeGroup()
{
  m_state->m_encoder.insertElement("CloseGroup");
}

void MWAWGraphicEncoder::drawRectangle(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DrawRectangle", list);
}

void MWAWGraphicEncoder::drawEllipse(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DrawEllipse", list);
}

void MWAWGraphicEncoder::drawPolygon(const ::librevenge::RVNGPropertyList &vertices)
{
  m_state->m_encoder.insertElement("DrawPolygon", vertices);
}

void MWAWGraphicEncoder::drawPolyline(const ::librevenge::RVNGPropertyList &vertices)
{
  m_state->m_encoder.insertElement("DrawPolyline", vertices);
}

void MWAWGraphicEncoder::drawPath(const ::librevenge::RVNGPropertyList &path)
{
  m_state->m_encoder.insertElement("DrawPath", path);
}

void MWAWGraphicEncoder::drawConnector(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DrawConnector", list);
}

void MWAWGraphicEncoder::drawGraphicObject(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DrawGraphicObject", list);
}

void MWAWGraphicEncoder::startTextObject(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("StartTextObject", list);
}

void MWAWGraphicEncoder::endTextObject()
{
  m_state->m_encoder.insertElement("EndTextObject");
}

void MWAWGraphicEncoder::startTableObject(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("StartTableObject", list);
}

void MWAWGraphicEncoder::endTableObject()
{
  m_state->m_encoder.insertElement("EndTableObject");
}

void MWAWGraphicEncoder::openTableRow(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenTableRow", list);
}

void MWAWGraphicEncoder::closeTableRow()
{
  m_state->m_encoder.insertElement("CloseTableRow");
}

void MWAWGraphicEncoder::openTableCell(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenTableCell", list);
}

void MWAWGraphicEncoder::closeTableCell()
{
  m_state->m_encoder.insertElement("CloseTableCell");
}

void MWAWGraphicEncoder::insertCoveredTableCell(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("InsertCoveredTableCell", list);
}

void MWAWGraphicEncoder::insertTab()
{
  m_state->m_encoder.insertElement("InsertTab");
}

void MWAWGraphicEncoder::insertSpace()
{
  m_state->m_encoder.insertElement("InsertSpace");
}

void MWAWGraphicEncoder::insertText(const librevenge::RVNGString &text)
{
  m_state->m_encoder.characters(text.cstr());
}

void MWAWGraphicEncoder::insertLineBreak()
{
  m_state->m_encoder.insertElement("InsertLineBreak");
}

void MWAWGraphicEncoder::insertField(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("InsertField", list);
}

void MWAWGraphicEncoder::openLink(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenLink", list);
}

void MWAWGraphicEncoder::closeLink()
{
  m_state->m_encoder.insertElement("CloseLink");
}

void MWAWGraphicEncoder::openOrderedListLevel(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenOrderedListLevel", list);
}

void MWAWGraphicEncoder::openUnorderedListLevel(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenUnorderedListLevel", list);
}

void MWAWGraphicEncoder::closeOrderedListLevel()
{
  m_state->m_encoder.insertElement("CloseOrderedListLevel");
}

void MWAWGraphicEncoder::closeUnorderedListLevel()
{
  m_state->m_encoder.insertElement("CloseOrderedListLevel");
}

void MWAWGraphicEncoder::openListElement(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenListElement", list);
}

void MWAWGraphicEncoder::closeListElement()
{
  m_state->m_encoder.insertElement("CloseListElement");
}

void MWAWGraphicEncoder::defineParagraphStyle(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DefineParagraphStyle", list);
}

void MWAWGraphicEncoder::openParagraph(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenParagraph", list);
}

void MWAWGraphicEncoder::closeParagraph()
{
  m_state->m_encoder.insertElement("CloseParagraph");
}

void MWAWGraphicEncoder::defineCharacterStyle(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DefineCharacterStyle", list);
}

void MWAWGraphicEncoder::openSpan(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenSpan", list);
}

void MWAWGraphicEncoder::closeSpan()
{
  m_state->m_encoder.insertElement("CloseSpan");
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
