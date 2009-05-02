/*******************************************************************************
 *
 *  Copyright 2004 Muschick Christian
 *  
 *  This file is part of Lear.
 *  
 *  Lear is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  Lear is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with Lear; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 *  -----------------------------------------------------------------------------
 *
 *  filename            : Serializer.cpp
 *  author              : Muschick Christian
 *  date of creation    : 14.11.2003
 *  date of last change : 14.12.2004
 *
 *******************************************************************************/

#include "Serializer.h"
#include "Log.h"


using namespace serializer;

//------------------------------------------------------------------------------
Serializer::Serializer() :
    tagging_enabled_(0),
    in_(NULL),
    out_(NULL)
{
}

//------------------------------------------------------------------------------
/**
 *  \see open
 *
 *  \param filename The file to read or write.
 *  \param mode     A combination of SOM_READ, SOM_WRITE, SOM_TAG, SOM_COMPRESS.
 *                  SOM_TAG and SOM_COMPRESS are relevant only when writing a file.
 *                  If reading, the file is automatically handled correctly.
 */
Serializer::Serializer(const std::string & filename, uint32_t mode) :
    tagging_enabled_(false),
    filename_(filename),
    in_(NULL),
    out_(NULL)
{
    open(filename, mode);
}


//------------------------------------------------------------------------------
Serializer::~Serializer()
{
    reset();
    close();
}

//------------------------------------------------------------------------------
/**
 *  A file can be written with compression enabled/disabled and
 *  tagging enabled/disabled.
 *
 *  If tagging is enabled, every fundamental datatype written to disk
 *  is preceded by a marker tag corresponding to the type. This way
 *  inconsistencies in the reading / writing code can easily be
 *  detected.
 *
 *  \param filename The file to read or write.
 *  \param mode     A combination of SOM_READ, SOM_WRITE, SOM_TAG, SOM_COMPRESS, SOM_APPEND.
 *                  SOM_TAG, SOM_COMPRESS and SOM_APPEND are relevant only when writing a file.
 *                  If reading, the file is automatically handled correctly.
 *  \todo policy based serializer
 */
void Serializer::open(const std::string & filename, uint32_t mode)
{
    using namespace std;

    tagging_enabled_ = (mode & SOM_TAG) != 0;
    
    ios_base::openmode m = ios_base::binary;
    if (mode & SOM_READ)
    {
        m |= ios_base::in;

#ifdef NO_ZLIB
        in_ = new ifstream(filename.c_str(), m);
#else
        in_ = new igzstream(filename.c_str(), m);
#endif        

        if (!*in_)
        {
            IoException e("Failed to open file ");
            e << filename << " for " << "reading";

            reset();
            
            throw e;
        }
        

        // decide whether the file was written in "tagging" mode
        uint32_t marker;
        readBuffer(&marker, sizeof(marker));
        if (marker == TAGGED_MARKER)
        {
            tagging_enabled_ = true;
            s_log << Log::debug('s') << "Reading " << filename << " in tagged mode.\n";
        } else
        {
            s_log << Log::debug('s') << "Reading " << filename << " in untagged mode.\n";

            bool b=0;
            b |= !in_->unget();
            b |= !in_->unget();
            b |= !in_->unget();
            b |= !in_->unget();

            if (b)
            {
                reset();
                throw IoException("Unable to unget");
            }
        }
        
    } else if (mode & SOM_WRITE)
    {
        m |= ios_base::out;
        if (!(mode & SOM_APPEND)) m |= ios_base::trunc;

        if (mode & SOM_COMPRESS)
        {
#ifndef NO_ZLIB
            out_ = new ogzstream(filename.c_str(), m);
#else
            s_log << Log::error
                  << "Compression not supported\n";
            out_ = new ofstream(filename.c_str(), m);
#endif            
        } else
        {
            out_ = new ofstream(filename.c_str(), m);
        }
        
        if (!*out_)
        {
            reset();
            
            IoException e("Failed to open file ");
            e << filename << " for " << "writing";
            throw e;
        }

        if (tagging_enabled_)
        {
            writeBuffer(&TAGGED_MARKER, sizeof(TAGGED_MARKER));
        }
    }
}


//------------------------------------------------------------------------------
bool Serializer::isTaggingEnabled() const
{
    return tagging_enabled_;
}

