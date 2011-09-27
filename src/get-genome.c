static char rcsid[] = "$Id: get-genome.c,v 1.50 2005/05/04 18:04:51 twu Exp $";
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>		/* For rindex */
#include <ctype.h>

#include "bool.h"
#include "mem.h"
#include "genomicpos.h"
#include "sequence.h"
#include "chrom.h"
#include "genome.h"
#include "iit-read.h"
#include "datadir.h"
#include "separator.h"
#include "getopt.h"

/*
#define DASH "-"
*/

#ifdef DEBUG
#define debug(x) x
#else
#define debug(x)
#endif


/* Program Options */
static bool replacex = false;
static bool uncompressedp = false;
static bool uppercasep = false;
static bool revcomp = false;
static bool coordp = false;
static char *user_genomedir = NULL;
static char *dbroot = NULL;
static char *releasestring = NULL;
static char altstrainp = false;
static char *user_typestring = NULL;
static int wraplength = 60;
static char *header = NULL;

/* Dump options */
static bool dumpchrp = false;
static bool dumpsegsp = false;

static struct option long_options[] = {
  /* Input options */
  {"dir", required_argument, 0, 'D'}, /* user_genomedir */
  {"db", required_argument, 0, 'd'}, /* dbroot */
  {"rel", required_argument, 0, 'R'}, /* releasestring */

  /* Output options */
  {"altstrain", no_argument, 0, 'S'}, /* altstrainp */
  {"strain", required_argument, 0, 's'}, /* user_typestring */
  {"coords", no_argument, 0, 'C'}, /* coordp */
  {"uppercase", no_argument, 0, 'U'}, /* uppercasep */
  {"replacex", no_argument, 0, 'X'}, /* replacex */
  {"wraplength", required_argument, 0, 'l'}, /* wraplength */
  {"fullgenome", no_argument, 0, 'G'}, /* uncompressedp */
  {"header", required_argument, 0, 'h'}, /* header */

  /* Dump options */
  {"chromosomes", no_argument, 0, 'L'},	/* dumpchrp */
  {"contigs", no_argument, 0, 'I'}, /* dumpsegsp */
  
  /* Help options */
  {"version", no_argument, 0, 'V'}, /* print_program_version */
  {"help", no_argument, 0, '?'}, /* print_program_usage */
  {0, 0, 0, 0}
};


static void
print_program_version () {
  fprintf(stdout,"\n");
  fprintf(stdout,"get-genome: retrieves genomic segments\n");
  fprintf(stdout,"Part of GMAP package, version %s\n",PACKAGE_VERSION);
  fprintf(stdout,"Thomas D. Wu and Colin K. Watanabe, Genentech, Inc.\n");
  fprintf(stdout,"Contact: twu@gene.com\n");
  fprintf(stdout,"\n");
  return;
}

static void
print_program_usage () {
  fprintf(stdout,"\
Usage: get-genome [OPTIONS...] -d genome [genome:]range, or\n\
       get-genome [OPTIONS...] -d genome chromosome:range, or\n\
       get-genome [OPTIONS...] -d genome contig[:range]\n\
where\n\
   range is startposition\n\
         or startposition-endposition\n\
         or startposition+length\n\
\n\
Input options\n\
  -D, --dir=STRING        Data directory\n\
  -d, --db=STRING         Database (e.g., NHGD)\n\
  -R, --rel=STRING        Release of database\n\
\n\
Output options\n\
  -S, --altstrain         Show sequence for all strains (in addition to reference)\n\
  -s, --strain=STRING     Show sequence for a particular strain\n\
  -C, --coords            Show coordinates only\n\
  -U, --uppercase         Convert sequence to uppercase\n\
  -X, --replacex          Replace Xs with N\n\
  -l, --wraplength=INT    Wrap length for sequence (default=60)\n\
  -G, --fullgenome        Use full (uncompressed) version of genome\n\
  -h, --header=STRING     Desired header line\n\
  --version               Show version\n\
\n\
Dump options\n\
  -L, --chromosomes       List all chromosomes with universal coordinates\n\
  -I, --contigs           List all contigs with universal coordinates\n\
\n\
Help options\n\
  -V, --version           Show version\n\
  -?, --help              Show this help message\n\
");
  return;
}

