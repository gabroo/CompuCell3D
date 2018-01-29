import sys
from os import environ
from os import getcwd
import string

sys.path.append(environ["PYTHON_MODULE_PATH"])

import CompuCellSetup

# CompuCellSetup.setSimulationXMLFileName("Simulation/FocalPointPlasticity.xml")

sim, simthread = CompuCellSetup.getCoreSimulationObjects()

# Create extra player fields here or add attributes

CompuCellSetup.initializeSimulationObjects(sim, simthread)

# Add Python steppables here
steppableRegistry = CompuCellSetup.getSteppableRegistry()

from FocalPointPlasticityAnchorsSteppables import FocalPointPlasticityAnchorSteppable

focalPointPlasticityAnchorSteppable = FocalPointPlasticityAnchorSteppable(_simulator=sim, _frequency=10)
steppableRegistry.registerSteppable(focalPointPlasticityAnchorSteppable)

CompuCellSetup.mainLoop(sim, simthread, steppableRegistry)
