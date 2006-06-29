
/* Copyright (c) 2005, Stefan Eilemann <eile@equalizergraphics.com> 
   All rights reserved. */

#include "window.h"

#include "channel.h"
#include "commands.h"
#include "configEvent.h"
#include "event.h"
#include "eventThread.h"
#include "global.h"
#include "nodeFactory.h"
#include "object.h"
#include "packets.h"
#include "windowEvent.h"

#include <eq/net/barrier.h>

using namespace eq;
using namespace eqBase;
using namespace std;

eq::Window::Window()
        : eqNet::Object( eq::Object::TYPE_WINDOW, CMD_WINDOW_CUSTOM ),
#ifdef GLX
          _xDrawable(0),
          _glXContext(NULL),
#endif
#ifdef CGL
          _cglContext( NULL ),
#endif
          _pipe(NULL)
{
    registerCommand( CMD_WINDOW_CREATE_CHANNEL, this,
                     reinterpret_cast<CommandFcn>(
                         &eq::Window::_cmdCreateChannel ));
    registerCommand( CMD_WINDOW_DESTROY_CHANNEL, this,
                     reinterpret_cast<CommandFcn>(
                         &eq::Window::_cmdDestroyChannel ));
    registerCommand( CMD_WINDOW_INIT, this, reinterpret_cast<CommandFcn>(
                         &eq::Window::_pushRequest ));
    registerCommand( REQ_WINDOW_INIT, this, reinterpret_cast<CommandFcn>(
                         &eq::Window::_reqInit ));
    registerCommand( CMD_WINDOW_EXIT, this, reinterpret_cast<CommandFcn>( 
                         &eq::Window::_pushRequest ));
    registerCommand( REQ_WINDOW_EXIT, this, reinterpret_cast<CommandFcn>( 
                         &eq::Window::_reqExit ));
    registerCommand( CMD_WINDOW_SWAP, this, reinterpret_cast<CommandFcn>(
                         &eq::Window::_pushRequest));
    registerCommand( REQ_WINDOW_SWAP, this, reinterpret_cast<CommandFcn>(
                         &eq::Window::_reqSwap));
    registerCommand( CMD_WINDOW_SWAP_WITH_BARRIER, this,
                     reinterpret_cast<CommandFcn>( &eq::Window::_pushRequest ));
    registerCommand( REQ_WINDOW_SWAP_WITH_BARRIER, this,
                     reinterpret_cast<CommandFcn>(
                         &eq::Window::_reqSwapWithBarrier));
    registerCommand( CMD_WINDOW_STARTFRAME, this, reinterpret_cast<CommandFcn>(
                         &eq::Window::_pushRequest));
    registerCommand( REQ_WINDOW_STARTFRAME, this, reinterpret_cast<CommandFcn>(
                         &eq::Window::_reqStartFrame));
    registerCommand( CMD_WINDOW_ENDFRAME, this, reinterpret_cast<CommandFcn>(
                         &eq::Window::_pushRequest));
    registerCommand( REQ_WINDOW_ENDFRAME, this, reinterpret_cast<CommandFcn>(
                         &eq::Window::_reqEndFrame));
}

eq::Window::~Window()
{
}

void eq::Window::_addChannel( Channel* channel )
{
    _channels.push_back( channel );
    channel->_window = this;
}

void eq::Window::_removeChannel( Channel* channel )
{
    vector<Channel*>::iterator iter = find( _channels.begin(), _channels.end(), 
                                            channel );
    if( iter == _channels.end( ))
        return;
    
    _channels.erase( iter );
    channel->_window = NULL;
}

//---------------------------------------------------------------------------
// command handlers
//---------------------------------------------------------------------------
eqNet::CommandResult eq::Window::_pushRequest( eqNet::Node* node,
                                               const eqNet::Packet* packet )
{
    if( _pipe )
        return _pipe->pushRequest( node, packet );

    return _cmdUnknown( node, packet );
}

eqNet::CommandResult eq::Window::_cmdCreateChannel( eqNet::Node* node,
                                                    const eqNet::Packet* pkg )
{
    WindowCreateChannelPacket* packet = (WindowCreateChannelPacket*)pkg;
    EQINFO << "Handle create channel " << packet << endl;

    Channel* channel = Global::getNodeFactory()->createChannel();
    
    getConfig()->addRegisteredObject( packet->channelID, channel, 
                                      eqNet::Object::SHARE_NODE );
    _addChannel( channel );
    return eqNet::COMMAND_HANDLED;
}

