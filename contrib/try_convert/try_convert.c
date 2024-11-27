#include "postgres.h"

#include "catalog/pg_cast.h"
#include "utils/syscache.h"
#include "utils/lsyscache.h"

#include "funcapi.h"


PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(try_convert);


Datum
try_convert_from_pg_cast(Datum value, Oid sourceTypeId, Oid targetTypeId, bool *is_null)
{
    HeapTuple tuple;

    /* Look in pg_cast */
    tuple = SearchSysCache2(CASTSOURCETARGET,
                            ObjectIdGetDatum(sourceTypeId),
                            ObjectIdGetDatum(targetTypeId));

    Datum res = 0;

    if (HeapTupleIsValid(tuple))
    {
        /* SELECT castcontext from pg_cast */
        Form_pg_cast castForm = (Form_pg_cast) GETSTRUCT(tuple);
        CoercionContext castcontext;

        Oid funcId = castForm->castfunc;

        PG_TRY();
        {
            res = OidFunctionCall1(funcId, value);
        }
        PG_CATCH();
        {
            *is_null = true;
            FlushErrorState();  /// TODO replace
        }
        PG_END_TRY();

        ReleaseSysCache(tuple);
    } else {
        *is_null = true;
    }

    return res;
}


Datum
try_convert_via_io(Datum value, Oid sourceTypeId, Oid targetTypeId, bool *is_null)
{
    FmgrInfo *outfunc;
    FmgrInfo *infunc;

    Oid iofunc = InvalidOid;
	bool outtypisvarlena = false;
    Oid intypioparam = InvalidOid;

    /* lookup the input type's output function */
    getTypeOutputInfo(sourceTypeId, &iofunc, &outtypisvarlena);
    fmgr_info(iofunc, &outfunc);

    getTypeInputInfo(targetTypeId, &iofunc, &intypioparam);
    fmgr_info(iofunc, &infunc);

    Datum res = 0;
    char *string;

	PG_TRY();
	{

        if (*is_null)
            string = NULL;			/* output functions are not called on nulls */
        else
            string = OutputFunctionCall(&outfunc, value);

        res = InputFunctionCall(&infunc,
                                string,
                                intypioparam,
                                -1);
    }
	PG_CATCH();
	{
        *is_null = true;

        FlushErrorState(); /// TODO replace
	}
	PG_END_TRY();

    return res;
}


// NO NULL INPUTS!!!!

Datum
try_convert(PG_FUNCTION_ARGS)
{
    Oid sourceTypeId = get_fn_expr_argtype(fcinfo->flinfo, 0);
    Oid targetTypeId = get_fn_expr_argtype(fcinfo->flinfo, 1);
        
    /* Perhaps the types are domains; if so, look at their base types */
	if (OidIsValid(sourceTypeId))
		sourceTypeId = getBaseType(sourceTypeId);
	if (OidIsValid(targetTypeId))
		targetTypeId = getBaseType(targetTypeId);

    Datum value = fcinfo->arg[0];
    Datum res = 0;

    	/* Domains are always coercible to and from their base type */
	if (sourceTypeId == targetTypeId)
		return value;

    bool is_null = fcinfo->isnull;

    res = try_convert_from_pg_cast(value, sourceTypeId, targetTypeId, &is_null);

    if (!is_null)
        return res;

    if (TypeCategory(sourceTypeId) == TYPCATEGORY_STRING 
     || TypeCategory(targetTypeId) == TYPCATEGORY_STRING) {
        is_null = fcinfo->isnull;
        res = try_convert_via_io(value, sourceTypeId, targetTypeId, &is_null);
    }

    fcinfo->isnull = is_null;
    return res;
}