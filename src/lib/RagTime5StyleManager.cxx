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

#include <iomanip>
#include <map>
#include <sstream>
#include <stack>

#include "MWAWDebug.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWParagraph.hxx"

#include "RagTime5Parser.hxx"
#include "RagTime5StructManager.hxx"

#include "RagTime5StyleManager.hxx"

/** Internal: the structures of a RagTime5Style */
namespace RagTime5StyleManagerInternal
{
////////////////////////////////////////
//! Internal: the helper to read field graphic field for a RagTime5StyleManager
struct GraphicFieldParser : public RagTime5StructManager::FieldParser {
  //! enum used to define the zone type
  enum Type { Z_Styles, Z_Colors };

  //! constructor
  GraphicFieldParser(Type type) :
    RagTime5StructManager::FieldParser(type==Z_Styles ? "GraphStyle" : "GraphColor"), m_type(type)
  {
    m_regroupFields=(m_type==Z_Styles);
  }
  //! return the debug name corresponding to a field
  std::string getZoneName(int n) const
  {
    std::stringstream s;
    s << (m_type==Z_Styles ? "GraphStyle-GS" : "GraphColor-GC") << n;
    return s.str();
  }
  //! parse a field
  virtual bool parseField(RagTime5StructManager::Field &field, RagTime5Zone &zone, int /*n*/, libmwaw::DebugStream &f)
  {
    if (m_type==Z_Styles) {
      RagTime5StyleManager::GraphicStyle style;
      MWAWInputStreamPtr input=zone.getInput();
      if (style.read(input, field))
        f << style;
      else
        f << "##" << field;
    }
    else
      f << field;
    return true;
  }

protected:
  //! the zone type
  Type m_type;
};

////////////////////////////////////////
//! Internal: the helper to read style for a RagTime5Text
struct TextFieldParser : public RagTime5StructManager::FieldParser {
  //! constructor
  TextFieldParser() : RagTime5StructManager::FieldParser("TextStyle"), m_styleList()
  {
  }
  //! return the debug name corresponding to a field
  std::string getZoneName(int n) const
  {
    std::stringstream s;
    s << "TextStyle-TS" << n;
    return s.str();
  }
  //! parse a field
  virtual bool parseField(RagTime5StructManager::Field &field, RagTime5Zone &/*zone*/, int n, libmwaw::DebugStream &f)
  {
    if (n<=0) {
      MWAW_DEBUG_MSG(("RagTime5TextInternal::TextFieldParser::parseField: n=%d is bad\n", n));
      n=0;
    }
    if (n>=int(m_styleList.size()))
      m_styleList.resize(size_t(n+1));
    RagTime5StyleManager::TextStyle &style=m_styleList[size_t(n)];
    if (style.read(field)) {
      RagTime5StyleManager::TextStyle modStyle;
      modStyle.read(field);
      f << modStyle;
    }
    else
      f << "#" << field;
    return true;
  }

