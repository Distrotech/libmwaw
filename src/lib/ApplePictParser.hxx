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

#ifndef APPLEPICT_PARSER
#  define APPLEPICT_PARSER

#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace ApplePictParserInternal
{
struct Bitmap;
struct Pixmap;
struct Region;
struct State;

class SubDocument;
}

/** \brief the main class to read a ApplePict file
 *
 */
class ApplePictParser : public MWAWGraphicParser
{
  friend class ApplePictParserInternal::SubDocument;
public:
  /** the drawing method: frame, paint, ... */
  enum DrawingMethod {
    D_FRAME, D_PAINT, D_ERASE, D_INVERT, D_FILL,
    D_TEXT, D_UNDEFINED
  };

  //! constructor
  ApplePictParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~ApplePictParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  //! the main parse function
  void parse(librevenge::RVNGDrawingInterface *documentInterface);

protected:
  //! try to read a zone
  bool readZone();

  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGDrawingInterface *documentInterface);

protected:
  //! finds the different objects zones
  bool createZones();

  //
  // low level
  //

  //! draw a line from current position to position
  void drawLine(MWAWVec2i const &pt);
  //! read the current rectangle and draw it
  bool readAndDrawRectangle(DrawingMethod method);
  //! draw the current rectangle
  void drawRectangle(DrawingMethod method);
  //! read the current round rectangle and draw it
  bool readAndDrawRoundRectangle(DrawingMethod method);
  //! draw the current round rectangle
  void drawRoundRectangle(DrawingMethod method);
  //! read the current circle and draw it
  bool readAndDrawCircle(DrawingMethod method);
  //! draw the current circle
  void drawCircle(DrawingMethod method);
  //! read the current pie and draw it
  bool readAndDrawPie(DrawingMethod method);
  //! draw the current circle
  void drawPie(DrawingMethod method, int startAngle, int dAngle);
  //! read the current polygon and draw it
  bool readAndDrawPolygon(DrawingMethod method);
  //! draw the current polygon
  void drawPolygon(DrawingMethod method);
  //! read the current text and draw it
  bool readAndDrawText(std::string &text);
  //! draw a current text
  void drawText(MWAWEntry const &entry);

  //! draw a bitmap
  void drawBitmap(ApplePictParserInternal::Bitmap const &bitmap);
  //! draw a pixmap
  void drawPixmap(ApplePictParserInternal::Pixmap const &pixmap);

  //! try to read a rgb color
  bool readRGBColor(MWAWColor &color);
  //! try to read a bw pattern
  bool readBWPattern(MWAWGraphicStyle::Pattern &pattern);
  //! try to read a color pattern
  bool readColorPattern(MWAWGraphicStyle::Pattern &pattern);
  //! try to read a region
  bool readRegion(ApplePictParserInternal::Region &region);
  //! read a bitmap
  bool readBitmap(ApplePictParserInternal::Bitmap &bitmap, bool isPacked, bool hasRgn);
  //! read a pixmap
  bool readPixmap(ApplePictParserInternal::Pixmap &pixmap, bool isPacked, bool haColorTable, bool hasRectMode, bool hasRgn);

  //! debug function to print a drawing method
  static std::string getDrawingName(DrawingMethod method)
  {
    switch (method) {
    case D_FRAME:
      return "frame";
    case D_PAINT:
      return "paint";
    case D_ERASE:
      return "erase";
    case D_INVERT:
      return "invert";
    case D_FILL:
      return "fill";
    case D_TEXT:
      return "text";
    case D_UNDEFINED:
    default:
      break;
    }
    return "";
  }
  //! debug function to print a mode name
  static std::string getModeName(int mode);
  //
  // data
  //
  //! the state
  shared_ptr<ApplePictParserInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