eqNet::CommandResult eq::Window::_cmdDestroyChannel(eqNet::Node* node, 
                                                    const eqNet::Packet* pkg)
{
    WindowDestroyChannelPacket* packet = (WindowDestroyChannelPacket*)pkg;
    EQINFO << "Handle destroy channel " << packet << endl;

    Config*  config  = getConfig();
    Channel* channel = (Channel*)config->pollObject(packet->channelID);
    if( !channel )
        return eqNet::COMMAND_HANDLED;

    _removeChannel( channel );
    EQASSERT( channel->getRefCount() == 1 );
    config->deregisterObject( channel );
    
    return eqNet::COMMAND_HANDLED;
}

eqNet::CommandResult eq::Window::_reqInit( eqNet::Node* node,
                                           const eqNet::Packet* pkg )
{
    WindowInitPacket* packet = (WindowInitPacket*)pkg;
    EQINFO << "handle window init " << packet << endl;

    if( packet->pvp.isValid( ))
        setPixelViewport( packet->pvp );
    else
        setViewport( packet->vp );

    _name = packet->name;

    WindowInitReplyPacket reply( packet );
    reply.result = init( packet->initID );

    if( !reply.result )
    {
        node->send( reply );
        return eqNet::COMMAND_HANDLED;
    }

    const WindowSystem windowSystem =  _pipe->getWindowSystem();
    switch( windowSystem )
    {
#ifdef GLX
        case WINDOW_SYSTEM_GLX:
            if( !_xDrawable || !_glXContext )
            {
                EQERROR << "init() did not provide a drawable and context" 
                        << endl;
                reply.result = false;
                node->send( reply );
                return eqNet::COMMAND_HANDLED;
            }
            break;
#endif
#ifdef CGL
        case WINDOW_SYSTEM_CGL:
            if( !_cglContext )
            {
                EQERROR << "init() did not provide an OpenGL context" << endl;
                reply.result = false;
                node->send( reply );
                return eqNet::COMMAND_HANDLED;
            }
            // TODO: pvp
            break;
#endif

        default: EQUNIMPLEMENTED;
    }

    reply.pvp = _pvp;
    node->send( reply );

    EventThread* thread = EventThread::get( windowSystem );
    thread->addWindow( this );

    return eqNet::COMMAND_HANDLED;
}

eqNet::CommandResult eq::Window::_reqExit( eqNet::Node* node,
                                           const eqNet::Packet* pkg )
{
    WindowExitPacket* packet = (WindowExitPacket*)pkg;
    EQINFO << "handle window exit " << packet << endl;

    EventThread* thread = EventThread::get( _pipe->getWindowSystem( ));
    thread->removeWindow( this );

    exit();

    WindowExitReplyPacket reply( packet );
    node->send( reply );
    return eqNet::COMMAND_HANDLED;
}

eqNet::CommandResult eq::Window::_reqSwap(eqNet::Node* node, 
                                      const eqNet::Packet* pkg)
{
    WindowSwapPacket* packet = (WindowSwapPacket*)pkg;
    EQVERB << "handle swap " << packet << endl;

    swapBuffers();
    return eqNet::COMMAND_HANDLED;
}

eqNet::CommandResult eq::Window::_reqSwapWithBarrier(eqNet::Node* node,
                                                 const eqNet::Packet* pkg)
{
    WindowSwapWithBarrierPacket* packet = (WindowSwapWithBarrierPacket*)pkg;
    EQVERB << "handle swap with barrier " << packet << endl;

    eqNet::Session* session = getSession();
    eqNet::Object*  object  = session->getObject( packet->barrierID );
    EQASSERT( dynamic_cast<eqNet::Barrier*>( object ) );

    eqNet::Barrier* barrier = (eqNet::Barrier*)object;
    finish();
    barrier->enter();
    swapBuffers();
    return eqNet::COMMAND_HANDLED;
}


eqNet::CommandResult eq::Window::_reqStartFrame(eqNet::Node* node, 
                                      const eqNet::Packet* pkg)
{
    WindowStartFramePacket* packet = (WindowStartFramePacket*)pkg;
    EQVERB << "handle startFrame " << packet << endl;

    if( packet->makeCurrent )
        makeCurrent();

    startFrame( packet->frameID );
    return eqNet::COMMAND_HANDLED;
}

