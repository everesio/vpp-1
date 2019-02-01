#include <assert.h>
#include "hanat_worker_db.h"
#include <vnet/udp/udp_packet.h>
#include <vnet/ip/icmp46_packet.h>
#include <vppinfra/bihash_template.c>
#include <vppinfra/vec.h>

/*
 * Functions:
 * Instrumentation:
 * Total number of entries / memory usage
 * Bihash collisions
 * Searches per second
 * New entries per second
 * Delete entries per second
 *
 * Create time
 * Last used time
 *
 * Bytes / Packets forwarded against entry
 *
 * How to update the mapper?
 *  - pure idle timeout
 *  - tcp flags
 *  - worker signals just like ipfix to mapper
 *    (even for local create a buffer)
 *  - 
 */

void
hanat_db_init (hanat_db_t * db, u32 buckets, u32 memory_size)
{
  clib_bihash_init_16_8 (&db->cache, "hanat-worker-cache", buckets, memory_size);
}

void
hanat_db_free (hanat_db_t * db)
{
  clib_bihash_free_16_8 (&db->cache);
}

static void
hanat_key_from_ip (u32 fib_index, ip4_header_t *ip, hanat_session_key_t *key)
{
  u16 sport = 0, dport = 0;
  if (ip->protocol == IP_PROTOCOL_TCP ||
      ip->protocol == IP_PROTOCOL_UDP) {
    udp_header_t *udp = ip4_next_header (ip);
    sport = udp->src_port;
    dport = udp->dst_port;
  } else if (ip->protocol == IP_PROTOCOL_ICMP) {
    icmp46_header_t *icmp = (icmp46_header_t *) ip4_next_header (ip);
    if (icmp->type == ICMP4_echo_request ||
	icmp->type == ICMP4_echo_reply) {
      icmp_echo_header_t *echo = (icmp_echo_header_t *) (icmp + 1);
      dport = sport = echo->identifier;
    }
  }

  key->sa = ip->src_address;
  key->da = ip->dst_address;
  key->proto = ip->protocol;
  key->fib_index = fib_index;
  key->sp = sport;
  key->dp = dport;
}

hanat_session_t *
hanat_session_find (hanat_db_t *db, hanat_session_key_t *key)
{
  clib_bihash_kv_16_8_t kv, value;

  /* Add to index */
  kv.key[0] = key->as_u64[0];
  kv.key[1] = key->as_u64[1];

  if (clib_bihash_search_16_8 (&db->cache, &kv, &value))
    return 0;
  if (pool_is_free_index (db->sessions, value.value)) /* Is this check necessary? */
    return 0;
  return pool_elt_at_index (db->sessions, value.value);
}

hanat_session_t *
hanat_session_find_ip (hanat_db_t *db, u32 fib_index, ip4_header_t *ip)
{
  hanat_session_key_t key;
  hanat_key_from_ip(fib_index, ip, &key);
  return hanat_session_find(db, &key);
}

static int
hanat_session_stale_cb(clib_bihash_kv_16_8_t *kv, void *arg)
{
  vlib_main_t *vm = vlib_get_main();
  hanat_db_t *db = arg;
  hanat_session_t *s = pool_elt_at_index (db->sessions, kv->value);
  f64 now = vlib_time_now (vm);

  if (now >= s->entry.last_heard + 10) {
    /* Session timed out, reusing!!! */
    // Send session refresh data
    clib_warning("Reusing session");
    pool_put_index(db->sessions, kv->value);

    return 1;
  }
  return 0;
}


hanat_session_t *
hanat_session_add (hanat_db_t *db, hanat_session_key_t *key, hanat_session_entry_t *e)
{
  hanat_session_t *s;
  clib_bihash_kv_16_8_t kv;

  /* Check if session already exists */
  if (hanat_session_find(db, key))
    return 0;
    
  /* Add session to pool */
  pool_get_zero(db->sessions, s);
  s->key = *key;
  s->entry = *e;

  /* Add to index */
  kv.key[0] = key->as_u64[0];
  kv.key[1] = key->as_u64[1];
  kv.value = s - db->sessions;
#if 0  
  if (clib_bihash_add_del_16_8 (&db->cache, &kv, 1)) {
    assert(0);
    return 0;
  }
#endif
  if (clib_bihash_add_or_overwrite_stale_16_8(&db->cache, &kv, hanat_session_stale_cb, &db))
    assert(0);
  return s;
}

