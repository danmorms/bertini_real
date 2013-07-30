#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include <time.h>
#include <float.h>
#include <limits.h>

#include <gmp.h>
#include <mpfr.h>
#include <mpf2mpfr.h>

#include <sys/stat.h>         /* declare the 'stat' structure */
#include <sys/types.h>


#include <dirent.h>

#include <string>


#ifndef _FILEOPS_H
#define _FILEOPS_H


extern "C" {
#include "polysolve.h"
}

extern "C" {
#include "partitionParse.h"
}


#include "data_type.hpp"
#include "missing_bertini_headers.hpp"
#include <boost/filesystem/path.hpp>


int partition_parse(int **declarations,
										boost::filesystem::path input_filename,
										boost::filesystem::path functions_filename,
										boost::filesystem::path config_filename,
										int not_sc_flag);


/**
 renames the arr.out deg.out num.out config and preproc_data files to *.bak.  these files restored by restore_bertini_files_dotbak().
 */
void rename_bertini_files_dotbak();


/**
 restores the .bak files which were renamed to *.bak by move_bertini_files_dotbak
 */
void restore_bertini_files_dotbak();

/**
 purges an entire directory of all files except those which start with a period (.)
 */
void purge_previous_directory(char *directoryName);

/**
 opens a file to read, giving fatal error if cannot.
 */
FILE *safe_fopen_read(boost::filesystem::path filename);

/**
 opens a file to write, giving fatal error if cannot.
 */
FILE *safe_fopen_write(boost::filesystem::path filename);

/**
 opens a file to append, giving fatal error if cannot.
 */
FILE *safe_fopen_append(boost::filesystem::path filename);

/**
 copies a file character by character.
 */


void copyfile(boost::filesystem::path input_file, boost::filesystem::path OUTfile);

void copyfile(FILE *IN,FILE *OUT);

void read_matrix(boost::filesystem::path INfile, mat_mp matrix);




#endif
