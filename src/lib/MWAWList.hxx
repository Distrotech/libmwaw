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

#ifndef MWAW_LIST_H
#  define MWAW_LIST_H

#include <iostream>

#include <vector>

#include <libwpd/WPXString.h>

class WPXPropertyList;
class WPXDocumentInterface;

/** a small structure used to store the informations about a list */
class MWAWList
{
public:
  /** small structure to keep information about a level */
  struct Level {
    /** the type of the level */
    enum Type { DEFAULT, NONE, BULLET, DECIMAL, LOWER_ALPHA, UPPER_ALPHA,
                LOWER_ROMAN, UPPER_ROMAN
              };

    /** basic constructor */
    Level() : m_labelIndent(0.0), m_labelWidth(0.0), m_startValue(0), m_type(NONE),
      m_prefix(""), m_suffix(""), m_bullet(""), m_sendToInterface(false) { }

    ~Level() {}

    /** returns true if the level type was not set */
    bool isDefault() const {
      return m_type ==DEFAULT;
    }
    /** returns true if the list is decimal, alpha or roman */
    bool isNumeric() const {
      return m_type !=DEFAULT && m_type !=NONE && m_type != BULLET;
    }
    /** add the information of this level in the propList */
    void addTo(WPXPropertyList &propList, int startVal) const;

    /** returns true, if addTo has been called */
    bool isSendToInterface() const {
      return m_sendToInterface;
    }
    /** reset the sendToInterface flag */
    void resetSendToInterface() const {
      m_sendToInterface = false;
    }

    /** returns the start value (if set) or 1 */
    int getStartValue() const {
      return m_startValue <= 0 ? 1 : m_startValue;
    }

    //! comparison function
    int cmp(Level const &levl) const;

    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Level const &ft);

    double m_labelIndent /** the list indent*/;
    double m_labelWidth /** the list width */;
    /** the actual value (if this is an ordered level ) */
    int m_startValue;
    /** the type of the level */
    Type m_type;
    WPXString m_prefix /** string which preceedes the number if we have an ordered level*/,
              m_suffix/** string which follows the number if we have an ordered level*/,
              m_bullet /** the bullet if we have an bullet level */;

  protected:
    /** true if it is already send to WPXDocumentInterface */
    mutable bool m_sendToInterface;
  };

  /** default constructor */
  MWAWList() : m_levels(), m_actLevel(-1), m_actualIndices(), m_nextIndices(),
    m_id(-1), m_previousId (-1) {}

  /** returns the list id */
  int getId() const {
    return m_id;
  }

  /** returns the previous list id

  \note a cheat because writerperfect imposes to get a new id if the level 1 changes
  */
  int getPreviousId() const {
    return m_previousId;
  }

  /** set the list id */
  void setId(int newId);

  /** returns the number of level */
  int numLevels() const {
    return int(m_levels.size());
  }
  /** sets a level */
  void set(int levl, Level const &level);

  /** set the list level */
  void setLevel(int levl) const;
  /** open the list element */
  void openElement() const;
  /** close the list element */
  void closeElement() const {}

  /** returns true is a level is numeric */
  bool isNumeric(int levl) const;

  /** returns true of the level must be send to the document interface */
  bool mustSendLevel(int level) const;

  /** send the list information to the document interface */
  void sendTo(WPXDocumentInterface &docInterface, int level) const;

protected:
  std::vector<Level> m_levels;

  mutable int m_actLevel;
  mutable std::vector<int> m_actualIndices, m_nextIndices;
  mutable int m_id, m_previousId;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
