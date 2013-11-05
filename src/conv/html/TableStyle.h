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

#ifndef TABLE_STYLE_H
#define TABLE_STYLE_H

#include "mwaw2html_internal.h"

#include <ostream>
#include <string>
#include <map>
#include <vector>

class librevenge::RVNGPropertyList;
class librevenge::RVNGPropertyListVector;

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
	void openTable(librevenge::RVNGPropertyListVector const &colList);
	//! close a table
	void closeTable();
	//! returns the class name corresponding to a propertylist
	std::string getCellClass(librevenge::RVNGPropertyList const &pList);
	//! returns the class name corresponding to a propertylist
	std::string getRowClass(librevenge::RVNGPropertyList const &pList);
	//! send the data to the stream
	void send(std::ostream &out);
private:
	//! convert a property list in a html content string
	std::string getCellContent(librevenge::RVNGPropertyList const &pList) const;
	//! convert a property list in a html content string
	std::string getRowContent(librevenge::RVNGPropertyList const &pList) const;
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
