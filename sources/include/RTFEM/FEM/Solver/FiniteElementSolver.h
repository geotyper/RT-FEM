#ifndef PROJECT_FINITEELEMENTSOLVER_H
#define PROJECT_FINITEELEMENTSOLVER_H

#include <RTFEM/DataTypes.h>
#include <RTFEM/DataStructure/Matrix.h>

#include <memory>

namespace rtfem {

class FiniteElement;
class Vector3;

 /**
 *  Contains:
 *      Volume (V)
 *      Geometry Matrix (B)
 *      Force vector (p)
 *
 *  Coordinates:
 *      x2 is assumed to point 'up'
 */
struct FiniteElementSolverData{
    FiniteElementSolverData() :
            volume(0){}
    Float volume;

    // Used to compute Global Stiffness
    //Matrix geometry_matrix;
    Eigen::Matrix<Float, Eigen::Dynamic, Eigen::Dynamic> geometry_matrix;

    // Used to compute Global Force
    //Matrix force_vector;
    Eigen::Matrix<Float, Eigen::Dynamic, 1> force_vector;
};

/**
 * Pressure forces directed along unit normal of i-th face
 * (traction_force_face1, i == 1)
 */
struct TractionForce {
    Float force_face1 = 0;
    Float force_face2 = 0;
    Float force_face3 = 0;
    Float force_face4 = 0;
};

/**
 * Computes FiniteElementSolverData for a given FiniteElement.
 */
class FiniteElementSolver {
public:
    FiniteElementSolver() = default;
    virtual ~FiniteElementSolver() = default;

    virtual FiniteElementSolverData Solve(std::shared_ptr<FiniteElement> finite_element) = 0;

    virtual FiniteElementSolverData Solve(std::shared_ptr<FiniteElement> finite_element,
                                          const Eigen::Vector3<Float>& body_force,
                                          const TractionForce& traction_force) = 0;
};
}


#endif //PROJECT_FINITEELEMENTSOLVER_H
