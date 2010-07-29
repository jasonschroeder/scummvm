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

#ifndef DEBUGTOOLS_H
#define DEBUGTOOLS_H

class BS_Debugtools
{
public:
	/**
	    @brief Gibt eine ID zur�ck, die die benutzte Programmversion eindeutig identifiziert.

		Um die Version zu ermitteln wird der MD5-Hash �ber die EXE-Datei gebildet.
		Falls die ausf�hrende Datei in einem SVN-Repository liegt wird zus�tzlich die Revision des Verzeichnisses gehasht.

		@return Gibt einen String zur�ck, der die Versions-ID des Programmes angibt.<br>
				Falls die Versions-ID nicht bestimmt werden konnte wird "???" zur�ckgegeben.
		@remark Diese Methode ist momentan nur f�r WIN32 implementiert.
	*/
	static const char * GetVersionID();

	/**

		@brief Gibt die Subversion-Revisionsnummer der Engine zur�ck.

		Diese Funktion versucht die aktuelle Revision aus der SVN entries Datei f�r das aktuelle Verzeichnis zu extrahieren.
		Dabei werden die SVN entries Formatversionen 7 und gr��er unterst�tzt.
		Die neueste Version ist aktuell 9. F�r folgende Versionen wird angenommen, dass sich das Format des Headers nicht mehr �ndert.
	
		@return Gibt die Revisionsnummer zur�ck. Falls die ausf�hrende Datei nicht in einem SVN-Repository liegt oder die Revision nicht
				festgestellt werden konnte wird 0 zur�ckgegeben.	
	*/
	static unsigned int GetSubversionRevision();
};

#endif