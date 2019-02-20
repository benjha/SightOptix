/***************************************************************************
 *cr
 *cr            (C) Copyright 1995-2016 The Board of Trustees of the
 *cr                        University of Illinois
 *cr                         All Rights Reserved
 *cr
 ***************************************************************************/

/***************************************************************************
 * RCS INFORMATION:
 *
 *      $RCSfile: readpdb.h,v $
 *      $Author: johns $       $Locker:  $             $State: Exp $
 *      $Revision: 1.43 $       $Date: 2016/11/28 05:01:54 $
 *
 ***************************************************************************/

#ifndef READ_PDB_H
#define READ_PDB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PDB_RECORD_LENGTH   80   /* actual record size */
#define PDB_BUFFER_LENGTH   83   /* size need to buffer + CR, LF, and NUL */

#define VMDUSECONECTRECORDS 1

/*  record type defines */
enum {
  PDB_HEADER, PDB_REMARK, PDB_ATOM, PDB_CONECT, PDB_UNKNOWN, PDB_END, PDB_EOF, PDB_CRYST1
};

/* read the next record from the specified pdb file, and put the string found
   in the given string pointer (the caller must provide adequate (81 chars)
   buffer space); return the type of record found
*/
static int read_pdb_record(FILE *f, char *retStr) {
  int ch;
  char inbuf[PDB_BUFFER_LENGTH]; /* space for line + cr + lf + NUL */
  int recType = PDB_UNKNOWN;
 
  /* XXX This PDB record reading code breaks with files that use
   * Mac or DOS style line breaks with ctrl-M characters.  We need
   * to replace the use of fgets() and comparisons against \n with
   * code that properly handles the other cases.
   */
 
  /* read the next line, including any ending cr/lf char */
  if (inbuf != fgets(inbuf, PDB_RECORD_LENGTH + 2, f)) {
    retStr[0] = '\0';
    recType = PDB_EOF;
  } else {
#if 0
    /* XXX disabled this code since \n chars are desirable in remarks */
    /* and to make the behavior consistent with webpdbplugin          */

    /* remove the newline character, if there is one */
    if (inbuf[strlen(inbuf)-1] == '\n')
      inbuf[strlen(inbuf)-1] = '\0';
#endif

    /* atom records are the most common */
    if (!strncmp(inbuf, "ATOM ",  5) || !strncmp(inbuf, "HETATM", 6)) {
      /* Note that by only comparing 5 chars for "ATOM " rather than 6,     */
      /* we allow PDB files containing > 99,999 atoms generated by AMBER    */
      /* to load which would otherwise fail.  Not needed for HETATM since   */
      /* those aren't going to show up in files produced for/by MD engines. */
      recType = PDB_ATOM;
    } else if (!strncmp(inbuf, "CONECT", 6)) {
      recType = PDB_CONECT;
    } else if (!strncmp(inbuf, "REMARK", 6)) {
      recType = PDB_REMARK;
    } else if (!strncmp(inbuf, "CRYST1", 6)) {
      recType = PDB_CRYST1;
    } else if (!strncmp(inbuf, "HEADER", 6)) {
      recType = PDB_HEADER;
    } else if (!strncmp(inbuf, "END", 3)) {  /* very permissive */
      /* XXX we treat any "ENDxxx" record as an end, to simplify testing */
      /*     since we don't remove trailing '\n' chars                   */

      /* the only two legal END records are "END   " and "ENDMDL" */
      recType = PDB_END;
    } 

#if 0
    /* XXX disable record type checking for now */
    if (recType == PDB_ATOM || 
        recType == PDB_CONECT || 
        recType == PDB_REMARK || 
        recType == PDB_HEADER || 
        recType == PDB_CRYST1) {
      strcpy(retStr, inbuf);
    } else {
      retStr[0] = '\0';
    }
#else
    strcpy(retStr, inbuf);
#endif
  }

  /* read the '\r', if there was one */
  ch = fgetc(f);
  if (ch != '\r')
    ungetc(ch, f);
  
  return recType;
}


/* Extract the alpha/beta/gamma a/b/c unit cell info from a CRYST1 record */
static void get_pdb_cryst1(const char *record, 
                           float *alpha, float *beta, float *gamma, 
                           float *a, float *b, float *c) {
  char tmp[PDB_RECORD_LENGTH+3]; /* space for line + cr + lf + NUL */
  char ch, *s;
  memset(tmp, 0, sizeof(tmp));
  strncpy(tmp, record, PDB_RECORD_LENGTH);

  s = tmp+6 ;          ch = tmp[15]; tmp[15] = 0;
  *a = (float) atof(s);
  s = tmp+15; *s = ch; ch = tmp[24]; tmp[24] = 0;
  *b = (float) atof(s);
  s = tmp+24; *s = ch; ch = tmp[33]; tmp[33] = 0;
  *c = (float) atof(s);
  s = tmp+33; *s = ch; ch = tmp[40]; tmp[40] = 0;
  *alpha = (float) atof(s);
  s = tmp+40; *s = ch; ch = tmp[47]; tmp[47] = 0;
  *beta = (float) atof(s);
  s = tmp+47; *s = ch; ch = tmp[54]; tmp[54] = 0;
  *gamma = (float) atof(s);
}


