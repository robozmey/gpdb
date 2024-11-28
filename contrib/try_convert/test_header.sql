-- SCRIPT-GENERATED TEST for TRY_CONVERT

create schema tryconvert;
set search_path = tryconvert;

-- start_ignore
CREATE EXTENSION IF NOT EXISTS try_convert;
-- end_ignore

-- SQL try_convert to compare with C one
CREATE OR REPLACE FUNCTION try_convert_by_sql(_in text, INOUT _out ANYELEMENT, source_type text default 'text')
  LANGUAGE plpgsql AS
$func$
BEGIN
   EXECUTE format('SELECT (%L::%s)::%s', $1, source_type, pg_typeof(_out))
   INTO  _out;
EXCEPTION WHEN others THEN
   -- do nothing: _out already carries default
END
$func$;
-- corrupts string
CREATE OR REPLACE FUNCTION corrupt(value text) RETURNS text
  LANGUAGE SQL AS 'select ''!@#$%^&*'' || value || ''!@#$%^&*''';
-- corrupts by random
CREATE OR REPLACE FUNCTION corrupt_random(value text, r float default 0.5) RETURNS text
  LANGUAGE SQL AS 'select case when random() < r then corrupt(value) else value end';
-- r
CREATE OR REPLACE FUNCTION test_cast_random(_source regtype, _target regtype, n int default 20, k int default 1000, OUT _out int)
  LANGUAGE plpgsql AS
$func$
BEGIN
   EXECUTE 
   'select count(*) from (
        select (try_convert(v, NULL::' || _target || ') is not distinct from try_convert_by_sql(v::text, NULL::' || _target || ', ''' || _source || '''::text))
            from (select (random()*' || k || ')::' || _source || ' from generate_series(1, ' || n || ')) as g(v)
    ) as t(eq) where not eq;'
   INTO  _out;
END
$func$;
-- test cast
CREATE OR REPLACE FUNCTION test_cast(_source regtype, _target regtype, n int default 20, k int default 1000, OUT _out int)
  LANGUAGE plpgsql AS
$func$
BEGIN
   EXECUTE 
   'select count(*) from (
        select (try_convert(v, NULL::' || _target || ') is not distinct from try_convert_by_sql(v::text, NULL::' || _target || ', ''' || _source || '''::text))
            from (select (random()*' || k || ')::' || _source || ' from generate_series(1, ' || n || ')) as g(v)
    ) as t(eq) where not eq;'
   INTO  _out;
END
$func$;