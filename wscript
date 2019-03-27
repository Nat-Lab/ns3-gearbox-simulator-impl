# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# def options(opt):
#     pass

# def configure(conf):
#     conf.check_nonfatal(header_name='stdint.h', define_name='HAVE_STDINT_H')

def build(bld):
    module = bld.create_ns3_module('gearbox-simulator', ['core'])
    module.source = [
        'model/gearbox-simulator-impl.cc',
        'model/gearbox-synchronizer.cc'
        ]

    headers = bld(features='ns3header')
    headers.module = 'gearbox-simulator'
    headers.source = [
        'model/gearbox-simulator-impl.h',
        'model/gearbox-synchronizer.h'
        ]

    # bld.ns3_python_bindings()

