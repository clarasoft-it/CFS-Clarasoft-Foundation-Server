-- Table: public.DVPRJM

-- DROP TABLE public."DVPRJM";

CREATE TABLE public."DVPRJM"
(
    prjtype character varying(10) COLLATE pg_catalog."default" NOT NULL,
    prjid character varying(32) COLLATE pg_catalog."default" NOT NULL,
    descr character varying(128) COLLATE pg_catalog."default" NOT NULL,
    date_open timestamp with time zone,
    date_close timestamp with time zone,
    status character varying(1) COLLATE pg_catalog."default" NOT NULL,
    crtu character varying(10) COLLATE pg_catalog."default" NOT NULL,
    crtd timestamp with time zone NOT NULL,
    modu character varying(10) COLLATE pg_catalog."default" NOT NULL,
    modd timestamp with time zone NOT NULL,
    ref character varying(64) COLLATE pg_catalog."default",
    CONSTRAINT "DVPRJM_pkey" PRIMARY KEY (prjtype, prjid)
)
WITH (
    OIDS = FALSE
)
TABLESPACE pg_default;

ALTER TABLE public."DVPRJM"
    OWNER to clarabase;


-- Table: public.DVSYSM

-- DROP TABLE public."DVSYSM";

CREATE TABLE public."DVSYSM"
(
    sysname character varying(32) COLLATE pg_catalog."C.UTF-8" NOT NULL,
    cptid character(36) COLLATE pg_catalog."C.UTF-8" NOT NULL,
    crtu character(64) COLLATE pg_catalog."default" NOT NULL,
    crtd timestamp with time zone NOT NULL,
    updu character(64) COLLATE pg_catalog."default" NOT NULL,
    updd timestamp with time zone NOT NULL,
    CONSTRAINT "DVSYSM_pkey" PRIMARY KEY (sysname)
)
WITH (
    OIDS = FALSE
)
TABLESPACE pg_default;

ALTER TABLE public."DVSYSM"
    OWNER to clarabase;

GRANT ALL ON TABLE public."DVSYSM" TO clara;

GRANT ALL ON TABLE public."DVSYSM" TO clarabase;

GRANT ALL ON TABLE public."DVSYSM" TO "clarasoft-it";

COMMENT ON TABLE public."DVSYSM"
    IS 'SDLC: target systems';


-- Table: public.DVTASKM

-- DROP TABLE public."DVTASKM";

CREATE TABLE public."DVTASKM"
(
    prjtype character varying(10) COLLATE pg_catalog."default" NOT NULL,
    prjid character varying(32) COLLATE pg_catalog."default" NOT NULL,
    taskid character varying(32) COLLATE pg_catalog."default" NOT NULL,
    descr character varying(128) COLLATE pg_catalog."default" NOT NULL,
    crtu character varying(10) COLLATE pg_catalog."default" NOT NULL,
    crtd timestamp with time zone NOT NULL,
    modu character varying(10) COLLATE pg_catalog."default" NOT NULL,
    modd timestamp without time zone NOT NULL,
    CONSTRAINT "DVTASKM_pkey" PRIMARY KEY (prjtype, prjid, taskid)
)
WITH (
    OIDS = FALSE
)
TABLESPACE pg_default;

ALTER TABLE public."DVTASKM"
    OWNER to clarabase;

-- Table: public.DVTRGM

-- DROP TABLE public."DVTRGM";

CREATE TABLE public."DVTRGM"
(
    trgid character varying(10) COLLATE pg_catalog."default" NOT NULL,
    cptid character varying(36) COLLATE pg_catalog."default" NOT NULL,
    crtu character varying(10) COLLATE pg_catalog."default" NOT NULL,
    crtd timestamp with time zone NOT NULL,
    modu character varying(10) COLLATE pg_catalog."default" NOT NULL,
    modd timestamp without time zone NOT NULL,
    CONSTRAINT "DVTRGM_pkey" PRIMARY KEY (trgid)
)
WITH (
    OIDS = FALSE
)
TABLESPACE pg_default;

ALTER TABLE public."DVTRGM"
    OWNER to clarabase;

COMMENT ON TABLE public."DVTRGM"
    IS 'SDLC: task targets';

-- Table: public.DVTSKTGM

-- DROP TABLE public."DVTSKTGM";

CREATE TABLE public."DVTSKTGM"
(
    prjtype character varying(10) COLLATE pg_catalog."default" NOT NULL,
    prjid character varying(32) COLLATE pg_catalog."default" NOT NULL,
    taskid character varying(32) COLLATE pg_catalog."default" NOT NULL,
    trgid character varying(10) COLLATE pg_catalog."default" NOT NULL,
    crtu character varying(10) COLLATE pg_catalog."default" NOT NULL,
    crtd timestamp with time zone NOT NULL,
    modu character varying(10) COLLATE pg_catalog."default" NOT NULL,
    modd timestamp without time zone NOT NULL,
    CONSTRAINT "DVTSKTGM_pkey" PRIMARY KEY (prjtype, prjid, taskid, trgid)
)
WITH (
    OIDS = FALSE
)
TABLESPACE pg_default;

ALTER TABLE public."DVTSKTGM"
    OWNER to clarabase;






