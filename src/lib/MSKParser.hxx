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

#ifndef MSK_PARSER
#  define MSK_PARSER

#include <string>

#include "libmwaw_internal.hxx"

#include "MWAWParser.hxx"

class MWAWPosition;
class WPXPropertyList;

class MSKGraph;

typedef class MWAWContentListener MSKContentListener;
typedef shared_ptr<MSKContentListener> MSKContentListenerPtr;

namespace MSKParserInternal
{
struct State;
}

/** \brief generic parser for Microsoft Works file
 *
 *
 *
 */
class MSKParser : public MWAWParser
{
  friend class MSKGraph;
public:
  //! constructor
  MSKParser(MWAWInputStreamPtr input, MWAWHeader *header);

  //! destructor
  virtual ~MSKParser();

  //! return the color which correspond to an index
  bool getColor(int id, Vec3uc &col, int vers=-1) const;

  //! return a list of color corresponding to a version
  static std::vector<Vec3uc> const &getPalette(int vers);

  //! check if a position is inside the file
  bool checkIfPositionValid(long pos);

  //! virtual function used to send the text of a frame (v4)
  virtual void sendFrameText(MWAWEntry const &entry, std::string const &frame);

  //! virtual function used to send an OLE (v4)
  virtual void sendOLE(int id, MWAWPosition const &pos,
                       WPXPropertyList frameExtras);

  //! returns the page top left point
  virtual Vec2f getPageTopLeft() const = 0;

protected:
  //! the listener
  MSKContentListenerPtr m_listener;

  //! the state
  shared_ptr<MSKParserInternal::State> m_state;
};

#endif
