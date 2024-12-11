#include "postgres.h"

#include "catalog/pg_cast.h"
#include "utils/syscache.h"
#include "utils/lsyscache.h"
#include "nodes/nodeFuncs.h"
#include "nodes/miscnodes.h"

#include "funcapi.h"


PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(try_convert);

// DEBUG
PG_FUNCTION_INFO_V1(get_targettypeid);
PG_FUNCTION_INFO_V1(get_targetbasetypeid);
PG_FUNCTION_INFO_V1(get_targettypmod);


typedef enum ConversionType
{
	CONVERSION_TYPE_FUNC,
	CONVERSION_TYPE_RELABEL,
	CONVERSION_TYPE_VIA_IO,
	CONVERSION_TYPE_ARRAY,	
    CONVERSION_TYPE_NONE	
} ConversionType;


ConversionType
find_conversion_way(Oid targetTypeId, Oid sourceTypeId, Oid *funcId)
{
	ConversionType result = CONVERSION_TYPE_NONE;
	HeapTuple	tuple;

	*funcId = InvalidOid;

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
                *funcId = castForm->castfunc;
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
					*funcId = elemfuncid;
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

ConversionType
find_typmod_conversion_function(Oid typeId, Oid *funcId)
{
	ConversionType result;
	// HeapTuple	targetType;
	Form_pg_type typeForm;
	HeapTuple	tuple;

	*funcId = InvalidOid;
	result = CONVERSION_TYPE_FUNC;

	// targetType = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typeId));
	// typeForm = (Form_pg_type) GETSTRUCT(targetType);

	// /* Check for a varlena array type */
	// if (typeForm->typelem != InvalidOid && typeForm->typlen == -1)
	// {
	// 	/* Yes, switch our attention to the element type */
	// 	typeId = typeForm->typelem;
	// 	result = CONVERSION_TYPE_ARRAY;
	// }
	// ReleaseSysCache(targetType);

	/* Look in pg_cast */
	tuple = SearchSysCache2(CASTSOURCETARGET,
							ObjectIdGetDatum(typeId),
							ObjectIdGetDatum(typeId));

	if (HeapTupleIsValid(tuple))
	{
		Form_pg_cast castForm = (Form_pg_cast) GETSTRUCT(tuple);

		*funcId = castForm->castfunc;
		ReleaseSysCache(tuple);
	}

	if (!OidIsValid(*funcId))
		result = CONVERSION_TYPE_NONE;

	return result;
}

Datum
try_convert_from_function(Datum value, int32 typmod, Oid funcId, bool *is_failed)
{
    Datum res = 0;

	ErrorSaveContext escontext = {T_ErrorSaveContext, false};;

	res = OidFunctionCall3Safe(funcId, value, typmod, true, &escontext);

	if (escontext.error_occurred) {
		*is_failed = true;
	}


    return res;
}


Datum
try_convert_via_io(Datum value, Oid sourceTypeId, Oid targetTypeId, int32 targetTypMod, bool *is_failed)
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

	ErrorSaveContext escontext = {T_ErrorSaveContext, false};

	PG_TRY();
	{
        // value cannot be null
        string = OutputFunctionCall(&outfunc, value);

        res = InputFunctionCallSafe(&infunc,
                                string,
                                intypioparam,
                                -1,
								&escontext);

		if (escontext.error_occurred) {
			*is_failed = true;
		}
    }
	PG_CATCH();
    {
		res = 0;
    }
	PG_END_TRY();

    return res;
}


/*
 * Get the actual typmod int32 of a specific function argument (counting from 0),
 * but working from the calling expression tree instead of FmgrInfo
 *
 * Returns -1 if information is not available
 */
int32
get_call_expr_argtypmod(Node *expr, int argnum)
{
	List	   *args;
	int32		argtypmod;

	if (expr == NULL)
		return -1;

	if (IsA(expr, FuncExpr))
		args = ((FuncExpr *) expr)->args;
	else if (IsA(expr, OpExpr))
		args = ((OpExpr *) expr)->args;
	else if (IsA(expr, DistinctExpr))
		args = ((DistinctExpr *) expr)->args;
	else if (IsA(expr, ScalarArrayOpExpr))
		args = ((ScalarArrayOpExpr *) expr)->args;
	else if (IsA(expr, ArrayCoerceExpr))
		args = list_make1(((ArrayCoerceExpr *) expr)->arg);
	else if (IsA(expr, NullIfExpr))
		args = ((NullIfExpr *) expr)->args;
	else if (IsA(expr, WindowFunc))
		args = ((WindowFunc *) expr)->args;
	else
		return -1;

	if (argnum < 0 || argnum >= list_length(args))
		return -1;

	argtypmod = exprTypmod((Node *) list_nth(args, argnum));

	// /*
	//  * special hack for ScalarArrayOpExpr and ArrayCoerceExpr: what the
	//  * underlying function will actually get passed is the element type of the
	//  * array.
	//  */
	// if (IsA(expr, ScalarArrayOpExpr) &&
	// 	argnum == 1)
	// 	argtypmod = get_base_element_type(argtype);
	// else if (IsA(expr, ArrayCoerceExpr) &&
	// 		 argnum == 0)
	// 	argtypmod = get_base_element_type(argtype);

	return argtypmod;
}

