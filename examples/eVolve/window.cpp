
/* Copyright (c) 2007-2010, Stefan Eilemann <eile@equalizergraphics.com> 
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 2.1 as published
 * by the Free Software Foundation.
 *  
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "window.h"
#include "pipe.h"

using namespace std;

namespace eVolve
{

bool Window::configInit( const uint32_t initID )
{
    // Enforce alpha channel, since we need one for rendering
    setIAttribute( IATTR_PLANES_ALPHA, 8 );

    return eq::Window::configInit( initID );
}

bool Window::configInitGL( const uint32_t initID )
{
    Pipe*     pipe     = static_cast<Pipe*>( getPipe() );
    Renderer* renderer = pipe->getRenderer();

    if( !renderer )
        return false;

    if( !GLEW_ARB_shader_objects )
    {
        setErrorMessage( "eVolve needs GL_ARB_shader_objects extension" );
        return false;
    }
    if( !GLEW_EXT_blend_func_separate )
    {
        setErrorMessage( "eVolve needs GL_EXT_blend_func_separate extension" );
        return false;
    }
    if( !GLEW_ARB_multitexture )
    {
        setErrorMessage( "eVolve needs GLEW_ARB_multitexture extension" );
        return false;
    }

    glEnable( GL_SCISSOR_TEST ); // needed to constrain channel viewport

    glClear( GL_COLOR_BUFFER_BIT );
    swapBuffers();
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    renderer->glewSetContext( glewGetContext( ));

    if( !renderer->loadShaders( ))
    {
        setErrorMessage( "Can't load shaders" );
        return false;
    }

    _loadLogo();
    return true;
}

static const char* _logoTextureName = "eVolve_logo";

void Window::_loadLogo()
{
    eq::Window::ObjectManager* objects = getObjectManager();

    if( objects->getTexture( _logoTextureName ) != 
        eq::Window::ObjectManager::INVALID )
    {
        // Already loaded by first window
        const eq::Pipe* pipe        = getPipe();
        const Window*   firstWindow = static_cast< Window* >
                                          ( pipe->getWindows()[0] );
        
        _logoTexture = firstWindow->_logoTexture;
        _logoSize    = firstWindow->_logoSize;
        return;
    }

    eq::Image image;
    if( !image.readImage( "logo.rgb", eq::Frame::BUFFER_COLOR ) &&
        !image.readImage( "examples/eVolve/logo.rgb", eq::Frame::BUFFER_COLOR ))
    {
        EQWARN << "Can't load overlay logo 'logo.rgb'" << endl;
        return;
    }

    _logoTexture = objects->newTexture( _logoTextureName );
    EQASSERT( _logoTexture != eq::Window::ObjectManager::INVALID );

    const eq::PixelViewport& pvp = image.getPixelViewport();
    _logoSize.x() = pvp.w;
    _logoSize.y() = pvp.h;

    glBindTexture( GL_TEXTURE_RECTANGLE_ARB, _logoTexture );
    glTexImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, 
                  image.getFormat( eq::Frame::BUFFER_COLOR ),
                  _logoSize.x(), _logoSize.y(), 0,
                  image.getFormat( eq::Frame::BUFFER_COLOR ), 
                  image.getType( eq::Frame::BUFFER_COLOR ),
                  image.getPixelPointer( eq::Frame::BUFFER_COLOR ));

    EQINFO << "Created logo texture of size " << _logoSize << endl;
}


void Window::swapBuffers()
{
    const Pipe*         pipe      = static_cast<Pipe*>( getPipe( ));
    const FrameData&    frameData = pipe->getFrameData();
    const eq::Channels& channels  = getChannels();

    if( frameData.useStatistics() && !channels.empty( ))
        EQ_GL_CALL( channels.back()->drawStatistics( ));

    eq::Window::swapBuffers();
}
}
