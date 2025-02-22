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
LANGUAGE C;


CREATE OR REPLACE FUNCTION add_type_for_try_convert(type regtype)
  RETURNS void 
  LANGUAGE plpgsql AS
$func$
BEGIN
   EXECUTE 'CREATE OR REPLACE FUNCTION try_convert(' || type || ', anyelement)
            RETURNS anyelement
            AS ''MODULE_PATHNAME'', ''try_convert''
            LANGUAGE C;';
END
$func$;

-- NUMBERS
select add_type_for_try_convert('int2'::regtype);
select add_type_for_try_convert('int4'::regtype);
select add_type_for_try_convert('int8'::regtype);
select add_type_for_try_convert('float4'::regtype);
select add_type_for_try_convert('float8'::regtype);
select add_type_for_try_convert('numeric'::regtype);
select add_type_for_try_convert('complex'::regtype);

-- TIME
select add_type_for_try_convert('date'::regtype);
select add_type_for_try_convert('time'::regtype);
select add_type_for_try_convert('timetz'::regtype);
select add_type_for_try_convert('timestamp'::regtype);
select add_type_for_try_convert('timestamptz'::regtype);
select add_type_for_try_convert('interval'::regtype);
select add_type_for_try_convert('abstime'::regtype);
select add_type_for_try_convert('reltime'::regtype);

-- CHARACTER
select add_type_for_try_convert('char'::regtype);
select add_type_for_try_convert('bpchar'::regtype);
select add_type_for_try_convert('varchar'::regtype);
select add_type_for_try_convert('text'::regtype);

-- BIT STRING
select add_type_for_try_convert('bit'::regtype);
select add_type_for_try_convert('varbit'::regtype);

select add_type_for_try_convert('bool'::regtype);

select add_type_for_try_convert('money'::regtype);

select add_type_for_try_convert('uuid'::regtype);

-- GEOMETRY
select add_type_for_try_convert('point'::regtype);

-- IP/MAC
select add_type_for_try_convert('cidr'::regtype);
select add_type_for_try_convert('inet'::regtype);
select add_type_for_try_convert('macaddr'::regtype);

-- OBJ
select add_type_for_try_convert('json'::regtype);
select add_type_for_try_convert('jsonb'::regtype);
select add_type_for_try_convert('xml'::regtype);
