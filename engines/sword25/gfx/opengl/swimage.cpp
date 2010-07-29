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

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include "package/packagemanager.h"
#include "gfx/image/imageloader.h"

#include "swimage.h"

#define BS_LOG_PREFIX "SWIMAGE"


// -----------------------------------------------------------------------------
// CONSTRUCTION / DESTRUCTION
// -----------------------------------------------------------------------------

BS_SWImage::BS_SWImage(const std::string & Filename, bool & Result) :
	_ImageDataPtr(0),
	m_Width(0),
	m_Height(0)
{
	Result = false;

	BS_PackageManager * pPackage = static_cast<BS_PackageManager*>(BS_Kernel::GetInstance()->GetService("package"));
	BS_ASSERT(pPackage);

	// Datei laden
	char* pFileData;
	unsigned int FileSize;
	if (!(pFileData = (char*) pPackage->GetFile(Filename, &FileSize)))
	{
		BS_LOG_ERRORLN("File \"%s\" could not be loaded.", Filename.c_str());
		return;
	}

	// Bildeigenschaften bestimmen
	BS_GraphicEngine::COLOR_FORMATS ColorFormat;
	int Pitch;
	if (!BS_ImageLoader::ExtractImageProperties(pFileData, FileSize, ColorFormat, m_Width, m_Height))
	{
		BS_LOG_ERRORLN("Could not read image properties.");
		return;
	}

	// Das Bild dekomprimieren
	char * pUncompressedData;
	if (!BS_ImageLoader::LoadImage(pFileData, FileSize, BS_GraphicEngine::CF_ABGR32, pUncompressedData, m_Width, m_Height, Pitch))
	{
		BS_LOG_ERRORLN("Could not decode image.");
		return;
	}

	// Dateidaten freigeben
	delete[] pFileData;

	_ImageDataPtr = (unsigned int *) pUncompressedData;

	Result = true;
	return;
}

// -----------------------------------------------------------------------------

BS_SWImage::~BS_SWImage()
{
	delete [] _ImageDataPtr;
}


// -----------------------------------------------------------------------------

bool BS_SWImage::Blit(int PosX, int PosY,
					  int Flipping,
					  BS_Rect* pPartRect,
					  unsigned int Color,
					  int Width, int Height)
{
	BS_LOG_ERRORLN("Blit() is not supported.");
	return false;
}

// -----------------------------------------------------------------------------

bool BS_SWImage::Fill(const BS_Rect* pFillRect, unsigned int Color)
{
	BS_LOG_ERRORLN("Fill() is not supported.");
	return false;
}

// -----------------------------------------------------------------------------

bool BS_SWImage::SetContent(const std::vector<unsigned char> & Pixeldata, unsigned int Offset, unsigned int Stride)
{
	BS_LOG_ERRORLN("SetContent() is not supported.");
	return false;
}

// -----------------------------------------------------------------------------

unsigned int BS_SWImage::GetPixel(int X, int Y)
{
	BS_ASSERT(X >= 0 && X < m_Width);
	BS_ASSERT(Y >= 0 && Y < m_Height);

	return _ImageDataPtr[m_Width * Y + X];
}