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

#include <string.h>
#include <iostream>

#include "libmwaw_internal.hxx"

#include "MWAWEntry.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWRSRCParser.hxx"

#include "MWAWHeader.hxx"

MWAWHeader::MWAWHeader(MWAWDocument::DocumentType documentType, int vers, DocumentKind kind) :
  m_version(vers),
  m_docType(documentType),
  m_docKind(kind)
{
}

MWAWHeader::~MWAWHeader()
{
}

/**
 * So far, we have identified
 */
std::vector<MWAWHeader> MWAWHeader::constructHeader
(MWAWInputStreamPtr input, shared_ptr<MWAWRSRCParser> /*rsrcParser*/)
{
  std::vector<MWAWHeader> res;
  if (!input) return res;
  // ------------ first check finder info -------------
  std::string type, creator;
  if (input->getFinderInfo(type, creator)) {
    // set basic version, the correct will be filled by check header
    if (creator=="BOBO") {
      if (type=="CWDB") {
        res.push_back(MWAWHeader(MWAWDocument::CW, 1, MWAWDocument::K_DATABASE));
        return res;
      }
      if (type=="CWGR") {
        res.push_back(MWAWHeader(MWAWDocument::CW, 1, MWAWDocument::K_DRAW));
        return res;
      }
      if (type=="CWSS") {
        res.push_back(MWAWHeader(MWAWDocument::CW, 1, MWAWDocument::K_SPREADSHEET));
        return res;
      }
      if (type=="CWWP") {
        res.push_back(MWAWHeader(MWAWDocument::CW, 1));
        return res;
      }
      if (type=="CWPR") {
        res.push_back(MWAWHeader(MWAWDocument::CW, 1, MWAWDocument::K_PRESENTATION));
        return res;
      }
    } else if (creator=="FS03") {
      if (type=="WRT+") {
        res.push_back(MWAWHeader(MWAWDocument::WPLUS, 1));
        return res;
      }
    } else if (creator=="FWRT") {
      if (type=="FWRM") {
        res.push_back(MWAWHeader(MWAWDocument::FULLW, 1));
        return res;
      }
      if (type=="FWRI") {
        res.push_back(MWAWHeader(MWAWDocument::FULLW,2));
        return res;
      }
    } else if (creator=="HMiw") { // japonese
      if (type=="IWDC") {
        res.push_back(MWAWHeader(MWAWDocument::HMAC,1));
        return res;
      }
    } else if (creator=="HMdr") { // korean
      if (type=="DRD2") {
        res.push_back(MWAWHeader(MWAWDocument::HMAC,1));
        return res;
      }
    } else if (creator=="LWTE") {
      if (type=="TEXT") {
        res.push_back(MWAWHeader(MWAWDocument::LWTEXT,1));
        return res;
      }
    } else if (creator=="MACA") {
      if (type=="WORD") {
        res.push_back(MWAWHeader(MWAWDocument::MW, 1));
        return res;
      }
    } else if (creator=="MMBB") {
      if (type=="MBBT") {
        res.push_back(MWAWHeader(MWAWDocument::MARIW, 1));
        return res;
      }
    } else if (creator=="MWII") { // MacWriteII
      if (type=="MW2D") {
        res.push_back(MWAWHeader(MWAWDocument::MWPRO, 0));
        return res;
      }
    } else if (creator=="MWPR") {
      if (type=="MWPd") {
        res.push_back(MWAWHeader(MWAWDocument::MWPRO, 1));
        return res;
      }
    } else if (creator=="MSWD") {
      if (type=="WDBN") {
        res.push_back(MWAWHeader(MWAWDocument::MSWORD, 3));
        return res;
      }
      if (type=="GLOS") {
        res.push_back(MWAWHeader(MWAWDocument::MSWORD, 3));
        return res;
      }
    } else if (creator=="WORD") {
      if (type=="WDBN") {
        res.push_back(MWAWHeader(MWAWDocument::MSWORD, 1));
        return res;
      }
    } else if (creator=="MSWK") {
      if (type=="AWWP") {
        res.push_back(MWAWHeader(MWAWDocument::MSWORKS, 3));
        return res;
      }
      if (type=="RLRB") {
        res.push_back(MWAWHeader(MWAWDocument::MSWORKS, 104));
        return res;
      }
    } else if (creator=="NISI") {
      if (type=="TEXT") {
        res.push_back(MWAWHeader(MWAWDocument::NISUSW, 1));
        return res;
      }
      if (type=="GLOS") { // checkme: glossary, ie. a list of picture/word, keep it ?
        res.push_back(MWAWHeader(MWAWDocument::NISUSW, 1));
        return res;
      }
      // "edtt": empty file, probably created when the file is edited
    } else if (creator=="PSIP") {
      if (type=="AWWP") {
        res.push_back(MWAWHeader(MWAWDocument::MSWORKS, 1));
        return res;
      }
    } else if (creator=="PSI2") {
      if (type=="AWWP") {
        res.push_back(MWAWHeader(MWAWDocument::MSWORKS, 2));
        return res;
      }
    } else if (creator=="PWRI") {
      if (type=="OUTL") {
        res.push_back(MWAWHeader(MWAWDocument::MINDW, 2));
        return res;
      }
    } else if (creator=="ZWRT") {
      if (type=="Zart") {
        res.push_back(MWAWHeader(MWAWDocument::ZWRT, 1));
        return res;
      }
    } else if (creator=="nX^n") {
      if (type=="nX^d") {
        res.push_back(MWAWHeader(MWAWDocument::WNOW, 2));
        return res;
      }
      if (type=="nX^2") {
        res.push_back(MWAWHeader(MWAWDocument::WNOW, 3));
        return res;
      }
    }
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: unknown finder info: type=%s[%s]\n", type.c_str(), creator.c_str()));

  }

  // ----------- now check resource fork ------------
  // ----------- now check data fork ------------
  if (!input->hasDataFork())
    return res;

  input->seek(8, WPX_SEEK_SET);
  if (input->atEOS() || input->tell() != 8)
    return res;

  input->seek(0, WPX_SEEK_SET);
  int val[4];
  for (int i = 0; i < 4; i++)
    val[i] = int(input->readULong(2));

  // ----------- clearly discriminant ------------------
  if (val[2] == 0x424F && val[3] == 0x424F && (val[0]>>8) < 8) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Claris Works file[Limited parsing]\n"));
    res.push_back(MWAWHeader(MWAWDocument::CW, (val[0]) >> 8));
  }
  if (val[0]==0x5772 && val[1]==0x6974 && val[2]==0x654e && val[3]==0x6f77) {
    input->seek(8, WPX_SEEK_SET);
    int version = int(input->readLong(2));

#ifdef DEBUG
    bool ok = (version >= 0 && version <= 3);
    if (ok)
      MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a WriteNow file version 3.0 or 4.0\n"));
    else
      MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a WriteNow file (unknown version %d)\n", version));
#else
    bool ok = version == 2;
#endif

    if (ok)
      res.push_back(MWAWHeader(MWAWDocument::WNOW, 3));
  }
  if (val[0]==0x4646 && val[1]==0x4646 && val[2]==0x3030 && val[3]==0x3030) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Mariner Write file[no parsing]\n"));
    res.push_back(MWAWHeader(MWAWDocument::MARIW, 1));
  }
  if (val[0]==0x4859 && val[1]==0x4c53 && val[2]==0x0210) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a HanMac Word-K file[mininal parsing]\n"));
    res.push_back(MWAWHeader(MWAWDocument::HMAC, 1));
  }
  if (val[0]==0x594c && val[1]==0x5953 && val[2]==0x100) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a HanMac Word-J file[no parsing and no output]\n"));
    res.push_back(MWAWHeader(MWAWDocument::HMAC, 1));
  }

  // magic ole header
  if (val[0]==0xd0cf && val[1]==0x11e0 && val[2]==0xa1b1 && val[3]==0x1ae1)
    res.push_back(MWAWHeader(MWAWDocument::MSWORKS, 104));

  if ((val[0]==0xfe32 && val[1]==0) || (val[0]==0xfe34 && val[1]==0) ||
      (val[0] == 0xfe37 && (val[1] == 0x23 || val[1] == 0x1c))) {
    int vers = -1;
    switch (val[1]) {
    case 0:
      if (val[0]==0xfe34) {
        MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Word 3.0 file[minimal parsing]\n"));
        vers = 3;
      } else if (val[0]==0xfe32) {
        MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Word 1.0 file\n"));
        vers = 1;
      }
      break;
    case 0x1c:
      MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Word 4.0 file[minimal parsing]\n"));
      vers = 4;
      break;
    case 0x23:
      MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Word 5.0 file[minimal parsing]\n"));
      vers = 5;
      break;
    default:
      break;
    }
    if (vers >= 0)
      res.push_back(MWAWHeader(MWAWDocument::MSWORD, vers));
  }

  // ----------- less discriminant ------------------
  if (val[0] == 0x2e && val[1] == 0x2e) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a MacWrite II file\n"));
    res.push_back(MWAWHeader(MWAWDocument::MWPRO, 0));
  }
  if (val[0] == 4 && val[1] == 4) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a MacWritePro file\n"));
    res.push_back(MWAWHeader(MWAWDocument::MWPRO, 1));
  }
  if (val[0] == 0x7704) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a MindWrite file 2.1\n"));
    res.push_back(MWAWHeader(MWAWDocument::MINDW, 2));
  }
  // ----------- other ------------------
  if (val[0]==0 && val[1]==0 && val[2]==0 && val[3]==0) {
    input->seek(8, WPX_SEEK_SET);
    if (input->readULong(1) == 0x4) {
      MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a WriteNow 1.0 or 2.0 file\n"));
      res.push_back(MWAWHeader(MWAWDocument::WNOW, 2));
    }
  }
  if (val[0]==0) {
    int vers = -1;
    switch(val[1]) {
    case 4:
      vers = 1;
      break;
    case 8:
      vers = 2;
      break;
    case 9:
      vers = 3;
      break;
#ifdef DEBUG
    case 11: // a msworks 4 file ( but not a text file)
      vers = 4;
      break;
#endif
    default:
      break;
    }
    if (vers > 0) {
      if (vers <= 3) {
        MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Microsoft Works %d.0 file\n", vers));
      } else {
        MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Microsoft Works %d.0 file\n", vers));
      }
      res.push_back(MWAWHeader(MWAWDocument::MSWORKS, vers));
    }
  }
  if (val[0] == 3 || val[0] == 6) {
    // version will be print by MWParser::check
    res.push_back(MWAWHeader(MWAWDocument::MW, val[0]));
  }
  if (val[0] == 0x110) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Writerplus file\n"));
    res.push_back(MWAWHeader(MWAWDocument::WPLUS, 1));
  }
  //ok now look at the end of file
  input->seek(0, WPX_SEEK_SET);
  while(!input->atEOS()) {
    if (input->seek(1024, WPX_SEEK_CUR) != 0) break;
  }
  input->seek(-4, WPX_SEEK_CUR);
  for (int i = 0; i < 2; i++)
    val[i]=int(input->readULong(2));
  if (val[0] == 0x4657 && val[1]==0x5254) // FWRT
    res.push_back(MWAWHeader(MWAWDocument::FULLW, 2));
  if (val[0] == 0 && val[1]==1) { // not probable, but
    res.push_back(MWAWHeader(MWAWDocument::FULLW, 1));
  }

  input->seek(0, WPX_SEEK_SET);
  return res;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
