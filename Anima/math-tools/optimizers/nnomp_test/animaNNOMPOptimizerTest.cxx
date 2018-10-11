#include <animaNNOrthogonalMatchingPursuitOptimizer.h>
#include <itkTimeProbe.h>
#include <iostream>
#include <fstream>

int main()
{
    typedef anima::NNOrthogonalMatchingPursuitOptimizer OptimizerType;

    itk::TimeProbe tmpTime;
    tmpTime.Start();

    OptimizerType::Pointer optTest = OptimizerType::New();

    unsigned int numSignals = 288;
    unsigned int numAtoms = 360;
    OptimizerType::MatrixType testData (numSignals,numAtoms);
    std::ifstream inputDataMatrix("dataMatrix.txt");

    for (unsigned int i = 0;i < numSignals;++i)
    {
        for (unsigned int j = 0;j < numAtoms;++j)
            inputDataMatrix >> testData(i,j);
    }

    inputDataMatrix.close();
    OptimizerType::ParametersType testPoints(numSignals);
    std::ifstream inputDataSignals("signals.txt");

    for (unsigned int i = 0;i < numSignals;++i)
        inputDataSignals >> testPoints[i];

    inputDataSignals.close();

    optTest->SetDataMatrix(testData);
    optTest->SetPoints(testPoints);

    optTest->SetMaximalNumberOfWeights(2);
    optTest->SetIgnoredIndexesUpperBound(0);

    optTest->StartOptimization();

    tmpTime.Stop();

    std::cout << "Computation time: " << tmpTime.GetTotal() << std::endl;
    std::cout << optTest->GetCurrentPosition() << std::endl;

    return EXIT_SUCCESS;
}
