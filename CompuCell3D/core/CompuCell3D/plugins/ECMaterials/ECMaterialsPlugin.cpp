/*************************************************************************
*    CompuCell - A software framework for multimodel simulations of     *
* biocomplexity problems Copyright (C) 2003 University of Notre Dame,   *
*                             Indiana                                   *
*                                                                       *
* This program is free software; IF YOU AGREE TO CITE USE OF CompuCell  *
*  IN ALL RELATED RESEARCH PUBLICATIONS according to the terms of the   *
*  CompuCell GNU General Public License RIDER you can redistribute it   *
* and/or modify it under the terms of the GNU General Public License as *
*  published by the Free Software Foundation; either version 2 of the   *
*         License, or (at your option) any later version.               *
*                                                                       *
* This program is distributed in the hope that it will be useful, but   *
*      WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU    *
*             General Public License for more details.                  *
*                                                                       *
*  You should have received a copy of the GNU General Public License    *
*     along with this program; if not, write to the Free Software       *
*      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.        *
*************************************************************************/

#include <CompuCell3D/CC3D.h>

using namespace CompuCell3D;

#include <CompuCell3D/plugins/NeighborTracker/NeighborTrackerPlugin.h>

#include "ECMaterialsPlugin.h"


ECMaterialsPlugin::ECMaterialsPlugin() :
    pUtils(0),
    lockPtr(0),
    xmlData(0),
    numberOfMaterials(0),
    weightDistance(false),
    ECMaterialsInitialized(false)
{}

ECMaterialsPlugin::~ECMaterialsPlugin() {
    pUtils->destroyLock(lockPtr);
    delete lockPtr;
    lockPtr = 0;
}

