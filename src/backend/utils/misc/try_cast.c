#include "postgres.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>

#include "funcapi.h"
#include "libpq/pqformat.h"
#include "utils/try_cast.h"
#include "utils/builtins.h"


Datum
try_cast(PG_FUNCTION_ARGS)
{
    Datum value = PG_GETARG_DATUM(0);

    Datum funcId = PG_GETARG_DATUM(1);

	PG_TRY();
	{
        Datum res = OidFunctionCall1(funcId, value);

        PG_RETURN_DATUM(res);
    }
	PG_CATCH();
	{
		PG_RETURN_NULL();
	}
	PG_END_TRY();
}