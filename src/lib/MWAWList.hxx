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

#include <libwpd/libwpd.h>

class WPXPropertyList;
class WPXDocumentInterface;

/** small structure to keep information about a list level */
struct MWAWListLevel {
  /** the type of the level */
  enum Type { DEFAULT, NONE, BULLET, DECIMAL, LOWER_ALPHA, UPPER_ALPHA,
              LOWER_ROMAN, UPPER_ROMAN, LABEL
            };
  //! the item alignement
  enum Alignment { LEFT, RIGHT, CENTER };

  /** basic constructor */
  MWAWListLevel() : m_type(NONE), m_labelBeforeSpace(0.0), m_labelWidth(0.1), m_labelAfterSpace(0.0), m_numBeforeLabels(0), m_alignment(LEFT), m_startValue(0),
    m_label(""), m_prefix(""), m_suffix(""), m_bullet(""), m_extra("") {
  }
  /** destructor */
  ~MWAWListLevel() {}

  /** returns true if the level type was not set */
  bool isDefault() const {
    return m_type ==DEFAULT;
  }
  /** returns true if the list is decimal, alpha or roman */
  bool isNumeric() const {
    return m_type !=DEFAULT && m_type !=NONE && m_type != BULLET;
  }
  /** add the information of this level in the propList */
  void addTo(WPXPropertyList &propList) const;

  /** returns the start value (if set) or 1 */
  int getStartValue() const {
    return m_startValue <= 0 ? 1 : m_startValue;
  }

  /** comparison function ( compare all values excepted m_startValues */
  int cmp(MWAWListLevel const &levl) const;

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, MWAWListLevel const &ft);

  /** the type of the level */
  Type m_type;
  double m_labelBeforeSpace /** the extra space between inserting a label */;
  double m_labelWidth /** the minimum label width */;
  double m_labelAfterSpace /** the minimum distance between the label and the text */;
  /** the number of label to show before this */
  int m_numBeforeLabels;
  //! the alignement ( left, center, ...)
  Alignment m_alignment;
  /** the actual value (if this is an ordered level ) */
  int m_startValue;
  WPXString m_label /** the text label */,
	    m_prefix /** string which preceedes the number if we have an ordered level*/,
            m_suffix/** string which follows the number if we have an ordered level*/,
            m_bullet /** the bullet if we have an bullet level */;
  //! extra data
  std::string m_extra;
};

/** a small structure used to store the informations about a list */
class MWAWList
{
public:
  /** default constructor */
  MWAWList() : m_levels(), m_actLevel(-1), m_actualIndices(), m_nextIndices() {
    m_id[0] = m_id[1] = -1;
  }

  /** returns the list id */
  int getId() const {
    return m_id[0];
  }

  /** resize the number of level of the list (keeping only n level) */
  void resize(int levl);
  /** returns true if we can add a new level in the list without changing is meaning */
  bool isCompatibleWith(int levl, MWAWListLevel const &level) const;
  /** returns true if the list is compatible with the defined level of new list */
  bool isCompatibleWith(MWAWList const &newList) const;
  /** update the indices, the actual level from newList */
  void updateIndicesFrom(MWAWList const &list);

  /** swap the list id

  \note a cheat because writerperfect imposes to get a new id if the level 1 changes
  */
  void swapId() const {
    int tmp = m_id[0];
    m_id[0] = m_id[1];
    m_id[1] = tmp;
  }

  /** set the list id */
  void setId(int newId) const;

  /** returns a level if it exists */
  MWAWListLevel getLevel(int levl) const {
    if (levl >= 0 && levl < int(m_levels.size()))
      return m_levels[size_t(levl)];
    MWAW_DEBUG_MSG(("MWAWList::getLevel: can not find level %d\n", levl));
    return MWAWListLevel();
  }
  /** returns the number of level */
  int numLevels() const {
    return int(m_levels.size());
  }
  /** sets a level */
  void set(int levl, MWAWListLevel const &level);

  /** set the list level */
  void setLevel(int levl) const;
  /** open the list element */
  void openElement() const;
  /** close the list element */
  void closeElement() const {}
  /** returns the startvalue corresponding to the actual level ( or -1 for an unknown/unordered list) */
  int getStartValueForNextElement() const;
  /** set the startvalue corresponding to the actual level*/
  void setStartValueForNextElement(int value);

  /** returns true is a level is numeric */
  bool isNumeric(int levl) const;

  /** send the list level information to the document interface. */
  void sendTo(WPXDocumentInterface &docInterface, int level) const;

protected:
  //! the different levels
  std::vector<MWAWListLevel> m_levels;

  //! the actual levels
  mutable int m_actLevel;
  mutable std::vector<int> m_actualIndices, m_nextIndices;
  //! the identificator ( actual and auxilliar )
  mutable int m_id[2];
};

/** a manager which manages the lists, keeps the different kind of lists, to assure the unicity of each list */
class MWAWListManager
{
public:
  //! the constructor
  MWAWListManager() : m_listList(), m_sendIdList() { }
  //! the destructor
  ~MWAWListManager() { }
  /** send the list to the document interface. If this is already done, does nothing and return false. */
  bool send(int index, WPXDocumentInterface &docInterface) const;
  //! returns a list with given index ( if found )
  shared_ptr<MWAWList> getList(int index) const;
  //! returns a new list corresponding to a list where we have a new level
  shared_ptr<MWAWList> getNewList(shared_ptr<MWAWList> actList, int levl, MWAWListLevel const &level);
protected:
  /** reset the list id corresponding to a list */
  void resetSend(size_t id) const;
  //! the list of created list
  std::vector<MWAWList> m_listList;
  //! the list of send list to interface
  mutable std::vector<bool> m_sendIdList;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
