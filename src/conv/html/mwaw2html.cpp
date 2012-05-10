/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* libmwaw
 * Copyright (C) 2002-2004 William Lachance (wrlach@gmail.com)
 * Copyright (C) 2002 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2004 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2005 Net Integration Technologies (http://www.net-itech.com)
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

#include <cstring>
#include <stdio.h>
#include <libwpd-stream/libwpd-stream.h>
#include "IMWAWDocument.hxx"
#include "HtmlDocumentGenerator.h"

int printUsage()
{
	printf("Usage: mwaw2html [OPTION] <Text Mac Document>\n");
	printf("\n");
	printf("Options:\n");
	printf("--help                Shows this help message\n");
	return -1;
}

int main(int argc, char *argv[])
{
	char *file = 0;

	if (argc < 2)
		return printUsage();

	for (int i = 1; i < argc; i++)
	{
		if (!file && strncmp(argv[i], "--", 2))
			file = argv[i];
		else
			return printUsage();
	}

	if (!file)
		return printUsage();

	WPXFileStream input(file);

	IMWAWDocument::DocumentType type;
	IMWAWDocument::DocumentKind kind;
	IMWAWConfidence confidence = IMWAWDocument::isFileFormatSupported(&input, type, kind);
	if (confidence == IMWAW_CONFIDENCE_NONE || confidence == IMWAW_CONFIDENCE_POOR)
	{
		printf("ERROR: Unsupported file format!\n");
		return 1;
	}
	if (type == IMWAWDocument::UNKNOWN)
	{
		printf("ERROR: can not determine the file type!\n");
		return 1;
	}
	if (kind != IMWAWDocument::K_TEXT && kind != IMWAWDocument::K_PRESENTATION)
	{
		printf("ERROR: find a not text document!\n");
		return 1;
	}
	HtmlDocumentGenerator documentGenerator;
	IMWAWResult error = IMWAWDocument::parse(&input, &documentGenerator);

	if (error == IMWAW_FILE_ACCESS_ERROR)
		fprintf(stderr, "ERROR: File Exception!\n");
	else if (error == IMWAW_PARSE_ERROR)
		fprintf(stderr, "ERROR: Parse Exception!\n");
	else if (error == IMWAW_OLE_ERROR)
		fprintf(stderr, "ERROR: File is an OLE document!\n");
	else if (error != IMWAW_OK)
		fprintf(stderr, "ERROR: Unknown Error!\n");

	if (error != IMWAW_OK)
		return 1;

	return 0;
}
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
