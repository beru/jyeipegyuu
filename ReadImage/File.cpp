#include "stdafx.h"
#include "File.h"

//#include <io.h>
#include <assert.h>
#include <memory.h>
#include <stdio.h>

File::File(FILE* file)
{
	file_ = file;
}

bool File::Read(void* pBuffer, size_t nNumberOfBytesToRead, size_t& nNumberOfBytesRead)
{
	nNumberOfBytesRead = fread(pBuffer, 1, nNumberOfBytesToRead, file_);
	return nNumberOfBytesRead;
}

bool File::Write(const void* pBuffer, size_t nNumberOfBytesToWrite, size_t& nNumberOfBytesWritten)
{
	nNumberOfBytesWritten = fwrite(pBuffer, 1, nNumberOfBytesToWrite, file_);
	return nNumberOfBytesWritten;
}

size_t File::Seek(long lDistanceToMove, size_t dwMoveMethod)
{
	return fseek(file_, lDistanceToMove, dwMoveMethod);
}

size_t File::Tell() const
{
	return ftell(file_);
}

size_t File::Size() const
{
	fpos_t pos;
	fgetpos( file_, &pos );
	fseek( file_, 0, SEEK_END );
	long pos2 = ftell(file_);
//	fgetpos( file_, &pos2 );
	fsetpos( file_, &pos );
	return pos2;
//	return GetFileSize(hFile_, NULL);
}

bool File::Flush()
{
	fflush(file_);
}

