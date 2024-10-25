#include "postgres.h"

#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include "catalog/pg_type.h"
#include "utils/int8.h"
#include "utils/builtins.h"

#include "funcapi.h"
#include "miscadmin.h"
#include "postmaster/syslogger.h"
#include "storage/fd.h"
#include "utils/datetime.h"


PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(try_convert);

Datum
try_convert(PG_FUNCTION_ARGS)
{
	Oid	argtype = get_fn_expr_argtype(fcinfo->flinfo, 0);
	Datum value = PG_GETARG_DATUM(0);
	int64 int_value = 0;

	Datum casttype = PG_GETARG_DATUM(1);

	switch (argtype)
	{
	case INT2OID:
		switch (casttype)
		{
		case INT2OID:
			return value;
		case INT4OID:
			return i2toi4(fcinfo);
		case INT8OID:
			PG_RETURN_NULL();
		case FLOAT4OID:
			return i2tof(fcinfo);
		case FLOAT8OID:
			return i2tod(fcinfo);
		
		default:
			PG_RETURN_NULL();
		}
		break;
	case INT4OID:
		switch (casttype)
		{
		case INT2OID:
			return i4toi2(fcinfo);
		case INT4OID:
			return value;
		case INT8OID:
			PG_RETURN_NULL();
		case FLOAT4OID:
			return i4tof(fcinfo);
		case FLOAT8OID:
			return i4tod(fcinfo);
		
		default:
			PG_RETURN_NULL();
		}
		break;
	case INT8OID:
		switch (casttype)
		{
		case INT2OID:
			PG_RETURN_NULL();
		case INT4OID:
			PG_RETURN_NULL();
		case INT8OID:
			return value;
		case FLOAT4OID:
			return i8tof(fcinfo);
		case FLOAT8OID:
			return i8tod(fcinfo);
		
		default:
			PG_RETURN_NULL();
		}
		break;
	
	default:
		PG_RETURN_NULL();
	}

	// PG_RETURN_INT32(argtype);
}