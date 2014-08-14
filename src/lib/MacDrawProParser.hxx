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
struct Shape;
struct State;

class SubDocument;
}

/** \brief the main class to read a MacDraw II file
 *
 */
class MacDrawProParser : public MWAWGraphicParser
{
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
  //! try to read the RSRC zones
  bool readRSRCZones();

  // Intermediate level

  //! try to read the print info zone
  bool readPrintInfo();
  //! try to the header info zone ( print info + some information about content + prefs ?)
  bool readHeaderInfo();
  //! try to the style zone
  bool readStyles();
  //! try to the layer info zone
  bool readLayersInfo();
  //! try to the layer library correspondance zone
  bool readLayerLibraryCorrespondance();
  //! try to read the library name info zone
  bool readLibrariesInfo();
  //! find the objet's data position
  bool findDataObjectPosition();
  //! find the objet's text position
  bool findTextObjectPosition();

  //! try to read a structured zone header
  bool readStructuredHeaderZone(MWAWEntry const &entry, std::map<int, long> &idToDeltaPosMap);

  //
  // low level
  //

  //! try to read an object
  bool readObject();
  //! try to read an object data
  bool readObjectData(MacDrawProParserInternal::Shape &shape, int zId);
  //! try to read the font style ( last style in data fork )
  bool readFontStyles(MWAWEntry const &entry);

  // data fork or rsrc fork

  //! try to read the Arrow styles or the resource Aset:256
  bool readArrows(MWAWEntry const &entry, bool inRsrc=false);
  //! try to read the dash settings or the resource Dset:256
  bool readDashs(MWAWEntry const &entry, bool inRsrc=false);
  //! try to read the Pen styles or the resource PSet:256
  bool readPens(MWAWEntry const &entry, bool inRsrc=false);
  //! try to read the Ruler styles or the resource Drul:256
  bool readRulers(MWAWEntry const &entry, bool inRsrc=false);

  // rsrc

  //! try to read the Document Information resource Dinf:256
  bool readDocumentInfo(MWAWEntry const &entry);
  //! try to read the main Preferences resource Pref:256
  bool readPreferences(MWAWEntry const &entry);
  //! try to read the font name resources Fmtx:256 and Fnms:256
  bool readFontNames();
  //! try to read colors map Ctbl:256
  bool readColors(MWAWEntry const &entry);
  //! try to read the BW pattern bawP:256
  bool readBWPatterns(MWAWEntry const &entry);
  //! try to read the color pattern colP:256
  bool readColorPatterns(MWAWEntry const &entry);
  //! try to read the list of pattern patR:256: list of BW/Color patterns list which appear in the patterns tools
  bool readPatternsToolList(MWAWEntry const &entry);
  //! try to read the Ruler settings resource Rset:256 or Rst2:256
  bool readRulerSettings(MWAWEntry const &entry);
  //! read the view positions resource Dvws:256
  bool readViews(MWAWEntry const &entry);

  //! read the Dstl:256 resource (unknown content)
  bool readRSRCDstl(MWAWEntry const &entry);

  //! try to send a shape
  bool send(MacDrawProParserInternal::Shape const &shape);
  //! try to send a bitmap to the listener
  bool sendBitmap(MacDrawProParserInternal::Shape const &shape, MWAWPosition const &pos);
  //! try to send a text zone to the listener
  bool sendText(int zoneId);

  //
  // data
  //
  //! the state
  shared_ptr<MacDrawProParserInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
