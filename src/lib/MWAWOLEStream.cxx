/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* POLE - Portable C++ library to access OLE Storage
   Copyright (C) 2002-2005 Ariya Hidayat <ariya@kde.org>

   Performance optimization: Dmitry Fedorov
   Copyright 2009 <www.bioimage.ucsb.edu> <www.dimin.net>

   Fix for more than 236 mbat block entries : Michel Boudinot
   Copyright 2010 <Michel.Boudinot@inaf.cnrs-gif.fr>

   Version: 0.4

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   * Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   * Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   * Neither the name of the authors nor the names of its contributors may be
     used to endorse or promote products derived from this software without
     specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
   THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 This file taken from libwpd WPXOLEStream.cpp
*/

#include <cstring>
#include <iostream>
#include <list>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "libwpd-stream/WPXStream.h"

#include "libmwaw_internal.hxx"
#include "MWAWOLEStream.hxx"

namespace libmwaw_internal
{
/** an internal class used to return the OLE InputStream */
class MWAWStringStream: public WPXInputStream
{
public:
  MWAWStringStream(const unsigned char *data, const unsigned int dataSize) :
    buffer(dataSize), offset(0) {
    memcpy(&buffer[0], data, dataSize);
  }
  ~MWAWStringStream() { }

  const unsigned char *read(unsigned long numBytes, unsigned long &numBytesRead);
  long tell() {
    return offset;
  }
  int seek(long offset, WPX_SEEK_TYPE seekType);
  bool atEOS() {
    return ((long)offset >= (long)buffer.size());
  }

  bool isOLEStream() {
    return false;
  }
  WPXInputStream *getDocumentOLEStream(const char *) {
    return 0;
  };

private:
  std::vector<unsigned char> buffer;
  volatile long offset;
  MWAWStringStream(const MWAWStringStream &);
  MWAWStringStream &operator=(const MWAWStringStream &);
};

int MWAWStringStream::seek(long _offset, WPX_SEEK_TYPE seekType)
{
  if (seekType == WPX_SEEK_CUR)
    offset += _offset;
  else if (seekType == WPX_SEEK_SET)
    offset = _offset;

  if (offset < 0) {
    offset = 0;
    return 1;
  }
  if ((long)offset > (long)buffer.size()) {
    offset = buffer.size();
    return 1;
  }
  return 0;
}

const unsigned char *MWAWStringStream::read(unsigned long numBytes, unsigned long &numBytesRead)
{
  numBytesRead = 0;

  if (numBytes == 0)
    return 0;

  int numBytesToRead;

  if ((offset+numBytes) < buffer.size())
    numBytesToRead = numBytes;
  else
    numBytesToRead = buffer.size() - offset;

  numBytesRead = numBytesToRead; // about as paranoid as we can be..

  if (numBytesToRead == 0)
    return 0;

  long oldOffset = offset;
  offset += numBytesToRead;

  return &buffer[oldOffset];
}
}

namespace libmwaw
{

class Header
{
public:
  unsigned char id[8];       // signature, or magic identifier
  unsigned b_shift;          // bbat->blockSize = 1 << b_shift
  unsigned s_shift;          // sbat->blockSize = 1 << s_shift
  unsigned num_bat;          // blocks allocated for big bat
  unsigned dirent_start;     // starting block for directory info
  unsigned threshold;        // switch from small to big file (usually 4K)
  unsigned sbat_start;       // starting block index to store small bat
  unsigned num_sbat;         // blocks allocated for small bat
  unsigned mbat_start;       // starting block to store meta bat
  unsigned num_mbat;         // blocks allocated for meta bat
  unsigned long bb_blocks[109];

