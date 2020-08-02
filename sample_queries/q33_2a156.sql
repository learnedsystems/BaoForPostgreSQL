SELECT COUNT(*) FROM title as t,
kind_type as kt,
info_type as it1,
movie_info as mi1,
movie_info as mi2,
info_type as it2,
cast_info as ci,
role_type as rt,
name as n,
movie_keyword as mk,
keyword as k
WHERE
t.id = ci.movie_id
AND t.id = mi1.movie_id
AND t.id = mi2.movie_id
AND t.id = mk.movie_id
AND k.id = mk.keyword_id
AND mi1.movie_id = mi2.movie_id
AND mi1.info_type_id = it1.id
AND mi2.info_type_id = it2.id
AND (it1.id in ('7'))
AND (it2.id in ('3'))
AND t.kind_id = kt.id
AND ci.person_id = n.id
AND ci.role_id = rt.id
AND (mi1.info in ('PCS:Spherical','PFM:16 mm','PFM:35 mm','RAT:1.33 : 1','RAT:1.66 : 1','RAT:1.85 : 1'))
AND (mi2.info in ('Adult','Comedy','Documentary','Drama','Mystery','Romance','Short','Thriller','Western'))
AND (kt.kind in ('tv series','video game','video movie'))
AND (rt.role in ('miscellaneous crew','producer'))
AND (n.gender in ('f','m'))
AND (t.production_year <= 1990)
AND (t.production_year >= 1950)
