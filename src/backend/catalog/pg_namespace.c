/*-------------------------------------------------------------------------
 *
 * pg_namespace.c
 *	  routines to support manipulation of the pg_namespace relation
 *
 * Portions Copyright (c) 1996-2011, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/catalog/pg_namespace.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_namespace.h"
#include "utils/builtins.h"
#include "utils/rel.h"
#include "utils/syscache.h"


/* ----------------
 * NamespaceCreate
 * ---------------
 */
Oid
NamespaceCreate(const char *nspName, Oid ownerId)
{
	Relation	nspdesc;
	HeapTuple	tup;
	Oid			nspoid;
	bool		nulls[Natts_pg_namespace];
	Datum		values[Natts_pg_namespace];
	NameData	nname;
	TupleDesc	tupDesc;
	ObjectAddress myself;
	int			i;

	/* sanity checks */
	if (!nspName)
		elog(ERROR, "no namespace name supplied");

	/* make sure there is no existing namespace of same name */
	if (SearchSysCacheExists1(NAMESPACENAME, PointerGetDatum(nspName)))
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_SCHEMA),
				 errmsg("schema \"%s\" already exists", nspName)));

	/* initialize nulls and values */
	for (i = 0; i < Natts_pg_namespace; i++)
	{
		nulls[i] = false;
		values[i] = (Datum) NULL;
	}
	namestrcpy(&nname, nspName);
	values[Anum_pg_namespace_nspname - 1] = NameGetDatum(&nname);
	values[Anum_pg_namespace_nspowner - 1] = ObjectIdGetDatum(ownerId);
	nulls[Anum_pg_namespace_nspacl - 1] = true;

	nspdesc = heap_open(NamespaceRelationId, RowExclusiveLock);
	tupDesc = nspdesc->rd_att;

	tup = heap_form_tuple(tupDesc, values, nulls);

	nspoid = simple_heap_insert(nspdesc, tup);
	Assert(OidIsValid(nspoid));

	CatalogUpdateIndexes(nspdesc, tup);

	heap_close(nspdesc, RowExclusiveLock);

	/* Record dependencies */
	myself.classId = NamespaceRelationId;
	myself.objectId = nspoid;
	myself.objectSubId = 0;

	/* dependency on owner */
	recordDependencyOnOwner(NamespaceRelationId, nspoid, ownerId);

	/* dependency on extension */
	recordDependencyOnCurrentExtension(&myself);

	/* Post creation hook for new schema */
	InvokeObjectAccessHook(OAT_POST_CREATE, NamespaceRelationId, nspoid, 0);

	return nspoid;
}
