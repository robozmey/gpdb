#include "postgres.h"

#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include "catalog/pg_type.h"
#include "catalog/pg_cast.h"
#include "nodes/makefuncs.h"
#include "utils/syscache.h"
#include "utils/lsyscache.h"
#include "pgstat.h"

#include "funcapi.h"
#include "miscadmin.h"
#include "postmaster/syslogger.h"
#include "storage/fd.h"


PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(try_convert);

Datum
try_convert(PG_FUNCTION_ARGS)
{
    HeapTuple tuple, tuple1, tuple2;

    Oid sourceTypeId = get_fn_expr_argtype(fcinfo->flinfo, 0);
    Datum value_datum = PG_GETARG_DATUM(0);
    int64 int_value = 0;

    Datum targetTypeId = PG_GETARG_DATUM(1);

    Oid funcId = InvalidOid;

    /* Perhaps the types are domains; if so, look at their base types */
    if (OidIsValid(sourceTypeId))
        sourceTypeId = getBaseType(sourceTypeId);
    if (OidIsValid(targetTypeId))
        targetTypeId = getBaseType(targetTypeId);

    /* Domains are always coercible to and from their base type */
    if (sourceTypeId == targetTypeId)
        PG_RETURN_DATUM(value_datum);

    /* Look in pg_cast */
    tuple = SearchSysCache2(CASTSOURCETARGET,
                            ObjectIdGetDatum(sourceTypeId),
                            ObjectIdGetDatum(targetTypeId));

    tuple1 = SearchSysCache1(TYPEOID, ObjectIdGetDatum(sourceTypeId));
    tuple2 = SearchSysCache1(TYPEOID, ObjectIdGetDatum(targetTypeId));

	PG_TRY();
	{

        if (HeapTupleIsValid(tuple))
        {
            /* SELECT castcontext from pg_cast */
            Form_pg_cast castForm = (Form_pg_cast) GETSTRUCT(tuple);
            CoercionContext castcontext;
            
            funcId = castForm->castfunc;

            Datum res = OidFunctionCall1(funcId, fcinfo->arg[0]);

            ReleaseSysCache(tuple);

            PG_RETURN_DATUM(res);
        } else {

            // ReleaseSysCache(tuple);

            // Form_pg_type typeForm1 = (Form_pg_type) GETSTRUCT(tuple1);
            // Form_pg_type typeForm2 = (Form_pg_type) GETSTRUCT(tuple2);

            // Oid outFunc = typeForm1->typoutput;
            // Oid inFunc  = typeForm1->typinput;

            // Datum txt = OidFunctionCall1(outFunc, fcinfo->arg[0]);
            // Datum res = OidFunctionCall1(inFunc, txt); 

            // ReleaseSysCache(tuple1);
            // ReleaseSysCache(tuple2);

            // PG_RETURN_DATUM(res);

            PG_RETURN_NULL();
        }
    }
	PG_CATCH();
	{
        // ReleaseSysCache(tuple);
        // ReleaseSysCache(tuple1);
        // ReleaseSysCache(tuple2);

		PG_RETURN_NULL();
	}
	PG_END_TRY();
}