create schema tryconvert;
set search_path = tryconvert;

-- start_ignore
CREATE EXTENSION IF NOT EXISTS try_convert;
-- end_ignore

-- no way to convert
select try_convert(12::text, NULL::date);

-- typecast via I/O
select try_convert('11/11/20111', NULL::int);
select try_convert('111d', NULL::int);

-- typecast from pg_proc
select try_convert('112344466343', NULL::int4);

-- arrays
select try_convert('{1, 2, 31111111111}', NULL::int4[]); -- pg_proc
select try_convert('{1, 2, lol}', NULL::int4[]); -- via I/O
select try_convert('{1, 2, 31111111111}', NULL::int4[]); -- 

reset search_path;