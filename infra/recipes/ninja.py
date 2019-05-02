# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.
"""Recipe for building Ninja."""

from recipe_engine.recipe_api import Property

DEPS = [
    'recipe_engine/buildbucket',
    'recipe_engine/cipd',
    'recipe_engine/context',
    'recipe_engine/file',
    'recipe_engine/json',
    'recipe_engine/path',
    'recipe_engine/platform',
    'recipe_engine/properties',
    'recipe_engine/python',
    'recipe_engine/raw_io',
    'recipe_engine/step',
    'macos_sdk',
    'windows_sdk',
]

GIT_URL = 'gn.googlesource.com/external/github.com/ninja-build/ninja'


def RunSteps(api):
  src_dir = api.path['start_dir'].join('ninja')

  with api.step.nest('git'), api.context(infra_steps=True):
    api.step('init', ['git', 'init', src_dir])

    with api.context(cwd=src_dir):
      gitiles_commit = api.buildbucket.build_input.gitiles_commit
      repository = ('https://%s/%s' % (gitiles_commit.host,
                                       gitiles_commit.project) if
                    gitiles_commit.host and gitiles_commit.project else GIT_URL)
      ref = gitiles_commit.id or 'refs/heads/master'
      # Fetch tags so `git describe` works.
      api.step('fetch', ['git', 'fetch', '--tags', repository, ref])
      api.step('checkout', ['git', 'checkout', 'FETCH_HEAD'])

  with api.context(infra_steps=True):
    cipd_dir = api.path['start_dir'].join('cipd')
    pkgs = api.cipd.EnsureFile()
    if api.platform.is_linux or api.platform.is_mac:
      pkgs.add_package('fuchsia/clang/${platform}', 'goma')
    if api.platform.is_linux:
      pkgs.add_package('fuchsia/sysroot/${platform}',
                       'git_revision:a28dfa20af063e5ca00634024c85732e20220419',
                       'sysroot')
    api.cipd.ensure(cipd_dir, pkgs)

  with api.macos_sdk(), api.windows_sdk():
    if api.platform.is_linux:
      sysroot = '--sysroot=%s' % cipd_dir.join('sysroot')
      env = {
          'CXX': cipd_dir.join('bin', 'clang++'),
          'AR': cipd_dir.join('bin', 'llvm-ar'),
          'CXXFLAGS': sysroot,
          'LDFLAGS': sysroot,
      }
    elif api.platform.is_mac:
      sysroot = '--sysroot=%s' % api.step(
          'xcrun', ['xcrun', '--show-sdk-path'],
          stdout=api.raw_io.output(name='sdk-path', add_output_log=True),
          step_test_data=
          lambda: api.raw_io.test_api.stream_output('/some/xcode/path')
      ).stdout.strip()
      stdlib = '-nostdlib++ %s' % (cipd_dir.join('lib', 'libc++.a'))
      env = {
          'CXX': cipd_dir.join('bin', 'clang++'),
          'AR': cipd_dir.join('bin', 'llvm-ar'),
          'CFLAGS': sysroot,
          'LDFLAGS': '%s -nostdlib++ %s' % (sysroot, stdlib),
      }
    else:
      env = {}

    with api.context(env=env, cwd=src_dir):
      api.python(
          'bootstrap', src_dir.join('configure.py'), args=['--bootstrap'])

      with api.step.nest('test'):
        api.step('ninja', [src_dir.join('ninja'), 'ninja_test'])
        api.step('test', [src_dir.join('ninja_test')])

      if api.platform.is_linux:
        api.step('strip', [
            cipd_dir.join('bin', 'llvm-objcopy'), '--strip-sections',
            src_dir.join('ninja')
        ])
      elif api.platform.is_mac:
        api.step('strip', ['xcrun', 'strip', '-x', src_dir.join('ninja')])

  cipd_pkg_name = 'gn/ninja/${platform}'
  ninja = 'ninja' + ('.exe' if api.platform.is_win else '')

  pkg_def = api.cipd.PackageDefinition(
      package_name=cipd_pkg_name,
      package_root=src_dir.join('out'),
      install_mode='copy')
  pkg_def.add_file(src_dir.join('out', ninja))
  pkg_def.add_version_file('.versions/%s.cipd_version' % ninja)

  cipd_pkg_file = api.path['cleanup'].join('ninja.cipd')

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
  for platform in ('linux', 'mac', 'win'):
    yield (api.test('ci_' + platform) + api.platform.name(platform) +
           api.buildbucket.ci_build(
               git_repo=GIT_URL,
               revision='a' * 40,
           ))

  yield (
      api.test('cipd_exists') + api.buildbucket.ci_build(
          project='infra-internal',
          git_repo=GIT_URL,
          revision='a' * 40,
      ) + api.step_data('rev-parse', api.raw_io.stream_output('a' * 40)) +
      api.step_data('cipd search gn/ninja/${platform} git_revision:' + 'a' * 40,
                    api.cipd.example_search('gn/ninja/linux-amd64',
                                            ['git_revision:' + 'a' * 40])))

  yield (
      api.test('cipd_register') + api.buildbucket.ci_build(
          project='infra-internal',
          git_repo=GIT_URL,
          revision='a' * 40,
      ) + api.step_data('rev-parse', api.raw_io.stream_output('a' * 40)) +
      api.step_data('cipd search gn/ninja/${platform} git_revision:' + 'a' * 40,
                    api.cipd.example_search('gn/ninja/linux-amd64', [])))