eqNet::CommandResult eq::Window::_reqEndFrame(eqNet::Node* node, 
                                      const eqNet::Packet* pkg)
{
    WindowEndFramePacket* packet = (WindowEndFramePacket*)pkg;
    EQVERB << "handle endFrame " << packet << endl;

    endFrame( packet->frameID );
    return eqNet::COMMAND_HANDLED;
}

//======================================================================
// pipe-thread methods
//======================================================================

//----------------------------------------------------------------------
// viewport
//----------------------------------------------------------------------
void eq::Window::setPixelViewport( const PixelViewport& pvp )
{
    if( !pvp.isValid( ))
        return;

    _pvp = pvp;
    _vp.invalidate();

    if( !_pipe )
        return;
    
    const PixelViewport& pipePVP = _pipe->getPixelViewport();
    if( pipePVP.isValid( ))
        _vp = pvp / pipePVP;

    EQINFO << "Window pvp set: " << _pvp << ":" << _vp << endl;
}

void eq::Window::setViewport( const Viewport& vp )
{
    if( !vp.isValid( ))
        return;
    
    _vp = vp;
    _pvp.invalidate();

    if( !_pipe )
        return;

    PixelViewport pipePVP = _pipe->getPixelViewport();
    if( pipePVP.isValid( ))
    {
        pipePVP.x = 0;
        pipePVP.y = 0;
        _pvp = pipePVP * vp;
    }
    EQINFO << "Window vp set: " << _pvp << ":" << _vp << endl;
}


//----------------------------------------------------------------------
// init
//----------------------------------------------------------------------
bool eq::Window::init( const uint32_t initID )
{
    const WindowSystem windowSystem = _pipe->getWindowSystem();
    switch( windowSystem )
    {
        case WINDOW_SYSTEM_GLX:
            if( !initGLX( ))
                return false;
            break;

        case WINDOW_SYSTEM_CGL:
            if( !initCGL( ))
                return false;

        default:
            EQERROR << "Unknown windowing system: " << windowSystem << endl;
            return false;
    }
    return initGL( initID );
}

bool eq::Window::initGL( const uint32_t initID )
{
    glEnable( GL_SCISSOR_TEST ); // needed to constrain channel viewport
    glEnable( GL_DEPTH_TEST );
    glDepthFunc (GL_LESS);

    glEnable( GL_LIGHTING );
    glEnable( GL_LIGHT0 );

    glColorMaterial( GL_FRONT_AND_BACK, GL_DIFFUSE );
    glEnable( GL_COLOR_MATERIAL );

    glClearDepth( 1. );
    glClearColor( .7, .5, .5, 1. );

    glClear( GL_COLOR_BUFFER_BIT );
    swapBuffers();
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    return true;
}

#ifdef GLX
static Bool WaitForNotify(Display *, XEvent *e, char *arg)
{ return (e->type == MapNotify) && (e->xmap.window == (::Window)arg); }
#endif

