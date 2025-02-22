/* contrib/pageinspect/pageinspect--1.8.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pageinspect" to load this file. \quit

--
-- get_raw_page()
--
CREATE FUNCTION get_raw_page(text, int4)
RETURNS bytea
AS 'MODULE_PATHNAME', 'get_raw_page'
LANGUAGE C STRICT;

CREATE FUNCTION get_raw_page(text, text, int4)
RETURNS bytea
AS 'MODULE_PATHNAME', 'get_raw_page_fork'
LANGUAGE C STRICT;

--
-- page_header()
--
CREATE FUNCTION page_header(IN page bytea,
    OUT lsn pg_lsn,
    OUT checksum smallint,
    OUT flags smallint,
    OUT lower smallint,
    OUT upper smallint,
    OUT special smallint,
    OUT pagesize smallint,
    OUT version smallint,
    OUT prune_xid xid)
AS 'MODULE_PATHNAME', 'page_header'
LANGUAGE C STRICT;

--
-- heap_page_items()
--
CREATE FUNCTION heap_page_items(IN page bytea,
    OUT lp smallint,
    OUT lp_off smallint,
    OUT lp_flags smallint,
    OUT lp_len smallint,
    OUT t_xmin xid,
    OUT t_xmax xid,
    OUT t_field3 int4,
    OUT t_ctid tid,
    OUT t_infomask2 integer,
    OUT t_infomask integer,
    OUT t_hoff smallint,
    OUT t_bits text,
    OUT t_oid oid)
RETURNS SETOF record
AS 'MODULE_PATHNAME', 'heap_page_items'
LANGUAGE C STRICT;

--
-- bt_metap()
--
CREATE FUNCTION bt_metap(IN relname text,
    OUT magic int4,
    OUT version int4,
    OUT root int4,
    OUT level int4,
    OUT fastroot int4,
    OUT fastlevel int4)
AS 'MODULE_PATHNAME', 'bt_metap'
LANGUAGE C STRICT;

--
-- bt_page_stats()
--
CREATE FUNCTION bt_page_stats(IN relname text, IN blkno int4,
    OUT blkno int4,
    OUT type "char",
    OUT live_items int4,
    OUT dead_items int4,
    OUT avg_item_size int4,
    OUT page_size int4,
    OUT free_size int4,
    OUT btpo_prev int4,
    OUT btpo_next int4,
    OUT btpo int4,
    OUT btpo_flags int4)
AS 'MODULE_PATHNAME', 'bt_page_stats'
LANGUAGE C STRICT;

--
-- bt_page_items()
--
CREATE FUNCTION bt_page_items(IN relname text, IN blkno int4,
    OUT itemoffset smallint,
    OUT ctid tid,
    OUT itemlen smallint,
    OUT nulls bool,
    OUT vars bool,
    OUT data text)
RETURNS SETOF record
AS 'MODULE_PATHNAME', 'bt_page_items'
LANGUAGE C STRICT;

--
-- fsm_page_contents()
--
CREATE FUNCTION fsm_page_contents(IN page bytea)
RETURNS text
AS 'MODULE_PATHNAME', 'fsm_page_contents'
LANGUAGE C STRICT;

--
-- bm_metap()
--
CREATE FUNCTION bm_metap(IN relname text,
     OUT magic int4,
     OUT version int4,
     OUT auxrelid oid,
     OUT auxindexrelid oid,
     OUT lovlastblknum bigint)
AS 'MODULE_PATHNAME', 'bm_metap'
LANGUAGE C STRICT;

--
-- bm_lov_page_items()
--
CREATE FUNCTION bm_lov_page_items(IN relname text,
                                  IN blkno int4,
                                  OUT itemoffset smallint,
                                  OUT lov_head_blkno bigint,
                                  OUT lov_tail_blkno bigint,
                                  OUT last_complete_word text,
                                  OUT last_word text,
                                  OUT last_tid numeric,
                                  OUT last_setbit_tid numeric,
                                  OUT is_last_complete_word_fill bool,
                                  OUT is_last_word_fill bool)
    RETURNS SETOF record
AS 'MODULE_PATHNAME', 'bm_lov_page_items'
    LANGUAGE C STRICT;

--
-- bm_bitmap_page_header()
--
CREATE FUNCTION bm_bitmap_page_header(IN relname text,
                         IN blkno int4,
                         OUT num_words bigint,
                         OUT next_blkno bigint,
                         OUT last_tid numeric)
AS 'MODULE_PATHNAME', 'bm_bitmap_page_header'
    LANGUAGE C STRICT;

--
-- bm_bitmap_page_items()
--
CREATE FUNCTION bm_bitmap_page_items(IN relname text,
                                  IN blkno int4,
                                  OUT word_num bigint,
                                  OUT compressed bool,
                                  OUT content_word text)
    RETURNS SETOF record
AS 'MODULE_PATHNAME', 'bm_bitmap_page_items'
    LANGUAGE C STRICT;

--
-- bm_bitmap_page_items_bytea()
--
CREATE FUNCTION bm_bitmap_page_items(IN page bytea,
                                  OUT word_num bigint,
                                  OUT compressed bool,
                                  OUT content_word text)
    RETURNS SETOF record
AS 'MODULE_PATHNAME', 'bm_bitmap_page_items_bytea'
    LANGUAGE C STRICT;


CREATE FUNCTION get_ao_headers_info(
    reloid OID
)
RETURNS TABLE (
    "first row number" BIGINT,
    "large read position" BIGINT,
    "buffer offset" INTEGER,
    "block kind" TEXT,
    "header kind" TEXT,
    "current item count" INTEGER,
    isCompressed BOOLEAN,
    isLarge BOOLEAN,
    dataLen INTEGER
)
AS 'MODULE_PATHNAME', 'get_ao_headers_info'
    LANGUAGE C STRICT
EXECUTE ON ALL SEGMENTS;

CREATE FUNCTION get_aocs_headers_info(
    reloid OID
)
RETURNS TABLE (
    "column number" INTEGER,
    "large read position" BIGINT,
    "buffer offset" INTEGER,
    "header kind" TEXT,
    "first row" INTEGER,
    isCompressed BOOLEAN,
    isLarge BOOLEAN,
    "row count" INTEGER
)
AS 'MODULE_PATHNAME', 'get_aocs_headers_info'
    LANGUAGE C STRICT
EXECUTE ON ALL SEGMENTS;
