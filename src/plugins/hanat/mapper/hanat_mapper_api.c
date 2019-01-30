/*
 * Copyright (c) 2019 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hanat_mapper.h"
#include "hanat_state_sync.h"
#include <vlibapi/api.h>
#include <vlibmemory/api.h>
#include "hanat_mapper_msg_enum.h"

/* define message structures */
#define vl_typedefs
#include "hanat_mapper_all_api_h.h"
#undef vl_typedefs

/* define generated endian-swappers */
#define vl_endianfun
#include "hanat_mapper_all_api_h.h"
#undef vl_endianfun

#define vl_print(handle, ...) vlib_cli_output (handle, __VA_ARGS__)

#define REPLY_MSG_ID_BASE nm->msg_id_base
#include <vlibapi/api_helper_macros.h>

/* Get the API version number */
#define vl_api_version(n,v) static u32 api_version=(v);
#include "hanat_mapper_all_api_h.h"
#undef vl_api_version

/* Macro to finish up custom dump fns */
#define FINISH                                  \
    vec_add1 (s, 0);                            \
    vl_print (handle, (char *)s);               \
    vec_free (s);                               \
    return handle;

static void
vl_api_hanat_mapper_control_ping_t_handler (vl_api_hanat_mapper_control_ping_t
					    * mp)
{
  vl_api_hanat_mapper_control_ping_reply_t *rmp;
  hanat_mapper_main_t *nm = &hanat_mapper_main;
  int rv = 0;

  /* *INDENT-OFF* */
  REPLY_MACRO2 (VL_API_HANAT_MAPPER_CONTROL_PING_REPLY,
  ({
    rmp->vpe_pid = ntohl (getpid ());
  }));
  /* *INDENT-ON* */
}

static void *
vl_api_hanat_mapper_control_ping_t_print (vl_api_hanat_mapper_control_ping_t *
					  mp, void *handle)
{
  u8 *s;

  s = format (0, "SCRIPT: hanat_mapper_control_ping ");

  FINISH;
}

static void
vl_api_hanat_mapper_enable_t_handler (vl_api_hanat_mapper_enable_t * mp)
{
  vl_api_hanat_mapper_enable_reply_t *rmp;
  hanat_mapper_main_t *nm = &hanat_mapper_main;
  int rv = 0;

  rv = hanat_mapper_enable (clib_net_to_host_u16 (mp->port));

  REPLY_MACRO (VL_API_HANAT_MAPPER_ENABLE_REPLY);
}

static void *
vl_api_hanat_mapper_enable_t_print (vl_api_hanat_mapper_enable_t * mp,
				    void *handle)
{
  u8 *s;

  s =
    format (0, "SCRIPT: hanat_mapper_enable port %d",
	    clib_net_to_host_u16 (mp->port));

  FINISH;
}

static void
  vl_api_hanat_mapper_add_del_ext_addr_pool_t_handler
  (vl_api_hanat_mapper_add_del_ext_addr_pool_t * mp)
{
  vl_api_hanat_mapper_add_del_ext_addr_pool_reply_t *rmp;
  hanat_mapper_main_t *nm = &hanat_mapper_main;
  int rv = 0;

  rv =
    hanat_mapper_add_del_ext_addr_pool ((ip4_address_t *) & mp->prefix.prefix,
					mp->prefix.len,
					clib_net_to_host_u32 (mp->pool_id),
					mp->is_add);

  REPLY_MACRO (VL_API_HANAT_MAPPER_ADD_DEL_EXT_ADDR_POOL_REPLY);
}

static void *vl_api_hanat_mapper_add_del_ext_addr_pool_t_print
  (vl_api_hanat_mapper_add_del_ext_addr_pool_t * mp, void *handle)
{
  u8 *s;

  s = format (0, "SCRIPT: hanat_mapper_add_del_ext_addr_pool ");
  s =
    format (s, "%U/%d ", format_ip4_address, mp->prefix.prefix,
	    mp->prefix.len);
  s = format (s, "pool_id %d", clib_net_to_host_u32 (mp->pool_id));

  FINISH;
}

