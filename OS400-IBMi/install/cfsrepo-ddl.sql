--  Clarasoft Foundation Server
--  CFS Repository (DB2 for i)
--  DDL

CREATE TABLE RPSCFM (
        DOMAIN VARCHAR(32) CCSID 37 NOT NULL ,
        SUBDMN VARCHAR(32) CCSID 37 NOT NULL ,
        CONFIG VARCHAR(32) CCSID 37 NOT NULL ,
        "PATH" VARCHAR(99) CCSID 37 NOT NULL ,
        STORAGE VARCHAR(32) CCSID 37 NOT NULL ,
        "FORMAT" VARCHAR(32) CCSID 37 NOT NULL ,
        ATTR VARCHAR(255) CCSID 37 NOT NULL ,
        DESCID VARCHAR(36) CCSID 37 NOT NULL ,
        CRTU VARCHAR(10) CCSID 37 NOT NULL ,
        CRTD TIMESTAMP NOT NULL ,
        UPDU VARCHAR(10) CCSID 37 NOT NULL ,
        UPDD TIMESTAMP NOT NULL )

        RCDFMT RRPSCFM    ;

LABEL ON TABLE RPSCFM
        IS 'Répositoire CFS: Configurations' ;

LABEL ON COLUMN RPSCFM
( DOMAIN IS 'Config. Domain' ,
        SUBDMN IS 'Config. Sub-domain' ,
        CONFIG IS 'Configuration Name' ,
        "PATH" IS 'Path' ,
        STORAGE IS 'Storage class' ,
        "FORMAT" IS 'Config format' ,
        ATTR IS 'Storage attributes' ,
        DESCID IS 'Desc. ID' ,
        CRTU IS 'Creation User' ,
        CRTD IS 'Creation Stamp' ,
        UPDU IS 'Modification User' ,
        UPDD IS 'Modification Stamp' ) ;

LABEL ON COLUMN RPSCFM
( DOMAIN TEXT IS 'Config. Domain' ,
        SUBDMN TEXT IS 'Config. Sub-domain' ,
        CONFIG TEXT IS 'Configuration Name' ,
        "PATH" TEXT IS 'Path' ,
        STORAGE TEXT IS 'Storage class' ,
        "FORMAT" TEXT IS 'Config format' ,
        ATTR TEXT IS 'Storage attributes' ,
        DESCID TEXT IS 'Description ID for string table' ,
        CRTU TEXT IS 'Creation User' ,
        CRTD TEXT IS 'Creation Stamp' ,
        UPDU TEXT IS 'Modification User' ,
        UPDD TEXT IS 'Modification Stamp' ) ;

GRANT DELETE , INSERT , SELECT , UPDATE
ON RPSCFM TO PUBLIC ;

GRANT ALTER , DELETE , INDEX , INSERT , REFERENCES , SELECT , UPDATE
ON RPSCFM TO QPGMR WITH GRANT OPTION ;


CREATE TABLE RPSCFP (
        DOMAIN VARCHAR(32) CCSID 37 NOT NULL ,
        SUBDMN VARCHAR(32) CCSID 37 NOT NULL ,
        CONFIG VARCHAR(32) CCSID 37 NOT NULL ,
        "PATH" VARCHAR(99) CCSID 37 NOT NULL ,
        PARAM VARCHAR(32) CCSID 37 NOT NULL ,
        "VALUE" VARCHAR(128) CCSID 37 NOT NULL ,
        CRTU VARCHAR(10) CCSID 37 NOT NULL ,
        CRTD TIMESTAMP NOT NULL ,
        UPDU VARCHAR(10) CCSID 37 NOT NULL ,
        UPDD TIMESTAMP NOT NULL )

        RCDFMT RRPSCFP    ;

LABEL ON TABLE RPSCFP
        IS 'Répositoire CFS: Paramètres' ;

LABEL ON COLUMN RPSCFP
( DOMAIN IS 'Config. Domain' ,
        SUBDMN IS 'Config. Sub-domain' ,
        CONFIG IS 'Configuration Name' ,
        "PATH" IS 'Parameter Path' ,
        PARAM IS 'Parameter Name' ,
        "VALUE" IS 'Parameter Value' ,
        CRTU IS 'Creation User' ,
        CRTD IS 'Creation Stamp' ,
        UPDU IS 'Modification User' ,
        UPDD IS 'Modification Stamp' ) ;

