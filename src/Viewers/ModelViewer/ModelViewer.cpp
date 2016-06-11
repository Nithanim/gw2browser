﻿/* \file       Viewers/ModelViewer.cpp
*  \brief      Contains the definition of the model viewer class.
*  \author     Khral Steelforge
*/

/*
Copyright (C) 2015-2016 Khral Steelforge <https://github.com/kytulendu>
Copyright (C) 2012 Rhoot <https://github.com/rhoot>

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

#include "Readers/ImageReader.h"

#include "ModelViewer.h"

namespace gw2b {

	//----------------------------------------------------------------------------
	//      RenderTimer
	//----------------------------------------------------------------------------

	RenderTimer::RenderTimer( ModelViewer* canvas ) : wxTimer( ) {
		RenderTimer::canvas = canvas;
	}

	void RenderTimer::Notify( ) {
		canvas->Refresh( );
	}

	void RenderTimer::start( ) {
		wxTimer::Start( 10 );
	}

	//----------------------------------------------------------------------------
	//      ModelViewer
	//----------------------------------------------------------------------------

	struct ModelViewer::MeshCache {
		std::vector<glm::vec3>	vertices;
		std::vector<glm::vec3>	normals;
		std::vector<glm::vec2>	uvs;
		std::vector<uint>		indices;
		std::vector<glm::vec3>	tangents;
		std::vector<glm::vec3>	bitangents;
	};

	struct ModelViewer::VBO {
		GLuint					vertexBuffer;
		GLuint					normalBuffer;
		GLuint					uvBuffer;
		GLuint					tangentbuffer;
		GLuint					bitangentbuffer;
	};

	struct ModelViewer::IBO {
		GLuint					elementBuffer;
	};

	struct ModelViewer::TBO {
		GLuint					diffuseMap;
		GLuint					normalMap;
		GLuint					lightMap;
	};

	struct ModelViewer::PackedVertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 uv;

		bool operator < ( const PackedVertex that ) const {
			return memcmp( ( void* )this, ( void* ) &that, sizeof( PackedVertex ) ) > 0;
		};
	};

	ModelViewer::ModelViewer( wxWindow* p_parent, const int *p_attrib, const wxPoint& p_pos, const wxSize& p_size, long p_style, DatFile& p_datFile )
		: ViewerGLCanvas( p_parent, p_attrib, p_pos, p_size, p_style )
		, m_datFile( p_datFile )
		, m_lastMousePos( std::numeric_limits<int>::min( ), std::numeric_limits<int>::min( ) ) {

		// Initialize OpenGL
		if ( !m_glInitialized ) {
			if ( !this->initGL( ) ) {
				wxLogMessage( wxT( "Could not initialize OpenGL!" ) );
				wxMessageBox( wxT( "Could not initialize OpenGL!" ), wxT( "" ), wxICON_ERROR );
				return;
			}
			m_glInitialized = true;

			m_renderTimer = new RenderTimer( this );
			m_renderTimer->start( );
		}

		// Hook up events
		this->Bind( wxEVT_PAINT, &ModelViewer::onPaintEvt, this );
		this->Bind( wxEVT_MOTION, &ModelViewer::onMotionEvt, this );
		this->Bind( wxEVT_MOUSEWHEEL, &ModelViewer::onMouseWheelEvt, this );
		this->Bind( wxEVT_KEY_DOWN, &ModelViewer::onKeyDownEvt, this );
		this->Bind( wxEVT_CLOSE_WINDOW, &ModelViewer::onClose, this );
	}

	ModelViewer::~ModelViewer( ) {
		// Clean VBO
		for ( auto& it : m_vertexBuffer ) {
			if ( it.vertexBuffer ) {
				glDeleteBuffers( 1, &it.vertexBuffer );
			}
			if ( it.normalBuffer ) {
				glDeleteBuffers( 1, &it.normalBuffer );
			}
			if ( it.uvBuffer ) {
				glDeleteBuffers( 1, &it.uvBuffer );
			}
			if ( it.tangentbuffer ) {
				glDeleteBuffers( 1, &it.tangentbuffer );
			}
			if ( it.bitangentbuffer ) {
				glDeleteBuffers( 1, &it.bitangentbuffer );
			}
		}

		// Clean IBO
		for ( auto& it : m_indexBuffer ) {
			if ( it.elementBuffer ) {
				glDeleteBuffers( 1, &it.elementBuffer );
			}
		}

		// Clean TBO
		for ( auto& it : m_textureBuffer ) {
			if ( it.diffuseMap ) {
				glDeleteBuffers( 1, &it.diffuseMap );
			}
			if ( it.normalMap ) {
				glDeleteBuffers( 1, &it.normalMap );
			}
			if ( it.lightMap ) {
				glDeleteBuffers( 1, &it.lightMap );
			}
		}

		// Clean dummy textures
		glDeleteBuffers( 1, &m_dummyBlackTexture );
		glDeleteBuffers( 1, &m_dummyWhiteTexture );

		// Clean shaders
		m_mainShader.clear( );
		m_normalVisualizerShader.clear( );

		// Clean VAO
		glDeleteVertexArrays( 1, &modelVAO );

		// Clean text renderer
		m_text.clear( );

		delete m_renderTimer;

		delete m_glContext;
	}

	void ModelViewer::clear( ) {
		// Clean VBO
		for ( auto& it : m_vertexBuffer ) {
			if ( it.vertexBuffer ) {
				glDeleteBuffers( 1, &it.vertexBuffer );
			}
			if ( it.normalBuffer ) {
				glDeleteBuffers( 1, &it.normalBuffer );
			}
			if ( it.uvBuffer ) {
				glDeleteBuffers( 1, &it.uvBuffer );
			}
			if ( it.tangentbuffer ) {
				glDeleteBuffers( 1, &it.tangentbuffer );
			}
			if ( it.bitangentbuffer ) {
				glDeleteBuffers( 1, &it.bitangentbuffer );
			}
		}

		// Clean IBO
		for ( auto& it : m_indexBuffer ) {
			if ( it.elementBuffer ) {
				glDeleteBuffers( 1, &it.elementBuffer );
			}
		}

		// Clean TBO
		for ( auto& it : m_textureBuffer ) {
			if ( it.diffuseMap ) {
				glDeleteBuffers( 1, &it.diffuseMap );
			}
			if ( it.normalMap ) {
				glDeleteBuffers( 1, &it.normalMap );
			}
			if ( it.lightMap ) {
				glDeleteBuffers( 1, &it.lightMap );
			}
		}

		m_vertexBuffer.clear( );
		m_indexBuffer.clear( );
		m_textureBuffer.clear( );
		m_meshCache.clear( );
		m_model = Model( );
		ViewerGLCanvas::clear( );
	}

	void ModelViewer::setReader( FileReader* p_reader ) {
		if ( !m_glInitialized ) {
			// Could not initialize OpenGL
			return;
		}

		Ensure::isOfType<ModelReader>( p_reader );
		ViewerGLCanvas::setReader( p_reader );

		// Load model
		auto reader = this->modelReader( );
		m_model = reader->getModel( );

		// Create mesh cache
		m_meshCache.resize( m_model.numMeshes( ) );

		uint indexBase = 1;
		// Load mesh to mesh cache
		for ( int i = 0; i < static_cast<int>( m_model.numMeshes( ) ); i++ ) {
			auto& mesh = m_model.mesh( i );
			auto& cache = m_meshCache[i];

			this->loadMeshes( cache, mesh, indexBase );
		}

		// Create Vertex Buffer Object and Index Buffer Object
		m_vertexBuffer.resize( m_model.numMeshes( ) );
		m_indexBuffer.resize( m_model.numMeshes( ) );

		// Populate Buffer Object
		for ( int i = 0; i < static_cast<int>( m_meshCache.size( ) ); i++ ) {
			auto& cache = m_meshCache[i];
			auto& vbo = m_vertexBuffer[i];
			auto& ibo = m_indexBuffer[i];

			this->populateBuffers( vbo, ibo, cache );
		}

		// Load textures to texture map
		std::map<uint32, GLuint> textureMap;
		for ( int i = 0; i < static_cast<int>( m_model.numMaterial( ) ); i++ ) {
			auto& material = m_model.material( i );
			std::map<uint32, GLuint>::iterator it;

			// Load diffuse texture
			if ( material.diffuseMap ) {
				it = textureMap.find( material.diffuseMap );
				if ( it == textureMap.end( ) ) {
					textureMap.insert( std::pair<uint32, GLuint>( material.diffuseMap, this->loadTexture( material.diffuseMap ) ) );
				}
			}

			if ( material.normalMap ) {
				it = textureMap.find( material.normalMap );
				if ( it == textureMap.end( ) ) {
					textureMap.insert( std::pair<uint32, GLuint>( material.normalMap, this->loadTexture( material.normalMap ) ) );
				}
			}

			if ( material.lightMap ) {
				it = textureMap.find( material.lightMap );
				if ( it == textureMap.end( ) ) {
					textureMap.insert( std::pair<uint32, GLuint>( material.lightMap, this->loadTexture( material.lightMap ) ) );
				}
			}
		}

		// Create Texture Buffer Object
		m_textureBuffer.resize( m_model.numMaterial( ) );

		// Copy texture id from texture map to TBO
		for ( int i = 0; i < static_cast<int>( m_model.numMaterial( ) ); i++ ) {
			auto& material = m_model.material( i );
			auto& cache = m_textureBuffer[i];

			// Load diffuse texture
			if ( material.diffuseMap ) {
				cache.diffuseMap = textureMap.find( material.diffuseMap )->second;
			} else {
				cache.diffuseMap = 0;
			}

			if ( material.normalMap ) {
				cache.normalMap = textureMap.find( material.normalMap )->second;
			} else {
				cache.normalMap = 0;
			}

			if ( material.lightMap ) {
				cache.lightMap = textureMap.find( material.lightMap )->second;
			} else {
				cache.lightMap = 0;
			}
		}

		// Clear texture map
		textureMap.clear( );

		// Re-focus and re-render
		this->focus( );
		this->render( );
	}

	int ModelViewer::initGL( ) {
		wxLogMessage( wxT( "Initializing OpenGL..." ) );
		// Create OpenGL context
		m_glContext = new wxGLContext( this );
		if ( !m_glContext ) {
			wxLogMessage( wxT( "Unable to create OpenGL context." ) );
			return false;
		}

		SetCurrent( *m_glContext );

		// Initialize GLEW to setup the OpenGL Function pointers
		glewExperimental = true;
		GLenum err = glewInit( );
		if ( GLEW_OK != err ) {
			wxLogMessage( wxT( "GLEW: Could not initialize GLEW library.\nError : %s" ), wxString( glewGetErrorString( err ) ) );
			return false;
		}

		if ( !GLEW_VERSION_3_3 ) {
			wxLogMessage( wxT( "GLEW: The modelviewer required OpenGL 3.3 support!" ) );
			wxMessageBox( wxT( "GLEW: The modelviewer required OpenGL 3.3 support!" ), wxT( "" ), wxICON_ERROR );
			return false;
		}

		wxLogMessage( wxT( "GLEW version %s" ), wxString( glewGetString( GLEW_VERSION ) ) );
		wxLogMessage( wxT( "Running on a %s from %s" ), wxString( glGetString( GL_RENDERER ) ), wxString( glGetString( GL_VENDOR ) ) );
		wxLogMessage( wxT( "OpenGL version %s" ), wxString( glGetString( GL_VERSION ) ) );

		// Set background color
		glClearColor( 0.21f, 0.21f, 0.21f, 1.0f );

		// Enable multisampling, not really need since it was enabled at context creation
		glEnable( GL_MULTISAMPLE );

		// Enable depth test
		glEnable( GL_DEPTH_TEST );

		// Accept fragment if it closer to the camera than the former one
		glDepthFunc( GL_LESS );

		// Cull triangles which normal is not towards the camera
		// Remove lighting glitch cause by some triangles
		glEnable( GL_CULL_FACE );

		// Load shader
		m_mainShader.load( "..//data//shaders//shader.vert", "..//data//shaders//shader.frag" );
		m_normalVisualizerShader.load( "..//data//shaders//normalVisualizer.vert", "..//data//shaders//normalVisualizer.frag", "..//data//shaders//normalVisualizer.geom" );

		// Initialize text renderer stuff
		if ( !m_text.init( ) ) {
			wxLogMessage( wxT( "Could not initialize text renderer." ) );
			return false;
		}

		////////
		//GLubyte specularTextureData[] = { 224, 224, 224, 255 };
		//specularTexture = createDummyTexture( specularTextureData );

		// Create dummy texture
		GLubyte blackTextureData[] = { 0, 0, 0, 255 };
		m_dummyBlackTexture = createDummyTexture( blackTextureData );

		GLubyte whiteTextureData[] = { 255, 255, 255, 255 };
		m_dummyWhiteTexture = createDummyTexture( whiteTextureData );

		return true;
	}

	void ModelViewer::onPaintEvt( wxPaintEvent& p_event ) {
		wxPaintDC dc( this );

		this->render( );
	}

	void ModelViewer::render( ) {
		// Set the OpenGL viewport according to the client size of wxGLCanvas.
		wxSize ClientSize = this->GetClientSize( );
		glViewport( 0, 0, ClientSize.x, ClientSize.y );

		// Clear color buffer and depth buffer
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

		this->updateMatrices( );

		uint vertexCount = 0;
		uint triangleCount = 0;

		// Model matrix
		glm::mat4 model;
		// Transform the model
		model = glm::translate( model, glm::vec3( 0.0f, 0.0f, 0.0f ) );

		// todo: Draw each model

		// Draw meshes
		for ( uint i = 0; i < m_model.numMeshes( ); i++ ) {
			this->drawMesh( m_mainShader, model, i );

			vertexCount += m_model.mesh( i ).vertices.size( );
			triangleCount += m_model.mesh( i ).triangles.size( );
		}

		// Draw normal lines for debugging/visualization
		if ( m_statusVisualizeNormal ) {
			for ( uint i = 0; i < m_model.numMeshes( ); i++ ) {
				this->drawMesh( m_normalVisualizerShader, model, i );
			}
		}

		// todo: render light source (for debugging/visualization)

		// Draw status text
		if ( m_statusText ) {
			this->displayStatusText( vertexCount, triangleCount );
		}

		SwapBuffers( );
	}

	void ModelViewer::drawMesh( Shader p_shader, const glm::mat4 p_model, const uint p_meshIndex ) {
		auto& vbo = m_vertexBuffer[p_meshIndex];
		auto& ibo = m_indexBuffer[p_meshIndex];
		auto& cache = m_meshCache[p_meshIndex];

		auto materialIndex = m_model.mesh( p_meshIndex ).materialIndex;

		if ( m_statusCullFace ) {
			glEnable( GL_CULL_FACE );
		} else {
			glDisable( GL_CULL_FACE );
		}

		if ( m_statusWireframe ) {
			glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		} else {
			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}

		// Use the shader
		p_shader.use( );

		// View matrix
		glUniformMatrix4fv( glGetUniformLocation( p_shader.program, "view" ), 1, GL_FALSE, glm::value_ptr( m_camera.calculateViewMatrix( ) ) );
		glUniformMatrix4fv( glGetUniformLocation( p_shader.program, "model" ), 1, GL_FALSE, glm::value_ptr( p_model ) );

		// Use Texture Unit 0
		glActiveTexture( GL_TEXTURE0 );

		if ( m_statusTextured && !m_textureBuffer.empty( ) ) {
			if ( m_statusWireframe ) {
				// Black texture, for wireframe view
				glBindTexture( GL_TEXTURE_2D, m_dummyBlackTexture );
			} else if ( materialIndex >= 0 && m_textureBuffer[materialIndex].diffuseMap ) {
				// "Bind" the texture : all future texture functions will modify this texture
				glBindTexture( GL_TEXTURE_2D, m_textureBuffer[materialIndex].diffuseMap );
			}
		} else {
			if ( m_statusWireframe ) {
				// Black texture, for wireframe view
				glBindTexture( GL_TEXTURE_2D, m_dummyBlackTexture );
			} else {
				// White texture, no texture
				glBindTexture( GL_TEXTURE_2D, m_dummyWhiteTexture );
			}
		}

		// Set our "diffuseMap" sampler to user Texture Unit 0
		glUniform1i( glGetUniformLocation( m_mainShader.program, "diffuseMap" ), 0 );

		// Bind our normal texture in Texture Unit 1
		glActiveTexture( GL_TEXTURE1 );
		glBindTexture( GL_TEXTURE_2D, m_textureBuffer[materialIndex].normalMap);
		// Set our "normalMap" sampler to user Texture Unit 1
		glUniform1i( glGetUniformLocation( m_mainShader.program, "normalMap" ), 1 );

		// Bind our normal texture in Texture Unit 2
		//glActiveTexture( GL_TEXTURE2 );
		//glBindTexture( GL_TEXTURE_2D, specularTexture );
		// Set our "specularMap" sampler to user Texture Unit 2
		//glUniform1i( glGetUniformLocation( m_mainShader.program, "specularMap" ), 2 );

		// Bind Vertex Array Object
		glBindVertexArray( modelVAO );

		// positions
		glEnableVertexAttribArray( 0 );
		glBindBuffer( GL_ARRAY_BUFFER, vbo.vertexBuffer );
		glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, ( GLvoid* ) 0 );

		// normals
		glEnableVertexAttribArray( 1 );
		glBindBuffer( GL_ARRAY_BUFFER, vbo.normalBuffer );
		glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, 0, ( GLvoid* ) 0 );

		// texCoords
		glEnableVertexAttribArray( 2 );
		glBindBuffer( GL_ARRAY_BUFFER, vbo.uvBuffer );
		glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE, 0, ( GLvoid* ) 0 );

		// tangents
		glEnableVertexAttribArray( 3 );
		glBindBuffer( GL_ARRAY_BUFFER, vbo.tangentbuffer );
		glVertexAttribPointer( 3, 3, GL_FLOAT, GL_FALSE, 0, ( void* ) 0 );

		// bitangents
		glEnableVertexAttribArray( 4 );
		glBindBuffer( GL_ARRAY_BUFFER, vbo.bitangentbuffer );
		glVertexAttribPointer( 4, 3, GL_FLOAT, GL_FALSE, 0, ( void* ) 0 );

		// index buffer
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ibo.elementBuffer );

		// Draw the triangles!
		glDrawElements( GL_TRIANGLES, cache.indices.size( ), GL_UNSIGNED_INT, ( void* ) 0 );

		glDisableVertexAttribArray( 0 );
		glDisableVertexAttribArray( 1 );
		glDisableVertexAttribArray( 2 );
		glDisableVertexAttribArray( 3 );
		glDisableVertexAttribArray( 4 );

		// Unbind Vertex Array Object
		glBindVertexArray( 0 );

		// Unbind texture from Texture Unit 1
		//glActiveTexture( GL_TEXTURE1 );
		glBindTexture( GL_TEXTURE_2D, 0 );

		// Unbind texture from Texture Unit 0
		glActiveTexture( GL_TEXTURE0 );
		glBindTexture( GL_TEXTURE_2D, 0 );

		if ( m_statusWireframe ) {
			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}

	void ModelViewer::displayStatusText( const uint p_vertexCount, const uint p_triangleCount ) {
		wxSize ClientSize = this->GetClientSize( );

		// Send ClientSize variable to text renderer
		m_text.m_ClientSize = ClientSize;

		glm::vec3 color = glm::vec3( 1.0f );
		GLfloat scale = 1.0f;

		// Top-left text
		m_text.drawText( wxString::Format( wxT( "Meshes: %d" ), m_model.numMeshes( ) ), 0.0f, ClientSize.y - 12.0f, scale, color );
		m_text.drawText( wxString::Format( wxT( "Vertices: %d" ), p_vertexCount ), 0.0f, ClientSize.y - 24.0f, scale, color );
		m_text.drawText( wxString::Format( wxT( "Triangles: %d" ), p_triangleCount ), 0.0f, ClientSize.y - 36.0f, scale, color );

		// Bottom-left text
		m_text.drawText( wxT( "Zoom: Scroll wheel" ), 0.0f, 0.0f + 2.0f, scale, color );
		m_text.drawText( wxT( "Rotate: Left mouse button" ), 0.0f, 12.0f + 2.0f, scale, color );
		m_text.drawText( wxT( "Pan: Right mouse button" ), 0.0f, 24.0f + 2.0f, scale, color );
		m_text.drawText( wxT( "Focus: press F" ), 0.0f, 36.0f + 2.0f, scale, color );
		m_text.drawText( wxT( "Toggle back-face culling: press 4" ), 0.0f, 48.0f + 2.0f, scale, color );
		m_text.drawText( wxT( "Toggle texture: press 3" ), 0.0f, 60.0f + 2.0f, scale, color );
		m_text.drawText( wxT( "Toggle wireframe: press 2" ), 0.0f, 72.0f + 2.0f, scale, color );
		m_text.drawText( wxT( "Toggle status text: press 1" ), 0.0f, 84.0f + 2.0f, scale, color );
	}

	void ModelViewer::loadMeshes( MeshCache& p_cache, const Mesh& p_mesh, uint p_indexBase ) {
		// Tempoarary buffers
		std::vector<glm::vec3> temp_vertices;
		std::vector<glm::vec3> temp_normals;
		std::vector<glm::vec2> temp_uvs;

		// Read positions
		for ( uint i = 0; i < p_mesh.vertices.size( ); i++ ) {
			auto tempVertices = p_mesh.vertices[i].position;
			temp_vertices.push_back( tempVertices );
		}

		// Read normals
		if ( p_mesh.hasNormal ) {
			for ( uint i = 0; i < p_mesh.vertices.size( ); i++ ) {
				auto tempNormals = p_mesh.vertices[i].normal;
				temp_normals.push_back( tempNormals );
			}
		}

		// Read UVs
		if ( p_mesh.hasUV ) {
			for ( uint i = 0; i < p_mesh.vertices.size( ); i++ ) {
				auto tempUvs = p_mesh.vertices[i].uv;
				temp_uvs.push_back( tempUvs );
			}
		}

		// Tempoarary buffers
		std::vector<uint> vertexIndices, normalIndices, uvIndices;

		// Read faces
		for ( uint i = 0; i < p_mesh.triangles.size( ); i++ ) {
			const Triangle& triangle = p_mesh.triangles[i];

			uint vertexIndex[3], uvIndex[3], normalIndex[3];

			for ( uint j = 0; j < 3; j++ ) {
				uint index = triangle.indices[j] + p_indexBase;

				vertexIndex[j] = index;

				// Normal reference
				if ( p_mesh.hasNormal ) {
					normalIndex[j] = index;
				}

				// UV reference
				if ( p_mesh.hasUV ) {
					uvIndex[j] = index;
				}
			}

			vertexIndices.push_back( vertexIndex[0] );
			vertexIndices.push_back( vertexIndex[1] );
			vertexIndices.push_back( vertexIndex[2] );

			if ( p_mesh.hasNormal ) {
				normalIndices.push_back( normalIndex[0] );
				normalIndices.push_back( normalIndex[1] );
				normalIndices.push_back( normalIndex[2] );
			}

			if ( p_mesh.hasUV ) {
				uvIndices.push_back( uvIndex[0] );
				uvIndices.push_back( uvIndex[1] );
				uvIndices.push_back( uvIndex[2] );
			}
		}
		p_indexBase += p_mesh.vertices.size( );

		// Temporary buffer before send to VBO indexer
		std::vector<glm::vec3> vertices;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec2> uvs;
		std::vector<glm::vec3> tangents;
		std::vector<glm::vec3> bitangents;

		// For each vertex of each triangle
		for ( uint i = 0; i < vertexIndices.size( ); i++ ) {
			// Get the indices of its attributes
			uint vertexIndex = vertexIndices[i];
			// Get the indices of its attributes
			glm::vec3 vertex = temp_vertices[vertexIndex - 1];
			// Put the attributes in buffers
			vertices.push_back( vertex );

			if ( p_mesh.hasNormal ) {
				uint normalIndex = normalIndices[i];
				glm::vec3 normal = temp_normals[normalIndex - 1];
				normals.push_back( normal );
			}

			if ( p_mesh.hasUV ) {
				uint uvIndex = uvIndices[i];
				glm::vec2 uv = temp_uvs[uvIndex - 1];
				uvs.push_back( uv );
			}
		}

		this->computeTangent(
			vertices, normals, uvs,
			tangents, bitangents
			);

		this->indexVBO( vertices, normals, uvs, tangents, bitangents,
			p_cache.indices, p_cache.vertices, p_cache.normals, p_cache.uvs, p_cache.tangents, p_cache.bitangents );
	}

	void ModelViewer::computeTangent(
		std::vector<glm::vec3>& in_vertices,
		std::vector<glm::vec3>& in_normals,
		std::vector<glm::vec2>& in_uvs,
		std::vector<glm::vec3>& out_tangents,
		std::vector<glm::vec3>& out_bitangents
		) {

		for ( uint i = 0; i < in_vertices.size( ); i += 3 ) {
			// Shortcuts for vertices
			glm::vec3& v0 = in_vertices[i + 0];
			glm::vec3& v1 = in_vertices[i + 1];
			glm::vec3& v2 = in_vertices[i + 2];

			// Shortcuts for UVs
			glm::vec2& uv0 = in_uvs[i + 0];
			glm::vec2& uv1 = in_uvs[i + 1];
			glm::vec2& uv2 = in_uvs[i + 2];

			// Edges of the triangle : postion delta
			glm::vec3 deltaPos1 = v1 - v0;
			glm::vec3 deltaPos2 = v2 - v0;

			// UV delta
			glm::vec2 deltaUV1 = uv1 - uv0;
			glm::vec2 deltaUV2 = uv2 - uv0;

			// calculate tangent/bitangent vectors
			float r = 1.0f / ( deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x );
			glm::vec3 tangent = ( deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y ) * r;
			glm::vec3 bitangent = ( deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x ) * r;

			// Set the same tangent for all three vertices of the triangle.
			// They will be merged later in VBO indexer.
			out_tangents.push_back( tangent );
			out_tangents.push_back( tangent );
			out_tangents.push_back( tangent );

			// Same thing for binormals
			out_bitangents.push_back( bitangent );
			out_bitangents.push_back( bitangent );
			out_bitangents.push_back( bitangent );
		}

		// We will do this in vertex shader
		/*for ( uint i = 0; i < in_vertices.size( ); i += 1 ) {
			glm::vec3& n = in_normals[i];
			glm::vec3& t = out_tangents[i];
			glm::vec3& b = out_bitangents[i];

			// Gram-Schmidt orthogonalize
			t = glm::normalize( t - n * glm::dot( n, t ) );

			// Calculate handedness
			if ( glm::dot( glm::cross( n, t ), b ) < 0.0f ) {
				t = t * -1.0f;
			}
		}*/
	}

	bool ModelViewer::getSimilarVertexIndex( PackedVertex & p_packed, std::map<PackedVertex, uint>& p_vertexToOutIndex, uint& p_result ) {
		std::map<PackedVertex, uint>::iterator it = p_vertexToOutIndex.find( p_packed );
		if ( it == p_vertexToOutIndex.end( ) ) {
			return false;
		} else {
			p_result = it->second;
			return true;
		}
	}

	void ModelViewer::indexVBO(
		std::vector<glm::vec3>& in_vertices,
		std::vector<glm::vec3>& in_normals,
		std::vector<glm::vec2>& in_uvs,
		std::vector<glm::vec3>& in_tangents,
		std::vector<glm::vec3>& in_bitangents,
		std::vector<uint>& out_indices,
		std::vector<glm::vec3>& out_vertices,
		std::vector<glm::vec3>& out_normals,
		std::vector<glm::vec2>& out_uvs,
		std::vector<glm::vec3>& out_tangents,
		std::vector<glm::vec3>& out_bitangents ) {

		std::map<PackedVertex, uint> VertexToOutIndex;

		// For each input vertex
		for ( uint i = 0; i < in_vertices.size( ); i++ ) {
			glm::vec3 vertices;
			glm::vec3 normals;
			glm::vec2 uvs;

			vertices = in_vertices[i];

			if ( !in_normals.empty( ) ) {
				normals = in_normals[i];
			}

			if ( !in_uvs.empty( ) ) {
				uvs = in_uvs[i];
			}

			PackedVertex packed = { vertices, normals, uvs };

			// Try to find a similar vertex in out_XXXX
			uint index;
			bool found = getSimilarVertexIndex( packed, VertexToOutIndex, index );

			if ( found ) { // A similar vertex is already in the VBO, use it instead !
				out_indices.push_back( index );

				// Average the tangents and the bitangents
				out_tangents[index] += in_tangents[i];
				out_bitangents[index] += in_bitangents[i];
			} else { // If not, it needs to be added in the output data.
				out_vertices.push_back( in_vertices[i] );

				if ( !in_normals.empty( ) ) {
					out_normals.push_back( in_normals[i] );
				}

				if ( !in_uvs.empty( ) ) {
					out_uvs.push_back( in_uvs[i] );
				}

				out_tangents.push_back( in_tangents[i] );
				out_bitangents.push_back( in_bitangents[i] );
				uint newindex = ( uint ) out_vertices.size( ) - 1;
				out_indices.push_back( newindex );
				VertexToOutIndex[packed] = newindex;
			}
		}
	}

	void ModelViewer::populateBuffers( VBO& p_vbo, IBO& p_ibo, const MeshCache& p_cache ) {
		// Generate Vertex Array Object
		glGenVertexArrays( 1, &modelVAO );
		// Bind Vertex Array Object
		glBindVertexArray( modelVAO );

		// Load the model data to VBO
		glGenBuffers( 1, &p_vbo.vertexBuffer );
		glBindBuffer( GL_ARRAY_BUFFER, p_vbo.vertexBuffer );
		glBufferData( GL_ARRAY_BUFFER, p_cache.vertices.size( ) * sizeof( glm::vec3 ), &p_cache.vertices[0], GL_STATIC_DRAW );

		if ( p_cache.normals.data( ) ) {
			glGenBuffers( 1, &p_vbo.normalBuffer );
			glBindBuffer( GL_ARRAY_BUFFER, p_vbo.normalBuffer );
			glBufferData( GL_ARRAY_BUFFER, p_cache.normals.size( ) * sizeof( glm::vec3 ), &p_cache.normals[0], GL_STATIC_DRAW );
		}

		if ( p_cache.uvs.data( ) ) {
			glGenBuffers( 1, &p_vbo.uvBuffer );
			glBindBuffer( GL_ARRAY_BUFFER, p_vbo.uvBuffer );
			glBufferData( GL_ARRAY_BUFFER, p_cache.uvs.size( ) * sizeof( glm::vec2 ), &p_cache.uvs[0], GL_STATIC_DRAW );
		}

		glGenBuffers( 1, &p_vbo.tangentbuffer );
		glBindBuffer( GL_ARRAY_BUFFER, p_vbo.tangentbuffer );
		glBufferData( GL_ARRAY_BUFFER, p_cache.tangents.size( ) * sizeof( glm::vec3 ), &p_cache.tangents[0], GL_STATIC_DRAW );

		glGenBuffers( 1, &p_vbo.bitangentbuffer );
		glBindBuffer( GL_ARRAY_BUFFER, p_vbo.bitangentbuffer );
		glBufferData( GL_ARRAY_BUFFER, p_cache.bitangents.size( ) * sizeof( glm::vec3 ), &p_cache.bitangents[0], GL_STATIC_DRAW );

		// Buffer for the indices
		glGenBuffers( 1, &p_ibo.elementBuffer );
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, p_ibo.elementBuffer );
		glBufferData( GL_ELEMENT_ARRAY_BUFFER, p_cache.indices.size( ) * sizeof( uint ), &p_cache.indices[0], GL_STATIC_DRAW );

		// Unbind Vertex Array Object
		glBindVertexArray( 0 );
	}

	GLuint ModelViewer::createDummyTexture( const GLubyte* p_data ) {
		// Create one OpenGL texture
		GLuint Texture;
		glGenTextures( 1, &Texture );

		// "Bind" the newly created texture
		glBindTexture( GL_TEXTURE_2D, Texture );

		// Give the image to OpenGL
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, p_data );

		glBindTexture( GL_TEXTURE_2D, 0 );

		return Texture;
	}

	GLuint ModelViewer::loadTexture( const uint p_fileId ) {
		auto entryNumber = m_datFile.entryNumFromFileId( p_fileId );
		auto fileData = m_datFile.readEntry( entryNumber );

		// Bail if read failed
		if ( fileData.GetSize( ) == 0 ) {
			return false;
		}

		// Convert to image
		ANetFileType fileType;
		m_datFile.identifyFileType( fileData.GetPointer( ), fileData.GetSize( ), fileType );
		auto reader = FileReader::readerForData( fileData, m_datFile, fileType );

		// Bail if not an image
		auto imgReader = dynamic_cast<ImageReader*>( reader );
		if ( !imgReader ) {
			deletePointer( reader );
			return false;
		}

		// Get image in wxImage
		auto imageData = imgReader->getImage( );

		if ( !imageData.IsOk( ) ) {
			deletePointer( reader );
			return false;
		}

		// wxImage store seperate alpha channel if present
		GLubyte* bitmapData = imageData.GetData( );
		GLubyte* alphaData = imageData.GetAlpha( );

		int imageWidth = imageData.GetWidth( );
		int imageHeight = imageData.GetHeight( );
		int bytesPerPixel = imageData.HasAlpha( ) ? 4 : 3;
		int imageSize = imageWidth * imageHeight * bytesPerPixel;

		Array<GLubyte> image( imageSize );

		// Merge wxImage alpha channel to RGBA
#pragma omp parallel for
		for ( int y = 0; y < imageHeight; y++ ) {
			for ( int x = 0; x < imageWidth; x++ ) {
				image[( x + y * imageWidth ) * bytesPerPixel + 0] = bitmapData[( x + y * imageWidth ) * 3];
				image[( x + y * imageWidth ) * bytesPerPixel + 1] = bitmapData[( x + y * imageWidth ) * 3 + 1];
				image[( x + y * imageWidth ) * bytesPerPixel + 2] = bitmapData[( x + y * imageWidth ) * 3 + 2];

				if ( bytesPerPixel == 4 ) {
					image[( x + y * imageWidth ) * bytesPerPixel + 3] = alphaData[x + y * imageWidth];
				}
			}
		}

		// Generate texture ID
		GLuint TextureID;
		glGenTextures( 1, &TextureID );

		// Assign texture to ID
		glBindTexture( GL_TEXTURE_2D, TextureID );

		// Give the image to OpenGL
		glTexImage2D( GL_TEXTURE_2D,
			0,
			bytesPerPixel,
			imageWidth,
			imageHeight,
			0,
			imageData.HasAlpha( ) ? GL_RGBA : GL_RGB,
			GL_UNSIGNED_BYTE,
			image.GetPointer( ) );

		glGenerateMipmap( GL_TEXTURE_2D );

		// Texture parameters

		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );

		// Trilinear texture filtering
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );

		// Anisotropic texture filtering
		//glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8 );

		glBindTexture( GL_TEXTURE_2D, 0 );

		deletePointer( reader );

		return TextureID;
	}

	void ModelViewer::updateMatrices( ) {
		// All models are located at 0,0,0 with no rotation, so no world matrix is needed

		// Calculate minZ/maxZ
		auto bounds = m_model.bounds( );
		auto size = bounds.size( );
		auto distance = m_camera.distance( );
		auto extents = glm::vec3( size.x * 0.5f, size.y * 0.5f, size.z * 0.5f );

		auto maxSize = ::sqrt( extents.x * extents.x + extents.y * extents.y + extents.z * extents.z );
		auto maxZ = ( maxSize + distance ) * 10.0f;
		auto minZ = maxZ * 0.001f;

		//minZ 0.1f
		//maxZ 100.0f

		const wxSize ClientSize = this->GetClientSize( );
		float aspectRatio = ( static_cast<float>( ClientSize.x ) / static_cast<float>( ClientSize.y ) );
		auto fov = ( 5.0f / 12.0f ) * glm::pi<float>( );

		// Projection matrix
		auto projection = glm::perspective( fov, aspectRatio, static_cast<float>( minZ ), static_cast<float>( maxZ ) );

		m_mainShader.use( );

		// Send projection matrix to main shader
		glUniformMatrix4fv( glGetUniformLocation( m_mainShader.program, "projection" ), 1, GL_FALSE, glm::value_ptr( projection ) );

		// View position
		glUniform3fv( glGetUniformLocation( m_mainShader.program, "viewPos" ), 1, glm::value_ptr( m_camera.position( ) ) );

		// Set light position to ... (currently is front of the model)
		lightPos = bounds.center( ) + glm::vec3( 0, 25, maxZ );
		glUniform3fv( glGetUniformLocation( m_mainShader.program, "lightPos" ), 1, glm::value_ptr( lightPos ) );

		m_normalVisualizerShader.use( );

		// Send projection matrix to normal visualizer shader
		glUniformMatrix4fv( glGetUniformLocation( m_normalVisualizerShader.program, "projection" ), 1, GL_FALSE, glm::value_ptr( projection ) );
	}

	void ModelViewer::focus( ) {
		float fov = ( 5.0f / 12.0f ) * glm::pi<float>( );

		uint meshCount = m_model.numMeshes( );
		if ( !meshCount ) {
			return;
		}

		// Calculate complete bounds
		Bounds bounds = m_model.bounds( );
		float height = bounds.max.y - bounds.min.y;
		if ( height <= 0 ) {
			return;
		}

		float distance = bounds.min.z - ( ( height * 0.5f ) / ::tanf( fov * 0.5f ) );
		if ( distance < 0 ) {
			distance *= -1;
		}

		// Update camera and render
		m_camera.setPivot( bounds.center( ) );
		m_camera.setDistance( distance );
		this->render( );
	}

	void ModelViewer::onMotionEvt( wxMouseEvent& p_event ) {
		if ( m_lastMousePos.x == std::numeric_limits<int>::min( ) &&
			m_lastMousePos.y == std::numeric_limits<int>::min( ) ) {
			m_lastMousePos = p_event.GetPosition( );
		}

		// Yaw/Pitch
		if ( p_event.LeftIsDown( ) ) {
			float rotateSpeed = 0.5f * ( glm::pi<float>( ) / 180.0f );   // 0.5 degrees per pixel
			m_camera.addYaw( rotateSpeed * -( p_event.GetX( ) - m_lastMousePos.x ) );
			m_camera.addPitch( rotateSpeed * ( p_event.GetY( ) - m_lastMousePos.y ) );
			this->render( );
		}

		// Pan
		if ( p_event.RightIsDown( ) ) {
			float xPan = ( p_event.GetX( ) - m_lastMousePos.x );
			float yPan = ( p_event.GetY( ) - m_lastMousePos.y );
			m_camera.pan( xPan, yPan );
			this->render( );
		}

		m_lastMousePos = p_event.GetPosition( );
	}

	void ModelViewer::onMouseWheelEvt( wxMouseEvent& p_event ) {
		float zoomSteps = static_cast<float>( p_event.GetWheelRotation( ) ) / static_cast<float>( p_event.GetWheelDelta( ) );
		m_camera.multiplyDistance( -zoomSteps );
		this->render( );
	}

	void ModelViewer::onKeyDownEvt( wxKeyEvent& p_event ) {
		if ( p_event.GetKeyCode( ) == 'F' ) {
			this->focus( );
		} else if ( p_event.GetKeyCode( ) == '1' ) {
			m_statusText = !m_statusText;
		} else if ( p_event.GetKeyCode( ) == '2' ) {
			m_statusWireframe = !m_statusWireframe;
		} else if ( p_event.GetKeyCode( ) == '3' ) {
			m_statusTextured = !m_statusTextured;
		} else if ( p_event.GetKeyCode( ) == '4' ) {
			m_statusCullFace = !m_statusCullFace;
		} else if ( p_event.GetKeyCode( ) == '=' ) {
			m_statusVisualizeNormal = !m_statusVisualizeNormal;
		}
	}

	void ModelViewer::onClose( wxCloseEvent& evt ) {
		m_renderTimer->Stop( );
		evt.Skip( );
	}

}; // namespace gw2b