static void
vl_api_hanat_mapper_set_timeouts_t_handler (vl_api_hanat_mapper_set_timeouts_t
					    * mp)
{
  vl_api_hanat_mapper_set_timeouts_reply_t *rmp;
  hanat_mapper_main_t *nm = &hanat_mapper_main;
  int rv = 0;

  nm->udp_timeout = clib_net_to_host_u32 (mp->udp);
  nm->tcp_established_timeout = clib_net_to_host_u32 (mp->tcp_established);
  nm->tcp_transitory_timeout = clib_net_to_host_u32 (mp->tcp_transitory);
  nm->icmp_timeout = clib_net_to_host_u32 (mp->icmp);

  REPLY_MACRO (VL_API_HANAT_MAPPER_SET_TIMEOUTS_REPLY);
}

static void *
vl_api_hanat_mapper_set_timeouts_t_print (vl_api_hanat_mapper_set_timeouts_t *
					  mp, void *handle)
{
  u8 *s;

  s = format (0, "SCRIPT: hanat_mapper_set_timeouts ");
  s = format (s, "udp %d tcp_established %d tcp_transitory %d icmp %d\n",
	      clib_net_to_host_u32 (mp->udp),
	      clib_net_to_host_u32 (mp->tcp_established),
	      clib_net_to_host_u32 (mp->tcp_transitory),
	      clib_net_to_host_u32 (mp->icmp));

  FINISH;
}

static void
  vl_api_hanat_mapper_add_del_static_mapping_t_handler
  (vl_api_hanat_mapper_add_del_static_mapping_t * mp)
{
  vl_api_hanat_mapper_add_del_static_mapping_reply_t *rmp;
  hanat_mapper_main_t *nm = &hanat_mapper_main;
  ip4_address_t l_addr, e_addr;
  u8 proto;
  int rv = 0;

  memcpy (&l_addr, &mp->local_ip_address, sizeof (l_addr));
  memcpy (&e_addr, &mp->external_ip_address, sizeof (e_addr));
  proto = ip_proto_to_hanat_mapper_proto (mp->protocol);

  rv =
    hanat_mapper_add_del_static_mapping (&l_addr, &e_addr, mp->local_port,
					 mp->external_port, proto,
					 clib_net_to_host_u32 (mp->tenant_id),
					 clib_net_to_host_u32 (mp->pool_id),
					 mp->is_add);

  REPLY_MACRO (VL_API_HANAT_MAPPER_ADD_DEL_STATIC_MAPPING_REPLY);
}

static void *vl_api_hanat_mapper_add_del_static_mapping_t_print
  (vl_api_hanat_mapper_add_del_static_mapping_t * mp, void *handle)
{
  u8 *s;

  s = format (0, "SCRIPT: hanat_mapper_add_del_static_mapping ");
  s =
    format (s, "local_ip_address %U ", format_ip4_address,
	    mp->local_ip_address);
  s =
    format (s, "external_ip_address %U ", format_ip4_address,
	    mp->external_ip_address);
  s = format (s, "local_port %d ", clib_net_to_host_u16 (mp->local_port));
  s =
    format (s, "external_port %d ", clib_net_to_host_u16 (mp->external_port));
  s = format (s, "protocol %d ", mp->protocol);
  s = format (s, "tenant_id %d", clib_net_to_host_u32 (mp->tenant_id));
  s = format (s, "pool_id %d", clib_net_to_host_u32 (mp->pool_id));

  FINISH;
}

static void
  vl_api_hanat_mapper_set_state_sync_listener_t_handler
  (vl_api_hanat_mapper_set_state_sync_listener_t * mp)
{
  vl_api_hanat_mapper_set_state_sync_listener_reply_t *rmp;
  hanat_mapper_main_t *nm = &hanat_mapper_main;
  ip4_address_t addr;
  int rv = 0;

  memcpy (&addr, &mp->ip_address, sizeof (addr));

  rv =
    hanat_state_sync_set_listener (&addr, clib_net_to_host_u16 (mp->port),
				   clib_net_to_host_u32 (mp->path_mtu));

  REPLY_MACRO (VL_API_HANAT_MAPPER_SET_STATE_SYNC_LISTENER_REPLY);
}

