#!/bin/bash
#
# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if [[ ! -d libcore ]];  then
  echo "Script needs to be run at the root of the android tree"
  exit 1
fi

if [[ `uname` != 'Linux' ]];  then
  echo "Script cannot be run on $(uname). It is Linux only."
  exit 2
fi

# See b/141907697. These tests all crash on both the RI and ART when using the libjdwp agent JDWP
# implementation. To avoid them cluttering the log on the buildbot we explicitly skip them. This
# list should not be added to.
declare -a known_bad_tests=(
  'org.apache.harmony.jpda.tests.jdwp.ClassType_NewInstanceTest#testNewInstance002'
  'org.apache.harmony.jpda.tests.jdwp.ObjectReference_GetValues002Test#testGetValues002'
  'org.apache.harmony.jpda.tests.jdwp.ObjectReference_SetValuesTest#testSetValues001'
  'org.apache.harmony.jpda.tests.jdwp.ThreadGroupReference_NameTest#testName001_NullObject'
  'org.apache.harmony.jpda.tests.jdwp.ThreadGroupReference_ParentTest#testParent_NullObject'
)

declare -a args=("$@")
debug="no"
has_variant="no"
has_mode="no"
mode="target"
has_gcstress="no"
has_timeout="no"
has_verbose="no"
has_jdwp_path="no"
# The bitmap of log messages in libjdwp. See list in the help message for more
# info on what these are. The default is 'errors | callbacks'
verbose_level=0xC0

arg_idx=0
while true; do
  if [[ $1 == "--debug" ]]; then
    debug="yes"
    shift
  elif [[ $1 == --test-timeout-ms ]]; then
    has_timeout="yes"
    shift
    arg_idx=$((arg_idx + 1))
    shift
  elif [[ "$1" == "--mode=jvm" ]]; then
    has_mode="yes"
    mode="ri"
    shift
  elif [[ "$1" == --mode=host ]]; then
    has_mode="yes"
    mode="host"
    shift
  elif [[ $1 == --verbose-all ]]; then
    has_verbose="yes"
    verbose_level=0xFFF
    unset args[arg_idx]
    shift
  elif [[ $1 == --no-skips ]]; then
    declare -a known_bad_tests=()
    unset args[arg_idx]
    shift
  elif [[ $1 == --verbose ]]; then
    has_verbose="yes"
    shift
  elif [[ $1 == --verbose-level ]]; then
    shift
    verbose_level=$1
    # remove both the --verbose-level and the argument.
    unset args[arg_idx]
    arg_idx=$((arg_idx + 1))
    unset args[arg_idx]
    shift
  elif [[ $1 == --variant=* ]]; then
    has_variant="yes"
    shift
  elif [[ $1 == *gcstress ]]; then
    has_gcstress="yes"
    shift
  elif [[ $1 == --jdwp-path* ]]; then
    has_jdwp_path="yes"
    shift
  elif [[ "$1" == "" ]]; then
    break
  else
    shift
  fi
  arg_idx=$((arg_idx + 1))
done

if [[ "$has_mode" = "no" ]];  then
  args+=(--mode=device)
fi

if [[ "$has_variant" = "no" && "$mode" != "ri" ]];  then
  # --mode=jvm doesn't support variant option.
  args+=(--variant=X32)
fi

if [[ "$has_jdwp_path" = "no" ]]; then
  if [[ "$mode" == "ri" ]]; then
    if [ -z "$ANDROID_BUILD_TOP" ]; then
      echo "Please set ANDROID_BUILD_TOP"
      exit 1
    fi
    args+=(--jdwp-path  $ANDROID_BUILD_TOP/"prebuilts/jdk/jdk21/linux-x86/lib/libjdwp.so")
  else
    args+=(--jdwp-path  "libjdwp.so")
  fi
fi

if [[ "$has_timeout" = "no" ]]; then
  # Double the timeout to 20 seconds
  args+=(--test-timeout-ms)
  if [[ "$has_verbose" = "yes" || "$has_gcstress" = "yes" ]]; then
    # Extra time if verbose or gcstress is set since those can be
    # quite heavy.
    args+=(300000)
  else
    args+=(20000)
  fi
fi

if [[ "$has_verbose" = "yes" ]]; then
  args+=(--vm-arg)
  args+=(-Djpda.settings.debuggeeAgentExtraOptions=directlog=y,logfile=/proc/self/fd/2,logflags=$verbose_level)
fi

if [[ "$mode" != "ri" ]]; then
  # We don't use full paths since it is difficult to determine them for device
  # tests and not needed due to resolution rules of dlopen.
  if [[ "$debug" = "yes" ]]; then
    args+=(-Xplugin:libopenjdkjvmtid.so)
  else
    args+=(-Xplugin:libopenjdkjvmti.so)
  fi
fi

expectations="--expectations $PWD/art/tools/external_oj_libjdwp_art_failures.txt"

if [[ "$debug" = "yes" && "$has_gcstress" = "yes" ]]; then
  expectations="$expectations --expectations $PWD/art/tools/external_oj_libjdwp_art_gcstress_debug_failures.txt"
fi

if [[ "${ART_USE_READ_BARRIER}" = "false" ]]; then
  expectations="$expectations --expectations $PWD/art/tools/external_oj_libjdwp_art_no_read_barrier_failures.txt"
fi

function verbose_run() {
  echo "$@"
  env "$@"
}

for skip in "${known_bad_tests[@]}"; do
  args+=("--skip-test" "$skip")
done

# Tell run-jdwp-tests.sh it was called from run-libjdwp-tests.sh
export RUN_JDWP_TESTS_CALLED_FROM_LIBJDWP=true

verbose_run ./art/tools/run-jdwp-tests.sh \
            "${args[@]}"                  \
            --vm-arg -Djpda.settings.debuggeeAgentExtraOptions=coredump=y \
            --vm-arg -Djpda.settings.testSuiteType=libjdwp \
            "$expectations"
