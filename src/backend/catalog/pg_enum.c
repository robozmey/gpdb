/*-------------------------------------------------------------------------
 *
 * pg_enum.c
 *	  routines to support manipulation of the pg_enum relation
 *
 * Copyright (c) 2006-2014, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/catalog/pg_enum.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/binary_upgrade.h"
#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "catalog/pg_enum.h"
#include "catalog/pg_type.h"
#include "cdb/cdbvars.h"
#include "storage/lmgr.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/fmgroids.h"
#include "utils/syscache.h"
#include "utils/tqual.h"


static void RenumberEnumType(Relation pg_enum, HeapTuple *existing, int nelems);
static int	oid_cmp(const void *p1, const void *p2);
static int	sort_order_cmp(const void *p1, const void *p2);


/*
 * EnumValuesCreate
 *		Create an entry in pg_enum for each of the supplied enum values.
 *
 * vals is a list of Value strings.
 */
void
EnumValuesCreate(Oid enumTypeOid, List *vals)
{
	Relation	pg_enum;
	NameData	enumlabel;
	Oid		   *oids;
	int			elemno,
				num_elems;
	Datum		values[Natts_pg_enum];
	bool		nulls[Natts_pg_enum];
	ListCell   *lc;
	HeapTuple	tup;

	num_elems = list_length(vals);

	/*
	 * We do not bother to check the list of values for duplicates --- if you
	 * have any, you'll get a less-than-friendly unique-index violation. It is
	 * probably not worth trying harder.
	 */

	pg_enum = heap_open(EnumRelationId, RowExclusiveLock);

	/*
	 * Allocate OIDs for the enum's members.
	 *
	 * While this method does not absolutely guarantee that we generate no
	 * duplicate OIDs (since we haven't entered each oid into the table before
	 * allocating the next), trouble could only occur if the OID counter wraps
	 * all the way around before we finish. Which seems unlikely.
	 */
	oids = (Oid *) palloc(num_elems * sizeof(Oid));

	for (elemno = 0; elemno < num_elems; elemno++)
	{
		/*
		 * We assign even-numbered OIDs to all the new enum labels.  This
		 * tells the comparison functions the OIDs are in the correct sort
		 * order and can be compared directly.
		 */
		Oid			new_oid;

		/*
		 * In QE node, however, use the OIDs assigned by the master (they are delivered
		 * out-of-band, see oid_dispatch.c.
		 */
		if (Gp_role == GP_ROLE_EXECUTE)
			new_oid = InvalidOid;
		else
		{
			do
			{
				new_oid = GetNewOid(pg_enum, '\0');
			} while (new_oid & 1);
		}
		oids[elemno] = new_oid;
	}

	/* sort them, just in case OID counter wrapped from high to low */
	qsort(oids, num_elems, sizeof(Oid), oid_cmp);

	/* and make the entries */
	memset(nulls, false, sizeof(nulls));

	elemno = 0;
	foreach(lc, vals)
	{
		char	   *lab = strVal(lfirst(lc));

		/*
		 * labels are stored in a name field, for easier syscache lookup, so
		 * check the length to make sure it's within range.
		 */
		if (strlen(lab) > (NAMEDATALEN - 1))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_NAME),
					 errmsg("invalid enum label \"%s\"", lab),
					 errdetail("Labels must be %d characters or less.",
							   NAMEDATALEN - 1)));

		values[Anum_pg_enum_enumtypid - 1] = ObjectIdGetDatum(enumTypeOid);
		values[Anum_pg_enum_enumsortorder - 1] = Float4GetDatum(elemno + 1);
		namestrcpy(&enumlabel, lab);
		values[Anum_pg_enum_enumlabel - 1] = NameGetDatum(&enumlabel);

		tup = heap_form_tuple(RelationGetDescr(pg_enum), values, nulls);
		HeapTupleSetOid(tup, oids[elemno]);

		simple_heap_insert(pg_enum, tup);
		CatalogUpdateIndexes(pg_enum, tup);
		heap_freetuple(tup);

		elemno++;
	}

	/* clean up */
	pfree(oids);
	heap_close(pg_enum, RowExclusiveLock);
}


/*
 * EnumValuesDelete
 *		Remove all the pg_enum entries for the specified enum type.
 */
