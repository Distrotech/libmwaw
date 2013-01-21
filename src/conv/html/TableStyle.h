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

#ifndef TABLE_STYLE_H
#define TABLE_STYLE_H

#include "mwaw2html_internal.h"

#include <ostream>
#include <string>
#include <map>
#include <vector>

class WPXPropertyList;
class WPXPropertyListVector;

/** Small class to manage the tables style */
class TableStyleManager
{
public:
	//! constructor
	TableStyleManager() : m_cellContentNameMap(), m_rowContentNameMap(), m_columWitdhsStack()
	{
	}
	//! destructor
	~TableStyleManager()
	{
	}
	//! open a table
	void openTable(WPXPropertyListVector const &colList);
	//! close a table
	void closeTable();
	//! returns the class name corresponding to a propertylist
	std::string getCellClass(WPXPropertyList const &pList);
	//! returns the class name corresponding to a propertylist
	std::string getRowClass(WPXPropertyList const &pList);
	//! send the data to the stream
	void send(std::ostream &out);
private:
	//! convert a property list in a html content string
	std::string getCellContent(WPXPropertyList const &pList) const;
	//! convert a property list in a html content string
	std::string getRowContent(WPXPropertyList const &pList) const;
	//! try to return the col width
	bool getColumnsWidth(int i, int numSpanned, double &w) const;
	//! a map cell content -> name
	std::map<std::string, std::string> m_cellContentNameMap;
	//! a map row content -> name
	std::map<std::string, std::string> m_rowContentNameMap;
	//! a stack of column width (in inches )
	std::vector<std::vector<double> > m_columWitdhsStack;

	TableStyleManager(TableStyleManager const &orig);
	TableStyleManager operator=(TableStyleManager const &orig);
};
#endif
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
