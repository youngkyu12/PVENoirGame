#include "pch.h"
#include "BufferReader.h"
#include "BufferWriter.h"

BufferReader::BufferReader()
{
}

BufferReader::BufferReader(BYTE* buffer, uint32 size, uint32 pos)
	: _buffer(buffer), _size(size), _pos(pos)
{

}

BufferReader::~BufferReader()
{

}



bool BufferReader::Peak(void* dest, uint32 len)
{
	if (FreeSize() < len)
		return false;

	::memcpy(dest, &_buffer[_pos], len);
	return true;
}

bool BufferReader::Read(void* dest, uint32 len)
{
	if(Peak(dest, len) == false)
		return false;

	_pos += len;
	return true;
}

