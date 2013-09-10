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

/* This header contains code specific to manage basic picture (line, rectangle, ...)
 *
 * Note: all unit are points
 *
 */

#ifndef MWAW_PICT_BASIC
#  define MWAW_PICT_BASIC

#  include <assert.h>
#  include <cstring>
#  include <map>
#  include <ostream>
#  include <set>
#  include <string>
#  include <vector>

#  include "libwpd/libwpd.h"
#  include "libmwaw_internal.hxx"

#  include "MWAWGraphicShape.hxx"
#  include "MWAWGraphicStyle.hxx"

#  include "MWAWPict.hxx"

class MWAWPropertyHandlerEncoder;

/** \brief an abstract class which defines basic picture (a line, a rectangle, ...) */
class MWAWPictBasic: public MWAWPict
{
public:
  //! virtual destructor
  virtual ~MWAWPictBasic() {}

  //! the picture subtype ( line, rectangle, polygon, circle, arc)
  enum SubType { GraphicObject, Group, Shape, Text };
  //! returns the picture type
  virtual Type getType() const {
    return Basic;
  }
  //! returns the picture subtype
  virtual SubType getSubType() const = 0;

  //! returns a ODG (encoded)
  virtual bool getODGBinary(WPXBinaryData &res) const;

  //! returns the current style
  MWAWGraphicStyle const &getStyle() const {
    return m_style;
  }
  //! set the current style
  void setStyle(MWAWGraphicStyle const &style) {
    m_style = style;
    updateBdBox();
  }
  //! return the layer
  int getLayer() const {
    return m_layer;
  }
  //! set the layer
  void setLayer(int layer) {
    m_layer=layer;
  }
  /** returns the final representation in encoded odg (if possible) */
  virtual bool getBinary(WPXBinaryData &data, std::string &s) const {
    if (!getODGBinary(data)) return false;
    s = "image/mwaw-odg";
    return true;
  }
  //! returns a ODG (low level)
  virtual bool send(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const=0;

  /** a virtual function used to obtain a strict order.
   * -  must be redefined in the subs class
   */
  virtual int cmp(MWAWPict const &a) const {
    int diff = MWAWPict::cmp(a);
    if (diff) return diff;

    MWAWPictBasic const &aPict = static_cast<MWAWPictBasic const &>(a);
    if (m_layer < aPict.m_layer) return -1;
    if (m_layer > aPict.m_layer) return 1;

    // the type
    diff = getSubType() - aPict.getSubType();
    if (diff) return (diff < 0) ? -1 : 1;
    diff = m_style.cmp(aPict.m_style);
    if (diff) return diff;
    for (int c = 0; c < 2; c++) {
      float diffF = m_extend[c]-aPict.m_extend[c];
      if (diffF < 0) return -1;
      if (diffF > 0) return 1;
    }
    return 0;
  }

protected:
  //! update the bdbox if needed, must be called if lineWidth or arrows change
  void updateBdBox() {
    extendBDBox(m_style.m_lineWidth, 0);
    extendBDBox((m_style.m_arrows[0] || m_style.m_arrows[1]) ? 5 : 0, 1);
  }

  //! function to implement in subclass in order to get the graphics style
  virtual void getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const = 0;

  //! adds the odg header knowing the minPt and the maxPt
  virtual void startODG(MWAWPropertyHandlerEncoder &doc) const;
  //! adds the current style
  virtual void sendStyle(MWAWPropertyHandlerEncoder &doc) const;
  //! adds the odg footer
  virtual void endODG(MWAWPropertyHandlerEncoder &doc) const;

  //! a function to extend the bdbox
  // - \param id=0 corresponds to linewidth
  // - \param id=1 corresponds to a second extension (arrow)
  void extendBDBox(float val, int id) {
    assert(id>=0&& id<=1);
    m_extend[id] = val;
    MWAWPict::extendBDBox(m_extend[0]+m_extend[1]);
  }

  //! protected constructor must not be called directly
  MWAWPictBasic(MWAWGraphicStyleManager &graphicManager) : MWAWPict(), m_graphicManager(graphicManager), m_layer(-1000), m_style() {
    for (int c = 0; c < 2; c++) m_extend[c]=0;
    updateBdBox();
  }
  //! protected constructor must not be called directly
  MWAWPictBasic(MWAWPictBasic const &p) : MWAWPict(), m_graphicManager(p.m_graphicManager), m_layer(-1000), m_style() {
    *this=p;
  }
  //! protected= must not be called directly
  MWAWPictBasic &operator=(MWAWPictBasic const &p) {
    if (&p == this) return *this;
    MWAWPict::operator=(p);
    m_layer = p.m_layer;
    m_style = p.m_style;
    for (int c=0; c < 2; c++) m_extend[c] = p.m_extend[c];
    return *this;
  }

protected:
  //! the graphic style manager
  MWAWGraphicStyleManager &m_graphicManager;
  //! the layer number if know
  int m_layer;
  //! the data style
  MWAWGraphicStyle m_style;
  //! m_extend[0]: from lineWidth, m_extend[1]: came from extra data
  float m_extend[2];
};


//! a class used to a pict based on a MWAWGraphicShape
class MWAWPictShape : public MWAWPictBasic
{
public:
  //! constructor
  MWAWPictShape(MWAWGraphicStyleManager &graphicManager, MWAWGraphicShape const &shape,  MWAWGraphicStyle const &style);
  //! virtual destructor
  virtual ~MWAWPictShape() {}

