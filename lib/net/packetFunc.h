
/* Copyright (c) 2006, Stefan Eilemann <eile@equalizergraphics.com> 
   All rights reserved. */

#ifndef EQNET_PACKETFUNC_H
#define EQNET_PACKETFUNC_H

#include <eq/net/commandResult.h>

namespace eqNet
{
    class Command;

    template< typename T > class PacketFunc
    {
    public:
        PacketFunc( T* object, 
                    CommandResult (T::*func)( Command& ))
            : _object( object ), _func( func ) {}

        template< typename O > PacketFunc( const O& from )
                : _object( from._object ),
                  _func( 
                      static_cast<CommandResult (T::*)( Command& )>(from._func))
            {}

        CommandResult operator()( Command& command )
        {
            return (_object->*_func)( command );
        }

         // Can't make private because copy constructor needs access. Can't
         // declare friend because C++ does not allow generic template friends
         // in template classes.
        // private:
        T* _object;
        CommandResult (T::*_func)( Command& );
    };

     
}

#endif //EQNET_PACKETFUNC_H
