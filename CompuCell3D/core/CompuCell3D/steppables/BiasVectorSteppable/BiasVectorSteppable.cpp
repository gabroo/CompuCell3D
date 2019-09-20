



#include <CompuCell3D/CC3D.h>



using namespace CompuCell3D;



using namespace std;



#include "BiasVectorSteppable.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <BasicUtils/BasicRandomNumberGenerator.h>


BiasVectorSteppable::BiasVectorSteppable() : cellFieldG(0),sim(0),potts(0),xmlData(0),boundaryStrategy(0),automaton(0),cellInventoryPtr(0){}



BiasVectorSteppable::~BiasVectorSteppable() {

}





void BiasVectorSteppable::init(Simulator *simulator, CC3DXMLElement *_xmlData) {

	cerr << "got into biasvec step" << endl;

  xmlData=_xmlData;

  

  potts = simulator->getPotts();

  cellInventoryPtr=& potts->getCellInventory();

  sim=simulator;

  cellFieldG = (WatchableField3D<CellG *> *)potts->getCellFieldG();

  fieldDim=cellFieldG->getDim();



  
  //potts->getCellFactoryGroupPtr()->registerClass(&biasVectorSteppableDataAccessor);
  simulator->registerSteerableObject(this);



  update(_xmlData,true);



}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



