/*
 * Copyright (c) 2019 Cisco and/or its affiliates.
 *
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

#include <stdbool.h>
#include <arpa/inet.h>
#include <vnet/fib/fib_table.h>
#include <vppinfra/pool.h>
#include <vnet/udp/udp_packet.h>
#include <vnet/udp/udp.h>

#include "hanat_worker_db.h"

hanat_worker_main_t hanat_worker_main;
extern vlib_node_registration_t hanat_worker_slow_input_node;

static clib_error_t *
hanat_worker_init (vlib_main_t * vm)
{
  hanat_worker_main_t *hm = &hanat_worker_main;
  vlib_node_t *node;
  clib_memset (hm, 0, sizeof (*hm));
  hanat_db_init(&hm->db, 1024, 2000000);

  node = vlib_get_node_by_name (vm, (u8 *) "ip4-lookup");
  hm->ip4_lookup_node_index = node->index;
  node = vlib_get_node_by_name (vm, (u8 *) "hanat-worker");
  hm->hanat_worker_node_index = node->index;

  hanat_mapper_table_init(&hm->pool_db);
  hm->pool_db.n_buckets = 1024;

  /* Init API */
  return hanat_worker_api_init(vm, hm);
}

int
hanat_worker_interface_add_del (u32 sw_if_index, bool is_add, vl_api_hanat_worker_if_mode_t mode)
{
  hanat_worker_main_t *hm = &hanat_worker_main;
  hanat_interface_t *interface = 0, *i;

  /* *INDENT-OFF* */
  pool_foreach (i, hm->interfaces, ({
	if (i->sw_if_index == sw_if_index) {
	  interface = i;
	  break;
	}
      }));
  /* *INDENT-ON* */

  if (is_add) {
    if (interface)
      return VNET_API_ERROR_VALUE_EXIST;

    pool_get (hm->interfaces, interface);
    interface->sw_if_index = sw_if_index;
    interface->mode = mode;
    u32 index = interface - hm->interfaces;
    vec_validate(hm->interface_by_sw_if_index, sw_if_index);
    hm->interface_by_sw_if_index[sw_if_index] = index;
  } else {
    if (!interface)
      return VNET_API_ERROR_NO_SUCH_ENTRY;
    pool_put (hm->interfaces, interface);
    hm->interface_by_sw_if_index[sw_if_index] = ~0;
  }
  return vnet_feature_enable_disable ("ip4-unicast", "hanat-worker",
				      sw_if_index, is_add, 0, 0);
}

/*
 * Checksum delta
 */
static int
l3_checksum_delta(hanat_instructions_t instructions,
		  ip4_address_t pre_sa, ip4_address_t post_sa,
		  ip4_address_t pre_da, ip4_address_t post_da)
{
  ip_csum_t c = 0;
  if (instructions & HANAT_INSTR_SOURCE_ADDRESS) {
    c = ip_csum_add_even(c, post_sa.as_u32);
    c = ip_csum_sub_even(c, pre_sa.as_u32);
  }
  if (instructions & HANAT_INSTR_DESTINATION_ADDRESS) {
    c = ip_csum_sub_even(c, pre_da.as_u32);
    c = ip_csum_add_even(c, post_da.as_u32);
  }
  return c;
}

static int
l4_checksum_delta (hanat_instructions_t instructions, ip_csum_t c,
		   u16 pre_sp, u16 post_sp, u16 pre_dp, u16 post_dp)
{
  if (instructions & HANAT_INSTR_SOURCE_PORT) {
    c = ip_csum_add_even(c, post_sp);
    c = ip_csum_sub_even(c, pre_sp);
  }
  if (instructions & HANAT_INSTR_DESTINATION_PORT) {
    c = ip_csum_add_even(c, post_dp);
    c = ip_csum_sub_even(c, pre_dp);
  }
  return c;
}