LABEL ON COLUMN RPSCFP
( DOMAIN TEXT IS 'Config. Domain' ,
        SUBDMN TEXT IS 'Config. Sub-domain' ,
        CONFIG TEXT IS 'Configuration Name' ,
        "PATH" TEXT IS 'Parameter Path' ,
        PARAM TEXT IS 'Parameter Name' ,
        "VALUE" TEXT IS 'Parameter Value' ,
        CRTU TEXT IS 'Creation User' ,
        CRTD TEXT IS 'Creation Stamp' ,
        UPDU TEXT IS 'Modification User' ,
        UPDD TEXT IS 'Modification Stamp' ) ;

GRANT ALTER , DELETE , INDEX , INSERT , REFERENCES , SELECT , UPDATE
ON RPSCFP TO ALDONCMS WITH GRANT OPTION ;

GRANT ALTER , DELETE , INDEX , INSERT , REFERENCES , SELECT , UPDATE
ON RPSCFP TO FE_OBJ_OWN WITH GRANT OPTION ;

GRANT ALTER , DELETE , INDEX , INSERT , REFERENCES , SELECT , UPDATE
ON RPSCFP TO QPGMR WITH GRANT OPTION ;


CREATE TABLE RPSENM (
        DOMAIN VARCHAR(32) CCSID 37 NOT NULL ,
        SUBDMN VARCHAR(32) CCSID 37 NOT NULL ,
        CONFIG VARCHAR(32) CCSID 37 NOT NULL ,
        "PATH" VARCHAR(99) CCSID 37 NOT NULL ,
        PARAM VARCHAR(32) CCSID 37 NOT NULL ,
        SEQ INTEGER NOT NULL ,
        "VALUE" VARCHAR(512) CCSID 37 NOT NULL ,
        CRTU VARCHAR(10) CCSID 37 NOT NULL ,
        CRTD TIMESTAMP NOT NULL ,
        UPDU VARCHAR(10) CCSID 37 NOT NULL ,
        UPDD TIMESTAMP NOT NULL )

        RCDFMT RRPSENM    ;

LABEL ON TABLE RPSENM
        IS 'Répositoire: Enumerations' ;

LABEL ON COLUMN RPSENM
( DOMAIN IS 'Config. Domain' ,
        SUBDMN IS 'Config. Sub-domain' ,
        CONFIG IS 'Config. name' ,
        "PATH" IS 'Config. path' ,
        PARAM IS 'Parameter' ,
        SEQ IS 'Sequence' ,
        "VALUE" IS 'Value' ,
        CRTU IS 'Creation user' ,
        CRTD IS 'Creation stamp' ,
        UPDU IS 'Modification user' ,
        UPDD IS 'Modification stamp' ) ;

LABEL ON COLUMN RPSENM
( DOMAIN TEXT IS 'Config. Domain' ,
        SUBDMN TEXT IS 'Config. Sub-domain' ,
        CONFIG TEXT IS 'Config. name' ,
        "PATH" TEXT IS 'Config. path' ,
        PARAM TEXT IS 'Parameter' ,
        SEQ TEXT IS 'Sequence' ,
        "VALUE" TEXT IS 'Value' ,
        CRTU TEXT IS 'Creation user' ,
        CRTD TEXT IS 'Creation stamp' ,
        UPDU TEXT IS 'Modification user' ,
        UPDD TEXT IS 'Modification stamp' ) ;

GRANT DELETE , INSERT , SELECT , UPDATE
ON RPSENM TO PUBLIC ;

GRANT ALTER , DELETE , INDEX , INSERT , REFERENCES , SELECT , UPDATE
ON RPSENM TO QPGMR WITH GRANT OPTION ;


CREATE TABLE TLSCFP (
        NAME VARCHAR(64) CCSID 37 NOT NULL ,
        PARAM INTEGER NOT NULL ,
        "TYPE" INTEGER NOT NULL ,
        LEVEL INTEGER NOT NULL ,
        "VALUE" VARCHAR(128) CCSID 37 NOT NULL ,
        CRTU VARCHAR(10) CCSID 37 NOT NULL ,
        CRTD TIMESTAMP NOT NULL ,
        UPDU VARCHAR(10) CCSID 37 NOT NULL ,
        UPDD TIMESTAMP NOT NULL )

        RCDFMT RTLSCFP    ;

LABEL ON TABLE TLSCFP
        IS 'Répositoire TLS: Paramètres des config.' ;

