#include "pch.h"
#include "SendBuffer.h"

SendBuffer::SendBuffer(int32 bufferSize)
{
	_buffer.resize(bufferSize);
}

SendBuffer::~SendBuffer()
{
}

void SendBuffer::CopyData(BYTE* data, int32 len)
{
	ASSERT_CRASH(len <= Capacity());
	::memcpy(_buffer.data(), data, len);
	_writeSize = len;
}