void BiasVectorSteppable::extraInit(Simulator *simulator){

    //PUT YOUR CODE HERE

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BiasVectorSteppable::start(){



  //PUT YOUR CODE HERE



}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



void BiasVectorSteppable::step(const unsigned int currentStep){

	return (this->*stepFcnPtr)(currentStep);

}
// TODO: refactor the step_3d,2d... to rnd_w_step_... ; I need to update the wrapper to do it
//pure white random change bias:
void CompuCell3D::BiasVectorSteppable::step_3d(const unsigned int currentStep)
{
	CellInventory::cellInventoryIterator cInvItr;
	CellG * cell = 0;

	//BasicRandomNumberGenerator *rand = BasicRandomNumberGenerator::getInstance();

	//cout << "in bias vector 3d step" << endl;

	//cerr << "currentStep=" << currentStep << endl;

	for (cInvItr = cellInventoryPtr->cellInventoryBegin(); cInvItr != cellInventoryPtr->cellInventoryEnd(); ++cInvItr)
	{


		cell = cellInventoryPtr->getCell(cInvItr);

		vector<double> noise = BiasVectorSteppable::noise_vec_generator();

		cell->biasVecX = noise[0];
		cell->biasVecY = noise[1];
		cell->biasVecZ = noise[2];




		//cell = cellInventoryPtr->getCell(cInvItr);

		////method for getting random unitary vector in sphere from Marsaglia 1972
		////example and reason for not using a uniform distribution
		////can be found @ mathworld.wolfram.com/SpherePointPicking.html
		//double tx = 2 * rand->getRatio() - 1;
		//double ty = 2 * rand->getRatio() - 1;

		//double dist_sqrd = (tx*tx + ty*ty);
		///*cerr << "in the 3d step method" << endl;*/

		//while (dist_sqrd >= 1)
		//{
		//	tx = 2 * rand->getRatio() - 1;
		//	ty = 2 * rand->getRatio() - 1;


		//	dist_sqrd = tx*tx + ty*ty;

		//}

		//if (dist_sqrd < 1)
		//{
		//	double x = 2 * tx * std::sqrt(1 - tx*tx - ty*ty);
		//	double y = 2 * ty * std::sqrt(1 - tx*tx - ty*ty);
		//	double z = 1 - 2 * (tx*tx + ty*ty);

		//	/*cout << "tx=" << tx << endl;
		//	cout << "ty=" << ty << endl;
		//	cout << "dist=" << dist_sqrd << endl;

		//	cout << x << endl;
		//	cout << y << endl;
		//	cout << z << endl;*/

		//	cell->biasVecX = x;
		//	cell->biasVecY = y;
		//	cell->biasVecZ = z;
		//}
	}
}

//pure white random change bias:
void CompuCell3D::BiasVectorSteppable::step_2d_x(const unsigned int currentStep)
{
	CellInventory::cellInventoryIterator cInvItr;
	CellG * cell = 0;

	//BasicRandomNumberGenerator *rand = BasicRandomNumberGenerator::getInstance();

	//cout << "in bias vector step" << endl;

	//cerr << "currentStep=" << currentStep << endl;

	for (cInvItr = cellInventoryPtr->cellInventoryBegin(); cInvItr != cellInventoryPtr->cellInventoryEnd(); ++cInvItr)
	{

		cell = cellInventoryPtr->getCell(cInvItr);

		vector<double> noise = BiasVectorSteppable::noise_vec_generator();

		//double angle = rand->getRatio() * 2 * M_PI;

		//double z = std::cos(angle);
		//double y = std::sin(angle);
		///*cout << "in the 2d step method" << endl;
		//cout << x << endl;
		//cout << y << endl;*/

		cell->biasVecX = 0;
		cell->biasVecY = noise[0];
		cell->biasVecZ = noise[1];
	}

}

//pure white random change bias:
void CompuCell3D::BiasVectorSteppable::step_2d_y(const unsigned int currentStep)
{
	CellInventory::cellInventoryIterator cInvItr;
	CellG * cell = 0;

	//BasicRandomNumberGenerator *rand = BasicRandomNumberGenerator::getInstance();

	//cout << "in bias vector step" << endl;

	//cerr << "currentStep=" << currentStep << endl;

	for (cInvItr = cellInventoryPtr->cellInventoryBegin(); cInvItr != cellInventoryPtr->cellInventoryEnd(); ++cInvItr)
	{

		cell = cellInventoryPtr->getCell(cInvItr);

		vector<double> noise = BiasVectorSteppable::noise_vec_generator();

		//double angle = rand->getRatio() * 2 * M_PI;

		//double x = std::cos(angle);
		//double z = std::sin(angle);
		///*cout << "in the 2d step method" << endl;
		//cout << x << endl;
		//cout << y << endl;*/

		cell->biasVecX = noise[0];
		cell->biasVecY = 0;
		cell->biasVecZ = noise[1];
	}
}
//pure white random change bias:
void CompuCell3D::BiasVectorSteppable::step_2d_z(const unsigned int currentStep)
{
	CellInventory::cellInventoryIterator cInvItr;
	CellG * cell = 0;

	//BasicRandomNumberGenerator *rand = BasicRandomNumberGenerator::getInstance();

	//cout << "in bias vector step" << endl;

	//cerr << "currentStep=" << currentStep << endl;

	for (cInvItr = cellInventoryPtr->cellInventoryBegin(); cInvItr != cellInventoryPtr->cellInventoryEnd(); ++cInvItr)
	{


		cell = cellInventoryPtr->getCell(cInvItr);

		vector<double> noise = BiasVectorSteppable::noise_vec_generator();

		//double angle = rand->getRatio() * 2 * M_PI;

		//double x = std::cos(angle);
		//double y = std::sin(angle);
		///*cout << "in the 2d step method" << endl;
		//cout << x << endl;
		//cout << y << endl;*/

		cell->biasVecX = noise[0];
		cell->biasVecY = noise[1];
		cell->biasVecZ = 0;
	}
}


vector<double> BiasVectorSteppable::noise_vec_generator()
{
	return (this->*noiseFcnPtr)();
}


//creates unitary 2d white noise vector
vector<double> BiasVectorSteppable::white_noise_2d()
{
	BasicRandomNumberGenerator *rand = BasicRandomNumberGenerator::getInstance();

	double angle = rand->getRatio() * 2 * M_PI;
	double x0 = std::cos(angle);
	double x1 = std::sin(angle);

	vector<double> noise{ x0,x1 };
	
	return noise;
}


vector<double> BiasVectorSteppable::white_noise_3d()
{

	BasicRandomNumberGenerator *rand = BasicRandomNumberGenerator::getInstance();

	//method for getting random unitary vector in sphere from Marsaglia 1972
	//example and reason for not using a uniform distribution
	//can be found @ mathworld.wolfram.com/SpherePointPicking.html

	double tx = 2 * rand->getRatio() - 1;
	double ty = 2 * rand->getRatio() - 1;

	double dist_sqrd = (tx*tx + ty*ty);
	/*cerr << "in the 3d step method" << endl;*/

	while (dist_sqrd >= 1)
	{
		tx = 2 * rand->getRatio() - 1;
		ty = 2 * rand->getRatio() - 1;


		dist_sqrd = tx*tx + ty*ty;

	}

	if (dist_sqrd < 1)
	{
		double x = 2 * tx * std::sqrt(1 - tx*tx - ty*ty);
		double y = 2 * ty * std::sqrt(1 - tx*tx - ty*ty);
		double z = 1 - 2 * (tx*tx + ty*ty);
		vector<double> noise{ x, y, z };

		return noise;
	}
	//TODO: some sort of catching for infinite loops
}




void BiasVectorSteppable::update(CC3DXMLElement *_xmlData, bool _fullInitFlag){



    //PARSE XML IN THIS FUNCTION

    //For more information on XML parser function please see CC3D code or lookup XML utils API

    automaton = potts->getAutomaton();

    ASSERT_OR_THROW("CELL TYPE PLUGIN WAS NOT PROPERLY INITIALIZED YET. MAKE SURE THIS IS THE FIRST PLUGIN THAT YOU SET", automaton)

   set<unsigned char> cellTypesSet;

    

    //boundaryStrategy has information aobut pixel neighbors 

    boundaryStrategy=BoundaryStrategy::getInstance();

	if (fieldDim.x == 1 || fieldDim.y == 1 || fieldDim.z == 1)
	{
		noiseType = WHITE2D;
	}
	else
	{
		noiseType = WHITE3D;
	}


	switch (noiseType)
	{
	case WHITE2D:
		noiseFcnPtr = &BiasVectorSteppable::white_noise_2d;
		break;
	case WHITE3D:
		noiseFcnPtr = &BiasVectorSteppable::white_noise_3d;
		break;
	default:
		noiseFcnPtr = &BiasVectorSteppable::white_noise_3d;
		break;
	}




	if (fieldDim.x == 1)
	{
		stepType = STEP2DX;
	}
	else if (fieldDim.y == 1)
	{
		stepType = STEP2DY;
	}
	else if (fieldDim.z == 1)
	{
		stepType = STEP2DZ;
	}
	else
	{
		stepType = STEP3D;
	}


	switch (stepType)
	{
	case STEP3D:
		stepFcnPtr = &BiasVectorSteppable::step_3d;
		break;
	case STEP2DX:
		stepFcnPtr = &BiasVectorSteppable::step_2d_x;
		break;
	case STEP2DY:
		stepFcnPtr = &BiasVectorSteppable::step_2d_y;
		break;
	case STEP2DZ:
		stepFcnPtr = &BiasVectorSteppable::step_2d_z;
		break;
	default:
		stepFcnPtr = &BiasVectorSteppable::step_3d;
	}

}



std::string BiasVectorSteppable::toString(){

   return "BiasVectorSteppable";

}



std::string BiasVectorSteppable::steerableName(){

   return toString();

}

        

