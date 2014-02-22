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

#ifndef __MWAW_GRAPHIC_ENCODER_HXX__
#define __MWAW_GRAPHIC_ENCODER_HXX__

#include <librevenge/librevenge.h>
#include <libmwaw_internal.hxx>

class MWAWPropertyHandlerEncoder;

namespace MWAWGraphicEncoderInternal
{
struct State;
}
/** main class used to define store librevenge::RVNGDrawingInterface
    lists of command in a librevenge::RVNGBinaryData. \see MWAWGraphicDecoder
    can be used to decode back the pictures...

	\note as this class implements the functions librevenge::RVNGDrawingInterface,
  the documentation is not duplicated..
*/
class MWAWGraphicEncoder : public librevenge::RVNGDrawingInterface
{
public:
  /// constructor
  MWAWGraphicEncoder();
  /// destructor
  ~MWAWGraphicEncoder();
  /// return the final graphic
  bool getBinaryResult(librevenge::RVNGBinaryData &result, std::string &mimeType);

  void startDocument(const ::librevenge::RVNGPropertyList &propList);
  void endDocument();

  void setDocumentMetaData(const librevenge::RVNGPropertyList &propList);
  void startPage(const ::librevenge::RVNGPropertyList &propList);
  void endPage();

  void setStyle(const ::librevenge::RVNGPropertyList &propList);
  void startLayer(const ::librevenge::RVNGPropertyList &propList);
  void endLayer();
  void startEmbeddedGraphics(const ::librevenge::RVNGPropertyList &propList);
  void endEmbeddedGraphics();

  void drawRectangle(const ::librevenge::RVNGPropertyList &propList);
  void drawEllipse(const ::librevenge::RVNGPropertyList &propList);
  void drawPolygon(const ::librevenge::RVNGPropertyList &vertices);
  void drawPolyline(const ::librevenge::RVNGPropertyList &vertices);
  void drawPath(const ::librevenge::RVNGPropertyList &path);

  void drawGraphicObject(const ::librevenge::RVNGPropertyList &propList);

  void startTextObject(const ::librevenge::RVNGPropertyList &propList);
  void endTextObject();

  void startTableObject(const librevenge::RVNGPropertyList &propList);
  void endTableObject();
  void openTableRow(const librevenge::RVNGPropertyList &propList);
  void closeTableRow();
  void openTableCell(const librevenge::RVNGPropertyList &propList);
  void closeTableCell();
  void insertCoveredTableCell(const librevenge::RVNGPropertyList &propList);

  void insertTab();
  void insertSpace();
  void insertText(const librevenge::RVNGString &text);
  void insertLineBreak();
  void insertField(const librevenge::RVNGPropertyList &propList);

  void openLink(const librevenge::RVNGPropertyList &propList);
  void closeLink();
  void openOrderedListLevel(const librevenge::RVNGPropertyList &propList);
  void openUnorderedListLevel(const librevenge::RVNGPropertyList &propList);
  void closeOrderedListLevel();
  void closeUnorderedListLevel();
  void openListElement(const librevenge::RVNGPropertyList &propList);
  void closeListElement();

  void defineParagraphStyle(const librevenge::RVNGPropertyList &propList);
  void openParagraph(const librevenge::RVNGPropertyList &propList);
  void closeParagraph();

  void defineCharacterStyle(const librevenge::RVNGPropertyList &propList);
  void openSpan(const librevenge::RVNGPropertyList &propList);
  void closeSpan();

protected:
  //! the actual state
  shared_ptr<MWAWGraphicEncoderInternal::State> m_state;
};

#endif

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
