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

#include "MWAWSpreadsheetEncoder.hxx"

//! a name space used to define internal data of MWAWSpreadsheetEncoder
namespace MWAWSpreadsheetEncoderInternal
{
//! the state of a MWAWSpreadsheetEncoder
struct State {
  //! constructor
  State() : m_encoder()
  {
  }
  //! the encoder
  MWAWPropertyHandlerEncoder m_encoder;
};

}

MWAWSpreadsheetEncoder::MWAWSpreadsheetEncoder() : librevenge::RVNGSpreadsheetInterface(), m_state(new MWAWSpreadsheetEncoderInternal::State)
{
}

MWAWSpreadsheetEncoder::~MWAWSpreadsheetEncoder()
{
}

bool MWAWSpreadsheetEncoder::getBinaryResult(librevenge::RVNGBinaryData &result, std::string &mimeType)
{
  if (!m_state->m_encoder.getData(result))
    return false;
  mimeType = "image/mwaw-ods";
  return true;
}

void MWAWSpreadsheetEncoder::setDocumentMetaData(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("SetDocumentMetaData", list);
}

void MWAWSpreadsheetEncoder::startDocument(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("StartDocument", list);
}

void MWAWSpreadsheetEncoder::endDocument()
{
  m_state->m_encoder.insertElement("EndDocument");
}

//
// page
//
void MWAWSpreadsheetEncoder::definePageStyle(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DefinePageStyle", list);
}

void MWAWSpreadsheetEncoder::openPageSpan(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenPageSpan", list);
}
void MWAWSpreadsheetEncoder::closePageSpan()
{
  m_state->m_encoder.insertElement("ClosePageSpan");
}

void MWAWSpreadsheetEncoder::openHeader(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenHeader", list);
}
void MWAWSpreadsheetEncoder::closeHeader()
{
  m_state->m_encoder.insertElement("CloseHeader");
}

void MWAWSpreadsheetEncoder::openFooter(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenFooter", list);
}
void MWAWSpreadsheetEncoder::closeFooter()
{
  m_state->m_encoder.insertElement("CloseFooter");
}

//
// spreadsheet
//
void MWAWSpreadsheetEncoder::defineSheetNumberingStyle(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DefineSheetNumberingStyle", list);
}
void MWAWSpreadsheetEncoder::openSheet(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenSheet", list);
}
void MWAWSpreadsheetEncoder::closeSheet()
{
  m_state->m_encoder.insertElement("CloseSheet");
}
void MWAWSpreadsheetEncoder::openSheetRow(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenSheetRow", list);
}

void MWAWSpreadsheetEncoder::closeSheetRow()
{
  m_state->m_encoder.insertElement("CloseSheetRow");
}

void MWAWSpreadsheetEncoder::openSheetCell(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenSheetCell", list);
}

void MWAWSpreadsheetEncoder::closeSheetCell()
{
  m_state->m_encoder.insertElement("CloseSheetCell");
}

//
// chart
//

void MWAWSpreadsheetEncoder::defineChartStyle(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DefineChartStyle", list);
}

void MWAWSpreadsheetEncoder::openChart(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenChart", list);
}

void MWAWSpreadsheetEncoder::closeChart()
{
  m_state->m_encoder.insertElement("CloseChart");
}

void MWAWSpreadsheetEncoder::openChartTextObject(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenChartTextObject", list);
}

void MWAWSpreadsheetEncoder::closeChartTextObject()
{
  m_state->m_encoder.insertElement("CloseChartTextObject");
}

void MWAWSpreadsheetEncoder::openChartPlotArea(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenChartPlotArea", list);
}

void MWAWSpreadsheetEncoder::closeChartPlotArea()
{
  m_state->m_encoder.insertElement("CloseChartPlotArea");
}

void MWAWSpreadsheetEncoder::insertChartAxis(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("InsertChartAxis", list);
}

void MWAWSpreadsheetEncoder::openChartSerie(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenChartSerie", list);
}

void MWAWSpreadsheetEncoder::closeChartSerie()
{
  m_state->m_encoder.insertElement("CloseChartSerie");
}


//
// para styles + character styles + link
//
void MWAWSpreadsheetEncoder::defineParagraphStyle(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DefineParagraphStyle", list);
}

void MWAWSpreadsheetEncoder::openParagraph(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenParagraph", list);
}

void MWAWSpreadsheetEncoder::closeParagraph()
{
  m_state->m_encoder.insertElement("CloseParagraph");
}

void MWAWSpreadsheetEncoder::defineCharacterStyle(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DefineCharacterStyle", list);
}

void MWAWSpreadsheetEncoder::openSpan(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenSpan", list);
}

void MWAWSpreadsheetEncoder::closeSpan()
{
  m_state->m_encoder.insertElement("CloseSpan");
}

void MWAWSpreadsheetEncoder::openLink(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenLink", list);
}

void MWAWSpreadsheetEncoder::closeLink()
{
  m_state->m_encoder.insertElement("CloseLink");
}