bool eq::Window::initGLX()
{
#ifdef GLX
    Display *display = _pipe->getXDisplay();
    if( !display ) 
        return false;

    int screen  = DefaultScreen( display );
    XID parent  = RootWindow( display, screen );

    int attributes[100], *aptr=attributes;    
    *aptr++ = GLX_RGBA;
    *aptr++ = 1;
    *aptr++ = GLX_RED_SIZE;
    *aptr++ = 8;
    //*aptr++ = GLX_ALPHA_SIZE;
    //*aptr++ = 1;
    *aptr++ = GLX_DEPTH_SIZE;
    *aptr++ = 1;
    *aptr++ = GLX_STENCIL_SIZE;
    *aptr++ = 8;
    *aptr++ = GLX_DOUBLEBUFFER;
    *aptr = None;

    XVisualInfo *visInfo = glXChooseVisual( display, screen, attributes );
    if ( !visInfo )
    {
        EQERROR << "Could not find a matching visual\n" << endl;
        return false;
    }

    XSetWindowAttributes wa;
    wa.colormap          = XCreateColormap( display, parent, visInfo->visual,
                                            AllocNone );
    wa.background_pixmap = None;
    wa.border_pixel      = 0;
    wa.event_mask        = StructureNotifyMask | VisibilityChangeMask;
    wa.override_redirect = True;

    XID drawable = XCreateWindow( display, parent, 
                                  _pvp.x, _pvp.y, _pvp.w, _pvp.h,
                                  0, visInfo->depth, InputOutput,
                                  visInfo->visual, CWBackPixmap|CWBorderPixel|
                                  CWEventMask|CWColormap/*|CWOverrideRedirect*/,
                                  &wa );
    
    if ( !drawable )
    {
        EQERROR << "Could not create window\n" << endl;
        return false;
    }

    // map and wait for MapNotify event
    XMapWindow( display, drawable );
    XEvent event;

    XIfEvent( display, &event, WaitForNotify, (XPointer)(drawable) );
    XFlush( display );

    // create context
    Pipe*      pipe        = getPipe();
    Window*    firstWindow = pipe->getWindow(0);
    GLXContext shareCtx    = firstWindow->getGLXContext();
    GLXContext context = glXCreateContext( display, visInfo, shareCtx, True );

    if ( !context )
    {
        EQERROR << "Could not create OpenGL context\n" << endl;
        return false;
    }

    glXMakeCurrent( display, drawable, context );

    setXDrawable( drawable );
    setGLXContext( context );
    EQINFO << "Created X11 drawable " << drawable << ", glX context "
           << context << endl;
    return true;
#else
    return false;
#endif
}

bool eq::Window::initCGL()
{
#ifdef CGL
    CGDirectDisplayID displayID = _pipe->getCGLDisplayID();

    CGLPixelFormatAttribute attribs[] = { 
        kCGLPFADisplayMask,
        (CGLPixelFormatAttribute)CGDisplayIDToOpenGLDisplayMask( displayID ),
        kCGLPFAFullScreen, 
        kCGLPFADoubleBuffer, 
        kCGLPFADepthSize, (CGLPixelFormatAttribute)16, 
        (CGLPixelFormatAttribute)0 };

    CGLPixelFormatObj pixelFormat = NULL;
    long numPixelFormats = 0;
    CGLChoosePixelFormat( attribs, &pixelFormat, &numPixelFormats );

    if( !pixelFormat )
    {
        EQERROR << "Could not find a matching pixel format\n" << endl;
        return false;
    }

    Pipe*         pipe        = getPipe();
    Window*       firstWindow = pipe->getWindow(0);
    CGLContextObj shareCtx    = firstWindow->getCGLContext();
    CGLContextObj context     = 0;
    CGLCreateContext( pixelFormat, shareCtx, &context );
    CGLDestroyPixelFormat ( pixelFormat );

    if( !context ) 
    {
        EQERROR << "Could not create OpenGL context\n" << endl;
        return false;
    }

    // CGRect displayRect = CGDisplayBounds( displayID );
    // glViewport( 0, 0, displayRect.size.width, displayRect.size.height );
    // glViewport( _pvp.x, _pvp.y, _pvp.w, _pvp.h );

    CGLSetCurrentContext( context );
    CGLSetFullScreen( context );

    setCGLContext( context );
    EQINFO << "Created CGL context " << context << endl;
    return true;
#else
    return false;
#endif
}

//----------------------------------------------------------------------
// exit
//----------------------------------------------------------------------
bool eq::Window::exit()
{
    const WindowSystem windowSystem = _pipe->getWindowSystem();
    switch( windowSystem )
    {
        case WINDOW_SYSTEM_GLX:
            exitGLX();
            return true;

        case WINDOW_SYSTEM_CGL:
            exitCGL();
            return false;

        default:
            EQWARN << "Unknown windowing system: " << windowSystem << endl;
            return false;
    }
}

void eq::Window::exitGLX()
{
#ifdef GLX
    Display *display = _pipe->getXDisplay();
    if( !display ) 
        return;

    GLXContext context = getGLXContext();
    if( context )
        glXDestroyContext( display, context );
    setGLXContext( NULL );

    XID drawable = getXDrawable();
    if( drawable )
        XDestroyWindow( display, drawable );
    setXDrawable( 0 );
    EQINFO << "Destroyed GLX context " << context << " and X drawable "
           << drawable << endl;
#endif
}

