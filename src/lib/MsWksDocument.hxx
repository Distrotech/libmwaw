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

#ifndef MS_WKS_DOCUMENT
#  define MS_WKS_DOCUMENT

#include <string>
#include <map>
#include <vector>

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"

namespace MsWksDocumentInternal
{
struct State;
}

class MsWks3Parser;
class MsWks4Parser;
class MsWksSSParser;
class MsWks4Zone;

class MsWksGraph;
class MsWks3Text;
class MsWks4Text;

/** \brief the main class to read/store generic data of a MsWorks document v1-v3
 */
class MsWksDocument
{
  friend class MsWks3Parser;
  friend class MsWks4Parser;
  friend class MsWks4Zone;
  friend class MsWksSSParser;
public:
  //! constructor
  MsWksDocument(MWAWInputStreamPtr input, MWAWParser &parser);
  //! destructor
  virtual ~MsWksDocument();

  //! returns the document's version
  int version() const;
  //! sets the document's version
  void setVersion(int vers);
  //! returns the document's kind
  MWAWDocument::Kind getKind() const;
  //! sets the document's kind
  void setKind(MWAWDocument::Kind kind);

  //! returns the actual input
  MWAWInputStreamPtr &getInput()
  {
    return m_input;
  }
  //! returns the main parser
  MWAWParser &getMainParser()
  {
    return *m_parser;
  }
  //! returns the graph parser
  shared_ptr<MsWksGraph> getGraphParser()
  {
    return m_graphParser;
  }
  //! returns the text parser (for v1-v3 document)
  shared_ptr<MsWks3Text> getTextParser3();
  //! returns the text parser (for v4 document)
  shared_ptr<MsWks4Text> getTextParser4();
  //! a DebugFile used to write what we recognize when we parse the document
  libmwaw::DebugFile &ascii()
  {
    return m_asciiFile;
  }

  //
  // utilities functions
  //

  //! checks if the file header corresponds to a v1-v3 document (or not)
  bool checkHeader3(MWAWHeader *header, bool strict=false);
  //! returns the color which correspond to an index
  static bool getColor(int id, MWAWColor &col, int vers);
  //! returns a list of color corresponding to a version
  static std::vector<MWAWColor> const &getPalette(int vers);
  //! returns the document entry map of a v4 document
  std::multimap<std::string, MWAWEntry> &getEntryMap();
  // interface with the text parser

  //! tries to create a new page
  void newPage(int page, bool softBreak=false);
  //! tries to send a footnote
  void sendFootnote(int id);

  // interface with the graph parser

  //! send an OLE zone
  void sendOLE(int id, MWAWPosition const &pos, MWAWGraphicStyle const &style);
  /** send a rbil zone */
  void sendRBIL(int id, Vec2i const &sz);
  /** send a textbox */
  void sendTextbox(MWAWEntry const &entry, std::string const &frame);

protected:

protected:

  //
  // low level
  //
  //! inits the ascii file
  void initAsciiFile(std::string const &name);

private:
  MsWksDocument(MsWksDocument const &orig);
  MsWksDocument &operator=(MsWksDocument const &orig);

  //
  // data
  //

protected:
  //! the state
  shared_ptr<MsWksDocumentInternal::State> m_state;
public:
  //! the parser state
  shared_ptr<MWAWParserState> m_parserState;

protected:
  //! the input which can be an OLE in MSWorks 4 file
  MWAWInputStreamPtr m_input;
  //! the main parser
  MWAWParser *m_parser;
  //! the debug file of the actual input
  libmwaw::DebugFile m_asciiFile;

  //! the graph document
  shared_ptr<MsWksGraph> m_graphParser;
  //! the text document (for v1-3 document)
  shared_ptr<MsWks3Text> m_textParser3;
  //! the text document (for v4 document)
  shared_ptr<MsWks4Text> m_textParser4;

  /** callback used to send a page break */
  typedef void (MWAWParser::* NewPage)(int page, bool softBreak);
  /** callback used to send a textbox */
  typedef void (MWAWParser::* SendTextbox)(MWAWEntry const &entry, std::string const &frame);
  /** callback used to send a footnote */
  typedef void (MWAWParser::* SendFootnote)(int id);
  /** callback used to send a OLE */
  typedef void (MWAWParser::* SendOLE)(int id, MWAWPosition const &pos, MWAWGraphicStyle const &style);
  /** callback used to send a RBIL */
  typedef void (MWAWParser::* SendRBIL)(int id, Vec2i const &sz);

  /** the new page callback */
  NewPage m_newPage;
  /** the send footnote callback */
  SendFootnote m_sendFootnote;
  /** the send textbox callback */
  SendTextbox m_sendTextbox;
  /** the send ole callback */
  SendOLE m_sendOLE;
  /** the send rbil callback */
  SendRBIL m_sendRBIL;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