/* Parsing functions */

/* Note that isnumber is a function in ctype.h on some systems */
static bool
isnumberp (unsigned int *result, char *string) {
  char *p = string;

  *result = 0U;
  while (*p != '\0') {
    if (*p == ',') {
      /* Skip commas */
    } else if (!isdigit(*p)) {
      return false;
    } else {
      *result = (*result) * 10 + (*p - '0');
    }
    p++;
  }
  return true;
}

/* Returns coordinates as zero-based */
static bool
isrange (unsigned int *left, unsigned int *length, bool *revcomp, char *string) {
  bool result;
  char *copy, *startstring, *endstring;
  unsigned int start, end;

  copy = (char *) CALLOC(strlen(string)+1,sizeof(char));
  strcpy(copy,string);

  if (index(copy,'.')) {
    startstring = strtok(copy,"..");
    endstring = strtok(NULL,"..");
    if (!isnumberp(&start,startstring) || !isnumberp(&end,endstring)) {
      result = false;
    } else if (start <= end) {
      *length = end - start + 1;
      *left = start - 1;
      *revcomp = false;
      debug(printf("(..) "));
      result = true;
    } else {
      *length = start - end + 1;
      *left = end - 1;
      *revcomp = true;
      debug(printf("(..) "));
      result = true;
    }

  } else if (index(copy,'-')) {
    /* Old notation */
    startstring = strtok(copy,"--");
    endstring = strtok(NULL,"--");
    if (!isnumberp(&start,startstring) || !isnumberp(&end,endstring)) {
      result = false;
    } else if (start <= end) {
      *length = end - start + 1;
      *left = start - 1;
      *revcomp = false;
      debug(printf("(--) "));
      result = true;
    } else {
      *length = start - end + 1;
      *left = end - 1;
      *revcomp = true;
      debug(printf("(--) "));
      result = true;
    }

  } else if (index(copy,'+')) {
    startstring = strtok(copy,"+");
    endstring = strtok(NULL,"+");
    if (!isnumberp(&start,startstring) || !isnumberp(&end,endstring)) {
      result = false;
    } else {
      *left = start - 1;
      *length = end;
      *revcomp = false;
      debug(printf("(+) "));
      result = true;
    }

    /* Don't allow this yet ...
  } else if (index(copy,'-')) {
    startstring = strtok(copy,"-");
    endstring = strtok(NULL,"-");
    if (!isnumberp(&start,startstring) || !isnumberp(&end,endstring)) {
      result = false;
    } else if (end > start - 1) {
      result = false;
    } else {
      *left = start - 1 - end;
      *length = end;
      *revcomp = true;
      result = true;
    }
    */
    
  } else {
    result = false;
  }

  FREE(copy);
  return result;
}
  

/* Printing functions */

static char *
convert_to_chrpos (unsigned int *chrpos, char *genomesubdir, char *fileroot, unsigned int position) {
  char *copy, *chromosome, *filename;
  IIT_T chromosome_iit;
  
  filename = (char *) CALLOC(strlen(genomesubdir)+strlen("/")+strlen(fileroot)+
			     strlen(".chromosome.iit")+1,sizeof(char));
  sprintf(filename,"%s/%s.chromosome.iit",genomesubdir,fileroot);
  if ((chromosome_iit = IIT_read(filename,NULL,true)) == NULL) {
    fprintf(stderr,"Can't read IIT file %s\n",filename);
    exit(9);
  }
  FREE(filename);

  chromosome = Chrom_string_from_position(&(*chrpos),position,chromosome_iit);
  copy = (char *) CALLOC(strlen(chromosome)+1,sizeof(char));
  strcpy(copy,chromosome);
  IIT_free_mmapped(&chromosome_iit);

  return copy;
}