/* Extract the x,y,z coords, occupancy, and beta from an ATOM record */
static void get_pdb_coordinates(const char *record, 
                                float *x, float *y, float *z,
                                float *occup, float *beta) {
  char numstr[50]; /* store all fields in one array to save memset calls */
  memset(numstr, 0, sizeof(numstr));

  if (x != NULL) {
    strncpy(numstr, record + 30, 8);
    *x = (float) atof(numstr);
  }

  if (y != NULL) {
    strncpy(numstr+10, record + 38, 8);
    *y = (float) atof(numstr+10);
  }

  if (z != NULL) {
    strncpy(numstr+20, record + 46, 8);
    *z = (float) atof(numstr+20);
  }

  if (occup != NULL) {
    strncpy(numstr+30, record + 54, 6);
    *occup = (float) atof(numstr+30);
  }

  if (beta != NULL) {
    strncpy(numstr+40, record + 60, 6);
    *beta = (float) atof(numstr+40);
  }
}


/* remove leading and trailing spaces from PDB fields */
static void adjust_pdb_field_string(char *field) {
  int i, len;

  len = strlen(field);
  while (len > 0 && field[len-1] == ' ') {
    field[len-1] = '\0';
    len--;
  }

  while (len > 0 && field[0] == ' ') {
    for (i=0; i < len; i++)
      field[i] = field[i+1];
    len--;
  }
}

static void get_pdb_header(const char *record, char *pdbcode, char *date,
                           char *classification) {
  if (date != NULL) {
    strncpy(date, record + 50, 9);
    date[9] = '\0';
  }

  if (classification != NULL) {
    strncpy(classification, record + 10, 40);
    classification[40] = '\0';
  }

  if (pdbcode != NULL) {
    strncpy(pdbcode, record + 62, 4);
    pdbcode[4] = '\0';
    adjust_pdb_field_string(pdbcode); /* remove spaces from accession code */
  }
}


static void get_pdb_conect(const char *record, int natoms, int *idxmap,
                           int *maxbnum, int *nbonds, int **from, int **to) {
  int bondto[11], numbonds, i;

  int reclen = strlen(record);
  for (numbonds=0, i=0; i<11; i++) {
    char bondstr[6];
    const int fieldwidth = 5;
    int start = 6 + i*fieldwidth;
    int end = start + fieldwidth;

    if (end >= reclen)
      break;

    memcpy(bondstr, record + start, fieldwidth);
    bondstr[5] = '\0';
    if (sscanf(bondstr, "%d", &bondto[numbonds]) < 0)
      break;
    numbonds++; 
  }

  for (i=0; i<numbonds; i++) {
    /* only add one bond per pair, PDBs list them redundantly */ 
    if (bondto[i] > bondto[0]) {
      int newnbonds = *nbonds + 1; /* add a new bond */

      /* allocate more bondlist space if necessary */
      if (newnbonds >= *maxbnum) {
        int newmax;
        int *newfromlist, *newtolist;
        newmax = (newnbonds + 11) * 1.25;

        newfromlist = (int *) realloc(*from, newmax * sizeof(int));
        newtolist = (int *) realloc(*to, newmax * sizeof(int));

        if (newfromlist != NULL || newtolist != NULL) {
          *maxbnum = newmax;
          *from = newfromlist;
          *to = newtolist;
        } else {
          printf("readpdb) failed to allocate memory for bondlists\n");
          return; /* abort */
        }
      }

      *nbonds = newnbonds;
      (*from)[newnbonds-1] = idxmap[bondto[0]] + 1;
      (*to)[newnbonds-1] = idxmap[bondto[i]] + 1;
    }
  }
}