  //! returns a ODG (encoded)
  virtual bool send(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const;

protected:
  //! returns the class type
  virtual SubType getSubType() const {
    return Shape;
  }
  //! returns the graphics style
  virtual void getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const;
  //! comparison function
  virtual int cmp(MWAWPict const &a) const;

  // the shape
  MWAWGraphicShape m_shape;
};

/** a class used to define a small text zone */
class MWAWPictSimpleText : public MWAWPictBasic
{
public:
  /** constructor: bdbox followed by the set of vertices */
  MWAWPictSimpleText(MWAWGraphicStyleManager &graphicManager, Box2f bdBox);
  //! virtual destructor
  virtual ~MWAWPictSimpleText();

  // -- text data
  /** insert a character using the font converter to find the utf8
      character */
  void insertCharacter(unsigned char c);
  //! adds a unicode string
  void insertUnicodeString(WPXString const &str);

  //! insert a tab
  void insertTab();
  //! insert a eol
  void insertEOL();

  // ------- fields ----------------
  //! adds a field type
  void insertField(MWAWField const &field);

  // ------- font and paragraph ----
  //! set the font
  void setFont(MWAWFont const &font);
  //! set the paragraph
  void setParagraph(MWAWParagraph const &paragraph);

  //! set the text padding
  void setPadding(Vec2f const &LT, Vec2f const &RB) {
    m_LTPadding = LT;
    m_RBPadding = RB;
  }
  //! returns a ODG (encoded)
  virtual bool send(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const;
protected:
  //! returns the class type
  virtual SubType getSubType() const {
    return Text;
  }
  //! returns the graphics style
  virtual void getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const;
  //! comparison function
  virtual int cmp(MWAWPict const &a) const;

  //! the left top padding
  Vec2f m_LTPadding;
  //! the right bottom padding
  Vec2f m_RBPadding;
  //! the text buffer
  WPXString m_textBuffer;
  //! the position where a line break happens
  std::set<int> m_lineBreakSet;
  //! the actual font id
  int m_fontId;
  //! a map pos to font
  std::map<int,MWAWFont> m_posFontMap;
  //! a map pos to paragraph
  std::map<int,MWAWParagraph> m_posParagraphMap;
};

//! a class used to define a graphicObject or an ellipse
class MWAWPictGraphicObject : public MWAWPictBasic
{
public:
  //! constructor
  MWAWPictGraphicObject(MWAWGraphicStyleManager &graphicManager, Box2f box, WPXBinaryData const &data, std::string mimeType) :
    MWAWPictBasic(graphicManager), m_data(data), m_mimeType(mimeType) {
    setBdBox(box);
  }
  //! virtual destructor
  virtual ~MWAWPictGraphicObject() {}

  //! returns a ODG (encoded)
  virtual bool send(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const;

protected:
  //! returns the class type
  virtual SubType getSubType() const {
    return GraphicObject;
  }
  //! returns the graphics style
  virtual void getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const;
  //! comparison function
  virtual int cmp(MWAWPict const &a) const;

  // the binary data
  WPXBinaryData m_data;
  // the mime type
  std::string m_mimeType;
};

//! \brief a class used to define a polygon
class MWAWPictGroup : public MWAWPictBasic
{
public:
  /** constructor: */
  MWAWPictGroup(MWAWGraphicStyleManager &graphicManager, Box2f box) : MWAWPictBasic(graphicManager), m_autoBdBox(false), m_child() {
    setBdBox(box);
  }
  /** constructor: with automatic box computation*/
  MWAWPictGroup(MWAWGraphicStyleManager &graphicManager) : MWAWPictBasic(graphicManager), m_autoBdBox(true), m_child() {
  }
  //! virtual destructor
  virtual ~MWAWPictGroup() {}

  //! returns a ODG (encoded)
  virtual bool send(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const;

  //! add a new child
  void addChild(shared_ptr<MWAWPictBasic> child);
protected:
  //! returns the class type
  virtual SubType getSubType() const {
    return Group;
  }
  //! returns the graphics style
  virtual void getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const;
  //! comparison function
  virtual int cmp(MWAWPict const &a) const {
    int diff = MWAWPictBasic::cmp(a);
    if (diff) return diff;
    MWAWPictGroup const &aGroup = static_cast<MWAWPictGroup const &>(a);
    if (m_child.size()<aGroup.m_child.size())
      return -1;
    if (m_child.size()>aGroup.m_child.size())
      return 1;

    // check the vertices
    for (size_t c = 0; c < m_child.size(); c++) {
      if (!m_child[c]) {
        if (aGroup.m_child[c]) return -1;
        continue;
      }
      if (!aGroup.m_child[c]) return 1;
      diff = m_child[c]->cmp(*aGroup.m_child[c]);
      if (diff) return diff;
    }
    return 0;
  }
  //! compute automatically the bdbox
  bool m_autoBdBox;
  //! the vertices list
  std::vector<shared_ptr<MWAWPictBasic> > m_child;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
