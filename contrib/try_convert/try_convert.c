#include "postgres.h"

#include "catalog/pg_cast.h"
#include "utils/syscache.h"
#include "utils/lsyscache.h"

#include "funcapi.h"


PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(try_convert);


typedef enum ConversionType
{
	CONVERSION_TYPE_FUNC,
	CONVERSION_TYPE_RELABEL,
	CONVERSION_TYPE_VIA_IO,
	CONVERSION_TYPE_ARRAY,	
    CONVERSION_TYPE_NONE	
} ConversionType;


ConversionType
find_conversion_way(Oid targetTypeId, Oid sourceTypeId, Oid *funcid)
{
	ConversionType result = CONVERSION_TYPE_NONE;
	HeapTuple	tuple;

	*funcid = InvalidOid;

	/* Perhaps the types are domains; if so, look at their base types */
	if (OidIsValid(sourceTypeId))
		sourceTypeId = getBaseType(sourceTypeId);
	if (OidIsValid(targetTypeId))
		targetTypeId = getBaseType(targetTypeId);

	/* Domains are always coercible to and from their base type */
	if (sourceTypeId == targetTypeId)
		return CONVERSION_TYPE_RELABEL;

	/* SELECT castcontext from pg_cast */

	/* Look in pg_cast */
	tuple = SearchSysCache2(CASTSOURCETARGET,
							ObjectIdGetDatum(sourceTypeId),
							ObjectIdGetDatum(targetTypeId));

	if (HeapTupleIsValid(tuple))
	{
		Form_pg_cast castForm = (Form_pg_cast) GETSTRUCT(tuple);

        switch (castForm->castmethod)
        {
            case COERCION_METHOD_FUNCTION:
                *funcid = castForm->castfunc;
                result = CONVERSION_TYPE_FUNC;
                break;
            case COERCION_METHOD_INOUT:
                result = CONVERSION_TYPE_VIA_IO;
                break;
            case COERCION_METHOD_BINARY:
                result = CONVERSION_TYPE_RELABEL;
                break;
            default:
                elog(ERROR, "unrecognized castmethod: %d",
                        (int) castForm->castmethod);
                break;
        }
	

		ReleaseSysCache(tuple);
	}
	else
	{
		/*
		 * If there's no pg_cast entry, perhaps we are dealing with a pair of
		 * array types.  If so, and if the element types have a suitable cast,
		 * report that we can coerce with an ArrayCoerceExpr.
		 *
		 * Note that the source type can be a domain over array, but not the
		 * target, because ArrayCoerceExpr won't check domain constraints.
		 *
		 * Hack: disallow coercions to oidvector and int2vector, which
		 * otherwise tend to capture coercions that should go to "real" array
		 * types.  We want those types to be considered "real" arrays for many
		 * purposes, but not this one.  (Also, ArrayCoerceExpr isn't
		 * guaranteed to produce an output that meets the restrictions of
		 * these datatypes, such as being 1-dimensional.)
		 */
		if (targetTypeId != OIDVECTOROID && targetTypeId != INT2VECTOROID)
		{
			Oid			targetElem;
			Oid			sourceElem;

			if ((targetElem = get_element_type(targetTypeId)) != InvalidOid &&
			(sourceElem = get_base_element_type(sourceTypeId)) != InvalidOid)
			{
				ConversionType elempathtype;
				Oid			   elemfuncid;

				elempathtype = find_conversion_way(targetElem,
												   sourceElem,
												  &elemfuncid);
				if (elempathtype != CONVERSION_TYPE_NONE &&
					elempathtype != CONVERSION_TYPE_ARRAY)
				{
					*funcid = elemfuncid;
					if (elempathtype == CONVERSION_TYPE_VIA_IO)
						result = CONVERSION_TYPE_VIA_IO;
					else
						result = CONVERSION_TYPE_ARRAY;
				}
			}
		}

		/*
		 * If we still haven't found a possibility, consider automatic casting
		 * using I/O functions.  We allow assignment casts to string types and
		 * explicit casts from string types to be handled this way. (The
		 * CoerceViaIO mechanism is a lot more general than that, but this is
		 * all we want to allow in the absence of a pg_cast entry.) It would
		 * probably be better to insist on explicit casts in both directions,
		 * but this is a compromise to preserve something of the pre-8.3
		 * behavior that many types had implicit (yipes!) casts to text.
		 */
		if (result == CONVERSION_TYPE_NONE)
		{
			if (TypeCategory(targetTypeId) == TYPCATEGORY_STRING)
				result = CONVERSION_TYPE_VIA_IO;
			else if (TypeCategory(sourceTypeId) == TYPCATEGORY_STRING)
				result = CONVERSION_TYPE_VIA_IO;
		}
	}

	return result;
}