  //! the list of read style
  std::vector<RagTime5StyleManager::TextStyle> m_styleList;
};

//! Internal: the state of a RagTime5Style
struct State {
  //! constructor
  State() : m_textStyleList() { }
  //! the list of text styles
  std::vector<RagTime5StyleManager::TextStyle> m_textStyleList;
};
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
RagTime5StyleManager::RagTime5StyleManager(RagTime5Parser &parser) :
  m_mainParser(parser), m_parserState(parser.getParserState()), m_state(new RagTime5StyleManagerInternal::State)
{
}

RagTime5StyleManager::~RagTime5StyleManager()
{
}

////////////////////////////////////////////////////////////
// read style
////////////////////////////////////////////////////////////
bool RagTime5StyleManager::readGraphicColors(RagTime5ClusterManager::Cluster &cluster)
{
  RagTime5StyleManagerInternal::GraphicFieldParser fieldParser(RagTime5StyleManagerInternal::GraphicFieldParser::Z_Colors);
  return m_mainParser.readStructZone(cluster, fieldParser, 14);
}

bool RagTime5StyleManager::readGraphicStyles(RagTime5ClusterManager::Cluster &cluster)
{
  RagTime5StyleManagerInternal::GraphicFieldParser fieldParser
  (RagTime5StyleManagerInternal::GraphicFieldParser::Z_Styles);
  return m_mainParser.readStructZone(cluster, fieldParser, 14);
}

bool RagTime5StyleManager::readTextStyles(RagTime5ClusterManager::Cluster &cluster)
{
  RagTime5StyleManagerInternal::TextFieldParser fieldParser;
  if (!m_mainParser.readStructZone(cluster, fieldParser, 14))
    return false;

  if (fieldParser.m_styleList.empty())
    fieldParser.m_styleList.resize(1);

  //
  // check parent relation, check for loop, ...
  //
  std::vector<size_t> rootList;
  std::stack<size_t> toCheck;
  std::multimap<size_t, size_t> idToChildIpMap;
  size_t numStyles=size_t(fieldParser.m_styleList.size());
  for (size_t i=0; i<numStyles; ++i) {
    RagTime5StyleManager::TextStyle &style=fieldParser.m_styleList[i];
    if (!style.m_fontName.empty()) // update the font it
      style.m_fontId=m_parserState->m_fontConverter->getId(style.m_fontName.cstr());
    bool ok=true;
    for (int j=0; j<2; ++j) {
      if (style.m_parentId[j]<=0)
        continue;
      if (style.m_parentId[j]>=(int) numStyles) {
        MWAW_DEBUG_MSG(("RagTime5StyleManager::readTextStyles: find unexpected parent %d for style %d\n",
                        (int) style.m_parentId[j], (int) i));
        style.m_parentId[j]=0;
        continue;
      }
      ok=false;
      idToChildIpMap.insert(std::multimap<size_t, size_t>::value_type(size_t(style.m_parentId[j]),i));
    }
    if (!ok) continue;
    rootList.push_back(i);
    toCheck.push(i);
  }
  std::set<size_t> seens;
  while (true) {
    size_t posToCheck=0; // to make clang happy
    if (!toCheck.empty()) {
      posToCheck=toCheck.top();
      toCheck.pop();
    }
    else if (seens.size()+1==numStyles)
      break;
    else {
      bool ok=false;
      for (size_t i=1; i<numStyles; ++i) {
        if (seens.find(i)!=seens.end())
          continue;
        MWAW_DEBUG_MSG(("RagTime5StyleManager::readTextStyles: find unexpected root %d\n", (int) i));
        posToCheck=i;
        rootList.push_back(i);

        RagTime5StyleManager::TextStyle &style=fieldParser.m_styleList[i];
        style.m_parentId[0]=style.m_parentId[1]=0;
        ok=true;
        break;
      }
      if (!ok)
        break;
    }
    if (seens.find(posToCheck)!=seens.end()) {
      MWAW_DEBUG_MSG(("RagTime5StyleManager::readTextStyles: oops, %d is already seens\n", (int) posToCheck));
      continue;
    }
    seens.insert(posToCheck);
    std::multimap<size_t, size_t>::iterator childIt=idToChildIpMap.lower_bound(posToCheck);
    std::vector<size_t> badChildList;
    while (childIt!=idToChildIpMap.end() && childIt->first==posToCheck) {
      size_t childId=childIt++->second;
      if (seens.find(childId)!=seens.end()) {
        MWAW_DEBUG_MSG(("RagTime5StyleManager::readTextStyles: find loop for child %d\n", (int)childId));
        RagTime5StyleManager::TextStyle &style=fieldParser.m_styleList[childId];
        if (style.m_parentId[0]==(int) posToCheck)
          style.m_parentId[0]=0;
        if (style.m_parentId[1]==(int) posToCheck)
          style.m_parentId[1]=0;
        badChildList.push_back(childId);
        continue;
      }
      toCheck.push(childId);
    }
    for (size_t i=0; i<badChildList.size(); ++i) {
      childIt=idToChildIpMap.lower_bound(posToCheck);
      while (childIt!=idToChildIpMap.end() && childIt->first==posToCheck) {
        if (childIt->second==badChildList[i]) {
          idToChildIpMap.erase(childIt);
          break;
        }
        ++childIt;
      }
    }
  }

  if (!m_state->m_textStyleList.empty()) {
    MWAW_DEBUG_MSG(("RagTime5StyleManager::readTextStyles: Ooops, we already set some textStyles\n"));
  }

  // now let generate the final style
  m_state->m_textStyleList.resize(numStyles);
  seens.clear();
  for (size_t i=0; i<rootList.size(); ++i) {
    size_t id=rootList[i];
    if (id>=numStyles) {
      MWAW_DEBUG_MSG(("RagTime5StyleManager::readTextStyles: find loop for id=%d\n", (int)id));
      continue;
    }
    updateTextStyles(id, fieldParser.m_styleList[id], fieldParser.m_styleList, idToChildIpMap, seens);
  }
  return true;
}

void RagTime5StyleManager::updateTextStyles
(size_t id, RagTime5StyleManager::TextStyle const &style, std::vector<RagTime5StyleManager::TextStyle> const &listReadStyles,
 std::multimap<size_t, size_t> const &idToChildIpMap, std::set<size_t> &seens)
{
  if (id>=m_state->m_textStyleList.size() || seens.find(id)!=seens.end()) {
    MWAW_DEBUG_MSG(("RagTime5StyleManager::updateTextStyles: problem with style with id=%d\n", (int)id));
    return;
  }
  seens.insert(id);
  RagTime5StyleManager::TextStyle styl=style;
  styl.m_fontFlags[0]&=(~style.m_fontFlags[1]);
  m_state->m_textStyleList[id]=styl;

  std::multimap<size_t, size_t>::const_iterator childIt=idToChildIpMap.lower_bound(id);
  while (childIt!=idToChildIpMap.end() && childIt->first==id) {
    size_t childId=childIt++->second;
    if (childId>=listReadStyles.size()) {
      MWAW_DEBUG_MSG(("RagTime5StyleManager::updateTextStyles: problem with style with childId=%d\n", (int)childId));
      continue;
    }
    RagTime5StyleManager::TextStyle childStyle=styl;
    childStyle.insert(listReadStyles[childId]);
    updateTextStyles(childId, childStyle, listReadStyles, idToChildIpMap, seens);
  }
}

bool RagTime5StyleManager::update(int tId, MWAWFont &font, MWAWParagraph &para)
{
  font=MWAWFont();
  para=MWAWParagraph();

  if (tId<=0 || tId>=(int) m_state->m_textStyleList.size()) {
    MWAW_DEBUG_MSG(("RagTime5StyleManager::update:l can not find text style %d\n", tId));
    return false;
  }
  RagTime5StyleManager::TextStyle const &style=m_state->m_textStyleList[size_t(tId)];
  if (style.m_fontId>0) font.setId(style.m_fontId);
  if (style.m_fontSize>0) font.setSize((float) style.m_fontSize);

  MWAWFont::Line underline(MWAWFont::Line::None);
  uint32_t flag=style.m_fontFlags[0];
  uint32_t flags=0;
  if (flag&0x1) flags |= MWAWFont::boldBit;
  if (flag&0x2) flags |= MWAWFont::italicBit;
  if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple); // checkme
  if (flag&0x8) flags |= MWAWFont::embossBit;
  if (flag&0x10) flags |= MWAWFont::shadowBit;