void ECMaterialsPlugin::init(Simulator *simulator, CC3DXMLElement *_xmlData) {
    xmlData = _xmlData;
    sim = simulator;
    potts = simulator->getPotts();
	automaton = potts->getAutomaton();

    pUtils = sim->getParallelUtils();
    lockPtr = new ParallelUtilsOpenMP::OpenMPLock_t;
    pUtils->initLock(lockPtr);

    cerr << "Registering ECMaterials cell attributes..." << endl;

    potts->getCellFactoryGroupPtr()->registerClass(&ECMaterialCellDataAccessor);

	cerr << "Registering ECMaterials plugin..." << endl;

    potts->registerEnergyFunctionWithName(this, "ECMaterials");
    potts->registerCellGChangeWatcher(this);
    simulator->registerSteerableObject(this);

    if (_xmlData->getFirstElement("WeightEnergyByDistance")) {
        weightDistance = true;
    }

	// Boundary strategy for adhesion; based on implementation in AdhesionFlex
	boundaryStrategyAdh = BoundaryStrategy::getInstance();
	maxNeighborIndexAdh = 0;

	if (_xmlData->getFirstElement("Depth")) {
		maxNeighborIndexAdh = boundaryStrategyAdh->getMaxNeighborIndexFromDepth(_xmlData->getFirstElement("Depth")->getDouble());
	}
	else {
		if (_xmlData->getFirstElement("NeighborOrder")) {
			maxNeighborIndexAdh = boundaryStrategyAdh->getMaxNeighborIndexFromNeighborOrder(_xmlData->getFirstElement("NeighborOrder")->getUInt());
		}
		else {
			maxNeighborIndexAdh = boundaryStrategyAdh->getMaxNeighborIndexFromNeighborOrder(1);
		}
	}

    // Gather XML user specifications
    CC3DXMLElementList ECMaterialNameXMLVec = _xmlData->getElements("ECMaterial");
    ECMaterialAdhesionXMLVec = _xmlData->getElements("ECAdhesion");
    CC3DXMLElementList ECMaterialAdvectionBoolXMLVec = _xmlData->getElements("ECMaterialAdvects");
    CC3DXMLElementList ECMaterialDurabilityXMLVec = _xmlData->getElements("ECMaterialDurability");
    ECMaterialRemodelingQuantityXMLVec = _xmlData->getElements("RemodelingQuantity");

    // generate name->integer index map for EC materials
    // generate array of pointers to EC materials according to user specification
    // assign material name from user specification
    set<std::string> ECMaterialNameSet;
    ECMaterialsVec.clear();
    ECMaterialNameIndexMap.clear();

    std::string ECMaterialName;

	cerr << "Declaring ECMaterials... " << endl;

    for (int i = 0; i < ECMaterialNameXMLVec.size(); ++i) {
        ECMaterialName = ECMaterialNameXMLVec[i]->getAttribute("Material");

		cerr << "   ECMaterial " << i << ": " << ECMaterialName << endl;

        if (!ECMaterialNameSet.insert(ECMaterialName).second) {
            ASSERT_OR_THROW(string("Duplicate ECMaterial Name=") + ECMaterialName + " specified in ECMaterials section ", false);
            continue;
        }

        ECMaterialsVec.push_back(ECMaterialComponentData());
        ECMaterialsVec[i].setName(ECMaterialName);

        ECMaterialNameIndexMap.insert(make_pair(ECMaterialName, i));
    }

    numberOfMaterials = ECMaterialsVec.size();

	cerr << "Number of ECMaterials defined: " << numberOfMaterials << endl;

    unsigned int ECMaterialIdx;
	bool ECMaterialIsAdvecting;

    // Assign optional advection specifications

	cerr << "Checking material advection options..." << endl;

    if (ECMaterialAdvectionBoolXMLVec.size() > 0){
        for (int i = 0; i < ECMaterialAdvectionBoolXMLVec.size(); i++) {
			ECMaterialName = ECMaterialAdvectionBoolXMLVec[i]->getAttribute("Material");
			ECMaterialIsAdvecting = ECMaterialAdvectionBoolXMLVec[i]->getBool();

			cerr << "   ECMaterial " << ECMaterialName << " advection mode: " << ECMaterialIsAdvecting << endl;

            ECMaterialIdx = getECMaterialIndexByName(ECMaterialName);
            ECMaterialsVec[ECMaterialIdx].setTransferable(ECMaterialIsAdvecting);
        }
    }

    // Assign barrier Lagrange multiplier to each material from user specification

	float durabilityLM;

	cerr << "Assigning ECMaterial durability coefficients..." << endl;

    if (ECMaterialDurabilityXMLVec.size() > 0) {
        for (int i = 0; i < ECMaterialDurabilityXMLVec.size(); ++i) {
			ECMaterialName = ECMaterialDurabilityXMLVec[i]->getAttribute("Material");
			durabilityLM = (float)ECMaterialDurabilityXMLVec[i]->getDouble();

			cerr << "   ECMaterial " << ECMaterialName << " barrier Lagrange multiplier: " << durabilityLM << endl;
			
			ECMaterialIdx = getECMaterialIndexByName(ECMaterialName);
            ECMaterialsVec[ECMaterialIdx].setDurabilityLM(durabilityLM);
        }
    }

    // Initialize quantity vector field

	cerr << "Initializing ECMaterials quantity field..." << endl;

    fieldDim=potts->getCellFieldG()->getDim();
    //      Unsure if constructor can be used as initial value
    // ECMaterialsField = new WatchableField3D<ECMaterialsData *>(fieldDim, ECMaterialsData());
    //      Trying 0, as in Potts3D::createCellField
    ECMaterialsField = new WatchableField3D<ECMaterialsData *>(fieldDim, 0);

    // initialize EC material quantity vector field values by user specification
    // default is all first ECM component
    // somehow apply user specification of initial values

	cerr << "Initializing ECMaterials quantity field values..." << endl;

    Point3D pt(0,0,0);
    ECMaterialsData *ECMaterialsDataLocal;
    for (int z = 0; z < fieldDim.z; ++z){
        for (int y = 0; y < fieldDim.y; ++y){
            for (int x = 0; x < fieldDim.x; ++x){
				pt = Point3D(x, y, z);

				ECMaterialsDataLocal = new ECMaterialsData();
				std::vector<float> & ECMaterialsQuantityVec = ECMaterialsDataLocal->ECMaterialsQuantityVec;
				std::vector<float> ECMaterialsQuantityVecNew(numberOfMaterials);
				ECMaterialsQuantityVecNew[0] = 1.0;
				ECMaterialsQuantityVec = ECMaterialsQuantityVecNew;
                ECMaterialsField->set(pt, ECMaterialsDataLocal);
            }
        }
    }

}