void
EnumValuesDelete(Oid enumTypeOid)
{
	Relation	pg_enum;
	ScanKeyData key[1];
	SysScanDesc scan;
	HeapTuple	tup;

	pg_enum = heap_open(EnumRelationId, RowExclusiveLock);

	ScanKeyInit(&key[0],
				Anum_pg_enum_enumtypid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(enumTypeOid));

	scan = systable_beginscan(pg_enum, EnumTypIdLabelIndexId, true,
							  NULL, 1, key);

	while (HeapTupleIsValid(tup = systable_getnext(scan)))
	{
		simple_heap_delete(pg_enum, &tup->t_self);
	}

	systable_endscan(scan);

	heap_close(pg_enum, RowExclusiveLock);
}


/*
 * AddEnumLabel
 *		Add a new label to the enum set. By default it goes at
 *		the end, but the user can choose to place it before or
 *		after any existing set member.
 */
void
AddEnumLabel(Oid enumTypeOid,
			 const char *newVal,
			 const char *neighbor,
			 bool newValIsAfter,
			 bool skipIfExists)
{
	Relation	pg_enum;
	Oid			newOid;
	Datum		values[Natts_pg_enum];
	bool		nulls[Natts_pg_enum];
	NameData	enumlabel;
	HeapTuple	enum_tup;
	float4		newelemorder;
	HeapTuple  *existing;
	CatCList   *list;
	int			nelems;
	int			i;

	/* check length of new label is ok */
	if (strlen(newVal) > (NAMEDATALEN - 1))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("invalid enum label \"%s\"", newVal),
				 errdetail("Labels must be %d characters or less.",
						   NAMEDATALEN - 1)));

	/*
	 * Acquire a lock on the enum type, which we won't release until commit.
	 * This ensures that two backends aren't concurrently modifying the same
	 * enum type.  Without that, we couldn't be sure to get a consistent view
	 * of the enum members via the syscache.  Note that this does not block
	 * other backends from inspecting the type; see comments for
	 * RenumberEnumType.
	 */
	LockDatabaseObject(TypeRelationId, enumTypeOid, 0, ExclusiveLock);

	/*
	 * Check if label is already in use.  The unique index on pg_enum would
	 * catch this anyway, but we prefer a friendlier error message, and
	 * besides we need a check to support IF NOT EXISTS.
	 */
	enum_tup = SearchSysCache2(ENUMTYPOIDNAME,
							   ObjectIdGetDatum(enumTypeOid),
							   CStringGetDatum(newVal));
	if (HeapTupleIsValid(enum_tup))
	{
		ReleaseSysCache(enum_tup);
		if (skipIfExists)
		{
			ereport(NOTICE,
					(errcode(ERRCODE_DUPLICATE_OBJECT),
					 errmsg("enum label \"%s\" already exists, skipping",
							newVal)));
			return;
		}
		else
			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_OBJECT),
					 errmsg("enum label \"%s\" already exists",
							newVal)));
	}

	pg_enum = heap_open(EnumRelationId, RowExclusiveLock);

	/* If we have to renumber the existing members, we restart from here */