  Header();
  bool valid();
  void load( const unsigned char *buffer, unsigned long size );
};

class AllocTable
{
public:
  static const unsigned Eof;
  static const unsigned Avail;
  static const unsigned Bat;
  static const unsigned MetaBat;
  unsigned blockSize;
  AllocTable();
  void clear();
  unsigned long count();
  void resize( unsigned long newsize );
  void set( unsigned long index, unsigned long val );
  std::vector<unsigned long> follow( unsigned long start );
  unsigned long operator[](unsigned long index );
  void load( const unsigned char *buffer, unsigned len );
private:
  std::vector<unsigned long> data;
  AllocTable( const AllocTable & );
  AllocTable &operator=( const AllocTable & );
};

class DirEntry
{
public:
  DirEntry() : valid(false), name(), dir(false), size(0), start(0),
    prev(0), next(0), child(0) {};
  bool valid;            // false if invalid (should be skipped)
  std::string name;      // the name, not in unicode anymore
  bool dir;              // true if directory
  unsigned long size;    // size (not valid if directory)
  unsigned long start;   // starting block
  unsigned prev;         // previous sibling
  unsigned next;         // next sibling
  unsigned child;        // first child
};

class DirTree
{
public:
  static const unsigned End;
  DirTree();
  void clear();
  unsigned entryCount();
  DirEntry *entry( unsigned index );
  DirEntry *entry( const std::string &name );
  unsigned find_child( unsigned index, const std::string &name );
  void load( unsigned char *buffer, unsigned len );
  std::vector<std::string> getOLENames();

private:
  void getOLENames(unsigned index, const std::string &prefix,
                   std::vector<std::string> &res, std::set<unsigned> &seen);
  std::vector<DirEntry> entries;
  DirTree( const DirTree & );
  DirTree &operator=( const DirTree & );
};

class StorageIO
{
public:
  Storage *storage;         // owner
  WPXInputStream *input;
  int result;               // result of operation

  Header *header;           // storage header
  DirTree *dirtree;         // directory tree
  AllocTable *bbat;         // allocation table for big blocks
  AllocTable *sbat;         // allocation table for small blocks

  std::vector<unsigned long> sb_blocks; // blocks for "small" files

  bool isLoadDone;

  StorageIO( Storage *storage, WPXInputStream *is );
  ~StorageIO();

  bool isOLEStream();
  void load();

  unsigned long loadBigBlocks( std::vector<unsigned long> blocks, unsigned char *buffer, unsigned long maxlen );

  unsigned long loadBigBlock( unsigned long block, unsigned char *buffer, unsigned long maxlen );

  unsigned long loadSmallBlocks( std::vector<unsigned long> blocks, unsigned char *buffer, unsigned long maxlen );

  unsigned long loadSmallBlock( unsigned long block, unsigned char *buffer, unsigned long maxlen );

  StreamIO *streamIO( const std::string &name );

  std::vector<std::string> getOLENames();
private:
  // no copy or assign
  StorageIO( const StorageIO & );
  StorageIO &operator=( const StorageIO & );

};

class StreamIO
{
public:
  StorageIO *io;
  DirEntry *entry;
  std::string fullName;
  bool eof;
  bool fail;

  StreamIO( StorageIO *io, DirEntry *entry );
  ~StreamIO();
  unsigned long size();
  unsigned long tell();
  unsigned long read( unsigned char *data, unsigned long maxlen );
  unsigned long read( unsigned long pos, unsigned char *data, unsigned long maxlen );


private:
  std::vector<unsigned long> blocks;

  // no copy or assign
  StreamIO( const StreamIO & );
  StreamIO &operator=( const StreamIO & );

  // pointer for read
  unsigned long m_pos;

  // simple cache system to speed-up getch()
  std::vector<unsigned char> cache_data;
  unsigned long cache_size;
  unsigned long cache_pos;
  void updateCache();
};

} // namespace libmwaw

static inline unsigned long readU16( const unsigned char *ptr )
{
  return ptr[0]+(ptr[1]<<8);
}

static inline unsigned long readU32( const unsigned char *ptr )
{
  return ptr[0]+(ptr[1]<<8)+(ptr[2]<<16)+(ptr[3]<<24);
}

