/*-------------------------------------------------------------------------
 *
 * miscnodes.h
 *	  Definitions for hard-to-classify node types.
 *
 * src/include/nodes/miscnodes.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef MISCNODES_H
#define MISCNODES_H
#include "nodes/nodes.h"

typedef struct ErrorSaveContext
{
	NodeTag		type;
	bool		error_occurred; /* set to true if we detect a soft error */
} ErrorSaveContext;

/* Often-useful macro for checking if a soft error was reported */
#define SOFT_ERROR_OCCURRED(escontext) \
	((escontext) != NULL && IsA(escontext, ErrorSaveContext) && \
	 ((ErrorSaveContext *) (escontext))->error_occurred)

#endif	