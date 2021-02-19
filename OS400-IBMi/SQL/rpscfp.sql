-- --------------------------------------------------------------------------
--  CFS REPOSITORY                                                         
--  RPSCFP                                                                 
--  Configuration parameters                                 
-- --------------------------------------------------------------------------

CREATE OR REPLACE TABLE RPSCFP  (
domain   VARCHAR(32)  NOT NULL,
subdmn   VARCHAR(32)  NOT NULL,
config   VARCHAR(32)  NOT NULL,
path     VARCHAR(99)  NOT NULL,
param    VARCHAR(32)  NOT NULL,
value    VARCHAR(128) NOT NULL,
crtu     VARCHAR(10)  NOT NULL,
crtd     TIMESTAMP    NOT NULL,
updu     VARCHAR(10)  NOT NULL,
updd     TIMESTAMP    NOT NULL
)
RCDFMT RRPSCFP;

LABEL ON TABLE RPSCFP
	IS 'CFS Repository: Parameters';

LABEL ON COLUMN RPSCFP
(
domain   IS 'Config. Domain',
subdmn   IS 'Config. Sub-domain',
config   IS 'Configuration Name',
path     IS 'Parameter Path',
param    IS 'Parameter Name',
value    IS 'Parameter Value',
crtu     IS 'Creation User',
crtd     IS 'Creation Stamp',
updu     IS 'Modification User',
updd     IS 'Modification Stamp'
);

LABEL ON COLUMN RPSCFP
(
domain   TEXT IS 'Config. Domain',
subdmn   TEXT IS 'Config. Sub-domain',
config   TEXT IS 'Configuration Name',
path     TEXT IS 'Parameter Path',
param    TEXT IS 'Parameter Name',
value    TEXT IS 'Parameter Value',
crtu     TEXT IS 'Creation User',
crtd     TEXT IS 'Creation Stamp',
updu     TEXT IS 'Modification User',
updd     TEXT IS 'Modification Stamp'
);

CREATE INDEX RPSCFP_I01
  ON RPSCFP ( path ASC);

LABEL ON INDEX RPSCFP_I01
 IS 'CFS Conf-Param (Index:path)';

CREATE INDEX RPSCFP_I02
  ON RPSCFP ( domain ASC, subdmn ASC, config ASC, param ASC);

LABEL ON INDEX RPSCFP_I02
 IS 'CFS Conf-Param (Idx:domain/subdmn/config/param)';