static const unsigned char mwawole_magic[] =
{ 0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1 };


// =========== Header ==========

libmwaw::Header::Header() :
  b_shift(9),
  s_shift(6),
  num_bat(0),
  dirent_start(0),
  threshold(4096),
  sbat_start(0),
  num_sbat(0),
  mbat_start(0),
  num_mbat(0)
{
  for( unsigned i = 0; i < 8; i++ )
    id[i] = mwawole_magic[i];
  for( unsigned j=0; j<109; j++ )
    bb_blocks[j] = libmwaw::AllocTable::Avail;
}

bool libmwaw::Header::valid()
{
  if( threshold != 4096 ) return false;
  if( num_bat == 0 ) return false;
  if( (num_bat > 109) && (num_bat > (num_mbat * 127) + 109)) return false;
  if( (num_bat < 109) && (num_mbat != 0) ) return false;
  if( s_shift > b_shift ) return false;
  if( b_shift <= 6 ) return false;
  if( b_shift >=31 ) return false;

  return true;
}

void libmwaw::Header::load( const unsigned char *buffer, unsigned long size )
{
  if (size < 512)
    return;
  b_shift      = ::readU16( buffer + 0x1e );
  s_shift      = ::readU16( buffer + 0x20 );
  num_bat      = ::readU32( buffer + 0x2c );
  dirent_start = ::readU32( buffer + 0x30 );
  threshold    = ::readU32( buffer + 0x38 );
  sbat_start   = ::readU32( buffer + 0x3c );
  num_sbat     = ::readU32( buffer + 0x40 );
  mbat_start   = ::readU32( buffer + 0x44 );
  num_mbat     = ::readU32( buffer + 0x48 );

  for( unsigned i = 0; i < 8; i++ )
    id[i] = buffer[i];
  for( unsigned j=0; j<109; j++ )
    bb_blocks[j] = ::readU32( buffer + 0x4C+j*4 );
}



// =========== AllocTable ==========

const unsigned libmwaw::AllocTable::Avail = 0xffffffff;
const unsigned libmwaw::AllocTable::Eof = 0xfffffffe;
const unsigned libmwaw::AllocTable::Bat = 0xfffffffd;
const unsigned libmwaw::AllocTable::MetaBat = 0xfffffffc;

libmwaw::AllocTable::AllocTable() :
  blockSize(4096),
  data()
{
  // initial size
  resize( 128 );
}

unsigned long libmwaw::AllocTable::count()
{
  return data.size();
}

void libmwaw::AllocTable::resize( unsigned long newsize )
{
  unsigned oldsize = data.size();
  data.resize( newsize );
  if( newsize > oldsize )
    for( unsigned i = oldsize; i<newsize; i++ )
      data[i] = Avail;
}

unsigned long libmwaw::AllocTable::operator[]( unsigned long index )
{
  unsigned long result;
  result = data[index];
  return result;
}

void libmwaw::AllocTable::set( unsigned long index, unsigned long value )
{
  if( index >= count() ) resize( index + 1);
  data[ index ] = value;
}

// TODO: optimize this with better search
static bool already_exist(const std::vector<unsigned long>& chain,
                          unsigned long item)
{
  for(unsigned i = 0; i < chain.size(); i++)
    if(chain[i] == item) return true;

  return false;
}

// follow
std::vector<unsigned long> libmwaw::AllocTable::follow( unsigned long start )
{
  std::vector<unsigned long> chain;

  if( start >= count() ) return chain;

  unsigned long p = start;
  while( p < count() ) {
    if( p == (unsigned long)Eof ) break;
    if( p == (unsigned long)Bat ) break;
    if( p == (unsigned long)MetaBat ) break;
    if( already_exist(chain, p) ) break;
    chain.push_back( p );
    if( data[p] >= count() ) break;
    p = data[ p ];
  }

  return chain;
}

