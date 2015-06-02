
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

//
//  alignment_incidence_matrix.cpp
//
//
//  Created by Glen Beane on 8/20/14.
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>

#include "alignment_incidence_matrix.h"

AlignmentIncidenceMatrix::AlignmentIncidenceMatrix(std::vector<std::string> haplotypes,
                                                   std::vector<std::string> reads,
                                                   std::vector<std::string> transcripts,
                                                   std::vector<int> col_ind,
                                                   std::vector<int> row_ptr,
                                                   std::vector<int> val) : haplotype_names(haplotypes), transcript_names(transcripts), read_names(reads), col_ind(col_ind), row_ptr(row_ptr), val(val) {

    has_gene_mappings_ = false;
    transcript_lengths_ = NULL;

}

AlignmentIncidenceMatrix::~AlignmentIncidenceMatrix() {
    if (transcript_lengths_) {
        delete [] transcript_lengths_;
    }
}

void AlignmentIncidenceMatrix::setGeneMappings(std::vector<int> tx_to_gene) {
    this->tx_to_gene = tx_to_gene;
    has_gene_mappings_ = true;
}

void AlignmentIncidenceMatrix::setGeneNames(std::vector<std::string> gene_names) {
    this->gene_names = gene_names;
    num_genes_ = (int)gene_names.size();
}

void AlignmentIncidenceMatrix::loadTranscriptLengths(std::string path) {
    std::ifstream input(path);


    // since we don't want to assume that the order the transcripts appear in
    // the length file is the same as the order they are obtained from the input
    // file we go through the pain of
    std::map<std::string, int> transcript_name_to_id;
    std::map<std::string, int> haplotype_name_to_id;

    for (int i = 0; i < num_transcripts(); ++i) {
        transcript_name_to_id[transcript_names[i]] = i;
    }

    for (int i = 0; i < num_haplotypes(); ++i) {
        haplotype_name_to_id[haplotype_names[i]] = i;
    }

    int lengths_loaded = 0;

    transcript_lengths_ = new int[num_transcripts() * num_haplotypes()];

    if (!input.is_open()) {
        // something went wrong reading from stream for now just bail out
        std::cerr << "ERROR LOADING TRANSCRIPT LENGHT FILE " << path << std::endl;
        exit(1);
    }

    int total_elements = num_transcripts() * num_haplotypes();

    int length;
    std::string t_name;
    std::string hap_name;
    std::string id;

    while (input >> id >> length) {

        // need to split t_name:  form read from file is transcriptName_haplotypeName
        std::stringstream id_stringstream(id);
        std::getline(id_stringstream, t_name, '_');
        std::getline(id_stringstream, hap_name, '_');

        if (++lengths_loaded > total_elements) {
            // lengths file contained more transcripts than we expected.
            // for now, just bail out

            std::cerr << "ERROR LOADING TRANSCRIPT LENGTH FILE " << path << std::endl
                      << "EXPECTED " << total_elements << " LENGTH VALUES BUT FILE CONTAINS MORE\n";
            exit(1);

        }

        auto transcript_search = transcript_name_to_id.find(t_name);

        if (transcript_search == transcript_name_to_id.end()) {
            // handle error condition.  Should we abort?  Should we just
            // continue without transcript lengths? For now we will just print
            // an error message and exit.

            std::cerr << "ERROR LOADING TRANSCRIPT LENGTH FILE " << path << std::endl
                      << "UNKNOWN TRANSCRIPT ID: " << t_name << std::endl;

            exit(1);
        }

        auto hap_search = haplotype_name_to_id.find(hap_name);

        if (hap_search == haplotype_name_to_id.end()) {
            std::cerr << "ERROR LOADING TRANSCRIPT LENGTH FILE " << path << std::endl
                      << "UNKNOWN HAPLOTYPE NAME: " << hap_name << std::endl;
        }

        // it exists,  add its lenght to our list of transcript lengths
        transcript_lengths_[transcript_search->second * num_haplotypes() + hap_search->second] = length;

    }

    if (lengths_loaded != total_elements) {
        // didn't have enough transcripts in file.  for now, just bail out.
        std::cerr << "ERROR LOADING TRANSCRIPT LENGHT FILE " << path << std::endl
                  << "EXPECTED " << total_elements << " VALUES BUT FILE CONTAINS FEWER\n";
        exit(1);
    }

    if (input.bad()) {
        // something went wrong reading from stream for now just bail out
        std::cerr << "ERROR LOADING TRANSCRIPT LENGHT FILE " << path << std::endl;
        exit(1);
    }
}
