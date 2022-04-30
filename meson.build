project('efivar', 'c', license: 'LGPL-2.1-only', version: '38')

lib_ver = '1.@0@'.format(meson.project_version())
add_project_arguments('-DLIBEFIVAR_VERSION=@0@'.format(meson.project_version()), language : 'c')

subdir('src')
subdir('docs')