hanat_session_t *
hanat_worker_cache_add_incomplete(hanat_db_t *db, u32 fib_index, ip4_header_t *ip, u32 bi, bool tunnel)
{
  hanat_session_key_t key;
  hanat_session_t *s;

  hanat_key_from_ip(fib_index, ip, &key);
  /* Check if session already exists */
  s = hanat_session_find(db, &key);
  if (s) {
    /* Just buffer packet */
    // TODO: Only buffer a maximum of n packets
    clib_warning("Buffer exists");
    //vec_add1(*s->entry.buffer_vec, bi);
    s->entry.buffer = bi;
    return s;
  }

  /* Add session to pool */
  pool_get_zero(db->sessions, s);
  s->key = key;
  //vec_add1(*s->entry.buffer_vec, bi);
  s->entry.buffer = bi;
  s->entry.tunnel = tunnel;

  /* Add to index */
  clib_bihash_kv_16_8_t kv;
  kv.key[0] = key.as_u64[0];
  kv.key[1] = key.as_u64[1];
  kv.value = s - db->sessions;
  if (clib_bihash_add_or_overwrite_stale_16_8(&db->cache, &kv, hanat_session_stale_cb, &db))
    assert(0);
  return s;
}

void
hanat_session_delete (hanat_db_t *db, hanat_session_key_t *key)
{
  clib_bihash_kv_16_8_t kv, value;

  kv.key[0] = key->as_u64[0];
  kv.key[1] = key->as_u64[1];

  if (clib_bihash_search_16_8 (&db->cache, &kv, &value)) {
    printf("Find failed\n");
    return;
  }

  /* Remove from pool */
  if (pool_is_free_index (db->sessions, value.value))
    printf("Delete failed 2");

  pool_put_index(db->sessions, value.value);

  /* Remove from index */
  if (clib_bihash_add_del_16_8 (&db->cache, &kv, 0))
    printf("Delete failed");
}

// Shared table. Needs locking.
// n^2 buckets -> mapper ids
// mapper ids -> endpoints (pool)
// on failure replace mapper id to endpoint mapping
// worker uses mapper-id to endpoint mapping on keepalive/status updates

// LPM
/* Worker to mapper table */
/*
 * Vector of mapper id's.
 * Separate pool of mapper id to IP address
 */

static uint64_t
masked_address64 (uint64_t addr, uint8_t len)
{
  return len == 64 ? addr : addr & ~(~0ull >> len);
}

static u64
lpm_key (u64 fib_index, u32 address)
{
  return fib_index << 32 | address;
}

static int
lpm_64_lookup_core (hanat_pool_t *lpm, u64 addr, u8 pfxlen, u32 *value)
{
  clib_bihash_kv_8_8_t kv, v;
  int rv;
  kv.key = masked_address64(addr, pfxlen);
  rv = clib_bihash_search_8_8(&lpm->bihash, &kv, &v);
  if (rv != 0)
    return -1;
  *value = v.value;
  return 0;
}

u32
hanat_lpm_64_lookup (hanat_pool_t *lpm, u32 fib_index, u32 address)
{
  u64 addr = lpm_key(fib_index, address);
  int i = 0, rv;
  u32 value;
  clib_bitmap_foreach (i, lpm->prefix_lengths_bitmap,
    ({
      rv = lpm_64_lookup_core(lpm, addr, 64 - i, &value);
      if (rv == 0)
	return value;
    }));
  return ~0;
}

void
hanat_lpm_64_add (hanat_pool_t *lpm, u32 fib_index, u32 address, u8 pfxlen, u32 value)
{
  clib_bihash_kv_8_8_t kv;
  u64 addr = lpm_key(fib_index, address);
  int len = pfxlen + 32;

  kv.key = masked_address64(addr, len);
  kv.value = value;
  if (clib_bihash_add_del_8_8 (&lpm->bihash, &kv, 1)) {
    clib_warning("ADD failed");
    assert(0);
  }
  lpm->prefix_length_refcount[len]++;
  lpm->prefix_lengths_bitmap = clib_bitmap_set (lpm->prefix_lengths_bitmap, 64 - len, 1);
}

void
hanat_lpm_64_delete (hanat_pool_t *lpm, u32 fib_index, u32 address, u8 pfxlen)
{
  u64 addr = lpm_key(fib_index, address);
  clib_bihash_kv_8_8_t kv;
  int len = pfxlen + 32;
  kv.key = masked_address64(addr, len);
  if (clib_bihash_add_del_8_8 (&lpm->bihash, &kv, 0)) {
    clib_warning("DELETE failed");
    assert(0);
  }

  /* refcount accounting */
  ASSERT (lpm->prefix_length_refcount[len] > 0);
  if (--lpm->prefix_length_refcount[len] == 0) {
    lpm->prefix_lengths_bitmap = clib_bitmap_set (lpm->prefix_lengths_bitmap, 
						  64 - len, 0);
  }
}


void
hanat_mapper_table_init(hanat_pool_t *db)
{
  /* Make bihash sizes configurable */
  clib_bihash_init_8_8 (&db->bihash, "LPM 64", 64*1024, 32<<20);
}