static void *vl_api_hanat_mapper_set_state_sync_listener_t_print
  (vl_api_hanat_mapper_set_state_sync_listener_t * mp, void *handle)
{
  u8 *s;

  s = format (0, "SCRIPT: hanat_mapper_set_state_sync_listener ");
  s = format (s, "ip_address %U ", format_ip4_address, mp->ip_address);
  s = format (s, "port %d ", clib_net_to_host_u16 (mp->port));
  s = format (s, "path_mtu %d", clib_net_to_host_u32 (mp->path_mtu));

  FINISH;
}

static void
  vl_api_hanat_mapper_add_del_state_sync_failover_t_handler
  (vl_api_hanat_mapper_add_del_state_sync_failover_t * mp)
{
  vl_api_hanat_mapper_add_del_state_sync_failover_reply_t *rmp;
  hanat_mapper_main_t *nm = &hanat_mapper_main;
  ip4_address_t addr;
  int rv = 0;
  u32 index;

  memcpy (&addr, &mp->ip_address, sizeof (addr));

  rv =
    hanat_state_sync_add_del_failover (&addr, clib_net_to_host_u16 (mp->port),
				       &index, mp->is_add);

  /* *INDENT-OFF* */
  REPLY_MACRO2 (VL_API_HANAT_MAPPER_ADD_DEL_STATE_SYNC_FAILOVER_REPLY,
  ({
    rmp->failover_index = clib_host_to_net_u32 (index);
  }));
  /* *INDENT-ON* */
}

static void *vl_api_hanat_mapper_add_del_state_sync_failover_t_print
  (vl_api_hanat_mapper_add_del_state_sync_failover_t * mp, void *handle)
{
  u8 *s;

  s = format (0, "SCRIPT: hanat_mapper_add_del_state_sync_failover ");
  s = format (s, "ip_address %U ", format_ip4_address, mp->ip_address);
  s = format (s, "port %d ", clib_net_to_host_u16 (mp->port));

  FINISH;
}

static void
  vl_api_hanat_mapper_set_pool_failover_t_handler
  (vl_api_hanat_mapper_set_pool_failover_t * mp)
{
  vl_api_hanat_mapper_set_pool_failover_reply_t *rmp;
  hanat_mapper_main_t *nm = &hanat_mapper_main;
  int rv = 0;

  rv =
    hanat_mapper_set_pool_failover (clib_net_to_host_u32 (mp->pool_id),
				    clib_net_to_host_u32
				    (mp->failover_index));

  REPLY_MACRO (VL_API_HANAT_MAPPER_SET_POOL_FAILOVER_REPLY);
}

static void *vl_api_hanat_mapper_set_pool_failover_t_print
  (vl_api_hanat_mapper_set_pool_failover_t * mp, void *handle)
{
  u8 *s;

  s = format (0, "SCRIPT: hanat_mapper_set_pool_failover ");
  s = format (s, "pool_id %d failover_index %d ",
	      clib_net_to_host_u32 (mp->pool_id),
	      clib_net_to_host_u32 (mp->failover_index));

  FINISH;
}

static void
send_hanat_mapper_user_details (hanat_mapper_user_t * u,
				vl_api_registration_t * reg, u32 context)
{
  vl_api_hanat_mapper_user_details_t *rmp;
  hanat_mapper_main_t *nm = &hanat_mapper_main;

  rmp = vl_msg_api_alloc (sizeof (*rmp));
  clib_memset (rmp, 0, sizeof (*rmp));
  rmp->_vl_msg_id =
    ntohs (VL_API_HANAT_MAPPER_USER_DETAILS + nm->msg_id_base);
  clib_memcpy (rmp->address, &u->addr, sizeof (ip4_address_t));
  rmp->nsessions = clib_host_to_net_u32 (u->nsessions);
  rmp->tenant_id = clib_host_to_net_u32 (u->tenant_id);
  rmp->context = context;

  vl_api_send_msg (reg, (u8 *) rmp);
}

