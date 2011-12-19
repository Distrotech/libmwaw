/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
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

/* This header contains code specific to a pict mac file
 */
#include <string.h>

#include <stack>

#include "libmwaw_tools.hxx"

#include <libwpd/WPXBinaryData.h>
#include <libwpd/WPXProperty.h>
#include <libwpd/WPXPropertyList.h>
#include <libwpd/WPXString.h>
#include <libwpd-stream/WPXStream.h>

#include "TMWAWPropertyHandler.hxx"

////////////////////////////////////////////////////
//
// TMWAWPropertyHandlerEncoder
//
////////////////////////////////////////////////////
TMWAWPropertyHandlerEncoder::TMWAWPropertyHandlerEncoder()
  : m_f(std::ios::in | std::ios::out | std::ios::binary)
{
}

void TMWAWPropertyHandlerEncoder::startElement
(const char *psName, const WPXPropertyList &xPropList)
{
  m_f << 'S';
  writeString(psName);
  WPXPropertyList::Iter i(xPropList);
  int numElt = 0;
  for (i.rewind(); i.next(); ) numElt++;
  writeInteger(numElt);
  for (i.rewind(); i.next(); ) {
    writeString(i.key());
    writeString(i()->getStr().cstr());
  }
}
void TMWAWPropertyHandlerEncoder::endElement(const char *psName)
{
  m_f << 'E';
  writeString(psName);
}

void TMWAWPropertyHandlerEncoder::characters(std::string const &sCharacters)
{
  if (sCharacters.length()==0) return;
  WPXString str(sCharacters.c_str());
  WPXString escaped(str,true);
  if (escaped.len() == 0) return;
  m_f << 'T';
  writeString(sCharacters.c_str());
}

void TMWAWPropertyHandlerEncoder::writeString(const char *name)
{
  int sz = (name == 0L) ? 0 : strlen(name);
  writeInteger(sz);
  if (sz) m_f.write(name, sz);
}

void TMWAWPropertyHandlerEncoder::writeInteger(int val)
{
  m_f.write((char const *)&val, sizeof(int));
}

bool TMWAWPropertyHandlerEncoder::getData(WPXBinaryData &data)
{
  data.clear();
  std::string d=m_f.str();
  if (d.length() == 0) return false;
  data.append((const unsigned char*)d.c_str(), d.length());
  return true;
}

/*! \brief Internal: the property decoder
 *
 * In order to be read by writerperfect, we must code document consisting in
 * tag and propertyList in an intermediar format:
 *  - [string:s]: an int length(s) follow by the length(s) characters of string s
 *  - [startElement:name proplist:prop]:
 *          char 'S', [string] name, int \#properties, 2\#prop*[string]
 *           (ie. \#prop sequence of ([string] key, [string] value) )
 *  - [endElement:name ]: char 'E',  [string] name
 *  - [characters:s ]: char 'T', [string] s
 *            - if len(s)==0, we write nothing
 *            - the string is written as is (ie. we do not escaped any characters).
*/
class TMWAWPropertyHandlerDecoder
{
public:
  //! constructor given a TMWAWPropertyHandler
  TMWAWPropertyHandlerDecoder(TMWAWPropertyHandler *hdl=0L):handler(hdl), openTag() {}

  //! tries to read the data
  bool readData(WPXBinaryData const &encoded) {
    try {
      WPXInputStream *inp = (WPXInputStream *)encoded.getDataStream();
      if (!inp) return false;

      while (!inp->atEOS()) {
        unsigned const char *c;
        unsigned long numRead;

        c = inp->read(1,numRead);
        if (!c || numRead != 1) {
          MWAW_DEBUG_MSG(("TMWAWPropertyHandlerDecoder: can not read data type \n"));
          return false;
        }
        switch(*c) {
        case 'S':
          if (!readStartElement(*inp)) return false;
          break;
        case 'E':
          if (!readEndElement(*inp)) return false;
          break;
        case 'T':
          if (!readCharacters(*inp)) return false;
          break;
        default:
          MWAW_DEBUG_MSG(("TMWAWPropertyHandlerDecoder: unknown type='%c' \n", *c));
          return false;
        }
      }
    } catch(...) {
      return false;
    }
    return true;
  }

protected:
  //! reads an startElement
  bool readStartElement(WPXInputStream &input) {
    std::string s;
    if (!readString(input, s)) return false;

    int numElt;
    if (!readInteger(input, numElt)) return false;

    if (s.empty() || numElt < 0) {
      MWAW_DEBUG_MSG(("TMWAWPropertyHandlerDecoder::readStartElement: can not read tag %s or numElt=%d\n",
                      s.c_str(), numElt));
      return false;
    }
    WPXPropertyList lists;
    for (int i = 0; i < numElt; i++) {
      if (readProperty(input, lists)) continue;
      MWAW_DEBUG_MSG(("TMWAWPropertyHandlerDecoder::readStartElement: can not read property for tag %s\n",
                      s.c_str()));
      return false;
    }
    openTag.push(s);

    if (handler) handler->startElement(s.c_str(), lists);
    return true;
  }

