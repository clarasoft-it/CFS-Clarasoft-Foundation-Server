-- Table: public.RPSCFP

-- DROP TABLE public."RPSCFP";

CREATE TABLE public."RPSCFP"
(
    domain character varying(32) COLLATE pg_catalog."C.UTF-8",
    subdomain character varying(32) COLLATE pg_catalog."C.UTF-8",
    config character(32) COLLATE pg_catalog."C.UTF-8",
    path character varying(99) COLLATE pg_catalog."C.UTF-8",
    param character varying(32) COLLATE pg_catalog."C.UTF-8",
    value character varying(255) COLLATE pg_catalog."default",
    crtu character(64) COLLATE pg_catalog."default",
    crtd timestamp with time zone,
    updu character(64) COLLATE pg_catalog."default",
    updd timestamp with time zone
)
WITH (
    OIDS = FALSE
)
TABLESPACE pg_default;

ALTER TABLE public."RPSCFP"
    OWNER to clarabase;

GRANT ALL ON TABLE public."RPSCFP" TO clara;

GRANT ALL ON TABLE public."RPSCFP" TO clarabase;

COMMENT ON TABLE public."RPSCFP"
    IS 'CFS Repository: Configuration Parameters';

COMMENT ON COLUMN public."RPSCFP".domain
    IS 'Configuration domain';

COMMENT ON COLUMN public."RPSCFP".subdomain
    IS 'Configuration sub-domain';

COMMENT ON COLUMN public."RPSCFP".config
    IS 'Configuration name';

COMMENT ON COLUMN public."RPSCFP".path
    IS 'Configuration path';

COMMENT ON COLUMN public."RPSCFP".param
    IS 'Configuration parameter';

COMMENT ON COLUMN public."RPSCFP".value
    IS 'Configuration parameter value';

COMMENT ON COLUMN public."RPSCFP".crtu
    IS 'Creation user';

COMMENT ON COLUMN public."RPSCFP".crtd
    IS 'Creation timestamp';

COMMENT ON COLUMN public."RPSCFP".updu
    IS 'Update user';

COMMENT ON COLUMN public."RPSCFP".updd
    IS 'Update timestamp';
