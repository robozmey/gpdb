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
	PG_RETURN_INT32(1);
}