void ECMaterialsPlugin::extraInit(Simulator *simulator) {
    update(xmlData, true);

}

void ECMaterialsPlugin::handleEvent(CC3DEvent & _event) {
    if (_event.id == CHANGE_NUMBER_OF_WORK_NODES) {

        //vectorized variables for convenient parallel access
        unsigned int maxNumberOfWorkNodes = pUtils->getMaxNumberOfWorkNodesPotts();

    }
}


double ECMaterialsPlugin::changeEnergy(const Point3D &pt, const CellG *newCell, const CellG *oldCell) {
    //cerr<<"ChangeEnergy"<<endl;
    if (!ECMaterialsInitialized) {
        pUtils->setLock(lockPtr);
        initializeECMaterials();
        pUtils->unsetLock(lockPtr);
    }

    double energy = 0;
    double distance = 0;
    Neighbor neighbor;
    vector<float> targetQuantityVec(numberOfMaterials);

    // Target medium and durability
    if (oldCell == 0) {
        targetQuantityVec = ECMaterialsField->get(pt)->ECMaterialsQuantityVec;
        energy += ECMaterialDurabilityEnergy(targetQuantityVec);
    }
    vector<float> copyQuantityVector(numberOfMaterials);
    if ((newCell == 0) && (oldCell != 0)) {
        copyQuantityVector = calculateCopyQuantityVec(oldCell, pt);
    }

    CellG *nCell = 0;
    WatchableField3D<CellG *> *fieldG = (WatchableField3D<CellG *> *)potts->getCellFieldG();

    if (weightDistance) {
        for (unsigned int nIdx = 0; nIdx <= maxNeighborIndexAdh; ++nIdx) {
            neighbor = boundaryStrategyAdh->getNeighborDirect(const_cast<Point3D&>(pt), nIdx);

            if (!neighbor.distance) {
                //if distance is 0 then the neighbor returned is invalid
                continue;
            }

            distance = neighbor.distance;

            nCell = fieldG->get(neighbor.pt);
			vector<float> nQuantityVec = ECMaterialsField->get(neighbor.pt)->ECMaterialsQuantityVec;

            if (nCell != oldCell) {
                if (nCell == 0) {
                    energy -= ECMaterialContactEnergy(oldCell, nQuantityVec) / neighbor.distance;
                }
                else if (oldCell == 0) {
                    energy -= ECMaterialContactEnergy(nCell, targetQuantityVec) / neighbor.distance;
                }
            }
            if (nCell != newCell) {
                if (nCell == 0) {
                    energy += ECMaterialContactEnergy(newCell, nQuantityVec) / neighbor.distance;
                }
                else if (newCell == 0) {
                    energy += ECMaterialContactEnergy(nCell, copyQuantityVector) / neighbor.distance;
                }
            }
        }
    }
    else {
        //default behaviour  no energy weighting

        for (unsigned int nIdx = 0; nIdx <= maxNeighborIndexAdh; ++nIdx) {
            neighbor = boundaryStrategyAdh->getNeighborDirect(const_cast<Point3D&>(pt), nIdx);
            if (!neighbor.distance) {
                //if distance is 0 then the neighbor returned is invalid
                continue;
            }
            nCell = fieldG->get(neighbor.pt);
			vector<float> nQuantityVec = ECMaterialsField->get(neighbor.pt)->ECMaterialsQuantityVec;

            if (nCell != oldCell) {
                if (nCell == 0) {
                    energy -= ECMaterialContactEnergy(oldCell, nQuantityVec);
                }
                else if (oldCell == 0) {
                    energy -= ECMaterialContactEnergy(nCell, targetQuantityVec);
                }
            }
            if (nCell != newCell) {
                if (nCell == 0) {
                    energy += ECMaterialContactEnergy(newCell, nQuantityVec);
                }
                else if (newCell == 0) {
                    energy += ECMaterialContactEnergy(nCell, copyQuantityVector);
                }

            }
        }
    }

    return energy;
}