//------------------------------------------------------------------------------
void Serializer::close()
{
    if (in_) 
    {
        DELNULL(in_);
    }
    if (out_) 
    {
        DELNULL(out_);
    }
}


//------------------------------------------------------------------------------
/**
 *  Saves the inverted address of the object as object id.
 *  \see ISerializable
 */
void Serializer::putObjectId(const ISerializable * object)
{
    writeTag(ST_OBJECT_ID);

    IdType id = ~reinterpret_cast<IdType>(object);
    writeNumber(id);
}

//------------------------------------------------------------------------------
/**
 *  Bools are saved as uint8_t.
 */
void Serializer::put(bool val)
{
    put((uint8_t)val);
}

//------------------------------------------------------------------------------
void Serializer::put(uint8_t val)
{
    writeTag(ST_UINT8);
    writeNumber(val);
}

//------------------------------------------------------------------------------
void Serializer::put(uint16_t val)
{
    writeTag(ST_UINT16);
    writeNumber(val);
}

//------------------------------------------------------------------------------
void Serializer::put(int32_t val)
{
    writeTag(ST_INT32);
    writeNumber(val);
}

//------------------------------------------------------------------------------
void Serializer::put(uint32_t val)
{
    writeTag(ST_UINT32);
    writeNumber(val);
}

//------------------------------------------------------------------------------
void Serializer::put(float32_t val)
{
    writeTag(ST_FLOAT32);
    writeNumber(val);
}


//------------------------------------------------------------------------------
void Serializer::put(const std::string & str)
{
    writeTag(ST_STRING);
    
    uint32_t length = str.length();
    put(length);

    for (unsigned i=0; i<length; ++i)
    {
        put((uint8_t)str[i]);
    }
}

//------------------------------------------------------------------------------
void Serializer::putRaw(const void * data, size_t length)
{
    return writeBuffer(data, length);
}


//------------------------------------------------------------------------------
void Serializer::get(bool & val)
{
    uint8_t v;
    get(v);
    val = (bool)v;
}


//------------------------------------------------------------------------------
void Serializer::get(uint8_t & val)
{
    checkTag(ST_UINT8);
    readNumber(val);
}

//------------------------------------------------------------------------------
void Serializer::get(uint16_t& val)
{
    checkTag(ST_UINT16);
    readNumber(val);
}

//------------------------------------------------------------------------------
void Serializer::get(int32_t & val)
{
    checkTag(ST_INT32);
    readNumber(val);
}

//------------------------------------------------------------------------------
void Serializer::get(uint32_t & val)
{
    checkTag(ST_UINT32);
    readNumber(val);
}

//------------------------------------------------------------------------------
void Serializer::get(float32_t & val)
{
    checkTag(ST_FLOAT32);
    readNumber(val);
}

//------------------------------------------------------------------------------
void Serializer::get(std::string & str)
{
    checkTag(ST_STRING);

    str.clear();
    uint32_t length;
    get(length);

    try
    {
        str.reserve(length);
    } catch(...)
    {
        Exception e("Failed to allocate a string of ");
        e << length << " characters.";

        reset();
        
        throw e;
    }
    
    uint8_t cur_char;
    for (unsigned i=0; i<length;++i)
    {
        get(cur_char);    
        str += cur_char;
    }
}

//------------------------------------------------------------------------------
/**
 *  Reads raw data from the stream into a buffer.
 *
 *  \param data The buffer to read the data into.
 *  \param length The size in bytes to read.
 */
void Serializer::getRaw(void * data, size_t length)
{
    return readBuffer(data, length);
}

//------------------------------------------------------------------------------
/**
 *  Used for loading Serializable objects.
 *
 *  Stores the memory location of the loaded object with the specified
 *  id so other objects' references can be update accordingly
 *  afterwards.
 */
void Serializer::addId(IdType id, ISerializable * location)
{
    id_map_.insert(std::make_pair(id, location)) ;
}



//------------------------------------------------------------------------------
/**
 *  Remembers the specified object so its finalizeDeserialization() -
 *  method can be called after deserialization.
 */
void Serializer::addDeserializedObject(ISerializable * object)
{
    deserialized_objects_.push_back(object);
}



