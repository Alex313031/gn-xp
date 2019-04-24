# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.
"""Recipe for building GN."""

from recipe_engine.recipe_api import Property

DEPS = [
    'gn',
    'windows_sdk',
    'macos_sdk',
    'recipe_engine/buildbucket',
    'recipe_engine/cipd',
    'recipe_engine/context',
    'recipe_engine/file',
    'recipe_engine/path',
    'recipe_engine/platform',
    'recipe_engine/properties',
    'recipe_engine/raw_io',
    'recipe_engine/step',
]

PROPERTIES = {
    'repository': Property(kind=str, default='https://gn.googlesource.com/gn'),
}


def RunSteps(api, repository):
  src_dir = api.path['start_dir'].join('gn')
  cipd_dir = api.path['start_dir'].join('cipd')

  api.gn.checkout(src_dir, repository)
  api.gn.ensure(cipd_dir)

  # The order is important since release build will get uploaded to CIPD.
  configs = [
      {
          'name': 'debug',
          'args': ['-d']
      },
      {
          'name': 'release',
          'args': ['--use-lto', '--use-icf']
      },
  ]

  with api.macos_sdk(), api.windows_sdk():
    for config in configs:
      api.gn.build(config['name'], config['args'], run_tests=True)

  if api.gn.build_input.gerrit_changes:
    return

  cipd_pkg_name = 'gn/gn/${platform}'
  gn = 'gn' + ('.exe' if api.platform.is_win else '')

  pkg_def = api.cipd.PackageDefinition(
      package_name=cipd_pkg_name,
      package_root=src_dir.join('out'),
      install_mode='copy')
  pkg_def.add_file(src_dir.join('out', gn))
  pkg_def.add_version_file('.versions/%s.cipd_version' % gn)

  cipd_pkg_file = api.path['cleanup'].join('gn.cipd')

  api.cipd.build_from_pkg(
      pkg_def=pkg_def,
      output_package=cipd_pkg_file,
  )

  if api.buildbucket.builder_id.project == 'infra-internal':
    with api.context(cwd=src_dir):
      revision = api.step(
          'rev-parse', ['git', 'rev-parse', 'HEAD'],
          stdout=api.raw_io.output()).stdout.strip()

    cipd_pin = api.cipd.search(cipd_pkg_name, 'git_revision:' + revision)
    if cipd_pin:
      api.step('Package is up-to-date', cmd=None)
      return

    api.cipd.register(
        package_name=cipd_pkg_name,
        package_path=cipd_pkg_file,
        refs=['latest'],
        tags={
            'git_repository': repository,
            'git_revision': revision,
        },
    )


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

  yield (api.test('cipd_exists') + api.buildbucket.ci_build(
      project='infra-internal',
      git_repo='gn.googlesource.com/gn',
      revision=REVISION,
  ) + api.step_data('rev-parse', api.raw_io.stream_output(REVISION)) +
         api.step_data('cipd search gn/gn/${platform} git_revision:' + REVISION,
                       api.cipd.example_search('gn/gn/linux-amd64',
                                               ['git_revision:' + REVISION])))

  yield (api.test('cipd_register') + api.buildbucket.ci_build(
      project='infra-internal',
      git_repo='gn.googlesource.com/gn',
      revision=REVISION,
  ) + api.step_data('rev-parse', api.raw_io.stream_output(REVISION)) +
         api.step_data('cipd search gn/gn/${platform} git_revision:' + REVISION,
                       api.cipd.example_search('gn/gn/linux-amd64', [])))