double ECMaterialsPlugin::ECMaterialContactEnergy(const CellG *cell, std::vector<float> _qtyVec) {

    double energy = 0.0;

    if (cell != 0) {
		std::vector<float> AdhesionCoefficients = ECMaterialCellDataAccessor.get(cell->extraAttribPtr)->AdhesionCoefficients;
        for (int i = 0; i < AdhesionCoefficients.size() ; ++i){
            energy += AdhesionCoefficients[i]*_qtyVec[i];
        }
    }

    return energy;

}

double ECMaterialsPlugin::ECMaterialDurabilityEnergy(std::vector<float> _qtyVec) {

    double energy = 0.0;
    float thisEnergy;

    for (unsigned int i = 0; i < _qtyVec.size(); ++i) {
        thisEnergy = _qtyVec[i]* ECMaterialsVec[i].getDurabilityLM();
        if (thisEnergy > 0.0) {energy += (double) thisEnergy;}
    }

    return energy;

}

void ECMaterialsPlugin::field3DChange(const Point3D &pt, CellG *newCell, CellG *oldCell) {

    // If source agent is a cell and target agent is the medium, then target EC materials are removed
    // If source agent is the medium, materials advect

	ECMaterialsData *ECMaterialsDataLocal = ECMaterialsField->get(pt);
	std::vector<float> & ECMaterialsQuantityVec = ECMaterialsDataLocal->ECMaterialsQuantityVec;
	std::vector<float> ECMaterialsQuantityVecNew(numberOfMaterials);

    if (newCell) { // Source agent is a cell
        if (!oldCell){
			ECMaterialsQuantityVec = ECMaterialsQuantityVecNew;
			ECMaterialsField->set(pt, ECMaterialsDataLocal);
        }
    }
    else { // Source agent is a medium
        if (oldCell){
			ECMaterialsQuantityVec = calculateCopyQuantityVec(oldCell, pt);
			ECMaterialsField->set(pt, ECMaterialsDataLocal);
        }
    }

}

std::vector<float> ECMaterialsPlugin::calculateCopyQuantityVec(const CellG * _cell, const Point3D &pt) {

    std::vector<float> copyQuantityVec(numberOfMaterials);

    // Calculate copy quantity vector
    // quantity vector is mean of all transferable neighborhood components + target cell remodeling quantity
    CellG *neighborCell;
	if (_cell) { copyQuantityVec = ECMaterialCellDataAccessor.get(_cell->extraAttribPtr)->RemodelingQuantity; }

    float numberOfMediumNeighbors = 1.0;
    std::vector<float> neighborQuantityVector(numberOfMaterials);
    std::vector<Neighbor> neighbors = getFirstOrderNeighbors(pt);
    WatchableField3D<CellG *> *fieldG = (WatchableField3D<CellG *> *)potts->getCellFieldG();
    for (int nIdx = 0; nIdx < neighbors.size(); ++nIdx){
        neighborCell = fieldG->get(neighbors[nIdx].pt);
        if (neighborCell) {continue;}
		neighborQuantityVector = ECMaterialsField->get(neighbors[nIdx].pt)->ECMaterialsQuantityVec;
		for (int i = 0; i < ECMaterialsVec.size(); ++i) {
			if ( !(ECMaterialsVec[i].getTransferable()) ) { neighborQuantityVector[i] = 0.0; }
		}

        for (int i = 0; i < copyQuantityVec.size(); ++i) {copyQuantityVec[i] += neighborQuantityVector[i];}
        numberOfMediumNeighbors += 1.0;
    }

    for (int i = 0; i < copyQuantityVec.size(); ++i){copyQuantityVec[i] /= numberOfMediumNeighbors;}

    std::vector<float> copyQuantityVecChecked = ECMaterialsPlugin::checkQuantities(copyQuantityVec);

	return copyQuantityVecChecked;

}

void ECMaterialsPlugin::update(CC3DXMLElement *_xmlData, bool _fullInitFlag) {

    automaton = potts->getAutomaton();
    ASSERT_OR_THROW("CELL TYPE PLUGIN WAS NOT PROPERLY INITIALIZED YET. MAKE SURE THIS IS THE FIRST PLUGIN THAT YOU SET", automaton)
        set<unsigned char> cellTypesSet;

}