void libmwaw::AllocTable::load( const unsigned char *buffer, unsigned len )
{
  resize( len / 4 );
  for( unsigned i = 0; i < count(); i++ )
    set( i, ::readU32( buffer + i*4 ) );
}

// =========== DirTree ==========

const unsigned libmwaw::DirTree::End = 0xffffffff;

libmwaw::DirTree::DirTree() :
  entries()
{
  clear();
}

void libmwaw::DirTree::clear()
{
  // leave only root entry
  entries.resize( 1 );
  entries[0].valid = true;
  entries[0].name = "Root Entry";
  entries[0].dir = true;
  entries[0].size = 0;
  entries[0].start = End;
  entries[0].prev = End;
  entries[0].next = End;
  entries[0].child = End;
}

unsigned libmwaw::DirTree::entryCount()
{
  return entries.size();
}

libmwaw::DirEntry *libmwaw::DirTree::entry( unsigned index )
{
  if( index >= entryCount() ) return (libmwaw::DirEntry *) 0;
  return &entries[ index ];
}

// given a fullname (e.g "/ObjectPool/_1020961869"), find the entry
libmwaw::DirEntry *libmwaw::DirTree::entry( const std::string &name )
{

  if( !name.length() ) return (libmwaw::DirEntry *)0;

  // quick check for "/" (that's root)
  if( name == "/" ) return entry( 0 );

  // split the names, e.g  "/ObjectPool/_1020961869" will become:
  // "ObjectPool" and "_1020961869"
  std::list<std::string> names;
  std::string::size_type start = 0, end = 0;
  if( name[0] == '/' ) start++;
  while( start < name.length() ) {
    end = name.find_first_of( '/', start );
    if( end == std::string::npos ) end = name.length();
    names.push_back( name.substr( start, end-start ) );
    start = end+1;
  }

  // start from root
  int index = 0 ;

  // trace one by one
  std::list<std::string>::iterator it;

  for( it = names.begin(); it != names.end(); ++it ) {
    unsigned child = 0;
    // dima: performace optimisation of the previous
    child = find_child( index, *it );
    // traverse to the child
    if( child > 0 ) index = child;
    else return (libmwaw::DirEntry *)0;
  }

  return entry( index );
}

static unsigned dirtree_find_sibling( libmwaw::DirTree *dirtree, unsigned index, const std::string &name )
{

  unsigned count = dirtree->entryCount();
  libmwaw::DirEntry *e = dirtree->entry( index );
  if (!e || !e->valid) return 0;
  if (e->name == name) return index;

  if (e->next>0 && e->next<count) {
    unsigned r = dirtree_find_sibling( dirtree, e->next, name );
    if (r>0) return r;
  }

  if (e->prev>0 && e->prev<count) {
    unsigned r = dirtree_find_sibling( dirtree, e->prev, name );
    if (r>0) return r;
  }

  return 0;
}

unsigned libmwaw::DirTree::find_child( unsigned index, const std::string &name )
{

  unsigned count = entryCount();
  libmwaw::DirEntry *p = entry( index );
  if (p && p->valid && p->child < count )
    return dirtree_find_sibling( this, p->child, name );

  return 0;
}

void libmwaw::DirTree::load( unsigned char *buffer, unsigned size )
{
  entries.clear();

  for( unsigned i = 0; i < size/128; i++ ) {
    unsigned p = i * 128;

    // parse name of this entry, which stored as Unicode 16-bit
    std::string name;
    int name_len = ::readU16( buffer + 0x40+p );
    if( name_len > 64 ) name_len = 64;
    for( int j=0; ( buffer[j+p]) && (j<name_len); j+= 2 )
      name.append( 1, buffer[j+p] );

    // would be < 32 if first char in the name isn't printable
    // first char isn't printable ? remove it...
    if( buffer[p] < 32 )
      name.erase( 0,1 );

    // 2 = file (aka stream), 1 = directory (aka storage), 5 = root
    unsigned type = buffer[ 0x42 + p];

    libmwaw::DirEntry e;
    e.valid = true;
    e.name = name;
    e.start = ::readU32( buffer + 0x74+p );
    e.size = ::readU32( buffer + 0x78+p );
    e.prev = ::readU32( buffer + 0x44+p );
    e.next = ::readU32( buffer + 0x48+p );
    e.child = ::readU32( buffer + 0x4C+p );
    e.dir = ( type!=2 );

    // sanity checks
    if( (type != 2) && (type != 1 ) && (type != 5 ) ) e.valid = false;
    if( name_len < 1 ) e.valid = false;

    entries.push_back( e );
  }
}

