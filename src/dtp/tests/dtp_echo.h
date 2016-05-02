/**
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the GNU Lesser General Public License
 * (LGPL) version 2.1 which accompanies this distribution, and is available at
 * http://www.gnu.org/licenses/lgpl-2.1.html
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * GOVERNMENT LICENSE RIGHTS-OPEN SOURCE SOFTWARE
 * The Government's rights to use, modify, reproduce, release, perform, display,
 * or disclose this software are subject to the terms of the LGPL License as
 * provided in Contract No. B609815.
 * Any reproduction of computer software, computer software documentation, or
 * portions thereof marked with this legend must also reproduce the markings.
 *
 * (C) Copyright 2015 Intel Corporation.
 */
/**
 * This file is part of the dtp echo example which is based on dtp APIs.
 */

#ifndef __DTP_ECHO_H__
#define __DTP_ECHO_H__

#include <daos/transport.h>
#include <daos/rpc.h>

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <openssl/md5.h>

#define ECHO_OPC_CHECKIN    (0xA1)
#define ECHO_OPC_BULK_TEST  (0xA2)
#define ECHO_OPC_SHUTDOWN   (0x100)

#define ECHO_EXTRA_CONTEXT_NUM (3)

struct gecho {
	dtp_context_t	dtp_ctx;
	dtp_context_t	*extra_ctx;
	int		complete;
	bool		server;
};

extern struct gecho gecho;

int echo_srv_checkin(dtp_rpc_t *rpc);
int echo_srv_bulk_test(dtp_rpc_t *rpc);
int echo_srv_shutdown(dtp_rpc_t *rpc);

struct dtp_msg_field *echo_ping_checkin[] = {
	&DMF_UINT32,
	&DMF_UINT32,
	&DMF_STRING,
};
struct dtp_echo_checkin_req {
	int age;
	int days;
	dtp_string_t name;
};

struct dtp_msg_field *echo_ping_checkout[] = {
	&DMF_INT,
	&DMF_UINT32,
};
struct dtp_echo_checkin_reply {
	int ret;
	uint32_t room_no;
};

struct dtp_msg_field *echo_bulk_test_in[] = {
	&DMF_STRING,
	&DMF_STRING,
	&DMF_BULK,
};
struct dtp_echo_bulk_in_req {
	dtp_string_t bulk_intro_msg;
	dtp_string_t bulk_md5_ptr;
	dtp_bulk_t remote_bulk_hdl;
};

struct dtp_msg_field *echo_bulk_test_out[] = {
	&DMF_STRING,
	&DMF_INT,
};
struct dtp_echo_bulk_out_reply {
	char *echo_msg;
	int ret;
};

struct dtp_req_format DQF_ECHO_PING_CHECK =
	DEFINE_DTP_REQ_FMT("ECHO_PING_CHECK", echo_ping_checkin,
			   echo_ping_checkout);

struct dtp_req_format DQF_ECHO_BULK_TEST =
	DEFINE_DTP_REQ_FMT("ECHO_BULK_TEST", echo_bulk_test_in,
			   echo_bulk_test_out);

static inline void
echo_init(int server)
{
	int rc = 0, i;

	rc = dtp_init(server);
	assert(rc == 0);

	gecho.server = (server != 0);

	rc = dtp_context_create(NULL, &gecho.dtp_ctx);
	assert(rc == 0);

	if (server && ECHO_EXTRA_CONTEXT_NUM > 0) {
		gecho.extra_ctx = calloc(ECHO_EXTRA_CONTEXT_NUM,
					 sizeof(dtp_context_t));
		assert(gecho.extra_ctx != NULL);
		for (i = 0; i < ECHO_EXTRA_CONTEXT_NUM; i++) {
			rc = dtp_context_create(NULL, &gecho.extra_ctx[i]);
			assert(rc == 0);
		}
	}

	/* Just show the case that the client does not know the rpc handler,
	 * then client side can use dtp_rpc_reg, and server side can use
	 * dtp_rpc_srv_reg.
	 * If both client and server side know the rpc handler, they can call
	 * the same dtp_rpc_srv_reg. */
	if (server == 0) {
		rc = dtp_rpc_reg(ECHO_OPC_CHECKIN, &DQF_ECHO_PING_CHECK);
		assert(rc == 0);
		rc = dtp_rpc_reg(ECHO_OPC_BULK_TEST, &DQF_ECHO_BULK_TEST);
		assert(rc == 0);
		rc = dtp_rpc_reg(ECHO_OPC_SHUTDOWN, NULL);
		assert(rc == 0);
	} else {
		rc = dtp_rpc_srv_reg(ECHO_OPC_CHECKIN, &DQF_ECHO_PING_CHECK,
				     echo_srv_checkin);
		assert(rc == 0);
		rc = dtp_rpc_srv_reg(ECHO_OPC_BULK_TEST, &DQF_ECHO_BULK_TEST,
				     echo_srv_bulk_test);
		assert(rc == 0);
		rc = dtp_rpc_srv_reg(ECHO_OPC_SHUTDOWN, NULL,
				     echo_srv_shutdown);
		assert(rc == 0);
	}
}

static inline void
echo_fini(void)
{
	int rc = 0, i;

	rc = dtp_context_destroy(gecho.dtp_ctx, 0);
	assert(rc == 0);

	if (gecho.server && ECHO_EXTRA_CONTEXT_NUM > 0) {
		for (i = 0; i < ECHO_EXTRA_CONTEXT_NUM; i++) {
			rc = dtp_context_destroy(gecho.extra_ctx[i], 0);
			assert(rc == 0);
		}
	}

	rc = dtp_finalize();
	assert(rc == 0);
}

/* convert to string just to facilitate the pack/unpack */
static inline void
echo_md5_to_string(unsigned char *md5, dtp_string_t md5_str)
{
	char tmp[3] = {'\0'};
	int i;

	assert(md5 != NULL && md5_str != NULL);

	for (i = 0; i < 16; i++) {
		sprintf(tmp, "%02x", md5[i]);
		strcat(md5_str, tmp);
	}
}

int client_cb_common(const struct dtp_cb_info *cb_info)
{
	dtp_rpc_t		*rpc_req;
	struct dtp_echo_checkin_req *e_req;
	struct dtp_echo_checkin_reply *e_reply;

	rpc_req = cb_info->dci_rpc;

	/* set complete flag */
	printf("in client_cb_common, opc: 0x%x, dci_rc: %d.\n",
	       rpc_req->dr_opc, cb_info->dci_rc);
	*(int *) cb_info->dci_arg = 1;

	switch (cb_info->dci_rpc->dr_opc) {
	case ECHO_OPC_CHECKIN:
		e_req = dtp_req_get(rpc_req);
		if (e_req == NULL)
			return -DER_INVAL;

		e_reply = dtp_reply_get(rpc_req);
		if (e_reply == NULL)
			return -DER_INVAL;

		printf("%s checkin result - ret: %d, room_no: %d.\n",
		       e_req->name, e_reply->ret, e_reply->room_no);
		break;
	case ECHO_OPC_SHUTDOWN:
		break;
	default:
		break;
	}

	return 0;
}

#endif /* __DTP_ECHO_H__ */