void ECMaterialsPlugin::initializeECMaterials() {
    //cerr<<"initializeECMaterials ECMaterialsInitialized="<<ECMaterialsInitialized<<endl;
    //exit(1);

    if (ECMaterialsInitialized)//we double-check this flag to makes sure this function does not get called multiple times by different threads
        return;


	// initialize cell type->remodeling quantity map
	//      first collect inputs as a 2D array by type and material ids, then assemble map

	cerr << "Initializing remodeling quantities..." << endl;

	int thisTypeId;
	int maxTypeId = (int)automaton->getMaxTypeId();
	unsigned int ECMaterialIdx;
	double thisRemodelingQuantity;
	std::string ECMaterialName;
	std::string cellTypeName;
	std::vector<std::string> cellTypeNamesByTypeId;
	std::vector<std::vector<float> > RemodelingQuantityByTypeId(maxTypeId + 1, vector<float>(numberOfMaterials));
	typeToRemodelingQuantityMap.clear();

	for (int i = 0; i < ECMaterialRemodelingQuantityXMLVec.size(); ++i) {
		cellTypeName = ECMaterialRemodelingQuantityXMLVec[i]->getAttribute("CellType");
		ECMaterialName = ECMaterialRemodelingQuantityXMLVec[i]->getAttribute("Material");
		thisRemodelingQuantity = ECMaterialRemodelingQuantityXMLVec[i]->getDouble();

		cerr << "   Initializing (" << cellTypeName << ", " << ECMaterialName << " ): " << thisRemodelingQuantity << endl;

		thisTypeId = (int)automaton->getTypeId(cellTypeName);
		ECMaterialIdx = getECMaterialIndexByName(ECMaterialName);
		cellTypeNamesByTypeId.push_back(cellTypeName);
		RemodelingQuantityByTypeId[thisTypeId][ECMaterialIdx] = (float)thisRemodelingQuantity;
	}
	//      Make the cell type->remodeling quantity map
	std::vector<float> thisRemodelingQuantityVec;
	for (int i = 0; i < cellTypeNamesByTypeId.size(); ++i) {
		cellTypeName = cellTypeNamesByTypeId[i];
		thisTypeId = (int)automaton->getTypeId(cellTypeName);

		cerr << "   Setting remodeling quantity for cell type " << thisTypeId << ": " << cellTypeName << endl;

		thisRemodelingQuantityVec = RemodelingQuantityByTypeId[thisTypeId];
		typeToRemodelingQuantityMap.insert(make_pair(cellTypeName, thisRemodelingQuantityVec));
	}

	// initialize cell adhesion coefficients by cell type from user specification

	cerr << "Initializing cell-ECMaterial interface adhesion coefficients by cell type and material component..." << endl;

	AdhesionCoefficientsByTypeId.clear();
	std::vector<std::vector<float> > AdhesionCoefficientsByTypeId(maxTypeId + 1, std::vector<float>(numberOfMaterials));
	double thisAdhesionCoefficient;
	for (int i = 0; i < ECMaterialAdhesionXMLVec.size(); ++i) {
		cellTypeName = ECMaterialAdhesionXMLVec[i]->getAttribute("CellType");
		ECMaterialName = ECMaterialAdhesionXMLVec[i]->getAttribute("Material");
		thisAdhesionCoefficient = ECMaterialAdhesionXMLVec[i]->getDouble();

		cerr << "   Initializing (" << cellTypeName << ", " << ECMaterialName << " ): " << thisAdhesionCoefficient << endl;

		thisTypeId = (int)automaton->getTypeId(cellTypeName);
		ECMaterialIdx = getECMaterialIndexByName(ECMaterialName);
		AdhesionCoefficientsByTypeId[thisTypeId][ECMaterialIdx] = (float)thisAdhesionCoefficient;
	}

	// assign remodeling quantities and adhesion coefficients to cells by type and material name from user specification

	cerr << "Assigning remodeling quantities and adhesion coefficients to cells..." << endl;

	std::map<std::string, std::vector<float> >::iterator mitr;
	CellInventory::cellInventoryIterator cInvItr;
	CellG * cell;
	CellInventory * cellInventoryPtr = &potts->getCellInventory();
	for (cInvItr = cellInventoryPtr->cellInventoryBegin(); cInvItr != cellInventoryPtr->cellInventoryEnd(); ++cInvItr) {
		cell = cellInventoryPtr->getCell(cInvItr);
		std::vector<float> & RemodelingQuantity = ECMaterialCellDataAccessor.get(cell->extraAttribPtr)->RemodelingQuantity;
		std::vector<float> & AdhesionCoefficients = ECMaterialCellDataAccessor.get(cell->extraAttribPtr)->AdhesionCoefficients;

		std::vector<float> RemodelingQuantityNew(numberOfMaterials);
		RemodelingQuantity = RemodelingQuantityNew;
		RemodelingQuantity[0] = 1.0;

		cellTypeName = automaton->getTypeName(automaton->getCellType(cell));
		mitr = typeToRemodelingQuantityMap.find(cellTypeName);
		if (mitr != typeToRemodelingQuantityMap.end()) {
			RemodelingQuantity = mitr->second;
		}

		AdhesionCoefficients = AdhesionCoefficientsByTypeId[(int)cell->type];
	}

    ECMaterialsInitialized = true;

}