  if (flag&0x200) font.setStrikeOutStyle(MWAWFont::Line::Simple);
  if (flag&0x400) flags |= MWAWFont::smallCapsBit;
  // flag&0x800: kumorarya
  if (flag&0x2000)
    underline.m_word=true;
  switch (style.m_caps) {
  case 1:
    flags |= MWAWFont::uppercaseBit;
    break;
  case 2:
    flags |= MWAWFont::lowercaseBit;
    break;
  case 3:
    flags |= MWAWFont::initialcaseBit;
    break;
  default:
    break;
  }
  switch (style.m_underline) {
  case 1:
    underline.m_style=MWAWFont::Line::Simple;
    font.setUnderline(underline);
    break;
  case 2:
    underline.m_style=MWAWFont::Line::Simple;
    underline.m_type=MWAWFont::Line::Double;
    font.setUnderline(underline);
    break;
  default:
    break;
  }
  if (style.m_letterSpacings[0]>0 || style.m_letterSpacings[0]<0)
    font.setDeltaLetterSpacing(float(1+style.m_letterSpacings[0]), librevenge::RVNG_PERCENT);
  if (style.m_widthStreching>0)
    font.setWidthStreching((float)style.m_widthStreching);
  if (style.m_scriptPosition.isSet() || style.m_fontScaling>=0) {
    float scaling=style.m_fontScaling>0 ? style.m_fontScaling : 1;
    font.set(MWAWFont::Script(*style.m_scriptPosition*100,librevenge::RVNG_PERCENT,int(scaling*100)));
  }
  if (style.m_language>0) {
    std::string lang=TextStyle::getLanguageLocale(style.m_language);
    if (!lang.empty())
      font.setLanguage(lang);
  }
  font.setFlags(flags);

  if (style.m_keepWithNext.isSet() && *style.m_keepWithNext)
    para.m_breakStatus = para.m_breakStatus.get()|MWAWParagraph::NoBreakWithNextBit;
  switch (style.m_justify) {
  case 0:
    break;
  case 1:
    para.m_justify = MWAWParagraph::JustificationCenter;
    break;
  case 2:
    para.m_justify = MWAWParagraph::JustificationRight ;
    break;
  case 3:
    para.m_justify = MWAWParagraph::JustificationFull;
    break;
  case 4:
    para.m_justify = MWAWParagraph::JustificationFullAllLines;
    break;
  default:
    break;
  }
  // TODO: use style.m_breakMethod
  para.m_marginsUnit=librevenge::RVNG_POINT;
  for (int i=0; i<3; ++i) {
    if (style.m_margins[i]<0) continue;
    if (i==2)
      para.m_margins[0]=style.m_margins[2]-*para.m_margins[1];
    else
      para.m_margins[i+1] = style.m_margins[i];
  }
  if (style.m_spacings[0]>0) {
    if (style.m_spacingUnits[0]==0)
      para.setInterline(style.m_spacings[0], librevenge::RVNG_PERCENT);
    else if (style.m_spacingUnits[0]==1)
      para.setInterline(style.m_spacings[0], librevenge::RVNG_POINT);
  }
  for (int i=1; i<3; ++i) {
    if (style.m_spacings[i]<0) continue;
    if (style.m_spacingUnits[i]==0)
      para.m_spacings[i]=style.m_spacings[i]*12./72.;
    else if (style.m_spacingUnits[0]==1)
      para.m_spacings[i]=style.m_spacings[i]/72.;
  }
  // tabs stop
  for (size_t i=0; i<style.m_tabList.size(); ++i) {
    RagTime5StructManager::TabStop const &tab=style.m_tabList[i];
    MWAWTabStop newTab;
    newTab.m_position = tab.m_position/72.;
    switch (tab.m_type) {
    case 2:
    case 5: // kintou waritsuke
      newTab.m_alignment = MWAWTabStop::CENTER;
      break;
    case 3:
      newTab.m_alignment = MWAWTabStop::RIGHT;
      break;
    case 4:
      newTab.m_alignment = MWAWTabStop::DECIMAL;
      break;
    case 1: // left
    default:
      break;
    }
    newTab.m_leaderCharacter=tab.m_leaderChar;
    para.m_tabs->push_back(newTab);
  }
  return true;
}

////////////////////////////////////////////////////////////
// parse cluster
////////////////////////////////////////////////////////////

