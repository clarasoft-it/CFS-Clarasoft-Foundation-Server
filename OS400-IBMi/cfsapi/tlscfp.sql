-- --------------------------------------------------------------------------
--  CFS REPOSITORY
--  TLS Configurations                                                       
--  TLSCFP                                                                   
--  TLS Parameters                                                           
-- -------------------------------------------------------------------------- --

CREATE TABLE TLSCFP (
name     VARCHAR(64)  NOT NULL,
param    INTEGER      NOT NULL,
type     INTEGER      NOT NULL,
level    INTEGER      NOT NULL,
value    VARCHAR(255) NOT NULL,
crtu     VARCHAR(10)  NOT NULL,
crtd     TIMESTAMP    NOT NULL,
updu     VARCHAR(10)  NOT NULL,
updd     TIMESTAMP    NOT NULL
)
RCDFMT RTLSCFP;

LABEL ON TABLE TLSCFP
	IS 'TLS Repository: Parameters';

LABEL ON COLUMN TLSCFP
(
name     IS 'Config. Name',
param    IS 'Parameter',
type     IS 'Parameter Type',
level    IS 'Initialize Level',
value    IS 'Parameter Value',
crtu     IS 'Creation User',
crtd     IS 'Creation Stamp',
updu     IS 'Modification User',
updd     IS 'Modification Stamp'
);

LABEL ON COLUMN TLSCFP
(
name     TEXT IS 'Config ID',
param    TEXT IS 'Parameter',
type     TEXT IS 'Parameter Type',
level    TEXT IS 'Initialize Level',
value    TEXT IS 'Parameter Value',
crtu     TEXT IS 'Creation User',
crtd     TEXT IS 'Creation Stamp',
updu     TEXT IS 'Modification User',
updd     TEXT IS 'Modification Stamp'
);

CREATE INDEX TLSCFP_I01
  ON TLSCFP ( name ASC, level ASC, type ASC);

LABEL ON INDEX TLSCFP_I01
 IS 'TLS Config Param (Index:name,level,type)';

CREATE INDEX TLSCFP_I02
  ON TLSCFP ( level ASC, param ASC, type ASC);

LABEL ON INDEX TLSCFP_I02
 IS 'TLS Config Param (Index:level,param,type)';

CREATE INDEX TLSCFP_I03
  ON TLSCFP ( level ASC, name ASC, type ASC);

LABEL ON INDEX TLSCFP_I03
 IS 'TLS Config Param (Index:level,name,type)';

CREATE INDEX TLSCFP_I04
  ON TLSCFP ( param ASC, type ASC);

LABEL ON INDEX TLSCFP_I04
 IS 'TLS Config Param (Index:param,type)';