int
hanat_worker_cache_add (hanat_session_key_t *key, hanat_session_entry_t *entry)
{
  hanat_worker_main_t *hm = &hanat_worker_main;

  ip_csum_t c = l3_checksum_delta(entry->instructions, key->sa, entry->post_sa, key->da, entry->post_da);
  entry->l4_checksum = l4_checksum_delta(entry->instructions, c, key->sp, entry->post_sp, key->dp, entry->post_dp);
  entry->checksum = c;

  hanat_session_t *s = hanat_session_add(&hm->db, key, entry);
  if (!s)
    return -1;
  return 0;
}

void
hanat_worker_cache_update(hanat_session_t *s, hanat_instructions_t instructions,
			  u32 fib_index, ip4_address_t *sa, ip4_address_t *da,
			  u16 sport, u16 dport)
{
  /* Update session entry */
  hanat_session_key_t *key = &s->key;
  hanat_session_entry_t *entry = &s->entry;
  entry->instructions = instructions;
  entry->fib_index = fib_index;
  memcpy(&entry->post_sa, &sa->as_u32, 4);
  memcpy(&entry->post_da, &da->as_u32, 4);
  entry->post_sp = sport; /* Network byte order */
  entry->post_dp = dport; /* Network byte order */

  ip_csum_t c = l3_checksum_delta(instructions, key->sa, entry->post_sa, key->da, entry->post_da);
  if (key->proto == IP_PROTOCOL_ICMP) /* ICMP checksum does not include pseudoheader */
    entry->l4_checksum = l4_checksum_delta(entry->instructions, 0, key->sp, entry->post_sp, key->dp, entry->post_dp);
  else
    entry->l4_checksum = l4_checksum_delta(entry->instructions, c, key->sp, entry->post_sp, key->dp, entry->post_dp);
  entry->checksum = c;

}

int
hanat_worker_mapper_add_del(bool is_add, u32 pool_id, ip4_address_t *prefix, u8 prefix_len,
			    ip46_address_t *mapper, ip46_address_t *src, u16 udp_port, u32 *mapper_index)
{
  hanat_worker_main_t *hm = &hanat_worker_main;
  hanat_pool_entry_t *poolentry;
  u32 mi = hanat_lpm_64_lookup (&hm->pool_db, pool_id, ntohl(prefix->as_u32));
  if (mi != ~0) {
    clib_warning("Exists already");
    return -1;
  }
  if (is_add) {
    pool_get_zero (hm->pool_db.pools, poolentry);
    *mapper_index = poolentry - hm->pool_db.pools;

    poolentry->pool_id = pool_id;
    poolentry->prefix = *prefix;
    poolentry->prefix_len = prefix_len;
    poolentry->src = *src;
    poolentry->mapper = *mapper;
    poolentry->udp_port = udp_port;

    /* Add prefix to LPM for outside to in traffix */
    hanat_lpm_64_add(&hm->pool_db, pool_id, ntohl(prefix->as_u32), prefix_len, *mapper_index);
  } else {
    hanat_lpm_64_delete(&hm->pool_db, pool_id, ntohl(prefix->as_u32), prefix_len);
    pool_put_index(hm->pool_db.pools, mi);
  }
  return 0;
}

/*
 * Vector of VRFs of pointers to bucket vector
 */
int
hanat_worker_mapper_buckets(u32 fib_index, u32 n, u32 mapper_index[])
{
  hanat_worker_main_t *hm = &hanat_worker_main;
  int i;
  u32 *b = 0;
  /* Replace stable hash */
  if (hm->pool_db.lb_buckets && vec_len(hm->pool_db.lb_buckets) >= fib_index &&
      hm->pool_db.lb_buckets[fib_index]) {
    vec_free(hm->pool_db.lb_buckets[fib_index]);
  }
  vec_validate_init_empty (hm->pool_db.lb_buckets, fib_index, 0);
  vec_validate(b, n);

  for (i = 0; i < n; i++) {
    b[i] = ntohl(mapper_index[i]);
  }

  hm->pool_db.lb_buckets[fib_index] = b;

  return 0;
}

int
hanat_worker_enable(u16 udp_port)
{
  vlib_main_t * vm = vlib_get_main();
  udp_register_dst_port (vm, udp_port, hanat_worker_slow_input_node.index, 1 /*is_ip4 */);

  return 0;
}

VLIB_INIT_FUNCTION (hanat_worker_init);