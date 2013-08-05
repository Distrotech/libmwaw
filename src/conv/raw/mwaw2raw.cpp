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

#include <stdio.h>
#include <string.h>
#include <libwpd-stream/libwpd-stream.h>
#include <libmwaw/libmwaw.hxx>
#include "RawDocumentGenerator.h"

int printUsage()
{
	printf("Usage: mwaw2raw [OPTION] <Text Mac Document>\n");
	printf("\n");
	printf("Options:\n");
	printf("--callgraph           Display the call graph nesting level\n");
	printf("--help                Shows this help message\n");
	return -1;
}

int main(int argc, char *argv[])
{
	bool printIndentLevel = false;
	char *file = NULL;

	if (argc < 2)
		return printUsage();

	for (int i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "--callgraph"))
			printIndentLevel = true;
		else if (!file && strncmp(argv[i], "--", 2))
			file = argv[i];
		else
			return printUsage();
	}

	if (!file)
		return printUsage();


	WPXFileStream input(file);

	MWAWDocument::DocumentType type;
	MWAWDocument::DocumentKind kind;
	MWAWConfidence confidence = MWAWDocument::isFileFormatSupported(&input, type, kind);
	if (confidence == MWAW_CONFIDENCE_NONE || confidence == MWAW_CONFIDENCE_POOR)
	{
		printf("ERROR: Unsupported file format!\n");
		return 1;
	}
	if (type == MWAWDocument::UNKNOWN)
	{
		printf("ERROR: can not determine the file type!\n");
		return 1;
	}

	RawDocumentGenerator documentGenerator(printIndentLevel);
	MWAWResult error = MWAWDocument::parse(&input, &documentGenerator);

	if (error == MWAW_FILE_ACCESS_ERROR)
		fprintf(stderr, "ERROR: File Exception!\n");
	else if (error == MWAW_PARSE_ERROR)
		fprintf(stderr, "ERROR: Parse Exception!\n");
	else if (error == MWAW_OLE_ERROR)
		fprintf(stderr, "ERROR: File is an OLE document!\n");
	else if (error != MWAW_OK)
		fprintf(stderr, "ERROR: Unknown Error!\n");

	if (error != MWAW_OK)
		return 1;

	return 0;
}
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