static void
vl_api_hanat_mapper_user_dump_t_handler (vl_api_hanat_mapper_user_dump_t * mp)
{
  vl_api_registration_t *reg;
  hanat_mapper_main_t *nm = &hanat_mapper_main;
  hanat_mapper_user_t *u;

  reg = vl_api_client_index_to_registration (mp->client_index);
  if (!reg)
    return;

  /* *INDENT-OFF* */
  pool_foreach (u, nm->db.users,
  ({
    send_hanat_mapper_user_details (u, reg, mp->context);
  }));
  /* *INDENT-ON* */
}

static void *
vl_api_hanat_mapper_user_dump_t_print (vl_api_hanat_mapper_user_dump_t * mp,
				       void *handle)
{
  u8 *s;

  s = format (0, "SCRIPT: hanat_mapper_user_dump ");

  FINISH;
}

static void
send_hanat_mapper_user_session_details (hanat_mapper_session_t * session,
					hanat_mapper_db_t * db,
					vl_api_registration_t * reg,
					u32 context)
{
  vl_api_hanat_mapper_user_session_details_t *rmp;
  hanat_mapper_main_t *nm = &hanat_mapper_main;
  hanat_mapper_mapping_t *mapping =
    pool_elt_at_index (db->mappings, session->mapping_index);

  rmp = vl_msg_api_alloc (sizeof (*rmp) + vec_len (session->opaque_data));
  clib_memset (rmp, 0, sizeof (*rmp));
  rmp->_vl_msg_id =
    ntohs (VL_API_HANAT_MAPPER_USER_SESSION_DETAILS + nm->msg_id_base);
  clib_memcpy (rmp->in_l_addr, &mapping->in_addr, sizeof (ip4_address_t));
  clib_memcpy (rmp->in_r_addr, &session->in_r_addr, sizeof (ip4_address_t));
  clib_memcpy (rmp->out_l_addr, &mapping->out_addr, sizeof (ip4_address_t));
  clib_memcpy (rmp->out_r_addr, &session->out_r_addr, sizeof (ip4_address_t));
  rmp->in_l_port = mapping->in_port;
  rmp->in_r_port = session->in_r_port;
  rmp->out_l_port = mapping->out_port;
  rmp->out_r_port = session->out_r_port;
  rmp->protocol = session->proto;
  rmp->tenant_id = clib_host_to_net_u32 (mapping->tenant_id);
  rmp->pool_id = clib_host_to_net_u32 (mapping->pool_id);
  rmp->total_bytes = clib_host_to_net_u64 (session->total_bytes);
  rmp->total_pkts = clib_host_to_net_u64 (session->total_pkts);
  rmp->opaque_len = (u8) vec_len (session->opaque_data);
  clib_memcpy (rmp->opaque_data, session->opaque_data,
	       vec_len (session->opaque_data));
  rmp->context = context;

  vl_api_send_msg (reg, (u8 *) rmp);
}

static void
  vl_api_hanat_mapper_user_session_dump_t_handler
  (vl_api_hanat_mapper_user_session_dump_t * mp)
{
  vl_api_registration_t *reg;
  hanat_mapper_main_t *nm = &hanat_mapper_main;
  hanat_mapper_user_t *u;
  dlist_elt_t *head, *elt;
  u32 elt_index, head_index;
  u32 session_index;
  hanat_mapper_session_t *session;
  ip4_address_t addr;

  reg = vl_api_client_index_to_registration (mp->client_index);
  if (!reg)
    return;

  clib_memcpy (&addr, &mp->address, sizeof (ip4_address_t));
  u =
    hanat_mapper_user_get (&nm->db, &addr,
			   clib_net_to_host_u32 (mp->tenant_id));
  if (!u)
    return;

  if (!u->nsessions)
    return;

  head_index = u->sessions_per_user_list_head_index;
  head = pool_elt_at_index (nm->db.list_pool, head_index);
  elt_index = head->next;
  elt = pool_elt_at_index (nm->db.list_pool, elt_index);
  session_index = elt->value;

  while (session_index != ~0)
    {
      session = pool_elt_at_index (nm->db.sessions, session_index);

      send_hanat_mapper_user_session_details (session, &nm->db, reg,
					      mp->context);

      elt_index = elt->next;
      elt = pool_elt_at_index (nm->db.list_pool, elt_index);
      session_index = elt->value;
    }
}

