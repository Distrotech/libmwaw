/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */

/* libmwaw: tools
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

#include <iostream>
#include <sstream>
#include <string>

#include <libmwaw_internal.hxx>
#include "file_internal.h"
#include "input.h"
#include "ole.h"

namespace libmwaw_tools
{
unsigned short OLE::readU16()
{
  unsigned long nRead;
  unsigned char const *data = m_input.read(2, nRead);
  if (!data || nRead != 2) return 0;
  return (unsigned short) (data[0]+(data[1]<<8));
}
unsigned int OLE::readU32()
{
  unsigned long nRead;
  unsigned char const *data = m_input.read(4, nRead);
  if (!data || nRead != 4) return 0;
  return (unsigned int)  (data[0]+(data[1]<<8)+(data[2]<<16)+(data[3]<<24));
}

std::string OLE::getCLSIDType()
{
  static const unsigned char magic[] =
  { 0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1 };

  m_input.seek(0, InputStream::SK_SET);
  // first check the magic signature
  for (int i = 0; i < 8; i++) {
    if (m_input.readU8() != magic[i])
      return "";
  }

  // get dir start entry
  m_input.seek(0x30, InputStream::SK_SET);
  unsigned int startDirent = readU32();
  if (m_input.atEOS())
    return "";
  // go to the clsid of the root dir entry
  m_input.seek((startDirent+1)*512+0x50, InputStream::SK_SET);
  if (m_input.atEOS())
    return "";
  unsigned int clsData[4];
  for (int i = 0; i < 4; i++)
    clsData[i] = readU32();
  if (m_input.atEOS())
    return "";

  // do not accept not standart ole
  if (clsData[1] != 0 || clsData[2] != 0xC0 || clsData[3] != 0x46000000L)
    return "";

  switch(clsData[0]) {
  case 0x00000319:
    return "OLE file(EMH-picture?)"; // addon Enhanced Metafile ( find in some file)

  case 0x00020906:
    return "OLE file(MSWord mac)";
  case 0x00021290:
    return "OLE file(MSClipArtGalley2)";
  case 0x000212F0:
    return "OLE file(MSWordArt)"; // or MSWordArt.2
  case 0x00021302:
    return "OLE file(MSWorksWPDoc)"; // addon

    // MS Apps
  case 0x00030000:
    return "OLE file(ExcelWorksheet)";
  case 0x00030001:
    return "OLE file(ExcelChart)";
  case 0x00030002:
    return "OLE file(ExcelMacrosheet)";
  case 0x00030003:
    return "OLE file(WordDocument)";
  case 0x00030004:
    return "OLE file(MSPowerPoint)";
  case 0x00030005:
    return "OLE file(MSPowerPointSho)";
  case 0x00030006:
    return "OLE file(MSGraph)";
  case 0x00030007:
    return "OLE file(MSDraw)"; // find also ca003 ?
  case 0x00030008:
    return "OLE file(Note-It)";
  case 0x00030009:
    return "OLE file(WordArt)";
  case 0x0003000a:
    return "OLE file(PBrush)";
  case 0x0003000b:
    return "OLE file(Microsoft Equation)"; // "Microsoft Equation Editor"
  case 0x0003000c:
    return "OLE file(Package)";
  case 0x0003000d:
    return "OLE file(SoundRec)";
  case 0x0003000e:
    return "OLE file(MPlayer)";
    // MS Demos
  case 0x0003000f:
    return "OLE file(ServerDemo)"; // "OLE 1.0 Server Demo"
  case 0x00030010:
    return "OLE file(Srtest)"; // "OLE 1.0 Test Demo"
  case 0x00030011:
    return "OLE file(SrtInv)"; //  "OLE 1.0 Inv Demo"
  case 0x00030012:
    return "OLE file(OleDemo)"; //"OLE 1.0 Demo"

    // Coromandel / Dorai Swamy / 718-793-7963
  case 0x00030013:
    return "OLE file(CoromandelIntegra)";
  case 0x00030014:
    return "OLE file(CoromandelObjServer)";

    // 3-d Visions Corp / Peter Hirsch / 310-325-1339
  case 0x00030015:
    return "OLE file(StanfordGraphics)";

    // Deltapoint / Nigel Hearne / 408-648-4000
  case 0x00030016:
    return "OLE file(DGraphCHART)";
  case 0x00030017:
    return "OLE file(DGraphDATA)";

    // Corel / Richard V. Woodend / 613-728-8200 x1153
  case 0x00030018:
    return "OLE file(CorelPhotoPaint)"; // "Corel PhotoPaint"
  case 0x00030019:
    return "OLE file(CorelShow)"; // "Corel Show"
  case 0x0003001a:
    return "OLE file(CorelChart)";
  case 0x0003001b:
    return "OLE file(CorelDraw)"; // "Corel Draw"

    // Inset Systems / Mark Skiba / 203-740-2400
  case 0x0003001c:
    return "OLE file(HJWIN1.0)"; // "Inset Systems"

    // Mark V Systems / Mark McGraw / 818-995-7671
  case 0x0003001d:
    return "OLE file(MarkV ObjMakerOLE)"; // "MarkV Systems Object Maker"

    // IdentiTech / Mike Gilger / 407-951-9503
  case 0x0003001e:
    return "OLE file(IdentiTech FYI)"; // "IdentiTech FYI"
  case 0x0003001f:
    return "OLE file(IdentiTech FYIView)"; // "IdentiTech FYI Viewer"

    // Inventa Corporation / Balaji Varadarajan / 408-987-0220
  case 0x00030020:
    return "OLE file(Stickynote)";

    // ShapeWare Corp. / Lori Pearce / 206-467-6723
  case 0x00030021:
    return "OLE file(ShapewareVISIO10)";
  case 0x00030022:
    return "OLE file(Shapeware ImportServer)"; // "Spaheware Import Server"

    // test app SrTest
  case 0x00030023:
    return "OLE file(SrvrTest)"; // "OLE 1.0 Server Test"

    // test app ClTest.  Doesn't really work as a server but is in reg db
  case 0x00030025:
    return "OLE file(Cltest)"; // "OLE 1.0 Client Test"

    // Microsoft ClipArt Gallery   Sherry Larsen-Holmes
  case 0x00030026:
    return "OLE file(MS_ClipArt_Gallery)";
    // Microsoft Project  Cory Reina
  case 0x00030027:
    return "OLE file(MSProject)";

    // Microsoft Works Chart
  case 0x00030028:
    return "OLE file(MSWorksChart)";

    // Microsoft Works Spreadsheet
  case 0x00030029:
    return "OLE file(MSWorksSpreadsheet)";

    // AFX apps - Dean McCrory
  case 0x0003002A:
    return "OLE file(MinSvr)"; // "AFX Mini Server"
  case 0x0003002B:
    return "OLE file(HierarchyList)"; // "AFX Hierarchy List"
  case 0x0003002C:
    return "OLE file(BibRef)"; // "AFX BibRef"
  case 0x0003002D:
    return "OLE file(MinSvrMI)"; // "AFX Mini Server MI"
  case 0x0003002E:
    return "OLE file(TestServ)"; // "AFX Test Server"

    // Ami Pro
  case 0x0003002F:
    return "OLE file(AmiProDocument)";

    // WordPerfect Presentations For Windows
  case 0x00030030:
    return "OLE file(WPGraphics)";
  case 0x00030031:
    return "OLE file(WPCharts)";

    // MicroGrafx Charisma
  case 0x00030032:
    return "OLE file(Charisma)";
  case 0x00030033:
    return "OLE file(Charisma_30)"; // v 3.0
  case 0x00030034:
    return "OLE file(CharPres_30)"; // v 3.0 Pres
    // MicroGrafx Draw
  case 0x00030035:
    return "OLE file(MicroGrafx Draw)"; //"MicroGrafx Draw"
    // MicroGrafx Designer
  case 0x00030036:
    return "OLE file(MicroGrafx Designer_40)"; // "MicroGrafx Designer 4.0"

    // STAR DIVISION
  case 0x000424CA:
    return "OLE file(StarMath)"; // "StarMath 1.0"
  case 0x00043AD2:
    return "OLE file(Star FontWork)"; // "Star FontWork"
  case 0x000456EE:
    return "OLE file(StarMath2)"; // "StarMath 2.0"
  default:
    MWAW_DEBUG_MSG(("OLE::getCLSIDType: Find unknown clsid=%ux\n", clsData[0]));
  case 0:
    break;
  }

  return "";
}
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
