from distutils.core import setup

setup(name='kinetic-cme-scripts',
      version='1.0',
      description='Scripts for generating input files and plotting output files',
      py_modules = ['scripts.models',
                    'scripts.id_class',
                    'scripts.initial_condition_class',
                    'scripts.index_functions',
                    'scripts.grid_class',
                    'scripts.reaction_class',
                    'scripts.tree_class']
      )
