/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * For further information visit http://libwps.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
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
