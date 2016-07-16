

#ifndef ILA_H
#define ILA_H

#include <vnet/vnet.h>
#include <vnet/ip/ip.h>

#include <vppinfra/bihash_8_8.h>
#include <vppinfra/bihash_template.h>

typedef enum {
  ILA_CSUM_MODE_NO_ACTION,
  ILA_CSUM_MODE_NEUTRAL_MAP,
  ILA_CSUM_MODE_ADJUST_TRANSPORT
} ila_csum_mode_t;

typedef struct {
  u64 identifier;
  u64 locator;
  u64 sir_prefix;
  u32 ila_adj_index;
  ila_csum_mode_t csum_mode;
  u16 csum_modifier;
} ila_entry_t;

typedef struct {
  ila_entry_t *entries; //Pool of ILA entries

  u64 lookup_table_nbuckets;
  u64 lookup_table_size;
  clib_bihash_8_8_t id_to_entry_table;

  u32 ila_sir2ila_feature_index;
} ila_main_t;


typedef struct {
  u64 identifier;
  u64 locator;
  u64 sir_prefix;
  u32 local_adj_index;
  ila_csum_mode_t csum_mode;
  u8 is_del;
} ila_add_del_entry_args_t;

int ila_add_del_entry(ila_add_del_entry_args_t *args);
int ila_interface(u32 sw_if_index, u8 disable);

#endif //ILA_H


