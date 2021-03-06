/*
 * Copyright (c) 2015 The Jackson Laboratory
 *
 * This software was developed by Gary Churchill's Lab at The Jackson
 * Laboratory (see http://research.jax.org/faculty/churchill).
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software. If not, see <http://www.gnu.org/licenses/>.
 */

#include <string>
#include <fstream>
#include <iostream>

#include "alignment_incidence_matrix.h"


/* This function will read in a binary file produced by Matt Vincent's
   Kallisto exporter (https://github.com/churchill-lab/kallisto-export) and
   create an AlignmentIncidenceMatrix instance.  A pointer to the new aim
   object will be returned.  If there is a problem loading the file, NULL will
   be returned.

   This code assumes that the reads/equivalence classes are stored in order.
 */
AlignmentIncidenceMatrix *loadFromBin(std::string filename)
{
    AlignmentIncidenceMatrix *aim = NULL;

    // the following vectors will hold the data we read in from the file,
    // the values, col_ind, and row_ptr vectors are the compressed row format
    // sparse matrix representation of the alignments.  counts are used for
    // equivalence class data, it is not used for read data
    std::vector<std::string> haplotypes;
    std::vector<std::string> reads;
    std::vector<std::string> transcripts;
    std::vector<int> values;
    std::vector<int> col_ind;
    std::vector<int> row_ptr;
    std::vector<int> counts;

    std::ifstream infile(filename, std::ios::binary);

    int version;
    int num_transcripts;
    int num_haplotypes;
    int num_reads;
    int num_alignments;

    int size;

    std::vector<char> buffer;

    if (!infile.is_open()) {
        // something went wrong reading from stream for now return NULL
        std::cerr << "ERROR LOADING FILE " << filename << std::endl;
        return NULL;
    }

    infile.read((char*)&version, sizeof(int));


    if (version == 0 || version == 1) {

        //load list of transcript names
        infile.read((char*)&num_transcripts, sizeof(int));
        transcripts.reserve(num_transcripts);

        for (int i = 0; i < num_transcripts; ++i) {
            infile.read((char*)&size, sizeof(int));
            buffer.clear();

            for (int j = 0; j < size; ++j) {
                char c;
                infile.read(&c, sizeof(c));
                buffer.push_back(c);
            }
            buffer.push_back('\0');
            transcripts.push_back(std::string(buffer.data()));
        }

        //load list of haplotype names
        infile.read((char*)&num_haplotypes, sizeof(int));
        haplotypes.reserve(num_haplotypes);

        for (int i = 0; i < num_haplotypes; ++i) {
            infile.read((char*)&size, sizeof(int));
            buffer.clear();

            for (int j = 0; j < size; ++j) {
                char c;
                infile.read(&c, sizeof(c));
                buffer.push_back(c);
            }
            buffer.push_back('\0');
            haplotypes.push_back(std::string(buffer.data()));
        }


        if (version == 0) {
            // load alignments, use default counts of 1 per alignment
            // format is "read_id transcript_id value"

            int read_id;
            int transcript_id;
            int value;


            // load list of read names
            infile.read((char*)&num_reads, sizeof(int));
            reads.reserve(num_reads);

            for (int i = 0; i < num_reads; i++) {
                infile.read((char*)&size, sizeof(int));
                buffer.clear();

                for (int j = 0; j < size; j++) {
                    char c;
                    infile.read(&c, sizeof(c));
                    buffer.push_back(c);
                }
                buffer.push_back('\0');
                reads.push_back(std::string(buffer.data()));
            }

            infile.read((char*)&num_alignments, sizeof(int));

            values.reserve(num_alignments);
            col_ind.reserve(num_alignments);

            // first read values start at index 0
            row_ptr.push_back(0);
            int last_read = 0;

            for (int i = 0; i < num_alignments; ++i) {
                infile.read((char*)&read_id, sizeof(int));
                infile.read((char*)&transcript_id, sizeof(int));
                infile.read((char*)&value, sizeof(int));

                // sanity check that read_id is not less than last_read
                if (read_id < last_read) {
                    // this is a problem with the file
                    std::cerr << "ERROR: binary input file must be sorted\n";
                    return NULL;
                }

                values.push_back(value);
                col_ind.push_back(transcript_id);

                if (read_id != last_read) {
                    // we've just transitioned to a new read, so we need to
                    // record the starting index in row_ptr;
                    row_ptr.push_back(i);
                    last_read = read_id;
                }
            }
            row_ptr.push_back(num_alignments);
            aim = new AlignmentIncidenceMatrix(haplotypes, reads, transcripts, col_ind, row_ptr, values);
        }
        else {
            // load equivalence class, also includes counts

            int equivalence_id;
            int transcript_id;
            int value;
            int num_classes;

            infile.read((char*)&num_classes, sizeof(int));
            counts.resize(num_classes);
            infile.read((char*)&counts[0], num_classes*sizeof(int));

            infile.read((char*)&num_alignments, sizeof(int));

            values.reserve(num_alignments);
            col_ind.reserve(num_alignments);

            int last_ec = 0;
            row_ptr.push_back(0);

            for (int i = 0; i < num_alignments; ++i) {
                infile.read((char*)&equivalence_id, sizeof(int));
                infile.read((char*)&transcript_id, sizeof(int));
                infile.read((char*)&value, sizeof(int));

                // sanity check that read_id is not less than last_read
                if (equivalence_id < last_ec) {
                    // this is a problem with the file
                    // XXX version 1 files are small enough that we could
                    // probably read this all in and then sort it, and then
                    // iterate over it and build the CRS representation.
                    std::cerr << "ERROR: binary input file must be sorted\n";
                    return NULL;
                }

                values.push_back(value);
                col_ind.push_back(transcript_id);

                if (equivalence_id != last_ec) {
                    // record the start index when we transition to a new read
                    row_ptr.push_back(i);
                    last_ec = equivalence_id;
                }
            }
            row_ptr.push_back(num_alignments);
            aim = new AlignmentIncidenceMatrix(haplotypes, reads, transcripts,
                                               col_ind, row_ptr, values,
                                               counts);
        }
    }
    else {
        //unknown version!
        std::cerr << "Binary input file is unknown format\n";
        return NULL;
    }

    return aim;
}
