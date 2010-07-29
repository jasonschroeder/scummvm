// -----------------------------------------------------------------------------
// This file is part of Broken Sword 2.5
// Copyright (c) Malte Thiesen, Daniel Queteschiner and Michael Elsd�rfer
//
// Broken Sword 2.5 is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Broken Sword 2.5 is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Broken Sword 2.5; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
// -----------------------------------------------------------------------------

#ifdef BS_MEMLOG

// Die folgende Zeile stellt sicher, dass alle Objekte in dieser Datei vor allen anderen erstellt und nach allen anderen
// zerst�rt werden.
// Damit wird sichergestellt, dass z.B. Singletons nicht f�lschlicherweise als Memory-Leaks erkannt werden.
// TODO Visual C++ 8 kommt mit der aktuellen Implementation nicht klar und st�rzt sowohl beim Start aus auch beim
// Beenden ab. Es muss eine Alternativimplementation her. An sichersten ist es wohl, wenn gar keine STL-Objekte benutzt
// werden.
// #pragma warning (disable : 4074)
// #pragma init_seg(compiler)

#include "filesystemutil.h"

#include "memlog_off.h"
#include <vector>
#include <algorithm>
#include <string>

#include <stdio.h>
#include <string.h>

typedef struct
{
	unsigned int		address;
	unsigned int		size;
	std::string			file;
	unsigned int		line;
} ALLOC_INFO;

static const char * MEMLEAK_LOG_FILE = "memory_leaks.txt";
static const unsigned int BUCKET_COUNT = 1021;
std::vector< std::vector<ALLOC_INFO> > TrackData(BUCKET_COUNT);

static unsigned int TotalSize = 0;

// Diese Klasse stellt sicher, dass beim Programmende, das Memory-Leak Log geschrieben wird.
static class LeakDumper
{
public:
	LeakDumper() : OutputFilename(BS_FileSystemUtil::GetInstance().GetUserdataDirectory() + "\\" + MEMLEAK_LOG_FILE)
	{
		// Sicherstellen, dass das Ausgabeverzeichnis f�r die Datei existiert.
		BS_FileSystemUtil::GetInstance().CreateDirectory(BS_FileSystemUtil::GetInstance().GetUserdataDirectory());
	}

	~LeakDumper()
	{
		DumpUnfreed(OutputFilename.c_str());
	}

	std::string OutputFilename;
} LeakDumperInstance;

void DumpUnfreed(const char * OutputFilename)
{
	FILE * Log = fopen(OutputFilename, "w");
	fputs("MEMORY LEAK REPORT:\n----------------------\n", Log);
	std::vector< std::vector<ALLOC_INFO> >::iterator BucketIter = TrackData.begin();
	for (; BucketIter != TrackData.end(); ++BucketIter)
	{
		std::vector<ALLOC_INFO>::iterator Iter = (*BucketIter).begin();
		for (; Iter != (*BucketIter).end(); ++Iter)
		{
			ALLOC_INFO & CurItem = (*Iter);
			fprintf(Log, "%-50s LINE:%d ADDRESS:0x%x SIZE:%d\n",
					CurItem.file.c_str(),
					CurItem.line,
					CurItem.address,
					CurItem.size);
		}
	}

	fprintf(Log, "----------------------\nTotal unfreed bytes: %d\n", TotalSize);

	fclose(Log);
}

void AddTrack(unsigned int addr,  unsigned int asize,  const char *fname, unsigned int lnum)
{
	std::vector<ALLOC_INFO> & CurBucket = TrackData[(addr >> 3) % BUCKET_COUNT];
	ALLOC_INFO Info;
	Info.address = addr;
	Info.size = asize;
	Info.file = fname;
	Info.line = lnum;
	CurBucket.push_back(Info);

	TotalSize += asize;
}

void RemoveTrack(unsigned int addr)
{
	if (addr != 0 && TrackData.size() == BUCKET_COUNT)
	{
		std::vector<ALLOC_INFO> & CurBucket = TrackData[(addr >> 3) % BUCKET_COUNT];
		std::vector<ALLOC_INFO>::iterator Iter = CurBucket.begin();
		for (; Iter != CurBucket.end(); ++Iter)
		{
			if ((*Iter).address == addr)
			{
				TotalSize -= (*Iter).size;
				
				std::swap(*Iter, CurBucket.back());
				CurBucket.pop_back();
				return;
			}
		}
	}
}

#endif