std::vector<std::string> libmwaw::DirTree::getOLENames()
{
  std::vector<std::string> res;
  std::set<unsigned> seens;
  getOLENames(0, "", res, seens);
  return res;
}

void libmwaw::DirTree::getOLENames(unsigned index, const std::string &prefix,
                                   std::vector<std::string> &res,
                                   std::set<unsigned> &seen)
{
  if (seen.find(index) != seen.end())
    return;
  seen.insert(index);
  unsigned count = entryCount();
  libmwaw::DirEntry *p = entry( index );
  if (!p || !p->valid)
    return;
  std::string name(prefix);
  if (index) {
    if (p->name.length())
      name+= p->name;
    else
      return;
  }
  if (!p->dir) {
    res.push_back(name);
    return;
  }
  if (index)
    name += "/";
  std::set<unsigned> siblingsSeen;
  std::vector<unsigned> siblingsStack;
  siblingsStack.push_back(p->child);
  siblingsSeen.insert(p->child);
  while(siblingsStack.size()) {
    unsigned child = siblingsStack.back();
    siblingsStack.pop_back();
    if (seen.find(child) == seen.end())
      getOLENames(child, name, res, seen);
    // look for next sibling
    DirEntry *e = entry( child );
    if (!e || !e->valid) continue;
    child = e->next;
    if (child > 0 && child <= count
        && siblingsSeen.find(child) == siblingsSeen.end()) {
      siblingsStack.push_back(child);
      siblingsSeen.insert(child);
    }
    child = e->prev;
    if (child > 0 && child <= count
        && siblingsSeen.find(child) == siblingsSeen.end()) {
      siblingsStack.push_back(child);
      siblingsSeen.insert(child);
    }
  }
}

// =========== StorageIO ==========

libmwaw::StorageIO::StorageIO( libmwaw::Storage *st, WPXInputStream *is ) :
  storage(st),
  input( is ),
  result(libmwaw::Storage::Ok),
  header(new libmwaw::Header()),
  dirtree(new libmwaw::DirTree()),
  bbat(new libmwaw::AllocTable()),
  sbat(new libmwaw::AllocTable()),
  sb_blocks(),
  isLoadDone(false)
{
  bbat->blockSize = 1 << header->b_shift;
  sbat->blockSize = 1 << header->s_shift;
}

libmwaw::StorageIO::~StorageIO()
{
  delete sbat;
  delete bbat;
  delete dirtree;
  delete header;
}

bool libmwaw::StorageIO::isOLEStream()
{
  if (!input) return false;
  long actPos = input->tell();
  load();
  input->seek(actPos, WPX_SEEK_SET);
  return (result == libmwaw::Storage::Ok);
}

