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

#include <librevenge/librevenge.h>
#include <libmwaw/libmwaw.hxx>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"

#include "MWAWGraphicDecoder.hxx"

void MWAWGraphicDecoder::insertElement(const char *psName)
{
  if (!m_output) return;
  if (!psName || !*psName) {
    MWAW_DEBUG_MSG(("MWAWGraphicDecoder::insertElement: called without any name\n"));
    return;
  }

  bool ok=true;
  switch (psName[0]) {
  case 'C':
    if (strcmp(psName,"CloseLink")==0)
      m_output->closeLink();
    else if (strcmp(psName,"CloseListElement")==0)
      m_output->closeListElement();
    else if (strcmp(psName,"CloseOrderedListLevel")==0)
      m_output->closeOrderedListLevel();
    else if (strcmp(psName,"CloseParagraph")==0)
      m_output->closeParagraph();
    else if (strcmp(psName,"CloseSpan")==0)
      m_output->closeSpan();
    else if (strcmp(psName,"CloseTableCell")==0)
      m_output->closeTableCell();
    else if (strcmp(psName,"CloseTableRow")==0)
      m_output->closeTableRow();
    else if (strcmp(psName,"CloseUnorderedListLevel")==0)
      m_output->closeUnorderedListLevel();
    else
      ok=false;
    break;
  case 'E':
    if (strcmp(psName,"EndDocument")==0)
      m_output->endDocument();
    else if (strcmp(psName,"EndPage")==0)
      m_output->endPage();
    else if (strcmp(psName,"EndLayer")==0)
      m_output->endLayer();
    else if (strcmp(psName,"EndEmbeddedGraphics")==0)
      m_output->endEmbeddedGraphics();
    else if (strcmp(psName,"EndTableObject")==0)
      m_output->endTableObject();
    else if (strcmp(psName,"EndTextObject")==0)
      m_output->endTextObject();
    else
      ok=false;
    break;
  case 'I':
    if (strcmp(psName,"InsertTab")==0)
      m_output->insertTab();
    else if (strcmp(psName,"InsertSpace")==0)
      m_output->insertSpace();
    else if (strcmp(psName,"InsertLineBreak")==0)
      m_output->insertLineBreak();
    else
      ok=false;
    break;
  default:
    ok=false;
    break;
  }
  if (!ok) {
    MWAW_DEBUG_MSG(("MWAWGraphicDecoder::insertElement: called with unexpected name %s\n", psName));
  }
}

void MWAWGraphicDecoder::insertElement(const char *psName, const librevenge::RVNGPropertyList &propList)
{
  if (!m_output) return;
  if (!psName || !*psName) {
    MWAW_DEBUG_MSG(("MWAWGraphicDecoder::insertElement: called without any name\n"));
    return;
  }

  bool ok=true;
  switch (psName[0]) {
  case 'D':
    if (strcmp(psName,"DefineCharacterStyle")==0)
      m_output->defineCharacterStyle(propList);
    else if (strcmp(psName,"DefineParagraphStyle")==0)
      m_output->defineParagraphStyle(propList);

    else if (strcmp(psName,"DrawEllipse")==0)
      m_output->drawEllipse(propList);
    else if (strcmp(psName,"DrawGraphicObject")==0)
      m_output->drawGraphicObject(propList);
    else if (strcmp(psName,"DrawPath")==0)
      m_output->drawPath(propList);
    else if (strcmp(psName,"DrawPolygon")==0)
      m_output->drawPolygon(propList);
    else if (strcmp(psName,"DrawPolyline")==0)
      m_output->drawPolyline(propList);
    else if (strcmp(psName,"DrawRectangle")==0)
      m_output->drawRectangle(propList);
    else
      ok=false;
    break;
  case 'I':
    if (strcmp(psName,"InsertCoveredTableCell")==0)
      m_output->insertCoveredTableCell(propList);
    else if (strcmp(psName,"InsertField")==0)
      m_output->insertField(propList);
    else
      ok=false;
    break;
  case 'O':
    if (strcmp(psName,"OpenLink")==0)
      m_output->openLink(propList);
    else if (strcmp(psName,"OpenListElement")==0)
      m_output->openListElement(propList);
    else if (strcmp(psName,"OpenOrderedListLevel")==0)
      m_output->openOrderedListLevel(propList);
    else if (strcmp(psName,"OpenParagraph")==0)
      m_output->openParagraph(propList);
    else if (strcmp(psName,"OpenSpan")==0)
      m_output->openSpan(propList);
    else if (strcmp(psName,"OpenTableCell")==0)
      m_output->openTableCell(propList);
    else if (strcmp(psName,"OpenTableRow")==0)
      m_output->openTableRow(propList);
    else if (strcmp(psName,"OpenUnorderedListLevel")==0)
      m_output->openUnorderedListLevel(propList);
    else
      ok=false;
    break;
  case 'S':
    if (strcmp(psName,"SetMetaData")==0)
      m_output->setDocumentMetaData(propList);
    else if (strcmp(psName,"SetStyle")==0)
      m_output->setStyle(propList);

    else if (strcmp(psName,"StartDocument")==0)
      m_output->startDocument(propList);
    else if (strcmp(psName,"StartEmbeddedGraphics")==0)
      m_output->startEmbeddedGraphics(propList);
    else if (strcmp(psName,"StartLayer")==0)
      m_output->startLayer(propList);
    else if (strcmp(psName,"StartPage")==0)
      m_output->startPage(propList);
    else if (strcmp(psName,"StartTableObject")==0)
      m_output->startTableObject(propList);
    else if (strcmp(psName,"StartTextObject")==0)
      m_output->startTextObject(propList);
    else
      ok=false;
    break;
  default:
    ok=false;
    break;
  }
  if (!ok) {
    MWAW_DEBUG_MSG(("MWAWGraphicDecoder::insertElement: called with unexpected name %s\n", psName));
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
