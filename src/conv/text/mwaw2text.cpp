/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* libmwaw
 * Copyright (C) 2002 William Lachance (wrlach@gmail.com)
 * Copyright (C) 2002-2003 Marc Maurer (uwog@uwog.net)
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

#include <stdio.h>
#include <string.h>

#include <libwpd-stream/libwpd-stream.h>
#include "libmwaw.hxx"

#include "TextDocumentGenerator.h"

int printUsage()
{
	printf("Usage: mwaw2text [OPTION] <Mac Text Document>\n");
	printf("\n");
	printf("Options:\n");
	printf("--info                Display document metadata instead of the text\n");
	printf("--help                Shows this help message\n");
	return -1;
}

int main(int argc, char *argv[])
{
	if (argc < 2)
		return printUsage();

	char *szInputFile = 0;
	bool isInfo = false;

	for (int i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "--info"))
			isInfo = true;
		else if (!szInputFile && strncmp(argv[i], "--", 2))
			szInputFile = argv[i];
		else
			return printUsage();
	}

	if (!szInputFile)
		return printUsage();

	WPXFileStream input(argv[1]);

	IMWAWDocument::DocumentType type;
	IMWAWDocument::DocumentKind kind;
	MWAWConfidence confidence = IMWAWDocument::isFileFormatSupported(&input, type, kind);
	if (confidence == MWAW_CONFIDENCE_NONE || confidence == MWAW_CONFIDENCE_POOR)
	{
		printf("ERROR: Unsupported file format!\n");
		return 1;
	}
	if (type == IMWAWDocument::UNKNOWN)
	{
		printf("ERROR: find an old mac file!\n");
		return 1;
	}
	if (kind != IMWAWDocument::K_TEXT && kind != IMWAWDocument::K_PRESENTATION)
	{
		printf("ERROR: find a not text document!\n");
		return 1;
	}

	TextDocumentGenerator documentGenerator(isInfo);
	MWAWResult error = IMWAWDocument::parse(&input, &documentGenerator);

	if (error == MWAW_FILE_ACCESS_ERROR)
		fprintf(stderr, "ERROR: File Exception!\n");
	else if (error == MWAW_PARSE_ERROR)
		fprintf(stderr, "ERROR: Parse Exception!\n");
	else if (error == MWAW_OLE_ERROR)
		fprintf(stderr, "ERROR: File is an OLE document, but does not contain a Works stream!\n");
	else if (error != MWAW_OK)
		fprintf(stderr, "ERROR: Unknown Error!\n");

	if (error != MWAW_OK)
		return 1;

	return 0;
}
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