void libmwaw::StorageIO::load()
{
  if (isLoadDone)
    return;
  isLoadDone = true;
  std::vector<unsigned long> blocks;

  // load header
  unsigned long numBytesRead = 0;
  input->seek(0, WPX_SEEK_SET);
  const unsigned char *buf = input->read(512, numBytesRead);

  result = libmwaw::Storage::NotOLE;
  if (numBytesRead < 512)
    return;

  header->load( buf, numBytesRead );

  // check OLE magic id
  for( unsigned i=0; i<8; i++ )
    if( header->id[i] != mwawole_magic[i] )
      return;

  // sanity checks
  result = libmwaw::Storage::BadOLE;
  if( !header->valid() ) return;
  if( header->threshold != 4096 ) return;

  // important block size
  bbat->blockSize = 1 << header->b_shift;
  sbat->blockSize = 1 << header->s_shift;

  // find blocks allocated to store big bat
  // the first 109 blocks are in header, the rest in meta bat
  blocks.clear();
  blocks.resize( header->num_bat );
  for( unsigned j = 0; j < 109; j++ )
    if( j >= header->num_bat ) break;
    else blocks[j] = header->bb_blocks[j];
  if( (header->num_bat > 109) && (header->num_mbat > 0) ) {
    std::vector<unsigned char> buffer2( bbat->blockSize );
    unsigned k = 109;
    unsigned sector;
    for( unsigned r = 0; r < header->num_mbat; r++ ) {
      if(r == 0) // 1st meta bat location is in file header.
        sector = header->mbat_start;
      else      // next meta bat location is the last current block value.
        sector = blocks[--k];
      loadBigBlock( sector, &buffer2[0], bbat->blockSize );
      for( unsigned s=0; s < bbat->blockSize; s+=4 ) {
        if( k >= header->num_bat ) break;
        else  blocks[k++] = ::readU32( &buffer2[s] );
      }
    }
  }

  // load big bat
  if( blocks.size()*bbat->blockSize > 0 ) {
    std::vector<unsigned char> buffer( blocks.size()*bbat->blockSize );
    loadBigBlocks( blocks, &buffer[0], buffer.size() );
    bbat->load( &buffer[0], buffer.size() );
  }

  // load small bat
  blocks.clear();
  blocks = bbat->follow( header->sbat_start );
  if( blocks.size()*bbat->blockSize > 0 ) {
    std::vector<unsigned char> buffer( blocks.size()*bbat->blockSize );
    loadBigBlocks( blocks, &buffer[0], buffer.size() );
    sbat->load( &buffer[0], buffer.size() );
  }

  // load directory tree
  blocks.clear();
  blocks = bbat->follow( header->dirent_start );
  std::vector<unsigned char> buffer(blocks.size()*bbat->blockSize);
  loadBigBlocks( blocks, &buffer[0], buffer.size() );
  dirtree->load( &buffer[0], buffer.size() );
  unsigned sb_start = ::readU32( &buffer[0x74] );

  // fetch block chain as data for small-files
  sb_blocks = bbat->follow( sb_start ); // small files

  // so far so good
  result = libmwaw::Storage::Ok;
}

libmwaw::StreamIO *libmwaw::StorageIO::streamIO( const std::string &name )
{
  // sanity check
  if( !name.length() ) return (libmwaw::StreamIO *)0;

  load();
  // search in the entries
  libmwaw::DirEntry *entry = dirtree->entry( name );
  if( !entry ) return (libmwaw::StreamIO *)0;
  if( entry->dir ) return (libmwaw::StreamIO *)0;

  libmwaw::StreamIO *res = new libmwaw::StreamIO( this, entry );
  res->fullName = name;

  return res;
}

unsigned long libmwaw::StorageIO::loadBigBlocks( std::vector<unsigned long> blocks,
    unsigned char *data, unsigned long maxlen )
{
  // sentinel
  if( !data ) return 0;
  if( blocks.size() < 1 ) return 0;
  if( maxlen == 0 ) return 0;

  // read block one by one, seems fast enough
  unsigned long bytes = 0;
  for( unsigned long i=0; (i < blocks.size() ) & ( bytes<maxlen ); i++ ) {
    unsigned long block = blocks[i];
    unsigned long pos =  bbat->blockSize * ( block+1 );
    unsigned long p = (bbat->blockSize < maxlen-bytes) ? bbat->blockSize : maxlen-bytes;

    input->seek(pos, WPX_SEEK_SET);
    unsigned long numBytesRead = 0;
    const unsigned char *buf = input->read(p, numBytesRead);
    memcpy(data+bytes, buf, numBytesRead);
    bytes += numBytesRead;
  }

  return bytes;
}

