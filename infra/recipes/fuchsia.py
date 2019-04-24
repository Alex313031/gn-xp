# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.
"""Recipe for building GN."""

from recipe_engine.config import List
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
    'repository':
        Property(kind=str, default='https://gn.googlesource.com/gn'),
    'project':
        Property(
            kind=str,
            help='Jiri remote manifest project',
            default='integration'),
    'manifest':
        Property(kind=str, help='Jiri manifest to use', default='flower'),
    'remote':
        Property(
            kind=str,
            help='Remote manifest repository',
            default='https://fuchsia.googlesource.com/integration'),
    'gn_args':
        Property(
            kind=List(basestring), help='GN args to build Fuchsia', default=[]),
}


def checkout_fuchsia(api, project, manifest, remote, fuchsia_dir,
                     jiri_executable):
  """Checkout the Fuchsia codebase"""
  api.file.ensure_directory('ensure fuchsia checkout dir', fuchsia_dir)
  with api.context(cwd=fuchsia_dir):
    with api.step.nest('checkout'):
      api.step('init', [
          jiri_executable,
          'init',
          '-vv',
          '-time',
          '-j=50',
          '-analytics-opt=false',
          '-rewrite-sso-to-https=true',
          '-cache',
          api.path['cache'].join('git'),
          '-shared',
      ])

      api.step('import', [
          jiri_executable,
          'import',
          '-vv',
          '-time',
          '-j=50',
          '-name',
          project,
          '-revision',
          'HEAD',
          manifest,
          remote,
      ])

      api.step('update', [
          jiri_executable,
          'update',
          '-vv',
          '-time',
          '-j=50',
          '-autoupdate=false',
          '-attempts=3',
          '-run-hooks=false',
      ])

      api.step('run-hooks', [
          jiri_executable,
          'run-hooks',
          '-vv',
          '-time',
          '-j=50',
          '-attempts=3',
      ])


def build_fuchsia(api,
                  fuchsia_dir,
                  gn_executable,
                  gn_args=None):
  """Build Fuchsia with the given GN executable"""
  ninja_executable = fuchsia_dir.join('buildtools', 'ninja')
  with api.step.nest('build'):
    with api.context(cwd=fuchsia_dir):
      build_dir = fuchsia_dir.join('out', 'default')
      zircon_build_dir = fuchsia_dir.join('out', 'default.zircon')
      gn_cmd = [gn_executable, 'gen', build_dir]
      if gn_args:
        gn_cmd.extend(gn_args)
      api.step('gn gen', gn_cmd)
      api.step('ninja', [ninja_executable, '-C', zircon_build_dir])
      api.step('ninja', [ninja_executable, '-C', build_dir])


def RunSteps(api, repository, project, manifest, remote, gn_args):
  src_dir = api.path['start_dir'].join('gn')
  cipd_dir = api.path['start_dir'].join('cipd')
  fuchsia_dir = api.path['start_dir'].join('fuchsia')

  # Checkout and build GN.
  api.gn.checkout(src_dir, repository)
  api.gn.ensure(cipd_dir)
  with api.macos_sdk(), api.windows_sdk():
    api.gn.build('release', ['--use-lto', '--use-icf'], run_tests=False)
  gn_executable = src_dir.join('out', 'gn')
  ninja_executable = cipd_dir.join('ninja')

  # Checkout and build Fuchsia with the new GN.
  with api.step.nest('ensure_jiri'):
    with api.context(infra_steps=True):
      pkgs = api.cipd.EnsureFile()
      pkgs.add_package('fuchsia/tools/jiri/${platform}', 'stable')
      api.cipd.ensure(cipd_dir, pkgs)
  jiri_executable = cipd_dir.join('jiri')

  checkout_fuchsia(api, project, manifest, remote, fuchsia_dir, jiri_executable)
  build_fuchsia(api, fuchsia_dir, gn_executable, gn_args)


def GenTests(api):
  REVISION = 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'

  yield (api.test('ci') + api.platform.name('linux') +
          api.properties(gn_args=['--args=""']) + api.buildbucket.ci_build(
              git_repo='gn.googlesource.com/gn',
              revision=REVISION,
          ))

  yield (api.test('cq') + api.platform.name('linux') +
          api.buildbucket.try_build(
              gerrit_host='gn-review.googlesource.com',
              change_number=1000,
              patch_set=1,
          ))
