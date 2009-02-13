/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "mapistore.h"
#include "mapistore_errors.h"
#include "mapistore_private.h"
#include <libmapi/dlinklist.h>
#include <libmapi/defs_private.h>

#include <tdb.h>

static struct id_mapping_context *mapping_ctx = NULL;
char *mapping_path = NULL;


/**
   \details Set the mapping path

   \param path pointer to the mapping path

   \note The mapping path can be set unless id_mapping_context is
   initialized. If path is NULL and mapping path is not yet
   initialized, then mapping_path will be reset to its default value
   when the initialization routine is called.

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ int mapistore_set_mapping_path(const char *path)
{
	TALLOC_CTX	*mem_ctx;
	DIR		*dir;

	/* Sanity checks */
	if (mapping_ctx) {
		return MAPISTORE_ERR_ALREADY_INITIALIZED;
	}

	/* Case 1. Path is set to NULL */
	if (!path) {
		if (mapping_path) {
			talloc_free(mapping_path);
		}
		mapping_path = NULL;
		return MAPISTORE_SUCCESS;
	}

	if (mapping_path) {
		talloc_free(mapping_path);
	}

	/* Case 2. path is initialized */

	/* Step 1. Check if path is valid path */
	dir = opendir(path);
	if (!dir) {
		return MAPISTORE_ERR_NO_DIRECTORY;
	}

	/* Step 2. TODO: Check for write permissions */
	
	mem_ctx = talloc_autofree_context();
	mapping_path = talloc_strdup(mem_ctx, path);
	return MAPISTORE_SUCCESS;
}

/**
   \details Return the current mapping path

   \return pointer to the mapping path.
 */
const char *mapistore_get_mapping_path(void)
{
	return (!mapping_path) ? MAPISTORE_MAPPING_PATH : (const char *)mapping_path;
}