void eq::Window::exitCGL()
{
#ifdef CGL
    CGLContextObj context = getCGLContext();
    if( !context )
        return;

    setCGLContext( NULL );

    CGLSetCurrentContext( NULL );
    CGLClearDrawable( context );
    CGLDestroyContext ( context );       
    EQINFO << "Destroyed CGL context " << context << endl;
#endif
}


#ifdef GLX
void eq::Window::setXDrawable( XID drawable )
{
    _xDrawable = drawable;

    if( !drawable )
    {
        _pvp.reset();
        return;
    }

    // query pixel viewport of window
    Display          *display = _pipe->getXDisplay();
    EQASSERT( display );

    XWindowAttributes wa;
    XGetWindowAttributes( display, drawable, &wa );
    
    // Window position is relative to parent: translate to absolute coordinates
    ::Window root, parent, *children;
    unsigned nChildren;
    
    XQueryTree( display, drawable, &root, &parent, &children, &nChildren );
    if( children != NULL ) XFree( children );

    int x,y;
    ::Window childReturn;
    XTranslateCoordinates( display, parent, root, wa.x, wa.y, &x, &y,
        &childReturn );

    _pvp.x = x;
    _pvp.y = y;
    _pvp.w = wa.width;
    _pvp.h = wa.height;
}
#endif // GLX

#ifdef CGL
void eq::Window::setCGLContext( CGLContextObj context )
{
    _cglContext = context;
    // TODO: pvp
}
#endif // CGL

void eq::Window::makeCurrent()
{
    switch( _pipe->getWindowSystem( ))
    {
        case WINDOW_SYSTEM_GLX:
#ifdef GLX
            glXMakeCurrent( _pipe->getXDisplay(), _xDrawable, _glXContext );
#endif
            break;

        case WINDOW_SYSTEM_CGL:
            EQUNIMPLEMENTED;
            break;

        default: EQUNIMPLEMENTED;
    }
}

void eq::Window::swapBuffers()
{
    switch( _pipe->getWindowSystem( ))
    {
        case WINDOW_SYSTEM_GLX:
#ifdef GLX
            glXSwapBuffers( _pipe->getXDisplay(), _xDrawable );
#endif
            break;
        case WINDOW_SYSTEM_CGL:
#ifdef CGL
            CGLFlushDrawable( _cglContext );
#endif
            break;
        default: EQUNIMPLEMENTED;
    }
    EQINFO << "----- SWAP -----" << endl;
}

//======================================================================
// event-thread methods
//======================================================================

void eq::Window::processEvent( const WindowEvent& event )
{
    ConfigEvent configEvent;
    switch( event.type )
    {
        case WindowEvent::TYPE_EXPOSE:
            return;

        case WindowEvent::TYPE_RESIZE:
            setPixelViewport( PixelViewport( event.resize.x, event.resize.y, 
                                             event.resize.w, event.resize.h ));
            return;

        case WindowEvent::TYPE_POINTER_MOTION:
            configEvent.type          = ConfigEvent::TYPE_POINTER_MOTION;
            configEvent.pointerMotion = event.pointerMotion;
            break;
            
        case WindowEvent::TYPE_POINTER_BUTTON_PRESS:
            configEvent.type = ConfigEvent::TYPE_POINTER_BUTTON_PRESS;
            configEvent.pointerButtonPress = event.pointerButtonPress;
            break;

        case WindowEvent::TYPE_POINTER_BUTTON_RELEASE:
            configEvent.type = ConfigEvent::TYPE_POINTER_BUTTON_RELEASE;
            configEvent.pointerButtonRelease = event.pointerButtonRelease;
            break;

        case WindowEvent::TYPE_KEY_PRESS:
            if( event.keyPress.key == KC_VOID )
                return;
            configEvent.type         = ConfigEvent::TYPE_KEY_PRESS;
            configEvent.keyPress.key = event.keyPress.key;
            break;
                
        case WindowEvent::TYPE_KEY_RELEASE:
            if( event.keyPress.key == KC_VOID )
                return;
            configEvent.type           = ConfigEvent::TYPE_KEY_RELEASE;
            configEvent.keyRelease.key = event.keyRelease.key;
            break;

        default:
            EQWARN << "Unhandled window event of type " << event.type << endl;
            EQUNIMPLEMENTED;
    }
    
    Config* config = getConfig();
    config->sendEvent( configEvent );
}
