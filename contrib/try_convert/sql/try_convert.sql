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
-- CREATE DATA
CREATE TABLE tt_int2 (v int2) DISTRIBUTED BY (v);
INSERT INTO tt_int2 (v) VALUES
(6),
(0),
(2),
(2),
(7),
(6),
(89),
(8),
(42),
(2),
(21),
(50),
(26),
(198),
(649),
(544),
(220),
(589),
(8094),
(64),
(8058),
(6981),
(3402),
(1554);CREATE TABLE tt_int4 (v int4) DISTRIBUTED BY (v);
INSERT INTO tt_int4 (v) VALUES
(9),
(3),
(0),
(9),
(84),
(60),
(807),
(729),
(536),
(9731),
(3785),
(5520),
(82940),
(61851),
(86170),
(577352),
(704571),
(45824),
(2278982),
(2893879),
(797919),
(23279088),
(10100142),
(27797360),
(635684444),
(364832178),
(370180967);CREATE TABLE tt_int8 (v int8) DISTRIBUTED BY (v);
INSERT INTO tt_int8 (v) VALUES
(2),
(2),
(93),
(64),
(609),
(171),
(7291),
(1634),
(37945),
(98952),
(639999),
(556949),
(6846142),
(8428519),
(77599991),
(22904807),
(32100243),
(315453048),
(2677408759),
(2109828435),
(94290971433),
(87636762647),
(314677880798),
(655438665294),
(3956319010606),
(9145475897405),
(45885185258739),
(26488016649805),
(246627507693983),
(561368134163150),
(2627416085229352),
(5845859902235405),
(89782288360247696),
(39940050514039728),
(219320759157283328),
(997537606495110272);CREATE TABLE tt_float4 (v float4) DISTRIBUTED BY (v);
INSERT INTO tt_float4 (v) VALUES
(5.095262936764645),
(0.9090941217379389),
(0.4711637542473457),
(1.0964913035065915),
(62.744604170309),
(79.20793643629641),
(42.215996679968406),
(6.352770615195713),
(381.61928650653675),
(996.1213802400968),
(529.114345099137),
(971.0783776136182),
(8607.797022344981),
(114.81021942819636),
(7207.218193601946),
(6817.103690265748),
(53697.03304087951),
(26682.51899525428),
(64096.17985798081),
(11155.217359587643),
(434765.250669105),
(453723.70632920647),
(953815.9275210801),
(875852.9403781941);CREATE TABLE tt_float8 (v float8) DISTRIBUTED BY (v);
INSERT INTO tt_float8 (v) VALUES
(2.6338905075109076),
(5.005861130502983),
(17.865188053013135),
(91.26278393448204),
(870.5185698367669),
(298.4447914486329),
(6389.494948660052),
(6089.702114381723),
(15283.926854963482),
(76251.08000751512),
(539379.0301196257),
(778626.4786305582),
(5303536.721951775),
(5718.961279435053),
(32415605.70046731),
(1947674.2385832302),
(929098616.2646171),
(878721877.8231843),
(8316655293.611794),
(3075141254.026614),
(5792516649.418756),
(87800959920.40405),
(946949445297.994),
(85653452067.87878),
(4859904633166.138),
(692125184683.836),
(76060216525723.16),
(76583442930698.78),
(128391464499762.8),
(475282378098731.3);CREATE TABLE tt_numeric (v numeric) DISTRIBUTED BY (v);
INSERT INTO tt_numeric (v) VALUES
(5.4980359349494385),
(2.650566289400591),
(87.24330410852575),
(42.31379402008869),
(211.79820544208206),
(539.2960887794584),
(7299.3106908997615),
(2011.510633896959),
(31171.629130089495),
(99514.93566608947),
(649878.0576394534),
(438100.08391450404),
(5175758.410355906),
(1210041.9586826572),
(22469733.703155737),
(33808556.21474554),
(588308718.4572333),
(230114732.596577),
(2202173844.515595),
(709930860.0903254),
(63110295727.00989),
(22894178381.115437),
(905420013006.1279),
(859635400253.7465),
(708573498886.5344),
(2380046343689.952),
(66897777829628.055),
(21423680737043.86),
(132311848725025.0),
(935514240580671.0),
(5710430933252845.0),
(4726710263117941.0),
(7.846194242907534e+16),
(8.074969977666434e+16),
(1.904099143618777e+17),
(9.693081422882333e+16),
(4.310511824063775e+18),
(4.235786230199208e+18),
(4.67024668036675e+19),
(7.290758494598506e+19);
-- TEXT TESTS
select count(*) from (select (try_convert(v, NULL::text) is not distinct from try_convert_by_sql(v::text, NULL::text, 'int8'::text)) from tt_int8) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::int8) is not distinct from try_convert_by_sql(v::text, NULL::int8, 'text'::text)) from (select v::text from tt_int8) as t(v)) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::text) is not distinct from try_convert_by_sql(v::text, NULL::text, 'int4'::text)) from tt_int4) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::int4) is not distinct from try_convert_by_sql(v::text, NULL::int4, 'text'::text)) from (select v::text from tt_int4) as t(v)) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::text) is not distinct from try_convert_by_sql(v::text, NULL::text, 'int2'::text)) from tt_int2) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::int2) is not distinct from try_convert_by_sql(v::text, NULL::int2, 'text'::text)) from (select v::text from tt_int2) as t(v)) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::text) is not distinct from try_convert_by_sql(v::text, NULL::text, 'float8'::text)) from tt_float8) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::float8) is not distinct from try_convert_by_sql(v::text, NULL::float8, 'text'::text)) from (select v::text from tt_float8) as t(v)) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::text) is not distinct from try_convert_by_sql(v::text, NULL::text, 'float4'::text)) from tt_float4) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::float4) is not distinct from try_convert_by_sql(v::text, NULL::float4, 'text'::text)) from (select v::text from tt_float4) as t(v)) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::text) is not distinct from try_convert_by_sql(v::text, NULL::text, 'numeric'::text)) from tt_numeric) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::numeric) is not distinct from try_convert_by_sql(v::text, NULL::numeric, 'text'::text)) from (select v::text from tt_numeric) as t(v)) as t(eq) where not eq;
-- FUNCTION TESTS
select count(*) from (select (try_convert(v, NULL::int2) is not distinct from try_convert_by_sql(v::text, NULL::int2, 'int8'::text)) from tt_int8) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::int4) is not distinct from try_convert_by_sql(v::text, NULL::int4, 'int8'::text)) from tt_int8) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::float4) is not distinct from try_convert_by_sql(v::text, NULL::float4, 'int8'::text)) from tt_int8) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::float8) is not distinct from try_convert_by_sql(v::text, NULL::float8, 'int8'::text)) from tt_int8) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::numeric) is not distinct from try_convert_by_sql(v::text, NULL::numeric, 'int8'::text)) from tt_int8) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::int8) is not distinct from try_convert_by_sql(v::text, NULL::int8, 'int2'::text)) from tt_int2) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::int4) is not distinct from try_convert_by_sql(v::text, NULL::int4, 'int2'::text)) from tt_int2) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::float4) is not distinct from try_convert_by_sql(v::text, NULL::float4, 'int2'::text)) from tt_int2) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::float8) is not distinct from try_convert_by_sql(v::text, NULL::float8, 'int2'::text)) from tt_int2) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::numeric) is not distinct from try_convert_by_sql(v::text, NULL::numeric, 'int2'::text)) from tt_int2) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::int8) is not distinct from try_convert_by_sql(v::text, NULL::int8, 'int4'::text)) from tt_int4) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::int2) is not distinct from try_convert_by_sql(v::text, NULL::int2, 'int4'::text)) from tt_int4) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::float4) is not distinct from try_convert_by_sql(v::text, NULL::float4, 'int4'::text)) from tt_int4) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::float8) is not distinct from try_convert_by_sql(v::text, NULL::float8, 'int4'::text)) from tt_int4) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::numeric) is not distinct from try_convert_by_sql(v::text, NULL::numeric, 'int4'::text)) from tt_int4) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::int8) is not distinct from try_convert_by_sql(v::text, NULL::int8, 'float4'::text)) from tt_float4) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::int2) is not distinct from try_convert_by_sql(v::text, NULL::int2, 'float4'::text)) from tt_float4) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::int4) is not distinct from try_convert_by_sql(v::text, NULL::int4, 'float4'::text)) from tt_float4) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::float8) is not distinct from try_convert_by_sql(v::text, NULL::float8, 'float4'::text)) from tt_float4) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::numeric) is not distinct from try_convert_by_sql(v::text, NULL::numeric, 'float4'::text)) from tt_float4) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::int8) is not distinct from try_convert_by_sql(v::text, NULL::int8, 'float8'::text)) from tt_float8) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::int2) is not distinct from try_convert_by_sql(v::text, NULL::int2, 'float8'::text)) from tt_float8) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::int4) is not distinct from try_convert_by_sql(v::text, NULL::int4, 'float8'::text)) from tt_float8) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::float4) is not distinct from try_convert_by_sql(v::text, NULL::float4, 'float8'::text)) from tt_float8) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::numeric) is not distinct from try_convert_by_sql(v::text, NULL::numeric, 'float8'::text)) from tt_float8) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::int8) is not distinct from try_convert_by_sql(v::text, NULL::int8, 'numeric'::text)) from tt_numeric) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::int2) is not distinct from try_convert_by_sql(v::text, NULL::int2, 'numeric'::text)) from tt_numeric) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::int4) is not distinct from try_convert_by_sql(v::text, NULL::int4, 'numeric'::text)) from tt_numeric) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::float4) is not distinct from try_convert_by_sql(v::text, NULL::float4, 'numeric'::text)) from tt_numeric) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::float8) is not distinct from try_convert_by_sql(v::text, NULL::float8, 'numeric'::text)) from tt_numeric) as t(eq) where not eq;
select count(*) from (select (try_convert(v, NULL::numeric) is not distinct from try_convert_by_sql(v::text, NULL::numeric, 'numeric'::text)) from tt_numeric) as t(eq) where not eq;
-- FOOTER

reset search_path;
