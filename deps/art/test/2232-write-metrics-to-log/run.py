#!/bin/bash
#
# Copyright (C) 2020 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


def run(ctx, args):
  ctx.default_run(
      args,
      android_log_tags="*:i",
      diff_min_log_tag="i",
      runtime_option=[
          "-Xmetrics-force-enable:true",
          "-Xmetrics-write-to-logcat:true",
          "-Xmetrics-reporting-mods:100",
      ])

  # Check that one of the metrics appears in stderr.
  ctx.run(
      fr"sed -i -n 's/.*\(ClassVerificationTotalTimeDelta\).*/\1/p' '{args.stderr_file}'"
  )