restart:

	/* Get the list of existing members of the enum */
	list = SearchSysCacheList1(ENUMTYPOIDNAME,
							   ObjectIdGetDatum(enumTypeOid));
	nelems = list->n_members;

	/* Sort the existing members by enumsortorder */
	existing = (HeapTuple *) palloc(nelems * sizeof(HeapTuple));
	for (i = 0; i < nelems; i++)
		existing[i] = &(list->members[i]->tuple);

	qsort(existing, nelems, sizeof(HeapTuple), sort_order_cmp);

	if (neighbor == NULL)
	{
		/*
		 * Put the new label at the end of the list. No change to existing
		 * tuples is required.
		 */
		if (nelems > 0)
		{
			Form_pg_enum en = (Form_pg_enum) GETSTRUCT(existing[nelems - 1]);

			newelemorder = en->enumsortorder + 1;
		}
		else
			newelemorder = 1;
	}
	else
	{
		/* BEFORE or AFTER was specified */
		int			nbr_index;
		int			other_nbr_index;
		Form_pg_enum nbr_en;
		Form_pg_enum other_nbr_en;

		/* Locate the neighbor element */
		for (nbr_index = 0; nbr_index < nelems; nbr_index++)
		{
			Form_pg_enum en = (Form_pg_enum) GETSTRUCT(existing[nbr_index]);

			if (strcmp(NameStr(en->enumlabel), neighbor) == 0)
				break;
		}
		if (nbr_index >= nelems)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("\"%s\" is not an existing enum label",
							neighbor)));
		nbr_en = (Form_pg_enum) GETSTRUCT(existing[nbr_index]);

		/*
		 * Attempt to assign an appropriate enumsortorder value: one less than
		 * the smallest member, one more than the largest member, or halfway
		 * between two existing members.
		 *
		 * In the "halfway" case, because of the finite precision of float4,
		 * we might compute a value that's actually equal to one or the other
		 * of its neighbors.  In that case we renumber the existing members
		 * and try again.
		 */
		if (newValIsAfter)
			other_nbr_index = nbr_index + 1;
		else
			other_nbr_index = nbr_index - 1;

		if (other_nbr_index < 0)
			newelemorder = nbr_en->enumsortorder - 1;
		else if (other_nbr_index >= nelems)
			newelemorder = nbr_en->enumsortorder + 1;
		else
		{
			other_nbr_en = (Form_pg_enum) GETSTRUCT(existing[other_nbr_index]);
			newelemorder = (nbr_en->enumsortorder +
							other_nbr_en->enumsortorder) / 2;

			/*
			 * On some machines, newelemorder may be in a register that's
			 * wider than float4.  We need to force it to be rounded to float4
			 * precision before making the following comparisons, or we'll get
			 * wrong results.  (Such behavior violates the C standard, but
			 * fixing the compilers is out of our reach.)
			 */
			newelemorder = DatumGetFloat4(Float4GetDatum(newelemorder));

			if (newelemorder == nbr_en->enumsortorder ||
				newelemorder == other_nbr_en->enumsortorder)
			{
				RenumberEnumType(pg_enum, existing, nelems);
				/* Clean up and start over */
				pfree(existing);
				ReleaseCatCacheList(list);
				goto restart;
			}
		}
	}

	if (Gp_role == GP_ROLE_EXECUTE || IsBinaryUpgrade)
	{
		/*
		 * In QE, the dispatcher has alrady allocated the OID for us. Like in
		 * EnumValuesCreate(), it is delivered out-of-band, and set on the tuple by
		 * heap_insert().
		 */
		newOid = InvalidOid;
	}
	else
	{
		/*
		 * Normal case: we need to allocate a new Oid for the value.
		 *
		 * We want to give the new element an even-numbered Oid if it's safe,
		 * which is to say it compares correctly to all pre-existing even
		 * numbered Oids in the enum.  Otherwise, we must give it an odd Oid.
		 */
		for (;;)
		{
			bool		sorts_ok;

			/* Get a new OID (different from all existing pg_enum tuples) */
			newOid = GetNewOid(pg_enum, '\0');

			/*
			 * Detect whether it sorts correctly relative to existing
			 * even-numbered labels of the enum.  We can ignore existing
			 * labels with odd Oids, since a comparison involving one of those
			 * will not take the fast path anyway.
			 */
			sorts_ok = true;
			for (i = 0; i < nelems; i++)
			{
				HeapTuple	exists_tup = existing[i];
				Form_pg_enum exists_en = (Form_pg_enum) GETSTRUCT(exists_tup);
				Oid			exists_oid = HeapTupleGetOid(exists_tup);

				if (exists_oid & 1)
					continue;	/* ignore odd Oids */

				if (exists_en->enumsortorder < newelemorder)
				{
					/* should sort before */
					if (exists_oid >= newOid)
					{
						sorts_ok = false;
						break;
					}
				}
				else
				{
					/* should sort after */
					if (exists_oid <= newOid)
					{
						sorts_ok = false;
						break;
					}
				}
			}

			if (sorts_ok)
			{
				/* If it's even and sorts OK, we're done. */
				if ((newOid & 1) == 0)
					break;

				/*
				 * If it's odd, and sorts OK, loop back to get another OID and
				 * try again.  Probably, the next available even OID will sort
				 * correctly too, so it's worth trying.
				 */
			}
			else
			{
				/*
				 * If it's odd, and does not sort correctly, we're done.
				 * (Probably, the next available even OID would sort
				 * incorrectly too, so no point in trying again.)
				 */
				if (newOid & 1)
					break;

				/*
				 * If it's even, and does not sort correctly, loop back to get
				 * another OID and try again.  (We *must* reject this case.)
				 */
			}
		}
	}

	/* Done with info about existing members */
	pfree(existing);
	ReleaseCatCacheList(list);

	/* Create the new pg_enum entry */
	memset(nulls, false, sizeof(nulls));
	values[Anum_pg_enum_enumtypid - 1] = ObjectIdGetDatum(enumTypeOid);
	values[Anum_pg_enum_enumsortorder - 1] = Float4GetDatum(newelemorder);
	namestrcpy(&enumlabel, newVal);
	values[Anum_pg_enum_enumlabel - 1] = NameGetDatum(&enumlabel);
	enum_tup = heap_form_tuple(RelationGetDescr(pg_enum), values, nulls);
	HeapTupleSetOid(enum_tup, newOid);
	simple_heap_insert(pg_enum, enum_tup);
	CatalogUpdateIndexes(pg_enum, enum_tup);
	heap_freetuple(enum_tup);

	heap_close(pg_enum, RowExclusiveLock);
}


