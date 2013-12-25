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

/*
 * Parser to BeagleWorks document
 *
 */
#ifndef BW_STRUCT_MANAGER
#  define BW_STRUCT_MANAGER

#include <map>

#include "libmwaw_internal.hxx"

namespace BWStructManagerInternal
{
struct State;
}
/** \brief the main class to read the structure shared between different BeagleWorks files
 *
 *
 *
 */
class BWStructManager
{
public:
  struct Frame;

  //! constructor
  BWStructManager(MWAWParserStatePtr parserState);
  //! destructor
  ~BWStructManager();

  //! returns a frame corresponding to an id
  bool getFrame(int fId, Frame &frame) const;
  //! returns the id to frame map
  std::map<int,Frame> const &getIdFrameMap() const;

  //! returns a font id corresponding to a file id (or -3)
  int getFontId(int fFontId) const;

  //! read the font names
  bool readFontNames(MWAWEntry const &entry);
  //! read the frame
  bool readFrame(MWAWEntry const &entry);

  // resource fork

  //! read a picture (edtp resource )
  bool readPicture(int pId, librevenge::RVNGBinaryData &pict);
  //! read the windows positions ( wPos 1001 resource block )
  bool readwPos(MWAWEntry const &entry);
  //! read the font style ressource
  bool readFontStyle(MWAWEntry const &entry);

//! Internal: a structure use to store a frame in a BeagleWorks files
  struct Frame {
    //! constructor
    Frame() : m_charAnchor(true), m_id(0), m_pictId(0), m_origin(), m_dim(), m_page(1),
      m_wrap(0), m_border(), m_bordersSet(0), m_extra("")
    {
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Frame const &frm)
    {
      if (frm.m_id) o << "id=" << frm.m_id << ",";
      if (!frm.m_charAnchor) o << "pageFrame,";
      if (frm.m_page!=1) o << "page=" << frm.m_page << ",";
      if (frm.m_origin[0]>0||frm.m_origin[1]>0)
        o << "origin=" << frm.m_origin << ",";
      o << "dim=" << frm.m_dim << ",";
      if (frm.m_pictId) o << "picId=" << std::hex << frm.m_pictId << std::dec << ",";
      o << frm.m_extra;
      return o;
    }
    //! a flag to know if this is a char or a page frame
    bool m_charAnchor;
    //! frame id
    int m_id;
    //! the picture id
    int m_pictId;
    //! the origin ( for a page frame )
    Vec2f m_origin;
    //! the dimension
    Vec2f m_dim;
    //! the page ( for a page frame )
    int m_page;
    //! the wrapping: 0=none, 1=rectangle, 2=irregular
    int m_wrap;
    //! the border
    MWAWBorder m_border;
    //! the list of border which are set in form libmwaw::LeftBit|...
    int m_bordersSet;
    //! extra data
    std::string m_extra;
  };
protected:
  //! return the input input
  MWAWInputStreamPtr getInput();
  //! a DebugFile used to write what we recognize when we parse the document
  libmwaw::DebugFile &ascii();
  //! return the input input
  MWAWInputStreamPtr rsrcInput();
  //! a DebugFile used to write what we recognize when we parse the document in rsrc
  libmwaw::DebugFile &rsrcAscii();

private:
  BWStructManager(BWStructManager const &orig);
  BWStructManager &operator=(BWStructManager const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<BWStructManagerInternal::State> m_state;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