//
// section + add basic char
//
void MWAWSpreadsheetEncoder::defineSectionStyle(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DefineSectionStyle", list);
}

void MWAWSpreadsheetEncoder::openSection(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenSection", list);
}

void MWAWSpreadsheetEncoder::closeSection()
{
  m_state->m_encoder.insertElement("CloseSection");
}

void MWAWSpreadsheetEncoder::insertTab()
{
  m_state->m_encoder.insertElement("InsertTab");
}

void MWAWSpreadsheetEncoder::insertSpace()
{
  m_state->m_encoder.insertElement("InsertSpace");
}

void MWAWSpreadsheetEncoder::insertText(const librevenge::RVNGString &text)
{
  m_state->m_encoder.characters(text.cstr());
}

void MWAWSpreadsheetEncoder::insertLineBreak()
{
  m_state->m_encoder.insertElement("InsertLineBreak");
}

void MWAWSpreadsheetEncoder::insertField(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("InsertField", list);
}

//
// list
//
void MWAWSpreadsheetEncoder::openOrderedListLevel(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenOrderedListLevel", list);
}

void MWAWSpreadsheetEncoder::openUnorderedListLevel(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenUnorderedListLevel", list);
}

void MWAWSpreadsheetEncoder::closeOrderedListLevel()
{
  m_state->m_encoder.insertElement("CloseOrderedListLevel");
}

void MWAWSpreadsheetEncoder::closeUnorderedListLevel()
{
  m_state->m_encoder.insertElement("CloseOrderedListLevel");
}

void MWAWSpreadsheetEncoder::openListElement(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenListElement", list);
}

void MWAWSpreadsheetEncoder::closeListElement()
{
  m_state->m_encoder.insertElement("CloseListElement");
}

//
// footnote, comment, frame
//

void MWAWSpreadsheetEncoder::openFootnote(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenFootnote", list);
}

void MWAWSpreadsheetEncoder::closeFootnote()
{
  m_state->m_encoder.insertElement("CloseFootnote");
}

void MWAWSpreadsheetEncoder::openComment(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenComment", list);
}
void MWAWSpreadsheetEncoder::closeComment()
{
  m_state->m_encoder.insertElement("CloseComment");
}

void MWAWSpreadsheetEncoder::openFrame(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenFrame", list);
}
void MWAWSpreadsheetEncoder::closeFrame()
{
  m_state->m_encoder.insertElement("CloseFrame");
}
void MWAWSpreadsheetEncoder::insertBinaryObject(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("InsertBinaryObject", list);
}

//
// specific text/table
//
void MWAWSpreadsheetEncoder::openTextBox(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenTextBox", list);
}

void MWAWSpreadsheetEncoder::closeTextBox()
{
  m_state->m_encoder.insertElement("CloseTextBox");
}

void MWAWSpreadsheetEncoder::openTable(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenTable", list);
}
void MWAWSpreadsheetEncoder::closeTable()
{
  m_state->m_encoder.insertElement("CloseTable");
}
void MWAWSpreadsheetEncoder::openTableRow(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenTableRow", list);
}

void MWAWSpreadsheetEncoder::closeTableRow()
{
  m_state->m_encoder.insertElement("CloseTableRow");
}

void MWAWSpreadsheetEncoder::openTableCell(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenTableCell", list);
}

void MWAWSpreadsheetEncoder::closeTableCell()
{
  m_state->m_encoder.insertElement("CloseTableCell");
}

void MWAWSpreadsheetEncoder::insertCoveredTableCell(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("InsertCoveredTableCell", list);
}

//
// simple Graphic
//

void MWAWSpreadsheetEncoder::openGroup(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("OpenGroup", list);
}

void MWAWSpreadsheetEncoder::closeGroup()
{
  m_state->m_encoder.insertElement("CloseGroup");
}

void MWAWSpreadsheetEncoder::defineGraphicStyle(const librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DefineGraphicStyle", list);
}

void MWAWSpreadsheetEncoder::drawRectangle(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DrawRectangle", list);
}

void MWAWSpreadsheetEncoder::drawEllipse(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("DrawEllipse", list);
}

void MWAWSpreadsheetEncoder::drawPolygon(const ::librevenge::RVNGPropertyList &vertices)
{
  m_state->m_encoder.insertElement("DrawPolygon", vertices);
}

void MWAWSpreadsheetEncoder::drawPolyline(const ::librevenge::RVNGPropertyList &vertices)
{
  m_state->m_encoder.insertElement("DrawPolyline", vertices);
}

void MWAWSpreadsheetEncoder::drawPath(const ::librevenge::RVNGPropertyList &path)
{
  m_state->m_encoder.insertElement("DrawPath", path);
}

//
// equation
//
void MWAWSpreadsheetEncoder::insertEquation(const ::librevenge::RVNGPropertyList &list)
{
  m_state->m_encoder.insertElement("InsertEquation", list);
}


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