static void
print_two_coords (char *genomesubdir, char *fileroot, unsigned int left, unsigned int length) {
  char *chromosome;
  unsigned int chrpos;

  printf("%u%s%u\t",left+1,SEPARATOR,left+length);
  chromosome = convert_to_chrpos(&chrpos,genomesubdir,fileroot,left+1);
  printf("%s:%u\t",chromosome,chrpos);
  FREE(chromosome);
  chromosome = convert_to_chrpos(&chrpos,genomesubdir,fileroot,left+length);
  printf("%s:%u\n",chromosome,chrpos);
  FREE(chromosome);
  return;
}


/* Retrieval functions */

static int
translate_chromosomepos (unsigned int *genomicstart, unsigned int *genomiclength, 
			 char *chromosome, unsigned int left, unsigned int length,
			 IIT_T chromosome_iit) {
  int rc = 1, index;
  Interval_T interval;
  
  if ((index = IIT_find_linear(chromosome_iit,chromosome)) >= 0) {
    interval = IIT_interval(chromosome_iit,index);
    *genomicstart = Interval_low(interval)+left;
    if (length == 0) {
      *genomiclength = Interval_length(interval)-left;
    } else {
      *genomiclength = length;
    }
    rc = 0;
  }
  
  return rc;
}


static int
translate_contig (unsigned int *genomicstart, unsigned int *genomiclength,
		  char *contig, unsigned int left, unsigned int length, IIT_T contig_iit) {
  int rc = 1, index;
  Interval_T interval;
  
  if ((index = IIT_find_one(contig_iit,contig)) >= 0) {
    interval = IIT_interval(contig_iit,index);
    *genomicstart = Interval_low(interval)+left;
    if (length == 0) {
      *genomiclength = Interval_length(interval)-left;
    } else {
      *genomiclength = length;
    }
    rc = 0;
  }

  return rc;
}


static bool
parse_query (Genomicpos_T *genomicstart, Genomicpos_T *genomiclength, bool *revcomp,
	     char *query, char *genomesubdir, char *fileroot, char *version) {
  char *segment, *coords, *filename;
  Genomicpos_T result, left, length;
  IIT_T chromosome_iit, contig_iit;
  int rc;
  
  *revcomp = false;
  if (index(query,':')) {
    /* Segment must be a genome, chromosome, or contig */
    debug(printf("Parsed query %s into ",query));
    segment = strtok(query,":");
    coords = strtok(NULL,":");
    debug(printf("segment %s and coords %s\n",segment,coords));

    /* Try chromosome first */
    filename = (char *) CALLOC(strlen(genomesubdir)+strlen("/")+strlen(fileroot)+
			       strlen(".chromosome.iit")+1,sizeof(char));
    sprintf(filename,"%s/%s.chromosome.iit",genomesubdir,fileroot);
    chromosome_iit = IIT_read(filename,NULL,true);
    FREE(filename);

    debug(printf("Interpreting segment %s as a chromosome\n",segment));
    if (coords == NULL) {
      debug(printf("  entire chromosome\n"));
      rc = translate_chromosomepos(&(*genomicstart),&(*genomiclength),segment,left=0,length=0,chromosome_iit);
    } else if (isnumberp(&result,coords)) {
      debug(printf("  and coords %s as a number\n",coords));
      rc = translate_chromosomepos(&(*genomicstart),&(*genomiclength),segment,left=result-1,length=1,chromosome_iit);
    } else if (isrange(&left,&length,&(*revcomp),coords)) {
      debug(printf("  and coords %s as a range starting at %u with length %u and revcomp = %d\n",
		   coords,left,length,*revcomp));
      rc = translate_chromosomepos(&(*genomicstart),&(*genomiclength),segment,left,length,chromosome_iit);
    } else {
      debug(printf("  but coords %s is neither a number nor a range\n",coords));
      rc = -1;
    }

    IIT_free_mmapped(&chromosome_iit);

    if (rc != 0) {
      /* Try contig */
      filename = (char *) CALLOC(strlen(genomesubdir)+strlen("/")+strlen(fileroot)+
				 strlen(".contig.iit")+1,sizeof(char));
      sprintf(filename,"%s/%s.contig.iit",genomesubdir,fileroot);
      contig_iit = IIT_read(filename,NULL,true);
      FREE(filename);

      debug(printf("Interpreting segment %s as a contig\n",segment));
      if (isnumberp(&result,coords)) {
	debug(printf("  and coords %s as a number\n",coords));
	rc = translate_contig(&(*genomicstart),&(*genomiclength),segment,left=result-1,length=1,contig_iit);
      } else if (isrange(&left,&length,&(*revcomp),coords)) {
	debug(printf("  and coords %s as a range starting at %u with length %u and revcomp = %d\n",
		     coords,left,length,*revcomp));
	rc = translate_contig(&(*genomicstart),&(*genomiclength),segment,left,length,contig_iit);
      } else {
	debug(printf("  but coords %s is neither a number nor a range\n",coords));
	rc = -1;
      }

      IIT_free_mmapped(&contig_iit);
    }

    if (rc != 0) {
      fprintf(stderr,"Can't find coordinates %s:%s\n",segment,coords);
      return false;
    } else {
      return true;
    }

  } else {
    /* Query must be a genomic position, genomic range, or contig */
    debug(printf("Parsed query %s as atomic ",query));
    if (isnumberp(&result,query)) {
      debug(printf("number\n"));
      *genomicstart = result-1;
      *genomiclength = 1;
      return true;
    } else if (isrange(&left,&length,&(*revcomp),query)) {
      debug(printf("range\n"));
      *genomicstart = left;
      *genomiclength = length;
      return true;
    } else {
      filename = (char *) CALLOC(strlen(genomesubdir)+strlen("/")+strlen(fileroot)+
				 strlen(".contig.iit")+1,sizeof(char));
      sprintf(filename,"%s/%s.contig.iit",genomesubdir,fileroot);
      contig_iit = IIT_read(filename,NULL,true);
      FREE(filename);

      debug(printf("contig\n"));
      rc = translate_contig(&(*genomicstart),&(*genomiclength),query,left=0,length=0,contig_iit);

      IIT_free_mmapped(&contig_iit);
    }

    if (rc != 0) {
      fprintf(stderr,"Can't find coordinates %s\n",query);
      return false;
    } else {
      return true;
    }
  }
}


/* This code is duplicated in gmap.c */
static IIT_T global_altstrain_iit;

static int
index_compare (const void *a, const void *b) {
  int index1 = * (int *) a;
  int index2 = * (int *) b;
  int type1, type2;
  Genomicpos_T pos1, pos2;

  type1 = Interval_type(IIT_interval(global_altstrain_iit,index1));
  type2 = Interval_type(IIT_interval(global_altstrain_iit,index2));
  
  if (type1 < type2) {
    return -1;
  } else if (type1 > type2) {
    return +1;
  } else {
    /* Store in descending genomic position, so right shifting works
       in Genome_patch_strain */
    pos1 = Interval_low(IIT_interval(global_altstrain_iit,index1));
    pos2 = Interval_low(IIT_interval(global_altstrain_iit,index2));

    if (pos1 > pos2) {
      return -1;
    } else if (pos1 < pos2) {
      return +1;
    } else {
      return 0;
    }
  }
}



int
main (int argc, char *argv[]) {
  char *iitfile;
  Genomicpos_T genomicstart, genomiclength, sourcelength, maxextra, extra;
  char *genomesubdir = NULL, *dbversion = NULL, *olddbroot, *fileroot = NULL;
  char *gbuffer1, *gbuffer2, *gbuffer3;
  Sequence_T genomicseg;
  Genome_T genome;
  IIT_T chromosome_iit, contig_iit, altstrain_iit = NULL;
  Interval_T interval;
  char *strain;
  int user_type = -1, type, *indexarray, nindices, index, i, j;

  int opt;
  extern int optind;
  extern char *optarg;
  int long_option_index = 0;

  while ((opt = getopt_long(argc,argv,"D:d:R:Ss:CUXl:Gh:LIV?",
			    long_options,&long_option_index)) != -1) {
    switch (opt) {
    case 'D': user_genomedir = optarg; break;
    case 'd': 
      dbroot = (char *) CALLOC(strlen(optarg)+1,sizeof(char));
      strcpy(dbroot,optarg);
      break;
    case 'R': releasestring = optarg; break;

    case 'S': altstrainp = true; break;
    case 's': user_typestring = optarg; break;
    case 'C': coordp = true; break;
    case 'U': uppercasep = true; break;
    case 'X': replacex = true; break;
    case 'l': wraplength = atoi(optarg); break;
    case 'G': uncompressedp = true; break;
    case 'h': header = optarg; break;

    case 'L': dumpchrp = true; break;
    case 'I': dumpsegsp = true; break;

    case 'V': print_program_version(); exit(0);
    case '?': print_program_usage(); exit(0);
    default: exit(9);
    }
  }
  argc -= optind;
  argv += optind;
  
  if (dbroot == NULL) {
    print_program_usage();
    exit(9);
  } else if (releasestring != NULL) {
    olddbroot = dbroot;
    dbroot = (char *) CALLOC(strlen(dbroot)+strlen("_R")+strlen(releasestring)+1,sizeof(char));
    sprintf(dbroot,"%s_R%s",olddbroot,releasestring);
    FREE(olddbroot);
  }
  genomesubdir = Datadir_find_genomesubdir(&fileroot,&dbversion,user_genomedir,dbroot);

  if (dumpchrp == true) {
    iitfile = (char *) CALLOC(strlen(genomesubdir)+strlen("/")+
			      strlen(fileroot)+strlen(".chromosome.iit")+1,sizeof(char));
    sprintf(iitfile,"%s/%s.chromosome.iit",genomesubdir,fileroot);
    chromosome_iit = IIT_read(iitfile,NULL,true);
    FREE(iitfile);

    IIT_dump_formatted(chromosome_iit,/*directionalp*/false);
    IIT_free_mmapped(&chromosome_iit);
    return 0;

  } else if (dumpsegsp == true) {
    iitfile = (char *) CALLOC(strlen(genomesubdir)+strlen("/")+
			      strlen(fileroot)+strlen(".contig.iit")+1,sizeof(char));
    sprintf(iitfile,"%s/%s.contig.iit",genomesubdir,fileroot);
    contig_iit = IIT_read(iitfile,NULL,true);
    FREE(iitfile);

    IIT_dump_formatted(contig_iit,/*directionalp*/true);
    IIT_free_mmapped(&contig_iit);
    return 0;

  } else if (argc < 1) {
    print_program_usage();
    exit(9);

  }

  if (replacex == true) {
    Genome_replace_x();
  }

  if (parse_query(&genomicstart,&genomiclength,&revcomp,argv[0],genomesubdir,fileroot,dbversion) == true) {
    debug(printf("Query parsed as: genomicstart = %u, genomiclength = %u, revcomp = %d\n",
		 genomicstart,genomiclength,revcomp));

    if (coordp == true) {
      print_two_coords(genomesubdir,fileroot,genomicstart,genomiclength);
    } else {
      gbuffer1 = (char *) CALLOC(genomiclength+1,sizeof(char));
      gbuffer2 = (char *) CALLOC(genomiclength+1,sizeof(char));
      genome = Genome_new(genomesubdir,fileroot,uncompressedp,false);

      /* Handle reference strain */
      if (user_typestring != NULL) {
	/* Don't print a header */
      } else if (header != NULL) {
	printf(">%s\n",header);
      } else if (revcomp == true) {
	printf(">%s:%u%s%u_rc\n",dbversion,genomicstart+1,SEPARATOR,genomicstart+genomiclength);
      } else {
	printf(">%s:%u%s%u\n",dbversion,genomicstart+1,SEPARATOR,genomicstart+genomiclength);
      }

      genomicseg = Genome_get_segment(genome,genomicstart,genomiclength,revcomp,
				      gbuffer1,gbuffer2,genomiclength);

      if (user_typestring == NULL) {
	Sequence_print(genomicseg,uppercasep,wraplength,/*trimmedp*/false);
	Sequence_free(&genomicseg);
      }

      /* Handle alternate strains */
      if (altstrainp == true || user_typestring != NULL) {
	iitfile = (char *) CALLOC(strlen(genomesubdir)+strlen("/")+
				  strlen(fileroot)+strlen(".altstrain.iit")+1,sizeof(char));
	sprintf(iitfile,"%s/%s.altstrain.iit",genomesubdir,fileroot);
	global_altstrain_iit = altstrain_iit = IIT_read(iitfile,NULL,true);
	FREE(iitfile);
      } else {
	global_altstrain_iit = altstrain_iit = (IIT_T) NULL;
      }

      /* We rely upon the fact that gbuffer1 still holds the genomic segment.  This code is duplicated in gmap.c. */
      if (altstrain_iit != NULL) {
	if (user_typestring == NULL) {
	  /* No user-specified strain.  Get all indices. */
	  indexarray = IIT_get(&nindices,altstrain_iit,genomicstart+1,genomicstart+genomiclength-1);
	} else {
	  user_type = IIT_typeint(altstrain_iit,user_typestring);
	  if (user_type < 0) {
	    /* Invalid user-specified strain.  Print nothing. */
	    fprintf(stderr,"No such type as %s.  Allowed strains are:\n",user_typestring);
	    IIT_dump_typestrings(stderr,altstrain_iit);
	    indexarray = NULL;
	    nindices = 0;
	    Sequence_free(&genomicseg);
	  } else {
	    /* Valid user-specified strain.  Get subset of indices. */
	    indexarray = IIT_get_typed(&nindices,altstrain_iit,genomicstart+1,genomicstart+genomiclength-1,user_type);
	    if (nindices == 0) {
	      /* Print reference strain */
	      Sequence_print(genomicseg,uppercasep,wraplength,/*trimmedp*/false);
	      Sequence_free(&genomicseg);
	    }
	  }
	}

	if (nindices > 0) {
	  /* Sort according to type and genome position*/
	  qsort(indexarray,nindices,sizeof(int),index_compare);

	  for (j = 0; j < nindices; ) {
	    i = j++;
	    type = Interval_type(interval = IIT_interval(altstrain_iit,indexarray[i]));
	    strain = IIT_typestring(altstrain_iit,type);
	    sourcelength = IIT_annotation_strlen(altstrain_iit,indexarray[i]);
	    if (sourcelength > Interval_length(interval)) {
	      extra = sourcelength - Interval_length(interval);
	    } else {
	      extra = 0;
	    }
	    while (j < nindices && Interval_type(interval = IIT_interval(altstrain_iit,indexarray[j])) == type) {
	      sourcelength = IIT_annotation_strlen(altstrain_iit,indexarray[j]);
	      if (sourcelength > Interval_length(interval)) {
		extra += sourcelength - Interval_length(interval);
	      }
	      j++;
	    }
	    /* Patch from i to j */
	    gbuffer3 = (char *) CALLOC(genomiclength+extra+1,sizeof(char));
	    genomicseg = Genome_patch_strain(&(indexarray[i]),j-i,altstrain_iit,
					     genomicstart,genomiclength,
					     revcomp,gbuffer1,gbuffer2,gbuffer3,
					     genomiclength+extra);

	    if (header != NULL) {
	      printf(">%s\n",header);
	    } else if (revcomp == true) {
	      printf(">%s:%u%s%u_rc variant:%s\n",
		     dbversion,genomicstart+1,SEPARATOR,genomicstart+genomiclength,strain);
	    } else {
	      printf(">%s:%u%s%u variant:%s\n",
		     dbversion,genomicstart+1,SEPARATOR,genomicstart+genomiclength,strain);
	    }
	    Sequence_print(genomicseg,uppercasep,wraplength,/*trimmedp*/false);
	    Sequence_free(&genomicseg);
	    FREE(gbuffer3);
	  }
	  FREE(indexarray);
	}
	IIT_free_mmapped(&altstrain_iit);
      }
      Genome_free(&genome);
      FREE(gbuffer2);
      FREE(gbuffer1);
    }
  }
    
  FREE(dbversion);
  FREE(genomesubdir);
  FREE(fileroot);
  FREE(dbroot);

  return 0;
}