std::vector<float> ECMaterialsPlugin::checkQuantities(std::vector<float> _qtyVec) {
    for (int i = 0; i < _qtyVec.size(); ++i) {
		if (_qtyVec[i] < 0.0 || isnan(_qtyVec[i])) { _qtyVec[i] = 0.0; }
		else if (_qtyVec[i] > 1.0) { _qtyVec[i] = 1.0; }
    }
    return _qtyVec;
}

void ECMaterialsPlugin::setRemodelingQuantityByName(const CellG * _cell, std::string _ECMaterialName, float _quantity) {
    unsigned int _idx = getECMaterialIndexByName(_ECMaterialName);
    setRemodelingQuantityByIndex(_cell, _idx, _quantity);
}

void ECMaterialsPlugin::setRemodelingQuantityByIndex(const CellG * _cell, unsigned int _idx, float _quantity) {
	std::vector<float> & RemodelingQuantity = ECMaterialCellDataAccessor.get(_cell->extraAttribPtr)->RemodelingQuantity;
	if (_idx < RemodelingQuantity.size()) { RemodelingQuantity[_idx] = _quantity; }
}

void ECMaterialsPlugin::setRemodelingQuantityVector(const CellG * _cell, std::vector<float> _quantityVec) {
	std::vector<float> & RemodelingQuantity = ECMaterialCellDataAccessor.get(_cell->extraAttribPtr)->RemodelingQuantity;
	RemodelingQuantity = _quantityVec;
}

void ECMaterialsPlugin::assignNewRemodelingQuantityVector(const CellG * _cell, unsigned int _numMtls) {
	std::vector<float> RemodelingQuantityNew(_numMtls);
	std::vector<float> & RemodelingQuantity = ECMaterialCellDataAccessor.get(_cell->extraAttribPtr)->RemodelingQuantity;
	RemodelingQuantity = RemodelingQuantityNew;
}

void ECMaterialsPlugin::setMediumECMaterialQuantityByName(const Point3D &pt, std::string _ECMaterialName, float _quantity) {
    setMediumECMaterialQuantityByIndex(pt, getECMaterialIndexByName(_ECMaterialName), _quantity);
}

void ECMaterialsPlugin::setMediumECMaterialQuantityByIndex(const Point3D &pt, unsigned int _idx, float _quantity) {
    ECMaterialsData *ECMaterialsDataLocal = ECMaterialsField->get(pt);
	std::vector<float> & ECMaterialsQuantityVec = ECMaterialsDataLocal->ECMaterialsQuantityVec;
	ECMaterialsQuantityVec[_idx] = _quantity;
    ECMaterialsField->set(pt, ECMaterialsDataLocal);
}

void ECMaterialsPlugin::setMediumECMaterialQuantityVector(const Point3D &pt, std::vector<float> _quantityVec) {
    ECMaterialsData *ECMaterialsDataLocal = ECMaterialsField->get(pt);
	std::vector<float> & ECMaterialsQuantityVec = ECMaterialsDataLocal->ECMaterialsQuantityVec;
	ECMaterialsQuantityVec = _quantityVec;
    ECMaterialsField->set(pt, ECMaterialsDataLocal);
}

