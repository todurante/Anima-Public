#include <animaGaussianMCMVariableProjectionCost.h>
#include <cmath>

#include <animaBaseTensorTools.h>
#include <boost/math/special_functions/gamma.hpp>
#include <boost/math/special_functions/fpclassify.hpp>

namespace anima
{

GaussianMCMVariableProjectionCost::MeasureType
GaussianMCMVariableProjectionCost::GetValues(const ParametersType &parameters)
{
    unsigned int nbParams = parameters.GetSize();

    // Set MCM parameters
    m_TestedParameters.resize(nbParams);

    for (unsigned int i = 0;i < nbParams;++i)
        m_TestedParameters[i] = parameters[i];

    m_MCMStructure->SetParametersFromVector(m_TestedParameters);

    this->PrepareDataForLLS();

    this->SolveLinearLeastSquares();

    m_MCMStructure->SetCompartmentWeights(m_OptimalWeights);
    return m_Residuals;
}

double GaussianMCMVariableProjectionCost::GetCurrentCostValue()
{
    // This is -2log(L) so that we only have to give one formula
    double costValue = 0;
    unsigned int nbImages = m_Residuals.size();

    costValue = nbImages * (1.0 + std::log(2.0 * M_PI * m_SigmaSquare));

    return costValue;
}

void
GaussianMCMVariableProjectionCost::SolveLinearLeastSquares()
{
    unsigned int nbValues = m_Gradients.size();
    unsigned int numCompartments = m_IndexesUsefulCompartments.size();
    unsigned int numOnCompartments = 0;

    for (unsigned int j = 0;j < numCompartments;++j)
    {
        if (m_CompartmentSwitches[j])
            ++numOnCompartments;
    }

    unsigned int vecSize = numOnCompartments;

    m_FSignal.resize(vecSize);
    std::fill(m_FSignal.begin(),m_FSignal.end(),0.0);
    m_GramMatrix.set_size(vecSize,vecSize);
    m_GramMatrix.fill(0.0);

    for (unsigned int i = 0;i < nbValues;++i)
    {
        unsigned int posX = 0;
        for (unsigned int j = 0;j < numCompartments;++j)
        {
            if (!m_CompartmentSwitches[j])
                continue;

            m_FSignal[posX] += m_PredictedSignalAttenuations(i,j) * m_ObservedSignals[i];

            unsigned int posY = posX;
            for (unsigned int k = j;k < numCompartments;++k)
            {
                if (!m_CompartmentSwitches[k])
                    continue;

                m_GramMatrix(posX,posY) += m_PredictedSignalAttenuations(i,j) * m_PredictedSignalAttenuations(i,k);
                ++posY;
            }

            ++posX;
        }
    }

    for (unsigned int i = 0;i < vecSize;++i)
    {
        for (unsigned int j = i+1;j < vecSize;++j)
            m_GramMatrix(j,i) = m_GramMatrix(i,j);
    }

    if (vecSize > 0)
        anima::GetTensorPower(m_GramMatrix,m_InverseGramMatrix,-1.0);

    // Here gets F G^-1
    m_FMatrixInverseG.set_size(nbValues,vecSize);
    for (unsigned int i = 0;i < nbValues;++i)
    {
        for (unsigned int j = 0;j < vecSize;++j)
        {
            m_FMatrixInverseG(i,j) = 0.0;
            unsigned int pos = 0;
            for (unsigned int k = 0;k < numCompartments;++k)
            {
                if (!m_CompartmentSwitches[k])
                    continue;

                m_FMatrixInverseG(i,j) += m_PredictedSignalAttenuations(i,k) * m_InverseGramMatrix(pos,j);
                ++pos;
            }
        }
    }

    // Solving for SigmaSquare
    m_SigmaSquare = 0.0;

    for (unsigned int i = 0;i < nbValues;++i)
    {
        double projObservedSignal = m_ObservedSignals[i];

        for (unsigned int j = 0;j < vecSize;++j)
            projObservedSignal -= m_FMatrixInverseG(i,j) * m_FSignal[j];

        m_Residuals[i] = projObservedSignal;
        m_SigmaSquare += projObservedSignal * projObservedSignal;
    }

    m_SigmaSquare /= nbValues;
}

void
GaussianMCMVariableProjectionCost::PrepareDataForLLS()
{
    unsigned int nbValues = m_Gradients.size();
    unsigned int numCompartments = m_MCMStructure->GetNumberOfCompartments();

    m_OptimalWeights.resize(numCompartments);
    std::fill(m_OptimalWeights.begin(),m_OptimalWeights.end(),0.0);

    m_IndexesUsefulCompartments.resize(numCompartments);
    unsigned int pos = 0;

    // Trick to keep the compartments with the lowest number of compartments by default
    // This is a trick because it assumes the first compartments (i.e. the iso ones are the ones with the lowest number of compartments)
    for (int i = numCompartments - 1;i >= 0;--i)
    {
        bool duplicated = false;
        for (unsigned int j = i+1;j < numCompartments;++j)
        {
            if (m_MCMStructure->GetCompartment(i)->IsEqual(m_MCMStructure->GetCompartment(j)))
            {
                duplicated = true;
                break;
            }
        }

        if (!duplicated)
        {
            m_IndexesUsefulCompartments[pos] = i;
            ++pos;
        }
    }

    numCompartments = pos;
    m_IndexesUsefulCompartments.resize(numCompartments);

    // Compute predicted signals and jacobian
    m_PredictedSignalAttenuations.set_size(nbValues,numCompartments);

    m_OptimalNNLSWeights.set_size(numCompartments);
    m_OptimalNNLSWeights.fill(0.0);

    bool directMethod = false;

    if (directMethod)
    {
        m_CholeskyMatrix.set_size(numCompartments,numCompartments);
        m_CholeskyMatrix.fill(0.0);

        // Scale factor applied to construct Cholesky matrix to avoid colinearities and wrong results
        double scaleFactor = 1000.0;

        for (unsigned int i = 0;i < nbValues;++i)
        {
            for (unsigned int j = 0;j < numCompartments;++j)
            {
                unsigned int indexComp = m_IndexesUsefulCompartments[j];
                double predictedSignal = m_MCMStructure->GetCompartment(indexComp)->GetFourierTransformedDiffusionProfile(m_SmallDelta, m_BigDelta, m_GradientStrengths[i], m_Gradients[i]);
                m_PredictedSignalAttenuations(i,j) = predictedSignal;
                m_OptimalNNLSWeights[j] += scaleFactor * predictedSignal * m_ObservedSignals[i];
                m_CholeskyMatrix(j,j) += scaleFactor * predictedSignal * predictedSignal;

                for (unsigned int k = 0;k < j;++k)
                {
                    double tmpVal = scaleFactor * predictedSignal * m_PredictedSignalAttenuations(i,k);
                    m_CholeskyMatrix(j,k) += tmpVal;
                    m_CholeskyMatrix(k,j) += tmpVal;
                }
            }
        }
        
        m_CholeskySolver = CholeskyDecomposition(m_CholeskyMatrix);
        m_CholeskySolver.PerformDecomposition();
    }
    else
    {
        m_VNLSignals.set_size(numCompartments);
        m_CholeskySolver = CholeskyDecomposition(numCompartments, 1.0);

        // Scale factor applied to construct Cholesky matrix to avoid colinearities and wrong results
        double scaleFactor = 1000.0;

        for (unsigned int i = 0;i < nbValues;++i)
        {
            for (unsigned int j = 0;j < numCompartments;++j)
            {
                unsigned int indexComp = m_IndexesUsefulCompartments[j];
                double predictedSignal = m_MCMStructure->GetCompartment(indexComp)->GetFourierTransformedDiffusionProfile(m_SmallDelta, m_BigDelta, m_GradientStrengths[i], m_Gradients[i]);
                m_PredictedSignalAttenuations(i,j) = predictedSignal;
                m_OptimalNNLSWeights[j] += scaleFactor * predictedSignal * m_ObservedSignals[i];
                m_VNLSignals[j] = std::sqrt(scaleFactor) * predictedSignal;
            }

            m_CholeskySolver.Update(m_VNLSignals);
        }
    }

    bool negativeWeightBounds = m_MCMStructure->GetNegativeWeightBounds();
    if (negativeWeightBounds)
        m_OptimalNNLSWeights *= -1.0;

    m_VNLSignals = m_OptimalNNLSWeights;
    m_CholeskySolver.SolveLinearSystem(m_OptimalNNLSWeights);

    bool performNNLS = false;
    for (unsigned int i = 0;i < numCompartments;++i)
    {
        if (m_OptimalNNLSWeights[i] < 0.0)
        {
            performNNLS = true;
            break;
        }
    }

    if (performNNLS)
    {
        if (!directMethod)
        {
            m_CholeskySolver.Recompose();
            m_CholeskyMatrix = m_CholeskySolver.GetInputMatrix();
        }

        m_NNLSBordersOptimizer->SetDataMatrix(m_CholeskyMatrix);
        m_NNLSBordersOptimizer->SetPoints(m_VNLSignals);
        m_NNLSBordersOptimizer->SetSquaredProblem(true);
        m_NNLSBordersOptimizer->StartOptimization();

        m_OptimalNNLSWeights = m_NNLSBordersOptimizer->GetCurrentPosition();
    }

    m_CompartmentSwitches.resize(numCompartments);
    for (unsigned int i = 0;i < numCompartments;++i)
    {
        m_OptimalWeights[m_IndexesUsefulCompartments[i]] = m_OptimalNNLSWeights[i];
        m_CompartmentSwitches[i] = (m_OptimalNNLSWeights[i] > 0.0);

        if (negativeWeightBounds)
            m_OptimalWeights[m_IndexesUsefulCompartments[i]] *= -1.0;
    }

    m_Residuals.SetSize(nbValues);
}

void
GaussianMCMVariableProjectionCost::PrepareDataForDerivative()
{
    unsigned int nbValues = m_Gradients.size();
    unsigned int numCompartments = m_IndexesUsefulCompartments.size();

    unsigned int nbParams = m_MCMStructure->GetNumberOfParameters();
    // Compute jacobian parts
    vnl_matrix<double> zeroMatrix(nbValues,numCompartments,0.0);
    m_SignalAttenuationsJacobian.resize(nbParams);
    std::fill(m_SignalAttenuationsJacobian.begin(),m_SignalAttenuationsJacobian.end(),zeroMatrix);
    ListType compartmentJacobian;

    for (unsigned int i = 0;i < nbValues;++i)
    {
        unsigned int pos = 0;

        for (unsigned int j = 0;j < numCompartments;++j)
        {
            unsigned int indexComp = m_IndexesUsefulCompartments[j];

            compartmentJacobian = m_MCMStructure->GetCompartment(indexComp)->GetSignalAttenuationJacobian(m_SmallDelta, m_BigDelta,
                                                                                                          m_GradientStrengths[i], m_Gradients[i]);

            unsigned int compartmentSize = compartmentJacobian.size();
            for (unsigned int k = 0;k < compartmentSize;++k)
                m_SignalAttenuationsJacobian[pos+k](i,j) = compartmentJacobian[k];

            pos += compartmentSize;
        }
    }
}

void
GaussianMCMVariableProjectionCost::GetDerivativeMatrix(const ParametersType &parameters, DerivativeMatrixType &derivative)
{
    unsigned int nbParams = parameters.GetSize();
    if (nbParams == 0)
        return;

    unsigned int nbValues = m_ObservedSignals.size();
    unsigned int numCompartments = m_IndexesUsefulCompartments.size();
    unsigned int numOnCompartments = 0;

    for (unsigned int j = 0;j < numCompartments;++j)
    {
        if (m_CompartmentSwitches[j])
            ++numOnCompartments;
    }

    this->PrepareDataForDerivative();

    // Assume get derivative is called with the same parameters as GetValue just before
    for (unsigned int i = 0;i < nbParams;++i)
    {
        if (m_TestedParameters[i] != parameters[i])
            itkExceptionMacro("Get derivative not called with the same parameters as GetValue, suggestive of NaN...");
    }

    derivative.SetSize(nbParams, nbValues);
    derivative.Fill(0.0);
    ListType DFw(nbValues,0.0);

    unsigned int vecSize = numOnCompartments;
    ListType tmpVec(vecSize,0.0);

    for (unsigned int k = 0;k < nbParams;++k)
    {
        // First, compute
        // - DFw = DF[k] w
        std::fill(DFw.begin(),DFw.end(),0.0);
        for (unsigned int i = 0;i < nbValues;++i)
        {
            DFw[i] = 0.0;
            for (unsigned int j = 0;j < numCompartments;++j)
            {
                if (!m_CompartmentSwitches[j])
                    continue;

                DFw[i] += m_SignalAttenuationsJacobian[k](i,j) * m_OptimalWeights[m_IndexesUsefulCompartments[j]];
            }
        }

        // Then, compute
        // - tmpVec = F^T DF[k] w - (DF[k])^T Residuals
        unsigned int pos = 0;
        for (unsigned int j = 0;j < numCompartments;++j)
        {
            if (!m_CompartmentSwitches[j])
                continue;

            tmpVec[pos] = 0.0;

            for (unsigned int i = 0;i < nbValues;++i)
                tmpVec[pos] += m_PredictedSignalAttenuations(i,j) * DFw[i] - m_SignalAttenuationsJacobian[k](i,j) * m_Residuals[i];

            ++pos;
        }

        // Finally, get
        // - derivative = FMatrixInverseG tmpVec - DFw
        for (unsigned int i = 0;i < nbValues;++i)
        {
            derivative[k][i] = - DFw[i];

            for (unsigned int j = 0;j < vecSize;++j)
                derivative[k][i] += m_FMatrixInverseG(i,j) * tmpVec[j];

            if (!std::isfinite(derivative[k][i]))
            {
                std::cerr << "Arf " << k << " " << i << " tmpVec ";
                for (unsigned int j = 0;j < vecSize;++j)
                    std::cerr << tmpVec[j] << " ";
                std::cerr << std::endl;
            }
        }
    }

    bool problem = false;
    for (unsigned int i = 0;i < nbParams;++i)
    {
        if (!boost::math::isfinite(derivative[i][0]))
        {
            problem = true;
            break;
        }
    }

    if (problem)
    {
        std::cerr << "Derivative: " << derivative << std::endl;
        std::cerr << "Optimal weights: " << m_OptimalNNLSWeights << std::endl;
        std::cerr << "Gram matrix: " << m_GramMatrix << std::endl;
        std::cerr << "Residuals: " << m_Residuals << std::endl;

        std::cerr << "Params: " << parameters << std::endl;
        itkExceptionMacro("Non finite derivative");
    }
}

void
GaussianMCMVariableProjectionCost::GetCurrentDerivative(DerivativeMatrixType &derivativeMatrix, DerivativeType &derivative)
{
    unsigned int nbParams = derivativeMatrix.rows();
    unsigned int nbValues = derivativeMatrix.columns();

    // Current derivative of the system is 2 residual^T derivativeMatrix
    // To handle the fact that the cost function is -2 log(L) and not the rms problem itself
    derivative.set_size(nbParams);
    for (unsigned int i = 0;i < nbParams;++i)
    {
        derivative[i] = 0.0;
        for (unsigned int j = 0;j < nbValues;++j)
            derivative[i] += m_Residuals[j] * derivativeMatrix(i,j);

        derivative[i] *= 2.0 / m_SigmaSquare;
    }
}

} // end namespace anima
