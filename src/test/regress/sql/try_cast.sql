create schema trycast;
set search_path = trycast;

-- no way to cast
select try_cast(12 as date);

-- typecast via I/O
select try_cast('11/11/20111' as int);
select try_cast('111d' as int);

-- typecast from pg_proc
select try_cast('112344466343'::int8 as int4);

-- arrays
select try_cast('{1, 2, 31111111111}'::int8[] as int4[]); -- pg_proc
select try_cast('{1, 2, lol}'::text[] as int4[]); -- via I/O
select try_cast('{1, 2, 31111111111}' as int4[]); -- 

reset search_path;