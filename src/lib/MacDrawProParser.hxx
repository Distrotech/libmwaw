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

#ifndef MACDRAWPRO_PARSER
#  define MACDRAWPRO_PARSER

#include <map>
#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace MacDrawProParserInternal
{
struct Layer;
struct Library;
struct Shape;
struct State;

class SubDocument;
}

class MacDrawProStyleManager;

/** \brief the main class to read a MacDraw II file
 *
 */
class MacDrawProParser : public MWAWGraphicParser
{
  friend class MacDrawProStyleManager;
  friend class MacDrawProParserInternal::SubDocument;
public:
  //! constructor
  MacDrawProParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~MacDrawProParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(librevenge::RVNGDrawingInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGDrawingInterface *documentInterface);

protected:
  //! finds the different objects zones
  bool createZones();

  // Intermediate level

  //! tries to read the print info zone
  bool readPrintInfo();
  //! tries to the header info zone ( print info + some information about content + prefs ?)
  bool readHeaderInfo();
  //! tries to the layer info zone
  bool readLayersInfo();
  //! tries to the layer library correspondance zone
  bool readLayerLibraryCorrespondance();
  //! tries to read the library name info zone
  bool readLibrariesInfo();
  //! finds the objet's data/text positions
  bool findObjectPositions(bool dataZone);
  //! computes the layers and libraries bounding box
  bool computeLayersAndLibrariesBoundingBox();

  //! tries to read a structured zone header
  bool readStructuredHeaderZone(MWAWEntry const &entry, std::map<int, long> &idToDeltaPosMap);

  //
  // low level
  //

  //! tries to read an object and returns the object id (-1 if error )
  int readObject();
  //! tries to read an object data
  bool readObjectData(MacDrawProParserInternal::Shape &shape, int zId);
  //! tries to read the rotation
  bool readRotationInObjectData(MacDrawProParserInternal::Shape &shape, long endPos, std::string &extra);
  //! tries to update the basic geometric data
  bool updateGeometryShape(MacDrawProParserInternal::Shape &shape, float cornerWidth);
  //! tries to read a basic geometric object data ( line, rect, arc,... )
  bool readGeometryShapeData(MacDrawProParserInternal::Shape &shape, MWAWEntry const &entry);
  //! tries to read a bitmpa data ( bitmap,... )
  bool readBitmap(MacDrawProParserInternal::Shape &shape, MWAWEntry const &entry);

  // send functions

  //! tries to send a library
  bool send(MacDrawProParserInternal::Library const &library);
  //! tries to send a layer
  bool send(MacDrawProParserInternal::Layer const &layer);
  //! tries to send a shape
  bool send(MacDrawProParserInternal::Shape const &shape);
  //! tries to send a bitmap to the listener
  bool sendBitmap(MacDrawProParserInternal::Shape const &shape, MWAWPosition const &pos);
  //! tries to send a text zone to the listener
  bool sendText(int zoneId);
  //! tries to send a line label to the listener
  bool sendLabel(MWAWEntry const &entry);
  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // data
  //
  //! the state
  shared_ptr<MacDrawProParserInternal::State> m_state;

  //! the style manager state
  shared_ptr<MacDrawProStyleManager> m_styleManager;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
