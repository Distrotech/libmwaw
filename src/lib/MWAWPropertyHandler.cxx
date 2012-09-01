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

/* This header contains code specific to a small picture
 */
#include <string.h>

#include <stack>

#include "libmwaw_internal.hxx"

#include <libwpd/WPXBinaryData.h>
#include <libwpd/WPXProperty.h>
#include <libwpd/WPXPropertyList.h>
#include <libwpd/WPXString.h>
#include <libwpd-stream/WPXStream.h>

#include "MWAWPropertyHandler.hxx"

////////////////////////////////////////////////////
//
// MWAWPropertyHandlerEncoder
//
////////////////////////////////////////////////////
MWAWPropertyHandlerEncoder::MWAWPropertyHandlerEncoder()
  : m_f(std::ios::in | std::ios::out | std::ios::binary)
{
}

void MWAWPropertyHandlerEncoder::startElement
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
void MWAWPropertyHandlerEncoder::endElement(const char *psName)
{
  m_f << 'E';
  writeString(psName);
}

void MWAWPropertyHandlerEncoder::characters(std::string const &sCharacters)
{
  if (sCharacters.length()==0) return;
  WPXString str(sCharacters.c_str());
  WPXString escaped(str,true);
  if (escaped.len() == 0) return;
  m_f << 'T';
  writeString(sCharacters.c_str());
}

void MWAWPropertyHandlerEncoder::writeString(const char *name)
{
  int sz = (name == 0L) ? 0 : int(strlen(name));
  writeInteger(sz);
  if (sz) m_f.write(name, sz);
}

void MWAWPropertyHandlerEncoder::writeInteger(int val)
{
  int32_t value=(int32_t) val;
  unsigned char const allValue[]= {(unsigned char)(value&0xFF), (unsigned char)((value>>8)&0xFF), (unsigned char)((value>>16)&0xFF), (unsigned char)((value>>24)&0xFF)};
  m_f.write((const char *)allValue, 4);
}

bool MWAWPropertyHandlerEncoder::getData(WPXBinaryData &data)
{
  data.clear();
  std::string d=m_f.str();
  if (d.length() == 0) return false;
  data.append((const unsigned char *)d.c_str(), d.length());
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
class MWAWPropertyHandlerDecoder
{
public:
  //! constructor given a MWAWPropertyHandler
  MWAWPropertyHandlerDecoder(MWAWPropertyHandler *hdl=0L):m_handler(hdl), m_openTag() {}

  //! tries to read the data
  bool readData(WPXBinaryData const &encoded) {
    try {
      WPXInputStream *inp = const_cast<WPXInputStream *>(encoded.getDataStream());
      if (!inp) return false;

      while (!inp->atEOS()) {
        unsigned const char *c;
        unsigned long numRead;

        c = inp->read(1,numRead);
        if (!c || numRead != 1) {
          MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder: can not read data type \n"));
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
          MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder: unknown type='%c' \n", *c));
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
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readStartElement: can not read tag %s or numElt=%d\n",
                      s.c_str(), numElt));
      return false;
    }
    WPXPropertyList lists;
    for (int i = 0; i < numElt; i++) {
      if (readProperty(input, lists)) continue;
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readStartElement: can not read property for tag %s\n",
                      s.c_str()));
      return false;
    }
    m_openTag.push(s);

    if (m_handler) m_handler->startElement(s.c_str(), lists);
    return true;
  }

  //! reads an endElement
  bool readEndElement(WPXInputStream &input) {
    std::string s;
    if (!readString(input, s)) return false;

    if (s.empty()) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readEndElement find empty tag\n"));
      return false;
    }
    // check stack
    if (m_openTag.empty()) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readEndElement %s with no openElement\n",
                      s.c_str()));
      return false;
    }
    if (m_openTag.top() != s) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readEndElement %s but last open %s\n",
                      m_openTag.top().c_str(), s.c_str()));
      return false;
    }
    m_openTag.pop();
    if (m_handler) m_handler->endElement(s.c_str());
    return true;
  }

  //! reads a set of characters
  bool readCharacters(WPXInputStream &input) {
    std::string s;
    if (!readString(input, s)) return false;
    if (!s.length()) return true;
    if (m_handler) m_handler->characters(WPXString(s.c_str()));
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
    const unsigned char *dt = input.read((unsigned long)numC, numRead);
    if (dt == 0L || numRead != (unsigned long) numC) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInteger: can not read a string\n"));
      return false;
    }
    s = std::string((const char *)dt, size_t(numC));
    return true;
  }

  //! low level: reads an integer value
  bool readInteger(WPXInputStream &input, int &val) {
    unsigned long numRead = 0;
    const unsigned char *dt = input.read(4, numRead);
    if (dt == 0L || numRead != 4) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInteger: can not read int\n"));
      return false;
    }
    val = int((dt[3]<<16)|(dt[2]<<16)|(dt[1]<<8)|dt[0]);
    return true;
  }
private:
  MWAWPropertyHandlerDecoder(MWAWPropertyHandlerDecoder const &orig);
  MWAWPropertyHandlerDecoder &operator=(MWAWPropertyHandlerDecoder const &);

protected:
  //! the streamfile
  MWAWPropertyHandler *m_handler;

  //! the list of open tags
  std::stack<std::string> m_openTag;
};

////////////////////////////////////////////////////
//
// MWAWPropertyHandler
//
////////////////////////////////////////////////////
bool MWAWPropertyHandler::checkData(WPXBinaryData const &encoded)
{
  MWAWPropertyHandlerDecoder decod;
  return decod.readData(encoded);
}

bool MWAWPropertyHandler::readData(WPXBinaryData const &encoded)
{
  MWAWPropertyHandlerDecoder decod(this);
  return decod.readData(encoded);
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