void ECMaterialsPlugin::assignNewMediumECMaterialQuantityVector(const Point3D &pt, unsigned int _numMtls) {
    ECMaterialsData *ECMaterialsDataLocal = ECMaterialsField->get(pt);
	std::vector<float> ECMaterialsQuantityVecNew(_numMtls);
	std::vector<float> & ECMaterialsQuantityVec = ECMaterialsDataLocal->ECMaterialsQuantityVec;
	ECMaterialsQuantityVec = ECMaterialsQuantityVecNew;
	ECMaterialsField->set(pt, ECMaterialsDataLocal);
}

void ECMaterialsPlugin::setECMaterialDurabilityByName(std::string _ECMaterialName, float _durabilityLM) {
    setECMaterialDurabilityByIndex(getECMaterialIndexByName(_ECMaterialName), _durabilityLM);
}

void ECMaterialsPlugin::setECMaterialDurabilityByIndex(unsigned int _idx, float _durabilityLM) {
    // if index exceeds number of materials, then ignore it
    if ( _idx < ECMaterialsVec.size() ) {
        ECMaterialsVec[_idx].setDurabilityLM(_durabilityLM);
    }
}

void ECMaterialsPlugin::setECMaterialAdvectingByName(std::string _ECMaterialName, bool _isAdvecting) {
    setECMaterialAdvectingByIndex(getECMaterialIndexByName(_ECMaterialName), _isAdvecting);
}

void ECMaterialsPlugin::setECMaterialAdvectingByIndex(unsigned int _idx, bool _isAdvecting) {
    // if index exceeds number of materials, then ignore it
    if ( _idx < ECMaterialsVec.size() ){
        ECMaterialsVec[_idx].setTransferable(_isAdvecting);
    }
}

void ECMaterialsPlugin::setECAdhesionByCell(const CellG *_cell, std::vector<float> _adhVec) {
    vector<float> & AdhesionCoefficients = ECMaterialCellDataAccessor.get(_cell->extraAttribPtr)->AdhesionCoefficients;
	AdhesionCoefficients = _adhVec;
}

float ECMaterialsPlugin::getRemodelingQuantityByName(const CellG * _cell, std::string _ECMaterialName) {
    return getRemodelingQuantityByIndex(_cell, getECMaterialIndexByName(_ECMaterialName));
}

float ECMaterialsPlugin::getRemodelingQuantityByIndex(const CellG * _cell, unsigned int _idx) {
	std::vector<float> remodelingQuantityVector = getRemodelingQuantityVector(_cell);
	if ( _idx > remodelingQuantityVector.size()-1 ) {
        ASSERT_OR_THROW(std::string("Material index ") + std::to_string(_idx) + " out of range!" , false);
        return -1.0;
    }
	return remodelingQuantityVector[_idx];
}

std::vector<float> ECMaterialsPlugin::getRemodelingQuantityVector(const CellG * _cell) {
    std::vector<float> remodelingQuantityVector(numberOfMaterials);
    if (_cell) {
		std::vector<float> & remodelingQuantityVector = ECMaterialCellDataAccessor.get(_cell->extraAttribPtr)->RemodelingQuantity;
    }
    return remodelingQuantityVector;
}

float ECMaterialsPlugin::getMediumECMaterialQuantityByName(const Point3D &pt, std::string _ECMaterialName) {
    return getMediumECMaterialQuantityByIndex(pt, getECMaterialIndexByName(_ECMaterialName));
}

float ECMaterialsPlugin::getMediumECMaterialQuantityByIndex(const Point3D &pt, unsigned int _idx) {
	std::vector<float> ECMaterialsQuantityVec = getMediumECMaterialQuantityVector(pt);
	if (_idx > ECMaterialsQuantityVec.size()-1) {
		ASSERT_OR_THROW(std::string("Material index ") + std::to_string(_idx) + " out of range!", false);
		return -1.0;
	}
	return ECMaterialsQuantityVec[_idx];
}

std::vector<float> ECMaterialsPlugin::getMediumECMaterialQuantityVector(const Point3D &pt) {
	vector<float> & ECMaterialsQuantityVec = ECMaterialsField->get(pt)->ECMaterialsQuantityVec;
    return ECMaterialsQuantityVec;
}

