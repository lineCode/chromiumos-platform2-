{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
    'cflags': [
      '-Wextra',
    ],
    'cflags_cc': [
      '-fno-strict-aliasing',
      '-Woverloaded-virtual',
    ],
    'defines': [
      '__STDC_FORMAT_MACROS',
      '__STDC_LIMIT_MACROS',
    ],
  },
  'targets': [
    {
      'target_name': 'libchromeos-dbus-bindings',
      'type': 'static_library',
      'sources': [
        'xml_interface_parser.cc',
      ],
      'variables': {
        'exported_deps': [
          'expat',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'link_settings': {
        'variables': {
          'deps': [
            'expat',
          ],
        },
      },
    },
    {
      'target_name': 'generate-chromeos-dbus-bindings',
      'type': 'executable',
      'dependencies': ['libchromeos-dbus-bindings'],
      'sources': [
        'generate_chromeos_dbus_bindings.cc',
      ]
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'chromeos_dbus_bindings_unittest',
          'type': 'executable',
          'dependencies': ['libchromeos-dbus-bindings'],
          'includes': ['../../platform2/common-mk/common_test.gypi'],
          'sources': [
            'testrunner.cc',
            'xml_interface_parser_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
