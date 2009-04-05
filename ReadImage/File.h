#pragma once

#include "IFile.h"

#include <stdio.h>

class File : public IFile
{
public:
	File(FILE* file);

	bool Read(void* pBuffer, size_t nNumberOfBytesToRead, size_t& nNumberOfBytesRead);
	
	bool Write(const void* pBuffer, size_t nNumberOfBytesToWrite, size_t& nNumberOfBytesWritten);
	
	size_t Seek(long lDistanceToMove, size_t dwMoveMethod);
	
	size_t Tell() const;
	
	size_t Size() const;
	
	bool Flush();
	
	bool HasBuffer() const { return false; }
	virtual const void* GetBuffer() const { return 0; }
	
private:
	FILE* file_;
};

