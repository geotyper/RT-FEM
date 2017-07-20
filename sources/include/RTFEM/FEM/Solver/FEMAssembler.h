#ifndef PROJECT_GLOBALSTIFFNESSASSEMBLER_H
#define PROJECT_GLOBALSTIFFNESSASSEMBLER_H

#include <memory>

#include <RTFEM/DataStructure/Matrix.h>

namespace rtfem {

class FEMModel;
class FiniteElement;

class FiniteElementSolver;
enum class FiniteElementType;

struct FEMAssemblerData{
    FEMAssemblerData(UInt global_dof_count) :
            global_stiffness(Matrix(global_dof_count,global_dof_count)),
            global_force(Matrix(global_dof_count, 1)) {}

    Matrix global_stiffness;
    Matrix global_force;
};

/**
 * Computes and Assembles Global Stiffness Matrix and Global Force Vector.
 *
 * Local Stiffness Matrix (k) is the stiffness of each element [3Ne x 3Ne]
 * e.g. for Tetrahedron (Ne = 4) thus: (dim = [12 x 12])
 *
 * Global Stiffness Matrix (K) is the stiffness of entire FEM Model [3N x 3N]
 * e.g. For 9 vertices (dim = [27 x 27])
 *
 * Partial Global Stiffness Matrix (Ke) is the matrix of dimension equal to Global Stiffness
 * but filled only with Local Stiffness data.
 *
 * Partial Global Force Vector (Qe) [3N x 1]
 * Global Force Vector (Q) [3N x 1]
 */
class FEMAssembler {
public:
    FEMAssembler();

    ~FEMAssembler();

    /**
     * Computes Global Stiffness Matrix (K).
     *
     *      1) Computes Constitutive Matrix (C)
     *      2) Computes Geometry Matrix (B) for each Finite Element
     *      3) Computes Local Stiffness (k) for each Finite Element
     *          - Using Constitutive Matrix and Geometry Matrix.
     *      4) Assembles all Local Stiffness matrices into Global Stiffness Matrix (K)
     *
     * TODO: Benchmark Global Stiffness Matrix
     * @param fem_model
     * @return
     */
    FEMAssemblerData Compute(const std::shared_ptr<FEMModel> fem_model);

private:
    /**
     * Computes Isotropic Constitutive Matrix (C).
     * Used to compute Local Stiffness.
     *
     * In case of Linear Elasticity, Constitutive Matrix is constant.
     *
     * Isotropy allows Constitutive Matrix to remain unchanged
     * after orthogonal transformations
     *
     * @param fem_model
     * @return
     */
    Matrix ComputeConstitutiveMatrix(const std::shared_ptr<FEMModel> fem_model);

    /**
     * Computes Geometry Matrix (B).
     * Used to compute Local Stiffness
     *
     * @param type
     * @return
     */
    std::unique_ptr<FiniteElementSolver> GetFiniteElementSolver(const FiniteElementType& type);

    /**
     * Computes The Boolean Assembly Matrix (A) [3Ne x 3N].
     *
     * Maps Local Stiffness to Global Stiffness.
     *
     * @param finite_element
     * @param vertex_count
     * @return
     */
    Matrix ComputeBooleanAssemblyMatrix(const std::shared_ptr<FiniteElement> finite_element,
                                        UInt vertex_count);

    /**
     * Computes Partial Global Stiffness Matrix.
     * Uses Boolean Assembly Matrix to map a single Local Stiffness to Global Stiffness.
     *
     * @param boolean_assembly_matrix_A
     * @param local_stiffness_k
     * @return
     */
    Matrix ComputePartialGlobalStiffnessMatrix(const Matrix& geometry_matrix,
                                               const Matrix& constitutive_matrix_C,
                                               const Matrix& boolean_assembly_matrix_A,
                                               Float volume);

    /**
     * Computes Local Stiffness (k) of a given element.
     *
     * @param finite_element
     * @param constitutive_matrix
     * @param volume
     * @return
     */
    Matrix
    ComputeLocalStiffness(const Matrix &geometry_matrix,
                          const Matrix &constitutive_matrix,
                          Float volume);

    /**
     * Computes the Partial Global Force vector (Q)
     * @param force_vector
     * @param boolean_assembly_matrix_A
     * @return
     */
    Matrix ComputePartialGlobalForceVector(const Matrix &force_vector,
                                           const Matrix &boolean_assembly_matrix_A);
};
}


#endif //PROJECT_GLOBALSTIFFNESSASSEMBLER_H