unsigned long libmwaw::StorageIO::loadBigBlock( unsigned long block,
    unsigned char *data, unsigned long maxlen )
{
  // sentinel
  if( !data ) return 0;

  // wraps call for loadBigBlocks
  std::vector<unsigned long> blocks;
  blocks.resize( 1 );
  blocks[ 0 ] = block;

  return loadBigBlocks( blocks, data, maxlen );
}

// return number of bytes which has been read
unsigned long libmwaw::StorageIO::loadSmallBlocks( std::vector<unsigned long> blocks,
    unsigned char *data, unsigned long maxlen )
{
  // sentinel
  if( !data ) return 0;
  if( blocks.size() < 1 ) return 0;
  if( maxlen == 0 ) return 0;

  // our own local buffer
  std::vector<unsigned char> tmpBuf( bbat->blockSize );

  // read small block one by one
  unsigned long bytes = 0;
  for( unsigned long i=0; ( i<blocks.size() ) & ( bytes<maxlen ); i++ ) {
    unsigned long block = blocks[i];

    // find where the small-block exactly is
    unsigned long pos = block * sbat->blockSize;
    unsigned long bbindex = pos / bbat->blockSize;
    if( bbindex >= sb_blocks.size() ) break;

    loadBigBlock( sb_blocks[ bbindex ], &tmpBuf[0], bbat->blockSize );

    // copy the data
    unsigned offset = pos % bbat->blockSize;
    unsigned long p = (maxlen-bytes < bbat->blockSize-offset ) ? maxlen-bytes :  bbat->blockSize-offset;
    p = (sbat->blockSize<p ) ? sbat->blockSize : p;
    memcpy( data + bytes, &tmpBuf[offset], p );
    bytes += p;
  }

  return bytes;
}

unsigned long libmwaw::StorageIO::loadSmallBlock( unsigned long block,
    unsigned char *data, unsigned long maxlen )
{
  // sentinel
  if( !data ) return 0;

  // wraps call for loadSmallBlocks
  std::vector<unsigned long> blocks;
  blocks.resize( 1 );
  blocks.assign( 1, block );

  return loadSmallBlocks( blocks, data, maxlen );
}

std::vector<std::string> libmwaw::StorageIO::getOLENames()
{
  if (!dirtree)
    return std::vector<std::string>();
  return dirtree->getOLENames();
}

// =========== StreamIO ==========

libmwaw::StreamIO::StreamIO( libmwaw::StorageIO *s, libmwaw::DirEntry *e) :
  io(s),
  entry(e),
  fullName(),
  eof(false),
  fail(false),
  blocks(),
  m_pos(0),
  cache_data(),
  cache_size(4096),
  cache_pos(0)
{
  if( entry->size >= io->header->threshold )
    blocks = io->bbat->follow( entry->start );
  else
    blocks = io->sbat->follow( entry->start );

  // prepare cache
  cache_data = std::vector<unsigned char>(cache_size);
  updateCache();
}

// FIXME tell parent we're gone
libmwaw::StreamIO::~StreamIO()
{
}

unsigned long libmwaw::StreamIO::tell()
{
  return m_pos;
}