LABEL ON COLUMN TLSCFP
( NAME IS 'Config. Name' ,
        PARAM IS 'Parameter' ,
        "TYPE" IS 'Parameter Type' ,
        LEVEL IS 'Initialize Level' ,
        "VALUE" IS 'Parameter Value' ,
        CRTU IS 'Creation User' ,
        CRTD IS 'Creation Stamp' ,
        UPDU IS 'Modification User' ,
        UPDD IS 'Modification Stamp' ) ;

LABEL ON COLUMN TLSCFP
( NAME TEXT IS 'Config ID' ,
        PARAM TEXT IS 'Parameter' ,
        "TYPE" TEXT IS 'Parameter Type' ,
        LEVEL TEXT IS 'Initialize Level' ,
        "VALUE" TEXT IS 'Parameter Value' ,
        CRTU TEXT IS 'Creation User' ,
        CRTD TEXT IS 'Creation Stamp' ,
        UPDU TEXT IS 'Modification User' ,
        UPDD TEXT IS 'Modification Stamp' ) ;

GRANT ALTER , DELETE , INDEX , INSERT , REFERENCES , SELECT , UPDATE
ON TLSCFP TO ALDONCMS WITH GRANT OPTION ;

GRANT ALTER , DELETE , INDEX , INSERT , REFERENCES , SELECT , UPDATE
ON TLSCFP TO FE_OBJ_OWN WITH GRANT OPTION ;

GRANT ALTER , DELETE , INDEX , INSERT , REFERENCES , SELECT , UPDATE
ON TLSCFP TO QPGMR WITH GRANT OPTION ;


CREATE TABLE TLSLVLK (
        LEVEL INTEGER NOT NULL ,
        NAME VARCHAR(64) CCSID 37 NOT NULL ,
        CRTU VARCHAR(10) CCSID 37 NOT NULL ,
        CRTD TIMESTAMP NOT NULL ,
        UPDU VARCHAR(10) CCSID 37 NOT NULL ,
        UPDD TIMESTAMP NOT NULL )

        RCDFMT RTLSLVLK   ;

LABEL ON TABLE TLSLVLK
        IS 'CFS Repository: TLS appli. level lookup table' ;

LABEL ON COLUMN TLSLVLK
( LEVEL IS 'Aplication level' ,
        NAME IS 'Level name' ,
        CRTU IS 'Creation user' ,
        CRTD IS 'Creation stamp' ,
        UPDU IS 'Modification user' ,
        UPDD IS 'Modification stamp' ) ;

LABEL ON COLUMN TLSLVLK
( LEVEL TEXT IS 'Application level' ,
        NAME TEXT IS 'Level name' ,
        CRTU TEXT IS 'Creation user' ,
        CRTD TEXT IS 'Creation stamp' ,
        UPDU TEXT IS 'Modification user' ,
        UPDD TEXT IS 'Modification stamp' ) ;

GRANT DELETE , INSERT , SELECT , UPDATE
ON TLSLVLK TO PUBLIC ;

GRANT ALTER , DELETE , INDEX , INSERT , REFERENCES , SELECT , UPDATE
ON TLSLVLK TO QPGMR WITH GRANT OPTION ;


CREATE TABLE TLSPRLK (
        NAME VARCHAR(128) CCSID 37 NOT NULL ,
        PARAM INTEGER NOT NULL ,
        STRID VARCHAR(36) CCSID 37 NOT NULL ,
        "TYPE" INTEGER NOT NULL ,
        LEVEL INTEGER NOT NULL ,
        CRTU VARCHAR(10) CCSID 37 NOT NULL ,
        CRTD TIMESTAMP NOT NULL ,
        UPDU VARCHAR(10) CCSID 37 NOT NULL ,
        UPDD TIMESTAMP NOT NULL )

        RCDFMT RTLSPRLK   ;

LABEL ON TABLE TLSPRLK
        IS 'CFS Repository: TLS parameter lookup table' ;

LABEL ON COLUMN TLSPRLK
( NAME IS 'Parameter name' ,
        PARAM IS 'Parameter' ,
        STRID IS 'String ID' ,
        "TYPE" IS 'Parameter Type' ,
        LEVEL IS 'Initialize Level' ,
        CRTU IS 'Creation user' ,
        CRTD IS 'Creation stamp' ,
        UPDU IS 'Modification user' ,
        UPDD IS 'Modification stamp' ) ;

