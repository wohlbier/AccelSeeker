#!/bin/bash

if [ "$#" -ne 1 ]; then
  echo "Usage: generate_accelcands_list.sh NAME"
  exit 1
fi
NAME=$1

# Prepare the AccelSeeker Candidates for the selection taken into account the overlapping rule.
# Generate AccelSeeker Candidates list with Merit/Cost and Call Function Indexes.

printf "Compute Merit/Cost File - MC.txt\n"
compute_merit.sh $NAME

printf "Generate Overlapping Rule - FCI_CROPPED_OVERLAP_RULE.txt\n"
prune_fci_files.sh
python ${AS_PATH}/scripts/generate_overlapping_rule.py

printf "Merge MC.txt and FCI_CROPPED_OVERLAP_RULE.txt\n"
printf "Generate AccelSeeker Candidates list with Merit/Cost and Call Function Indexes - MCI.txt\n"
merge_mc_and_fci.sh