unsigned long libmwaw::StreamIO::read( unsigned long pos, unsigned char *data, unsigned long maxlen )
{
  // sanity checks
  if( !data ) return 0;
  if( maxlen == 0 ) return 0;

  unsigned long totalbytes = 0;

  if ( entry->size < io->header->threshold ) {
    // small file
    unsigned long index = pos / io->sbat->blockSize;

    if( index >= blocks.size() ) return 0;

    std::vector<unsigned char> buf( io->sbat->blockSize );
    unsigned long offset = pos % io->sbat->blockSize;
    while( totalbytes < maxlen ) {
      if( index >= blocks.size() ) break;
      io->loadSmallBlock( blocks[index], &buf[0], io->bbat->blockSize );
      unsigned long count = io->sbat->blockSize - offset;
      if( count > maxlen-totalbytes ) count = maxlen-totalbytes;
      memcpy( data+totalbytes, &buf[offset], count );
      totalbytes += count;
      offset = 0;
      index++;
    }
  } else {
    // big file
    unsigned long index = pos / io->bbat->blockSize;

    if( index >= blocks.size() ) return 0;

    std::vector<unsigned char> buf( io->bbat->blockSize );
    unsigned long offset = pos % io->bbat->blockSize;
    while( totalbytes < maxlen ) {
      if( index >= blocks.size() ) break;
      io->loadBigBlock( blocks[index], &buf[0], io->bbat->blockSize );
      unsigned long count = io->bbat->blockSize - offset;
      if( count > maxlen-totalbytes ) count = maxlen-totalbytes;
      memcpy( data+totalbytes, &buf[offset], count );
      totalbytes += count;
      index++;
      offset = 0;
    }
  }

  return totalbytes;
}

unsigned long libmwaw::StreamIO::read( unsigned char *data, unsigned long maxlen )
{
  unsigned long bytes = read( tell(), data, maxlen );
  m_pos += bytes;
  return bytes;
}

void libmwaw::StreamIO::updateCache()
{
  // sanity check
  if( cache_data.empty() ) return;

  cache_pos = m_pos - ( m_pos % cache_size );
  unsigned long bytes = cache_size;
  if( cache_pos + bytes > entry->size ) bytes = entry->size - cache_pos;
  cache_size = read( cache_pos, &cache_data[0], bytes );
}


// =========== Storage ==========

libmwaw::Storage::Storage( WPXInputStream *is ) :
  io(NULL)
{
  io = new StorageIO( this, is );
}

libmwaw::Storage::~Storage()
{
  delete io;
}

int libmwaw::Storage::result()
{
  return io->result;
}

bool libmwaw::Storage::isOLEStream()
{
  return io->isOLEStream();
}

std::vector<std::string> libmwaw::Storage::getOLENames()
{
  return io->getOLENames();
}

WPXInputStream *libmwaw::Storage::getDocumentOLEStream(const std::string &name)
{
  if (!isOLEStream() || !name.length())
    return 0;
  Stream stream(this, name);
  unsigned long sz = stream.size();
  if (result() != Ok  || !sz)
    return 0;

  unsigned char *buf = new unsigned char[sz];
  if (buf == 0) return 0;

  unsigned long oleLength = stream.read(buf, sz);
  if (oleLength != sz) {
    MWAW_DEBUG_MSG(("libmwaw::Storage::getDocumentOLEStream: Ole=%s expected length %ld but read %ld\n",
                    name.c_str(), sz, oleLength));

    // we ignore this error, if we read a ole in the root directory
    // and we read at least 50% of the data. This may help to read
    // a damaged file.
    bool rootDir = name.find('/', 0) == std::string::npos;
    if (!rootDir || oleLength <= (sz+1)/2) {
      delete [] buf;
      return 0;
    }
    // continue
    MWAW_DEBUG_MSG(("libmwaw::Storage::getDocumentOLEStream: tries to use damaged OLE: %s\n", name.c_str()));
  }

  WPXInputStream *res = new libmwaw_internal::MWAWStringStream(buf, oleLength);
  delete [] buf;
  return res;
}

// =========== Stream ==========
libmwaw::Stream::Stream( libmwaw::Storage *storage, const std::string &name ) :
  io(storage->io->streamIO( name ))
{
}

// FIXME tell parent we're gone
libmwaw::Stream::~Stream()
{
  delete io;
}

unsigned long libmwaw::Stream::size()
{
  return io ? io->entry->size : 0;
}

unsigned long libmwaw::Stream::read( unsigned char *data, unsigned long maxlen )
{
  return io ? io->read( data, maxlen ) : 0;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab: