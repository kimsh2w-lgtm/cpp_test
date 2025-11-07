#!/bin/bash

subsystem="Sample" # Default name

# Check if an argument is provided and use it as the name
if [ -n "$1" ]; then
  subsystem="$1"
fi

subsystem_dirname="$(echo "$subsystem" | tr '[:upper:]' '[:lower:]')"

# ROOT directory
ROOT_PATH="$(dirname "$(dirname "$(dirname "$(readlink -f "$0")")")")"
SUBSYSTEMS_PATH="$ROOT_PATH/subsystems"
SUBSYSTEM_PATH="$ROOT_PATH/subsystems/$subsystem_dirname"
SCRIPT_PATH="$ROOT_PATH/tools/generator"

echo "Root dir path        : $ROOT_PATH"
echo "Subsystems dir path  : $SUBSYSTEMS_PATH"
echo "Subsystem  dir path  : $SUBSYSTEM_PATH"
echo "Generator path       : $SCRIPT_PATH"

mkdir -p "$SUBSYSTEM_PATH"



# Generating ABI
echo "Generating Subsystem($subsystem) abi for: $subsystem"

${SCRIPT_PATH}/gen_subsystem_abi.py $subsystem \
  --outdir ${SUBSYSTEM_PATH}


# Generating Service class
service_class=${subsystem}Service

echo "Generating Subsystem($subsystem) service class for: $service_class"

${SCRIPT_PATH}/gen_class.py $service_class -n $subsystem \
  --outdir ${SUBSYSTEM_PATH}/service \
  --includes "memory,string" \
  --base-includes "ISystemService.hpp" \
  --bases "ISystemService" \
  --methods "void SampleCommand(SampleModel model);"


# Service Publisher class
publisher_class=${subsystem}Publisher

echo "Generating Subsystem($subsystem) publisher class for: $publisher_class"

${SCRIPT_PATH}/gen_class.py $publisheer_class -n $subsystem \
  --outdir ${SUBSYSTEM_PATH}/service \
  --includes "memory,string" \
  --base-includes "ISystemPublisher.hpp" \
  --bases "IServicePublisher" \
  --methods "void RegisterTypes();"

# Sample Domain class
domain_class=${subsystem}Domain

echo "Generating Subsystem($subsystem) sample domain class for: $domain_class"

${SCRIPT_PATH}/gen_class.py $domain_class -n $subsystem \
  --outdir ${SUBSYSTEM_PATH}/domains \
  --includes "memory,string" \
  