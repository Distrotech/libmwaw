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
 * For further information visit http://libmwaw.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#ifndef MWAW_MWAW_CONTENT_LISTENER_H
#define MWAW_MWAW_CONTENT_LISTENER_H

#include <vector>

#include <libwpd/WPXString.h>
#include "IMWAWContentListener.hxx"

namespace MWAWTools
{
class Convertissor;
typedef shared_ptr<Convertissor> ConvertissorPtr;
}

/** an internal MWAWContentListener which adds some functions to a basic IMWAWContentListener to manage font and paragraph properties and mac objects. */
class MWAWContentListener : public IMWAWContentListener
{
  typedef IMWAWContentListener parent;
protected:
  //! the actual state
  struct ParsingState : public parent::ParsingState {
    //! constructor
    ParsingState() : parent::ParsingState(), m_nextTableIndice(0) {}
    //! destructor
    virtual ~ParsingState() {}
    //! the next table index
    int m_nextTableIndice;
  };

public:
  //! access to the constructor
  static shared_ptr<MWAWContentListener> create
  (std::list<DMWAWPageSpan> &pageList,WPXDocumentInterface *documentInterface, MWAWTools::ConvertissorPtr &convertissor) {
    shared_ptr<MWAWContentListener> res(new MWAWContentListener(pageList, documentInterface, convertissor));
    res->_init();
    return res;
  }

  //! a virtual destructor
  ~MWAWContentListener();

  //! ends the document (potentially adding unsent tables, ...)
  void endDocument();

  using parent::insertBreak;
  using parent::startDocument;
  using parent::justificationChange;
  using parent::lineSpacingChange;

  using parent::setTextAttribute;
  using parent::setTextFont;
  using parent::setFontSize;
  using parent::setFontColor;

  using parent::insertEOL;
  using parent::insertTab;

  using parent::setTabs;
  using parent::setParagraphMargin;
  using parent::setParagraphTextIndent;

  using parent::insertUnicode;
  using parent::insertCharacter;

protected:
  //! protected constructor \sa create
  MWAWContentListener(std::list<DMWAWPageSpan> &pageList,
                      WPXDocumentInterface *documentInterface,
                      MWAWTools::ConvertissorPtr &convertissor);

  //! returns a new parsing state
  virtual ParsingState *_newParsingState() const {
    return new ParsingState;
  }
  //! returns the actual parsing state
  ParsingState *parsingState() {
    return reinterpret_cast<ParsingState *>
           (parent::parsingState());
  }

  /** creates a new parsing state (copy of the actual state)
   *
   * \return the old one */
  ParsingState *_pushParsingState() {
    return reinterpret_cast<ParsingState *>
           (parent::_pushParsingState());
  }



private:
  //! protected constructor: forbidden
  MWAWContentListener(const MWAWContentListener&);
  //! operator=: forbidden
  MWAWContentListener& operator=(const MWAWContentListener&);

  //! the convertissor;
  MWAWTools::ConvertissorPtr m_convertissor;
};

typedef shared_ptr<MWAWContentListener> MWAWContentListenerPtr;

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