/*
 * Get the actual typemod of a specific function argument (counting from 0)
 *
 * Returns -1 if information is not available
 */
Oid
get_fn_expr_argtypmod(FmgrInfo *flinfo, int argnum)
{
	/*
	 * can't return anything useful if we have no FmgrInfo or if its fn_expr
	 * node has not been initialized
	 */
	if (!flinfo || !flinfo->fn_expr)
		return -1;

	return get_call_expr_argtypmod(flinfo->fn_expr, argnum);
}

Datum
get_targettypeid(PG_FUNCTION_ARGS)
{
	PG_RETURN_INT32(get_fn_expr_argtype(fcinfo->flinfo, 0));
}

Datum
get_targetbasetypeid(PG_FUNCTION_ARGS)
{
	PG_RETURN_INT32(getBaseType(get_fn_expr_argtype(fcinfo->flinfo, 0)));
}

Datum
get_targettypmod(PG_FUNCTION_ARGS)
{
	PG_RETURN_INT32(get_fn_expr_argtypmod(fcinfo->flinfo, 0));
}


Datum
convert(Datum value, ConversionType conversion_type, Oid funcId, Oid sourceTypeId, Oid targetTypeId, int32 targetTypMod, bool *is_failed) {
	
	switch (conversion_type)
    {
    case CONVERSION_TYPE_RELABEL:
        return value;

    case CONVERSION_TYPE_FUNC:
        return try_convert_from_function(value, targetTypMod, funcId, is_failed);

    case CONVERSION_TYPE_VIA_IO:
        return try_convert_via_io(value, sourceTypeId, targetTypeId, targetTypMod, is_failed);

    case CONVERSION_TYPE_ARRAY:
        elog(ERROR, "no sopport for ARRAY CONVERSION");
        is_failed = true;
        break;

    case CONVERSION_TYPE_NONE:
        is_failed = true;
        return 0;
    
    default:
        /// TODO RAISE ERROR
        elog(ERROR, "unrecognized conversion method: %d",
						 (int) conversion_type);
        break;
    }

	return 0;
}

Datum convert_type_typmod(Datum value, int32 sourceTypMod, Oid targetTypeId, int32 targetTypMod, bool *is_failed) {
	if (targetTypMod < 0 || targetTypMod == sourceTypMod)
		return value;

	Oid funcId = InvalidOid;
	ConversionType conversion_type = find_typmod_conversion_function(targetTypeId, &funcId);
	
	return convert(value, conversion_type, funcId, InvalidOid, targetTypeId, targetTypMod, is_failed);
}


Datum
try_convert(PG_FUNCTION_ARGS)
{
    if (fcinfo->argnull[0]) {
        PG_RETURN_NULL();
    }

    Oid sourceTypeId = get_fn_expr_argtype(fcinfo->flinfo, 0);
	int32 sourceTypMod = get_fn_expr_argtypmod(fcinfo->flinfo, 0);

    Oid targetTypeId = get_fn_expr_argtype(fcinfo->flinfo, 1);
	int32 targetTypMod = get_fn_expr_argtypmod(fcinfo->flinfo, 1);

	int32 baseTypMod = targetTypMod;
	Oid baseTypeId = getBaseTypeAndTypmod(targetTypeId, &baseTypMod);

    Oid funcId;

    ConversionType conversion_type = find_conversion_way(targetTypeId, sourceTypeId, &funcId);

    Datum value = fcinfo->arg[0];

    Datum res = value;
	Oid resTypeId = sourceTypeId;
	int32 resTypMod = sourceTypMod;

    bool is_failed = false;

	if (conversion_type != CONVERSION_TYPE_RELABEL) {

		res = convert(value, conversion_type, funcId, sourceTypeId, baseTypeId, baseTypMod, &is_failed);

		if (is_failed) {
			fcinfo->isnull = fcinfo->argnull[1];
			res = fcinfo->arg[1];
		}

		resTypeId = baseTypeId;
		resTypMod = -1;

		// if (targetTypeId != baseTypeId) {
		// COERCE_DOMAIN();
		// }
	} else {

		res = value;

		// if (targetTypeId != baseTypeId) {
		// COERCE_DOMAIN();
		// }
	}

	res = convert_type_typmod(res, -1, targetTypeId, targetTypMod, &is_failed);

	if (is_failed) {
		fcinfo->isnull = fcinfo->argnull[1];
		res = fcinfo->arg[1];
	}
    
    return res;
}