#ifndef BAO_BUFFERSTATE_H
#define BAO_BUFFERSTATE_H

#include <stdio.h>
#include <stdint.h>

#include "postgres.h"
#include "uthash.h"
#include "postgres_ext.h"
#include "storage/bufmgr.h"
#include "storage/buf_internals.h"
#include "utils/lsyscache.h"
#include "utils/relfilenodemap.h"

#include "bao_util.h"

// Functions to create a JSON representation of the current PostgreSQL buffer state.

// Used to track how many buffer blocks are used for a particular relation.
struct buffer_counter {
  const char* key;
  int count;
  UT_hash_handle hh;
};
  

// modified from pg_buffercache_pages
static char* buffer_state() {
  // Generate a JSON string mapping each relation name to the number of buffered
  // blocks that relation has in the PG buffer cache.
  
  int i;
  Oid tablespace, relfilenode, relid;
  char* rel_name;
  char* buf;
  size_t json_size;
  FILE* stream;
 

  struct buffer_counter* map = NULL;
  struct buffer_counter* query = NULL;
  struct buffer_counter* tmp = NULL;

  // For each buffer, we either add or increment a hash table entry.
  for (i = 0; i < NBuffers; i++) {
    BufferDesc *bufHdr;
    
    bufHdr = GetBufferDescriptor(i);

    // In theory, we could lock each buffer header before reading it.
    // But this might slow down PG, and if our buffer cache is a little
    // inaccurate that is OK. Just keep in mind that the tablespace
    // and relfilenode we read from the buffer header may be inconsistent.
    //buf_state = LockBufHdr(bufHdr);

    tablespace = bufHdr->tag.rnode.spcNode;
    relfilenode = bufHdr->tag.rnode.relNode;

    // Ensure both are valid.
    if (tablespace == InvalidOid || relfilenode == InvalidOid)
      continue;

    // Get the relation ID attached to this file node.
    relid = RelidByRelfilenode(tablespace, relfilenode);
    if (relid == InvalidOid)
      continue;

    // Convert the relid to an actual relation name.
    rel_name = get_rel_name(relid);
    if (rel_name == NULL)
      continue;

    // Exclude system tables.
    if (starts_with(rel_name, "pg_") || starts_with(rel_name, "sql_"))
      continue;
    
    // See if this string is already in the table. If so, increment the count.
    // If not, add a new entry.
    HASH_FIND_STR(map, rel_name, query);
    if (query) {
      query->count++;
    } else {
      query = (struct buffer_counter*) malloc(sizeof(struct buffer_counter));
      query->key = rel_name;
      query->count = 1;
      HASH_ADD_KEYPTR(hh, map, query->key, strlen(query->key), query);
    }
  }

  // The hash table aggregation is done. Next, construct a JSON string.
  stream = open_memstream(&buf, &json_size);

  fprintf(stream, "{   "); // extra space here in case there is nothing in cache
  HASH_ITER(hh, map, query, tmp) {
    HASH_DEL(map, query);
    fprintf(stream, "\"%s\": %d,", query->key, query->count);
  }
  fprintf(stream, "}\n");
  fclose(stream);

  // Replace the last trailing comma with a space.
  buf[json_size-3] = ' ';
  return buf;

}


#endif