std::vector<float> ECMaterialsPlugin::getMediumAdvectingECMaterialQuantityVector(const Point3D &pt) {
    std::vector<float> _qtyVec = getMediumECMaterialQuantityVector(pt);
    std::vector<ECMaterialComponentData> _ecmVec = getECMaterialsVec();
    for (int i = 0; i < _qtyVec.size(); ++i) {
		if (!_ecmVec[i].getTransferable()) {
            _qtyVec[i] = 0.0;
        }
    }
    return _qtyVec;
}

float ECMaterialsPlugin::getECMaterialDurabilityByName(std::string _ECMaterialName) {
    return getECMaterialDurabilityByIndex(getECMaterialIndexByName(_ECMaterialName));
}

float ECMaterialsPlugin::getECMaterialDurabilityByIndex(unsigned int _idx) {
    // if index exceeds number of materials, then ignore it
    if ( _idx < ECMaterialsVec.size()){
        return ECMaterialsVec[_idx].getDurabilityLM();
    }
	ASSERT_OR_THROW(std::string("Material index ") + std::to_string(_idx) + " out of range!", false);
    return 0;
}

bool ECMaterialsPlugin::getECMaterialAdvectingByName(std::string _ECMaterialName) {
    return getECMaterialAdvectingByIndex(getECMaterialIndexByName(_ECMaterialName));
}

bool ECMaterialsPlugin::getECMaterialAdvectingByIndex(unsigned int _idx) {
    // if index exceeds number of materials, then ignore it
    if ( _idx > ECMaterialsVec.size()-1 ){
        return ECMaterialsVec[_idx].getTransferable();
    }
	ASSERT_OR_THROW(std::string("Material index ") + std::to_string(_idx) + " out of range!", false);
    return 0;
}

unsigned int ECMaterialsPlugin::getECMaterialIndexByName(std::string _ECMaterialName){
    std::map<std::string, unsigned int> ecmaterialNameIndexMap = getECMaterialNameIndexMap();
    map<std::string, unsigned int>::iterator mitr = ecmaterialNameIndexMap.find(_ECMaterialName);
    if ( mitr != ecmaterialNameIndexMap.end() ){
        return mitr->second;
    }
    ASSERT_OR_THROW(string("EC Material Name=") + _ECMaterialName + " is not defined", false);
    return 0;
}

std::vector<float> ECMaterialsPlugin::getECAdhesionByCell(const CellG *_cell) {
    std::vector<float> & adhVec = ECMaterialCellDataAccessor.get(_cell->extraAttribPtr)->AdhesionCoefficients;
    return adhVec;
}

std::vector<float> ECMaterialsPlugin::getECAdhesionByCellTypeId(unsigned int _idx) {
    std::vector<float> _adhVec(numberOfMaterials);
    if (_idx > AdhesionCoefficientsByTypeId.size()-1) {ASSERT_OR_THROW("Material index out of range!" , false);}
    else {_adhVec = AdhesionCoefficientsByTypeId[_idx];}
    return _adhVec;
}

std::vector<Neighbor> ECMaterialsPlugin::getFirstOrderNeighbors(const Point3D &pt) {
    // initialize neighborhood according to Potts neighborhood
    boundaryStrategy = BoundaryStrategy::getInstance();
    maxNeighborIndex = boundaryStrategy->getMaxNeighborIndexFromNeighborOrder(1);
    std::vector<Neighbor> neighbors;
    for (unsigned int nIdx = 0; nIdx <= maxNeighborIndex; ++nIdx) {
		Neighbor neighbor = boundaryStrategy->getNeighborDirect(const_cast<Point3D&>(pt), nIdx);
		if (!neighbor.distance) {
			//if distance is 0 then the neighbor returned is invalid
			continue;
		}
        neighbors.push_back(neighbor);
    }
    return neighbors;
}

void ECMaterialsPlugin::overrideInitialization() {
    ECMaterialsInitialized = true;
    cerr << "ECMaterialsInitialized=" << ECMaterialsInitialized << endl;
}

std::string ECMaterialsPlugin::toString() {
    return "ECMaterials";
}


std::string ECMaterialsPlugin::steerableName() {
    return toString();
}


