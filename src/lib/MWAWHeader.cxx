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

MWAWHeader::MWAWHeader(MWAWDocument::Type documentType, int vers, MWAWDocument::Kind kind) :
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
    if (creator=="ACTA") {
      if (type=="OTLN") { // at least basic v2
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_ACTA, 1));
        return res;
      } else if (type=="otln") { // classic version
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_ACTA, 2));
        return res;
      }
    } else if (creator=="BOBO") {
      if (type=="CWDB" || type=="CWD2" || type=="sWDB") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_CLARISWORKS, 1, MWAWDocument::MWAW_K_DATABASE));
        return res;
      }
      if (type=="CWGR" || type=="sWGR") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_CLARISWORKS, 1, MWAWDocument::MWAW_K_DRAW));
        return res;
      }
      if (type=="CWSS" || type=="CWS2" || type=="sWSS") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_CLARISWORKS, 1, MWAWDocument::MWAW_K_SPREADSHEET));
        return res;
      }
      if (type=="CWWP" || type=="CWW2" || type=="sWPP") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_CLARISWORKS, 1));
        return res;
      }
      if (type=="CWPR") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_CLARISWORKS, 1, MWAWDocument::MWAW_K_PRESENTATION));
        return res;
      }
    } else if (creator=="BWks") {
      if (type=="BWwp") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_BEAGLEWORKS, 1));
        return res;
      }
    } else if (creator=="Dk@P") {
      if (type=="APPL") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_DOCMAKER, 1));
        return res;
      }
    } else if (creator=="FS03") {
      if (type=="WRT+") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_WRITERPLUS, 1));
        return res;
      }
    } else if (creator=="FWRT") {
      if (type=="FWRM") { // 1.7 ?
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_FULLWRITE, 1));
        return res;
      }
      if (type=="FWRT") { // 1.0 ?
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_FULLWRITE, 1));
        return res;
      }
      if (type=="FWRI") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_FULLWRITE,2));
        return res;
      }
    } else if (creator=="HMiw") { // japonese
      if (type=="IWDC") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_HANMACWORDJ,1));
        return res;
      }
    } else if (creator=="HMdr") { // korean
      if (type=="DRD2") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_HANMACWORDK,1));
        return res;
      }
    } else if (creator=="LWTE") {
      if (type=="TEXT" || type=="ttro") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_LIGHTWAYTEXT,1));
        return res;
      }
    } else if (creator=="LWTR") {
      if (type=="APPL") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_LIGHTWAYTEXT,1));
        return res;
      }
    } else if (creator=="MACA") {
      if (type=="WORD") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MACWRITE, 1));
        return res;
      }
    } else if (creator=="MDsr") {
      if (type=="APPL") { // auto content
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MACDOC, 1));
        return res;
      }
    } else if (creator=="MDvr") {
      if (type=="MDdc") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MACDOC, 1));
        return res;
      }
    } else if (creator=="MMBB") {
      if (type=="MBBT") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MARINERWRITE, 1));
        return res;
      }
    } else if (creator=="MORE") {
      if (type=="MORE") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MORE, 1));
        return res;
      }
    } else if (creator=="MOR2") {
      if (type=="MOR2") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MORE, 2));
        return res;
      }
      if (type=="MOR3") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MORE, 3));
        return res;
      }
    } else if (creator=="MWII") { // MacWriteII
      if (type=="MW2D") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MACWRITEPRO, 0));
        return res;
      }
    } else if (creator=="MWPR") {
      if (type=="MWPd") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MACWRITEPRO, 1));
        return res;
      }
    } else if (creator=="MSWD") {
      if (type=="WDBN") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MICROSOFTWORD, 3));
        return res;
      }
      if (type=="GLOS") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MICROSOFTWORD, 3));
        return res;
      }
    } else if (creator=="WORD") {
      if (type=="WDBN") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MICROSOFTWORD, 1));
        return res;
      }
    } else if (creator=="MSWK") {
      if (type=="AWWP") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MICROSOFTWORKS, 3));
        return res;
      }
      if (type=="RLRB" || type=="sWRB") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MICROSOFTWORKS, 104));
        return res;
      }
    } else if (creator=="NISI") {
      if (type=="TEXT") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_NISUSWRITER, 1));
        return res;
      }
      if (type=="GLOS") { // checkme: glossary, ie. a list of picture/word, keep it ?
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_NISUSWRITER, 1));
        return res;
      }
      // "edtt": empty file, probably created when the file is edited
    } else if (creator=="PSIP") {
      if (type=="AWWP") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MICROSOFTWORKS, 1));
        return res;
      }
    } else if (creator=="PSI2") {
      if (type=="AWWP") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MICROSOFTWORKS, 2));
        return res;
      }
    } else if (creator=="PWRI") {
      if (type=="OUTL") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MINDWRITE, 2));
        return res;
      }
    } else if (creator=="TBB5") {
      if (type=="TEXT" || type=="ttro") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_TEXEDIT, 1));
        return res;
      }
    } else if (creator=="ZEBR") {
      if (type=="ZWRT") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_GREATWORKS, 1));
        return res;
      }
      if (type=="ZOBJ") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_GREATWORKS, 1, MWAWDocument::MWAW_K_DRAW));
        return res;
      }
      // can we treat also ZOLN ?
    } else if (creator=="ZWRT") {
      if (type=="Zart") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_ZWRITE, 1));
        return res;
      }
    } else if (creator=="eDcR") {
      if (type=="eDoc") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_EDOC, 1));
        return res;
      }
    } else if (creator=="eSRD") { // self reading application
      if (type=="APPL") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_EDOC, 1));
        return res;
      }
    } else if (creator=="nX^n") {
      if (type=="nX^d") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_WRITENOW, 2));
        return res;
      }
      if (type=="nX^2") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_WRITENOW, 3));
        return res;
      }
    } else if (creator=="ttxt") {
      if (type=="TEXT" || type=="ttro") {
        res.push_back(MWAWHeader(MWAWDocument::MWAW_T_TEACHTEXT, 1));
        return res;
      }
    }
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: unknown finder info: type=%s[%s]\n", type.c_str(), creator.c_str()));

  }

  // ----------- now check resource fork ------------
  // ----------- now check data fork ------------
  if (!input->hasDataFork() || input->size() < 8)
    return res;

  input->seek(0, WPX_SEEK_SET);
  int val[4];
  for (int i = 0; i < 4; i++)
    val[i] = int(input->readULong(2));

  // ----------- clearly discriminant ------------------
  if (val[2] == 0x424F && val[3] == 0x424F && (val[0]>>8) < 8) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Claris Works file[Limited parsing]\n"));
    res.push_back(MWAWHeader(MWAWDocument::MWAW_T_CLARISWORKS, (val[0]) >> 8));
    return res;
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

    if (ok) {
      res.push_back(MWAWHeader(MWAWDocument::MWAW_T_WRITENOW, 3));
      return res;
    }
  }
  if (val[0]==0x4646 && val[1]==0x4646 && val[2]==0x3030 && val[3]==0x3030) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Mariner Write file\n"));
    res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MARINERWRITE, 1));
    return res;
  }
  if (val[0]==0x4257 && val[1]==0x6b73 && val[2]==0x4257 && val[3]==0x7770) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a BeagleWorks file\n"));
    res.push_back(MWAWHeader(MWAWDocument::MWAW_T_BEAGLEWORKS, 1));
    return res;
  }
  if (val[0]==0x4859 && val[1]==0x4c53 && val[2]==0x0210) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a HanMac Word-K file\n"));
    res.push_back(MWAWHeader(MWAWDocument::MWAW_T_HANMACWORDK, 1));
    return res;
  }
  if (val[0]==0x594c && val[1]==0x5953 && val[2]==0x100) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a HanMac Word-J file\n"));
    res.push_back(MWAWHeader(MWAWDocument::MWAW_T_HANMACWORDJ, 1));
    return res;
  }
  if (val[0]==3 && val[1]==0x4d52 && val[2]==0x4949 && val[3]==0x80) { // MRII
    res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MORE, 2));
    return res;
  }
  if (val[0]==6 && val[1]==0x4d4f && val[2]==0x5233 && val[3]==0x80) { // MOR3
    res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MORE, 3));
    return res;
  }

  if (val[0]==0x100 || val[0]==0x200) {
    if (val[1]==0x5a57 && val[2]==0x5254) {
      res.push_back(MWAWHeader(MWAWDocument::MWAW_T_GREATWORKS, val[0]==0x100 ? 1 : 2));
      return res;
    }
    if (val[1]==0x5a4f && val[2]==0x424a) {
      res.push_back(MWAWHeader(MWAWDocument::MWAW_T_GREATWORKS, val[0]==0x100 ? 1 : 2, MWAWDocument::MWAW_K_DRAW));
      return res;
    }
    // maybe we can also add outline: if (val[1]==0x5a4f && val[2]==0x4c4e)
  }
  // magic ole header
  if (val[0]==0xd0cf && val[1]==0x11e0 && val[2]==0xa1b1 && val[3]==0x1ae1)
    res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MICROSOFTWORKS, 104));

  if ((val[0]==0xfe32 && val[1]==0) || (val[0]==0xfe34 && val[1]==0) ||
      (val[0] == 0xfe37 && (val[1] == 0x23 || val[1] == 0x1c))) {
    int vers = -1;
    switch (val[1]) {
    case 0:
      if (val[0]==0xfe34) {
        MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Word 3.0 file\n"));
        vers = 3;
      } else if (val[0]==0xfe32) {
        MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Word 1.0 file\n"));
        vers = 1;
      }
      break;
    case 0x1c:
      MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Word 4.0 file\n"));
      vers = 4;
      break;
    case 0x23:
      MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Word 5.0 file\n"));
      vers = 5;
      break;
    default:
      break;
    }
    if (vers >= 0)
      res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MICROSOFTWORD, vers));
  }

  // ----------- less discriminant ------------------
  if (val[0] == 0x2e && val[1] == 0x2e) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a MacWrite II file\n"));
    res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MACWRITEPRO, 0));
  }
  if (val[0] == 4 && val[1] == 4) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a MacWritePro file\n"));
    res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MACWRITEPRO, 1));
  }
  if (val[0] == 0x7704) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a MindWrite file 2.1\n"));
    res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MINDWRITE, 2));
  }
  // ----------- other ------------------
  if (val[0]==0 && val[1]==0 && val[2]==0 && val[3]==0) {
    input->seek(8, WPX_SEEK_SET);
    int value=(int) input->readULong(1);
    if (value==0x4 || value==0x44) {
      MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a WriteNow 1.0 or 2.0 file\n"));
      res.push_back(MWAWHeader(MWAWDocument::MWAW_T_WRITENOW, 2));
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
      MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Microsoft Works %d.0 file\n", vers));
      res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MICROSOFTWORKS, vers));
    }
  }
  if (val[0] == 3 || val[0] == 6) {
    // version will be print by MWParser::check
    res.push_back(MWAWHeader(MWAWDocument::MWAW_T_MACWRITE, val[0]));
  }
  if (val[0] == 0x110) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Writerplus file\n"));
    res.push_back(MWAWHeader(MWAWDocument::MWAW_T_WRITERPLUS, 1));
  }
  //ok now look at the end of file
  if (input->seek(-4, WPX_SEEK_END))
    return res;
  int lVal[2];
  for (int i = 0; i < 2; i++)
    lVal[i]=int(input->readULong(2));
  if (lVal[0] == 0x4E4C && lVal[1]==0x544F) // NLTO
    res.push_back(MWAWHeader(MWAWDocument::MWAW_T_ACTA, 2));
  else if (lVal[1]==0 && val[0]==1 && (val[1]==1||val[1]==2))
    res.push_back(MWAWHeader(MWAWDocument::MWAW_T_ACTA, 1));
  else if (lVal[0] == 0x4657 && lVal[1]==0x5254) // FWRT
    res.push_back(MWAWHeader(MWAWDocument::MWAW_T_FULLWRITE, 2));
  else if (lVal[0] == 0 && lVal[1]==1) // not probable, but
    res.push_back(MWAWHeader(MWAWDocument::MWAW_T_FULLWRITE, 1));

  input->seek(0, WPX_SEEK_SET);
  return res;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