//------------------------------------------------------------------------------
/**
 *  After reading all objects containing references, updates their
 *  references using the id_map_ built during deserialization. Also
 *  calls the object's finalizeDeserialization() - method.
 */
void Serializer::finalizeDeserialization()
{
    for (std::vector<IReference*>::iterator cur_ref = references_to_resolve_.begin();
         cur_ref != references_to_resolve_.end();
         ++cur_ref)
    {
        (*cur_ref)->resolve(id_map_);
        
        delete (*cur_ref);
        *cur_ref = NULL;
    }
    references_to_resolve_.clear();

    // Here all references are valid for the ISerializable objects to
    // use.
    for (std::vector<ISerializable*>::const_iterator cur_object = deserialized_objects_.begin();
         cur_object != deserialized_objects_.end();
         ++cur_object)
    {
        (*cur_object)->finalizeDeserialization();
    }

    deserialized_objects_.clear();
}


//------------------------------------------------------------------------------
void Serializer::reset()
{
    DELNULL(in_);
    
    for (std::vector<IReference*>::iterator cur_ref = references_to_resolve_.begin();
         cur_ref != references_to_resolve_.end();
         ++cur_ref)
    {
        delete *cur_ref;
    }
    references_to_resolve_.clear();
    deserialized_objects_.clear();
}


//------------------------------------------------------------------------------
/**
 *  Does the dirty work of actually writing the data to the file. If
 *  writing fails, an exception is thrown and the stream is closed.
 *
 *  \param val the data to write
 *  \param length The size in bytes of the data to write.
 */
void Serializer::writeBuffer(const void * val, size_t length)
{
    if (out_ == NULL)
    {
        throw IoException("Stream is not open for writing.");
    }

    if(!out_->write((char*)val, length))
    {
        reset();
        throw IoException("Couldn't write to file " + filename_);
    }
}


//------------------------------------------------------------------------------
/**
 *  Does the dirty work of actually reading the data from file. If
 *  reading fails, an exception is thrown and the stream is closed.
 *
 *  \param buf The destination buffer.
 *  \param length The number of bytes to read.
 */
void Serializer::readBuffer(void * buf, size_t length)
{
    if (in_ == NULL)
    {
        throw IoException("Stream is not open for reading.");
    }
    
    if (!in_->read((char*)buf, length))
    {
        reset();
        throw IoException("Couldn't read from file " + filename_);
    }
}

//------------------------------------------------------------------------------
/**
 *  Depending on whether tagging_enabled_ is true, either writes the
 *  specified taag or does nothing.
 *
 *  \param tag The tag to write.
 */
void Serializer::writeTag(const SERIALIZER_TAG tag)
{
    if (tagging_enabled_)
    {
        writeBuffer(&tag, sizeof(tag));
    }
}

//------------------------------------------------------------------------------
/**
 *  Depending on whether tagging_enabled_ is true, either reads the
 *  next DWORD and checks if it corresponds to the specified tag, or
 *  does nothing. If the tags don't match, an exception is thrownand
 *  the stream is closed.
 *
 *  \param tag The tag to match.
 */
void Serializer::checkTag(const SERIALIZER_TAG tag) throw (InvalidTagException, IoException)
{
    if (tagging_enabled_) 
    {
        SERIALIZER_TAG t;
        readBuffer(&t, sizeof(t));
        if (t != tag)
        {
            reset();
            throw InvalidTagException(tag, t);
        }
    }
}


    
//------------------------------------------------------------------------------
/**
 *  First saves the address of the Serializable object to be used as
 *  id upon deserialization. Then writes the actual object.
 */
void serializer::putInto(Serializer & s, const ISerializable & serializable)
{
    IdType id = ~reinterpret_cast<IdType>(&serializable);
    s.putRaw(&id, sizeof(id));
    
    serializable.putInto(s);
}


//------------------------------------------------------------------------------
/**
 *  First retrieves the id of the stored object and remembers the
 *  mapping id->object address. Then stores the object for later call
 *  of its finalizeDeserialization() -method.
 *
 *  Finally retrieves the actual object.
 */
void serializer::getFrom(Serializer & s, ISerializable & serializable)
{
    IdType id;
    s.getRaw(&id, sizeof(id));    
    s.addId(id, &serializable);
    
    s.addDeserializedObject(&serializable);
    serializable.getFrom(s);
}



