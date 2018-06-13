# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.
"""Recipe for building GN."""

DEPS = [
    'recipe_engine/buildbucket',
    'recipe_engine/cipd',
    'recipe_engine/context',
    'recipe_engine/path',
    'recipe_engine/platform',
    'recipe_engine/properties',
    'recipe_engine/python',
    'recipe_engine/step',
]


def RunSteps(api):
  src_dir = api.path['start_dir'].join('gn')

  with api.step.nest('git'), api.context(infra_steps=True):
    api.step('init', ['git', 'init', src_dir])

    with api.context(cwd=src_dir):
      build_input = api.buildbucket.build_input
      ref = (
          build_input.gitiles_commit.id
          if build_input.gitiles_commit else 'refs/heads/master')
      api.step('fetch', ['git', 'fetch', 'https://gn.googlesource.com/gn', ref])
      api.step('checkout', ['git', 'checkout', 'FETCH_HEAD'])
      for change in build_input.gerrit_changes:
        api.step('fetch %s/%s' % (change.change, change.patchset), [
            'git', 'fetch',
            'https://%s/gn' % change.host,
            'refs/changes/%s/%s/%s' %
            (str(change.change)[-2:], change.change, change.patchset)
        ])
        api.step('cherry-pick %s/%s' % (change.change, change.patchset),
                 ['git', 'cherry-pick', 'FETCH_HEAD'])

  with api.context(infra_steps=True):
    cipd_dir = api.path['start_dir'].join('cipd')
    packages = {
        'infra/ninja/${platform}': 'version:1.8.2',
    }
    packages.update({
        'linux': {
            'fuchsia/clang/${platform}': 'goma',
        },
        'mac': {},
        'win': {
            'chrome_internal/third_party/sdk/windows': 'latest',
        },
    }[api.platform.name])
    api.cipd.ensure(cipd_dir, packages)

  win_env = {}
  if api.platform.name == 'win':
    # Load .../win_sdk/bin/SetEnv.x64.json to extract the required environment.
    # It contains a dict that looks like this:
    #
    # {
    #   "env": {
    #     "VSINSTALLDIR": [["..", "..\\"]],
    #     "VCINSTALLDIR": [["..", "..", "VC\\"]],
    #     "INCLUDE": [["..", "..", "win_sdk", "Include", "10.0.17134.0", "um"], ["..", "..", "win_sdk", "Include", "10.0.17134.0", "shared"], ["..", "..", "win_sdk", "Include", "10.0.17134.0", "winrt"], ["..", "..", "win_sdk", "Include", "10.0.17134.0", "ucrt"], ["..", "..", "VC", "Tools", "MSVC", "14.14.26428", "include"], ["..", "..", "VC", "Tools", "MSVC", "14.14.26428", "atlmfc", "include"]],
    #     "VCToolsInstallDir": [["..", "..", "VC", "Tools", "MSVC", "14.14.26428\\"]],
    #     "PATH": [["..", "..", "win_sdk", "bin", "10.0.17134.0", "x64"], ["..", "..", "VC", "Tools", "MSVC", "14.14.26428", "bin", "HostX64", "x64"]],
    #     "LIB": [["..", "..", "VC", "Tools", "MSVC", "14.14.26428", "lib", "x64"], ["..", "..", "win_sdk", "Lib", "10.0.17134.0", "um", "x64"], ["..", "..", "win_sdk", "Lib", "10.0.17134.0", "ucrt", "x64"], ["..", "..", "VC", "Tools", "MSVC", "14.14.26428", "atlmfc", "lib", "x64"]]
    #   }
    # }
    sdk_dir = cipd_dir.join('chrome_internal', 'third_party', 'sdk', 'windows',
                            'winsdk')
    json_file = sdk_dir.join('bin', 'SetEnv.x64.json')
    env = json.load(open(json_file))['env']
    for k in env:
      entries = [os.path.join(*([sdk_dir.join('bin')] + e))
                 for e in env[k]]
      env[k] = sep.join(entries)
    win_env = env

  environ = {
      'linux': {
          'CC': cipd_dir.join('bin', 'clang'),
          'CXX': cipd_dir.join('bin', 'clang++'),
          'AR': cipd_dir.join('bin', 'llvm-ar'),
          'LDFLAGS': '-static-libstdc++ -ldl -lpthread',
      },
      'mac': {},
      'win': win_env,
  }[api.platform.name]

  configs = [
      {
          'name': 'debug',
          'args': ['-d']
      },
      {
          'name': 'release',
          'args': []
      },
  ]

  for config in configs:
    with api.step.nest(config['name']):
      with api.step.nest('build'):
        with api.context(env=environ, cwd=src_dir):
          api.python('generate',
                     src_dir.join('build', 'gen.py'),
                     args=config['args'])

        api.step('ninja', [cipd_dir.join('ninja'), '-C', src_dir.join('out')])

      with api.context(cwd=src_dir):
        api.step('test', [src_dir.join('out', 'gn_unittests')])


def GenTests(api):
  for platform in ('linux', 'mac', 'win'):
    yield (api.test('ci_' + platform) + api.platform.name(platform) +
           api.properties(buildbucket={
               'build': {
                   'tags': [
                       'buildset:commit/gitiles/gn.googlesource.com/gn/+/'
                       'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
                   ]
               }
           }))
    yield (
        api.test('cq_' + platform) + api.platform.name(platform) +
        api.properties(buildbucket={
            'build': {
                'tags': [
                    'buildset:patch/gerrit/gn-review.googlesource.com/1000/1',
                ]
            }
        }))
