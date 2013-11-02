/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
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

#ifndef TEXT_RUN_STYLE_H
#define TEXT_RUN_STYLE_H

#include <ostream>
#include <string>
#include <map>
#include <vector>

#include <librevenge/librevenge.h>

#include "mwaw2html_internal.h"

/** Small class to manage the paragraph style */
class ParagraphStyleManager
{
public:
	//! constructor
	ParagraphStyleManager() : m_contentNameMap()
	{
	}
	//! destructor
	virtual ~ParagraphStyleManager()
	{
	}
	//! returns the class name corresponding to a propertylist
	std::string getClass(RVNGPropertyList const &pList, RVNGPropertyListVector const &tabsStop);
	//! send the data to the stream
	void send(std::ostream &out);
protected:
	//! convert a property list in a html content string
	std::string getContent(RVNGPropertyList const &pList, bool isList) const;
	//! a map content -> name
	std::map<std::string, std::string> m_contentNameMap;
	//! add data corresponding to the border
	void parseBorders(RVNGPropertyList const &pList, std::ostream &out) const;
private:
	ParagraphStyleManager(ParagraphStyleManager const &orig);
	ParagraphStyleManager operator=(ParagraphStyleManager const &orig);
};

/** Small class to manage the list style */
class ListStyleManager : public ParagraphStyleManager
{
public:
	struct List
	{
		//! constructor
		List() : m_contentsList(), m_level(0)
		{
		}
		//! destructor
		~List()
		{
		}
		//! set the property correspond to a level
		void setLevel(int lvl, RVNGPropertyList const &property, bool ordered);
		//! open a new level
		void openLevel() const
		{
			m_level++;
		}
		//! open a new level
		void closeLevel() const
		{
			if (m_level <= 0)
			{
				MWAW_DEBUG_MSG(("ListStyleManager::List: no level is open\n"));
				return;
			}
			m_level--;
		}

		//! return the content string
		std::string str() const;
	protected:
		//! the properties
		std::vector<std::string> m_contentsList;
		//! the actual list level
		mutable int m_level;
	};
	//! constructor
	ListStyleManager() : ParagraphStyleManager(), m_levelNameMap(),
		m_idListMap(), m_actualIdStack()
	{
	}
	//! destructor
	~ListStyleManager()
	{
	}
	//! add a level to the corresponding list
	void defineLevel(RVNGPropertyList const &property, bool ordered);
	//! returns the class name corresponding to a propertylist
	std::string openLevel(RVNGPropertyList const &pList, bool ordered);
	//! close a level
	void closeLevel();
	//! returns the classname corresponding to a list element
	std::string getClass(RVNGPropertyList const &pList, RVNGPropertyListVector const &tabsStop);

	//! send the data to the stream
	void send(std::ostream &out);
protected:
	//! a map content -> list level name
	std::map<std::string, std::string> m_levelNameMap;
	//! a map listId -> list
	std::map<int, List> m_idListMap;
	//! the actual list id
	std::vector<int> m_actualIdStack;
private:
	ListStyleManager(ListStyleManager const &orig);
	ListStyleManager operator=(ListStyleManager const &orig);
};

/** Small class to manage the span style */
class SpanStyleManager
{
public:
	//! constructor
	SpanStyleManager() : m_contentNameMap()
	{
	}
	//! destructor
	~SpanStyleManager()
	{
	}
	//! returns the class name corresponding to a propertylist
	std::string getClass(RVNGPropertyList const &pList);
	//! send the data to the stream
	void send(std::ostream &out);
protected:
	//! convert a property list in a html content string
	std::string getContent(RVNGPropertyList const &pList) const;
	//! add data corresponding to a text position in out
	void parseTextPosition(char const *value, std::ostream &out) const;
	//! add data corresponding to the line decoration
	void parseDecorations(RVNGPropertyList const &pList, std::ostream &out) const;
	//! a map content -> name
	std::map<std::string, std::string> m_contentNameMap;
private:
	SpanStyleManager(SpanStyleManager const &orig);
	SpanStyleManager operator=(SpanStyleManager const &orig);
};

#endif
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
