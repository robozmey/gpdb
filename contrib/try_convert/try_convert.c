#include "postgres.h"

#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include "catalog/pg_type.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "postmaster/syslogger.h"
#include "storage/fd.h"
#include "utils/builtins.h"
#include "utils/datetime.h"


PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(try_convert);

Datum
try_convert(PG_FUNCTION_ARGS)
{
	Oid	argtype = get_fn_expr_argtype(fcinfo->flinfo, 0);
	Datum value_datum = PG_GETARG_DATUM(0);
	int64 int_value = 0;

	Datum casttype = PG_GETARG_DATUM(1);

	switch (argtype)
	{
	case INT2OID:
	case INT4OID:
	case INT8OID:
		int_value = (int64) PG_GETARG_INT64(0);

		switch (casttype)
		{
		case INT2OID:
			PG_RETURN_INT16((int16) int_value);
		case INT4OID:
			PG_RETURN_INT32((int32) int_value);
		case INT8OID:
			PG_RETURN_INT32((int64) int_value);
		case FLOAT4OID:
			PG_RETURN_FLOAT4((float4) int_value);
		case FLOAT8OID:
			PG_RETURN_FLOAT8((float8) int_value);
		
		default:
			PG_RETURN_NULL();
		}
		break;
	
	default:
		PG_RETURN_NULL();
	}

	// PG_RETURN_INT32(argtype);
}