/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

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

/**
   \file openchangedb.c

   \brief OpenChange Dispatcher database routines
 */

#include <mapiproxy/dcesrv_mapiproxy.h>
#include "libmapiproxy.h"
#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>
#include <libmapi/defs_private.h>

/**
   \details Retrieve the mailbox FolderID for given recipient from
   openchange dispatcher database

   \param ldb_ctx pointer to the OpenChange LDB context
   \param recipient the mailbox username
   \param SystemIdx the system folder index
   \param FolderId pointer to the folder identifier the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_SystemFolderID(void *ldb_ctx,
							 char *recipient, uint32_t SystemIdx,
							 uint64_t *FolderId)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	char				*ldb_filter;
	const char * const		attrs[] = { "*", NULL };
	int				ret;
	const char			*dn;
	struct ldb_dn			*ldb_dn = NULL;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!FolderId, MAPI_E_INVALID_PARAMETER, NULL);
	
	mem_ctx = talloc_named(NULL, 0, "get_SystemFolderID");

	/* Step 1. Search Mailbox Root DN */
	ldb_filter = talloc_asprintf(mem_ctx, "CN=%s", recipient);
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, ldb_filter);
	talloc_free(ldb_filter);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. If Mailbox root folder, check for FolderID within current record */
	if (SystemIdx == 0x1) {
		*FolderId = ldb_msg_find_attr_as_int64(res->msgs[0], "PidTagFolderId", 0);
		OPENCHANGE_RETVAL_IF(!*FolderId, MAPI_E_CORRUPT_STORE, mem_ctx);

		talloc_free(mem_ctx);
		return MAPI_E_SUCCESS;
	}

	dn = ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL);
	OPENCHANGE_RETVAL_IF(!dn, MAPI_E_CORRUPT_STORE, mem_ctx);

	/* Step 3. Search FolderID */
	ldb_dn = ldb_dn_new(mem_ctx, ldb_ctx, dn);
	OPENCHANGE_RETVAL_IF(!ldb_dn, MAPI_E_CORRUPT_STORE, mem_ctx);
	talloc_free(res);

	ldb_filter = talloc_asprintf(mem_ctx, "(&(objectClass=systemfolder)(SystemIdx=%d))", SystemIdx);
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_dn, LDB_SCOPE_SUBTREE, attrs, ldb_filter);
	talloc_free(ldb_filter);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*FolderId = ldb_msg_find_attr_as_int64(res->msgs[0], "PidTagFolderId", 0);
	OPENCHANGE_RETVAL_IF(!*FolderId, MAPI_E_CORRUPT_STORE, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the mailbox GUID for given recipient from
   openchange dispatcher database

   \param ldb_ctx pointer to the OpenChange LDB context
   \param recipient the mailbox username
   \param MailboxGUID pointer to the mailbox GUID the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_MailboxGuid(void *ldb_ctx,
						      char *recipient,
						      struct GUID *MailboxGUID)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	char				*ldb_filter;
	const char			*guid;
	const char * const		attrs[] = { "*", NULL };
	int				ret;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!MailboxGUID, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "get_MailboxGuid");

	/* Step 1. Search Mailbox DN */
	ldb_filter = talloc_asprintf(mem_ctx, "CN=%s", recipient);
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, ldb_filter);
	talloc_free(ldb_filter);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);
	
	/* Step 2. Retrieve MailboxGUID attribute's value */
	guid = ldb_msg_find_attr_as_string(res->msgs[0], "MailboxGUID", NULL);
	OPENCHANGE_RETVAL_IF(!guid, MAPI_E_CORRUPT_STORE, mem_ctx);

	GUID_from_string(guid, MailboxGUID);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the mailbox replica identifier and GUID for given
   recipient from openchange dispatcher database

   \param ldb_ctx pointer to the OpenChange LDB context
   \param recipient the mailbox username
   \param ReplID pointer to the replica identifier the function returns
   \param ReplGUID pointer to the replica GUID the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_MailboxReplica(void *ldb_ctx,
							 char *recipient, uint16_t *ReplID,
							 struct GUID *ReplGUID)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	char				*ldb_filter;
	const char			*guid;
	const char * const		attrs[] = { "*", NULL };
	int				ret;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ReplID, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ReplGUID, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "get_MailboxReplica");

	/* Step 1. Search Mailbox DN */
	ldb_filter = talloc_asprintf(mem_ctx, "CN=%s", recipient);
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, ldb_filter);
	talloc_free(ldb_filter);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. Retrieve ReplicaID attribute's value */
	*ReplID = ldb_msg_find_attr_as_int(res->msgs[0], "ReplicaID", 0);

	/* Step 3/ Retrieve ReplicaGUID attribute's value */
	guid = ldb_msg_find_attr_as_string(res->msgs[0], "ReplicaGUID", 0);
	OPENCHANGE_RETVAL_IF(!guid, MAPI_E_CORRUPT_STORE, mem_ctx);

	GUID_from_string(guid, ReplGUID);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the mapistore URI associated to a mailbox system
   folder.

   \param parent_ctx pointer to the memory context
   \param ldb_ctx pointer to the openchange LDB context
   \param fid the Folder identifier to search for
   \param mapistoreURL pointer on pointer to the mapistore URI the
   function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_mapistoreURI(TALLOC_CTX *parent_ctx,
						       void *ldb_ctx,
						       uint64_t fid,
						       char **mapistoreURL)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	char			*ldb_filter;
	const char * const	attrs[] = { "*", NULL };
	int			ret;

	mem_ctx = talloc_named(NULL, 0, "get_mapistoreURI");

	ldb_filter = talloc_asprintf(mem_ctx, "CN=0x%.16"PRIx64, fid);
	DEBUG(0, ("ldb_filter = '%s'\n", ldb_filter));
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, ldb_filter);
	talloc_free(ldb_filter);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*mapistoreURL = talloc_strdup(parent_ctx, ldb_msg_find_attr_as_string(res->msgs[0], "mapistore_uri", NULL));

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the Explicit message class and Folder identifier
   associated to the MessageClass search pattern.

   \param parent_ctx pointer to the memory context
   \param ldb_ctx pointer to the openchange LDB context
   \param recipient pointer to the mailbox's username
   \param MessageClass substring to search for
   \param fid pointer to the folder identifier the function returns
   \param ExplicitMessageClass pointer on pointer to the complete
   message class the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_ReceiveFolder(TALLOC_CTX *parent_ctx,
							void *ldb_ctx,
							const char *recipient,
							const char *MessageClass,
							uint64_t *fid,
							const char **ExplicitMessageClass)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	struct ldb_dn			*dn;
	struct ldb_message_element	*ldb_element;
	char				*dnstr;
	char				*ldb_filter;
	const char * const		attrs[] = { "*", NULL };
	int				ret;
	int				i;
	int				length;

	mem_ctx = talloc_named(NULL, 0, "get_ReceiveFolder");

	/* Step 1. Search Mailbox DN */
	ldb_filter = talloc_asprintf(mem_ctx, "CN=%s", recipient);
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, ldb_filter);
	talloc_free(ldb_filter);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	dnstr = talloc_strdup(mem_ctx, ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL));
	OPENCHANGE_RETVAL_IF(!dnstr, MAPI_E_NOT_FOUND, mem_ctx);

	talloc_free(res);

	/* Step 2. Search for MessageClass substring within user's mailbox */
	dn = ldb_dn_new(mem_ctx, ldb_ctx, dnstr);
	talloc_free(dnstr);

	ldb_filter = talloc_asprintf(mem_ctx, "(PidTagMessageClass=%s*)", MessageClass);
	ret = ldb_search(ldb_ctx, mem_ctx, &res, dn, LDB_SCOPE_SUBTREE, attrs, ldb_filter);
	talloc_free(ldb_filter);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);
	
	*fid = ldb_msg_find_attr_as_uint64(res->msgs[0], "PidTagFolderId", 0x0);

	/* Step 3. Find the longest ExplicitMessageClass matching MessageClass */
	ldb_element = ldb_msg_find_element(res->msgs[0], "PidTagMessageClass");
	for (i = 0, length = 0; i < ldb_element[0].num_values; i++) {
		if (!strncasecmp(MessageClass, (char *)ldb_element->values[i].data, 
				 strlen((char *)ldb_element->values[i].data)) &&
		    strlen((char *)ldb_element->values[i].data) > length) {

			if (*ExplicitMessageClass && strcmp(*ExplicitMessageClass, "")) {
				talloc_free((char *)*ExplicitMessageClass);
			}

			if (MessageClass && !strcmp(MessageClass, "All")) {
				*ExplicitMessageClass = "";
			} else {
				*ExplicitMessageClass = talloc_strdup(parent_ctx, (char *)ldb_element->values[i].data);
			}
			length = strlen((char *)ldb_element->values[i].data);
		}
	}
	OPENCHANGE_RETVAL_IF(!*ExplicitMessageClass, MAPI_E_NOT_FOUND, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Check if a property exists within an openchange dispatcher
   database record

   \param ldb_ctx pointer to the openchange LDB context
   \param proptag the MAPI property tag to lookup
   \param fid the record folder identifier

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_lookup_folder_property(void *ldb_ctx, 
							     uint32_t proptag, 
							     uint64_t fid)
{
	TALLOC_CTX	       	*mem_ctx;
	struct ldb_result      	*res = NULL;
	char		       	*ldb_filter;
	const char * const     	attrs[] = { "*", NULL };
	const char	       	*PidTagAttr = NULL;
	int		       	ret;

	mem_ctx = talloc_named(NULL, 0, "get_folder_property");

	/* Step 1. Find PidTagFolderId record */
	ldb_filter = talloc_asprintf(mem_ctx, "(PidTagFolderId=0x%llx)", fid);
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, ldb_filter);
	talloc_free(ldb_filter);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. Convert proptag into PidTag attribute */
	PidTagAttr = openchangedb_property_get_attribute(proptag);

	/* Step 3. Search for attribute */
	OPENCHANGE_RETVAL_IF(!ldb_msg_find_element(res->msgs[0], PidTagAttr), MAPI_E_NOT_FOUND, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details 
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_folder_property(TALLOC_CTX *parent_ctx, 
							  void *ldb_ctx,
							  uint32_t proptag,
							  uint64_t fid,
							  void **data)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	char			*ldb_filter;
	const char * const	attrs[] = { "*", NULL };
	const char		*PidTagAttr = NULL;
	int			ret;
	const char     		*str;
	uint64_t		*d;
	uint32_t		l;

	mem_ctx = talloc_named(NULL, 0, "get_folder_property");

	/* Step 1. Find PidTagFolderId record */
	ldb_filter = talloc_asprintf(mem_ctx, "(PidTagFolderId=0x%.16"PRIx64")", fid);
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, ldb_filter);
	talloc_free(ldb_filter);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. Convert proptag into PidTag attribute */
	PidTagAttr = openchangedb_property_get_attribute(proptag);
	OPENCHANGE_RETVAL_IF(!PidTagAttr, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 3. Ensure the element exists */
	OPENCHANGE_RETVAL_IF(!ldb_msg_find_element(res->msgs[0], PidTagAttr), MAPI_E_NOT_FOUND, mem_ctx);

	switch (proptag & 0xFFFF) {
	case PT_LONG:
		l = ldb_msg_find_attr_as_int(res->msgs[0], PidTagAttr, 0x0);
		*data = (void *)&l;
		break;
	case PT_I8:
		str = ldb_msg_find_attr_as_string(res->msgs[0], PidTagAttr, 0x0);
		d = talloc_zero(parent_ctx, uint64_t);
		*d = strtoull(str, NULL, 16);
		*data = (void *)d;
		break;
	case PT_STRING8:
	case PT_UNICODE:
		str = ldb_msg_find_attr_as_string(res->msgs[0], PidTagAttr, NULL);
		*data = (char *) talloc_strdup(parent_ctx, str);
		break;
	default:
		talloc_free(mem_ctx);
		DEBUG(0, ("[%s:%d] Property Type 0x%.4x not supported\n", __FUNCTION__, __LINE__, (proptag & 0xFFFF)));
		return MAPI_E_NOT_FOUND;
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
