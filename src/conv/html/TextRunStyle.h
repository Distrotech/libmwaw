/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* libmwaw
 * Copyright (C) 2002-2005 William Lachance (wrlach@gmail.com)
 * Copyright (C) 2005 Net Integration Technologies (http://www.net-itech.com)
 * Copyright (C) 2002 Marc Maurer (uwog@uwog.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
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

#ifndef TEXT_RUN_STYLE_H
#define TEXT_RUN_STYLE_H

#include <ostream>
#include <string>
#include <map>
#include <vector>

#include <libwpd/libwpd.h>

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
	std::string getClass(WPXPropertyList const &pList, WPXPropertyListVector const &tabsStop);
	//! send the data to the stream
	void send(std::ostream &out);
protected:
	//! convert a property list in a html content string
	std::string getContent(WPXPropertyList const &pList, bool isList) const;
	//! a map content -> name
	std::map<std::string, std::string> m_contentNameMap;
	//! add data corresponding to the border
	void parseBorders(WPXPropertyList const &pList, std::ostream &out) const;
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
		void setLevel(int lvl, WPXPropertyList const &property, bool ordered);
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
	void defineLevel(WPXPropertyList const &property, bool ordered);
	//! returns the class name corresponding to a propertylist
	std::string openLevel(WPXPropertyList const &pList, bool ordered);
	//! close a level
	void closeLevel();
	//! returns the classname corresponding to a list element
	std::string getClass(WPXPropertyList const &pList, WPXPropertyListVector const &tabsStop);

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
	std::string getClass(WPXPropertyList const &pList);
	//! send the data to the stream
	void send(std::ostream &out);
protected:
	//! convert a property list in a html content string
	std::string getContent(WPXPropertyList const &pList) const;
	//! add data corresponding to a text position in out
	void parseTextPosition(char const *value, std::ostream &out) const;
	//! add data corresponding to the line decoration
	void parseDecorations(WPXPropertyList const &pList, std::ostream &out) const;
	//! a map content -> name
	std::map<std::string, std::string> m_contentNameMap;
private:
	SpanStyleManager(SpanStyleManager const &orig);
	SpanStyleManager operator=(SpanStyleManager const &orig);
};

#endif
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
