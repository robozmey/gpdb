/* contrib/try_convert/try_convert--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION try_convert" to load this file. \quit

/* ***********************************************
 * try_convert function for PostgreSQL
 * *********************************************** */

/* generic file access functions */

CREATE FUNCTION try_convert(text, anyelement)
RETURNS anyelement
AS 'MODULE_PATHNAME', 'try_convert'
LANGUAGE C STRICT;