static void *vl_api_hanat_mapper_user_session_dump_t_print
  (vl_api_hanat_mapper_user_session_dump_t * mp, void *handle)
{
  u8 *s;

  s = format (0, "SCRIPT: hanat_mapper_user_session_dump ");
  s = format (s, "address %U tenant_id %d\n",
	      format_ip4_address, mp->address,
	      clib_net_to_host_u32 (mp->tenant_id));

  FINISH;
}

/* List of message types that this plugin understands */
#define foreach_hanat_mapper_plugin_api_msg                                  \
_(HANAT_MAPPER_CONTROL_PING, hanat_mapper_control_ping)                      \
_(HANAT_MAPPER_ENABLE, hanat_mapper_enable)                                  \
_(HANAT_MAPPER_ADD_DEL_EXT_ADDR_POOL, hanat_mapper_add_del_ext_addr_pool)    \
_(HANAT_MAPPER_SET_TIMEOUTS, hanat_mapper_set_timeouts)                      \
_(HANAT_MAPPER_ADD_DEL_STATIC_MAPPING, hanat_mapper_add_del_static_mapping)  \
_(HANAT_MAPPER_SET_STATE_SYNC_LISTENER, hanat_mapper_set_state_sync_listener)\
_(HANAT_MAPPER_ADD_DEL_STATE_SYNC_FAILOVER,                                  \
  hanat_mapper_add_del_state_sync_failover)                                  \
_(HANAT_MAPPER_SET_POOL_FAILOVER, hanat_mapper_set_pool_failover)            \
_(HANAT_MAPPER_USER_DUMP, hanat_mapper_user_dump)                            \
_(HANAT_MAPPER_USER_SESSION_DUMP, hanat_mapper_user_session_dump)

/* Set up the API message handling tables */
static clib_error_t *
hanat_mapper_plugin_api_hookup (vlib_main_t * vm)
{
  hanat_mapper_main_t *nm __attribute__ ((unused)) = &hanat_mapper_main;
#define _(N,n)                                                  \
    vl_msg_api_set_handlers((VL_API_##N + nm->msg_id_base),     \
                           #n,					\
                           vl_api_##n##_t_handler,              \
                           vl_noop_handler,                     \
                           vl_api_##n##_t_endian,               \
                           vl_api_##n##_t_print,                \
                           sizeof(vl_api_##n##_t), 1);
  foreach_hanat_mapper_plugin_api_msg;
#undef _

  return 0;
}

#define vl_msg_name_crc_list
#include "hanat_mapper_all_api_h.h"
#undef vl_msg_name_crc_list

static void
setup_message_id_table (hanat_mapper_main_t * nm, api_main_t * am)
{
#define _(id,n,crc) \
  vl_msg_api_add_msg_name_crc (am, #n "_" #crc, id + nm->msg_id_base);
  foreach_vl_msg_name_crc_hanat_mapper;
#undef _
}

static void
plugin_custom_dump_configure (hanat_mapper_main_t * nm)
{
#define _(n,f) nm->api_main->msg_print_handlers \
  [VL_API_##n + nm->msg_id_base]                \
    = (void *) vl_api_##f##_t_print;
  foreach_hanat_mapper_plugin_api_msg;
#undef _
}

clib_error_t *
hanat_mapper_api_init (vlib_main_t * vm, hanat_mapper_main_t * nm)
{
  u8 *name;
  clib_error_t *error = 0;

  name = format (0, "hanat_mapper_%08x%c", api_version, 0);

  /* Ask for a correctly-sized block of API message decode slots */
  nm->msg_id_base =
    vl_msg_api_get_msg_ids ((char *) name, VL_MSG_FIRST_AVAILABLE);

  error = hanat_mapper_plugin_api_hookup (vm);

  /* Add our API messages to the global name_crc hash table */
  setup_message_id_table (nm, nm->api_main);

  plugin_custom_dump_configure (nm);

  vec_free (name);

  return error;
}

/*
 * fd.io coding-style-patch-verification: ON
 *
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */