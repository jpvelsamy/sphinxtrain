/* ====================================================================
 * Copyright (c) 1997-2000 Carnegie Mellon University.  All rights 
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */
/*********************************************************************
 *
 * File: mk_model_def.c
 * 
 * Description: 
 *    Make SPHINX-III model definition files from a variety of
 *    input sources.  One input source is a SPHINX-II senone mapping
 *    file and triphone file.  Another is a list of phones (in which
 *    case the transition matrices are tied within base phone class
 *    and the states are untied).
 *
 * Author: 
 *    Eric Thayer (eht@cs.cmu.edu) 
 *********************************************************************/

#include "parse_cmd_ln.h"

#include <s3/acmod_set.h>
#include <s3/model_def_io.h>
#include <s3/s2_param.h>
#include <s3/ckd_alloc.h>
#include <s3/s2_read_map.h>

#include <s3/cmd_ln.h>
#include <s3/err.h>
#include <s3/s3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <time.h>

int32  n_state_pm;

int
output_model_def(FILE *fp,
		 char *pgm,
		 char **base_str,
		 char **left_str,
		 char **right_str,
		 char **posn_str,
		 uint32 *tmat,
		 uint32 **state,
		 uint32 n_base,
		 uint32 n_tri,
		 uint32 n_total,
		 uint32 n_tied_state,
		 uint32 n_tied_ci_state,
		 uint32 n_tied_tmat)
{
    time_t t;
    uint32 i, j;
    char *at;

    t = time(NULL);
    at = ctime((const time_t *)&t);
    at[strlen(at)-1] = '\0';

    fprintf(fp, "# Generated by %s on %s\n", pgm, at);
    fprintf(fp, "# triphone: %s\n",
	    (const char *)cmd_ln_access("-triphonefn"));
    fprintf(fp, "# seno map: %s\n#\n",
	    (const char *)cmd_ln_access("-mapfn"));

    fprintf(fp, "%s\n", MODEL_DEF_VERSION);
    fprintf(fp, "%u n_base\n", n_base);
    fprintf(fp, "%u n_tri\n", n_tri);
    fprintf(fp, "%u n_state_map\n", (n_base + n_tri) * n_state_pm);
    fprintf(fp, "%u n_tied_state\n", n_tied_state);
    fprintf(fp, "%u n_tied_ci_state\n", n_tied_ci_state);
    fprintf(fp, "%u n_tied_tmat\n", n_tied_tmat);
      
    fprintf(fp, "#\n# Columns definitions\n");
    fprintf(fp, "#%4s %3s %3s %1s %6s %4s %s\n",
	    "base", "lft", "rt", "p", "attrib", "tmat",
	    "     ... state id's ...");
    for (i = 0; i < n_total; i++) {
	if ((base_str[i][0] == '+') ||
	    (strncmp(base_str[i], "SIL", 3) == 0)) {
	    fprintf(fp, "%5s %3s %3s %1s %6s %4d",
		    base_str[i],
		    (left_str[i] != NULL ? left_str[i] : "-"),
		    (right_str[i] != NULL ? right_str[i] : "-"),
		    (posn_str[i] != NULL ? posn_str[i] : "-"),
		    "filler",
		    tmat[i]);
	}
	else {
	    fprintf(fp, "%5s %3s %3s %1s %6s %4d",
		    base_str[i],
		    (left_str[i] != NULL ? left_str[i] : "-"),
		    (right_str[i] != NULL ? right_str[i] : "-"),
		    (posn_str[i] != NULL ? posn_str[i] : "-"),
		    "n/a",
		    tmat[i]);
	}
	    

	for (j = 0; j < n_state_pm-1; j++) {
	    fprintf(fp, " %4u", state[i][j]);
	}
	fprintf(fp, "    N\n");
    }
    return 0;
}

