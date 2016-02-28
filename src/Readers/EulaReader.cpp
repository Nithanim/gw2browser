/* \file       Readers/EulaReader.cpp
*  \brief      Contains the definition of the eula reader class.
*  \author     Khral Steelforge
*/

/*
Copyright (C) 2016 Khral Steelforge <https://github.com/kytulendu>

This file is part of Gw2Browser.

Gw2Browser is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"

#include <gw2formats/pf/EulaPackFile.h>

#include "EulaReader.h"

namespace gw2b {

	EulaReader::EulaReader( const Array<byte>& p_data, ANetFileType p_fileType )
		: FileReader( p_data, p_fileType ) {
	}

	EulaReader::~EulaReader( ) {
	}

	std::vector<wxString> EulaReader::getString( ) const {
		gw2f::pf::EulaPackFile eulaFile( m_data.GetPointer( ), m_data.GetSize( ) );
		auto chunk = eulaFile.chunk<gw2f::pf::EulaChunks::eula>( );
		auto size = chunk->languages.size( );

		std::vector<wxString> eula;
		if ( size ) {
			for ( size_t i = 0; i < size; i++ ) {
				eula.push_back( wxString( chunk->languages[i].text.data( ) ) );
			}
		}

		return eula;
	}

}; // namespace gw2b
