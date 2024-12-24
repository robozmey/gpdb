
#include "postgres.h"

#include "access/htup_details.h"
#include "funcapi.h"
#include "utils/builtins.h"
#include "miscadmin.h"

#include "fmgr.h"
#include "access/heapam.h"
#include "utils/snapmgr.h"
#include "catalog/pg_namespace.h"
#include "utils/syscache.h"

#include "cdb/cdbaocsam.h"
#include "cdb/cdbappendonlyam.h"

PG_FUNCTION_INFO_V1(get_ao_headers_info);

typedef struct AOHeadersInfoCxt {
    AppendOnlyScanDesc scan;
    TupleTableSlot *slot;
} AOHeadersInfoCxt;


#define NUM_GET_AO_HEADERS_INFO 5

Datum get_ao_headers_info(PG_FUNCTION_ARGS)
{
    Relation r;
    AppendOnlyScanDesc scan;
    TupleTableSlot *slot;
    FuncCallContext *funcctx;
    MemoryContext oldcontext;
    HeapTuple tuple;
    Datum result;
    /* segment if loaded */
    Datum values[NUM_GET_AO_HEADERS_INFO];
    bool nulls[NUM_GET_AO_HEADERS_INFO];

    // int colno = PG_GETARG_INT32(1);	AOCSHeaderScanDesc hdesc;
    Oid	   oid = PG_GETARG_OID(0);


	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("must be superuser to use raw page functions"))));
 
    r = relation_open(oid, AccessShareLock);

    if (!RelationIsAoRows(r))
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 (errmsg("must be an Append-Optimized relation to use raw headerfunctions"))));

    /* stuff done only on the first call of the function */
    if (SRF_IS_FIRSTCALL()) {
        /* create a function context for cross-call persistence */
        funcctx = SRF_FIRSTCALL_INIT();

        /*
        * switch to memory context appropriate for multiple function calls
        */
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);


        scan = appendonly_beginscan(r, GetActiveSnapshot(),
                                                    GetActiveSnapshot(), 0, NULL);

        slot = MakeSingleTupleTableSlot(RelationGetDescr(r));

        funcctx->tuple_desc =
            CreateTemplateTupleDesc(NUM_GET_AO_HEADERS_INFO, false);


        TupleDescInitEntry(funcctx->tuple_desc, (AttrNumber)1, "first row number", INT8OID,
                        -1 /* typmod */, 0 /* attdim */);
        TupleDescInitEntry(funcctx->tuple_desc, (AttrNumber)2, "current item count", INT4OID,
                        -1 /* typmod */, 0 /* attdim */);
        TupleDescInitEntry(funcctx->tuple_desc, (AttrNumber)3, "isCompressed",
                        BOOLOID, -1 /* typmod */, 0 /* attdim */);
        TupleDescInitEntry(funcctx->tuple_desc, (AttrNumber)4, "isLarge",
                        BOOLOID, -1 /* typmod */, 0 /* attdim */);
        TupleDescInitEntry(funcctx->tuple_desc, (AttrNumber)5,
                        "dataLen", INT4OID, -1 /* typmod */,
                        0 /* attdim */);

        funcctx->tuple_desc = BlessTupleDesc(funcctx->tuple_desc);

        funcctx->user_fctx = palloc(sizeof(AOHeadersInfoCxt));

        ((AOHeadersInfoCxt*)funcctx->user_fctx)->scan = scan;
        ((AOHeadersInfoCxt*)funcctx->user_fctx)->slot = slot;

        MemoryContextSwitchTo(oldcontext);
    }

    /* stuff done on every call of the function */
    funcctx = SRF_PERCALL_SETUP();

    scan = ((AOHeadersInfoCxt*)funcctx->user_fctx)->scan;
    slot = ((AOHeadersInfoCxt*)funcctx->user_fctx)->slot;

    for (;;)
    {       
        if (scan->aos_need_new_segfile)
        {
            /*
            * Need to open a new segment file.
            */
            if (!SetNextFileSegForRead(scan))
            {
                break;
            }
        }
        
        if (!AppendOnlyExecutorReadBlock_GetBlockInfo(
                                                    &scan->storageRead,
                                                    &scan->executorReadBlock))
        {
            /* done reading the file */
            AppendOnlyStorageRead_CloseFile(&scan->storageRead);
            break;
        }

        elog(DEBUG5, "block stat: first row num: %ld, current item count: %d, isCompressed: %d, isLarge: %d, dataLen: %d", 
            scan->executorReadBlock.blockFirstRowNum,
            scan->executorReadBlock.currentItemCount,
            scan->executorReadBlock.isCompressed, scan->executorReadBlock.isLarge,
            scan->executorReadBlock.dataLen);

        AppendOnlyExecutorReadBlock_GetContents(
                                                &scan->executorReadBlock);
        MemSet(nulls, 0, sizeof(nulls));

        values[0] = Int64GetDatum(scan->executorReadBlock.blockFirstRowNum);
        values[1] = Int32GetDatum(scan->executorReadBlock.currentItemCount);
        values[2] = BoolGetDatum(scan->executorReadBlock.isCompressed);
        values[3] = BoolGetDatum(scan->executorReadBlock.isLarge);
        values[4] = Int32GetDatum(scan->executorReadBlock.dataLen);

        tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
        result = HeapTupleGetDatum(tuple);

        for (;;)
        {
            bool		found;

            found = AppendOnlyExecutorReadBlock_ScanNextTuple(&scan->executorReadBlock,
                                                            0,
                                                            NULL,
                                                            slot);

            if (found)
                continue;
            
            scan->bufferDone = true;
            break;
        }

        relation_close(r, AccessShareLock);
        SRF_RETURN_NEXT(funcctx, result);
    }


    ExecDropSingleTupleTableSlot(slot);

    appendonly_endscan(scan);

    relation_close(r, AccessShareLock);
    SRF_RETURN_DONE(funcctx);
}