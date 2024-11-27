create schema tryconvert;
set search_path = tryconvert;

-- start_ignore
CREATE EXTENSION IF NOT EXISTS try_convert;
-- end_ignore

-- SQL try_convert to compare with C one
CREATE OR REPLACE FUNCTION try_convert_by_sql(_in text, INOUT _out ANYELEMENT)
  LANGUAGE plpgsql AS
$func$
BEGIN
   EXECUTE format('SELECT %L::%s', $1, pg_typeof(_out))
   INTO  _out;
EXCEPTION WHEN others THEN
   -- do nothing: _out already carries default
END
$func$;

-- multiple fails
select count(*) from (
    select (try_convert(v, NULL::int2) is not distinct from try_convert_by_sql(v::text, NULL::int2))
        from (select (random()*100000)::int4 from generate_series(1, 1000)) as g(v)
) as t(eq) where not eq;

-- multiple fails from table

-- Domains check
-- Custom cast functions
-- Arrays

-- not date
select try_convert('12', NULL::date);

-- not int
select try_convert('11/11/20111', NULL::int);
select try_convert('111d', NULL::int);

-- to big for int2
select try_convert('112344466343', NULL::int2);

reset search_path;