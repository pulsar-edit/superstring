{
    "targets": [
        {
            "target_name": "superstring",
            "dependencies": [
                "superstring_core"
            ],
            "sources": [
                "src/bindings/bindings.cc",
                "src/bindings/marker-index-wrapper.cc",
                "src/bindings/patch-wrapper.cc",
                "src/bindings/point-wrapper.cc",
                "src/bindings/range-wrapper.cc",
                "src/bindings/text-buffer-wrapper.cc",
                "src/bindings/text-buffer-snapshot-wrapper.cc",
                "src/bindings/text-reader.cc",
                "src/bindings/string-conversion.cc",
                "src/bindings/text-writer.cc",
            ],
            "include_dirs": [
              "src/core",
              "<!(node -e \"require('nan')\")"
            ],
            "conditions": [
                ['OS=="mac"', {
                    "postbuilds": [
                        {
                            'postbuild_name': 'Adjust vendored libiconv install name',
                            'action': [
                                'install_name_tool',
                                "-change",
                                "libiconv.2.dylib",
                                "@loader_path/../../ext/lib/libiconv.2.dylib",
                                "<(PRODUCT_DIR)/superstring.node"
                            ]

                            # NOTE: This version of the post-build action
                            # should be used if we find it necessary to avoid
                            # changing the `dylib`’s install name in an earlier
                            # step.
                            #
                            # 'action': [
                            #     'bash',
                            #     '<(module_root_dir)/script/adjust-install-name.sh',
                            #     '<(PRODUCT_DIR)'
                            # ]

                        }
                    ]
                }]
            ]
        },
        {
            "target_name": "superstring_core",
            "type": "static_library",
            "dependencies": [
                "./vendor/pcre/pcre.gyp:pcre"
            ],
            "sources": [
                "src/core/encoding-conversion.cc",
                "src/core/marker-index.cc",
                "src/core/patch.cc",
                "src/core/point.cc",
                "src/core/range.cc",
                "src/core/regex.cc",
                "src/core/text.cc",
                "src/core/text-buffer.cc",
                "src/core/text-slice.cc",
                "src/core/text-diff.cc",
                "src/core/libmba-diff.cc",
            ],
            "include_dirs": [
                "vendor/libcxx"
            ],
            "conditions": [
                ['OS=="mac"', {
                    'dependencies': [
                        'build_libiconv'
                    ],
                    'include_dirs': [
                        '<(module_root_dir)/ext/include'
                    ],
                    'link_settings': {
                        'libraries': ['<(module_root_dir)/ext/lib/libiconv.2.dylib']
                    }
                }],
                ['OS=="win"', {
                   'sources': [
                       'vendor/win-iconv/win_iconv.c',
                    ],
                    'include_dirs': [
                        'vendor/win-iconv'
                    ],
                    'defines': [
                        'WINICONV_CONST=',
                        'PCRE2_STATIC',
                    ]
                }],
            ],
        }
    ],

    "variables": {
        "tests": 0
    },

    "conditions": [
        ['OS=="mac"', {
            'targets+': [
                {
                    "target_name": "build_libiconv",
                    "target_type": "none",
                    "actions": [
                        {
                            "action_name": "Run script",
                            "message": "Building GNU libiconv...",
                            "inputs": [],
                            "outputs": ["ext"],
                            "action": [
                                "bash",
                                "script/fetch-libiconv-61.sh"
                            ]
                        }
                    ]
                }
                # {
                #     "target_name": "find_libiconv",
                #     "target_type": "none",
                #     "actions": [
                #         {
                #             "action_name": "Run script",
                #             "message": "Locating GNU libiconv...",
                #             "inputs": [],
                #             "outputs": ["vendor/libiconv/lib/libiconv.2.dylib"],
                #             "action": [
                #                 "bash",
                #                 "script/find-gnu-libiconv.sh"
                #             ]
                #         }
                #     ]
                # }
            ]
        }],

        # If --tests is passed to node-gyp configure, we'll build a standalone
        # executable that runs tests on the patch.
        ['tests != 0', {
            "targets": [{
                "target_name": "tests",
                "type": "executable",
                "cflags_cc!": ["-fno-exceptions"],
                "defines": [
                    "CATCH_CONFIG_CPP11_NO_IS_ENUM"
                ],
                "sources": [
                    "test/native/test-helpers.cc",
                    "test/native/tests.cc",
                    "test/native/encoding-conversion-test.cc",
                    "test/native/patch-test.cc",
                    "test/native/text-buffer-test.cc",
                    "test/native/text-test.cc",
                    "test/native/text-diff-test.cc",
                ],
                "include_dirs": [
                    "vendor",
                    "src/core",
                ],
                "dependencies": [
                    "superstring_core"
                ],
                "conditions": [
                    ['OS=="mac"', {
                        'cflags': [
                            '-mmacosx-version-min=10.8'
                        ],
                        "xcode_settings": {
                            "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
                            'MACOSX_DEPLOYMENT_TARGET': '10.8',
                        }
                    }]
                ]
            }]
        }]
    ],

    "target_defaults": {
        "cflags_cc": ["-std=c++11"],
        "conditions": [
            ['OS=="mac"', {
                "xcode_settings": {
                    'CLANG_CXX_LIBRARY': 'libc++',
                    'CLANG_CXX_LANGUAGE_STANDARD':'c++11',
                }
            }],
            ['OS=="win"', {
                "link_settings": {
                    "libraries": ["ws2_32.lib"]
                },
                "defines": [
                    "NOMINMAX"
                ],
            }]
        ]
    }
}