LABEL ON COLUMN TLSPRLK
( NAME TEXT IS 'Parameter name' ,
        PARAM TEXT IS 'Parameter' ,
        STRID TEXT IS 'String ID' ,
        "TYPE" TEXT IS 'Parameter Type' ,
        LEVEL TEXT IS 'Initialize Level' ,
        CRTU TEXT IS 'Creation user' ,
        CRTD TEXT IS 'Creation stamp' ,
        UPDU TEXT IS 'Modification user' ,
        UPDD TEXT IS 'Modification stamp' ) ;

GRANT ALTER , DELETE , INDEX , INSERT , REFERENCES , SELECT , UPDATE
ON TLSPRLK TO ALDONCMS WITH GRANT OPTION ;

GRANT ALTER , DELETE , INDEX , INSERT , REFERENCES , SELECT , UPDATE
ON TLSPRLK TO FE_OBJ_OWN WITH GRANT OPTION ;

GRANT ALTER , DELETE , INDEX , INSERT , REFERENCES , SELECT , UPDATE
ON TLSPRLK TO QPGMR WITH GRANT OPTION ;


CREATE TABLE TLSPRLKV (
        PARAM INTEGER NOT NULL ,
        "TYPE" INTEGER NOT NULL ,
        CHKVAL VARCHAR(10) CCSID 37 NOT NULL ,
        NAME VARCHAR(128) CCSID 37 NOT NULL ,
        CRTU VARCHAR(10) CCSID 37 NOT NULL ,
        CRTD TIMESTAMP NOT NULL ,
        UPDU VARCHAR(10) CCSID 37 NOT NULL ,
        UPDD TIMESTAMP NOT NULL )

        RCDFMT RTLSPRLKV  ;

LABEL ON TABLE TLSPRLKV
        IS 'CFS Repo.: TLS parm. value lookup table' ;

LABEL ON COLUMN TLSPRLKV
( PARAM IS 'Parameter value' ,
        "TYPE" IS 'Parameter Type' ,
        CHKVAL IS 'Param check val.' ,
        NAME IS 'Param check val name' ,
        CRTU IS 'Creation user' ,
        CRTD IS 'Creation stamp' ,
        UPDU IS 'Modification user' ,
        UPDD IS 'Modification stamp' ) ;

LABEL ON COLUMN TLSPRLKV
( PARAM TEXT IS 'Parameter value' ,
        "TYPE" TEXT IS 'Parameter Type' ,
        CHKVAL TEXT IS 'Param check val.' ,
        NAME TEXT IS 'Param check val name' ,
        CRTU TEXT IS 'Creation user' ,
        CRTD TEXT IS 'Creation stamp' ,
        UPDU TEXT IS 'Modification user' ,
        UPDD TEXT IS 'Modification stamp' ) ;

GRANT DELETE , INSERT , SELECT , UPDATE
ON TLSPRLKV TO PUBLIC ;

GRANT ALTER , DELETE , INDEX , INSERT , REFERENCES , SELECT , UPDATE
ON TLSPRLKV TO QPGMR WITH GRANT OPTION ;


CREATE TABLE TLSTYLK (
        "TYPE" INTEGER NOT NULL ,
        NAME VARCHAR(64) CCSID 37 NOT NULL ,
        CRTU VARCHAR(10) CCSID 37 NOT NULL ,
        CRTD TIMESTAMP NOT NULL ,
        UPDU VARCHAR(10) CCSID 37 NOT NULL ,
        UPDD TIMESTAMP NOT NULL )

        RCDFMT RTLSTYLK   ;

LABEL ON TABLE TLSTYLK
        IS 'CFS Repository: TLS parameter type lookup table' ;

LABEL ON COLUMN TLSTYLK
( "TYPE" IS 'Parameter Type' ,
        NAME IS 'Type name' ,
        CRTU IS 'Creation user' ,
        CRTD IS 'Creation stamp' ,
        UPDU IS 'Modification user' ,
        UPDD IS 'Modification stamp' ) ;

LABEL ON COLUMN TLSTYLK
( "TYPE" TEXT IS 'Parameter Type' ,
        NAME TEXT IS 'Type name' ,
        CRTU TEXT IS 'Creation user' ,
        CRTD TEXT IS 'Creation stamp' ,
        UPDU TEXT IS 'Modification user' ,
        UPDD TEXT IS 'Modification stamp' ) ;

GRANT DELETE , INSERT , SELECT , UPDATE
ON TLSTYLK TO PUBLIC ;

GRANT ALTER , DELETE , INDEX , INSERT , REFERENCES , SELECT , UPDATE
ON TLSTYLK TO QPGMR WITH GRANT OPTION ;