  //! reads an endElement
  bool readEndElement(WPXInputStream &input) {
    std::string s;
    if (!readString(input, s)) return false;

    if (s.empty()) {
      MWAW_DEBUG_MSG(("TMWAWPropertyHandlerDecoder::readEndElement find empty tag\n"));
      return false;
    }
    // check stack
    if (openTag.empty()) {
      MWAW_DEBUG_MSG(("TMWAWPropertyHandlerDecoder::readEndElement %s with no openElement\n",
                      s.c_str()));
      return false;
    }
    if (openTag.top() != s) {
      MWAW_DEBUG_MSG(("TMWAWPropertyHandlerDecoder::readEndElement %s but last open %s\n",
                      openTag.top().c_str(), s.c_str()));
      return false;
    }
    openTag.pop();
    if (handler) handler->endElement(s.c_str());
    return true;
  }

  //! reads a set of characters
  bool readCharacters(WPXInputStream &input) {
    std::string s;
    if (!readString(input, s)) return false;
    if (!s.length()) return true;
    if (handler) handler->characters(WPXString(s.c_str()));
    return true;
  }

  //
  // low level
  //

  //! low level: reads a property and its value, adds it to \a list
  bool readProperty(WPXInputStream &input, WPXPropertyList &list) {
    std::string key, val;
    if (!readString(input, key)) return false;
    if (!readString(input, val)) return false;
    list.insert(key.c_str(), val.c_str());
    return true;
  }

  //! low level: reads a string : size and string
  bool readString(WPXInputStream &input, std::string &s) {
    int numC = 0;
    if (!readInteger(input, numC)) return false;
    if (numC==0) {
      s = std::string("");
      return true;
    }
    unsigned long numRead;
    const unsigned char *dt = input.read(numC, numRead);
    if (dt == 0L || numRead != (unsigned long) numC) {
      MWAW_DEBUG_MSG(("TMWAWPropertyHandlerDecoder::readInteger: can not read a string\n"));
      return false;
    }
    s = std::string((const char *)dt, numC);
    return true;
  }

  //! low level: reads an integer value
  bool readInteger(WPXInputStream &input, int &val) {
    unsigned long numRead = 0;
    const unsigned char *dt = input.read(sizeof(int), numRead);
    if (dt == 0L || numRead != sizeof(int)) {
      MWAW_DEBUG_MSG(("TMWAWPropertyHandlerDecoder::readInteger: can not read int\n"));
      return false;
    }
    val = *(int *) dt;
    return true;
  }
private:
  TMWAWPropertyHandlerDecoder(TMWAWPropertyHandlerDecoder const &orig) : handler(), openTag() {
    *this = orig;
  }

  TMWAWPropertyHandlerDecoder &operator=(TMWAWPropertyHandlerDecoder const &) {
    MWAW_DEBUG_MSG(("TMWAWPropertyHandlerDecoder::operator=: MUST NOT BE CALLED\n"));
    return *this;
  }
protected:
  //! the streamfile
  TMWAWPropertyHandler *handler;

  //! the list of open tags
  std::stack<std::string> openTag;
};

////////////////////////////////////////////////////
//
// TMWAWPropertyHandler
//
////////////////////////////////////////////////////
bool TMWAWPropertyHandler::checkData(WPXBinaryData const &encoded)
{
  TMWAWPropertyHandlerDecoder decod;
  return decod.readData(encoded);
}

bool TMWAWPropertyHandler::readData(WPXBinaryData const &encoded)
{
  TMWAWPropertyHandlerDecoder decod(this);
  return decod.readData(encoded);
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