/*
 * RenumberEnumType
 *		Renumber existing enum elements to have sort positions 1..n.
 *
 * We avoid doing this unless absolutely necessary; in most installations
 * it will never happen.  The reason is that updating existing pg_enum
 * entries creates hazards for other backends that are concurrently reading
 * pg_enum.  Although system catalog scans now use MVCC semantics, the
 * syscache machinery might read different pg_enum entries under different
 * snapshots, so some other backend might get confused about the proper
 * ordering if a concurrent renumbering occurs.
 *
 * We therefore make the following choices:
 *
 * 1. Any code that is interested in the enumsortorder values MUST read
 * all the relevant pg_enum entries with a single MVCC snapshot, or else
 * acquire lock on the enum type to prevent concurrent execution of
 * AddEnumLabel().
 *
 * 2. Code that is not examining enumsortorder can use a syscache
 * (for example, enum_in and enum_out do so).
 */
static void
RenumberEnumType(Relation pg_enum, HeapTuple *existing, int nelems)
{
	int			i;

	/*
	 * We should only need to increase existing elements' enumsortorders,
	 * never decrease them.  Therefore, work from the end backwards, to avoid
	 * unwanted uniqueness violations.
	 */
	for (i = nelems - 1; i >= 0; i--)
	{
		HeapTuple	newtup;
		Form_pg_enum en;
		float4		newsortorder;

		newtup = heap_copytuple(existing[i]);
		en = (Form_pg_enum) GETSTRUCT(newtup);

		newsortorder = i + 1;
		if (en->enumsortorder != newsortorder)
		{
			en->enumsortorder = newsortorder;

			simple_heap_update(pg_enum, &newtup->t_self, newtup);

			CatalogUpdateIndexes(pg_enum, newtup);
		}

		heap_freetuple(newtup);
	}

	/* Make the updates visible */
	CommandCounterIncrement();
}


/* qsort comparison function for oids */
static int
oid_cmp(const void *p1, const void *p2)
{
	Oid			v1 = *((const Oid *) p1);
	Oid			v2 = *((const Oid *) p2);

	if (v1 < v2)
		return -1;
	if (v1 > v2)
		return 1;
	return 0;
}

/* qsort comparison function for tuples by sort order */
static int
sort_order_cmp(const void *p1, const void *p2)
{
	HeapTuple	v1 = *((const HeapTuple *) p1);
	HeapTuple	v2 = *((const HeapTuple *) p2);
	Form_pg_enum en1 = (Form_pg_enum) GETSTRUCT(v1);
	Form_pg_enum en2 = (Form_pg_enum) GETSTRUCT(v2);

	if (en1->enumsortorder < en2->enumsortorder)
		return -1;
	else if (en1->enumsortorder > en2->enumsortorder)
		return 1;
	else
		return 0;
}