//
// graphic style
//
bool RagTime5StyleManager::GraphicStyle::read(MWAWInputStreamPtr &input, RagTime5StructManager::Field const &field)
{
  std::stringstream s;
  if (field.m_type==RagTime5StructManager::Field::T_FieldList) {
    switch (field.m_fileType) {
    case 0x7d02a:
    case 0x145e05a: {
      int wh=field.m_fileType==0x7d02a ? 0 : 1;
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Color && child.m_fileType==0x84040) {
          m_colors[wh]=child.m_color;
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unknown color %d block\n", wh));
        s << "##col[" << wh << "]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    }
    case 0x145e02a:
    case 0x145e0ea: {
      int wh=field.m_fileType==0x145e02a ? 0 : 1;
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Double && child.m_fileType==0xb6000) {
          m_colorsAlpha[wh]=float(child.m_doubleValue);
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unknown colorAlpha[%d] block\n", wh));
        s << "###colorAlpha[" << wh << "]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    }
    case 0x145e01a: {
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Long && child.m_fileType==0x147c080) {
          m_parentId=(int) child.m_longValue[0];
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unknown parent block\n"));
        s << "###parent=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    }
    case 0x7d04a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Double && child.m_fileType==0x1494800) {
          m_width=float(child.m_doubleValue);
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unknown width block\n"));
        s << "###w=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x145e0ba: {
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Bool && child.m_fileType==0x360c0) {
          m_hidden=child.m_longValue[0]!=0;
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unknown no print block\n"));
        s << "###hidden=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    }

    case 0x14600ca:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==(long)0x80033000) {
          m_dash=child.m_longList;
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unknown dash block\n"));
        s << "###dash=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x146005a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Code && child.m_fileType==0x8d000) {
          if (child.m_string=="LiOu")
            m_position=3;
          else if (child.m_string=="LiCe") // checkme
            m_position=2;
          else if (child.m_string=="LiIn")
            m_position=1;
          else if (child.m_string=="LiRo")
            m_position=4;
          else {
            MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unknown position string %s\n", child.m_string.cstr()));
            s << "##pos=" << child.m_string.cstr() << ",";
          }
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unknown position block\n"));
        s << "###pos=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x146007a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Code && child.m_fileType==0x8d000) {
          if (child.m_string=="LiRo")
            m_mitter=2;
          else if (child.m_string=="LiBe")
            m_mitter=3;
          else {
            MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unknown mitter string %s\n", child.m_string.cstr()));
            s << "##mitter=" << child.m_string.cstr() << ",";
          }
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unknown mitter block\n"));
        s << "###mitter=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x148981a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Code && child.m_fileType==0x8d000) {
          if (child.m_string=="GrNo")
            m_gradient=1;
          else if (child.m_string=="GrRa")
            m_gradient=2;
          else {
            MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unknown gradient string %s\n", child.m_string.cstr()));
            s << "##gradient=" << child.m_string.cstr() << ",";
          }
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unknown gradient block\n"));
        s << "###gradient=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x14600aa:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Code && child.m_fileType==0x8d000) {
          if (child.m_string=="CaRo")
            m_cap=2;
          else if (child.m_string=="CaSq")
            m_cap=3;
          else {
            MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unknown cap string %s\n", child.m_string.cstr()));
            s << "##cap=" << child.m_string.cstr() << ",";
          }
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unknown cap block\n"));
        s << "###cap=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x148985a: // checkme
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Double && child.m_fileType==0x1495800) {
          m_gradientRotation=float(360*child.m_doubleValue);
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unknown grad rotation block\n"));
        s << "###rot[grad]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x148983a: // checkme
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_DoubleList && child.m_doubleList.size()==2 && child.m_fileType==0x74040) {
          m_gradientCenter=MWAWVec2f((float) child.m_doubleList[0], (float) child.m_doubleList[1]);
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unknown grad center block\n"));
        s << "###rot[center]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x146008a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Double && child.m_fileType==0xb6000) {
          m_limitPercent=(float) child.m_doubleValue;
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unknown limit percent block\n"));
        s << "###limitPercent=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    // unknown small id
    case 0x145e11a: { // frequent
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Long && child.m_fileType==0x17d5880) {
          s << "#unkn0=" << child.m_longValue[0] << ",";
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unknown unkn0 block\n"));
        s << "###unkn0=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    }
    case 0x145e12a: // unknown small int 2|3
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Long && child.m_fileType==0x17d5880) {
          MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unkn1 block\n"));
          s << "#unkn1=" << child.m_longValue[0];
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some unknown unkn1 block\n"));
        s << "###unkn1=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    default:
      break;
    }
  }
  else if (field.m_type==RagTime5StructManager::Field::T_Unstructured) {
    switch (field.m_fileType) {
    case 0x148c01a: {
      if (field.m_entry.length()!=12) {
        MWAW_DEBUG_MSG(("RagTime5StyleManager::GraphicStyle::read: find some odd size for pattern\n"));
        s << "##pattern=" << field << ",";
        m_extra+=s.str();
        return true;
      }
      input->seek(field.m_entry.begin(), librevenge::RVNG_SEEK_SET);
      for (int i=0; i<2; ++i) {
        static int const(expected[])= {0xb, 0x40};
        int val=(int) input->readULong(2);
        if (val!=expected[i])
          s << "pat" << i << "=" << std::hex << val << std::dec << ",";
      }
      m_pattern.reset(new MWAWGraphicStyle::Pattern);
      m_pattern->m_colors[0]=MWAWColor::white();
      m_pattern->m_colors[1]=MWAWColor::black();
      m_pattern->m_dim=MWAWVec2i(8,8);
      m_pattern->m_data.resize(8);
      for (size_t i=0; i<8; ++i)
        m_pattern->m_data[i]=(unsigned char) input->readULong(1);
      m_extra+=s.str();
      return true;
    }
    default:
      break;
    }
  }
  return false;
}

std::ostream &operator<<(std::ostream &o, RagTime5StyleManager::GraphicStyle const &style)
{
  if (style.m_parentId>=0) o << "parent=GS" << style.m_parentId << ",";
  if (style.m_width>=0) o << "w=" << style.m_width << ",";
  if (!style.m_colors[0].isBlack()) o << "color0=" << style.m_colors[0] << ",";
  if (!style.m_colors[1].isWhite()) o << "color1=" << style.m_colors[1] << ",";
  for (int i=0; i<2; ++i) {
    if (style.m_colorsAlpha[i]<1)
      o << "color" << i << "[alpha]=" << style.m_colorsAlpha[i] << ",";
  }
  if (!style.m_dash.empty()) {
    o << "dash=";
    for (size_t i=0; i<style.m_dash.size(); ++i)
      o << style.m_dash[i] << ":";
    o << ",";
  }
  if (style.m_pattern)
    o << "pattern=[" << *style.m_pattern << "],";
  switch (style.m_gradient) {
  case 0:
    break;
  case 1:
    o << "grad[normal],";
    break;
  case 2:
    o << "grad[radial],";
    break;
  default:
    o<< "##gradient=" << style.m_gradient;
    break;
  }
  if (style.m_gradientRotation<0 || style.m_gradientRotation>0)
    o << "rot[grad]=" << style.m_gradientRotation << ",";
  if (style.m_gradientCenter!=MWAWVec2f(0.5f,0.5f))
    o << "center[grad]=" << style.m_gradientCenter << ",";
  switch (style.m_position) {
  case 1:
    o << "pos[inside],";
    break;
  case 2:
    break;
  case 3:
    o << "pos[outside],";
    break;
  case 4:
    o << "pos[round],";
    break;
  default:
    o << "#pos=" << style.m_position << ",";
    break;
  }
  switch (style.m_cap) {
  case 1: // triangle
    break;
  case 2:
    o << "cap[round],";
    break;
  case 3:
    o << "cap[square],";
    break;
  default:
    o << "#cap=" << style.m_cap << ",";
    break;
  }
  switch (style.m_mitter) {
  case 1: // no add
    break;
  case 2:
    o << "mitter[round],";
    break;
  case 3:
    o << "mitter[out],";
    break;
  default:
    o << "#mitter=" << style.m_mitter << ",";
    break;
  }
  if (style.m_limitPercent<1||style.m_limitPercent>1)
    o << "limit=" << 100*style.m_limitPercent << "%";
  if (style.m_hidden)
    o << "hidden,";
  o << style.m_extra;
  return o;
}

//
// text style
//
std::string RagTime5StyleManager::TextStyle::getLanguageLocale(int id)
{
  switch (id) {
  case 1:
    return "hr_HR";
  case 4:
    return "ru_RU";
  case 8:
    return "da_DK";
  case 9:
    return "sv_SE";
  case 0xa:
    return "nl_NL";
  case 0xb:
    return "fi_FI";
  case 0xc:
    return "it_IT";
  case 0xd: // initial accent
  case 0x800d:
    return "es_ES";
  case 0xf:
    return "gr_GR";
  case 0x11:
    return "ja_JP";
  case 0x16:
    return "tr_TR";
  case 0x4005:
  case 0x8005: // initial accent
    return "fr_FR";
  case 0x4006: // old?
  case 0x6006:
    return "de_CH";
  case 0x8006: // old?
  case 0xa006:
    return "de_DE";
  case 0x4007:
    return "en_GB";
  case 0x8007:
    return "en_US";
  case 0x400e:
    return "pt_BR";
  case 0x800e:
    return "pt_PT";
  case 0x4012:
    return "nn_NO";
  case 0x8012:
    return "no_NO";
  default:
    break;
  }
  return "";
}

bool RagTime5StyleManager::TextStyle::read(RagTime5StructManager::Field const &field)
{
  std::stringstream s;
  if (field.m_type==RagTime5StructManager::Field::T_FieldList) {
    switch (field.m_fileType) {
    case 0x7a0aa: // style parent id?
    case 0x1474042: { // main parent id?
      int wh=field.m_fileType==0x1474042 ? 0 : 1;
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Long && child.m_fileType==0x1479080) {
          m_parentId[wh]=(int) child.m_longValue[0];
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown parent id[%d] block\n", wh));
        s << "###parent" << wh << "[id]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    }
    case 0x14741fa:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==(long)0x80045080) {
          for (size_t j=0; j<child.m_longList.size(); ++j)
            m_linkIdList.push_back((int) child.m_longList[j]);
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown link id block\n"));
        s << "###link[id]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x1469840:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Long && child.m_fileType==0x147b880) {
          m_dateStyleId=(int) child.m_longValue[0];
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown date style block\n"));
        s << "###date[id]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x145e01a:
    case 0x14741ea:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Long && child.m_fileType==0x147c080) {
          if (field.m_fileType==0x145e01a)
            m_graphStyleId=(int) child.m_longValue[0];
          else
            m_graphLineStyleId=(int) child.m_longValue[0];
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown graphic style block\n"));
        s << "###graph[" << std::hex << field.m_fileType << std::dec << "]=" << child << ",";
      }
      m_extra+=s.str();
      return true;

    //
    // para
    //
    case 0x14750ea:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Bool && child.m_fileType==0x360c0) {
          m_keepWithNext=child.m_longValue[0]!=0;
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown keep with next block\n"));
        s << "###keep[withNext]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x147505a: // left margin
    case 0x147506a: // right margin
    case 0x147507a: { // first margin
      int wh=int(((field.m_fileType&0xF0)>>4)-5);
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Double && child.m_fileType==0x1493800) {
          m_margins[wh]=child.m_doubleValue;
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown margins[%d] block\n", wh));
        s << "###margins[" << wh << "]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    }
    case 0x147501a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Code && child.m_fileType==0x8d000) {
          if (child.m_string=="----") // checkme
            m_justify=-1;
          else if (child.m_string=="left")
            m_justify=0;
          else if (child.m_string=="cent")
            m_justify=1;
          else if (child.m_string=="rght")
            m_justify=2;
          else if (child.m_string=="full")
            m_justify=3;
          else if (child.m_string=="fful")
            m_justify=4;
          // find also thgr
          else {
            MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some justify block %s\n", child.m_string.cstr()));
            s << "##justify=" << child.m_string.cstr() << ",";
          }
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown justify block\n"));
        s << "###justify=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x147502a:
    case 0x14750aa:
    case 0x14750ba: {
      int wh=field.m_fileType==0x147502a ? 0 : field.m_fileType==0x14750aa ? 1 : 2;
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_LongDouble && child.m_fileType==0x149a940) {
          m_spacings[wh]=child.m_doubleValue;
          m_spacingUnits[wh]=(int) child.m_longValue[0];
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown spacings %d block\n", wh));
        s << "###spacings[" << wh << "]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    }
    case 0x14752da:
    case 0x147536a:
    case 0x147538a: {
      int wh=field.m_fileType==0x14752da ? 0 : field.m_fileType==0x147536a ? 1 : 2;
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Double && child.m_fileType==0x1495000) {
          s << "delta[" << (wh==0 ? "interline" : wh==1 ? "before" : "after") << "]=" << child.m_doubleValue << ",";
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown spacings delta %d block\n", wh));
        s << "###delta[spacings" << wh << "]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    }
    case 0x147530a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Code && child.m_fileType==0x8d000) {
          if (child.m_string=="----") // checkme
            m_breakMethod=0;
          else if (child.m_string=="nxtC")
            m_breakMethod=1;
          else if (child.m_string=="nxtP")
            m_breakMethod=2;
          else if (child.m_string=="nxtE")
            m_breakMethod=3;
          else if (child.m_string=="nxtO")
            m_breakMethod=4;
          else {
            MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown break method block %s\n", child.m_string.cstr()));
            s << "##break[method]=" << child.m_string.cstr() << ",";
          }
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown break method block\n"));
        s << "###break[method]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x147550a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Bool && child.m_fileType==0x360c0) {
          if (child.m_longValue[0])
            s << "text[margins]=canOverlap,";
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown text margin overlap block\n"));
        s << "###text[margins]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x147516a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Bool && child.m_fileType==0x360c0) {
          if (child.m_longValue[0])
            s << "line[align]=ongrid,";
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown line grid align block\n"));
        s << "###line[gridalign]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x147546a:
    case 0x147548a: {
      std::string wh(field.m_fileType==0x147546a ? "orphan" : "widows");
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Long && child.m_fileType==0x328c0) {
          s << wh << "=" << child.m_longValue[0];
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown number %s block\n", wh.c_str()));
        s << "###" << wh << "=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    }
    case 0x14754ba:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Unstructured && child.m_fileType==0x1476840) {
          // height in line, number of character, first line with text, scaling
          s << "drop[initial]=" << child.m_extra << ",";
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown drop initial block\n"));
        s << "###drop[initial]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x14750ca: // one tab, remove tab?
    case 0x147510a:
      if (field.m_fileType==0x14750ca) s << "#tab0";
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_TabList && (child.m_fileType==(long)0x81474040 || child.m_fileType==0x1474040)) {
          m_tabList=child.m_tabList;
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown tab block\n"));
        s << "###tab=" << child << ",";
      }
      m_extra+=s.str();
      return true;

    //
    // char
    //
    case 0x7a05a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Double && child.m_fileType==0x1495000) {
          m_fontSize=float(child.m_doubleValue);
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown font size block\n"));
        s << "###size[font]=" << child << ",";
      }
      m_extra+=s.str();
      return true;

    case 0xa7017:
    case 0xa7037:
    case 0xa7047:
    case 0xa7057:
    case 0xa7067: {
      int wh=int(((field.m_fileType&0x70)>>4)-1);
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Unicode && child.m_fileType==0xc8042) {
          if (wh==2)
            m_fontName=child.m_string;
          else {
            static char const *(what[])= {"[full]" /* unsure */, "[##UNDEF]", "", "[style]" /* regular, ...*/, "[from]", "[full2]"};
            s << "font" << what[wh] << "=\"" << child.m_string.cstr() << "\",";
          }
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some font name[%d] block\n", wh));
        s << "###font[" << wh << "]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    }
    case 0xa7077:
    case 0x147407a:
    case 0x147408a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Long && child.m_fileType==0x3b880) {
          switch (field.m_fileType) {
          case 0xa7077:
            m_fontId=(int) child.m_longValue[0];
            break;
          case 0x147407a:
            s << "hyph[minSyl]=" << child.m_longValue[0] << ",";
            break;
          case 0x147408a:
            s << "hyph[minWord]=" << child.m_longValue[0] << ",";
            break;
          default:
            MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown long=%lx\n", (unsigned long)field.m_fileType));
            break;
          }
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown long=%lx block\n", (unsigned long)field.m_fileType));
        s << "###long[" << std::hex << field.m_fileType << std::dec << "]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x7a09a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_2Long && child.m_fileType==0xa4840) {
          m_fontFlags[0]=(uint32_t)child.m_longValue[0];
          m_fontFlags[1]=(uint32_t)child.m_longValue[1];
          continue;
        }
        if (child.m_type==RagTime5StructManager::Field::T_Long && child.m_fileType==0xa4000) {
          m_fontFlags[0]=(uint32_t)child.m_longValue[0];
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown font flags block\n"));
        s << "###flags[font]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x14740ba:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Code && child.m_fileType==0x8d000) {
          if (child.m_string=="----") // checkme
            m_underline=0;
          else if (child.m_string=="undl")
            m_underline=1;
          else if (child.m_string=="Dund")
            m_underline=2;
          else {
            MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown underline block %s\n", child.m_string.cstr()));
            s << "##underline=" << child.m_string.cstr() << ",";
          }
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some underline block\n"));
        s << "###underline=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x147403a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Code && child.m_fileType==0x8d000) {
          if (child.m_string=="----") // checkme
            m_caps=0;
          else if (child.m_string=="alcp")
            m_caps=1;
          else if (child.m_string=="lowc")
            m_caps=2;
          else if (child.m_string=="Icas")
            m_caps=3;
          else {
            MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown caps block %s\n", child.m_string.cstr()));
            s << "##caps=" << child.m_string.cstr() << ",";
          }
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some caps block\n"));
        s << "###caps=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x14753aa: // min spacing
    case 0x14753ca: // optimal spacing
    case 0x14753ea: { // max spacing
      int wh=field.m_fileType==0x14753aa ? 2 : field.m_fileType==0x14753ca ? 1 : 3;
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Double && child.m_fileType==0xb6000) {
          m_letterSpacings[wh]=child.m_doubleValue;
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown spacings[%d] block\n", wh));
        s << "###spacings[" << wh << "]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    }

    case 0x147404a: // space scaling
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_LongDouble && child.m_fileType==0x149c940) {
          m_letterSpacings[0]=child.m_doubleValue;
          // no sure what do to about this int : a number between 0 and 256...
          if (child.m_longValue[0]) s << "[" << child.m_longValue[0] << "],";
          else s << ",";
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown space scaling block\n"));
        s << "###space[scaling]=" << child << ",";
      }
      m_extra+=s.str();
      return true;

    case 0x147405a: // script position
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_LongDouble && child.m_fileType==0x149c940) {
          m_scriptPosition=(float) child.m_doubleValue;
          if ((child.m_doubleValue<0 || child.m_doubleValue>0) && m_fontScaling<0)
            m_fontScaling=0.75;
          // no sure what do to about this int : a number between 0 and 256...
          if (child.m_longValue[0]) s << "script2[pos]?=" << child.m_longValue[0] << ",";
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown font script block\n"));
        s << "###font[script]=" << child << ",";
      }
      m_extra+=s.str();
      return true;

    case 0x14741ba:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Double && child.m_fileType==0xb6000) {
          m_fontScaling=(float) child.m_doubleValue;
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown font scaling block\n"));
        s << "###scaling=" << child << ",";
      }
      m_extra+=s.str();
      return true;

    case 0x14740ea: // horizontal streching
    case 0x147418a: // small cap horizontal scaling
    case 0x14741aa: { // small cap vertical scaling
      std::string wh(field.m_fileType==0x14740ea ? "font[strech]" :
                     field.m_fileType==0x147418a ? "font[smallScaleH]" : "font[smallScaleV]");
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Double && child.m_fileType==0xb6000) {
          if (field.m_fileType==0x14740ea)
            m_widthStreching=child.m_doubleValue;
          else
            s << wh << "=" << child.m_doubleValue << ",";
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown %s block\n", wh.c_str()));
        s << "###" << wh << "=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    }
    case 0x147406a: // automatic hyphenation
    case 0x147552a: { // ignore 1 word ( for spacings )
      std::string wh(field.m_fileType==0x147406a ? "hyphen" : "spacings[ignore1Word]");
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Bool && child.m_fileType==0x360c0) {
          if (child.m_longValue[0])
            s << wh << ",";
          else
            s << wh << "=no,";
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown %s block\n", wh.c_str()));
        s << "###" << wh << "=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    }
    case 0x147402a: // language
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Long && child.m_fileType==0x34080) {
          m_language=(int) child.m_longValue[0];
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown language block\n"));
        s << "###language=" << child << ",";
      }
      m_extra+=s.str();
      return true;

    //
    // columns
    //
    case 0x147512a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Long && child.m_fileType==0x328c0) {
          m_numColumns=(int) child.m_longValue[0];
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown column's number block\n"));
        s << "###num[cols]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x147513a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Double && child.m_fileType==0x1493800) {
          m_columnGap=child.m_doubleValue;
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StyleManager::TextStyle::read: find some unknown columns gaps block\n"));
        s << "###col[gap]=" << child << ",";
      }
      m_extra+=s.str();
      return true;

    default:
      break;
    }
  }
  return false;
}

void RagTime5StyleManager::TextStyle::insert(RagTime5StyleManager::TextStyle const &child)
{
  if (!child.m_linkIdList.empty()) m_linkIdList=child.m_linkIdList; // usefull?
  if (child.m_graphStyleId>=0) m_graphStyleId=child.m_graphStyleId;
  if (child.m_graphLineStyleId>=0) m_graphLineStyleId=child.m_graphLineStyleId;
  if (child.m_dateStyleId>=0) m_dateStyleId=child.m_dateStyleId;
  if (child.m_keepWithNext.isSet()) m_keepWithNext=child.m_keepWithNext;
  if (child.m_justify>=0) m_justify=child.m_justify;
  if (child.m_breakMethod>=0) m_breakMethod=child.m_breakMethod;
  for (int i=0; i<3; ++i) {
    if (child.m_margins[i]>=0) m_margins[i]=child.m_margins[i];
  }
  for (int i=0; i<3; ++i) {
    if (child.m_spacings[i]<0) continue;
    m_spacings[i]=child.m_spacings[i];
    m_spacingUnits[i]=child.m_spacingUnits[i];
  }
  if (!child.m_tabList.empty()) m_tabList=child.m_tabList; // append ?
  // char
  if (!child.m_fontName.empty()) m_fontName=child.m_fontName;
  if (child.m_fontId>=0) m_fontId=child.m_fontId;
  if (child.m_fontSize>=0) m_fontSize=child.m_fontSize;
  for (int i=0; i<2; ++i) {
    uint32_t fl=child.m_fontFlags[i];
    if (!fl) continue;
    if (i==0) m_fontFlags[0]|=fl;
    else m_fontFlags[0]&=(~fl);
  }
  if (child.m_caps>=0) m_caps=child.m_caps;
  if (child.m_underline>=0) m_underline=child.m_underline;
  if (child.m_scriptPosition.isSet()) m_scriptPosition=child.m_scriptPosition;
  if (child.m_fontScaling>=0) m_fontScaling=child.m_fontScaling;

  for (int i=0; i<4; ++i) {
    if (child.m_letterSpacings[i]>0 || child.m_letterSpacings[i]<0)
      m_letterSpacings[i]=child.m_letterSpacings[i];
  }
  if (child.m_language>=0) m_language=child.m_language;
  if (child.m_widthStreching>=0) m_widthStreching=child.m_widthStreching;
  // column
  if (child.m_numColumns>=0) m_numColumns=child.m_numColumns;
  if (child.m_columnGap>=0) m_columnGap=child.m_columnGap;
}

std::ostream &operator<<(std::ostream &o, RagTime5StyleManager::TextStyle const &style)
{
  if (style.m_parentId[0]>=0) o << "parent=TS" << style.m_parentId[0] << ",";
  if (style.m_parentId[1]>=0) o << "parent[style?]=TS" << style.m_parentId[1] << ",";
  if (!style.m_linkIdList.empty()) {
    // fixme: 3 text style's id values with unknown meaning, probably important...
    o << "link=[";
    for (size_t i=0; i<style.m_linkIdList.size(); ++i)
      o << "TS" << style.m_linkIdList[i] << ",";
    o << "],";
  }
  if (style.m_graphStyleId>=0) o << "graph[id]=GS" << style.m_graphStyleId << ",";
  if (style.m_graphLineStyleId>=0) o << "graphLine[id]=GS" << style.m_graphLineStyleId << ",";
  if (style.m_dateStyleId>=0) o << "date[id]=DS" << style.m_dateStyleId << ",";
  if (style.m_keepWithNext.isSet()) {
    o << "keep[withNext]";
    if (!*style.m_keepWithNext)
      o << "=false,";
    else
      o << ",";
  }
  switch (style.m_justify) {
  case 0: // left
    break;
  case 1:
    o << "justify=center,";
    break;
  case 2:
    o << "justify=right,";
    break;
  case 3:
    o << "justify=full,";
    break;
  case 4:
    o << "justify=full[all],";
    break;
  default:
    if (style.m_justify>=0)
      o << "##justify=" << style.m_justify << ",";
  }

  switch (style.m_breakMethod) {
  case 0: // as is
    break;
  case 1:
    o << "break[method]=next[container],";
    break;
  case 2:
    o << "break[method]=next[page],";
    break;
  case 3:
    o << "break[method]=next[evenP],";
    break;
  case 4:
    o << "break[method]=next[oddP],";
    break;
  default:
    if (style.m_breakMethod>=0)
      o << "##break[method]=" << style.m_breakMethod << ",";
  }
  for (int i=0; i<3; ++i) {
    if (style.m_margins[i]<0) continue;
    static char const *(wh[])= {"left", "right", "first"};
    o << "margins[" << wh[i] << "]=" << style.m_margins[i] << ",";
  }
  for (int i=0; i<3; ++i) {
    if (style.m_spacings[i]<0) continue;
    o << (i==0 ? "interline" : i==1 ? "before[spacing]" : "after[spacing]");
    o << "=" << style.m_spacings[i];
    if (style.m_spacingUnits[i]==0)
      o << "%";
    else if (style.m_spacingUnits[i]==1)
      o << "pt";
    else
      o << "[###unit]=" << style.m_spacingUnits[i];
    o << ",";
  }
  if (!style.m_tabList.empty()) {
    o << "tabs=[";
    for (size_t i=0; i<style.m_tabList.size(); ++i)
      o << style.m_tabList[i] << ",";
    o << "],";
  }
  // char
  if (!style.m_fontName.empty())
    o << "font=\"" << style.m_fontName.cstr() << "\",";
  if (style.m_fontId>=0)
    o << "id[font]=" << style.m_fontId << ",";
  if (style.m_fontSize>=0)
    o << "sz[font]=" << style.m_fontSize << ",";
  for (int i=0; i<2; ++i) {
    uint32_t fl=style.m_fontFlags[i];
    if (!fl) continue;
    if (i==1)
      o << "flag[rm]=[";
    if (fl&1) o << "bold,";
    if (fl&2) o << "it,";
    // 4 underline?
    if (fl&8) o << "outline,";
    if (fl&0x10) o << "shadow,";
    if (fl&0x200) o << "strike[through],";
    if (fl&0x400) o << "small[caps],";
    if (fl&0x800) o << "kumoraru,"; // ie. with some char overlapping
    if (fl&0x20000) o << "underline[word],";
    if (fl&0x80000) o << "key[pairing],";
    fl &= 0xFFF5F1E4;
    if (fl) o << "#fontFlags=" << std::hex << fl << std::dec << ",";
    if (i==1)
      o << "],";
  }
  switch (style.m_caps) {
  case 0:
    break;
  case 1:
    o << "upper[caps],";
    break;
  case 2:
    o << "lower[caps],";
    break;
  case 3:
    o << "upper[initial+...],";
    break;
  default:
    if (style.m_caps >= 0)
      o << "###caps=" << style.m_caps << ",";
    break;
  }
  switch (style.m_underline) {
  case 0:
    break;
  case 1:
    o << "underline=single,";
    break;
  case 2:
    o << "underline=double,";
    break;
  default:
    if (style.m_underline>=0)
      o << "###underline=" << style.m_underline << ",";
  }
  if (style.m_scriptPosition.isSet())
    o << "ypos[font]=" << *style.m_scriptPosition << "%,";
  if (style.m_fontScaling>=0)
    o << "scale[font]=" << style.m_fontScaling << "%,";

  for (int i=0; i<4; ++i) {
    if (style.m_letterSpacings[i]<=0&&style.m_letterSpacings[i]>=0) continue;
    static char const *(wh[])= {"", "[unkn]", "[min]", "[max]"};
    o << "letterSpacing" << wh[i] << "=" << style.m_letterSpacings[i] << ",";
  }
  if (style.m_widthStreching>=0)
    o << "width[streching]=" << style.m_widthStreching*100 << "%,";
  if (style.m_language>0) {
    std::string lang=RagTime5StyleManager::TextStyle::getLanguageLocale(style.m_language);
    if (!lang.empty())
      o << lang << ",";
    else
      o << "##language=" << std::hex << style.m_language << std::dec << ",";
  }
  // column
  if (style.m_numColumns>=0)
    o << "num[col]=" << style.m_numColumns << ",";
  if (style.m_columnGap>=0)
    o << "col[gap]=" << style.m_columnGap << ",";
  o << style.m_extra;
  return o;
}


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
