#ifndef __CORELET_H__
#define __CORELET_H__

#include <ServiceAPI/bpcfunctions.h>

#include <sstream>

extern const BPCFunctionTable * g_bpCoreFunctions;

#define BPCLOG_LEVEL( level, message ) \
    g_bpCoreFunctions->log( level, (message) );

#define BPCLOG_FATAL( message ) BPCLOG_LEVEL( BP_FATAL, message )
#define BPCLOG_ERROR( message ) BPCLOG_LEVEL( BP_ERROR, message )
#define BPCLOG_WARN( message ) BPCLOG_LEVEL( BP_WARN, message )
#define BPCLOG_INFO( message ) BPCLOG_LEVEL( BP_INFO, message )
#define BPCLOG_DEBUG( message ) BPCLOG_LEVEL( BP_DEBUG, message )

#define BPCLOG_LEVEL_STRM( level, x ) \
{ \
    std::stringstream ss; \
    ss << x; \
    BPCLOG_LEVEL( level, ss.str().c_str() ); \
}

#define BPCLOG_FATAL_STRM( x ) BPCLOG_LEVEL_STRM( BP_FATAL, x )
#define BPCLOG_ERROR_STRM( x ) BPCLOG_LEVEL_STRM( BP_ERROR, x )
#define BPCLOG_WARN_STRM( x ) BPCLOG_LEVEL_STRM( BP_WARN, x )
#define BPCLOG_INFO_STRM( x ) BPCLOG_LEVEL_STRM( BP_INFO, x )
#define BPCLOG_DEBUG_STRM( x ) BPCLOG_LEVEL_STRM( BP_DEBUG, x )

#endif