/* ATOM field format according to PDB standard v2.2
  COLUMNS        DATA TYPE       FIELD         DEFINITION
---------------------------------------------------------------------------------
 1 -  6        Record name     "ATOM  "
 7 - 11        Integer         serial        Atom serial number.
13 - 16        Atom            name          Atom name.
17             Character       altLoc        Alternate location indicator.
18 - 20        Residue name    resName       Residue name.
22             Character       chainID       Chain identifier.
23 - 26        Integer         resSeq        Residue sequence number.
27             AChar           iCode         Code for insertion of residues.
31 - 38        Real(8.3)       x             Orthogonal coordinates for X in Angstroms.
39 - 46        Real(8.3)       y             Orthogonal coordinates for Y in Angstroms.
47 - 54        Real(8.3)       z             Orthogonal coordinates for Z in Angstroms.
55 - 60        Real(6.2)       occupancy     Occupancy.
61 - 66        Real(6.2)       tempFactor    Temperature factor.
73 - 76        LString(4)      segID         Segment identifier, left-justified.
77 - 78        LString(2)      element       Element symbol, right-justified.
79 - 80        LString(2)      charge        Charge on the atom.
 */

/* Break a pdb ATOM record into its fields.  The user must provide the
   necessary space to store the atom name, residue name, and segment name.
   Character strings will be null-terminated.
*/
static void get_pdb_fields(const char *record, int reclength, int *serial,
                           char *name, char *resname, char *chain, 
                           char *segname, char *resid, char *insertion, 
                           char *altloc, char *elementsymbol,
                           float *x, float *y, float *z, 
                           float *occup, float *beta) {
  char serialbuf[6];

  /* get atom serial number */
  strncpy(serialbuf, record + 6, 5);
  serialbuf[5] = '\0';
  *serial = 0;
  sscanf(serialbuf, "%5d", serial);
  
  /* get atom name */
  strncpy(name, record + 12, 4);
  name[4] = '\0';
  adjust_pdb_field_string(name); /* remove spaces from the name */

  /* get alternate location identifier */
  strncpy(altloc, record + 16, 1);
  altloc[1] = '\0';

  /* get residue name */
  strncpy(resname, record + 17, 4);
  resname[4] = '\0';
  adjust_pdb_field_string(resname); /* remove spaces from the resname */

  /* get chain name */
  chain[0] = record[21];
  chain[1] = '\0';

  /* get residue id number */
  strncpy(resid, record + 22, 4);
  resid[4] = '\0';
  adjust_pdb_field_string(resid); /* remove spaces from the resid */

  /* get the insertion code */
  insertion[0] = record[26];
  insertion[1] = '\0';

  /* get x, y, and z coordinates */
  get_pdb_coordinates(record, x, y, z, occup, beta);

  /* get segment name */
  if (reclength >= 73) {
    strncpy(segname, record + 72, 4);
    segname[4] = '\0';
    adjust_pdb_field_string(segname); /* remove spaces from the segname */
  } else {
    segname[0] = '\0';
  }

  /* get the atomic element symbol */
  if (reclength >= 77) {
    strncpy(elementsymbol, record + 76, 2);
    elementsymbol[2] = '\0';
  } else {
    elementsymbol[0] = '\0';
  }
}  


/* Write PDB data to given file descriptor; return success. */
static int write_raw_pdb_record(FILE *fd, const char *recordname,
    int index,const char *atomname, const char *resname,int resid, 
    const char *insertion, const char *altloc, const char *elementsymbol,
    float x, float y, float z, float occ, float beta, 
    const char *chain, const char *segname) {
  int rc;
  char indexbuf[32];
  char residbuf[32];
  char segnamebuf[5];
  char resnamebuf[5];
  char altlocchar;

  /* XXX                                                          */
  /* if the atom or residue indices exceed the legal PDB spec, we */
  /* start emitting asterisks or hexadecimal strings rather than  */
  /* aborting.  This is not really legal, but is an accepted hack */
  /* among various other programs that deal with large PDB files  */
  /* If we run out of hexadecimal indices, then we just print     */
  /* asterisks.                                                   */
  if (index < 100000) {
    sprintf(indexbuf, "%5d", index);
  } else if (index < 1048576) {
    sprintf(indexbuf, "%05x", index);
  } else {
    sprintf(indexbuf, "*****");
  }

  if (resid < 10000) {
    sprintf(residbuf, "%4d", resid);
  } else if (resid < 65536) {
    sprintf(residbuf, "%04x", resid);
  } else { 
    sprintf(residbuf, "****");
  }

  altlocchar = altloc[0];
  if (altlocchar == '\0') {
    altlocchar = ' ';
  }

  /* make sure the segname or resname do not overflow the format */ 
  strncpy(segnamebuf,segname,4);
  segnamebuf[4] = '\0';
  strncpy(resnamebuf,resname,4);
  resnamebuf[4] = '\0';

 
  rc = fprintf(fd,
         "%-6s%5s %4s%c%-4s%c%4s%c   %8.3f%8.3f%8.3f%6.2f%6.2f      %-4s%2s\n",
         recordname, indexbuf, atomname, altlocchar, resnamebuf, chain[0], 
         residbuf, insertion[0], x, y, z, occ, beta, segnamebuf, elementsymbol);

  return (rc > 0);
}


#endif
