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
    Oid sourceTypeId = get_fn_expr_argtype(fcinfo->flinfo, 0);
    Datum value_datum = PG_GETARG_DATUM(0);
    int64 int_value = 0;

    Datum targetTypeId = PG_GETARG_DATUM(1);

    HeapTuple   tuple;

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

    if (HeapTupleIsValid(tuple))
    {
		    
		/* SELECT castcontext from pg_cast */
        Form_pg_cast castForm = (Form_pg_cast) GETSTRUCT(tuple);
        CoercionContext castcontext;
        
        funcId = castForm->castfunc;

		// // ReleaseSysCache(tuple);
		// // PG_RETURN_OID(funcId);

		// Datum		result;
		// FunctionCallInfoData convert_fcinfodata;
		// FunctionCallInfo convert_fcinfo = &convert_fcinfodata; /// RENAME ALLL
		// PgStat_FunctionCallUsage fcusage;

		// /// SETUP FCINFO

		// FmgrInfo convert_flinfo;
		// fmgr_info(funcId, &convert_flinfo);

		// InitFunctionCallInfoData(*convert_fcinfo, &convert_flinfo, 1, InvalidOid, NULL, NULL);

		// convert_fcinfo->arg[0] = fcinfo->arg[0];
		// convert_fcinfo->argnull[0] = fcinfo->arg[0];

		// /* Guard against stack overflow due to overly complex expressions */
		// check_stack_depth();

		// pgstat_init_function_usage(convert_fcinfo, &fcusage);

		// convert_fcinfo->isnull = false;
		// result = FunctionCallInvoke(convert_fcinfo);
		// fcinfo->isnull = convert_fcinfo->isnull;
		// fcinfo->resultinfo = convert_fcinfo->resultinfo;

		// pgstat_end_function_usage(&fcusage, true);

        ReleaseSysCache(tuple);

		PG_RETURN_DATUM(OidFunctionCall1(funcId, fcinfo->arg[0]));
    }

    PG_RETURN_NULL();
}