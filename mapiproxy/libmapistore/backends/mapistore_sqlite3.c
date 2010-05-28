/*
   OpenChange Storage Abstraction Layer library
   MAPIStore SQLite backend

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

#include "mapistore_sqlite3.h"


/**
   \details Initialize sqlite3 mapistore backend

   \return MAPISTORE_SUCCESS on success
 */
static int sqlite3_init(void)
{
	DEBUG(0, ("sqlite3 backend initialized\n"));

	return MAPISTORE_SUCCESS;
}


/**
   \details Create a connection context to the sqlite3 backend

   \param mem_ctx pointer to the memory context
   \param uri pointer to the database path
   \param private_data pointer to the private backend context
   returns

   \return MAPISTORE_SUCCESS on success
 */
static int sqlite3_create_context(TALLOC_CTX *mem_ctx, const char *uri, void **private_data)
{
	struct sqlite3_context		*sqlite_ctx;
	sqlite3				*db;
	int				ret;

	DEBUG(0, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	ret = sqlite3_open(uri, &db);
	if (ret) {
		DEBUG(3, ("[%s:%d]: %s\n", __FUNCTION__, __LINE__,
			  sqlite3_errmsg(db)));
		sqlite3_close(db);
		return -1;
	}

	sqlite_ctx = talloc_zero(mem_ctx, struct sqlite3_context);
	sqlite_ctx->db = db;
	sqlite_ctx->private_data = NULL;

	*private_data = (void *)sqlite_ctx;

	return MAPISTORE_SUCCESS;
}


/**
   \details Delete a connection context from the sqlite3 backend

   \param private_data pointer to the current sqlite3 context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int sqlite3_delete_context(void *private_data)
{
	struct sqlite3_context	*sqlite_ctx = (struct sqlite3_context *)private_data;
	int			ret;

	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	if (!private_data) {
		return MAPISTORE_SUCCESS;
	}

	ret = sqlite3_close(sqlite_ctx->db);
	if (ret) return MAPISTORE_ERROR;

	return MAPISTORE_SUCCESS;
}


/**
   \details Atomic operation: Create directory (mkdir)

   \param private_data generic pointer to the sqlite3 context

   \return MAPI_E_SUCCESS on success
 */
static int sqlite3_op_mkdir(void *private_data, uint64_t parent_fid, uint64_t fid,
			   struct SRow *aRow)
{
	struct sqlite3_context		*sqlite_ctx = (struct sqlite3_context *)private_data;

	if (!sqlite_ctx) {
		return MAPISTORE_ERROR;
	}

	return MAPISTORE_SUCCESS;
}


/**
   \details Atomic operation: Delete directory (rmdir)

   \param private_data generic pointer to the sqlite3 context

   \return MAPI_E_SUCCESS on success
 */
static int sqlite3_op_rmdir(void *private_data)
{
	struct sqlite3_context		*sqlite_ctx = (struct sqlite3_context *)private_data;

	if (!sqlite_ctx) {
		return MAPISTORE_ERROR;
	}

	return MAPISTORE_SUCCESS;
}


/**
   \details Atomic operation: Open directory (opendir)

   \param private_data generic pointer to the sqlite3 context
   \param parent_fid the parent folder identifier
   \param fid the identifier of the colder to open  

   \return MAPI_E_SUCCESS on success
 */
static int sqlite3_op_opendir(void *private_data, uint64_t parent_fid, uint64_t fid)
{
	struct sqlite3_context		*sqlite_ctx = (struct sqlite3_context *)private_data;

	if (!sqlite_ctx) {
		return MAPISTORE_ERROR;
	}

	return MAPISTORE_SUCCESS;
}


/**
   \details Atomic operation: Close directory (closedir)

   \param private_data generic pointer to the sqlite3 context

   \return MAPI_E_SUCCESS on success
 */
static int sqlite3_op_closedir(void *private_data)
{
	struct sqlite3_context		*sqlite_ctx = (struct sqlite3_context *)private_data;

	if (!sqlite_ctx) {
		return MAPISTORE_ERROR;
	}

	return MAPISTORE_SUCCESS;
}


/**
   \details Entry point for mapistore SQLite backend

   \return MAPISTORE_SUCCESS on success, otherwise -1
 */
int mapistore_init_backend(void)
{
	struct mapistore_backend	backend;
	int				ret;

	/* Fill in our name */
	backend.name = "sqlite3";
	backend.description = "mapistore sqlite3 backend";
	backend.namespace = "sqlite://";

	/* Fill in all the operations */
	backend.init = sqlite3_init;
	backend.create_context = sqlite3_create_context;
	backend.delete_context = sqlite3_delete_context;
	backend.op_mkdir = sqlite3_op_mkdir;
	backend.op_rmdir = sqlite3_op_rmdir;
	backend.op_opendir = sqlite3_op_opendir;
	backend.op_closedir = sqlite3_op_closedir;
	backend.op_readdir_count = NULL;

	/* Register ourselves with the MAPISTORE subsystem */
	ret = mapistore_backend_register(&backend);
	if (ret != MAPISTORE_SUCCESS) {
		DEBUG(0, ("Failed to register the '%s' mapistore backend!\n", backend.name));
		return ret;
	}

	return MAPISTORE_SUCCESS;
}