/**
   \details Initialize the ID mapping context or return the existing
   one if already initialized.

   \param pctx pointer to the processing context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
int mapistore_init_mapping_context(struct processing_context *pctx)
{
	TDB_DATA	key;
	TDB_DATA	dbuf;
	TALLOC_CTX	*mem_ctx;
	char		*dbpath;
	uint64_t	last_id;
	char		*tmp_buf;
	int		ret;

	if (!pctx) return MAPISTORE_ERR_NOT_INITIALIZED;

	/* Step 0. return existing mapping context if already initialized */
	if (mapping_ctx) {
		pctx->mapping_ctx = mapping_ctx;
		return MAPISTORE_SUCCESS;
	} else {
		mapping_ctx = talloc_named(NULL, 0, "mapping_context");
		if (!mapping_ctx) return MAPISTORE_ERR_NO_MEMORY;
	}

	mem_ctx = talloc_named(NULL, 0, "mapistore_init_mapping_context");

	/* Step 1. Open/Create the used ID database */
	if (!mapping_ctx->used_ctx) {
		dbpath = talloc_asprintf(mem_ctx, "%s/%s", mapistore_get_mapping_path(), MAPISTORE_DB_NAME_USED_ID);
		mapping_ctx->used_ctx = tdb_open(dbpath, 0, 0, O_RDWR|O_CREAT, 0600);
		talloc_free(dbpath);
		if (!mapping_ctx->used_ctx) {
			DEBUG(3, ("[%s:%d]: %s\n", __FUNCTION__, __LINE__, strerror(errno)));
			talloc_free(mem_ctx);
			talloc_free(mapping_ctx);
			return MAPISTORE_ERR_DATABASE_INIT;
		}
	}

	/* Step 2. Open/Create the free ID database */
	if (!mapping_ctx->free_ctx) {
		dbpath = talloc_asprintf(mem_ctx, "%s/%s", mapistore_get_mapping_path(), MAPISTORE_DB_NAME_FREE_ID);
		mapping_ctx->free_ctx = tdb_open(dbpath, 0, 0, O_RDWR|O_CREAT, 0600);
		talloc_free(dbpath);
		if (!mapping_ctx->free_ctx) {
			DEBUG(3, ("[%s:%d]: %s\n", __FUNCTION__, __LINE__, strerror(errno)));
			talloc_free(mem_ctx);
			talloc_free(mapping_ctx);
			return MAPISTORE_ERR_DATABASE_INIT;
		}
	}

	/* Step 3. Retrieve the last ID value */
	key.dptr = (unsigned char *) MAPISTORE_DB_LAST_ID_KEY;
	key.dsize = strlen(MAPISTORE_DB_LAST_ID_KEY);

	dbuf = tdb_fetch(mapping_ctx->used_ctx, key);

	/* If the record doesn't exist, insert it */
	if (!dbuf.dptr || !dbuf.dsize) {
		dbuf.dptr = (unsigned char *) talloc_asprintf(mem_ctx, "0x%"PRIx64, (uint64_t) MAPISTORE_DB_LAST_ID_VAL);
		dbuf.dsize = strlen((const char *) dbuf.dptr);
		last_id = MAPISTORE_DB_LAST_ID_VAL;

		ret = tdb_store(mapping_ctx->used_ctx, key, dbuf, TDB_INSERT);
		talloc_free(dbuf.dptr);
		if (ret == -1) {
			DEBUG(3, ("[%s:%d]: Unable to create %s record: %s\n", __FUNCTION__, __LINE__,
				  MAPISTORE_DB_LAST_ID_KEY, tdb_errorstr(mapping_ctx->used_ctx)));
			talloc_free(mem_ctx);
			talloc_free(mapping_ctx);

			return MAPISTORE_ERR_DATABASE_OPS;
		}

	} else {
		tmp_buf = talloc_strndup(mem_ctx, (char *)dbuf.dptr, dbuf.dsize);
		free(dbuf.dptr);
		last_id = strtoull(tmp_buf, NULL, 16);
		talloc_free(tmp_buf);
	}

	mapping_ctx->last_id = last_id;

	pctx->mapping_ctx = mapping_ctx;
	talloc_free(mem_ctx);

	return MAPISTORE_SUCCESS;
}


/**
   \details Return an unused or new context identifier

   \param pctx pointer to the processing context
   \param context_id pointer to the context identifier the function
   returns

   \return a non zero context identifier on success, otherwise 0.
 */
int mapistore_get_context_id(struct processing_context *pctx, uint32_t *context_id)
{
	struct context_id_list	*el;

	/* Sanity checks */
	if (!pctx) return MAPISTORE_ERR_NOT_INITIALIZED;

	/* Step 1. The free context list doesn't exist yet */
	if (!pctx->free_ctx) {
		pctx->last_context_id++;
		*context_id = pctx->last_context_id;
	}

	/* Step 2. We have a free list */
	for (el = pctx->free_ctx; el; el = el->next) {
		if (el->context_id) {
			*context_id = el->context_id;
			DLIST_REMOVE(pctx->free_ctx, el);
			break;
		}
	}

	return MAPISTORE_SUCCESS;
}


/**
   \details Add a context identifier to the list

   \param pctx pointer to the processing context
   \param context_id the identifier referencing the context to free

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
int mapistore_free_context_id(struct processing_context *pctx, uint32_t context_id)
{
	struct context_id_list	*el;

	/* Sanity checks */
	if (!pctx) return MAPISTORE_ERR_NOT_INITIALIZED;

	/* Step 1. Ensure the list is not corrupted */
	for (el = pctx->free_ctx; el; el = el->next) {
		if (el->context_id == context_id) {
			return MAPISTORE_ERR_CORRUPTED;
		}
	}

	/* Step 2. Create the element and add it to the list */
	el = talloc_zero((TALLOC_CTX *)pctx, struct context_id_list);
	el->context_id = context_id;
	DLIST_ADD_END(pctx->free_ctx, el, struct context_id_list *);

	return MAPISTORE_SUCCESS;
}