Datum
try_convert_from_function(Datum value, Oid funcId, bool *is_failed)
{
    Datum res = 0;

    PG_TRY();
    {
        res = OidFunctionCall1(funcId, value);
    }
    PG_CATCH();
    {
        *is_failed = true;
        FlushErrorState();  /// TODO replace
    }
    PG_END_TRY();


    return res;
}


Datum
try_convert_via_io(Datum value, Oid sourceTypeId, Oid targetTypeId, bool *is_failed)
{
    FmgrInfo *outfunc;
    FmgrInfo *infunc;

    Oid iofunc = InvalidOid;
	bool outtypisvarlena = false;
    Oid intypioparam = InvalidOid;

    /* Perhaps the types are domains; if so, look at their base types */
	if (OidIsValid(sourceTypeId))
		sourceTypeId = getBaseType(sourceTypeId);
	if (OidIsValid(targetTypeId))
		targetTypeId = getBaseType(targetTypeId);

    /* lookup the input type's output function */
    getTypeOutputInfo(sourceTypeId, &iofunc, &outtypisvarlena);
    fmgr_info(iofunc, &outfunc);

    getTypeInputInfo(targetTypeId, &iofunc, &intypioparam);
    fmgr_info(iofunc, &infunc);

    Datum res = 0;
    char *string;

	PG_TRY();
	{
        // value cannot be null
        string = OutputFunctionCall(&outfunc, value);

        res = InputFunctionCall(&infunc,
                                string,
                                intypioparam,
                                -1);
    }
	PG_CATCH();
	{
        *is_failed = true;

        FlushErrorState(); /// TODO replace
	}
	PG_END_TRY();

    return res;
}


Datum
try_convert(PG_FUNCTION_ARGS)
{
    if (fcinfo->argnull[0]) {
        PG_RETURN_NULL();
    }

    Oid sourceTypeId = get_fn_expr_argtype(fcinfo->flinfo, 0);
    Oid targetTypeId = get_fn_expr_argtype(fcinfo->flinfo, 1);

    Oid funcid;

    ConversionType conversion_type = find_conversion_way(targetTypeId, sourceTypeId, &funcid);

    Datum value = fcinfo->arg[0];
    Datum res = 0;

    bool is_failed = false;

    switch (conversion_type)
    {
    case CONVERSION_TYPE_RELABEL:
        return value;

    case CONVERSION_TYPE_FUNC:
        res = try_convert_from_function(value, funcid, &is_failed);
        break;

    case CONVERSION_TYPE_VIA_IO:
        res = try_convert_via_io(value, sourceTypeId, targetTypeId, &is_failed);
        break;

    case CONVERSION_TYPE_ARRAY:
        elog(ERROR, "no sopport for ARRAY CONVERSION");
        is_failed = true;
        break;

    case CONVERSION_TYPE_NONE:
        fcinfo->isnull = fcinfo->argnull[1];
        res = fcinfo->arg[1];
        
        break;
    
    default:
        /// TODO RAISE ERROR
        elog(ERROR, "unrecognized conversion method: %d",
						 (int) conversion_type);
        break;
    }

    if (is_failed) {
        fcinfo->isnull = fcinfo->argnull[1];
        res = fcinfo->arg[1];
    }
    
    return res;
}