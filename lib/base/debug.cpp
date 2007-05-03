
/* Copyright (c) 2007, Stefan Eilemann <eile@equalizergraphics.com> 
   All rights reserved. */

#include "debug.h"

EQ_EXPORT void eqBase::abortDebug()
{
    // if EQ_ABORT_WAIT is set, spin forever to allow identifying and debugging
    // of crashed nodes.
    if( getenv( "EQ_ABORT_WAIT" ))
        while( true );

    ::abort();
}