int
main(int argc, char *argv[])
{
    FILE *fp;
    char big_str[4096];
    uint32 i, j, k, ll;
    char **phone_list;
    char **base_str=0;
    char **left_str=0;
    char **right_str=0;
    char **posn_str=0;
    uint32 n_base=0;
    uint32 n_tri=0;
    uint32 n_total=0;
    acmod_id_t base;
    acmod_id_t left;
    acmod_id_t right;
    word_posn_t posn;
    char *tok;
    acmod_set_t *acmod_set;
    char *pmap = WORD_POSN_CHAR_MAP;
    uint32 *tmat=0;
    uint32 **smap=0;
    uint32 n_base_state;
    uint32 *cluster_size = NULL;
    uint32 *state_of = NULL;
    const char *filler_attr[] = {"filler", NULL};
    const char *base_attr[] = {"base", NULL};
    const char *na_attr[] = {"n/a", NULL};
    uint32 n_tied_state_cd=0;
    uint32 n_tied_state_ci=0;
    uint32 n_tied_tmat=0;

    parse_cmd_ln(argc, argv);

    n_state_pm = *(int32 *)cmd_ln_access("-n_state_pm") + 1;

    if (cmd_ln_access("-triphonefn")) {
	fp = fopen(cmd_ln_access("-triphonefn"), "r");
	if (fp == NULL) {
	    perror(cmd_ln_access("-triphonefn"));
	    exit(1);
	}

	for (n_total = 0; fgets(big_str, 4096, fp); n_total++);

	rewind(fp);

	phone_list = ckd_calloc(n_total, sizeof(char *));

	base_str = ckd_calloc(n_total, sizeof(char *));
	left_str = ckd_calloc(n_total, sizeof(char *));
	right_str = ckd_calloc(n_total, sizeof(char *));
	posn_str = ckd_calloc(n_total, sizeof(char *));
    
	if (cmd_ln_access("-n_tmat")) {
	    n_tied_tmat = *(int32 *)cmd_ln_access("-n_tmat");
	}
	else {
	    E_FATAL("Please specify an -n_tmat argument\n");
	}
	
	if (cmd_ln_access("-n_cistate")) {
	    n_tied_state_ci = *(int32 *)cmd_ln_access("-n_cistate");
	}
	else {
	    E_FATAL("Please specify an -n_cistate argument\n");
	}

	if (cmd_ln_access("-n_cdstate")) {
	    n_tied_state_cd = *(int32 *)cmd_ln_access("-n_cdstate");
	}
	else {
	    E_FATAL("Please specify an -n_cdstate argument\n");
	}

	tmat = ckd_calloc(n_total, sizeof(uint32));

	for (i = 0; fgets(big_str, 4096, fp); i++)
	    phone_list[i] = strdup(strtok(big_str, " \t"));

	fclose(fp);

	for (n_tri = n_base = i = 0; i < n_total; i++) {
	    if (!strchr(phone_list[i], '('))
		++n_base;
	    else
		++n_tri;
	}

	E_INFO("%d n_base, %d n_tri\n", n_base, n_tri);

	acmod_set = acmod_set_new();

	acmod_set_set_n_ci_hint(acmod_set, n_base);
	acmod_set_set_n_tri_hint(acmod_set, n_tri);

	for (i = 0, n_base_state = 0; i < n_total; i++, n_base_state += n_state_pm-1) {

	    /* set up the context independent entries */
	    if (!strchr(phone_list[i], '(')) {
		if (phone_list[i][0] != '+') {
		    base = acmod_set_add_ci(acmod_set,
					    phone_list[i],
					    base_attr);
		}
		else {
		    base = acmod_set_add_ci(acmod_set,
					    phone_list[i],
					    filler_attr);
		}
	    
		base_str[i] = strdup(phone_list[i]);
		left_str[i] = NULL;
		right_str[i] = NULL;
		posn_str[i] = NULL;

		tmat[base] = base;
	    }
	    else
		break;
	}

	for (;i < n_total; i++) {
	    if (strchr(phone_list[i], '(')) {
		tok = strtok(phone_list[i], "(,)");

		base = acmod_set_name2id(acmod_set, tok);
		assert(base != NO_ACMOD);
		base_str[i] = strdup(tok);

		tok = strtok(NULL, "(,)");
		left = acmod_set_name2id(acmod_set, tok);
		assert(left != NO_ACMOD);
		left_str[i] = strdup(tok);

		tok = strtok(NULL, "(,)");
		right = acmod_set_name2id(acmod_set, tok);
		assert(right != NO_ACMOD);
		right_str[i] = strdup(tok);

		tok = strtok(NULL, "(,)");
		if (tok) {
		    for (j = 0; j < strlen(pmap); j++) {
			if (tok[0] == pmap[j])
			    break;
		    }
		}
		else {
		    j = (uint32)WORD_POSN_INTERNAL;
		}

		posn_str[i] = ckd_calloc(2, sizeof(char));
		if (j < strlen(pmap)) {
		    posn = (word_posn_t)j;

		    posn_str[i][0] = pmap[j];
		}
		else {
		    E_WARN("unknown word position %s; using 'i'\n",
			   tok);

		    posn = WORD_POSN_INTERNAL;
		}

		acmod_set_add_tri(acmod_set, base, left, right, posn, na_attr);

		tmat[i] = base;

		/* state assignments need to wait until we read the seno file */
	    }
	    else {
		E_FATAL("expected triphone but saw %s\n",
			phone_list[i]);
	    }
	}

	for (i = 0; i < n_total; i++) {
	    free(phone_list[i]);
	}
	free(phone_list);

	smap = (uint32 **)ckd_calloc_2d(acmod_set_n_acmod(acmod_set),
					n_state_pm-1,
					sizeof(uint32));

	cluster_size = ckd_calloc(acmod_set_n_ci(acmod_set),
				  sizeof(uint32));

	if (s2_read_seno_mapping_file(smap, cluster_size,
				      cmd_ln_access("-mapfn"),
				      acmod_set) != S3_SUCCESS)
	    exit(1);
    
	s2_convert_smap_to_global(acmod_set,
				  smap,
				  &state_of,
				  cluster_size);
    }
    else if (cmd_ln_access("-phonelstfn")) {
	fp = fopen(cmd_ln_access("-phonelstfn"), "r");
	if (fp == NULL) {
	    E_FATAL_SYSTEM("Can't open phone list file %u:", 
			   cmd_ln_access("-phonelstfn"));
	}
	
	for (n_total = 0; fgets(big_str, 4096, fp); n_total++);

	rewind(fp);

	base_str = ckd_calloc(n_total, sizeof(char *));
	left_str = ckd_calloc(n_total, sizeof(char *));
	right_str = ckd_calloc(n_total, sizeof(char *));
	posn_str = ckd_calloc(n_total, sizeof(char *));

	tmat = ckd_calloc(n_total, sizeof(uint32));

        E_INFO("%d n-phones, each to have %d states\n", n_total,n_state_pm-1);
	smap = (uint32 **)ckd_calloc_2d(n_total, n_state_pm-1, sizeof(uint32));

	for (i = 0, n_base = 0, ll = 0, n_tri = 0; i < n_total; i++) {
	    char tmp[64];

	    if (fscanf(fp, "%s", tmp) != 1) {
		E_FATAL("Error reading triphone list file\n");
	    }
            if (base_str[ll] != NULL) ckd_free(base_str[ll]);
	    base_str[ll] = strdup(tmp);

	    if (fscanf(fp, "%s", tmp) != 1) {
		E_FATAL("Error reading triphone list file\n");
	    }
            if (left_str[ll] != NULL) ckd_free(left_str[ll]);
	    left_str[ll] = strdup(tmp);

	    if (fscanf(fp, "%s", tmp) != 1) {
		E_FATAL("Error reading triphone list file\n");
	    }
            if (right_str[ll] != NULL) ckd_free(right_str[ll]);
	    right_str[ll] = strdup(tmp);

	    if (fscanf(fp, "%s", tmp) != 1) {
		E_FATAL("Error reading triphone list file\n");
	    }
            if (posn_str[ll] != NULL) ckd_free(posn_str[ll]);
	    posn_str[ll] = strdup(tmp);

	    if ((strcmp(left_str[ll], "-") == 0) &&
		(strcmp(right_str[ll], "-") == 0)) {
		n_base++; ll++;
	    }
	    else if ((strcmp(left_str[ll], "-") != 0) &&
		     (strcmp(right_str[ll], "-") != 0)) {
                        if (base_str[ll][0] != '+' &&
                                         strncmp(base_str[ll],"SIL",3) != 0) {
	 	            n_tri++; ll++; 
                        }
	    }
	    else {
		E_FATAL("Unhandled phone %s %s %s %s\n",
		      base_str[ll], left_str[ll], right_str[ll], posn_str[ll]);
	    }
	}
	fclose(fp);

        assert(ll == n_base + n_tri);
        n_total = ll; 
	for (i = 0, k = 0; i < n_total; i++)
	    for (j = 0; j < n_state_pm-1; j++)
		smap[i][j] = k++;

	n_tied_state_ci = n_base * (n_state_pm-1);
	n_tied_state_cd = n_tri * (n_state_pm-1);
	n_tied_tmat = n_base;
    
	E_INFO("%d n_base, %d n_tri\n", n_base, n_tri);

	acmod_set = acmod_set_new();

	acmod_set_set_n_ci_hint(acmod_set, n_base);
	acmod_set_set_n_tri_hint(acmod_set, n_tri);

	for (i = 0; i < n_base; i++) {
	    if (base_str[i][0] == '+') {
		base = acmod_set_add_ci(acmod_set, base_str[i], filler_attr);
	    }
	    else {
		base = acmod_set_add_ci(acmod_set, base_str[i], base_attr);
	    }

	    free(left_str[i]);
	    left_str[i] = NULL;
	    free(right_str[i]);
	    right_str[i] = NULL;
	    free(posn_str[i]);
	    posn_str[i] = NULL;

	    tmat[base] = base;
	}

	for (; i < n_total; i++) {
	    base = acmod_set_name2id(acmod_set, base_str[i]);
	    left = acmod_set_name2id(acmod_set, left_str[i]);
	    right = acmod_set_name2id(acmod_set, right_str[i]);

	    for (j = 0; j < strlen(pmap); j++) {
		if (posn_str[i][0] == pmap[j])
		    break;
	    }
	    if (j < strlen(pmap)) {
		posn = (word_posn_t)j;
	    }
	    else {
		if (posn_str[i][0] == '-') {
		    E_FATAL("CI phone %s in the CD phone list.\n", base_str[i]);
		}
		else {
		    E_WARN("unknown word position %s; using 'i'\n",
			   posn_str[i]);
		}
		
		posn = WORD_POSN_INTERNAL;
	    }

	    acmod_set_add_tri(acmod_set, base, left, right, posn, na_attr);

	    tmat[i] = base;
	}
    }

    fp = fopen(cmd_ln_access("-moddeffn"), "w");
    if (fp == NULL) {
	perror(cmd_ln_access("-moddeffn"));
	exit(1);
    }

    output_model_def(fp,
		     argv[0],
		     base_str, left_str, right_str, posn_str, tmat, smap,
		     n_base, n_tri, n_total,
		     n_tied_state_ci+n_tied_state_cd, n_tied_state_ci, n_tied_tmat);

    fclose(fp);

    if (state_of)
	ckd_free(state_of);

    if (cluster_size)
	ckd_free(cluster_size);
    
    ckd_free_2d((void **)smap);

    return 0;
}

/*
 * Log record.  Maintained by RCS.
 *
 * $Log$
 * Revision 1.4  2004/07/21  18:30:36  egouvea
 * Changed the license terms to make it the same as sphinx2 and sphinx3.
 * 
 * Revision 1.3  2001/04/05 20:02:31  awb
 * *** empty log message ***
 *
 * Revision 1.2  2000/09/29 22:35:14  awb
 * *** empty log message ***
 *
 * Revision 1.1  2000/09/24 21:38:31  awb
 * *** empty log message ***
 *
 * Revision 1.7  97/07/16  11:34:32  eht
 * - Add the ability to create an untied state model definition
 *   file from a list of phones
 * - Use updated library functions
 * 
 *
 */
