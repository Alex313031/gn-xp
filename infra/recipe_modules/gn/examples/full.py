# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEPS = [
    'gn',
    'recipe_engine/buildbucket',
    'recipe_engine/path',
    'recipe_engine/platform',
    'recipe_engine/properties',
    'recipe_engine/step',
]


def RunSteps(api):
  src_dir = api.path['start_dir'].join('gn')
  cipd_dir = api.path['start_dir'].join('cipd')

  api.gn.checkout(src_dir, 'gn.googlesource.com/gn')
  api.gn.build_input
  api.gn.ensure(cipd_dir)
  api.gn.build('debug', ['-d'], run_tests=True)
  api.gn.build('release', [], run_tests=True)


def GenTests(api):
  REVISION = 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'

  for platform in ('linux', 'mac', 'win'):
    yield (api.test('ci_' + platform) + api.platform.name(platform) +
           api.buildbucket.ci_build(
               git_repo='gn.googlesource.com/gn',
               revision=REVISION,
           ))

    yield (api.test('cq_' + platform) + api.platform.name(platform) +
           api.buildbucket.try_build(
               gerrit_host='gn-review.googlesource.com',
               change_number=1000,
               patch_set=1,
           ))